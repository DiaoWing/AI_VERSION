#include "camparasetdialog.h"
#include "cameramanager.h"
#include <QDebug>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QSpacerItem>

camParaSetDialog::camParaSetDialog(QWidget *parent)
    : QDialog(parent)
    , m_isLoading(false)
{
    setupUI();

    setWindowTitle("相机参数设置");
    setModal(false);
    resize(420, 380);
    setMinimumSize(350, 300);

    loadCameraList();
    updateUIState();

    qDebug() << "相机参数设置对话框已创建";
}

camParaSetDialog::~camParaSetDialog()
{
}

void camParaSetDialog::setupUI()
{
    // ===== 主垂直布局 =====
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // ============================================================
    // 第1行：相机选择
    // ============================================================
    QHBoxLayout* cameraLayout = new QHBoxLayout();
    cameraLayout->setSpacing(10);

    QLabel* labelCamera = new QLabel("选择相机:", this);
    labelCamera->setMinimumWidth(70);
    cameraLayout->addWidget(labelCamera);

    m_comboBoxCamera = new QComboBox(this);
    m_comboBoxCamera->setMinimumWidth(180);
    m_comboBoxCamera->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cameraLayout->addWidget(m_comboBoxCamera);

    cameraLayout->addStretch();

    m_pushButtonRefresh = new QPushButton("刷新", this);
    m_pushButtonRefresh->setFixedSize(70, 28);
    cameraLayout->addWidget(m_pushButtonRefresh);

    mainLayout->addLayout(cameraLayout);

    // ============================================================
    // 第2行：参数分组
    // ============================================================
    m_groupBoxParams = new QGroupBox("相机参数", this);
    m_groupBoxParams->setMinimumHeight(180);

    QGridLayout* paramLayout = new QGridLayout(m_groupBoxParams);
    paramLayout->setSpacing(12);
    paramLayout->setContentsMargins(20, 18, 20, 15);

    int row = 0;

    // ----- 曝光时间 -----
    QLabel* labelExposure = new QLabel("曝光时间:", m_groupBoxParams);
    labelExposure->setMinimumWidth(70);
    labelExposure->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    paramLayout->addWidget(labelExposure, row, 0);

    m_spinBoxExposure = new QSpinBox(m_groupBoxParams);
    m_spinBoxExposure->setMinimum(1);
    m_spinBoxExposure->setMaximum(100000);
    m_spinBoxExposure->setSingleStep(100);
    m_spinBoxExposure->setValue(10000);
    m_spinBoxExposure->setMinimumWidth(100);
    paramLayout->addWidget(m_spinBoxExposure, row, 1);

    m_labelExposureValue = new QLabel("10000 us", m_groupBoxParams);
    m_labelExposureValue->setMinimumWidth(70);
    m_labelExposureValue->setStyleSheet("color: #0055aa; font-weight: bold;");
    paramLayout->addWidget(m_labelExposureValue, row, 2);

    paramLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 3);
    row++;

    // ----- 触发模式 -----
    QLabel* labelTriggerMode = new QLabel("采集模式:", m_groupBoxParams);
    labelTriggerMode->setMinimumWidth(70);
    labelTriggerMode->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    paramLayout->addWidget(labelTriggerMode, row, 0);

    m_comboBoxTriggerMode = new QComboBox(m_groupBoxParams);
    m_comboBoxTriggerMode->addItem("连续模式");
    m_comboBoxTriggerMode->addItem("触发模式");
    m_comboBoxTriggerMode->setMinimumWidth(120);
    paramLayout->addWidget(m_comboBoxTriggerMode, row, 1);

    paramLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 2);
    paramLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 3);
    row++;

    // ----- 白平衡（动态显示/隐藏） -----
    m_labelWhiteBalance = new QLabel("白  平  衡:", m_groupBoxParams);
    m_labelWhiteBalance->setMinimumWidth(70);
    m_labelWhiteBalance->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    paramLayout->addWidget(m_labelWhiteBalance, row, 0);

    m_comboBoxWhiteBalance = new QComboBox(m_groupBoxParams);
    m_comboBoxWhiteBalance->addItem("关闭");
    m_comboBoxWhiteBalance->addItem("一次白平衡");
    m_comboBoxWhiteBalance->addItem("连续白平衡");
    m_comboBoxWhiteBalance->setMinimumWidth(120);
    paramLayout->addWidget(m_comboBoxWhiteBalance, row, 1);

    // 默认隐藏白平衡（黑白相机不支持）
    m_labelWhiteBalance->setVisible(false);
    m_comboBoxWhiteBalance->setVisible(false);

    paramLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 2);
    paramLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), row, 3);
    row++;

    // ----- 按钮行 -----
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_pushButtonApply = new QPushButton("应用参数", m_groupBoxParams);
    m_pushButtonApply->setFixedSize(90, 30);
    buttonLayout->addWidget(m_pushButtonApply);

    buttonLayout->addSpacing(15);

    m_pushButtonDefault = new QPushButton("恢复默认", m_groupBoxParams);
    m_pushButtonDefault->setFixedSize(90, 30);
    buttonLayout->addWidget(m_pushButtonDefault);

    paramLayout->addLayout(buttonLayout, row, 0, 1, 4);

    mainLayout->addWidget(m_groupBoxParams);

    // ============================================================
    // 第3行：确定/取消按钮
    // ============================================================
    QHBoxLayout* buttonBoxLayout = new QHBoxLayout();
    buttonBoxLayout->addStretch();

    m_pushButtonOk = new QPushButton("确定", this);
    m_pushButtonOk->setFixedSize(80, 32);
    buttonBoxLayout->addWidget(m_pushButtonOk);

    buttonBoxLayout->addSpacing(10);

    m_pushButtonCancel = new QPushButton("取消", this);
    m_pushButtonCancel->setFixedSize(80, 32);
    buttonBoxLayout->addWidget(m_pushButtonCancel);

    mainLayout->addLayout(buttonBoxLayout);

    // ===== 连接信号槽 =====
    connect(m_comboBoxCamera, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &camParaSetDialog::onCameraChanged);

    connect(m_spinBoxExposure, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &camParaSetDialog::onExposureChanged);

    connect(m_pushButtonRefresh, &QPushButton::clicked,
            this, &camParaSetDialog::onRefreshClicked);

    connect(m_pushButtonApply, &QPushButton::clicked,
            this, &camParaSetDialog::onApplyClicked);

    connect(m_pushButtonDefault, &QPushButton::clicked,
            this, &camParaSetDialog::onDefaultClicked);

    connect(m_pushButtonOk, &QPushButton::clicked,
            this, &camParaSetDialog::onOkClicked);

    connect(m_pushButtonCancel, &QPushButton::clicked,
            this, &camParaSetDialog::onCancelClicked);
}

