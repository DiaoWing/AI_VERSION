#include "cameramanager.h"
#include <QDateTime>
#include <QDebug>
#include <QImage>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QThread>

CameraManager* CameraManager::m_instance = nullptr;
std::vector<DeviceInfo> CameraManager::deviceList;  //初始化

CameraManager* CameraManager::instance()
{
    if (!m_instance) {
        m_instance = new CameraManager();
    }
    return m_instance;
}

CameraManager::CameraManager(QObject* parent)
    : QObject(parent)
{
    // 初始化时就对应创建保存的信息结构体
    for (int i = 1; i <= MAX_DEVICE_NUM; i++) {
        CameraInfo* info = new CameraInfo();   //创建相机的对应信息
        info->name = QString("cam%1").arg(i);
        info->camera = new MvCameraQt();    //其中能够对相机进行操作的对象
        m_cameras.append(info);
    }

    //sdk初始化
    if (!MvCameraQt::initSDK()) {
        emit errorOccurred("SDK初始化失败");
    }
}

CameraManager::~CameraManager()
{
    stopAllCameras();
    for (CameraInfo* info : m_cameras) {
        if (info->camera) {
            delete info->camera;
        }
        delete info;
    }
    m_cameras.clear();
    MvCameraQt::finalizeSDK();
}

//刷新相机
bool CameraManager::refreshAllCameras()
{
    QMutexLocker locker(&m_mutex);

    qDebug() << "=== 开始刷新所有相机 ===";

    //先是停止采集
    stopAllCameras();

    //遍历对应的相机存储信息，对其进行关闭
    for (CameraInfo* info : m_cameras) {
        closeCamera(info);
    }

    //枚举 并且打开相机，此处需要对相机信息进行存储
    bool result = enumAndOpenCameras();

    if (result) {
        qDebug() << "=== 刷新完成 ===";
    } else {
        qDebug() << "=== 刷新失败 ===";
    }

    return result;
}

// bool CameraManager::enumAndOpenCameras()
// {
//     // =========删除局部std::vector<DeviceInfo> deviceList;=========
//     QString errorMsg;

//     //枚举全部的相机放入【全局静态】deviceList中
//     if (!MvCameraQt::enumDevices(CameraManager::deviceList, errorMsg)) {
//         emit errorOccurred(QString("枚举设备失败: %1").arg(errorMsg));
//         return false;
//     }

//     auto& deviceList = CameraManager::deviceList; //引用全局静态
//     if (deviceList.empty()) {
//         emit errorOccurred("未找到任何相机设备");
//         return false;
//     }

//     qDebug() << "找到" << deviceList.size() << "个设备";

//     int count = qMin(static_cast<int>(deviceList.size()), m_cameras.size());
//     for (int i = 0; i < count; i++) {
//         CameraInfo* info = m_cameras[i];
//         DeviceInfo& devInfo = deviceList[i];

//         info->serialNumber = devInfo.serialNumber;
//         info->modelName = devInfo.modelName;
//         info->ipAddress = devInfo.ipAddress;
//         info->deviceInfo = (MV_CC_DEVICE_INFO*)devInfo.deviceHandle;

//         qDebug() << "分配设备到" << info->name
//                  << "型号:" << info->modelName
//                  << "序列号:" << info->serialNumber;

//         if (!openCamera(info)) {
//             qWarning() << "打开相机失败:" << info->name;
//         }
//     }

//     for (int i = count; i < m_cameras.size(); i++) {
//         CameraInfo* info = m_cameras[i];
//         info->serialNumber.clear();
//         info->modelName.clear();
//         info->ipAddress.clear();
//         info->deviceInfo = nullptr;
//         info->isOpen = false;
//         info->isGrabbing = false;
//         qDebug() << info->name << "未分配设备";
//     }

//     return true;
// }

bool CameraManager::openCamera(CameraInfo* info)
{
    if (!info || !info->deviceInfo) {
        return false;
    }

    if (info->isOpen) {
        closeCamera(info);
    }

    int ret = info->camera->open(info->deviceInfo);
    if (ret != MV_OK) {
        QString error = QString("打开相机 %1 失败: %2").arg(info->name).arg(ret);
        emit errorOccurred(error);
        info->isOpen = false;
        return false;
    }

    info->isOpen = true;
    qDebug() << info->name << "打开成功";

    //回调就是先对其进行打开，然后在注册回调，注册回调之后，对其进行开始采集，然后有图，触发回调，开始显示图像
    ret = info->camera->registerImageCallBack(imageCallback, info);//
    if (ret != MV_OK) {
        qWarning() << info->name << "注册回调失败:" << ret;
    }

    return true;
    //return startGrabbing(info);
}

