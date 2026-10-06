#include "filecleaner.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QStorageInfo>
#include <QDirIterator>

FileCleaner::FileCleaner(QObject *parent)
    : QObject(parent)
    , m_monitorPath("")
    , m_recursive(true)
    , m_thresholdPercent(50)
    , m_cleanRatio(0.5)
    , m_checkInterval(60000)  // 默认1分钟
    , m_autoCleanEnabled(true)
    , m_currentUsagePercent(0.0)
    , m_lastCleanedCount(0)
    , m_lastFreedSpaceMB(0.0)
{
    // 默认支持图片格式
    m_fileExtensions << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp"
                     << "*.tif" << "*.tiff" << "*.gif" << "*.webp";

    // 创建定时器
    m_checkTimer = new QTimer(this);
    connect(m_checkTimer, &QTimer::timeout, this, &FileCleaner::onCheckTimer);
    m_checkTimer->start(m_checkInterval);

    qDebug() << "[FileCleaner] 初始化完成，检查间隔:" << m_checkInterval << "ms";
}

FileCleaner::~FileCleaner()
{
    if (m_checkTimer) {
        m_checkTimer->stop();
    }
}

// ============================================================
// 配置方法
// ============================================================

void FileCleaner::setMonitorPath(const QString& path)
{
    m_monitorPath = path;
    qDebug() << "[FileCleaner] 监控路径:" << m_monitorPath;
}

void FileCleaner::setThresholdPercent(int percent)
{
    m_thresholdPercent = qBound(0, percent, 100);
    qDebug() << "[FileCleaner] 阈值:" << m_thresholdPercent << "%";
}

void FileCleaner::setCleanRatio(double ratio)
{
    m_cleanRatio = qBound(0.0, ratio, 1.0);
    qDebug() << "[FileCleaner] 清理比例:" << m_cleanRatio;
}

void FileCleaner::setFileExtensions(const QStringList& extensions)
{
    m_fileExtensions = extensions;
    qDebug() << "[FileCleaner] 文件过滤:" << m_fileExtensions.join(", ");
}

void FileCleaner::setRecursive(bool recursive)
{
    m_recursive = recursive;
    qDebug() << "[FileCleaner] 递归扫描:" << m_recursive;
}

void FileCleaner::setCheckInterval(int milliseconds)
{
    m_checkInterval = milliseconds;
    if (m_checkTimer) {
        m_checkTimer->setInterval(m_checkInterval);
    }
    qDebug() << "[FileCleaner] 检查间隔:" << m_checkInterval << "ms";
}

void FileCleaner::setAutoCleanEnabled(bool enabled)
{
    m_autoCleanEnabled = enabled;
    if (m_checkTimer) {
        if (enabled) {
            m_checkTimer->start();
        } else {
            m_checkTimer->stop();
        }
    }
    qDebug() << "[FileCleaner] 自动清理:" << (enabled ? "开启" : "关闭");
}

// ============================================================
// 磁盘信息
// ============================================================

bool FileCleaner::getDiskInfo(const QString& path, qint64& totalBytes, qint64& freeBytes)
{
    if (path.isEmpty()) {
        emit errorOccurred("监控路径为空");
        return false;
    }

    QStorageInfo storage(path);
    if (!storage.isValid()) {
        emit errorOccurred(QString("无效的路径: %1").arg(path));
        return false;
    }

    totalBytes = storage.bytesTotal();
    freeBytes = storage.bytesFree();

    return true;
}

double FileCleaner::checkDiskUsage()
{
    if (m_monitorPath.isEmpty()) {
        emit errorOccurred("监控路径未设置");
        return -1.0;
    }

    qint64 totalBytes, freeBytes;
    if (!getDiskInfo(m_monitorPath, totalBytes, freeBytes)) {
        return -1.0;
    }

    double usedPercent = 100.0 * (totalBytes - freeBytes) / totalBytes;
    m_currentUsagePercent = usedPercent;

    qDebug() << "[FileCleaner] 磁盘使用率:" << QString::number(usedPercent, 'f', 1) << "%"
             << "(" << (totalBytes - freeBytes) / (1024.0 * 1024.0 * 1024.0) << "GB /"
             << totalBytes / (1024.0 * 1024.0 * 1024.0) << "GB)";

    return usedPercent;
}

// ============================================================
// 文件操作
// ============================================================

QMap<QDateTime, QString> FileCleaner::getAllFiles(const QString& path)
{
    QMap<QDateTime, QString> files;  // 自动按时间排序

    QDir dir(path);
    if (!dir.exists()) {
        emit cleanProgress(QString("目录不存在: %1").arg(path));
        return files;
    }

    // 获取文件列表（使用过滤器）
    QFileInfoList fileInfoList;
    if (m_recursive) {
        // 递归获取所有文件
        QDirIterator it(path, m_fileExtensions, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QFileInfo info = it.fileInfo();
            files[info.lastModified()] = info.absoluteFilePath();
        }
    } else {
        // 只获取当前目录
        dir.setNameFilters(m_fileExtensions);
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        fileInfoList = dir.entryInfoList();
        for (const QFileInfo& info : fileInfoList) {
            files[info.lastModified()] = info.absoluteFilePath();
        }
    }

    emit cleanProgress(QString("找到 %1 个文件").arg(files.size()));
    return files;
}