void camParaSetDialog::setCurrentCamera(const QString& cameraName)
{
    qDebug() << "设置当前相机:" << cameraName;
    m_currentCamera = cameraName;

    selectCameraInList(cameraName);

    if (!m_currentCamera.isEmpty()) {
        checkCameraType();
        loadCameraParameters();
        updateUIState();
    }
}

void camParaSetDialog::checkCameraType()
{
    CameraManager* manager = CameraManager::instance();
    if (!manager) {
        m_labelWhiteBalance->setVisible(false);
        m_comboBoxWhiteBalance->setVisible(false);
        return;
    }

    CameraInfo* info = manager->getCamera(m_currentCamera);
    if (!info || !info->camera || !info->isOpen) {
        m_labelWhiteBalance->setVisible(false);
        m_comboBoxWhiteBalance->setVisible(false);
        return;
    }

    // 尝试获取白平衡支持（彩色相机才有）
    MVCC_ENUMVALUE enumVal;
    bool hasWhiteBalance = (info->camera->getEnumValue("BalanceWhiteAuto", &enumVal) == MV_OK);

    m_labelWhiteBalance->setVisible(hasWhiteBalance);
    m_comboBoxWhiteBalance->setVisible(hasWhiteBalance);

    qDebug() << "相机" << m_currentCamera
             << "白平衡支持:" << (hasWhiteBalance ? "是(彩色)" : "否(黑白)");
}

