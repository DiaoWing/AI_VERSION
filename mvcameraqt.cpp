#include "mvcameraqt.h"
#include <cstring>
#include <sstream>

// 静态成员初始化
bool MvCameraQt::m_sdkInitialized = false;

MvCameraQt::MvCameraQt() : m_hDevHandle(nullptr)
{
}

MvCameraQt::~MvCameraQt()
{
    if (m_hDevHandle) {
        close();
    }
}

// 静态方法：初始化SDK
bool MvCameraQt::initSDK()
{
    if (m_sdkInitialized) {
        return true;
    }

    int nRet = MV_CC_Initialize();
    if (nRet != MV_OK) {
        qDebug() << "SDK初始化失败，错误码:" << nRet;
        return false;
    }

    m_sdkInitialized = true;
    qDebug() << "SDK初始化成功，版本:" << getSDKVersion();
    return true;
}

// 静态方法：反初始化SDK
void MvCameraQt::finalizeSDK()
{
    if (m_sdkInitialized) {
        MV_CC_Finalize();
        m_sdkInitialized = false;
        qDebug() << "SDK已反初始化";
    }
}

// 静态方法：获取SDK版本
QString MvCameraQt::getSDKVersion()
{
    unsigned int version = MV_CC_GetSDKVersion();
    unsigned int major = (version >> 24) & 0xFF;
    unsigned int minor = (version >> 16) & 0xFF;
    unsigned int revision = (version >> 8) & 0xFF;

    return QString("v%1.%2.%3").arg(major).arg(minor).arg(revision);
}

// 静态方法：枚举设备
bool MvCameraQt::enumDevices(std::vector<DeviceInfo>& deviceList, QString& errorMsg)
{
    deviceList.clear();

    if (!m_sdkInitialized) {
        errorMsg = "SDK未初始化，请先调用initSDK()";
        return false;
    }

    // 枚举所有设备
    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

    int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    if (nRet != MV_OK) {
        errorMsg = QString("枚举设备失败，错误码: %1").arg(nRet);
        return false;
    }

    // 遍历设备列表
    for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++) {
        MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
        if (pDeviceInfo == nullptr) {
            continue;
        }

        DeviceInfo info;
        info.deviceHandle = pDeviceInfo;
        info.ipAddress = "N/A";

        // 根据设备类型获取信息
        if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE) {
            MV_GIGE_DEVICE_INFO& gigEInfo = pDeviceInfo->SpecialInfo.stGigEInfo;

            info.serialNumber = QString::fromLatin1((char*)gigEInfo.chSerialNumber);
            info.modelName = QString::fromLatin1((char*)gigEInfo.chModelName);
            info.userDefinedName = QString::fromLatin1((char*)gigEInfo.chUserDefinedName);

            // 格式化IP地址
            info.ipAddress = QString("%1.%2.%3.%4")
                                 .arg(gigEInfo.nCurrentIp & 0xFF)
                                 .arg((gigEInfo.nCurrentIp >> 8) & 0xFF)
                                 .arg((gigEInfo.nCurrentIp >> 16) & 0xFF)
                                 .arg((gigEInfo.nCurrentIp >> 24) & 0xFF);

        } else if (pDeviceInfo->nTLayerType == MV_USB_DEVICE) {
            MV_USB3_DEVICE_INFO& usbInfo = pDeviceInfo->SpecialInfo.stUsb3VInfo;

            info.serialNumber = QString::fromLatin1((char*)usbInfo.chSerialNumber);
            info.modelName = QString::fromLatin1((char*)usbInfo.chModelName);
            info.userDefinedName = QString::fromLatin1((char*)usbInfo.chUserDefinedName);
            info.ipAddress = "USB";
        }

        deviceList.push_back(info);
        qDebug() << "找到设备:" << info.modelName << "(SN:" << info.serialNumber << ")";
    }

    qDebug() << "共找到" << deviceList.size() << "个设备";
    errorMsg = "";
    return true;
}

// 静态方法：判断设备是否可达
bool MvCameraQt::isDeviceAccessible(MV_CC_DEVICE_INFO* pstDevInfo, unsigned int nAccessMode)
{
    return MV_CC_IsDeviceAccessible(pstDevInfo, nAccessMode) == MV_OK;
}

// 打开设备
int MvCameraQt::open(MV_CC_DEVICE_INFO* pstDeviceInfo)
{
    if (nullptr == pstDeviceInfo) {
        m_lastError = "设备信息为空";
        return MV_E_PARAMETER;
    }

    if (m_hDevHandle) {
        m_lastError = "设备已打开";
        return MV_E_CALLORDER;
    }

    // 创建句柄
    int nRet = MV_CC_CreateHandle(&m_hDevHandle, pstDeviceInfo);
    if (MV_OK != nRet) {
        m_lastError = QString("创建设备句柄失败: %1").arg(nRet);
        return nRet;
    }

    // 打开设备
    nRet = MV_CC_OpenDevice(m_hDevHandle);
    if (MV_OK != nRet) {
        MV_CC_DestroyHandle(m_hDevHandle);
        m_hDevHandle = nullptr;
        m_lastError = QString("打开设备失败: %1").arg(nRet);
    } else {
        qDebug() << "设备打开成功";
    }

    return nRet;
}

// 关闭设备
int MvCameraQt::close()
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }

    // 先停止采集（如果正在采集）
    stopGrabbing();

    // 关闭设备
    MV_CC_CloseDevice(m_hDevHandle);

    // 销毁句柄
    int nRet = MV_CC_DestroyHandle(m_hDevHandle);
    m_hDevHandle = nullptr;

    qDebug() << "设备已关闭";
    return nRet;
}

