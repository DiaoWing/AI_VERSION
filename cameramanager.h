// cameramanager.h

#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QImage>
#include <QString>
#include "mvcameraqt.h"
#include "MvCameraControl.h"
#include "configmanager.h"

#define MAX_DEVICE_NUM 4


// 相机信息结构
struct CameraInfo {
    QString name;                   // 相机名称: cam1, cam2, ...
    QString serialNumber;           // 序列号
    QString modelName;              // 型号名称
    QString ipAddress;              // IP地址
    QString userDefinedName;        // 用户自定义名称
    MV_CC_DEVICE_INFO* deviceInfo;  // 设备信息指针
    MvCameraQt* camera;             // 相机操作对象
    bool isOpen;                    // 是否已打开
    bool isGrabbing;                // 是否正在采集
    QImage lastImage;               // 最后一帧图像

    CameraInfo() : deviceInfo(nullptr), camera(nullptr), isOpen(false), isGrabbing(false) {}
};

class CameraManager : public QObject
{
    Q_OBJECT

public:
    static CameraManager* instance();
    ~CameraManager();

    // ===== 相机管理 =====
    bool refreshAllCameras();
    bool enumAndOpenCameras();

    bool startCamera(const QString& name);
    bool stopCamera(const QString& name);
    void startAllCameras();
    void stopAllCameras();

    // ===== 获取信息 =====
    CameraInfo* getCamera(const QString& name);
    QVector<CameraInfo*> getAllCameras();
    bool isCameraOpen(const QString& name);
    bool isCameraGrabbing(const QString& name);

    // ===== 新增：IP配置相关 =====
    bool setCameraIp(const QString& serialNumber,
                     const QString& ip,
                     const QString& subnetMask,
                     const QString& gateway);

    bool setCameraIpByIndex(int deviceIndex,
                            const QString& ip,
                            const QString& subnetMask,
                            const QString& gateway);

    // ===== 新增：带重试的强制IP连接 =====
    bool openCameraWithRetry(CameraInfo* info, int maxRetries = 10);

    // ===== 静态成员 =====
    static std::vector<DeviceInfo> deviceList;
    void closeCamera(CameraInfo* info);

    bool saveCurrentCameraConfig();
signals:
    void cameraStatusChanged(const QString& name, bool isOpen, bool isGrabbing);

    void imageReceived(const QString& name, QImage image);
    void errorOccurred(const QString& error);
    void ipConfigSuccess(const QString& serialNumber, const QString& ip);
    void ipConfigFailed(const QString& serialNumber, const QString& error);

private:
    explicit CameraManager(QObject* parent = nullptr);
    static CameraManager* m_instance;

    // ===== 内部方法 =====
    bool openCamera(CameraInfo* info);

    bool startGrabbing(CameraInfo* info);
    void stopGrabbing(CameraInfo* info);

    // 图像回调（静态）
    static void __stdcall imageCallback(unsigned char* pData,
                                        MV_FRAME_OUT_INFO_EX* pFrameInfo,
                                        void* pUser);

    void processImage(CameraInfo* info, unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo);
    QImage convertToQImage(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo);

    // ===== 新增：IP配置辅助方法 =====
    bool tryOpenCameraDirectly(CameraInfo* info);
    bool tryOpenCameraWithForceIp(CameraInfo* info, int maxRetries = 10);

    // 解析IP地址
    bool parseIpAddress(const QString& ipStr, unsigned int& nIp);
    bool parseSubnetMask(const QString& maskStr, unsigned int& nMask);
    bool parseGateway(const QString& gatewayStr, unsigned int& nGateway);

    // 获取IP前三段
    QString getIpPrefix(const QString& ipAddress);

    // 随机生成IP最后一段
    int generateRandomHost();

    // 强制配置IP
    bool forceConfigureIp(CameraInfo* info,
                          unsigned int nIp,
                          unsigned int nMask,
                          unsigned int nGateway);

    // 重新打开相机（IP变更后）
    bool reopenCameraAfterIpChange(CameraInfo* info,
                                   const QString& newIp,
                                   const QString& newMask,
                                   const QString& newGateway);

    QVector<CameraInfo*> m_cameras;
    QMutex m_mutex;

    static const int MAX_RETRY_COUNT = 10;
    bool  parseIpManual(const QString& ipStr, unsigned int& nIp);
    bool  parseIpToNetwork(const QString& ipStr, unsigned int& nIp);
};

QString fixIpByteOrder(const QString& wrongIp);


#endif // CAMERAMANAGER_H