void camParaSetDialog::selectCameraInList(const QString& cameraName)
{
    for (int i = 0; i < m_comboBoxCamera->count(); i++) {
        QString displayName = m_comboBoxCamera->itemText(i);
        QString camName = m_cameraMap.value(displayName, "");

        if (camName == cameraName) {
            m_comboBoxCamera->setCurrentIndex(i);
            qDebug() << "已在列表中选中:" << displayName;
            return;
        }
    }

    qDebug() << "未在列表中找到相机:" << cameraName;
}

void camParaSetDialog::loadCameraList()
{
    m_comboBoxCamera->clear();
    m_cameraMap.clear();

    CameraManager* manager = CameraManager::instance();
    if (!manager) {
        m_comboBoxCamera->addItem("相机管理器未初始化");
        return;
    }

    auto cameras = manager->getAllCameras();
    bool hasOpenCamera = false;

    for (auto* info : cameras) {
        if (info && info->isOpen) {
            QString displayName = QString("%1 (%2)")
            .arg(info->name)
                .arg(info->modelName.isEmpty() ? "未知型号" : info->modelName);

            m_comboBoxCamera->addItem(displayName);
            m_cameraMap[displayName] = info->name;
            hasOpenCamera = true;

            qDebug() << "添加相机:" << info->name << info->modelName;
        }
    }

    if (!hasOpenCamera) {
        m_comboBoxCamera->addItem("无可用相机");
        m_groupBoxParams->setEnabled(false);
        m_pushButtonApply->setEnabled(false);
    } else {
        m_groupBoxParams->setEnabled(true);
        m_pushButtonApply->setEnabled(true);
        m_comboBoxCamera->setCurrentIndex(0);
    }
}

void camParaSetDialog::loadCameraParameters()
{
    if (m_currentCamera.isEmpty()) {
        qDebug() << "当前相机为空，无法加载参数";
        return;
    }

    m_isLoading = true;

    CameraManager* manager = CameraManager::instance();
    if (!manager) {
        m_isLoading = false;
        return;
    }

    CameraInfo* info = manager->getCamera(m_currentCamera);
    if (!info || !info->camera || !info->isOpen) {
        qDebug() << "相机未打开:" << m_currentCamera;
        m_isLoading = false;
        return;
    }

    MvCameraQt* camera = info->camera;

    // ===== 读取曝光时间 =====
    MVCC_FLOATVALUE floatVal;
    if (camera->getFloatValue("ExposureTime", &floatVal) == MV_OK) {
        m_spinBoxExposure->setValue((int)floatVal.fCurValue);
        m_labelExposureValue->setText(QString("%1 us").arg((int)floatVal.fCurValue));
        qDebug() << "曝光时间:" << floatVal.fCurValue;
    } else {
        m_spinBoxExposure->setValue(10000);
        m_labelExposureValue->setText("10000 us");
    }

    // ===== 读取触发模式 =====
    MVCC_ENUMVALUE enumVal;
    if (camera->getEnumValue("TriggerMode", &enumVal) == MV_OK) {
        m_comboBoxTriggerMode->setCurrentIndex((int)enumVal.nCurValue);
        qDebug() << "触发模式:" << enumVal.nCurValue;
    } else {
        m_comboBoxTriggerMode->setCurrentIndex(0);
    }

    // ===== 读取白平衡（只在支持时读取） =====
    if (m_comboBoxWhiteBalance->isVisible()) {
        if (camera->getEnumValue("BalanceWhiteAuto", &enumVal) == MV_OK) {
            m_comboBoxWhiteBalance->setCurrentIndex((int)enumVal.nCurValue);
            qDebug() << "白平衡模式:" << enumVal.nCurValue;
        }
    }

    m_isLoading = false;
}

