#include "configmanager.h"
#include <QDir>
#include <QStandardPaths>

// ==================== 单例实现 ====================
ConfigManager* ConfigManager::m_instance = nullptr;

ConfigManager* ConfigManager::instance()
{
    if (!m_instance) {
        m_instance = new ConfigManager();
    }
    return m_instance;
}

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
    , m_configFilePath("./config/app_config.json")      //初始化的存储配置的路径
{
    // 创建配置目录
    QDir dir("./config");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

ConfigManager::~ConfigManager()
{
    // 析构时自动保存
    saveConfig();
}

//加锁是因为其要保持，不让其被同时操作。
// ==================== 加载/保存配置 ====================
bool ConfigManager::loadConfig(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);

    QString path = filePath.isEmpty() ? m_configFilePath : filePath;
    if (!QFile::exists(path)) {
        qDebug() << "[ConfigManager] 配置文件不存在，使用默认配置";
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[ConfigManager] 无法打开配置文件:" << path;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qDebug() << "[ConfigManager] 解析JSON失败";
        return false;
    }

    QJsonObject root = doc.object();

    // 加载各模块配置，每部分都需要有自己的加载 和导入 ！，此处是一件导入全部，但是需要方法，一个一个导入
    bool success = true;
    if (root.contains("detectionConfigs")) { //配置表
        success &= loadDetectionConfigs(root["detectionConfigs"].toObject());
    }
    if (root.contains("modbusConfig")) { //协议的连接配置
        success &= loadModbusConfig(root["modbusConfig"].toObject());
    }
    if (root.contains("projectParams")) { //项目的参数配置
        success &= loadProjectParams(root["projectParams"].toObject());
    }
    if (success) {
        emit configLoaded(); //提示配置成功
        qDebug() << "[ConfigManager] 配置加载成功";
    }

    return success;
}

bool ConfigManager::saveConfig(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);

    QString path = filePath.isEmpty() ? m_configFilePath : filePath;

    QJsonObject root;

    // 保存各模块配置
    root["detectionConfigs"] = saveDetectionConfigs();
    root["cameraConfigs"] = saveCameraConfigs();
    root["projectParams"] = saveProjectParams();

    QJsonDocument doc(root);
    QByteArray data = doc.toJson(QJsonDocument::Indented);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "[ConfigManager] 无法写入配置文件:" << path;
        return false;
    }

    file.write(data);
    file.close();

    emit configSaved();
    qDebug() << "[ConfigManager] 配置保存成功:" << path;
    return true;
}


// ==================== 加载各模块配置 ====================

bool ConfigManager::loadDetectionConfigs(const QJsonObject& obj)
{
    if (!obj.contains("configs")) return false;

    QJsonArray configsArray = obj["configs"].toArray();
    m_detectionConfigs.clear();

    for (const QJsonValue& value : configsArray) {
        QJsonObject configObj = value.toObject();
        DetectionConfig config;
        config.id = configObj["id"].toInt();
        config.cameraId = configObj["cameraId"].toInt();
        config.projectName = configObj["projectName"].toString();
        config.lightController = configObj["lightController"].toInt();
        config.channel1 = configObj["channel1"].toInt();
        config.channel2 = configObj["channel2"].toInt();
        config.channel3 = configObj["channel3"].toInt();
        config.channel4 = configObj["channel4"].toInt();
        config.calibrated = configObj["calibrated"].toBool();
        config.groupId = configObj["groupId"].toInt();
        m_detectionConfigs.append(config);
    }

    qDebug() << "[ConfigManager] 加载了" << m_detectionConfigs.size() << "个检测配置";
    return true;
}



bool ConfigManager::loadModbusConfig(const QJsonObject& obj)
{
    // m_modbusConfig = ModbusConfig::fromJson(obj);
    // qDebug() << "[ConfigManager] 加载Modbus配置: port=" << m_modbusConfig.portName;
    return true;
}


bool ConfigManager::loadProjectParams(const QJsonObject& obj)
{
    m_projectParams = obj;
    qDebug() << "[ConfigManager] 加载了" << m_projectParams.keys().size() << "个项目的参数";
    return true;
}

// ==================== 保存各模块配置 ====================

QJsonObject ConfigManager::saveDetectionConfigs() const
{
    QJsonObject result;
    QJsonArray configsArray;

    for (const DetectionConfig& config : m_detectionConfigs) {
        QJsonObject obj;
        obj["id"] = config.id;
        obj["cameraId"] = config.cameraId;
        obj["projectName"] = config.projectName;
        obj["lightController"] = config.lightController;
        obj["channel1"] = config.channel1;
        obj["channel2"] = config.channel2;
        obj["channel3"] = config.channel3;
        obj["channel4"] = config.channel4;
        obj["calibrated"] = config.calibrated;
        obj["groupId"] = config.groupId;
        configsArray.append(obj);
    }

    result["configs"] = configsArray;
    return result;
}