void CameraManager::closeCamera(CameraInfo* info)
{
    if (!info || !info->isOpen) {
        return;
    }

    stopGrabbing(info);
    info->camera->close();
    info->isOpen = false;
    info->isGrabbing = false;

    qDebug() << info->name << "已关闭";
}

bool CameraManager::startGrabbing(CameraInfo* info)
{
    if (!info || !info->isOpen || info->isGrabbing) {
        return false;
    }

    int ret = info->camera->startGrabbing();
    if (ret != MV_OK) {
        QString error = QString("开始采集 %1 失败: %2").arg(info->name).arg(ret);
        emit errorOccurred(error);
        info->isGrabbing = false;
        return false;
    }

    info->isGrabbing = true;
    qDebug() << info->name << "开始采集";

    emit cameraStatusChanged(info->name, info->isOpen, info->isGrabbing);
    return true;
}

void CameraManager::stopGrabbing(CameraInfo* info)
{
    if (!info || !info->isGrabbing) {
        return;
    }

    info->camera->stopGrabbing();
    info->isGrabbing = false;

    qDebug() << info->name << "停止采集";

    emit cameraStatusChanged(info->name, info->isOpen, info->isGrabbing);
}

bool CameraManager::startCamera(const QString& name)
{
    for (CameraInfo* info : m_cameras) {
        if (info->name == name) {
            return startGrabbing(info);
        }
    }
    return false;
}

bool CameraManager::stopCamera(const QString& name)
{
    for (CameraInfo* info : m_cameras) {
        if (info->name == name) {
            stopGrabbing(info);
            return true;
        }
    }
    return false;
}

void CameraManager::stopAllCameras()
{
    for (CameraInfo* info : m_cameras) {
        stopGrabbing(info);
    }
}

void CameraManager::startAllCameras()
{
    for (CameraInfo* info : m_cameras) {
        startGrabbing(info);
    }
}

CameraInfo* CameraManager::getCamera(const QString& name)
{
    for (CameraInfo* info : m_cameras) {
        if (info->name == name) {
            return info;
        }
    }
    return nullptr;
}

QVector<CameraInfo*> CameraManager::getAllCameras()
{
    return m_cameras;
}

bool CameraManager::isCameraOpen(const QString& name)
{
    CameraInfo* info = getCamera(name);
    return info ? info->isOpen : false;
}

bool CameraManager::isCameraGrabbing(const QString& name)
{
    CameraInfo* info = getCamera(name);
    return info ? info->isGrabbing : false;
}

void __stdcall CameraManager::imageCallback(unsigned char* pData,
                                            MV_FRAME_OUT_INFO_EX* pFrameInfo,
                                            void* pUser)
{
    // ❌ 错误：静态函数不能访问非静态成员
    CameraInfo* info = static_cast<CameraInfo*>(pUser);
    if (info && info->isGrabbing) {
        CameraManager::instance()->processImage(info, pData, pFrameInfo);   //单例调用非静态成员
    }
}


//采集线程需要自己创建吗，并不是，而是在开始采集时创建的
//采集线程并不是在你注册回调时创建的，而是在你调用 startGrabbing() 时由SDK内部创建的。
void CameraManager::processImage(CameraInfo* info, unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo)
{
    if (!pData || !pFrameInfo) {
        return;
    }

    // 转换为QImage
    QImage image = convertToQImage(pData, pFrameInfo);
    if (!image.isNull()) {
        info->lastImage = image;
        emit imageReceived(info->name, image); //将回调线程中的图像发送到UI线程
    }
}

