#ifndef FILECLEANER_H
#define FILECLEANER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QDateTime>
#include <QMap>

/**
 * @brief 文件清理器 - 自动清理旧文件以释放磁盘空间
 *
 * 功能：
 * 1. 监控指定路径下的磁盘使用率
 * 2. 当使用率超过阈值时自动清理旧文件
 * 3. 保留最新的文件，删除最旧的文件
 */
class FileCleaner : public QObject
{
    Q_OBJECT

public:
    explicit FileCleaner(QObject *parent = nullptr);
    ~FileCleaner();

    // ===== 配置方法 =====

    /// 设置要监控的路径
    void setMonitorPath(const QString& path);

    /// 设置磁盘使用率阈值 (0-100)，默认 50%
    void setThresholdPercent(int percent);

    /// 设置清理比例 (0.0-1.0)，默认 0.5 即清理 50%
    void setCleanRatio(double ratio);

    /// 设置文件扩展名过滤，默认图片格式
    void setFileExtensions(const QStringList& extensions);

    /// 设置是否递归扫描子目录
    void setRecursive(bool recursive);

    /// 设置监控间隔（毫秒），默认 60000 (1分钟)
    void setCheckInterval(int milliseconds);

    /// 启用/禁用自动监控
    void setAutoCleanEnabled(bool enabled);

    // ===== 状态查询 =====

    /// 获取当前监控路径
    QString getMonitorPath() const { return m_monitorPath; }

    /// 获取当前磁盘使用率
    double getCurrentUsagePercent() const { return m_currentUsagePercent; }

    /// 获取上次清理的文件数量
    int getLastCleanedCount() const { return m_lastCleanedCount; }

    /// 获取上次清理释放的空间 (MB)
    double getLastFreedSpaceMB() const { return m_lastFreedSpaceMB; }

    /// 获取上次清理时间
    QDateTime getLastCleanTime() const { return m_lastCleanTime; }

public slots:
    /// 手动执行一次清理
    bool cleanNow();

    /// 手动检查磁盘使用率
    double checkDiskUsage();

signals:
    /// 磁盘使用率超过阈值时发出
    void diskUsageWarning(double currentPercent, double thresholdPercent);

    /// 清理完成时发出
    void cleanFinished(int cleanedCount, double freedSpaceMB);

    /// 清理过程中发出进度信息
    void cleanProgress(const QString& info);

    /// 发生错误时发出
    void errorOccurred(const QString& error);

private slots:
    void onCheckTimer();

private:
    // ===== 核心方法 =====

    /// 递归获取所有文件及其修改时间
    QMap<QDateTime, QString> getAllFiles(const QString& path);

    /// 计算文件总大小 (MB)
    double calculateTotalSize(const QMap<QDateTime, QString>& files);

    /// 执行清理操作
    int performClean(QMap<QDateTime, QString>& files, double targetSizeMB);

    /// 获取磁盘信息
    bool getDiskInfo(const QString& path, qint64& totalBytes, qint64& freeBytes);

    // ===== 成员变量 =====

    QString m_monitorPath;                          // 监控路径
    QStringList m_fileExtensions;                   // 文件扩展名过滤
    bool m_recursive;                               // 是否递归
    int m_thresholdPercent;                         // 阈值 (%)
    double m_cleanRatio;                            // 清理比例
    int m_checkInterval;                            // 检查间隔 (ms)
    bool m_autoCleanEnabled;                        // 是否启用自动清理

    QTimer* m_checkTimer;                           // 定时检查器

    double m_currentUsagePercent;                   // 当前使用率
    int m_lastCleanedCount;                         // 上次清理文件数
    double m_lastFreedSpaceMB;                      // 上次清理释放空间 (MB)
    QDateTime m_lastCleanTime;                      // 上次清理时间
};

#endif // FILECLEANER_H
