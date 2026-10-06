#ifndef TOOL_H
#define TOOL_H
#include <QImage>
#include <QDebug>
#include <opencv2/opencv.hpp>
#include <calibrationdialog.h>

class tool
{
public:
    tool();
};

QImage cvMatToQImage(const cv::Mat& mat);
bool needUndistortion(const CalibrationResult& calibrationResult);
cv::Mat qImageToCvMat(const QImage& image);
bool undistortImage(const cv::Mat& image,
                    const CalibrationResult& calibrationResult,
                    cv::Mat& outputImage);
bool undistortImageOptimized(const cv::Mat& image,
                             const CalibrationResult& calibrationResult,
                             cv::Mat& outputImage,
                             double alpha );
bool undistortQImage(const QImage& image,
                     const CalibrationResult& calibrationResult,
                     QImage& outputImage);

#endif // TOOL_H
