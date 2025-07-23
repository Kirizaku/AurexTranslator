/******************************************************************************
    Copyright (C) 2025 by Daniil Nabiulin

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

void OpenCV::on_thresholdMethodChanged(const bool &checked)
{
    if (checked) {
        m_thresholdMethod = SIMPLE_THRESHOLD;
    } else {
        m_thresholdMethod = ADAPTIVE_THRESHOLD;
    }
}

void OpenCV::setCurrentThresh(const double &thresh) { m_thresholdValue = thresh; }

void OpenCV::on_thresholdSimpleTypeChanged(const int &type)
{
    switch (type) {
    case 0:
        m_thresholdSimpleType = cv::THRESH_BINARY;
        break;
    case 1:
        m_thresholdSimpleType = cv::THRESH_BINARY_INV;
        break;
    case 2:
        m_thresholdSimpleType = cv::THRESH_TRUNC;
        break;
    case 3:
        m_thresholdSimpleType = cv::THRESH_TOZERO;
        break;
    case 4:
        m_thresholdSimpleType = cv::THRESH_TOZERO_INV;
        break;
    default:
        break;
    }
}

void OpenCV::on_thresholdAdaptiveTypeChanged(const int &type)
{
    switch (type) {
    case 0:
        m_thresholdAdaptiveType = cv::ADAPTIVE_THRESH_MEAN_C;
        break;
    case 1:
        m_thresholdAdaptiveType = cv::ADAPTIVE_THRESH_GAUSSIAN_C;
        break;
    default:
        break;
    }
}

void OpenCV::on_otsuChanged(const int &state) {
    if (state == Qt::Checked) {
        m_isOtsu = true;
    } else {
        m_isOtsu = false;
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

        QRect primaryScreenGeometry = QApplication::primaryScreen()->geometry();

        if (!m_roi.empty())
        {
            int displayWidth = primaryScreenGeometry.width();
            int displayheight = primaryScreenGeometry.height();
            int imgWidth = m_frameOriginal.cols;
            int imgHeight = m_frameOriginal.rows;

            double displayAspect = static_cast<double>(displayWidth) / displayheight;
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

            double scaleX = static_cast<double>(effectiveWidth) / displayWidth;
            double scaleY = static_cast<double>(effectiveHeight) / displayheight;

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

                cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1, 1));
                cv::cvtColor(frameProcessed, frameProcessed, cv::COLOR_BGR2GRAY);

                if (m_thresholdMethod == SIMPLE_THRESHOLD) {
                    int flags = m_thresholdSimpleType;
                    if (m_isOtsu) {
                        flags |= cv::THRESH_OTSU;
                    }
                    cv::threshold(frameProcessed, frameProcessed, m_thresholdValue, 255, flags);
                } else {
                    cv::adaptiveThreshold(frameProcessed, frameProcessed, 255, m_thresholdAdaptiveType, cv::THRESH_BINARY, 11, 2);
                }

                cv::morphologyEx(frameProcessed, frameProcessed, cv::MORPH_OPEN, kernel);

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
