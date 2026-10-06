#include "tool.h"


tool::tool() {}

// ===== 辅助转换函数 =====

/**
 * @brief cv::Mat 转 QImage
 */
QImage cvMatToQImage(const cv::Mat& mat)
{
    if (mat.empty()) {
        return QImage();
    }

    switch (mat.type()) {
    case CV_8UC1: {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return image.copy();
    }
    case CV_8UC3: {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        return image.copy();
    }
    case CV_8UC4: {
        cv::Mat bgra;
        cv::cvtColor(mat, bgra, cv::COLOR_BGRA2RGBA);
        QImage image(bgra.data, bgra.cols, bgra.rows, bgra.step, QImage::Format_RGBA8888);
        return image.copy();
    }
    default: {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        return image.copy();
    }
    }
}

/**
 * @brief 检查图像是否需要校正
 */
bool needUndistortion(const CalibrationResult& calibrationResult)
{
    if (!calibrationResult.success) {
        return false;
    }

    // 检查畸变系数是否接近零
    if (!calibrationResult.distCoeffs.empty()) {
        double sum = cv::sum(calibrationResult.distCoeffs)[0];
        if (std::abs(sum) < 1e-6) {
            qDebug() << "[Undistort] ⚠️ 畸变系数接近零，可能不需要校正";
            return false;
        }
    }

    return true;
}

/**
 * @brief QImage 转 cv::Mat
 */
cv::Mat qImageToCvMat(const QImage& image)
{
    if (image.isNull()) {
        return cv::Mat();
    }

    switch (image.format()) {
    case QImage::Format_RGB888: {
        cv::Mat mat(image.height(), image.width(), CV_8UC3,
                    (void*)image.constBits(), image.bytesPerLine());
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
        return mat.clone();
    }
    case QImage::Format_BGR888: {
        return cv::Mat(image.height(), image.width(), CV_8UC3,
                       (void*)image.constBits(), image.bytesPerLine()).clone();
    }
    case QImage::Format_Grayscale8: {
        return cv::Mat(image.height(), image.width(), CV_8UC1,
                       (void*)image.constBits(), image.bytesPerLine()).clone();
    }
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied: {
        QImage rgb = image.convertToFormat(QImage::Format_RGB888);
        cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3,
                    (void*)rgb.constBits(), rgb.bytesPerLine());
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
        return mat.clone();
    }
    default: {
        QImage rgb = image.convertToFormat(QImage::Format_RGB888);
        cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3,
                    (void*)rgb.constBits(), rgb.bytesPerLine());
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
        return mat.clone();
    }
    }
}



/**
 * @brief 使用标定参数对图像进行畸变校正
 * @param image 输入的原始图像（cv::Mat格式）
 * @param calibrationResult 标定结果
 * @param outputImage 校正后的图像（输出参数）
 * @return 是否校正成功
 */
bool undistortImage(const cv::Mat& image,
                    const CalibrationResult& calibrationResult,
                    cv::Mat& outputImage)
{
    // ===== 1. 检查输入 =====
    if (image.empty()) {
        qDebug() << "[Undistort] ❌ 输入图像为空";
        return false;
    }

    if (!calibrationResult.success) {
        qDebug() << "[Undistort] ❌ 标定结果无效";
        return false;
    }

    if (calibrationResult.cameraMatrix.empty() || calibrationResult.distCoeffs.empty()) {
        qDebug() << "[Undistort] ❌ 相机矩阵或畸变系数为空";
        return false;
    }

    // ===== 2. 检查图像尺寸是否匹配 =====
    if (calibrationResult.imageWidth > 0 && calibrationResult.imageHeight > 0) {
        if (image.cols != calibrationResult.imageWidth ||
            image.rows != calibrationResult.imageHeight) {
            qDebug() << "[Undistort] ⚠️ 图像尺寸与标定尺寸不匹配:"
                     << "图像=" << image.cols << "x" << image.rows
                     << "标定=" << calibrationResult.imageWidth << "x" << calibrationResult.imageHeight;
            // 可以选择继续处理或返回false
        }
    }

    // ===== 3. 执行畸变校正 =====
    try {
        qDebug() << "[Undistort] 开始图像校正..."
                 << "尺寸:" << image.cols << "x" << image.rows
                 << "重投影误差:" << calibrationResult.reprojectionError;

        // 方法1：使用 undistort（简单直接）
        cv::undistort(image, outputImage,
                      calibrationResult.cameraMatrix,
                      calibrationResult.distCoeffs);

        qDebug() << "[Undistort] ✅ 图像校正成功";
        return true;

    } catch (const cv::Exception& e) {
        qDebug() << "[Undistort] ❌ OpenCV异常:" << e.what();
        return false;
    } catch (...) {
        qDebug() << "[Undistort] ❌ 未知异常";
        return false;
    }
}

/**
 * @brief 使用标定参数对图像进行校正（带缩放和裁剪优化）
 * @param image 输入图像
 * @param calibrationResult 标定结果
 * @param outputImage 校正后的图像
 * @param alpha 缩放系数（0-1），0表示裁剪黑边，1表示保留全部
 * @return 是否成功
 */
bool undistortImageOptimized(const cv::Mat& image,
                             const CalibrationResult& calibrationResult,
                             cv::Mat& outputImage,
                             double alpha = 0.0)
{
    if (image.empty() || !calibrationResult.success) {
        return false;
    }

    try {
        // 获取新的相机矩阵（优化视角）
        cv::Mat newCameraMatrix = cv::getOptimalNewCameraMatrix(
            calibrationResult.cameraMatrix,
            calibrationResult.distCoeffs,
            image.size(),
            alpha,
            image.size(),
            nullptr
            );

        // 执行校正
        cv::undistort(image, outputImage,
                      calibrationResult.cameraMatrix,
                      calibrationResult.distCoeffs,
                      newCameraMatrix);

        qDebug() << "[Undistort] ✅ 优化校正成功 (alpha=" << alpha << ")";
        return true;

    } catch (const cv::Exception& e) {
        qDebug() << "[Undistort] ❌ OpenCV异常:" << e.what();
        return false;
    }
}

/**
 * @brief QImage版本的图像校正
 * @param image 输入的QImage
 * @param calibrationResult 标定结果
 * @param outputImage 校正后的QImage（输出参数）
 * @return 是否成功
 */
bool undistortQImage(const QImage& image,
                     const CalibrationResult& calibrationResult,
                     QImage& outputImage)
{
    if (image.isNull()) {
        qDebug() << "[Undistort] ❌ QImage为空";
        return false;
    }

    // QImage → cv::Mat
    cv::Mat cvImage = qImageToCvMat(image);
    if (cvImage.empty()) {
        qDebug() << "[Undistort] ❌ QImage转cv::Mat失败";
        return false;
    }

    // 执行校正
    cv::Mat correctedMat;
    if (!undistortImage(cvImage, calibrationResult, correctedMat)) {
        return false;
    }

    // cv::Mat → QImage
    outputImage = cvMatToQImage(correctedMat);
    return !outputImage.isNull();
}


