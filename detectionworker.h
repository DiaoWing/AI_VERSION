#ifndef DETECTIONWORKER_H
#define DETECTIONWORKER_H

#include <QObject>
#include <QImage>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <opencv2/opencv.hpp>
#include "SegmentationModel.h"
#include "detectionconfigdialog.h"  //配置表
#include "configmanager.h"
#include "parammanager.h"   //直接对注册之后的参数进行使用！自动加载等功能！

//先实现每次都去文件中加载
// 检测任务结构，《先暂时实现一个群组ID对应一个检测项目》序号
struct DetectionTask {

    int groupId;   //群组ID为唯一标识 -- 可以获取显示的序号
    QString projectName;
    QImage image;

};

// 检测结果结构
struct DetectionResult {
    int groupId;
    QImage resultImage;  // 绘制了检测结果的图像
    QString  projectName;
    bool success;        //检测结果
    QString errorMessage;
};

class DetectionWorker : public QObject
{
    Q_OBJECT

public:
    explicit DetectionWorker(QObject* parent = nullptr);
    ~DetectionWorker();

    // 添加检测任务
    void addTask(const DetectionTask& task);

    // 停止处理
    void stop();

    void setDetectionConfigs(const QList<DetectionConfig>& configs);

signals:
    // 检测完成信号（发送到UI线程）
    void detectionFinished(const DetectionResult& result);
    void debugInfo(const QString& info);

private slots:
    void processLoop();

private:
    // 实际的检测函数
    DetectionResult doDetection(const DetectionTask& task);

    // 图像转换
    cv::Mat qImageToMat(const QImage& image);
    QImage matToQImage(cv::Mat& mat);


    QQueue<DetectionTask> m_taskQueue;
    QMutex m_mutex;
    QWaitCondition m_condition;
    bool m_isRunning;
    QList<DetectionConfig> m_configs;  // 添加配置成员

public:

    std::unique_ptr<SegmentationModel> m_segmentationModel;
    bool initModel();
    bool m_modelLoaded = false;
    cv::Mat  runInference(cv::Mat img);

    // ===== 各类检测函数 =====
    DetectionResult runInference(const cv::Mat& image);  //海岸线检测
    DetectionResult defaultDetection(const cv::Mat& image); //默认检测项目

    //是否修改之后，在此处检测时，就将其加载到内存中，速度更快！

    // ===== 注册表类型改为返回 DetectionResult =====
    using DetectionFunc = DetectionResult (DetectionWorker::*)(const cv::Mat&);
    QMap<QString, DetectionFunc> m_detectionFuncMap;

    // ===== 注册检测函数 =====
    void registerDetection(const QString& projectName);
    void registerAllDetections();
};

#endif // DETECTIONWORKER_H