// 判断相机是否连接
// 在 MvCameraQt.cpp 中
bool MvCameraQt::isConnected()
{
    if (nullptr == m_hDevHandle) {
        qDebug() << "isConnected: 句柄为空";
        return false;
    }

    // 方法1：直接返回 true（如果已经成功打开）
    // 因为 open 返回 0 表示成功，我们可以认为设备已连接
    return true;
}

// 开始采集
int MvCameraQt::startGrabbing()
{
    if (nullptr == m_hDevHandle) {
        m_lastError = "设备未打开";
        return MV_E_HANDLE;
    }
    return MV_CC_StartGrabbing(m_hDevHandle);
}

// 停止采集
int MvCameraQt::stopGrabbing()
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_StopGrabbing(m_hDevHandle);
}

// 获取图像
int MvCameraQt::getImageBuffer(MV_FRAME_OUT* pFrame, int nMsec)
{
    if (nullptr == m_hDevHandle) {
        m_lastError = "设备未打开";
        return MV_E_HANDLE;
    }
    return MV_CC_GetImageBuffer(m_hDevHandle, pFrame, nMsec);
}

// 释放图像缓存
int MvCameraQt::freeImageBuffer(MV_FRAME_OUT* pFrame)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_FreeImageBuffer(m_hDevHandle, pFrame);
}

// 注册图像回调
int MvCameraQt::registerImageCallBack(void(__stdcall* cbOutput)(unsigned char * pData,
                                                                MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser), void* pUser)
{
    if (nullptr == m_hDevHandle) {
        m_lastError = "设备未打开";
        return MV_E_HANDLE;
    }
    return MV_CC_RegisterImageCallBackEx(m_hDevHandle, cbOutput, pUser);
}

// 注册异常回调
int MvCameraQt::registerExceptionCallBack(void(__stdcall* cbException)(unsigned int nMsgType,
                                                                       void* pUser), void* pUser)
{
    if (nullptr == m_hDevHandle) {
        m_lastError = "设备未打开";
        return MV_E_HANDLE;
    }
    return MV_CC_RegisterExceptionCallBack(m_hDevHandle, cbException, pUser);
}

// 获取Int参数
int MvCameraQt::getIntValue(const char* strKey, MVCC_INTVALUE_EX* pIntValue)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_GetIntValueEx(m_hDevHandle, strKey, pIntValue);
}

// 设置Int参数
int MvCameraQt::setIntValue(const char* strKey, int64_t nValue)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_SetIntValueEx(m_hDevHandle, strKey, nValue);
}

// 获取Float参数
int MvCameraQt::getFloatValue(const char* strKey, MVCC_FLOATVALUE* pFloatValue)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_GetFloatValue(m_hDevHandle, strKey, pFloatValue);
}

// 设置Float参数
int MvCameraQt::setFloatValue(const char* strKey, float fValue)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_SetFloatValue(m_hDevHandle, strKey, fValue);
}

// 获取Enum参数
int MvCameraQt::getEnumValue(const char* strKey, MVCC_ENUMVALUE* pEnumValue)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_GetEnumValue(m_hDevHandle, strKey, pEnumValue);
}

// 设置Enum参数
int MvCameraQt::setEnumValue(const char* strKey, unsigned int nValue)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_SetEnumValue(m_hDevHandle, strKey, nValue);
}

// 通过字符串设置Enum参数
int MvCameraQt::setEnumValueByString(const char* strKey, const char* sValue)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_SetEnumValueByString(m_hDevHandle, strKey, sValue);
}

// 获取Bool参数
int MvCameraQt::getBoolValue(const char* strKey, bool* pbValue)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_GetBoolValue(m_hDevHandle, strKey, pbValue);
}

// 设置Bool参数
int MvCameraQt::setBoolValue(const char* strKey, bool bValue)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_SetBoolValue(m_hDevHandle, strKey, bValue);
}

// 获取String参数
int MvCameraQt::getStringValue(const char* strKey, MVCC_STRINGVALUE* pStringValue)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_GetStringValue(m_hDevHandle, strKey, pStringValue);
}

// 设置String参数
int MvCameraQt::setStringValue(const char* strKey, const char* strValue)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_SetStringValue(m_hDevHandle, strKey, strValue);
}

// 执行命令
int MvCameraQt::commandExecute(const char* strKey)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_SetCommandValue(m_hDevHandle, strKey);
}

// 获取最优包大小
int MvCameraQt::getOptimalPacketSize(unsigned int* pOptimalPacketSize)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }

    int nRet = MV_CC_GetOptimalPacketSize(m_hDevHandle);
    if (nRet < MV_OK) {
        return nRet;
    }

    *pOptimalPacketSize = (unsigned int)nRet;
    return MV_OK;
}

// 强制IP
int MvCameraQt::forceIp(unsigned int nIP, unsigned int nSubNetMask, unsigned int nDefaultGateWay)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_GIGE_ForceIpEx(m_hDevHandle, nIP, nSubNetMask, nDefaultGateWay);
}

// 像素格式转换
int MvCameraQt::convertPixelType(MV_CC_PIXEL_CONVERT_PARAM_EX* pstCvtParam)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_ConvertPixelTypeEx(m_hDevHandle, pstCvtParam);
}

// 保存图片
int MvCameraQt::saveImage(MV_SAVE_IMAGE_PARAM_EX3* pstParam)
{
    if (nullptr == m_hDevHandle) {
        return MV_E_HANDLE;
    }
    return MV_CC_SaveImageEx3(m_hDevHandle, pstParam);
}

int MvCameraQt::setIpConfig(unsigned int nConfig)
{
    if (nullptr == m_hDevHandle) {
        m_lastError = "设备未打开";
        return MV_E_HANDLE;
    }
    return MV_GIGE_SetIpConfig(m_hDevHandle, nConfig);
}