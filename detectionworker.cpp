#include "detectionworker.h"
#include <QDebug>
#include <QDateTime>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <QtConcurrent/QtConcurrent>
#include <QMessageBox>

DetectionWorker::DetectionWorker(QObject* parent)
    : QObject(parent)
    , m_isRunning(true)
{
    initModel();
    registerAllDetections();
    QtConcurrent::run(std::bind(&DetectionWorker::processLoop, this));
}

DetectionWorker::~DetectionWorker()
{
    stop();
}

void DetectionWorker::stop()
{
    m_isRunning = false;
    m_condition.wakeAll();
}

void DetectionWorker::setDetectionConfigs(const QList<DetectionConfig>& configs)
{
    m_configs = configs;
    emit debugInfo(QString("检测配置已加载: %1 个项目").arg(configs.size()));
}

//将其添加到队列中，其触发的是那个函数？
void DetectionWorker::addTask(const DetectionTask& task)
{
    QMutexLocker locker(&m_mutex);
    m_taskQueue.enqueue(task);
    m_condition.wakeOne();

    emit debugInfo(QString("检测任务入队: ID=%1, 项目=%2")
                       .arg(task.groupId).arg(task.projectName));
}

void DetectionWorker::processLoop()
{
    while (m_isRunning) {
        DetectionTask task;

        {
            QMutexLocker locker(&m_mutex);
            while (m_taskQueue.isEmpty() && m_isRunning) {
                m_condition.wait(&m_mutex, 100);
            }

            if (!m_isRunning) break;

            if (m_taskQueue.isEmpty()) continue;

            task = m_taskQueue.dequeue();
        }

        DetectionResult result = doDetection(task);

        // 发送结果到UI线程
        emit detectionFinished(result);
    }
}

cv::Mat DetectionWorker::runInference(cv::Mat img) {
    // 检查模型是否已加载
    if (!m_modelLoaded || !m_segmentationModel) {
        qDebug() << "❌ 模型未加载！";
        return cv::Mat();
    }

    if (img.empty()) {
        qDebug() << "❌ 输入图像为空!";
        return cv::Mat();
    }

    //imwrite("E:/temp_pic/fdasf.png",img);

    qDebug() << "✅ 开始推理，图像尺寸: " << img.cols << "x" << img.rows;

    cv::Mat resized_img;
    cv::resize(img,resized_img,cv::Size(256 ,256));

    // 执行推理
    if (!m_segmentationModel->predict(resized_img, 0.5f)) {
        qDebug() << "❌ 推理失败!";
        return cv::Mat();
    }

    // 获取结果
    cv::Mat mask = m_segmentationModel->getMask();
    cv::Mat prob = m_segmentationModel->getProbabilityMap();

    qDebug() << "✅ 推理成功! 覆盖率: " << m_segmentationModel->getMaskCoverage() << "%";

     cv::imwrite("E:/temp_pic/dafas__1.png",mask);
    // 生成叠加图像
    cv::Mat display_mask;
    cv::resize(mask, display_mask, img.size());

    cv::Mat overlay = img.clone();
    for (int i = 0; i < display_mask.rows; i++) {
        for (int j = 0; j < display_mask.cols; j++) {
            if (display_mask.at<unsigned char>(i, j) > 0) {
                overlay.at<cv::Vec3b>(i, j) = cv::Vec3b(0, 0, 255);
            }
        }
    }

    return overlay;
}


//载入对应的模型
bool DetectionWorker::initModel()
{
    if (m_modelLoaded) return true;

    try {
        std::string model_path = "E:/dataset/notebookscatch/TRAIN_MODEL/model.pt";             //"E:/dataset/seg_data_new/model/unet_model.pt";

        m_segmentationModel = std::make_unique<SegmentationModel>(
            model_path,
            256,   // input_width
            256,   // input_height
            false  // use_cuda
            );

        m_modelLoaded = m_segmentationModel->isValid();

        if (m_modelLoaded) {
            emit debugInfo("✅ 分割模型加载成功！");
            cv::Size inputSize = m_segmentationModel->getInputSize();
            emit debugInfo(QString("   输入尺寸: %1x%2")
                               .arg(inputSize.width).arg(inputSize.height));
        } else {
            emit debugInfo("❌ 分割模型加载失败！");
        }

        return m_modelLoaded;
    } catch (const std::exception& e) {
        emit debugInfo(QString("❌ 模型加载异常: %1").arg(e.what()));
        m_modelLoaded = false;
        return false;
    }
}


DetectionResult DetectionWorker::doDetection(const DetectionTask& task)
{
    DetectionResult result;
    result.groupId = task.groupId;
    result.projectName = task.projectName;
    result.success = true;

    emit debugInfo(QString("开始检测: 项目=%1, groupId=%2").arg(task.projectName).arg(task.groupId));

    try {
        cv::Mat mat = qImageToMat(task.image);

        if (mat.empty()) {
            result.success = false;
            result.errorMessage = "图像为空";
            return result;
        }

        if (m_detectionFuncMap.contains(task.projectName)) {
            DetectionFunc func = m_detectionFuncMap[task.projectName];
            DetectionResult funcResult = (this->*func)(mat);

            // ✅ 合并结果，但保留 projectName 和 groupId
            result.success = funcResult.success;
            result.resultImage = funcResult.resultImage;
            result.errorMessage = funcResult.errorMessage;
            // projectName 和 groupId 保持不变

            emit debugInfo(QString("检测完成: %1").arg(task.projectName));
        } else {
            emit debugInfo(QString("未找到检测函数: projectName=%1，返回原图").arg(task.projectName));
            result.resultImage = matToQImage(mat);
            result.success = true;
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = QString("检测异常: %1").arg(e.what());
        emit debugInfo(result.errorMessage);
    }

    return result;
}

// ===== 图像转换函数 =====
cv::Mat DetectionWorker::qImageToMat(const QImage& image)
{
    if (image.isNull()) return cv::Mat();
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3,
                (void*)rgb.constBits(), rgb.bytesPerLine());
    cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
    return mat.clone();
}