QJsonObject ConfigManager::saveCameraConfigs() const
{
    QJsonObject result;
    // QJsonArray configsArray;

    // for (const CameraConfig& config : m_cameraConfigs) {
    //     configsArray.append(config.toJson());
    // }

    // result["configs"] = configsArray;
    return result;
}

QJsonObject ConfigManager::saveProjectParams() const
{
    return m_projectParams;
}

// ==================== 检测配置管理 ====================

void ConfigManager::setDetectionConfigs(const QList<DetectionConfig>& configs)
{
    QMutexLocker locker(&m_mutex);
    m_detectionConfigs = configs;
    emit detectionConfigChanged();
    saveConfig();
}

bool ConfigManager::addDetectionConfig(const DetectionConfig& config)
{
    QMutexLocker locker(&m_mutex);

    DetectionConfig newConfig = config;
    newConfig.id = getNextDetectionId();
    m_detectionConfigs.append(newConfig);

    emit detectionConfigChanged();
    saveConfig();
    return true;
}

bool ConfigManager::removeDetectionConfig(int id)
{
    QMutexLocker locker(&m_mutex);

    for (int i = 0; i < m_detectionConfigs.size(); ++i) {
        if (m_detectionConfigs[i].id == id) {
            m_detectionConfigs.removeAt(i);
            emit detectionConfigChanged();
            saveConfig();
            return true;
        }
    }
    return false;
}

bool ConfigManager::updateDetectionConfig(const DetectionConfig& config)
{
    QMutexLocker locker(&m_mutex);

    for (int i = 0; i < m_detectionConfigs.size(); ++i) {
        if (m_detectionConfigs[i].id == config.id) {
            m_detectionConfigs[i] = config;
            emit detectionConfigChanged();
            saveConfig();
            return true;
        }
    }
    return false;
}



DetectionConfig ConfigManager::getDetectionConfigByProjectName(const QString& projectName) const
{
    QMutexLocker locker(&m_mutex);
    for (const DetectionConfig& config : m_detectionConfigs) {
        if (config.projectName == projectName) {
            return config;
        }
    }
    return DetectionConfig();
}

// ==================== Modbus配置管理 ====================

void ConfigManager::setModbusConfig(const ModbusConfig& config)
{
    QMutexLocker locker(&m_mutex);
    m_modbusConfig = config;
    emit modbusConfigChanged();
    saveConfig();
}


// ==================== 项目参数管理 ====================

void ConfigManager::setProjectParams(const QJsonObject& params)
{
    QMutexLocker locker(&m_mutex);
    m_projectParams = params;
    emit configChanged("projectParams");
    saveConfig();
}

QJsonObject ConfigManager::getProjectParams(const QString& projectName) const
{
    QMutexLocker locker(&m_mutex);
    return m_projectParams[projectName].toObject();
}

void ConfigManager::setProjectParam(const QString& projectName, const QString& key, const QVariant& value)
{
    QMutexLocker locker(&m_mutex);

    QJsonObject projectObj = m_projectParams[projectName].toObject();

    // 转换QVariant为QJsonValue
    QJsonValue jsonValue;
    switch (value.type()) {
    case QVariant::Int:
        jsonValue = value.toInt();
        break;
    case QVariant::Double:
        jsonValue = value.toDouble();
        break;
    case QVariant::Bool:
        jsonValue = value.toBool();
        break;
    case QVariant::String:
        jsonValue = value.toString();
        break;
    default:
        jsonValue = value.toString();
        break;
    }

    projectObj[key] = jsonValue;
    m_projectParams[projectName] = projectObj;

    emit configChanged("projectParams");
    saveConfig();
}

QVariant ConfigManager::getProjectParam(const QString& projectName, const QString& key, const QVariant& defaultValue) const
{
    QMutexLocker locker(&m_mutex);

    QJsonObject projectObj = m_projectParams[projectName].toObject();
    if (!projectObj.contains(key)) {
        return defaultValue;
    }

    QJsonValue value = projectObj[key];
    switch (value.type()) {
    case QJsonValue::Bool:
        return value.toBool();
    case QJsonValue::Double:
        return value.toDouble();
    case QJsonValue::String:
        return value.toString();
    case QJsonValue::Array:
    case QJsonValue::Object:
        return value.toString();
    default:
        return defaultValue;
    }
}

// ==================== 辅助函数 ====================

int ConfigManager::getNextDetectionId() const
{
    int maxId = 0;
    for (const DetectionConfig& config : m_detectionConfigs) {
        if (config.id > maxId) {
            maxId = config.id;
        }
    }
    return maxId + 1;
}


