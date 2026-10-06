#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QMap>
#include <QCloseEvent>
#include <QDir>
#include <QDateTime>
#include "layoutmanager.h"
#include "cameramanager.h"
#include "camparasetdialog.h"
#include "modbusslavedialog.h"
#include "modbusmasterdialog.h"
#include "detectionconfigdialog.h"
#include "calibrationdialog.h"
#include "MessageQueue.h"
#include "BusinessWorker.h"
#include "detectionworker.h"
#include "detectiondisplaydialog.h"
#include "ConfigManager.h"
#include "filecleaner.h"   //在内存盘内存不足时对其进行清理
#include "tool.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

    //bool eventFilter(QObject* obj, QEvent* event) override; //UI显示文字

private slots:
    void on_Refresh_clicked();
    void updateCameraStatus(const QString& name, bool isOpen, bool isGrabbing);
    void updateImageDisplay(const QString& name, QImage image);
    void on_checkBox_cam1_stateChanged(int state);
    void on_checkBox_cam2_stateChanged(int state);
    void on_checkBox_cam3_stateChanged(int state);
    void on_checkBox_cam4_stateChanged(int state);

    // 配置管理相关槽函数
    void on_actioncreatConfig_triggered();      // 创建新配置
    void on_actionimportConfig_triggered();     // 导入配置

    void on_getImage_clicked();
    void on_camParaSet_clicked();
    void on_DecResult_clicked();
    void onTriggerCapture(int groupId, const ModbusMessage& msg);
    void onImageCapturedForProject(const QString& cameraName, QImage image);
    void onDetectionFinished(const DetectionResult& result);

signals:
    void requestDetection(const DetectionTask& task);  //开始了可以检测

private:
    Ui::MainWindow *ui;
    LayoutManager* m_layoutManager;
    CameraManager* m_cameraManager;

    QMap<QString, QLabel*> m_displayLabels;
    QLabel* m_mainDisplayLabel;
    QString m_currentMainCamera;
    bool m_isMainDisplay;
    QString m_zoomedCamera;

    // 记录每个相机的选中状态
    QMap<QString, bool> m_cameraCheckStates;

    void initLayouts();
    void updateUIStatus();
    void setupImageDisplays();
    void setMainDisplay(const QString& cameraName);
    void restoreSmallDisplay();
    void showImageOnLabel(QLabel* label, const QImage& image);
    void cleanupAndExit();
    void updateCameraCheckState(const QString& name, bool checked);
    bool shouldCameraBeActive(const QString& name);

    QString generateImageFileName();
    bool saveImageToFile(const QImage& image, const QString& filePath);

    void on_actionmodbusserver_triggered();
    void on_actionmodbusclient_triggered();

    modbusSlaveDialog* m_modbusDialog;
    ModbusMasterDialog* m_masterDialog;
    void onShowModbusDialog();

    void on_actiondecConfigTable_triggered();
    void on_actionclibration_triggered();

    BusinessWorker* m_businessWorker;
    QThread* m_businessThread;

    void onRegisterChanged(quint16 address, quint16 value);
    void onTaskProcessed(const ModbusMessage& msg);

    ModbusMessage m_currentTask;
    int m_currentProjectId;
    void captureImageFromCamera(const QString& cameraName,QString projectName);

    // 当前的检测
    DetectionWorker* m_detectionWorker;
    QThread* m_detectionThread;

    // 当前任务
    int m_taskCounter;

    DetectionDisplayDialog* m_detectionDisplayDialog;
    void setupDetectionWorker();
    void onDetectionDebugInfo(const QString& info);

    DetectionDisplayDialog* m_detectionDialog = nullptr;

    QString getProjectNameById(int projectId);
    void forwardDetectionResult(const DetectionResult& result);

    // 配置管理相关
    DetectionConfigDialog* m_detectionConfigDialog = nullptr;

    QList<DetectionConfig> m_detectionConfigs; //当前的检测项目表，需要实时的实现更新！

    QString m_currentConfigFilePath;

    camParaSetDialog* m_camParaSetDialog = nullptr;

    bool hasValidConfig() const;
    void loadConfigToDialog(DetectionConfigDialog* dialog);
    void saveConfigFromDialog(DetectionConfigDialog* dialog);
    void updateConfigFileStatus(const QString& filePath);

    //帮助 和 关于
    void  on_actionUserManual_triggered();
    void  on_actionbaseinfo_triggered();

private slots:
    void on_actionReportIssue_triggered();
    void on_actionsavepathconfig_triggered();

private:
    void showFeedbackDialog();
    void showSetImagePathDialog();

    QString m_imageSavePath;
    FileCleaner* m_fileCleaner;
    void  setupFileCleaner();
public:
    QString getImageSavePath();
    int  getCameraIdFromConfigs(const QList<DetectionConfig>& configs, int groupId);
    void  registerCoastlineParams();
};

#endif // MAINWINDOW_H