QImage DetectionWorker::matToQImage(cv::Mat& mat)
{
    if (mat.empty()) {
        return QImage();
    }

    switch (mat.type()) {
    case CV_8UC1: {
        // 灰度图：直接复制
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return image.copy();  // 深拷贝
    }
    case CV_8UC3: {
        // BGR 转 RGB，然后复制
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
        return image.copy();  // ✅ 深拷贝，数据独立
    }
    case CV_8UC4: {
        // BGRA 转 ARGB（Qt 格式）
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        QImage image(rgba.data, rgba.cols, rgba.rows, rgba.step, QImage::Format_RGBA8888);
        return image.copy();  // ✅ 深拷贝
    }
    default: {
        // 其他格式：先转换为 8UC3
        cv::Mat rgb8;
        mat.convertTo(rgb8, CV_8UC3);
        cv::cvtColor(rgb8, rgb8, cv::COLOR_BGR2RGB);
        QImage image(rgb8.data, rgb8.cols, rgb8.rows, rgb8.step, QImage::Format_RGB888);
        return image.copy();  // ✅ 深拷贝
    }
    }
}


// ============================================================
// 注册检测函数（核心）
// ============================================================
void DetectionWorker::registerDetection(const QString& projectName)
{
    if (m_detectionFuncMap.contains(projectName)) {
        qDebug() << "[DetectionWorker] 已注册，跳过: " << projectName;
        return;
    }

    DetectionFunc func = nullptr;

    if (projectName.contains("海岸线检测")) {
        func = &DetectionWorker::runInference;
    } else if (projectName.contains("空项目")) {
        // ✅ 为空项目添加一个默认检测函数
        func = &DetectionWorker::defaultDetection;
    } else {
        func = nullptr;
        qDebug() << "[DetectionWorker] 未知检测类型，将返回原图: " << projectName;
    }

    if (func) {
        m_detectionFuncMap[projectName] = func;
        qDebug() << "[DetectionWorker] ✅ 注册检测: " << projectName;
    }
}

// ============================================================
// 各类检测函数（现在返回 DetectionResult）
// ============================================================
DetectionResult DetectionWorker::defaultDetection(const cv::Mat& image)
{
    DetectionResult result;
    result.success = true;

    // 在原图上添加文字标记
    cv::Mat output = image.clone();
    cv::putText(output, "No Detection", cv::Point(10, 30),
                cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 255), 2);

    result.resultImage = matToQImage(output);
    return result;
}

// -----  海岸线检测（基于模型） -----
DetectionResult DetectionWorker::runInference(const cv::Mat& image)
{
    //比如此处需要对其进行使用，不可能每次都对其进行重新加载！对单例中的创建的结构体进行更新不就行了
    //此处直接使用单例不是不可以
    // ===== 获取参数（直接从单例中获取，不需要重新加载） =====
    ParamManager* pm = ParamManager::instance();

    // 获取海岸线检测参数
    QString coastlineType = pm->getStringParam("海岸线检测", "coastlineType", "v1");

    // 打印获取到的参数
    qDebug() << "[Param] 海岸线类型: " << coastlineType;


    DetectionResult result;
    result.success = true;

    qDebug() << "[Detection] 执行模型推理（缺陷检测）";

    if (!m_modelLoaded || !m_segmentationModel) {
        result.success = false;
        result.errorMessage = "模型未加载";
        return result;
    }

    if (image.empty()) {
        result.success = false;
        result.errorMessage = "图像为空";
        return result;
    }

    cv::Mat resized_img;
    cv::resize(image, resized_img, cv::Size(256, 256));

    if (!m_segmentationModel->predict(resized_img, 0.5f)) {
        result.success = false;
        result.errorMessage = "推理失败";
        return result;
    }

    cv::Mat mask = m_segmentationModel->getMask();
    qDebug() << "✅ 推理成功! 覆盖率: " << m_segmentationModel->getMaskCoverage() << "%";

    // 生成叠加图像
    cv::Mat display_mask;
    cv::resize(mask, display_mask, image.size());

    cv::Mat overlay = image.clone();
    for (int i = 0; i < display_mask.rows; i++) {
        for (int j = 0; j < display_mask.cols; j++) {
            if (display_mask.at<unsigned char>(i, j) > 0) {
                overlay.at<cv::Vec3b>(i, j) = cv::Vec3b(0, 0, 255);
            }
        }
    }

    cv::putText(overlay, "detection_over", cv::Point(10, 30),
                cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);

    result.resultImage = matToQImage(overlay);
    result.success = true;
    return result;
}


void DetectionWorker::registerAllDetections()
{
    // 预注册一些默认的检测（可选）
    // 实际会通过 setDetectionConfigs() 动态注册
    qDebug() << "[DetectionWorker] 等待配置加载后注册检测函数";

    // 也可以手动注册一些默认的
    registerDetection("海岸线检测");
    registerDetection("空项目");
}
