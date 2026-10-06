#ifndef PARAMMANAGER_H
#define PARAMMANAGER_H

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>

struct ParamDefinition {
    QString name;
    QString displayName;
    QString type;        // "int", "double", "bool", "enum", "string"
    QVariant defaultValue;
    QVariant minValue;
    QVariant maxValue;
    QStringList enumValues;
};

struct ProjectParams {
    QString projectName;
    QList<ParamDefinition> paramDefs;
    QMap<QString, QVariant> paramValues;
};

class ParamManager : public QObject
{
    Q_OBJECT

public:
    static ParamManager* instance();

    // ==================== 原有功能 ====================
    void registerProject(const ProjectParams& params);
    QList<ParamDefinition> getParamDefs(const QString& projectName) const;
    QMap<QString, QVariant> getParamValues(const QString& projectName) const;
    void updateParamValues(const QString& projectName, const QMap<QString, QVariant>& values);
    bool hasProject(const QString& projectName) const;
    QStringList getProjectNames() const;

    // ==================== 新增：便捷参数访问方法 ====================

    /**
     * @brief 获取单个参数值（模板方法）
     * @param projectName 项目名称
     * @param paramName 参数名称
     * @param defaultValue 默认值（参数不存在时返回）
     * @return 参数值
     */
    template<typename T>
    T getParam(const QString& projectName, const QString& paramName, const T& defaultValue = T()) const {
        if (m_projects.contains(projectName)) {
            const ProjectParams& params = m_projects[projectName];
            if (params.paramValues.contains(paramName)) {
                return params.paramValues[paramName].value<T>();
            }
        }
        return defaultValue;
    }

    /**
     * @brief 获取整数类型参数
     */
    int getIntParam(const QString& projectName, const QString& paramName, int defaultValue = 0) const {
        return getParam<int>(projectName, paramName, defaultValue);
    }

    /**
     * @brief 获取浮点数类型参数
     */
    double getDoubleParam(const QString& projectName, const QString& paramName, double defaultValue = 0.0) const {
        return getParam<double>(projectName, paramName, defaultValue);
    }

    /**
     * @brief 获取布尔类型参数
     */
    bool getBoolParam(const QString& projectName, const QString& paramName, bool defaultValue = false) const {
        return getParam<bool>(projectName, paramName, defaultValue);
    }

    /**
     * @brief 获取字符串类型参数
     */
    QString getStringParam(const QString& projectName, const QString& paramName, const QString& defaultValue = QString()) const {
        return getParam<QString>(projectName, paramName, defaultValue);
    }

    /**
     * @brief 获取项目的所有参数（返回结构体）
     */
    ProjectParams getProjectParams(const QString& projectName) const {
        if (m_projects.contains(projectName)) {
            return m_projects[projectName];
        }
        return ProjectParams();
    }

    /**
     * @brief 批量更新参数并自动保存到JSON
     * @param projectName 项目名称
     * @param values 参数键值对
     * @return 是否成功
     */
    bool updateAndSave(const QString& projectName, const QMap<QString, QVariant>& values) {
        updateParamValues(projectName, values);
        return saveProjectParamsToJson(projectName);
    }

    // ==================== JSON保存和加载功能 ====================
    bool saveProjectParamsToJson(const QString& projectName, const QString& filePath = QString());
    bool loadProjectParamsFromJson(const QString& filePath);
    bool loadAllProjectsFromJson(const QString& directoryPath = QString());
    QString getDefaultJsonPath(const QString& projectName) const;
    void setJsonDirectory(const QString& directoryPath);
    QString getJsonDirectory() const { return m_jsonDirectory; }

signals:
    void paramsSaved(const QString& projectName);
    void paramsLoaded(const QString& projectName);

private:
    ParamManager(QObject* parent = nullptr);
    ~ParamManager();

    static ParamManager* m_instance;
    QMap<QString, ProjectParams> m_projects;
    QString m_jsonDirectory;  // JSON文件存储目录

    bool saveToFile(const QString& filePath, const ProjectParams& params);
    bool loadFromFile(const QString& filePath, ProjectParams& params);
    QJsonObject projectParamsToJson(const ProjectParams& params);
    ProjectParams jsonToProjectParams(const QJsonObject& jsonObj);
};

#endif // PARAMMANAGER_H