QImage CameraManager::convertToQImage(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo)
{
    int width = pFrameInfo->nWidth;
    int height = pFrameInfo->nHeight;

    switch (pFrameInfo->enPixelType) {
    case PixelType_Gvsp_Mono8: {
        QImage image(width, height, QImage::Format_Grayscale8);
        memcpy(image.bits(), pData, static_cast<size_t>(width * height));
        return image;
    }
    case PixelType_Gvsp_Mono10:
    case PixelType_Gvsp_Mono12:
    case PixelType_Gvsp_Mono14:
    case PixelType_Gvsp_Mono16: {
        //16bit灰度，转QImage Format_Grayscale16
        QImage image(width, height, QImage::Format_Grayscale16);
        memcpy(image.bits(), pData, static_cast<size_t>(width*height*2));
        return image;
    }

    case PixelType_Gvsp_BayerGR8:
    case PixelType_Gvsp_BayerRG8:
    case PixelType_Gvsp_BayerGB8:
    case PixelType_Gvsp_BayerBG8: {
        QImage image(width, height, QImage::Format_Grayscale8);
        memcpy(image.bits(), pData, static_cast<size_t>(width * height));
        return image;
    }

    case PixelType_Gvsp_RGB8_Packed: {
        QImage image(width, height, QImage::Format_RGB888);
        memcpy(image.bits(), pData, static_cast<size_t>(width*height*3));
        return image;
    }

    case PixelType_Gvsp_BGR8_Packed: {
        QImage image(width, height, QImage::Format_RGB888);
        unsigned char* dst = image.bits();
        const unsigned char* src = pData;
        for (int i = 0; i < width * height; i++) {
            dst[i*3] = src[i*3 + 2];
            dst[i*3 + 1] = src[i*3 + 1];
            dst[i*3 + 2] = src[i*3];
        }
        return image;
    }

    default: {
        qDebug() << "未知像素格式:" << pFrameInfo->enPixelType;
        QImage image(width, height, QImage::Format_Grayscale8);
        if (pFrameInfo->nFrameLen >= static_cast<unsigned int>(width * height)) {
            memcpy(image.bits(), pData, static_cast<size_t>(width * height));
        }
        return image;
    }
    }
}

bool CameraManager::setCameraIp(const QString& serialNumber,
                                const QString& ip,
                                const QString& subnetMask,
                                const QString& gateway)
{
    QMutexLocker locker(&m_mutex);

    // 1. 查找对应的设备
    int deviceIndex = -1;
    for (size_t i = 0; i < deviceList.size(); i++) {
        if (deviceList[i].serialNumber == serialNumber) {
            deviceIndex = static_cast<int>(i);
            break;
        }
    }

    if (deviceIndex < 0) {
        emit errorOccurred(QString("未找到序列号为 %1 的设备").arg(serialNumber));
        return false;
    }

    return setCameraIpByIndex(deviceIndex, ip, subnetMask, gateway);
}

bool CameraManager::setCameraIpByIndex(int deviceIndex,
                                       const QString& ip,
                                       const QString& subnetMask,
                                       const QString& gateway)
{
    QMutexLocker locker(&m_mutex);

    if (deviceIndex < 0 || deviceIndex >= static_cast<int>(deviceList.size())) {
        emit errorOccurred(QString("无效的设备索引: %1").arg(deviceIndex));
        return false;
    }

    // 解析IP地址
    unsigned int nIp = 0, nMask = 0, nGateway = 0;
    if (!parseIpAddress(ip, nIp)) {
        emit errorOccurred(QString("无效的IP地址: %1").arg(ip));
        return false;
    }
    if (!parseSubnetMask(subnetMask, nMask)) {
        emit errorOccurred(QString("无效的子网掩码: %1").arg(subnetMask));
        return false;
    }
    if (!parseGateway(gateway, nGateway)) {
        emit errorOccurred(QString("无效的网关: %1").arg(gateway));
        return false;
    }

    // 获取对应的 CameraInfo
    CameraInfo* info = nullptr;
    for (CameraInfo* cam : m_cameras) {
        if (cam->serialNumber == deviceList[deviceIndex].serialNumber) {
            info = cam;
            break;
        }
    }

    if (!info) {
        emit errorOccurred("未找到对应的相机信息");
        return false;
    }

    // 执行强制IP配置
    return forceConfigureIp(info, nIp, nMask, nGateway);
}

// ============================================================
// 核心：带重试的自动IP配置连接
// ============================================================

