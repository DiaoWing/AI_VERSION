#include "parammanager.h"

ParamManager* ParamManager::m_instance = nullptr;

ParamManager::ParamManager(QObject* parent)
    : QObject(parent)
{
    // 设置默认JSON存储目录
    m_jsonDirectory = QCoreApplication::applicationDirPath() + "/param_configs";
    QDir dir(m_jsonDirectory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

ParamManager::~ParamManager()
{
}

ParamManager* ParamManager::instance()
{
    if (!m_instance) {
        m_instance = new ParamManager();
    }
    return m_instance;
}

void ParamManager::registerProject(const ProjectParams& params)
{
    if (params.projectName.isEmpty()) return;
    m_projects[params.projectName] = params;
    qDebug() << "[ParamManager] 注册项目:" << params.projectName
             << ", 参数数量:" << params.paramDefs.size();
}

QList<ParamDefinition> ParamManager::getParamDefs(const QString& projectName) const
{
    if (m_projects.contains(projectName)) {
        return m_projects[projectName].paramDefs;
    }
    return QList<ParamDefinition>();
}

QMap<QString, QVariant> ParamManager::getParamValues(const QString& projectName) const
{
    if (m_projects.contains(projectName)) {
        return m_projects[projectName].paramValues;
    }
    return QMap<QString, QVariant>();
}

void ParamManager::updateParamValues(const QString& projectName,
                                     const QMap<QString, QVariant>& values)
{
    if (m_projects.contains(projectName)) {
        m_projects[projectName].paramValues = values;
        qDebug() << "[ParamManager] 更新项目参数:" << projectName
                 << ", 参数数量:" << values.size();
    }
}

bool ParamManager::hasProject(const QString& projectName) const
{
    return m_projects.contains(projectName);
}

QStringList ParamManager::getProjectNames() const
{
    return m_projects.keys();
}

// ==================== JSON功能实现 ====================

QString ParamManager::getDefaultJsonPath(const QString& projectName) const
{
    return m_jsonDirectory + "/" + projectName + "_params.json";
}

void ParamManager::setJsonDirectory(const QString& directoryPath)
{
    m_jsonDirectory = directoryPath;
    QDir dir(m_jsonDirectory);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}

bool ParamManager::saveProjectParamsToJson(const QString& projectName, const QString& filePath)
{
    if (!m_projects.contains(projectName)) {
        qDebug() << "[ParamManager] 项目不存在:" << projectName;
        return false;
    }

    QString savePath = filePath.isEmpty() ? getDefaultJsonPath(projectName) : filePath;
    return saveToFile(savePath, m_projects[projectName]);
}

bool ParamManager::loadProjectParamsFromJson(const QString& filePath)
{
    ProjectParams params;
    if (!loadFromFile(filePath, params)) {
        return false;
    }

    // 如果项目已存在，更新参数值；否则注册新项目
    if (m_projects.contains(params.projectName)) {
        m_projects[params.projectName].paramValues = params.paramValues;
        qDebug() << "[ParamManager] 更新项目参数从JSON:" << params.projectName;
    } else {
        registerProject(params);
        qDebug() << "[ParamManager] 从JSON加载新项目:" << params.projectName;
    }

    emit paramsLoaded(params.projectName);
    return true;
}

bool ParamManager::loadAllProjectsFromJson(const QString& directoryPath)
{
    QString dirPath = directoryPath.isEmpty() ? m_jsonDirectory : directoryPath;
    QDir dir(dirPath);

    if (!dir.exists()) {
        qDebug() << "[ParamManager] 目录不存在:" << dirPath;
        return false;
    }

    QStringList filters;
    filters << "*_params.json";
    QStringList jsonFiles = dir.entryList(filters, QDir::Files);

    if (jsonFiles.isEmpty()) {
        qDebug() << "[ParamManager] 未找到JSON配置文件";
        return false;
    }

    int loadedCount = 0;
    for (const QString& fileName : jsonFiles) {
        QString fullPath = dir.absoluteFilePath(fileName);
        if (loadProjectParamsFromJson(fullPath)) {
            loadedCount++;
        }
    }

    qDebug() << "[ParamManager] 加载了" << loadedCount << "个项目的配置";
    return loadedCount > 0;
}

bool ParamManager::saveToFile(const QString& filePath, const ProjectParams& params)
{
    QJsonObject jsonObj = projectParamsToJson(params);
    QJsonDocument doc(jsonObj);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "[ParamManager] 无法创建文件:" << filePath;
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "[ParamManager] 参数已保存到:" << filePath;
    emit paramsSaved(params.projectName);
    return true;
}

bool ParamManager::loadFromFile(const QString& filePath, ProjectParams& params)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[ParamManager] 无法打开文件:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "[ParamManager] JSON解析失败:" << filePath;
        return false;
    }

    params = jsonToProjectParams(doc.object());
    return true;
}

