#ifndef DETECTIONCONFIGDIALOG_H
#define DETECTIONCONFIGDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QList>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QScrollArea>

// ========== 检测配置结构体（不包含参数） ==========
struct DetectionConfig {
    int id;        //唯一标识，类似序号
    int cameraId;
    QString projectName;
    int lightController;
    int channel1, channel2, channel3, channel4;
    bool calibrated;
    int groupId;       //群组ID暂时现在是设置为不会重复的，但是可以即使群组ID相同，都去操作同一个

    DetectionConfig()
        : id(0), cameraId(1), lightController(1)
        , channel1(0), channel2(0), channel3(0), channel4(0)
        , calibrated(false), groupId(1) {}

    // 比较操作符
    bool operator==(const DetectionConfig& other) const {
        return id == other.id &&
               cameraId == other.cameraId &&
               projectName == other.projectName &&
               lightController == other.lightController &&
               channel1 == other.channel1 &&
               channel2 == other.channel2 &&
               channel3 == other.channel3 &&
               channel4 == other.channel4 &&
               calibrated == other.calibrated &&
               groupId == other.groupId;
    }

};

class DetectionConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DetectionConfigDialog(QWidget *parent = nullptr);
    ~DetectionConfigDialog();

    QList<DetectionConfig> getConfigs() const { return m_configs; }
    void setConfigs(const QList<DetectionConfig>& configs);
    void  saveTableToConfigs();
    void  updateTable();

private slots:
    void onAddClicked();
    void onDeleteClicked();
    void onClearClicked();
    void onSaveClicked();
    void onLoadClicked();
    void onConfirmClicked();
    void onCancelClicked();

    // 右键菜单槽函数
    void onCustomContextMenuRequested(const QPoint& pos);
    void onDetectSingleImage();
    void onDetectFolder();

private:
    void setupUI();
    void setupTable();
    void addRow(const DetectionConfig& config = DetectionConfig());
    void updateRowNumbers();
    DetectionConfig getRowData(int row) const;
    void setCellComboBox(int row, int col, const QStringList& items, int index);

    // 使用ConfigManager保存和加载
    bool saveToConfigManager();
    bool loadFromConfigManager();

    // 保留原有的文件保存/加载（可选）
    bool saveToFile(const QString& filePath);
    bool loadFromFile(const QString& filePath);

    // 检测相关函数
    QPixmap performDetection(const QString& imagePath, const DetectionConfig& config);
    void processImage(const QString& imagePath);

    QTableWidget* m_tableWidget;

    QList<DetectionConfig> m_configs;  //添加之后，应该马上对这个进行更新才对呀

    QMenu* m_contextMenu;
    QAction* m_detectSingleAction;
    QAction* m_detectFolderAction;

    // 下拉框选项
    QStringList m_channelOptions;
    QStringList m_cameraOptions;
    QStringList m_controllerOptions;
    QStringList m_calibratedOptions;
    QStringList m_groupOptions;
    QStringList m_projectOptions;

    // 标记是否从ConfigManager加载
    bool m_loadedFromManager;
};

#endif // DETECTIONCONFIGDIALOG_H