bool CameraManager::openCameraWithRetry(CameraInfo* info, int maxRetries)
{
    if (!info || !info->deviceInfo) {
        emit errorOccurred("无效的相机信息");
        return false;
    }

    qDebug() << "=== 尝试连接相机:" << info->name << "===";
    qDebug() << "序列号:" << info->serialNumber;
    qDebug() << "当前IP:" << info->ipAddress;

    // ===== 第一步：尝试直接连接 =====
    if (tryOpenCameraDirectly(info)) {
        qDebug() << "✅ 直接连接成功:" << info->ipAddress;
        return true;
    }

    qDebug() << "⚠️ 直接连接失败，尝试强制IP配置...";

    // ===== 第二步：强制IP配置并重试 =====
    return tryOpenCameraWithForceIp(info, maxRetries);
}

bool CameraManager::tryOpenCameraDirectly(CameraInfo* info)
{
    if (!info || !info->deviceInfo) {
        return false;
    }

    // 检查设备是否可达
    bool accessible = MvCameraQt::isDeviceAccessible(info->deviceInfo, MV_ACCESS_Exclusive);

    if (!accessible) {
        qDebug() << "设备不可达，直接连接失败";

        //不可达，但是可以直接尝试打开
        return openCamera(info);
    }

}

bool CameraManager::parseIpManual(const QString& ipStr, unsigned int& nIp)
{
    QStringList parts = ipStr.split('.');
    if (parts.size() != 4) return false;

    bool ok[4];
    int p[4];
    for (int i = 0; i < 4; i++) {
        p[i] = parts[i].toInt(&ok[i]);
        if (!ok[i] || p[i] < 0 || p[i] > 255) return false;
    }

    // 网络字节序：高位在前
    nIp = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    return true;
}

bool CameraManager::tryOpenCameraWithForceIp(CameraInfo* info, int maxRetries)
{
    if (!info) {
        return false;
    }

    // 获取IP前缀（前三段）
    QString ipPrefix = getIpPrefix(info->ipAddress);
    if (ipPrefix.isEmpty()) {
        ipPrefix = "192.168.1";
        qDebug() << "使用默认网段:" << ipPrefix;
    }

    // 默认值
    QString subnetMask = "255.255.255.0";
    QString gateway = ipPrefix + ".1";

    // 如果设备信息中有子网掩码和网关，尝试读取
    if (info->deviceInfo) {
        MV_GIGE_DEVICE_INFO& gigEInfo = info->deviceInfo->SpecialInfo.stGigEInfo;

        // ✅ 读取并修正子网掩码
        unsigned int mask = gigEInfo.nCurrentSubNetMask;
        if (mask != 0) {
            // 反转字节序
            unsigned int hostMask =
                ((mask & 0xFF000000) >> 24) |
                ((mask & 0x00FF0000) >> 8)  |
                ((mask & 0x0000FF00) << 8)  |
                ((mask & 0x000000FF) << 24);

            subnetMask = QString("%1.%2.%3.%4")
                             .arg((hostMask >> 24) & 0xFF)
                             .arg((hostMask >> 16) & 0xFF)
                             .arg((hostMask >> 8) & 0xFF)
                             .arg(hostMask & 0xFF);
            subnetMask  =fixIpByteOrder(subnetMask);
        }

        // ✅ 读取并修正网关
        unsigned int gw = gigEInfo.nDefultGateWay;
        if (gw != 0) {
            unsigned int hostGw =
                ((gw & 0xFF000000) >> 24) |
                ((gw & 0x00FF0000) >> 8)  |
                ((gw & 0x0000FF00) << 8)  |
                ((gw & 0x000000FF) << 24);

            gateway = QString("%1.%2.%3.%4")
                          .arg((hostGw >> 24) & 0xFF)
                          .arg((hostGw >> 16) & 0xFF)
                          .arg((hostGw >> 8) & 0xFF)
                          .arg(hostGw & 0xFF);
            gateway  =fixIpByteOrder(gateway);
        }
    }

    qDebug() << "IP前缀:" << ipPrefix;
    qDebug() << "子网掩码:" << subnetMask;
    qDebug() << "网关:" << gateway;

    // 尝试多个IP地址
    for (int attempt = 0; attempt < maxRetries; attempt++) {
        int host = generateRandomHost();
        QString testIp = QString("%1.%2").arg(ipPrefix).arg(host);

        qDebug() << "尝试 #" << (attempt + 1) << " IP:" << testIp;

        unsigned int nIp = 0, nMask = 0, nGateway = 0;

        // ✅ 使用手动解析，直接得到网络字节序
        if (!parseIpToNetwork(testIp, nIp) ||
            !parseIpToNetwork(subnetMask, nMask) ||
            !parseIpToNetwork(gateway, nGateway)) {
            continue;
        }

        qDebug() << "网络字节序 - IP: 0x" << QString::number(nIp, 16)
                 << "掩码: 0x" << QString::number(nMask, 16)
                 << "网关: 0x" << QString::number(nGateway, 16);

        if (forceConfigureIp(info, nIp, nMask, nGateway)) {
            qDebug() << "✅ 强制IP配置成功:" << testIp;

            if (reopenCameraAfterIpChange(info, testIp, subnetMask, gateway)) {
                return true;
            }
        }

        QThread::msleep(100);
    }

    emit errorOccurred(QString("强制IP配置失败，已尝试 %1 次").arg(maxRetries));
    return false;
}
// ============================================================
// IP配置辅助方法
// ============================================================