QJsonObject ParamManager::projectParamsToJson(const ProjectParams& params)
{
    QJsonObject rootObj;
    rootObj["projectName"] = params.projectName;
    rootObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObj["version"] = "1.0";

    // 保存参数值
    QJsonObject valuesObj;
    for (auto it = params.paramValues.begin(); it != params.paramValues.end(); ++it) {
        QJsonValue value;
        switch (it.value().type()) {
        case QVariant::Int:
            value = it.value().toInt();
            break;
        case QVariant::Double:
            value = it.value().toDouble();
            break;
        case QVariant::Bool:
            value = it.value().toBool();
            break;
        case QVariant::String:
            value = it.value().toString();
            break;
        default:
            value = it.value().toString();
            break;
        }
        valuesObj[it.key()] = value;
    }
    rootObj["paramValues"] = valuesObj;

    // 保存参数定义（可选，用于恢复参数结构）
    QJsonArray defsArray;
    for (const ParamDefinition& def : params.paramDefs) {
        QJsonObject defObj;
        defObj["name"] = def.name;
        defObj["displayName"] = def.displayName;
        defObj["type"] = def.type;
        defObj["defaultValue"] = QJsonValue::fromVariant(def.defaultValue);
        defObj["minValue"] = QJsonValue::fromVariant(def.minValue);
        defObj["maxValue"] = QJsonValue::fromVariant(def.maxValue);
        if (!def.enumValues.isEmpty()) {
            defObj["enumValues"] = QJsonArray::fromStringList(def.enumValues);
        }
        defsArray.append(defObj);
    }
    rootObj["paramDefs"] = defsArray;

    return rootObj;
}

ProjectParams ParamManager::jsonToProjectParams(const QJsonObject& jsonObj)
{
    ProjectParams params;
    params.projectName = jsonObj["projectName"].toString();

    // 恢复参数值
    QJsonObject valuesObj = jsonObj["paramValues"].toObject();
    for (auto it = valuesObj.begin(); it != valuesObj.end(); ++it) {
        params.paramValues[it.key()] = it.value().toVariant();
    }

    // 恢复参数定义
    QJsonArray defsArray = jsonObj["paramDefs"].toArray();
    for (const QJsonValue& val : defsArray) {
        QJsonObject defObj = val.toObject();
        ParamDefinition def;
        def.name = defObj["name"].toString();
        def.displayName = defObj["displayName"].toString();
        def.type = defObj["type"].toString();
        def.defaultValue = defObj["defaultValue"].toVariant();
        def.minValue = defObj["minValue"].toVariant();
        def.maxValue = defObj["maxValue"].toVariant();

        if (defObj.contains("enumValues")) {
            QJsonArray enumArray = defObj["enumValues"].toArray();
            for (const QJsonValue& enumVal : enumArray) {
                def.enumValues.append(enumVal.toString());
            }
        }
        params.paramDefs.append(def);
    }

    return params;
}