bool ConfigManager::importDetectionConfigs(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);  // 保留这把锁

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[ConfigManager] 无法打开检测配置文件:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qDebug() << "[ConfigManager] 解析检测配置JSON失败";
        return false;
    }

    QJsonObject root = doc.object();
    if (!root.contains("detectionConfigs")) {
        qDebug() << "[ConfigManager] 检测配置文件格式错误";
        return false;
    }

    QJsonObject detectionObj = root["detectionConfigs"].toObject();
    if (!detectionObj.contains("configs")) {
        qDebug() << "[ConfigManager] 检测配置文件缺少configs字段";
        return false;
    }

    // ===== 直接在这里处理数据，不调用 importDetectionConfigsFromJson =====
    QJsonArray configsArray = detectionObj["configs"].toArray();
    QList<DetectionConfig> importedConfigs;

    for (const QJsonValue& value : configsArray) {
        QJsonObject configObj = value.toObject();
        DetectionConfig config;
        config.id = configObj["id"].toInt();
        config.cameraId = configObj["cameraId"].toInt();
        config.projectName = configObj["projectName"].toString();
        config.lightController = configObj["lightController"].toInt();
        config.channel1 = configObj["channel1"].toInt();
        config.channel2 = configObj["channel2"].toInt();
        config.channel3 = configObj["channel3"].toInt();
        config.channel4 = configObj["channel4"].toInt();
        config.calibrated = configObj["calibrated"].toBool();
        config.groupId = configObj["groupId"].toInt();
        importedConfigs.append(config);
    }

    // 替换现有配置
    m_detectionConfigs = importedConfigs;

    // ===== 锁内发射信号和保存？不，应该移到锁外 =====
    // emit detectionConfigChanged();  // 注释掉
    // saveConfig();                    // 注释掉

    // 记录路径
    m_configFilePath = filePath;
    qDebug() << "[ConfigManager] 记录检测配置路径:" << m_configFilePath;

    qDebug() << "[ConfigManager] 导入检测配置成功，共" << m_detectionConfigs.size() << "个配置";

    // ===== 锁外触发信号和保存 =====
    // 但这里需要释放锁才能调用 saveConfig()
    // 所以我们需要在锁外处理

    return true;
}

bool ConfigManager::exportDetectionConfigs(const QString& filePath) const
{
    QMutexLocker locker(&m_mutex);

    QJsonObject root;
    root["detectionConfigs"] = saveDetectionConfigs();

    QJsonDocument doc(root);
    QByteArray data = doc.toJson(QJsonDocument::Indented);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "[ConfigManager] 无法写入检测配置文件:" << filePath;
        return false;
    }

    file.write(data);
    file.close();

    // 记录路径（使用const_cast）
    const_cast<ConfigManager*>(this)->m_configFilePath = filePath;
    qDebug() << "[ConfigManager] 记录检测配置路径:" << m_configFilePath;

    qDebug() << "[ConfigManager] 检测配置导出成功:" << filePath;
    return true;
}

QJsonArray ConfigManager::exportDetectionConfigsToJson() const
{
    QMutexLocker locker(&m_mutex);
    QJsonArray configsArray;

    for (const DetectionConfig& config : m_detectionConfigs) {
        QJsonObject obj;
        obj["id"] = config.id;
        obj["cameraId"] = config.cameraId;
        obj["projectName"] = config.projectName;
        obj["lightController"] = config.lightController;
        obj["channel1"] = config.channel1;
        obj["channel2"] = config.channel2;
        obj["channel3"] = config.channel3;
        obj["channel4"] = config.channel4;
        obj["calibrated"] = config.calibrated;
        obj["groupId"] = config.groupId;
        configsArray.append(obj);
    }

    return configsArray;
}

bool ConfigManager::appendDetectionConfigsToFile(const QString& filePath, const QList<DetectionConfig>& configs)
{
    QMutexLocker locker(&m_mutex);

    if (configs.isEmpty()) {
        qDebug() << "[ConfigManager] 没有要保存的配置";
        return false;
    }

    QJsonObject root;

    // 1. 如果文件存在，先读取现有内容
    if (QFile::exists(filePath)) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();

            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull()) {
                root = doc.object();
            }
        }
    }

    // 2. 构建新的检测配置JSON
    QJsonArray configsArray;
    for (const DetectionConfig& config : configs) {
        QJsonObject obj;
        obj["id"] = config.id;
        obj["cameraId"] = config.cameraId;
        obj["projectName"] = config.projectName;
        obj["lightController"] = config.lightController;
        obj["channel1"] = config.channel1;
        obj["channel2"] = config.channel2;
        obj["channel3"] = config.channel3;
        obj["channel4"] = config.channel4;
        obj["calibrated"] = config.calibrated;
        obj["groupId"] = config.groupId;
        configsArray.append(obj);
    }

    // 3. 更新detectionConfigs部分（其他部分保持不变）
    root["detectionConfigs"] = QJsonObject{{"configs", configsArray}};

    // 4. 写回文件
    QJsonDocument doc(root);
    QByteArray data = doc.toJson(QJsonDocument::Indented);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "[ConfigManager] 无法写入文件:" << filePath;
        return false;
    }

    file.write(data);
    file.close();

    // 记录路径
    m_configFilePath = filePath;

    qDebug() << "[ConfigManager] 保存了" << configs.size() << "个配置到:" << filePath;
    return true;
}