double FileCleaner::calculateTotalSize(const QMap<QDateTime, QString>& files)
{
    double totalSizeMB = 0.0;
    QMapIterator<QDateTime, QString> it(files);
    while (it.hasNext()) {
        it.next();
        QFileInfo info(it.value());
        totalSizeMB += info.size() / (1024.0 * 1024.0);
    }
    return totalSizeMB;
}

// ============================================================
// 清理逻辑
// ============================================================

int FileCleaner::performClean(QMap<QDateTime, QString>& files, double targetSizeMB)
{
    int cleanedCount = 0;
    double currentSizeMB = calculateTotalSize(files);

    emit cleanProgress(QString("当前文件总大小: %1 MB，目标保留: %2 MB")
                           .arg(currentSizeMB, 0, 'f', 2)
                           .arg(targetSizeMB, 0, 'f', 2));

    // 如果当前大小已经小于目标，不需要清理
    if (currentSizeMB <= targetSizeMB) {
        emit cleanProgress("当前大小未超过阈值，无需清理");
        return 0;
    }

    // 按时间从旧到新遍历（QMap已自动排序）
    QMapIterator<QDateTime, QString> it(files);
    while (it.hasNext() && currentSizeMB > targetSizeMB) {
        it.next();
        QString filePath = it.value();
        QFileInfo info(filePath);
        double fileSizeMB = info.size() / (1024.0 * 1024.0);

        // 删除文件
        QFile file(filePath);
        if (file.remove()) {
            cleanedCount++;
            currentSizeMB -= fileSizeMB;
            emit cleanProgress(QString("已删除: %1 (%2 MB)")
                                   .arg(QFileInfo(filePath).fileName())
                                   .arg(fileSizeMB, 0, 'f', 2));
        } else {
            emit cleanProgress(QString("删除失败: %1").arg(filePath));
        }
    }

    return cleanedCount;
}

bool FileCleaner::cleanNow()
{
    qDebug() << "[FileCleaner] 开始手动清理...";
    emit cleanProgress("========== 开始清理 ==========");

    if (m_monitorPath.isEmpty()) {
        emit errorOccurred("监控路径未设置");
        return false;
    }

    // 1. 获取所有文件
    QMap<QDateTime, QString> files = getAllFiles(m_monitorPath);
    if (files.isEmpty()) {
        emit cleanProgress("没有找到需要清理的文件");
        emit cleanFinished(0, 0.0);
        return true;
    }

    // 2. 计算当前总大小
    double totalSizeMB = calculateTotalSize(files);
    emit cleanProgress(QString("总文件数: %1，总大小: %2 MB")
                           .arg(files.size())
                           .arg(totalSizeMB, 0, 'f', 2));

    // 3. 计算需要保留的大小
    double targetSizeMB = totalSizeMB * (1.0 - m_cleanRatio);
    emit cleanProgress(QString("清理比例: %1%，目标保留: %2 MB")
                           .arg(m_cleanRatio * 100, 0, 'f', 1)
                           .arg(targetSizeMB, 0, 'f', 2));

    // 4. 执行清理
    int cleanedCount = performClean(files, targetSizeMB);

    // 5. 计算释放空间
    double freedSpaceMB = 0.0;
    if (cleanedCount > 0) {
        // 重新计算剩余文件大小
        QMap<QDateTime, QString> remainingFiles = getAllFiles(m_monitorPath);
        double remainingSizeMB = calculateTotalSize(remainingFiles);
        freedSpaceMB = totalSizeMB - remainingSizeMB;
    }

    // 6. 更新状态
    m_lastCleanedCount = cleanedCount;
    m_lastFreedSpaceMB = freedSpaceMB;
    m_lastCleanTime = QDateTime::currentDateTime();

    qDebug() << "[FileCleaner] 清理完成: 删除" << cleanedCount << "个文件，"
             << "释放" << QString::number(freedSpaceMB, 'f', 2) << "MB";

    emit cleanProgress(QString("========== 清理完成 =========="));
    emit cleanProgress(QString("✅ 删除 %1 个文件，释放 %2 MB 空间")
                           .arg(cleanedCount)
                           .arg(freedSpaceMB, 0, 'f', 2));

    emit cleanFinished(cleanedCount, freedSpaceMB);
    return true;
}

// ============================================================
// 定时检查
// ============================================================

void FileCleaner::onCheckTimer()
{
    if (!m_autoCleanEnabled) {
        return;
    }

    if (m_monitorPath.isEmpty()) {
        return;
    }

    // 检查磁盘使用率
    double usage = checkDiskUsage();
    if (usage < 0) {
        return;
    }

    // 如果超过阈值，触发清理
    if (usage >= m_thresholdPercent) {
        emit diskUsageWarning(usage, m_thresholdPercent);
        qDebug() << "[FileCleaner] ⚠️ 磁盘使用率超过阈值:"
                 << QString::number(usage, 'f', 1) << "% >= "
                 << m_thresholdPercent << "%";

        cleanNow();
    }
}