bool CameraManager::forceConfigureIp(CameraInfo* info,
                                     unsigned int nIp,
                                     unsigned int nMask,
                                     unsigned int nGateway)
{
    if (!info || !info->camera) {
        return false;
    }

    qDebug() << "执行强制IP配置...";
    qDebug() << "IP: 0x" << QString::number(nIp, 16);
    qDebug() << "掩码: 0x" << QString::number(nMask, 16);
    qDebug() << "网关: 0x" << QString::number(nGateway, 16);

    // 1. 先关闭相机（如果已打开）
    if (info->isOpen) {
        closeCamera(info);
    }

    // 2. ✅ 创建句柄（不打开设备）
    void* handle = nullptr;
    int ret = MV_CC_CreateHandle(&handle, info->deviceInfo);
    if (ret != MV_OK) {
        qWarning() << "创建设备句柄失败，错误码:" << ret;
        return false;
    }
    qDebug() << "创建设备句柄成功";

    // 3. 检查设备是否可达
    bool accessible = MvCameraQt::isDeviceAccessible(info->deviceInfo, MV_ACCESS_Exclusive);
    qDebug() << "设备是否可达:" << accessible;

    if (accessible) {
        // 4a. 可达：先设置静态IP模式
        ret = MV_GIGE_SetIpConfig(handle, MV_IP_CFG_STATIC);
        if (ret != MV_OK) {
            qWarning() << "设置IP配置模式失败，错误码:" << ret;
            MV_CC_DestroyHandle(handle);
            return false;
        }
        qDebug() << "设置IP配置模式成功";
    } else {
        // 4b. 不可达：直接强制IP
        qDebug() << "设备不可达，直接强制IP配置";
    }

    // 5. 强制配置IP
    ret = MV_GIGE_ForceIpEx(handle, nIp, nMask, nGateway);
    if (ret != MV_OK) {
        qWarning() << "强制IP配置失败，错误码:" << ret;
        MV_CC_DestroyHandle(handle);
        return false;
    }
    qDebug() << "强制IP配置成功";

    // 6. 销毁临时句柄
    MV_CC_DestroyHandle(handle);
    handle = nullptr;

    // 7. 等待IP生效
    QThread::msleep(500);

    // 8. 更新IP地址字符串
    info->ipAddress = QString("%1.%2.%3.%4")
                          .arg((nIp >> 24) & 0xFF)
                          .arg((nIp >> 16) & 0xFF)
                          .arg((nIp >> 8) & 0xFF)
                          .arg(nIp & 0xFF);

    qDebug() << "IP配置成功，新IP:" << info->ipAddress;

    emit ipConfigSuccess(info->serialNumber, info->ipAddress);
    return true;
}
bool CameraManager::reopenCameraAfterIpChange(CameraInfo* info,
                                              const QString& newIp,
                                              const QString& newMask,
                                              const QString& newGateway)
{
    if (!info) {
        return false;
    }

    qDebug() << "IP变更后重新打开相机:" << info->name;

    // 1. 关闭旧相机
    if (info->isOpen) {
        closeCamera(info);
    }

    // 2. 重新枚举设备，获取新的句柄
    QString errorMsg;
    std::vector<DeviceInfo> tempDeviceList;
    if (!MvCameraQt::enumDevices(tempDeviceList, errorMsg)) {
        qWarning() << "重新枚举设备失败:" << errorMsg;
        return false;
    }

    // 3. 根据序列号查找对应的新设备
    bool found = false;
    for (auto& devInfo : tempDeviceList) {
        if (devInfo.serialNumber == info->serialNumber) {
            info->deviceInfo = (MV_CC_DEVICE_INFO*)devInfo.deviceHandle;
            info->ipAddress = fixIpByteOrder(devInfo.ipAddress);
            CameraManager::deviceList = tempDeviceList;
            found = true;
            qDebug() << "重新枚举找到设备，IP:" << info->ipAddress;
            break;
        }
    }

    if (!found) {
        qWarning() << "重新枚举后未找到相机:" << info->serialNumber;
        return false;
    }

    // 4. 等待设备稳定
    QThread::msleep(300);

    // 5. 用新句柄打开相机
    qDebug() << "使用新句柄打开相机...";
    bool result = openCamera(info);

    if (result) {
        qDebug() << "✅ 重新打开相机成功:" << info->name << "IP:" << info->ipAddress;
    } else {
        qWarning() << "❌ 重新打开相机失败:" << info->name;
    }

    return result;
}
// ============================================================
// IP解析工具函数
// ============================================================