bool ConfigManager::saveModbusConfigToFile(const QString& filePath, const ModbusConfig& config)
{
    QMutexLocker locker(&m_mutex);

    QJsonObject root;

    // 1. 如果文件存在，先读取现有内容
    if (QFile::exists(filePath)) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();

            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull()) {
                root = doc.object();
            }
        }
    }

    // 2. 构建Modbus配置JSON
    QJsonObject modbusObj;
    modbusObj["portName"] = config.portName;
    modbusObj["baudRate"] = config.baudRate;
    modbusObj["dataBits"] = config.dataBits;
    modbusObj["stopBits"] = config.stopBits;
    modbusObj["parity"] = config.parity;
    modbusObj["slaveId"] = config.slaveId;
    modbusObj["saveTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 3. 更新modbusConfig部分（其他部分保持不变）
    root["modbusConfig"] = modbusObj;

    // 4. 写回文件
    QJsonDocument doc(root);
    QByteArray data = doc.toJson(QJsonDocument::Indented);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "[ConfigManager] 无法写入Modbus配置文件:" << filePath;
        return false;
    }

    file.write(data);
    file.close();

    qDebug() << "[ConfigManager] Modbus配置已保存到:" << filePath;
    return true;
}


// ==================== 标定结果保存（新增） ====================

QJsonObject ConfigManager::calibrationResultToJson(const CalibrationResult& result,
                                                   const QString& projectName,
                                                   int boardWidth,
                                                   int boardHeight,
                                                   double squareSizeMM) const
{
    QJsonObject obj;
    obj["projectName"] = projectName;
    obj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    obj["imageWidth"] = result.imageWidth;
    obj["imageHeight"] = result.imageHeight;
    obj["reprojectionError"] = result.reprojectionError;
    obj["scale"] = result.scale;
    obj["boardWidth"] = boardWidth;
    obj["boardHeight"] = boardHeight;
    obj["squareSizeMM"] = squareSizeMM;
    obj["success"] = result.success;

    // 相机内参矩阵 (3x3)
    QJsonArray cameraMatrixArr;
    if (!result.cameraMatrix.empty()) {
        for (int i = 0; i < result.cameraMatrix.rows; i++) {
            for (int j = 0; j < result.cameraMatrix.cols; j++) {
                cameraMatrixArr.append(result.cameraMatrix.at<double>(i, j));
            }
        }
    }
    obj["cameraMatrix"] = cameraMatrixArr;

    // 畸变系数
    QJsonArray distCoeffsArr;
    if (!result.distCoeffs.empty()) {
        if (result.distCoeffs.rows == 1) {
            for (int j = 0; j < result.distCoeffs.cols; j++) {
                distCoeffsArr.append(result.distCoeffs.at<double>(0, j));
            }
        } else if (result.distCoeffs.cols == 1) {
            for (int i = 0; i < result.distCoeffs.rows; i++) {
                distCoeffsArr.append(result.distCoeffs.at<double>(i, 0));
            }
        }
    }
    obj["distCoeffs"] = distCoeffsArr;

    return obj;
}

bool ConfigManager::appendCalibrationResult(const CalibrationResult& result,
                                            const QString& projectName,
                                            int boardWidth,
                                            int boardHeight,
                                            double squareSizeMM,
                                            const QString& filePath)
{
    QMutexLocker locker(&m_mutex);

    // 只保存成功的标定结果
    if (!result.success) {
        qDebug() << "[ConfigManager] 标定失败，不保存结果";
        return false;
    }

    // 确定文件路径
    QString path = filePath.isEmpty() ? m_calibrationResultFilePath : filePath;
    if (path.isEmpty()) {
        // 如果路径为空，使用默认路径
        path = "./config/calibration_results.json";
        m_calibrationResultFilePath = path;
    }

    // 确保目录存在
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QJsonObject root;

    // 1. 如果文件存在，先读取现有内容
    if (QFile::exists(path)) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();

            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull()) {
                root = doc.object();
            }
        }
    }

    // 2. 获取现有的结果列表
    QJsonArray resultsArray;
    if (root.contains("calibrationResults")) {
        resultsArray = root["calibrationResults"].toArray();
    }

    // 3. 添加新结果
    QJsonObject resultObj = calibrationResultToJson(result, projectName,
                                                    boardWidth, boardHeight,
                                                    squareSizeMM);
    resultsArray.append(resultObj);

    // 4. 更新root
    root["calibrationResults"] = resultsArray;
    root["lastUpdate"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["totalCount"] = resultsArray.size();

    // 5. 写回文件
    QJsonDocument doc(root);
    QByteArray data = doc.toJson(QJsonDocument::Indented);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "[ConfigManager] 无法写入标定结果文件:" << path;
        return false;
    }

    file.write(data);
    file.close();

    // 更新路径
    m_calibrationResultFilePath = path;

    emit calibrationResultSaved(projectName);
    qDebug() << "[ConfigManager] 标定结果已保存到:" << path
             << ", 项目:" << projectName
             << ", 重投影误差:" << result.reprojectionError;

    return true;
}