void camParaSetDialog::applyCameraParameters()
{
    if (m_currentCamera.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择相机");
        return;
    }

    CameraManager* manager = CameraManager::instance();
    if (!manager) {
        QMessageBox::warning(this, "错误", "相机管理器未初始化");
        return;
    }

    CameraInfo* info = manager->getCamera(m_currentCamera);
    if (!info || !info->camera || !info->isOpen) {
        QMessageBox::warning(this, "错误", "相机未打开");
        return;
    }

    MvCameraQt* camera = info->camera;

    // ===== 设置曝光时间 =====
    int exposure = m_spinBoxExposure->value();
    if (camera->setFloatValue("ExposureTime", exposure) == MV_OK) {
        qDebug() << "设置曝光时间:" << exposure;
    } else {
        qWarning() << "设置曝光时间失败";
    }

    // ===== 设置触发模式 =====
    int triggerMode = m_comboBoxTriggerMode->currentIndex();
    if (camera->setEnumValue("TriggerMode", triggerMode) == MV_OK) {
        qDebug() << "设置触发模式:" << triggerMode;
    } else {
        qWarning() << "设置触发模式失败";
    }

    // ===== 设置白平衡（只在可见时设置） =====
    if (m_comboBoxWhiteBalance->isVisible()) {
        int wbMode = m_comboBoxWhiteBalance->currentIndex();
        if (camera->setEnumValue("BalanceWhiteAuto", wbMode) == MV_OK) {
            qDebug() << "设置白平衡模式:" << wbMode;
        }
    }

    QMessageBox::information(this, "成功", "相机参数已应用");

    if (manager) {
        manager->saveCurrentCameraConfig();
        qDebug() << "[camParaSetDialog] 相机配置已保存";
    }

    QMessageBox::information(this, "成功", "相机参数已应用并保存");
}

void camParaSetDialog::setDefaultParameters()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认",
                                  "确定要恢复默认参数吗？",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_spinBoxExposure->setValue(10000);
        m_comboBoxTriggerMode->setCurrentIndex(0);
        if (m_comboBoxWhiteBalance->isVisible()) {
            m_comboBoxWhiteBalance->setCurrentIndex(0);
        }
        m_labelExposureValue->setText("10000 us");
        QMessageBox::information(this, "提示", "已恢复默认参数");
    }
}

void camParaSetDialog::updateUIState()
{
    bool hasCamera = m_comboBoxCamera->count() > 0 &&
                     m_comboBoxCamera->currentText() != "无可用相机" &&
                     m_comboBoxCamera->currentText() != "相机管理器未初始化";

    m_groupBoxParams->setEnabled(hasCamera);
    m_pushButtonApply->setEnabled(hasCamera);
    m_pushButtonDefault->setEnabled(hasCamera);
    m_pushButtonRefresh->setEnabled(hasCamera);
}

// ===== 槽函数实现 =====

void camParaSetDialog::onOkClicked()
{
    qDebug() << "用户点击确定";
    applyCameraParameters();
    accept();
}

void camParaSetDialog::onCancelClicked()
{
    qDebug() << "用户点击取消";
    reject();
}

void camParaSetDialog::onRefreshClicked()
{
    qDebug() << "手动刷新参数";
    if (!m_currentCamera.isEmpty()) {
        checkCameraType();
        loadCameraParameters();
        QMessageBox::information(this, "提示", "参数已刷新");
    }
}

void camParaSetDialog::onApplyClicked()
{
    applyCameraParameters();
}

void camParaSetDialog::onDefaultClicked()
{
    setDefaultParameters();
}

void camParaSetDialog::onCameraChanged(int index)
{
    if (index < 0 || index >= m_comboBoxCamera->count()) {
        return;
    }

    QString displayName = m_comboBoxCamera->currentText();
    if (displayName == "无可用相机" || displayName == "相机管理器未初始化") {
        m_currentCamera.clear();
        updateUIState();
        return;
    }

    m_currentCamera = m_cameraMap.value(displayName, "");
    qDebug() << "切换到相机:" << m_currentCamera;

    if (!m_currentCamera.isEmpty()) {
        checkCameraType();
        loadCameraParameters();
        updateUIState();
    }
}

void camParaSetDialog::onExposureChanged(int value)
{
    if (!m_isLoading) {
        m_labelExposureValue->setText(QString("%1 us").arg(value));
    }
}