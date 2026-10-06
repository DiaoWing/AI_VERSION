#ifndef DETECTIONDISPLAYDIALOG_H
#define DETECTIONDISPLAYDIALOG_H

#include <QDialog>
#include <QVector>
#include <QMap>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QThread>
#include <QPushButton>
#include <QFormLayout>
#include <QPixmap>
#include <QImage>
#include <QVariant>
#include <QList>
#include <QString>
#include <QWidget>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QMenu>
#include <QAction>
#include <QProgressDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QDir>
#include "detectionconfigdialog.h"
#include "parammanager.h"
#include "detectionworker.h"

// ==================== 可双击的图像标签 ====================
class ClickableImageLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickableImageLabel(QWidget* parent = nullptr);
    ~ClickableImageLabel();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;

signals:
    void doubleClicked();
};

// ==================== 检测结果图像显示 ====================
struct DisplayItem {
    QString projectName;
    QString cameraName;
    ClickableImageLabel* imageLabel;
    QLabel* nameLabel;
    QWidget* container;
    bool hasImage;
    QPixmap lastImage;
};

// ==================== 检测结果参数面板 ====================
struct ParamPanel {
    QComboBox* projectCombo;
    QScrollArea* paramScrollArea;
    QWidget* paramContent;
    QFormLayout* paramLayout;
    QMap<QString, QWidget*> paramWidgets;
    QMap<QString, QVariant> currentValues;
    QPushButton* applyBtn;
};

// ==================== 检测结果显示对话框 ====================
class DetectionDisplayDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DetectionDisplayDialog(QWidget* parent = nullptr);
    ~DetectionDisplayDialog();

    void setDetectionConfigs(const QList<DetectionConfig>& configs);

public slots:
    void onTriggerDetection(const QString& projectName, const QImage& image);
    void onManualDetect();

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onDetectionFinished(const DetectionResult& result);
    void onDebugInfo(const QString& info);
    void onProjectSelected(int index);
    void onParamValueChanged();
    void onApplyParams();
    void onImageDoubleClicked(const QString& projectName);

    // 右键菜单槽函数
    void onCustomContextMenuRequested(const QPoint& pos);
    void onDetectSingleImage();
    void onDetectBatchImages();

private:
    QString m_contextMenuProjectName;
    void setupUI();
    void setupLeftPanel();
    void setupRightPanel();
    void setupParamPanel();
    void setupDetectionWorker();
    void setupFullScreenContainer();
    void setupContextMenu();

    void updateImageLayout();
    void createImageGrid(int count);

    void enterFullScreenMode(const QString& projectName);
    void exitFullScreenMode();
    bool isFullScreenMode() const;

    void loadProjectParams(const QString& projectName);

    // 此处涉及了对检测项目参数的控制
    void rebuildParamWidgets(const QList<ParamDefinition>& paramDefs);
    void clearParamWidgets();
    void collectCurrentParams();

    QString getButtonStyle(const QString& color);
    void showImageOnLabel(QLabel* label, const QPixmap& pixmap);

    int getProjectIdByName(const QString& projectName);
    QString getProjectNameById(int projectId);
    QString getCameraNameByProject(const QString& projectName);

    // 检测辅助函数
    void performDetectionOnImage(const QString& imagePath, const QString& projectName);
    QString getCurrentProjectName() const;

    // UI组件
    QWidget* m_leftPanel;
    QWidget* m_rightPanel;
    QScrollArea* m_leftScrollArea;
    QWidget* m_leftContent;

    QWidget* m_fullScreenContainer;
    ClickableImageLabel* m_fullScreenLabel;
    QLabel* m_fullScreenTitle;
    QLabel* m_fullScreenHint;
    QString m_fullScreenProjectName;

    QVector<DisplayItem> m_displayItems;
    QMap<QString, int> m_projectNameToIndex;
    QGridLayout* m_imageGridLayout;

    ParamPanel m_paramPanel;

    QList<DetectionConfig> m_configs;
    QMap<QString, QPixmap> m_lastResults;

    QThread* m_detectionThread;
    DetectionWorker* m_detectionWorker;
    bool m_isInitialized;

    bool m_isFullScreen;

    // 右键菜单
    QMenu* m_contextMenu;
    QAction* m_detectSingleAction;
    QAction* m_detectBatchAction;

public:
    void updateImageDisplay(const QString& projectName, const QPixmap& pixmap);
    void updateResult(const DetectionResult& result);
    int  getgroupIdByName(const QString& projectName);
    int  getNumberByName(const QString& projectName);

private:
    // 批量检测状态
    int m_batchDetectionCount = 0;
    int m_batchTotalCount = 0;

};

#endif // DETECTIONDISPLAYDIALOG_H