#include "configmanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QDir>

// ============================================================
// 加载相机信息配置
// ============================================================
QVector<UsedCamInfo> ConfigManager::loadCamInfoConfig()
{
    QMutexLocker locker(&m_mutex);

    QVector<UsedCamInfo> camInfoList;

    // 1. 检查配置文件路径是否有效
    if (m_configFilePath.isEmpty()) {
        qDebug() << "[ConfigManager] 配置文件路径为空，无法加载相机信息";
        return camInfoList;
    }

    // 2. 检查文件是否存在
    if (!QFile::exists(m_configFilePath)) {
        qDebug() << "[ConfigManager] 配置文件不存在:" << m_configFilePath;
        return camInfoList;
    }

    // 3. 读取并解析JSON文件
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[ConfigManager] 无法打开配置文件:" << m_configFilePath;
        return camInfoList;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "[ConfigManager] JSON解析失败";
        return camInfoList;
    }

    QJsonObject root = doc.object();

    // 4. 检查是否存在相机信息字段（兼容两种字段名）
    QJsonArray cameraArray;
    if (root.contains("cameraInfo") && root["cameraInfo"].isArray()) {
        cameraArray = root["cameraInfo"].toArray();
    } else if (root.contains("cameras") && root["cameras"].isArray()) {
        cameraArray = root["cameras"].toArray();
    } else {
        qDebug() << "[ConfigManager] 配置文件中没有相机信息字段";
        return camInfoList;
    }

    // 5. 解析每个相机信息
    for (const QJsonValue& value : cameraArray) {
        if (!value.isObject()) {
            continue;
        }

        QJsonObject camObj = value.toObject();
        UsedCamInfo info;

        // 读取各个字段
        info.serialNumber = camObj["serialNumber"].toString("");
        info.ipAddress = camObj["ipAddress"].toString("");
        info.exposure = camObj["exposure"].toInt(10000);  // 默认10000us

        // 兼容旧字段名
        if (info.ipAddress.isEmpty()) {
            info.ipAddress = camObj["ip"].toString("");
        }
        if (info.exposure == 10000 && camObj.contains("exposureTime")) {
            info.exposure = camObj["exposureTime"].toInt(10000);
        }

        if (!info.serialNumber.isEmpty()) {
            camInfoList.append(info);
            qDebug() << "[ConfigManager] 加载相机信息:"
                     << "序列号=" << info.serialNumber
                     << "IP=" << info.ipAddress
                     << "曝光=" << info.exposure << "us";
        }
    }

    qDebug() << "[ConfigManager] 成功加载" << camInfoList.size() << "个相机配置";
    return camInfoList;
}

