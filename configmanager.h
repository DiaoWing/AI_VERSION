#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDebug>
#include <QMutex>
#include <QSerialPort>

#include "detectionconfigdialog.h"
#include "modbusslavedialog.h"
#include "calibrationdialog.h"
#include "configstruct.h"


//全局只要一个存储路径，在导入之后，全部的信息都存储到其中
//需要实现的函数就是 从配置文件导入 存储到这个配置文件（追加）
//将多个文件配置的路径先统一起来

// ==================== 配置管理器（单例） ====================
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    static ConfigManager* instance();
    ~ConfigManager();

    // 加载/保存配置
    bool loadConfig(const QString& filePath = "");
    bool saveConfig(const QString& filePath = "");

    // 获取/设置配置文件路径，只需要一个函数，获取当前的配置文件的路径即可
    QString getConfigFilePath() const { return m_configFilePath; }
    void setConfigFilePath(const QString& path) { m_configFilePath = path; }

    // ===== 检测配置 =====
    //需要实现一个函数一次导入全部配置将其分配给对应的结构体，让其他文件可以通过函数获取  待实现,此处是不是该对其进行导入才行
    QList<DetectionConfig> getDetectionConfigs() const { return m_detectionConfigs; }
    void setDetectionConfigs(const QList<DetectionConfig>& configs);
    bool addDetectionConfig(const DetectionConfig& config);
    bool removeDetectionConfig(int id);
    bool updateDetectionConfig(const DetectionConfig& config);

    DetectionConfig getDetectionConfigByProjectName(const QString& projectName) const;
    bool importDetectionConfigs(const QString& filePath);
    bool exportDetectionConfigs(const QString& filePath) const;

    QJsonArray exportDetectionConfigsToJson() const;
    bool appendDetectionConfigsToFile(const QString& filePath, const QList<DetectionConfig>& configs);

    // ===== Modbus配置 =====
    ModbusConfig getModbusConfig() const { return m_modbusConfig; }
    void setModbusConfig(const ModbusConfig& config);
    bool saveModbusConfigToFile(const QString& filePath, const ModbusConfig& config);

    // ===== 相机参数配置 ====
    QVector<UsedCamInfo> loadCamInfoConfig();               //直接通过当前路径对其进行调用,去json文件中找出对应字段返回到结构体中
    void saveCamInfoConfig(QVector<UsedCamInfo> camInfo);   //相同序列号的就覆盖，存在不同的序列号就追加

    // ===== 参数管理（原有ParamManager的集成） =====
    QJsonObject getAllProjectParams() const { return m_projectParams; }
    void setProjectParams(const QJsonObject& params);
    QJsonObject getProjectParams(const QString& projectName) const;
    void setProjectParam(const QString& projectName, const QString& key, const QVariant& value);
    QVariant getProjectParam(const QString& projectName, const QString& key, const QVariant& defaultValue = QVariant()) const;

    QList<DetectionConfig> loadDetectionConfigsFromFile(const QString& filePath);

signals:
    void configLoaded();
    void configSaved();
    void configChanged(const QString& key);
    void detectionConfigChanged();
    void cameraConfigChanged();
    void modbusConfigChanged();
    void calibrationResultSaved(const QString& projectName);

private:
    explicit ConfigManager(QObject* parent = nullptr);
    static ConfigManager* m_instance;

    // 加载各个部分的配置
    bool loadDetectionConfigs(const QJsonObject& obj);
    bool loadModbusConfig(const QJsonObject& obj);
    bool loadProjectParams(const QJsonObject& obj);

    // 保存各个部分的配置
    QJsonObject saveDetectionConfigs() const;
    QJsonObject saveCameraConfigs() const;
    QJsonObject saveProjectParams() const;

    // 数据成员
    QString m_configFilePath;
    QList<DetectionConfig> m_detectionConfigs;
    ModbusConfig m_modbusConfig;
    QJsonObject m_projectParams;
    QString m_calibrationResultFilePath;

    mutable QMutex m_mutex;
private:
    int getNextDetectionId() const;
    QJsonObject calibrationResultToJson(const CalibrationResult& result,
                                        const QString& projectName,
                                        int boardWidth,
                                        int boardHeight,
                                        double squareSizeMM) const;

public:

    void SaveImageSavePathtojsonFile(const QString& path, const QString& saveImageFilePath);
    QString  LoadImageSavePathFromjsonFile();
    QString  m_imageSavePath;
    QString  getImageSavePath() const;


public:
    bool appendCalibrationResult(const CalibrationResult& result,
                                 const QString& projectName,
                                 int boardWidth = 9,
                                 int boardHeight = 6,
                                 double squareSizeMM = 2.5,
                                 const QString& filePath = "");

    // 设置标定结果文件路径
    void setCalibrationResultPath(const QString& path) { m_calibrationResultFilePath = path; }
    QString getCalibrationResultPath() const { return m_calibrationResultFilePath; }


    //追加配置文件到json中
    //从配置文件中加载
    bool SaveCalibrationResultTojsonFile(const CalibrationResult& result,
                                         const QString& projectName,
                                         int boardWidth,
                                         int boardHeight,
                                         double squareSizeMM);
    CalibrationResult LoadCalibrationResultFromjsonFile(const QString& projectName = "");
    QMap<QString, CalibrationResult> m_calibrationResultCache;
};

#endif // CONFIGMANAGER_H
