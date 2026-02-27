/******************************************************************************
    Copyright (C) 2025-2026 by Daniil Nabiulin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "opencv.h"
#include <QApplication>
#include <QScreen>
#include <QRect>
#include <QImage>

OpenCV::OpenCV(QObject *parent)
    : QObject{parent} {}

void OpenCV::setCurrentRoi(QRect currentRect)
{
    m_roi = cv::Rect(currentRect.x(), currentRect.y(), currentRect.width(), currentRect.height());
    m_ignoreRoi = cv::Rect();
}

void OpenCV::setCurrentIgnoreRoi(QRect currentRect)
{
    m_ignoreRoi = cv::Rect(currentRect.x(), currentRect.y(), currentRect.width(), currentRect.height());
}

void OpenCV::setCurrentFrameBuffer(uint32_t height, uint32_t width, void* frame)
{
    if (!m_stopped) {
        cv::Mat m_frameOriginal = cv::Mat(height, width, CV_8UC4, frame);
        cv::cvtColor(m_frameOriginal, m_frameOriginal, cv::COLOR_BGRA2BGR);

        if (!m_roi.empty())
        {
            QRect primaryScreenGeometry = QApplication::primaryScreen()->geometry();
            int screenWidth = primaryScreenGeometry.width();
            int screenHeight = primaryScreenGeometry.height();
            int imgWidth = m_frameOriginal.cols;
            int imgHeight = m_frameOriginal.rows;

            double displayAspect = static_cast<double>(screenWidth) / screenHeight;
            double imgAspect = static_cast<double>(imgWidth) / imgHeight;

            int effectiveWidth, effectiveHeight;
            int offsetX = 0, offsetY = 0;

            if (displayAspect > imgAspect) {
                effectiveHeight = imgHeight;
                effectiveWidth = static_cast<int>(imgHeight * displayAspect);
                offsetX = (imgWidth - effectiveWidth) / 2;
            } else {
                effectiveWidth = imgWidth;
                effectiveHeight = static_cast<int>(imgWidth / displayAspect);
                offsetY = (imgHeight - effectiveHeight) / 2;
            }

            double scaleX = static_cast<double>(effectiveWidth) / screenWidth;
            double scaleY = static_cast<double>(effectiveHeight) / screenHeight;

            cv::Rect roi(
                static_cast<int>(m_roi.x * scaleX) + offsetX,
                static_cast<int>(m_roi.y * scaleY) + offsetY,
                static_cast<int>(m_roi.width * scaleX),
                static_cast<int>(m_roi.height * scaleY)
                );

            if (isROIValid(roi, m_frameOriginal))
            {
                if (!m_ignoreRoi.empty())
                {
                    cv::Rect ignoreRoi(
                        static_cast<int>(m_ignoreRoi.x * scaleX) + offsetX,
                        static_cast<int>(m_ignoreRoi.y * scaleY) + offsetY,
                        static_cast<int>(m_ignoreRoi.width * scaleX),
                        static_cast<int>(m_ignoreRoi.height * scaleY)
                        );

                    if (ignoreRoi.x >= roi.x &&
                        ignoreRoi.x + ignoreRoi.width <= roi.x + roi.width &&
                        ignoreRoi.y >= roi.y &&
                        ignoreRoi.y + ignoreRoi.height <= roi.y + roi.height)
                    {
                        cv::rectangle(m_frameOriginal, ignoreRoi, cv::Scalar(0, 0, 0), -1);
                    }
                }

                cv::Mat frameProcessed = m_frameOriginal(roi);
                cv::cvtColor(frameProcessed, frameProcessed, cv::COLOR_BGR2GRAY);

                applyBlur(frameProcessed);
                applyThreshold(frameProcessed);

                // Processed Mat
                emit currentProcessedMat(frameProcessed);

                // Procesed frame
                QImage qImgProcessed(frameProcessed.data, frameProcessed.cols, frameProcessed.rows, frameProcessed.step, QImage::Format_Grayscale8);
                emit currentProcessedFrame(qImgProcessed);
            }
        }

        // Original frame
        QImage qImgOriginal(m_frameOriginal.data, m_frameOriginal.cols, m_frameOriginal.rows, m_frameOriginal.step, QImage::Format_BGR888);
        emit currentOriginalFrame(qImgOriginal);
    }
}

void OpenCV::on_blurStateChanged(int state)
{
    m_isBlur = (state == Qt::Checked);
}

void OpenCV::on_blurTypeChanged(int type)
{
    switch (type) {
    case 0: m_blurType = BlurType::GAUSSIAN_BLUR; break;
    case 1: m_blurType = BlurType::MEDIAN_BLUR;   break;
    case 2: m_blurType = BlurType::BOX_BLUR;      break;
    default: break;
    }
}

void OpenCV::setCurrentBlurSize(int kSize)
{
    m_blurSize = (kSize % 2 == 0) ? kSize + 1 : kSize;
}

void OpenCV::setSubtractBlurChanged(int state)
{
    m_isBlurSubtract = (state == Qt::Checked);
}

void OpenCV::setNormalizeChanged(int state)
{
    m_isBlurNormalize = (state == Qt::Checked);
}

void OpenCV::on_thresholdMethodChanged(bool checked)
{
    m_thresholdMethod = checked ? SIMPLE_THRESHOLD : ADAPTIVE_THRESHOLD;
}

void OpenCV::setCurrentThresh(double thresh)
{
    m_thresholdValue = thresh;
}

void OpenCV::on_thresholdSimpleTypeChanged(int type)
{
    switch (type) {
    case 0: m_thresholdSimpleType = cv::THRESH_BINARY;      break;
    case 1: m_thresholdSimpleType = cv::THRESH_BINARY_INV;  break;
    case 2: m_thresholdSimpleType = cv::THRESH_TRUNC;       break;
    case 3: m_thresholdSimpleType = cv::THRESH_TOZERO;      break;
    case 4: m_thresholdSimpleType = cv::THRESH_TOZERO_INV;  break;
    default: break;
    }
}

void OpenCV::on_thresholdAdaptiveTypeChanged(int type)
{
    switch (type) {
    case 0: m_thresholdAdaptiveType = cv::ADAPTIVE_THRESH_MEAN_C; break;
    case 1: m_thresholdAdaptiveType = cv::ADAPTIVE_THRESH_GAUSSIAN_C; break;
    default: break;
    }
}

void OpenCV::on_otsuChanged(int state)
{
    m_isOtsu = (state == Qt::Checked);
}

void OpenCV::applyBlur(cv::Mat &image)
{
    if (!m_isBlur) return;

    cv::Mat blur;
    cv::Size kSize(m_blurSize, m_blurSize);

    switch (m_blurType) {
    case BlurType::GAUSSIAN_BLUR:
        cv::GaussianBlur(image, blur, kSize, 0);
        break;
    case BlurType::MEDIAN_BLUR:
        cv::medianBlur(image, blur, m_blurSize);
        break;
    case BlurType::BOX_BLUR:
        cv::blur(image, blur, kSize);
        break;
    }

    if (m_isBlurSubtract) {
        cv::subtract(image, blur, image);
    } else {
        image = blur;
    }

    if (m_isBlurNormalize)
        cv::normalize(image, image, 0, 255, cv::NORM_MINMAX);
}

void OpenCV::applyThreshold(cv::Mat &image)
{
    if (m_thresholdMethod == SIMPLE_THRESHOLD) {
        int flags = m_thresholdSimpleType;
        if (m_isOtsu) {
            flags |= cv::THRESH_OTSU;
        }
        cv::threshold(image, image, m_thresholdValue, 255, flags);
    } else {
        cv::adaptiveThreshold(image, image, 255, m_thresholdAdaptiveType, cv::THRESH_BINARY, 11, 2);
    }
}

bool OpenCV::isROIValid(const cv::Rect &roi, const cv::Mat &image)
{
    return roi.x >= 0 &&
           roi.y >= 0 &&
           roi.x + roi.width <= image.cols &&
           roi.y + roi.height <= image.rows &&
           roi.width > 0 &&
           roi.height > 0;
}