// ============================================================
// 保存相机信息配置（相同序列号覆盖，不同序列号追加）
// ============================================================
void ConfigManager::saveCamInfoConfig(QVector<UsedCamInfo> camInfoList)
{
    QMutexLocker locker(&m_mutex);

    // 1. 检查配置文件路径是否有效
    if (m_configFilePath.isEmpty()) {
        qDebug() << "[ConfigManager] 配置文件路径为空，无法保存相机信息";
        return;
    }

    // 2. 确保目录存在
    QDir dir = QFileInfo(m_configFilePath).dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "[ConfigManager] 无法创建配置目录:" << dir.path();
            return;
        }
    }

    // 3. 读取现有配置（如果文件存在）
    QJsonObject root;
    QMap<QString, int> serialToIndex;  // 用于快速查找已存在的序列号
    QJsonArray existingCameraArray;

    if (QFile::exists(m_configFilePath)) {
        QFile file(m_configFilePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray jsonData = file.readAll();
            file.close();

            QJsonDocument doc = QJsonDocument::fromJson(jsonData);
            if (!doc.isNull() && doc.isObject()) {
                root = doc.object();

                // 读取现有的相机数组
                if (root.contains("cameraInfo") && root["cameraInfo"].isArray()) {
                    existingCameraArray = root["cameraInfo"].toArray();
                } else if (root.contains("cameras") && root["cameras"].isArray()) {
                    existingCameraArray = root["cameras"].toArray();
                }

                // 建立序列号到索引的映射
                for (int i = 0; i < existingCameraArray.size(); i++) {
                    QJsonObject obj = existingCameraArray[i].toObject();
                    QString serial = obj["serialNumber"].toString("");
                    if (!serial.isEmpty()) {
                        serialToIndex[serial] = i;
                    }
                }
            }
        }
    }

    // 4. 构建新的相机数组（合并现有 + 新增/覆盖）
    QJsonArray newCameraArray;

    // 4a. 先保留所有现有的相机（除了被覆盖的）
    for (int i = 0; i < existingCameraArray.size(); i++) {
        QJsonObject obj = existingCameraArray[i].toObject();
        QString serial = obj["serialNumber"].toString("");

        // 检查这个序列号是否在新的列表中
        bool found = false;
        for (const UsedCamInfo& info : camInfoList) {
            if (info.serialNumber == serial) {
                found = true;
                break;
            }
        }

        if (!found) {
            // 不在新列表中，保留
            newCameraArray.append(obj);
        }
        // 如果在列表中，稍后会用新数据覆盖
    }

    // 4b. 添加/更新新的相机信息
    for (const UsedCamInfo& info : camInfoList) {
        if (info.serialNumber.isEmpty()) {
            continue;  // 跳过无效数据
        }

        QJsonObject camObj;
        camObj["serialNumber"] = info.serialNumber;
        camObj["ipAddress"] = info.ipAddress;
        camObj["exposure"] = info.exposure;

        // 检查是否已存在（通过序列号映射）
        if (serialToIndex.contains(info.serialNumber)) {
            // 已存在，替换（在数组中查找并替换）
            for (int i = 0; i < newCameraArray.size(); i++) {
                QJsonObject obj = newCameraArray[i].toObject();
                if (obj["serialNumber"].toString() == info.serialNumber) {
                    newCameraArray[i] = camObj;
                    break;
                }
            }
        } else {
            // 不存在，追加
            newCameraArray.append(camObj);
        }

        qDebug() << "[ConfigManager] 保存相机信息:"
                 << "序列号=" << info.serialNumber
                 << "IP=" << info.ipAddress
                 << "曝光=" << info.exposure << "us";
    }

    // 5. 更新root对象
    root["cameraInfo"] = newCameraArray;

    // 6. 写入文件
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "[ConfigManager] 无法写入配置文件:" << m_configFilePath;
        return;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "[ConfigManager] 成功保存" << camInfoList.size() << "个相机配置到:" << m_configFilePath;
}

// ============================================================
// 保存图像保存路径到配置文件
// ============================================================
void ConfigManager::SaveImageSavePathtojsonFile(const QString& path, const QString& saveImageFilePath)
{
    QMutexLocker locker(&m_mutex);

    // 1. 确定文件路径
    QString filePath = saveImageFilePath.isEmpty() ? m_configFilePath : saveImageFilePath;
    if (filePath.isEmpty()) {
        qDebug() << "[ConfigManager] 配置文件路径为空，无法保存图像保存路径";
        return;
    }

    // 2. 确保目录存在
    QDir dir = QFileInfo(filePath).dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "[ConfigManager] 无法创建配置目录:" << dir.path();
            return;
        }
    }

    // 3. 读取现有配置
    QJsonObject root;
    if (QFile::exists(filePath)) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray jsonData = file.readAll();
            file.close();

            QJsonDocument doc = QJsonDocument::fromJson(jsonData);
            if (!doc.isNull() && doc.isObject()) {
                root = doc.object();
            }
        }
    }

    // 4. 更新图像保存路径
    root["imageSavePath"] = path;
    root["imageSavePathUpdateTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 5. 写入文件
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "[ConfigManager] 无法写入配置文件:" << filePath;
        return;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    // 6. 更新成员变量
    m_imageSavePath = path;

    qDebug() << "[ConfigManager] 图像保存路径已保存:" << path;
}

// ============================================================
// 从配置文件加载图像保存路径
// ============================================================
QString ConfigManager::LoadImageSavePathFromjsonFile()
{
    QMutexLocker locker(&m_mutex);

    // 1. 检查配置文件路径是否有效
    if (m_configFilePath.isEmpty()) {
        qDebug() << "[ConfigManager] 配置文件路径为空，无法加载图像保存路径";
        return "";
    }

    // 2. 检查文件是否存在
    if (!QFile::exists(m_configFilePath)) {
        qDebug() << "[ConfigManager] 配置文件不存在:" << m_configFilePath;
        return "";
    }

    // 3. 读取并解析JSON文件
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[ConfigManager] 无法打开配置文件:" << m_configFilePath;
        return "";
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "[ConfigManager] JSON解析失败";
        return "";
    }

    QJsonObject root = doc.object();

    // 4. 读取图像保存路径
    if (!root.contains("imageSavePath")) {
        qDebug() << "[ConfigManager] 配置文件中没有图像保存路径字段";
        return "";
    }

    QString savePath = root["imageSavePath"].toString("");
    if (savePath.isEmpty()) {
        qDebug() << "[ConfigManager] 图像保存路径为空";
        return "";
    }

    // 5. 验证路径是否有效
    QDir dir(savePath);
    if (!dir.exists()) {
        // 尝试创建目录
        if (dir.mkpath(".")) {
            qDebug() << "[ConfigManager] 创建目录:" << savePath;
        } else {
            qDebug() << "[ConfigManager] 目录不存在且创建失败:" << savePath;
            return "";
        }
    }

    // 6. 更新成员变量
    m_imageSavePath = savePath;

    qDebug() << "[ConfigManager] 图像保存路径已加载:" << savePath;
    return savePath;
}

