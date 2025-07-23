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

#ifndef OPENCV_H
#define OPENCV_H

#include <QObject>
#include <opencv2/opencv.hpp>

class OpenCV : public QObject
{
    Q_OBJECT

public:
    explicit OpenCV(QObject *parent = nullptr);
    void setIsStopped(bool value) { m_stopped = value; }

signals:
    void currentOriginalFrame(const QImage &frame);
    void currentProcessedFrame(const QImage &frame);
    void currentProcessedMat(const cv::Mat &frame);

public slots:
    void setCurrentRoi(QRect currentRect);
    void setCurrentIgnoreRoi(QRect currentRect);
    void setCurrentFrameBuffer(uint32_t height, uint32_t width, void* data);
    void on_thresholdMethodChanged(const bool &checked);
    void setCurrentThresh(const double &thresh);
    void on_thresholdSimpleTypeChanged(const int &type);
    void on_thresholdAdaptiveTypeChanged(const int &type);
    void on_otsuChanged(const int &state);

private:
    bool m_stopped = false;

    cv::Rect m_roi;
    cv::Rect m_ignoreRoi;

    enum ThresholdMethod {
        SIMPLE_THRESHOLD,
        ADAPTIVE_THRESHOLD
    };

    ThresholdMethod m_thresholdMethod = SIMPLE_THRESHOLD;

    int m_thresholdSimpleType = 0;
    int m_thresholdAdaptiveType = 0;
    bool m_isOtsu = false;
    double m_thresholdValue = 185;

    bool isROIValid(const cv::Rect& roi, const cv::Mat& image);
};
#endif // OPENCV_H
