#ifndef CONFIGSTRUCT_H
#define CONFIGSTRUCT_H
//存储配置文件的相关结构体

#include <QString>

struct UsedCamInfo
{
    QString serialNumber;           // 序列号
    QString ipAddress;              // IP地址
    int exposure;                   // 曝光时间
};

#endif // CONFIGSTRUCT_H