// ============================================================
// 获取图像保存路径（如果未设置则返回默认路径）
// ============================================================
QString ConfigManager::getImageSavePath() const
{
    QMutexLocker locker(&m_mutex);

    if (!m_imageSavePath.isEmpty()) {
        return m_imageSavePath;
    }

    // 返回默认路径
    return QCoreApplication::applicationDirPath() + "/captureImage";
}

bool ConfigManager::SaveCalibrationResultTojsonFile(const CalibrationResult& result,
                                                    const QString& projectName,
                                                    int boardWidth,
                                                    int boardHeight,
                                                    double squareSizeMM)
{
    QMutexLocker locker(&m_mutex);

    // 1. 检查标定是否成功
    if (!result.success) {
        qDebug() << "[ConfigManager] 标定失败，不保存结果";
        return false;
    }

    // 2. 使用单例已有的配置文件路径
    QString path = m_configFilePath;
    if (path.isEmpty()) {
        path = "./config/app_config.json";
        m_configFilePath = path;
    }

    // 3. 确保目录存在
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "[ConfigManager] 无法创建目录:" << dir.path();
            return false;
        }
    }

    // 4. 读取现有配置
    QJsonObject root;
    QJsonArray calibrationArray;

    if (QFile::exists(path)) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();

            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.isObject()) {
                root = doc.object();
                if (root.contains("calibrationResults")) {
                    calibrationArray = root["calibrationResults"].toArray();
                }
            }
        }
    }

    // 5. 检查是否已存在该项目的标定结果（按项目名查找并覆盖）
    bool found = false;
    for (int i = 0; i < calibrationArray.size(); i++) {
        QJsonObject obj = calibrationArray[i].toObject();
        if (obj["projectName"].toString() == projectName) {
            // 更新现有记录
            QJsonObject newObj = calibrationResultToJson(result, projectName,
                                                         boardWidth, boardHeight,
                                                         squareSizeMM);
            calibrationArray[i] = newObj;
            found = true;
            qDebug() << "[ConfigManager] 更新项目标定结果:" << projectName;
            break;
        }
    }

    // 6. 如果不存在，添加新记录
    if (!found) {
        QJsonObject newObj = calibrationResultToJson(result, projectName,
                                                     boardWidth, boardHeight,
                                                     squareSizeMM);
        calibrationArray.append(newObj);
        qDebug() << "[ConfigManager] 添加新项目标定结果:" << projectName;
    }

    // 7. 更新root
    root["calibrationResults"] = calibrationArray;
    root["lastUpdate"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    // 8. 写入文件
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "[ConfigManager] 无法写入配置文件:" << path;
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    // 9. 更新缓存
    m_calibrationResultCache[projectName] = result;

    emit calibrationResultSaved(projectName);
    qDebug() << "[ConfigManager] 标定结果已保存到:" << path
             << ", 项目:" << projectName
             << ", 重投影误差:" << result.reprojectionError;

    return true;
}

