#ifndef MVCAMERAQT_H
#define MVCAMERAQT_H

#include <MvCameraControl.h>
#include <QString>
#include <QDebug>
#include <vector>

#ifndef MV_E_TIMEOUT
#define MV_E_TIMEOUT 0x800101f5  // 超时错误码
#endif

// 设备信息结构（与你现有的保持一致）
struct DeviceInfo {
    QString serialNumber;        // 序列号
    QString modelName;           // 型号名称
    QString ipAddress;           // IP地址（网口相机） 一般是从枚举之后的信息中获取，然后通过句柄来来强制配置IP
    QString userDefinedName;     // 用户自定义名称
    void* deviceHandle;          // 设备句柄（内部使用）
};

class MvCameraQt
{
public:
    MvCameraQt();
    ~MvCameraQt();

    // 1. SDK 初始化/反初始化
    static bool initSDK();
    static void finalizeSDK();

    // 2. 获取SDK版本
    static QString getSDKVersion();

    // 3. 枚举设备（静态方法，无需打开相机）
    static bool enumDevices(std::vector<DeviceInfo>& deviceList, QString& errorMsg);

    // 4. 判断设备是否可达
    static bool isDeviceAccessible(MV_CC_DEVICE_INFO* pstDevInfo, unsigned int nAccessMode);

    // 5. 打开/关闭设备（实例方法）
    int open(MV_CC_DEVICE_INFO* pstDeviceInfo);
    int close();

    // 6. 判断相机是否连接
    bool isConnected();

    // 7. 图像采集控制
    int startGrabbing();
    int stopGrabbing();

    // 8. 获取图像
    int getImageBuffer(MV_FRAME_OUT* pFrame, int nMsec);
    int freeImageBuffer(MV_FRAME_OUT* pFrame);

    // 9. 注册回调
    int registerImageCallBack(void(__stdcall* cbOutput)(unsigned char * pData,
                                                        MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser), void* pUser);
    int registerExceptionCallBack(void(__stdcall* cbException)(unsigned int nMsgType,
                                                               void* pUser), void* pUser);

    // 10. 参数设置（Int类型）
    int getIntValue(const char* strKey, MVCC_INTVALUE_EX* pIntValue);
    int setIntValue(const char* strKey, int64_t nValue);

    // 11. 参数设置（Float类型）
    int getFloatValue(const char* strKey, MVCC_FLOATVALUE* pFloatValue);
    int setFloatValue(const char* strKey, float fValue);

    // 12. 参数设置（Enum类型）
    int getEnumValue(const char* strKey, MVCC_ENUMVALUE* pEnumValue);
    int setEnumValue(const char* strKey, unsigned int nValue);
    int setEnumValueByString(const char* strKey, const char* sValue);

    // 13. 参数设置（Bool类型）
    int getBoolValue(const char* strKey, bool* pbValue);
    int setBoolValue(const char* strKey, bool bValue);

    // 14. 参数设置（String类型）
    int getStringValue(const char* strKey, MVCC_STRINGVALUE* pStringValue);
    int setStringValue(const char* strKey, const char* strValue);

    // 15. 执行命令
    int commandExecute(const char* strKey);

    // 16. 获取最优包大小（GigE相机）
    int getOptimalPacketSize(unsigned int* pOptimalPacketSize);

    // 17. 强制IP（GigE相机）
    int forceIp(unsigned int nIP, unsigned int nSubNetMask, unsigned int nDefaultGateWay);

    // 18. 像素格式转换
    int convertPixelType(MV_CC_PIXEL_CONVERT_PARAM_EX* pstCvtParam);

    // 19. 保存图片
    int saveImage(MV_SAVE_IMAGE_PARAM_EX3* pstParam);

    // 20. 获取最后错误信息
    QString getLastError() const { return m_lastError; }

    // 21. 获取设备句柄
    void* getHandle() const { return m_hDevHandle; }

    int setIpConfig(unsigned int nConfig);

private:
    void* m_hDevHandle;           // 设备句柄
    QString m_lastError;          // 最后一次错误信息

    static bool m_sdkInitialized; // SDK是否已初始化（静态成员）
};

#endif // MVCAMERAQT_H