bool CameraManager::parseIpAddress(const QString& ipStr, unsigned int& nIp)
{
    QHostAddress addr(ipStr);
    if (addr.isNull() || addr.protocol() != QAbstractSocket::IPv4Protocol) {
        return false;
    }

    quint32 ip = addr.toIPv4Address();
    nIp = ip;  // 主机字节序
    return true;
}

bool CameraManager::parseSubnetMask(const QString& maskStr, unsigned int& nMask)
{
    QHostAddress addr(maskStr);
    if (addr.isNull() || addr.protocol() != QAbstractSocket::IPv4Protocol) {
        return false;
    }

    quint32 mask = addr.toIPv4Address();
    nMask = mask;
    return true;
}

bool CameraManager::parseGateway(const QString& gatewayStr, unsigned int& nGateway)
{
    QHostAddress addr(gatewayStr);
    if (addr.isNull() || addr.protocol() != QAbstractSocket::IPv4Protocol) {
        return false;
    }

    quint32 gw = addr.toIPv4Address();
    nGateway = gw;
    return true;
}

QString CameraManager::getIpPrefix(const QString& ipAddress)
{
    if (ipAddress.isEmpty() || ipAddress == "N/A" || ipAddress == "USB") {
        return QString();
    }

    QStringList parts = ipAddress.split('.');
    if (parts.size() != 4) {
        return QString();
    }

    // 返回前三段
    return QString("%1.%2.%3").arg(parts[0]).arg(parts[1]).arg(parts[2]);
}

int CameraManager::generateRandomHost()
{
    // 生成1-254之间的随机数
    return QRandomGenerator::global()->bounded(1, 255);
}



// IP地址字节序修正 - 完全反转
QString fixIpByteOrder(const QString& wrongIp) {
    QStringList parts = wrongIp.split('.');
    if (parts.size() != 4) return wrongIp;

    // 完全反转：第1个<->第4个，第2个<->第3个
    return QString("%1.%2.%3.%4")
        .arg(parts[3])  // 原第4个 -> 新第1个
        .arg(parts[2])  // 原第3个 -> 新第2个
        .arg(parts[1])  // 原第2个 -> 新第3个
        .arg(parts[0]); // 原第1个 -> 新第4个
}

