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

QRectF OpenCV::normalizedRoi(int frameWidth, int frameHeight) const
{
    if (m_roi.width <= 0 || m_roi.height <= 0) return QRectF();

    const LetterboxTransform t = letterboxTransform(frameWidth, frameHeight);
    if (t.scale <= 0.0) return QRectF();

    const double px = (m_roi.x - t.offsetX) / t.scale;
    const double py = (m_roi.y - t.offsetY) / t.scale;
    const double pw = m_roi.width / t.scale;
    const double ph = m_roi.height / t.scale;

    return QRectF(px / frameWidth, py / frameHeight, pw / frameWidth, ph / frameHeight);
}

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
            const int imgWidth = m_frameOriginal.cols;
            const int imgHeight = m_frameOriginal.rows;
            const LetterboxTransform t = letterboxTransform(imgWidth, imgHeight);

            cv::Rect roi;
            if (t.scale > 0.0) {
                roi = cv::Rect(
                    static_cast<int>((m_roi.x - t.offsetX) / t.scale),
                    static_cast<int>((m_roi.y - t.offsetY) / t.scale),
                    static_cast<int>(m_roi.width / t.scale),
                    static_cast<int>(m_roi.height / t.scale)
                ) & cv::Rect(0, 0, imgWidth, imgHeight);
            }

            if (roi.width > 0 && roi.height > 0)
            {
                if (!m_ignoreRoi.empty() && m_roi.width > 0 && m_roi.height > 0)
                {
                    double ifx = static_cast<double>(m_ignoreRoi.x - m_roi.x) / m_roi.width;
                    double ify = static_cast<double>(m_ignoreRoi.y - m_roi.y) / m_roi.height;
                    double ifw = static_cast<double>(m_ignoreRoi.width) / m_roi.width;
                    double ifh = static_cast<double>(m_ignoreRoi.height) / m_roi.height;

                    cv::Rect ignoreRoi(
                        roi.x + static_cast<int>(qRound(ifx * roi.width)),
                        roi.y + static_cast<int>(qRound(ify * roi.height)),
                        static_cast<int>(qRound(ifw * roi.width)),
                        static_cast<int>(qRound(ifh * roi.height))
                        );

                    ignoreRoi = ignoreRoi & roi;

                    if (ignoreRoi.width > 0 && ignoreRoi.height > 0)
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

void OpenCV::setBlurEnabled(int state)
{
    m_isBlur = (state == Qt::Checked);
}

void OpenCV::setBlurType(int type)
{
    switch (type) {
    case 0: m_blurType = BlurType::GAUSSIAN_BLUR; break;
    case 1: m_blurType = BlurType::MEDIAN_BLUR;   break;
    case 2: m_blurType = BlurType::BOX_BLUR;      break;
    default: break;
    }
}

void OpenCV::setBlurSize(int kSize)
{
    m_blurSize = (kSize % 2 == 0) ? kSize + 1 : kSize;
}

void OpenCV::setSubtractBlur(int state)
{
    m_isBlurSubtract = (state == Qt::Checked);
}

void OpenCV::setNormalizeBlur(int state)
{
    m_isBlurNormalize = (state == Qt::Checked);
}

void OpenCV::setThresholdMethod(bool checked)
{
    m_thresholdMethod = checked ? SIMPLE_THRESHOLD : ADAPTIVE_THRESHOLD;
}

void OpenCV::setSimpleThresholdType(int type)
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

void OpenCV::setThresholdValue(double thresh)
{
    m_thresholdValue = thresh;
}

void OpenCV::setOtsuEnabled(int state)
{
    m_isOtsu = (state == Qt::Checked);
}

void OpenCV::setAdaptiveThresholdType(int type)
{
    switch (type) {
    case 0: m_thresholdAdaptiveType = cv::ADAPTIVE_THRESH_MEAN_C; break;
    case 1: m_thresholdAdaptiveType = cv::ADAPTIVE_THRESH_GAUSSIAN_C; break;
    default: break;
    }
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

OpenCV::LetterboxTransform OpenCV::letterboxTransform(int frameWidth, int frameHeight)
{
    QScreen *screen = QApplication::primaryScreen();
    if (!screen || frameWidth <= 0 || frameHeight <= 0)
        return { 0.0, 0.0, 0.0 };

    const QRect screenGeometry = screen->geometry();
    const int screenWidth = screenGeometry.width();
    const int screenHeight = screenGeometry.height();
    if (screenWidth <= 0 || screenHeight <= 0)
        return { 0.0, 0.0, 0.0 };

    const double scaleToW = static_cast<double>(screenWidth) / frameWidth;
    const double scaleToH = static_cast<double>(screenHeight) / frameHeight;
    const double scale = scaleToW < scaleToH ? scaleToW : scaleToH;
    const double dispWidth = frameWidth * scale;
    const double dispHeight = frameHeight * scale;

    return { scale, (screenWidth - dispWidth) / 2.0, (screenHeight - dispHeight) / 2.0 };
}