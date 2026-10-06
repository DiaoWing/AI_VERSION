#ifndef CAMPARASETDIALOG_H
#define CAMPARASETDIALOG_H

#include <QDialog>
#include <QMap>
#include <QString>

class QLabel;
class QComboBox;
class QSpinBox;
class QPushButton;
class QGroupBox;

class camParaSetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit camParaSetDialog(QWidget *parent = nullptr);
    ~camParaSetDialog();

    void setCurrentCamera(const QString& cameraName);
    QString getCurrentCamera() const { return m_currentCamera; }

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onRefreshClicked();
    void onApplyClicked();
    void onDefaultClicked();
    void onCameraChanged(int index);
    void onExposureChanged(int value);

private:
    void setupUI();
    void loadCameraList();
    void loadCameraParameters();
    void applyCameraParameters();
    void setDefaultParameters();
    void updateUIState();
    void selectCameraInList(const QString& cameraName);
    void checkCameraType();

    // 动态控件
    QLabel* m_labelExposureValue;
    QLabel* m_labelWhiteBalance;
    QComboBox* m_comboBoxCamera;
    QComboBox* m_comboBoxTriggerMode;
    QComboBox* m_comboBoxWhiteBalance;
    QSpinBox* m_spinBoxExposure;
    QPushButton* m_pushButtonRefresh;
    QPushButton* m_pushButtonApply;
    QPushButton* m_pushButtonDefault;
    QPushButton* m_pushButtonOk;
    QPushButton* m_pushButtonCancel;
    QGroupBox* m_groupBoxParams;

    QString m_currentCamera;
    QMap<QString, QString> m_cameraMap;
    bool m_isLoading;
};

#endif // CAMPARASETDIALOG_H