bool CameraManager::enumAndOpenCameras()
{
    QString errorMsg;

    // 枚举全部相机
    if (!MvCameraQt::enumDevices(CameraManager::deviceList, errorMsg)) {
        emit errorOccurred(QString("枚举设备失败: %1").arg(errorMsg));
        return false;
    }

    auto& deviceList = CameraManager::deviceList;
    if (deviceList.empty()) {
        emit errorOccurred("未找到任何相机设备");
        return false;
    }

    qDebug() << "找到" << deviceList.size() << "个设备";

    // ===== 加载配置 =====
    ConfigManager* config = ConfigManager::instance();
    QVector<UsedCamInfo> camConfigs;
    if (config) {
        camConfigs = config->loadCamInfoConfig();
        qDebug() << "加载了" << camConfigs.size() << "个相机配置";
    }

    int count = qMin(static_cast<int>(deviceList.size()), m_cameras.size());
    for (int i = 0; i < count; i++) {
        //按照枚举到的顺序进行分配到cam1 - cam4，所以，此处 是对应的，那么
        CameraInfo* info = m_cameras[i];
        DeviceInfo& devInfo = deviceList[i];

        info->serialNumber = devInfo.serialNumber;
        info->modelName = devInfo.modelName;
        info->ipAddress = fixIpByteOrder(devInfo.ipAddress);
        info->deviceInfo = (MV_CC_DEVICE_INFO*)devInfo.deviceHandle;

        qDebug() << "分配设备到" << info->name
                 << "型号:" << info->modelName
                 << "序列号:" << info->serialNumber
                 << "IP:" << info->ipAddress;


        // 打开相机
        if (!openCameraWithRetry(info, MAX_RETRY_COUNT)) {
            qWarning() << "打开相机失败:" << info->name;
            continue;
        }

        // ===== 应用曝光配置 =====
        int exposure = 10000;  // 默认值
        for (const UsedCamInfo& cfg : camConfigs) {
            if (cfg.serialNumber == info->serialNumber) {
                exposure = cfg.exposure;
                qDebug() << "应用配置曝光值:" << exposure << "us, 序列号:" << info->serialNumber;
                break;
            }
        }

        if (info->camera->setFloatValue("ExposureTime", exposure) != MV_OK) {
            qWarning() << "设置曝光失败:" << info->name;
        }
    }

    for (int i = count; i < m_cameras.size(); i++) {
        CameraInfo* info = m_cameras[i];
        info->serialNumber.clear();
        info->modelName.clear();
        info->ipAddress.clear();
        info->deviceInfo = nullptr;
        info->isOpen = false;
        info->isGrabbing = false;
        qDebug() << info->name << "未分配设备";
    }

    return true;
}

// ============================================================
// 手动解析IP字符串为网络字节序（参考官方示例）
// ============================================================
bool CameraManager::parseIpToNetwork(const QString& ipStr, unsigned int& nIp)
{
    QStringList parts = ipStr.split('.');
    if (parts.size() != 4) return false;

    bool ok[4];
    int p[4];
    for (int i = 0; i < 4; i++) {
        p[i] = parts[i].toInt(&ok[i]);
        if (!ok[i] || p[i] < 0 || p[i] > 255) return false;
    }

    // 官方示例的方式：直接组装成网络字节序
    nIp = (static_cast<unsigned int>(p[0]) << 24) |
          (static_cast<unsigned int>(p[1]) << 16) |
          (static_cast<unsigned int>(p[2]) << 8) |
          static_cast<unsigned int>(p[3]);
    return true;
}

// ===== ✅ 保存当前相机配置 =====
bool CameraManager::saveCurrentCameraConfig()
{
    ConfigManager* config = ConfigManager::instance();
    if (!config) {
        qDebug() << "[CameraManager] ConfigManager未初始化";
        return false;
    }

    QVector<UsedCamInfo> camInfoList;

    for (CameraInfo* info : m_cameras) {
        if (!info->serialNumber.isEmpty()) {
            UsedCamInfo camInfo;
            camInfo.serialNumber = info->serialNumber;
            camInfo.ipAddress = info->ipAddress;

            // 从相机读取当前曝光值
            if (info->isOpen && info->camera) {
                MVCC_FLOATVALUE floatVal;
                if (info->camera->getFloatValue("ExposureTime", &floatVal) == MV_OK) {
                    camInfo.exposure = (int)floatVal.fCurValue;
                } else {
                    camInfo.exposure = 10000;  // 默认值
                }
            } else {
                camInfo.exposure = 10000;
            }

            camInfoList.append(camInfo);
            qDebug() << "[CameraManager] 保存相机:"
                     << info->name << "序列号:" << info->serialNumber
                     << "IP:" << info->ipAddress << "曝光:" << camInfo.exposure;
        }
    }

    config->saveCamInfoConfig(camInfoList);
    qDebug() << "[CameraManager] 保存了" << camInfoList.size() << "个相机配置";
    return true;
}