// ============================================================
// 从配置文件加载标定结果
// ============================================================
CalibrationResult ConfigManager::LoadCalibrationResultFromjsonFile(const QString& projectName)
{
    QMutexLocker locker(&m_mutex);

    CalibrationResult result;

    // 1. 使用单例已有的配置文件路径
    QString path = m_configFilePath;
    if (path.isEmpty()) {
        path = "./config/app_config.json";
        m_configFilePath = path;
    }

    // 2. 检查文件是否存在
    if (!QFile::exists(path)) {
        qDebug() << "[ConfigManager] 配置文件不存在:" << path;
        return result;
    }

    // 3. 读取文件
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "[ConfigManager] 无法打开配置文件:" << path;
        return result;
    }

    QByteArray data = file.readAll();
    file.close();

    // 4. 解析JSON
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "[ConfigManager] JSON解析失败";
        return result;
    }

    QJsonObject root = doc.object();

    // 5. 检查是否有标定结果
    if (!root.contains("calibrationResults") || !root["calibrationResults"].isArray()) {
        qDebug() << "[ConfigManager] 没有标定结果数据";
        return result;
    }

    QJsonArray calibrationArray = root["calibrationResults"].toArray();
    if (calibrationArray.isEmpty()) {
        qDebug() << "[ConfigManager] 标定结果列表为空";
        return result;
    }

    // 6. 清空缓存并加载所有结果
    m_calibrationResultCache.clear();

    for (const QJsonValue& value : calibrationArray) {
        QJsonObject obj = value.toObject();

        CalibrationResult cachedResult;
        cachedResult.success = obj["success"].toBool(false);
        cachedResult.imageWidth = obj["imageWidth"].toInt(0);
        cachedResult.imageHeight = obj["imageHeight"].toInt(0);
        cachedResult.reprojectionError = obj["reprojectionError"].toDouble(0.0);
        cachedResult.scale = obj["scale"].toDouble(0.0);

        // 加载内参矩阵
        QJsonArray cameraMatrixArr = obj["cameraMatrix"].toArray();
        if (cameraMatrixArr.size() == 9) {
            cachedResult.cameraMatrix = (cv::Mat_<double>(3, 3) <<
                                             cameraMatrixArr[0].toDouble(), cameraMatrixArr[1].toDouble(), cameraMatrixArr[2].toDouble(),
                                         cameraMatrixArr[3].toDouble(), cameraMatrixArr[4].toDouble(), cameraMatrixArr[5].toDouble(),
                                         cameraMatrixArr[6].toDouble(), cameraMatrixArr[7].toDouble(), cameraMatrixArr[8].toDouble()
                                         );
        }

        // 加载畸变系数
        QJsonArray distCoeffsArr = obj["distCoeffs"].toArray();
        if (distCoeffsArr.size() >= 5) {
            cachedResult.distCoeffs = (cv::Mat_<double>(1, 5) <<
                                           distCoeffsArr[0].toDouble(), distCoeffsArr[1].toDouble(),
                                       distCoeffsArr[2].toDouble(), distCoeffsArr[3].toDouble(),
                                       distCoeffsArr[4].toDouble()
                                       );
        }

        // 缓存到Map（按项目名）
        QString projName = obj["projectName"].toString();
        if (!projName.isEmpty()) {
            m_calibrationResultCache[projName] = cachedResult;
        }

        // 如果指定了项目名，匹配则返回
        if (!projectName.isEmpty() && projName == projectName) {
            result = cachedResult;
            result.success = true;
        }
    }

    // 如果没有指定项目名或没找到，返回第一个
    if (projectName.isEmpty() && !m_calibrationResultCache.isEmpty()) {
        result = m_calibrationResultCache.first();
        result.success = true;
    }

    qDebug() << "[ConfigManager] 从配置文件加载了" << m_calibrationResultCache.size() << "个标定结果";
    return result;
}


// ===== 从文件加载检测配置列表 =====
QList<DetectionConfig> ConfigManager::loadDetectionConfigsFromFile(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);

    QList<DetectionConfig> configs;

    // 如果路径为空，直接返回空列表
    if (filePath.isEmpty()) {
        qDebug() << "[ConfigManager] 文件路径为空";
        return configs;
    }

    // 检查文件是否存在
    if (!QFile::exists(filePath)) {
        qDebug() << "[ConfigManager] 配置文件不存在:" << filePath;
        return configs;
    }

    // 读取文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[ConfigManager] 无法打开文件:" << filePath;
        return configs;
    }

    QByteArray data = file.readAll();
    file.close();

    // 解析JSON
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "[ConfigManager] JSON解析失败:" << filePath;
        return configs;
    }

    QJsonObject root = doc.object();

    // ===== 修改这里：先取 detectionConfigs，再取 configs =====
    if (!root.contains("detectionConfigs") || !root["detectionConfigs"].isObject()) {
        qDebug() << "[ConfigManager] 没有detectionConfigs字段";
        return configs;
    }

    QJsonObject detectionObj = root["detectionConfigs"].toObject();

    if (!detectionObj.contains("configs") || !detectionObj["configs"].isArray()) {
        qDebug() << "[ConfigManager] detectionConfigs中没有configs字段";
        return configs;
    }

    QJsonArray configsArray = detectionObj["configs"].toArray();

    // 解析每个配置
    for (const QJsonValue& value : configsArray) {
        if (!value.isObject()) {
            continue;
        }

        QJsonObject configObj = value.toObject();
        DetectionConfig config;
        config.id = configObj["id"].toInt();
        config.cameraId = configObj["cameraId"].toInt();
        config.projectName = configObj["projectName"].toString();
        config.lightController = configObj["lightController"].toInt();
        config.channel1 = configObj["channel1"].toInt();
        config.channel2 = configObj["channel2"].toInt();
        config.channel3 = configObj["channel3"].toInt();
        config.channel4 = configObj["channel4"].toInt();
        config.calibrated = configObj["calibrated"].toBool();
        config.groupId = configObj["groupId"].toInt();

        configs.append(config);
    }

    qDebug() << "[ConfigManager] 从文件加载了" << configs.size() << "个检测配置:" << filePath;

    // 顺便打印一下加载的配置信息
    for (const DetectionConfig& config : configs) {
        qDebug() << "  - ID:" << config.id << "项目:" << config.projectName;
    }

    return configs;
}