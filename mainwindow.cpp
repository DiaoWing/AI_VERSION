#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QTimer>
#include <QMessageBox>
#include <QPixmap>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_layoutManager(nullptr)
    , m_cameraManager(nullptr)
    , m_mainDisplayLabel(nullptr)
    , m_isMainDisplay(false)
    , m_modbusDialog(nullptr)
    , m_businessWorker(nullptr)      // 新增
    , m_businessThread(nullptr)
    , m_detectionThread(nullptr)     // 新增
    , m_masterDialog(nullptr)
    , m_detectionWorker(nullptr)
    , m_imageSavePath("")
    , m_taskCounter(0)
{
    ui->setupUi(this);
    setWindowTitle("AI 视觉检测系统 V1.0.0");
    statusBar()->setVisible(false); //灰色的状态栏，暂时用不上，暂时不添加。

    showMaximized();

    // ===== 注册自定义类型 =====
    qRegisterMetaType<DetectionTask>("DetectionTask");
    qRegisterMetaType<DetectionResult>("DetectionResult");

    // 初始化相机选中状态 (默认全部选中)
    m_cameraCheckStates["cam1"] = true;
    m_cameraCheckStates["cam2"] = true;
    m_cameraCheckStates["cam3"] = true;
    m_cameraCheckStates["cam4"] = true;

    m_cameraManager = CameraManager::instance();
    connect(m_cameraManager, &CameraManager::imageReceived,
            this, &MainWindow::updateImageDisplay);

    connect(m_cameraManager, &CameraManager::errorOccurred,
            [this](const QString& error) {
                qWarning() << "相机错误:" << error;
            });
    //手动创建连接
    connect(ui->actionmodbusserver, &QAction::triggered,
            this, &MainWindow::on_actionmodbusserver_triggered);

    connect(ui->actionclient, &QAction::triggered,
            this, &MainWindow::on_actionmodbusclient_triggered);

    connect(ui->actiondecConfigTable, &QAction::triggered,
            this, &MainWindow::on_actiondecConfigTable_triggered);

    connect(ui->actionclibration, &QAction::triggered,
            this, &MainWindow::on_actionclibration_triggered);

    connect(ui->actionbaseinfo, &QAction::triggered,
            this, &MainWindow::on_actionbaseinfo_triggered);

    connect(ui->actionUserManual, &QAction::triggered,
            this, &MainWindow::on_actionUserManual_triggered);

    m_layoutManager = new LayoutManager(centralWidget(), this);

    // 注册所有控件
    m_layoutManager->registerWidget(ui->checkBox_cam1, "cam1");
    m_layoutManager->registerWidget(ui->checkBox_cam2, "cam2");
    m_layoutManager->registerWidget(ui->checkBox_cam3, "cam3");
    m_layoutManager->registerWidget(ui->checkBox_cam4, "cam4");
    m_layoutManager->registerWidget(ui->label_show1, "show1");
    m_layoutManager->registerWidget(ui->label_show2, "show2");
    m_layoutManager->registerWidget(ui->label_show3, "show3");
    m_layoutManager->registerWidget(ui->label_show4, "show4");
    m_layoutManager->registerWidget(ui->label_show_main, "show_main");
    m_layoutManager->registerWidget(ui->camParaSet, "相机设置");
    m_layoutManager->registerWidget(ui->getImage, "采集图像");
    m_layoutManager->registerWidget(ui->DecResult, "检测结果");
    m_layoutManager->registerWidget(ui->Refresh, "刷新");

    // 连接 checkbox 状态变化信号
    connect(ui->checkBox_cam1, &QCheckBox::stateChanged, this, &MainWindow::on_checkBox_cam1_stateChanged);
    connect(ui->checkBox_cam2, &QCheckBox::stateChanged, this, &MainWindow::on_checkBox_cam2_stateChanged);
    connect(ui->checkBox_cam3, &QCheckBox::stateChanged, this, &MainWindow::on_checkBox_cam3_stateChanged);
    connect(ui->checkBox_cam4, &QCheckBox::stateChanged, this, &MainWindow::on_checkBox_cam4_stateChanged);

    setupImageDisplays();
    initLayouts();
    m_layoutManager->switchToMode(LayoutManager::Mode_Control);

    //此处直接创建可能是存在问题的
    m_businessThread = new QThread(this);
    m_businessWorker = new BusinessWorker();
    m_businessWorker->moveToThread(m_businessThread);

    connect(m_businessThread, &QThread::started, m_businessWorker, &BusinessWorker::start);
    connect(m_businessWorker, &BusinessWorker::taskProcessed, this, &MainWindow::onTaskProcessed);
    connect(m_businessWorker, &BusinessWorker::debugInfo, this, [this](const QString& info) {
        qDebug() << "[Business]" << info;
    });

    bool connected = connect(m_businessWorker, &BusinessWorker::triggerCapture,
                             this, &MainWindow::onTriggerCapture);

    m_businessThread->start();
    setupDetectionWorker();         //初始化检测线程

    // ===== modbusslave 连接信号到检测线程 =====
    connect(this, &MainWindow::requestDetection,
            m_detectionWorker, &DetectionWorker::addTask);

    setupFileCleaner();
    registerCoastlineParams();   //注册检测项目需要的参数！
}

MainWindow::~MainWindow()
{
    // 清理 Modbus 对话框
    if (m_modbusDialog) {
        delete m_modbusDialog;
        m_modbusDialog = nullptr;
    }

    // ===== 清理检测线程 =====
    if (m_detectionWorker) {
        m_detectionWorker->stop();
    }
    if (m_detectionThread) {
        m_detectionThread->quit();
        m_detectionThread->wait();
        delete m_detectionThread;
        m_detectionThread = nullptr;
    }

    if (m_businessWorker) {
        m_businessWorker->stop();
    }
    if (m_businessThread) {
        m_businessThread->quit();
        m_businessThread->wait();
        delete m_businessThread;
    }

    cleanupAndExit();
    delete ui;
}

void MainWindow::setupImageDisplays()
{
    m_displayLabels["cam1"] = ui->label_show1;
    m_displayLabels["cam2"] = ui->label_show2;
    m_displayLabels["cam3"] = ui->label_show3;
    m_displayLabels["cam4"] = ui->label_show4;

    m_mainDisplayLabel = ui->label_show_main;

    // 小窗口 - 灰色
    for (auto it = m_displayLabels.begin(); it != m_displayLabels.end(); ++it) {
        it.value()->installEventFilter(this);
        it.value()->setProperty("cameraName", it.key());
        //it.value()->setScaledContents(true);
        it.value()->setAlignment(Qt::AlignCenter);
        it.value()->setStyleSheet("border: 2px solid #999999; background-color: #e0e0e0;");
        it.value()->setText("等待图像...");
    }

    // 主窗口 - 灰色
    m_mainDisplayLabel->installEventFilter(this);
    m_mainDisplayLabel->setScaledContents(false);
    m_mainDisplayLabel->setAlignment(Qt::AlignCenter);
    m_mainDisplayLabel->setStyleSheet("border: 3px solid #666666; background-color: #d0d0d0;");
    m_mainDisplayLabel->setText("主显示区域\n双击可切换");
}

void MainWindow::initLayouts()
{
    LayoutManager::LayoutDefinition controlLayout;
    controlLayout.mode = LayoutManager::Mode_Control;
    controlLayout.description = "控制模式";

    LayoutManager::WidgetPosition pos;

    // cam1 (第1行)
    pos.widgetName = "cam1";
    pos.geometry = QRect(3, 1, 10, 3);
    pos.visible = true;
    controlLayout.positions.append(pos);

    // show1 (第1行显示)
    pos.widgetName = "show1";
    pos.geometry = QRect(3, 4, 10, 20);  // 高度22
    pos.visible = true;
    controlLayout.positions.append(pos);

    // cam2 (第2行)
    pos.widgetName = "cam2";
    pos.geometry = QRect(3, 25, 10, 3);
    pos.visible = true;
    controlLayout.positions.append(pos);

    // show2 (第2行显示)
    pos.widgetName = "show2";
    pos.geometry = QRect(3, 28, 10, 20);
    pos.visible = true;
    controlLayout.positions.append(pos);

    // cam3 (第3行)
    pos.widgetName = "cam3";
    pos.geometry = QRect(3, 49, 10, 3);
    pos.visible = true;
    controlLayout.positions.append(pos);

    // show3 (第3行显示)
    pos.widgetName = "show3";
    pos.geometry = QRect(3, 52, 10, 20);
    pos.visible = true;
    controlLayout.positions.append(pos);

    // cam4 (第4行)
    pos.widgetName = "cam4";
    pos.geometry = QRect(3, 73, 10, 3);
    pos.visible = true;
    controlLayout.positions.append(pos);

    // show4 (第4行显示)
    pos.widgetName = "show4";
    pos.geometry = QRect(3, 76, 10, 20);
    pos.visible = true;
    controlLayout.positions.append(pos);

    // ============================================================
    // 主显示区域
    // ============================================================
    pos.widgetName = "show_main";
    pos.geometry = QRect(17, 0, 65, 100);
    pos.visible = true;
    controlLayout.positions.append(pos);

    // 主显示
    pos.widgetName = "show_main";
    pos.geometry = QRect(17, 0, 65, 100);
    pos.visible = true;
    controlLayout.positions.append(pos);

    // 右侧按钮
    pos.widgetName = "相机设置";
    pos.geometry = QRect(83, 65, 10, 5);
    pos.visible = true;
    controlLayout.positions.append(pos);

    pos.widgetName = "刷新";
    pos.geometry = QRect(90, 65, 10, 5);
    pos.visible = true;
    controlLayout.positions.append(pos);

    pos.widgetName = "采集图像";
    pos.geometry = QRect(83, 77, 10, 5);
    pos.visible = true;
    controlLayout.positions.append(pos);

    pos.widgetName = "检测结果";
    pos.geometry = QRect(90, 77, 10, 5);
    pos.visible = true;
    controlLayout.positions.append(pos);

    m_layoutManager->addLayout(LayoutManager::Mode_Control, controlLayout);
    qDebug() << "Control layout added";

    //UI控件
    ui->camParaSet->setIcon(QIcon(":/icons/resources/icons/settings.png"));
    ui->camParaSet->setIconSize(QSize(72, 72));  // ✅ 图标放大到 72x72
    ui->camParaSet->setFixedSize(100, 100);      // ✅ 按钮也相应放大
    ui->camParaSet->setFlat(true);
    ui->camParaSet->setText("");
    ui->camParaSet->installEventFilter(this);
    ui->camParaSet->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    border-radius: 50px;"       // 圆形（按钮大小的一半）
        "    padding: 0px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(74, 138, 244, 0.3);"
        "    border: 2px solid #4a8af4;"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgba(74, 138, 244, 0.6);"
        "}"
        );

    ui->Refresh->setIcon(QIcon(":/icons/resources/icons/refresh.png"));
    ui->Refresh->setIconSize(QSize(72, 72));
    ui->Refresh->setFixedSize(100, 100);
    ui->Refresh->setFlat(true);
    ui->Refresh->setText("");  // ✅ 清空文字
    ui->Refresh->installEventFilter(this);
    ui->Refresh->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    border-radius: 40px;"
        "    padding: 0px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(74, 138, 244, 0.3);"
        "    border: 2px solid #4a8af4;"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgba(74, 138, 244, 0.6);"
        "}"
        );

    ui->getImage->setIcon(QIcon(":/icons/resources/icons/getiamge.png"));
    ui->getImage->setIconSize(QSize(72, 72));
    ui->getImage->setFixedSize(100, 100);
    ui->getImage->setFlat(true);
    ui->getImage->setText("");  // ✅ 清空文字
    ui->getImage->installEventFilter(this);
    ui->getImage->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    border-radius: 40px;"
        "    padding: 0px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(74, 138, 244, 0.3);"
        "    border: 2px solid #4a8af4;"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgba(74, 138, 244, 0.6);"
        "}"
        );

    ui->DecResult->setIcon(QIcon(":/icons/resources/icons/decresult.png"));
    ui->DecResult->setIconSize(QSize(72, 72));
    ui->DecResult->setFixedSize(100, 100);
    ui->DecResult->setFlat(true);
    ui->DecResult->setText("");  // ✅ 清空文字
    ui->DecResult->installEventFilter(this);
    ui->DecResult->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    border-radius: 40px;"
        "    padding: 0px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(74, 138, 244, 0.3);"
        "    border: 2px solid #4a8af4;"
        "}"
        "QPushButton:pressed {"
        "    background-color: rgba(74, 138, 244, 0.6);"
        "}"
        );

}




void MainWindow::onShowModbusDialog()
{
    // 显示对话框（非模态）
    m_modbusDialog->showDialog();
}

void MainWindow::on_Refresh_clicked()
{
    qDebug() << "=== 刷新按钮被点击 ===";

    // ui->Refresh->setEnabled(false);
    // ui->Refresh->setText("刷新中...");

    for (int i = 1; i <= MAX_DEVICE_NUM; i++) {
        QString name = QString("cam%1").arg(i);
        qDebug() << name << "shouldCameraBeActive:" << shouldCameraBeActive(name)
                 << "m_cameraCheckStates:" << m_cameraCheckStates.value(name, false);
    }

    // 先停止所有采集
    m_cameraManager->stopAllCameras();

    // 关闭所有相机
    for (int i = 1; i <= CameraManager::deviceList.size(); i++) {
        QString name = QString("cam%1").arg(i);
        CameraInfo* info = m_cameraManager->getCamera(name);
        if (info && info->isOpen) {
            m_cameraManager->closeCamera(info);
        }
    }

    // 枚举并打开相机
    bool result = m_cameraManager->enumAndOpenCameras();

    if (result) {
        qDebug() << "刷新成功";

        for (int i = 1; i <= CameraManager::deviceList.size(); i++) {
            QString name = QString("cam%1").arg(i);
            bool shouldActive = shouldCameraBeActive(name);

            if (shouldActive) {
                m_cameraManager->startCamera(name);
                qDebug() << name << "已激活(选中状态)";
            } else {
                CameraInfo* info = m_cameraManager->getCamera(name);
                if (info && info->isOpen) {
                    m_cameraManager->closeCamera(info);
                }
                qDebug() << name << "已关闭(未选中)";
            }
        }

        //此时才会去调用更新对应的状态！
        updateUIStatus();

        // 设置主显示（优先显示第一个激活的相机）
        bool mainSet = false;
        for (int i = 1; i <= CameraManager::deviceList.size(); i++) {
            QString name = QString("cam%1").arg(i);
            if (shouldCameraBeActive(name) && m_cameraManager->isCameraGrabbing(name)) {
                setMainDisplay(name);
                mainSet = true;
                break;
            }
        }

        if (!mainSet) {
            m_mainDisplayLabel->clear();
            m_mainDisplayLabel->setText("无相机\n请检查连接");
            m_currentMainCamera.clear();
            m_isMainDisplay = false;
        }

    } else {
        qDebug() << "刷新失败";
        QMessageBox::warning(this, "提示", "相机刷新失败，请检查相机连接");
    }

    // ui->Refresh->setEnabled(true);
    // ui->Refresh->setText("刷新");
}

void MainWindow::updateCameraStatus(const QString& name, bool isOpen, bool isGrabbing)
{
    QCheckBox* checkBox = nullptr;
    if (name == "cam1") checkBox = ui->checkBox_cam1;
    else if (name == "cam2") checkBox = ui->checkBox_cam2;
    else if (name == "cam3") checkBox = ui->checkBox_cam3;
    else if (name == "cam4") checkBox = ui->checkBox_cam4;

    if (checkBox) {
        checkBox->setChecked(isOpen);   //此处其实会对其进行更新，然后选中操作！

        QString status = isOpen ? (isGrabbing ? " ✓ 采集" : " ○ 已开") : " ✗ 关闭";
        checkBox->setText(QString("%1%2").arg(name).arg(status));

        if (isOpen && isGrabbing) {
            checkBox->setStyleSheet("color: green; font-weight: bold;");
        } else if (isOpen) {
            checkBox->setStyleSheet("color: orange; font-weight: bold;");
        } else {
            checkBox->setStyleSheet("color: red;");
        }

        // bool userSelected = shouldCameraBeActive(name);
        // if (userSelected && !isOpen) {
        //     checkBox->setText(QString("%1 ✗ 未连接").arg(name));
        // }
    }
}

//很有可能是被点击之后，此处又重新的将其显示放大了
void MainWindow::updateImageDisplay(const QString& name, QImage image)
{
    if (image.isNull()) return;

    // 当前是放大模式，并且这帧是放大相机的数据
    if (m_isMainDisplay && m_zoomedCamera == name) {
        // 更新主显示窗口
        showImageOnLabel(m_mainDisplayLabel, image);
        // ❗不要return，其他相机还要继续走下面逻辑更新小窗口
    }

    // 如果不是放大相机，或者已经退出放大模式：更新小窗口
    if (m_displayLabels.contains(name))
    {
        // 如果当前相机正好是被放大的那个，小窗口不刷图像，保留“已放大”文字
        if(m_isMainDisplay && m_zoomedCamera == name)
        {
            // do nothing，小窗口维持文字“已放大”
        }
        else
        {
            showImageOnLabel(m_displayLabels[name], image);
        }
    }

    // 如果没有主显示，且相机激活，设置为主显示
    // if (m_currentMainCamera.isEmpty() && shouldCameraBeActive(name)) {
    //     setMainDisplay(name);
    // }
}

void MainWindow::showImageOnLabel(QLabel* label, const QImage& image)
{
    if (!label || image.isNull()) return;

    QPixmap pixmap = QPixmap::fromImage(image);
    QSize labelSize = label->size();

    // 确保标签有有效大小
    if (labelSize.width() <= 0 || labelSize.height() <= 0) {
        label->setPixmap(pixmap);
        return;
    }

    // 按比例缩放，保持宽高比
    QPixmap scaled = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    label->setPixmap(scaled);
    label->setAlignment(Qt::AlignCenter);  // 居中显示
}

void MainWindow::setMainDisplay(const QString& cameraName)
{
    m_currentMainCamera = cameraName;
    m_isMainDisplay = true;
    m_zoomedCamera = cameraName;

    // 显示在主窗口
    CameraInfo* info = m_cameraManager->getCamera(cameraName);
    if (info && !info->lastImage.isNull()) {
        showImageOnLabel(m_mainDisplayLabel, info->lastImage);
    } else {
        m_mainDisplayLabel->setText("等待图像...");
    }

    // 被放大的小窗口显示"已放大"标记
    if (m_displayLabels.contains(cameraName)) {
        m_displayLabels[cameraName]->clear();
        m_displayLabels[cameraName]->setText("已放大");
        m_displayLabels[cameraName]->setScaledContents(false);
        m_displayLabels[cameraName]->setStyleSheet("border: 2px solid #00ff00; background-color: #1a1a1a; color: #00ff00; font-weight: bold;");
    }

    qDebug() << "主显示切换到:" << cameraName;
}

void MainWindow::restoreSmallDisplay()
{
    m_isMainDisplay = false;
    m_zoomedCamera.clear();
    m_currentMainCamera.clear();

    for (auto it = m_displayLabels.begin(); it != m_displayLabels.end(); ++it) {
        QString name = it.key();
        QLabel* label = it.value();

        label->clear();
        label->setStyleSheet("border: 2px solid #999999; background-color: #e0e0e0;");
        label->setScaledContents(false);
        label->setAlignment(Qt::AlignCenter);

        CameraInfo* info = m_cameraManager->getCamera(name);
        if (info && !info->lastImage.isNull()) {
            showImageOnLabel(label, info->lastImage);
        } else {
            label->setText("等待图像...");
        }
    }

    m_mainDisplayLabel->clear();
    m_mainDisplayLabel->setScaledContents(false);  // 改为 false
    m_mainDisplayLabel->setText("主显示区域\n双击可切换");
    m_mainDisplayLabel->setStyleSheet("border: 3px solid #666666; background-color: #d0d0d0;");

    qDebug() << "已恢复所有小窗口显示";
}


bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    // ===== 处理所有按钮的悬停事件 =====
    if (obj == ui->camParaSet || obj == ui->Refresh ||
        obj == ui->getImage || obj == ui->DecResult) {

        QPushButton* btn = qobject_cast<QPushButton*>(obj);
        if (!btn) return QMainWindow::eventFilter(obj, event);

        // 确定按钮显示的文字
        QString btnText;
        if (obj == ui->camParaSet) btnText = "相机设置";
        else if (obj == ui->Refresh) btnText = "刷新";
        else if (obj == ui->getImage) btnText = "采集图像";
        else if (obj == ui->DecResult) btnText = "检测结果";

        if (event->type() == QEvent::Enter) {
            // 鼠标进入：显示文字
            btn->setText(btnText);
            btn->setStyleSheet(
                "QPushButton {"
                "    background-color: transparent;"
                "    border: none;"
                "    border-radius: 40px;"
                "    padding: 0px;"
                "    color: #ff0000;"           // 红色文字
                "    font-size: 13px;"
                "    font-weight: bold;"
                "}"
                "QPushButton:hover {"
                "    background-color: rgba(74, 138, 244, 0.3);"
                "    border: 2px solid #4a8af4;"
                "}"
                "QPushButton:pressed {"
                "    background-color: rgba(74, 138, 244, 0.6);"
                "}"
                );
            return true;
        } else if (event->type() == QEvent::Leave) {
            // 鼠标离开：清空文字，恢复原样
            btn->setText("");
            btn->setStyleSheet(
                "QPushButton {"
                "    background-color: transparent;"
                "    border: none;"
                "    border-radius: 40px;"
                "    padding: 0px;"
                "}"
                "QPushButton:hover {"
                "    background-color: rgba(74, 138, 244, 0.3);"
                "    border: 2px solid #4a8af4;"
                "}"
                "QPushButton:pressed {"
                "    background-color: rgba(74, 138, 244, 0.6);"
                "}"
                );
            return true;
        }
    }

    // ===== 原有的双击事件处理 =====
    if (event->type() == QEvent::MouseButtonDblClick) {
        QLabel* label = qobject_cast<QLabel*>(obj);
        if (label) {
            QString cameraName = label->property("cameraName").toString();

            if (m_displayLabels.values().contains(label) && !cameraName.isEmpty()) {
                if (!shouldCameraBeActive(cameraName)) {
                    return true;
                }
                if (m_isMainDisplay && m_zoomedCamera == cameraName) {
                    restoreSmallDisplay();
                } else {
                    setMainDisplay(cameraName);
                }
                return true;
            }

            if (label == m_mainDisplayLabel) {
                if (m_isMainDisplay && !m_zoomedCamera.isEmpty()) {
                    restoreSmallDisplay();
                }
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}
void MainWindow::on_checkBox_cam1_stateChanged(int state)
{
    updateCameraCheckState("cam1", state == Qt::Checked);
}

void MainWindow::on_checkBox_cam2_stateChanged(int state)
{
    updateCameraCheckState("cam2", state == Qt::Checked);
}

void MainWindow::on_checkBox_cam3_stateChanged(int state)
{
    updateCameraCheckState("cam3", state == Qt::Checked);
}

void MainWindow::on_checkBox_cam4_stateChanged(int state)
{
    updateCameraCheckState("cam4", state == Qt::Checked);
}

void MainWindow::updateCameraCheckState(const QString& name, bool checked)
{
    m_cameraCheckStates[name] = checked;
    qDebug() << name << "选中状态:" << checked;
}

bool MainWindow::shouldCameraBeActive(const QString& name)
{
    return m_cameraCheckStates.value(name, false);
}

void MainWindow::updateUIStatus()
{
    for (int i = 1; i <= MAX_DEVICE_NUM; i++) {
        QString name = QString("cam%1").arg(i);
        bool isOpen = m_cameraManager->isCameraOpen(name);
        bool isGrabbing = m_cameraManager->isCameraGrabbing(name);
        updateCameraStatus(name, isOpen, isGrabbing);
    }
}

void MainWindow::cleanupAndExit()
{
    qDebug() << "=== 开始清理退出 ===";

    if (m_cameraManager) {
        m_cameraManager->stopAllCameras();
        qDebug() << "停止所有采集";

        for (int i = 1; i <= 4; i++) {
            QString name = QString("cam%1").arg(i);
            CameraInfo* info = m_cameraManager->getCamera(name);
            if (info && info->isOpen) {
                info->camera->close();
                info->isOpen = false;
                info->isGrabbing = false;
                qDebug() << name << "已关闭";
            }
        }
    }

    MvCameraQt::finalizeSDK();
    qDebug() << "SDK已反初始化";
    qDebug() << "=== 清理完成 ===";
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << "窗口关闭事件触发";
    cleanupAndExit();
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (m_layoutManager) {
        m_layoutManager->onContainerResized();
    }
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    QTimer::singleShot(100, this, [this]() {
        if (m_layoutManager) {
            m_layoutManager->onContainerResized();
        }
    });
}

void MainWindow::on_getImage_clicked()
{

    qDebug() << "=== 采集图像按钮被点击 ===";

    // 1. 检查是否有相机在采集
    bool hasGrabbing = false;
    for (int i = 1; i <= 4; i++) {
        QString name = QString("cam%1").arg(i);
        if (m_cameraManager->isCameraGrabbing(name)) {
            hasGrabbing = true;
            break;
        }
    }

    if (!hasGrabbing) {
        QMessageBox::warning(this, "提示", "没有相机在采集图像，请先打开相机！");
        return;
    }

    // 2. 确定目标相机
    QString targetCamera;

    if (m_isMainDisplay && !m_zoomedCamera.isEmpty()) {
        targetCamera = m_zoomedCamera;
        qDebug() << "采集放大的相机:" << targetCamera;
    } else {
        for (int i = 1; i <= 4; i++) {
            QString name = QString("cam%1").arg(i);
            if (shouldCameraBeActive(name) && m_cameraManager->isCameraGrabbing(name)) {
                targetCamera = name;
                qDebug() << "采集第一个激活的相机:" << targetCamera;
                break;
            }
        }
    }

    if (targetCamera.isEmpty()) {
        QMessageBox::warning(this, "提示", "没有找到可用的相机图像！");
        return;
    }

    // 3. 获取图像
    CameraInfo* info = m_cameraManager->getCamera(targetCamera);
    if (!info || info->lastImage.isNull()) {
        QMessageBox::warning(this, "提示", "相机图像为空，请等待图像采集！");
        return;
    }

    // 4. 创建目录
    QString saveDir = QCoreApplication::applicationDirPath() + "/captureImage";
    QDir dir;
    if (!dir.exists(saveDir)) {
        if (!dir.mkpath(saveDir)) {
            QMessageBox::critical(this, "错误", "无法创建保存目录！");
            return;
        }
        qDebug() << "创建目录:" << saveDir;
    }

    // 5. 生成文件名并保存
    QString fileName = generateImageFileName();
    QString filePath = saveDir + "/" + fileName;

    bool success = saveImageToFile(info->lastImage, filePath);

    if (success) {
        qDebug() << "图像保存成功:" << filePath;
        QMessageBox::information(this, "成功",
                                 QString("图像已保存到:\n%1").arg(filePath));
    } else {
        QMessageBox::critical(this, "错误", "图像保存失败！");
    }
}

QString MainWindow::generateImageFileName()
{
    QDateTime now = QDateTime::currentDateTime();
    QString dateTime = now.toString("yyyy-MM-dd_hh-mm-ss");
    QString ms = QString::number(now.time().msec(), 10).rightJustified(3, '0');

    QString cameraName = "unknown";
    if (m_isMainDisplay && !m_zoomedCamera.isEmpty()) {
        cameraName = m_zoomedCamera;
    } else {
        for (int i = 1; i <= 4; i++) {
            QString name = QString("cam%1").arg(i);
            if (shouldCameraBeActive(name) && m_cameraManager->isCameraGrabbing(name)) {
                cameraName = name;
                break;
            }
        }
    }

    return QString("%1_%2_%3.jpg").arg(cameraName).arg(dateTime).arg(ms);
}

bool MainWindow::saveImageToFile(const QImage& image, const QString& filePath)
{
    if (image.isNull()) {
        qWarning() << "图像为空，无法保存";
        return false;
    }

    bool success = image.save(filePath, "JPG", 95);

    if (success) {
        qDebug() << "图像已保存，大小:" << image.width() << "x" << image.height();
    } else {
        qWarning() << "图像保存失败:" << filePath;
    }

    return success;
}

//创建非模态对话框才可以，传递当前放大的相机是那个，然后对其参数进行修改
void MainWindow::on_camParaSet_clicked()
{
    if (!m_camParaSetDialog) {
        m_camParaSetDialog = new camParaSetDialog(nullptr);
        m_camParaSetDialog->setAttribute(Qt::WA_DeleteOnClose);
        m_camParaSetDialog->setModal(false);

        connect(m_camParaSetDialog, &QDialog::destroyed, this, [this]() {
            m_camParaSetDialog = nullptr;
        });
    }

    QString targetCamera;
    if (m_isMainDisplay && !m_zoomedCamera.isEmpty()) {
        targetCamera = m_zoomedCamera;
        qDebug() << "使用放大的相机:" << targetCamera;
    } else {
        for (int i = 1; i <= 4; i++) {
            QString name = QString("cam%1").arg(i);
            if (shouldCameraBeActive(name) && m_cameraManager->isCameraGrabbing(name)) {
                targetCamera = name;
                qDebug() << "使用第一个激活的相机:" << targetCamera;
                break;
            }
        }
    }

    if (!targetCamera.isEmpty()) {
        m_camParaSetDialog->setCurrentCamera(targetCamera);
    }

    m_camParaSetDialog->show();
    m_camParaSetDialog->raise();
    m_camParaSetDialog->activateWindow();
}

//此处接受信号是否发生了改变，然后执行后续的操作
void MainWindow::onRegisterChanged(quint16 address, quint16 value)
{
    //此处出发对应的操作，接收到检测信号，需要实现检测！
    if (address == 0) {
        // 0号寄存器变化，入队
        ModbusMessage msg(0, address, value);
        MessageQueue::instance()->enqueue(msg);    //队列
        qDebug() << "0号寄存器变化，已入队: 值=0x" << QString::number(value, 16);
    }
}

void MainWindow::onTaskProcessed(const ModbusMessage& msg)
{
    qDebug() << "任务处理完成: 任务ID=" << msg.taskId
             << ", 项目ID=" << msg.projectId;
    // 这里可以更新UI
}

//初始化时，调用单例对数据进行导入，连接就暂时算了
void MainWindow::on_actionmodbusserver_triggered()
{
    qDebug() << "Modbus 服务器菜单被点击";

    // 如果对话框不存在，创建它
    if (!m_modbusDialog) {
        //此时创建对话框之后
        m_modbusDialog = new modbusSlaveDialog(this);

        // ===== 连接对话框的 started 信号，在 worker 创建后连接 =====
        connect(m_modbusDialog, &modbusSlaveDialog::slaveStarted,
                this, [this]() {
                    if (m_modbusDialog && m_modbusDialog->m_worker) {
                        connect(m_modbusDialog->m_worker,
                                &ModbusSlaveWorker::registerChanged,
                                this, &MainWindow::onRegisterChanged);
                        qDebug() << "✅ registerChanged 信号已连接！";
                    }
                });

        // 对话框关闭时清空指针
        connect(m_modbusDialog, &modbusSlaveDialog::destroyed, [this]() {
            m_modbusDialog = nullptr;
        });
    }

    // 显示对话框（使用自定义的 showDialog 方法，保持连接状态）
    if(m_modbusDialog != nullptr)
    {
        m_modbusDialog->showDialog();
    }
    else
    {
        return;
    }
}

void MainWindow::on_actionmodbusclient_triggered()
{
    if (!m_masterDialog) {
        m_masterDialog = new ModbusMasterDialog(this);
    }
    m_masterDialog->showDialog();
}

void MainWindow::on_actionclibration_triggered()
{
    qDebug() << "=== 标定菜单被点击 ===";

    // 创建并显示对话框（模态）
    CalibrationDialog dialog(this);
    dialog.exec();
}

// 打开对应的界面时，应该时对应的配置，才对！
void MainWindow::on_DecResult_clicked()
{
    ConfigManager* cm = ConfigManager::instance();
    QString path = cm->getConfigFilePath();
    QList<DetectionConfig> configs = cm->loadDetectionConfigsFromFile(path);

    // ✅ 如果对话框存在，先销毁再创建（保证每次都是新的）
    if (m_detectionDialog) {
        // 断开所有信号连接，防止悬空指针
        disconnect(m_detectionDialog, nullptr, this, nullptr);
        m_detectionDialog->deleteLater();
        m_detectionDialog = nullptr;
        qDebug() << "[MainWindow] 旧对话框已销毁";
    }

    // 创建检测结果显示对话框（非模态）
    m_detectionDialog = new DetectionDisplayDialog(this);
    m_detectionDialog->setAttribute(Qt::WA_DeleteOnClose);  // 点击X时自动销毁
    m_detectionDialog->setWindowFlags(Qt::Window);          // 作为独立窗口

    // ✅ 连接对话框的销毁信号，将指针置空
    connect(m_detectionDialog, &QDialog::destroyed, [this]() {
        m_detectionDialog = nullptr;
        qDebug() << "[MainWindow] 检测显示对话框已销毁";
    });

    // ✅ 连接对话框的 finished 信号（点击X时会触发），确保指针置空
    connect(m_detectionDialog, &QDialog::finished, [this](int result) {
        Q_UNUSED(result);
        // 不要在这里置空，因为 destroyed 信号会处理
        // 但如果因为某些原因 destroyed 没触发，这里作为备份
        if (m_detectionDialog) {
            // 注意：不要 delete，因为 WA_DeleteOnClose 会处理
            qDebug() << "[MainWindow] 对话框已关闭 (finished)";
        }
    });

    //单独的进行参数的注册

    // 设置配置并显示对话框
    m_detectionDialog->setDetectionConfigs(configs);
    m_detectionDialog->show();  // 非模态显示

    qDebug() << "[MainWindow] 检测对话框已创建并显示，配置数量:" << configs.size();
}


void MainWindow::registerCoastlineParams()
{
    ParamManager* pm = ParamManager::instance();

    // 如果已经注册，跳过
    if (pm->hasProject("海岸线检测")) {
        qDebug() << "[MainWindow] 海岸线检测参数已注册，跳过";
        return;
    }

    QList<ParamDefinition> params;

    // ✅ 添加海岸线类型参数（QString类型）
    ParamDefinition p1;
    p1.name = "coastlineType";           // 内部标识名
    p1.displayName = "海岸线类型";        // 界面显示名称
    p1.type = "enum";                    // 使用 enum 类型来实现选项
    p1.defaultValue = "v1";              // 默认值
    p1.enumValues = {"v1", "v2"};        // 可选值列表
    params.append(p1);

    // 可以继续添加其他参数
    // ParamDefinition p2;
    // p2.name = "threshold";
    // p2.displayName = "检测阈值";
    // p2.type = "int";
    // p2.defaultValue = 128;
    // p2.minValue = 0;
    // p2.maxValue = 255;
    // params.append(p2);

    ProjectParams project;
    project.projectName = "海岸线检测";
    project.paramDefs = params;
    pm->registerProject(project);

    qDebug() << "✅ 注册海岸线检测参数，共" << params.size() << "个";
}

int MainWindow::getCameraIdFromConfigs(const QList<DetectionConfig>& configs, int groupId)
{
    for (const DetectionConfig& config : configs) {
        if (config.groupId == groupId) {
            qDebug() << "[getCameraIdFromConfigs] 找到配置: groupId=" << groupId
                     << " cameraId=" << config.cameraId;
            return config.cameraId;
        }
    }

    qDebug() << "[getCameraIdFromConfigs] 未找到 groupId=" << groupId << " 的配置";
    return -1;  // 返回 -1 表示未找到
}


void MainWindow::onTriggerCapture(int groupId, const ModbusMessage& msg)
{
    //没有执行，能够执行的前置条件，应该是存在ID 和配置表中是存在能够启动的 一致才行
    //此处去获取配置表中的东西
    qDebug() << "=== 触发拍照，项目ID:" << groupId << "===";

    // 保存当前任务
    m_currentProjectId = groupId; //群组ID可以这样传递呀，但是只能支持一个检测项目
    m_currentTask = msg;

    //此处需要调用获取配置表中，然后得到对应的相机是几号，然后去触发对应的相机
    ConfigManager* cm = ConfigManager::instance();
    QString path = cm->getConfigFilePath();
    QList<DetectionConfig> configs = cm->loadDetectionConfigsFromFile(path);

    //从群组ID获取当前的相机的序号
    int camId = getCameraIdFromConfigs(configs,groupId);

    //获取当前的检测项目名字,暂时没有考虑一个群组ID对应多个检测项目！
    QString projectName = getProjectNameById(groupId);

    // ===== 根据项目ID选择相机 =====
    QString targetCamera;
    switch(camId) {
    case 1:
        targetCamera = "cam1";
        break;
    case 2:
        targetCamera = "cam2";
        break;
    case 3:
        targetCamera = "cam3";
        break;
    case 4:
        targetCamera = "cam4";
        break;
    default:
        qDebug() << "未知项目ID:" << groupId;
        return;
    }

    // 检查相机是否在采集，相机没启动还有去自己启动，简直了
    if (!m_cameraManager->isCameraGrabbing(targetCamera)) {
        qDebug() << "相机" << targetCamera << "相机未启动，结束检测";

        // m_cameraManager->startCamera(targetCamera);
        // // 等待一下让相机启动
        // QTimer::singleShot(500, this, [this, targetCamera]() {
        //     captureImageFromCamera(targetCamera);
        // });

        QMessageBox::warning(this, "警告", "相机未打开，请先打开相机！");
        return;
    } else {
        captureImageFromCamera(targetCamera, projectName);  //传递name，用于标定！
    }

}

bool  getCalibretionFromTable(QList<DetectionConfig> table, QString projectName)
{
    // 遍历配置表，查找对应项目
    for (const DetectionConfig& config : table) {
        if (config.projectName == projectName) {
            return config.calibrated;
        }
    }

    // 如果找不到该项目，默认返回false（不需要标定）
    qDebug() << "[Config] 未找到项目:" << projectName << "，默认不需要标定";
    return false;
}

void MainWindow::captureImageFromCamera(const QString& cameraName ,QString projectName)
{
    //传递配置表

    qDebug() << "从相机获取图像:" << cameraName;

    // 获取相机最新图像
    CameraInfo* info = m_cameraManager->getCamera(cameraName);
    if (!info) {
        qDebug() << "相机不存在:" << cameraName;
        return;
    }

    //开始检测时，将相机设置为触发模式，此处进行依次软触发，然后在去取图，保证时最新帧

    // 检查是否有图像
    if (info->lastImage.isNull()) {
        qDebug() << "相机图像为空，等待下一帧...";
        // 等待图像回调
        // 如果图像为空，等100ms再试一次
        QTimer::singleShot(100, this, [this, cameraName]() {
            CameraInfo* info2 = m_cameraManager->getCamera(cameraName);
            if (info2 && !info2->lastImage.isNull()) {
                onImageCapturedForProject(cameraName, info2->lastImage);
            } else {
                qDebug() << "获取图像超时";
            }
        });
        //取图失败，生成一张较大的黑色图像，其中现在是生成的黑色图像即可！


        return;
    }

    ConfigManager* cm = ConfigManager::instance();
    QString path = cm->getConfigFilePath();
    //还需要传递当前检测项目是否需要标定给，如果不需要，直接进行下一步！

    //此处需要通过配置表来判断当前项目是否需要标定，来对其进行标定校正,通过单例来调用！
    CalibrationResult relativeR = cm->LoadCalibrationResultFromjsonFile(projectName);

    QList<DetectionConfig> table = cm->loadDetectionConfigsFromFile(path);

    bool iscalib = getCalibretionFromTable(table,projectName);

    if(iscalib)
    {
        // 有图像，直接处理
        cv::Mat cvImage = qImageToCvMat(info->lastImage);
        cv::Mat correctedMat;

        // 执行校正
        if (undistortImage(cvImage, relativeR, correctedMat)) {
            info->lastImage = cvMatToQImage(correctedMat);
            qDebug() << "[DetectionDisplay] ✅ 图像校正成功";
        } else {
            qDebug() << "[DetectionDisplay] ❌ 图像校正失败，使用原始图像";
            //info->lastImage = image;  // 使用原始图像
            return;
        }
    }

    onImageCapturedForProject(cameraName, info->lastImage);
}


void MainWindow::onImageCapturedForProject(const QString& cameraName, QImage image)
{

    if (image.isNull()) {
        qDebug() << "图像为空";
        return;
    }

    // ===== 1. 显示图像到对应窗口 =====
    if (m_displayLabels.contains(cameraName)) {
        showImageOnLabel(m_displayLabels[cameraName], image);
    }

    // ===== 2. 保存图像 =====
    QString saveDir = QCoreApplication::applicationDirPath() + "/captureImage";
    QDir dir;
    if (!dir.exists(saveDir)) {
        dir.mkpath(saveDir);
    }

    QString fileName = QString("%1_project%2_%3.jpg")
                           .arg(cameraName)
                           .arg(m_currentProjectId)
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"));
    QString filePath = saveDir + "/" + fileName;
    image.save(filePath, "JPG", 95);
    qDebug() << "图像已保存:" << filePath;

    DetectionTask task;
    task.groupId = m_currentProjectId;   //前面已经将群组ID赋值给了他
    task.image = image;
    task.projectName = getProjectNameById(m_currentProjectId) ;

    // ===== 使用信号提交任务（替代 invokeMethod） =====
    emit requestDetection(task);
    qDebug() << QString("检测任务已提交: ID=%1, 项目=%2").arg(task.groupId).arg(task.projectName);
}

void MainWindow::onDetectionFinished(const DetectionResult& result)
{
    qDebug() << "=== 检测完成 ===";
    qDebug() << "任务ID:" << result.groupId;
    qDebug() << "成功:" << result.success;

    if (!result.success) {
        qDebug() << "检测失败:" << result.errorMessage;
        return;
    }

    // ===== 1. 显示到主窗口 =====
    if (!result.resultImage.isNull()) {
        showImageOnLabel(m_mainDisplayLabel, result.resultImage);
    }

    // ===== 2. 如果检测结果显示对话框已打开，也显示到那里 =====
    if (m_detectionDialog && !result.resultImage.isNull()) {
        // 获取项目名称
        QString projectName = "缺陷检测"; //getProjectNameById(result.projectId);
        if (!projectName.isEmpty()) {
            QPixmap pixmap = QPixmap::fromImage(result.resultImage);
            m_detectionDialog->updateImageDisplay(projectName, pixmap);
        }
    }

    // ===== 3. 保存检测结果 =====
    if (!result.resultImage.isNull()) {
        QString saveDir = QCoreApplication::applicationDirPath() + "/detectResult";
        QDir dir;
        if (!dir.exists(saveDir)) {
            dir.mkpath(saveDir);
        }

        QString fileName = QString("_%2.jpg")
                               .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"));
        QString filePath = saveDir + "/" + fileName;
        result.resultImage.save(filePath, "JPG", 95);
        qDebug() << "检测结果已保存:" << filePath;
    }
}

// ===== 新增：初始化检测线程 =====
void MainWindow::setupDetectionWorker()
{
    m_detectionThread = new QThread(this);
    m_detectionWorker = new DetectionWorker();
    m_detectionWorker->moveToThread(m_detectionThread);

    // 连接信号
    connect(m_detectionThread, &QThread::started, [this]() {
        qDebug() << "[MainWindow] 检测线程已启动";
    });

    connect(m_detectionWorker, &DetectionWorker::detectionFinished,
            this, &MainWindow::onDetectionFinished);
    connect(m_detectionWorker, &DetectionWorker::debugInfo,
            this, &MainWindow::onDetectionDebugInfo);

    connect(m_detectionWorker, &DetectionWorker::detectionFinished,
            this, &MainWindow::onDetectionFinished);

    // ===== 转发信号到检测对话框 =====
    connect(m_detectionWorker, &DetectionWorker::detectionFinished,
            this, &MainWindow::forwardDetectionResult);

    // 启动线程
    m_detectionThread->start();
}

void MainWindow::onDetectionDebugInfo(const QString& info)
{
    qDebug() << "[DetectionWorker]" << info;
}

QString MainWindow::getProjectNameById(int projectId)
{
    for (const DetectionConfig& config : m_detectionConfigs) {
        if (config.groupId == projectId) {
            return config.projectName;
        }
    }
    return QString();
}

void MainWindow::forwardDetectionResult(const DetectionResult& result)
{
    if (m_detectionDialog) {
        // 通过信号或直接调用更新
        emit m_detectionDialog->updateResult(result);
    }
}

bool MainWindow::hasValidConfig() const
{
    ConfigManager* configManager = ConfigManager::instance();
    QList<DetectionConfig> configs = configManager->getDetectionConfigs();
    return !configs.isEmpty();
}

void MainWindow::loadConfigToDialog(DetectionConfigDialog* dialog)
{
    if (!dialog) return;

    ConfigManager* cm = ConfigManager::instance();
    QString path = cm->getConfigFilePath();
    QList<DetectionConfig> configs = cm->loadDetectionConfigsFromFile(path);

    if (!configs.isEmpty()) {
        dialog->setConfigs(configs);
        m_detectionConfigs = configs; //只是此处加载了，但是其他位置没有加载，包括显示同样没有更新
        qDebug() << "加载了" << configs.size() << "条配置";
    } else {
        qDebug() << "没有找到配置";
    }
}

void MainWindow::saveConfigFromDialog(DetectionConfigDialog* dialog)
{
    if (!dialog) return;

    QList<DetectionConfig> configs = dialog->getConfigs();

    // 保存到ConfigManager
    ConfigManager* configManager = ConfigManager::instance();
    configManager->setDetectionConfigs(configs);
    m_detectionConfigs = configs;

    qDebug() << "保存了" << configs.size() << "条配置";
}

void MainWindow::updateConfigFileStatus(const QString& filePath)
{
    m_currentConfigFilePath = filePath;

    // 更新状态栏显示
    if (!filePath.isEmpty()) {
        QFileInfo fileInfo(filePath);
        QString status = QString("当前配置: %1 (%2)").arg(fileInfo.fileName())
                             .arg(QDateTime::currentDateTime().toString("hh:mm:ss"));
        // 如果有状态栏，更新状态显示
        // ui->statusBar->showMessage(status);
        qDebug() << "配置已加载:" << filePath;
    }
}

// ========== 菜单槽函数 ==========
#include <QFile>
#include <QDir>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

//提示创建新的配置成功，并提示当前的配置路径在什么位置，以及实现对其他的一些相关的重新初始化
void MainWindow::on_actioncreatConfig_triggered()
{
    qDebug() << "=== 创建新配置 ===";

    QString configFilePath = QFileDialog::getSaveFileName(
        this,
        "创建配置文件",
        "./config/detection_config.json",
        "JSON配置文件 (*.json);;所有文件 (*)"
        );

    if (configFilePath.isEmpty()) {
        qDebug() << "用户取消了创建";
        return;
    }

    // 补齐json后缀
    if (!configFilePath.endsWith(".json", Qt::CaseInsensitive)) {
        configFilePath += ".json";
    }

    // 自动创建文件夹
    QDir dir = QFileInfo(configFilePath).dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::critical(this, "错误", "创建目录失败！");
            return;
        }
    }

    QFile file(configFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "文件创建失败：" << file.errorString();
        QMessageBox::critical(this, "错误", QString("文件创建失败：\n%1").arg(file.errorString()));
        return;
    }

    // 构造JSON，自带顶部说明文字
    QJsonObject root;
    root["说明"] = "配置文件";
    root["配置参数"] = QJsonObject();

    QJsonDocument doc(root);
    // 格式化写入文件
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    // 设置配置路径到管理器
    ConfigManager* cm = ConfigManager::instance();
    cm->setConfigFilePath(configFilePath);

    qDebug() << "配置文件创建成功：" << configFilePath;

    // ✅ 成功提示 - 显示路径
    QString successMessage = QString(
                                 "✅ 配置文件创建成功！\n\n"
                                 "📁 配置路径：\n%1\n\n"
                                 "💡  现在编辑的内容都将存入其中,方便下次使用。"
                                 ).arg(configFilePath);

    QMessageBox::information(this, "创建成功", successMessage);

    // 可选：在状态栏也显示
    // ui->statusBar->showMessage(
    //     QString("✅ 配置文件已创建: %1").arg(QFileInfo(configFilePath).fileName()),
    //     5000  // 显示5秒
    //     );

    // 可选：自动打开文件所在位置（Windows）
    // 如果你想自动打开文件夹并选中文件，可以取消注释下面这行
    // #ifdef Q_OS_WIN
    // QString explorerCmd = QString("explorer /select,\"%1\"").arg(QDir::toNativeSeparators(configFilePath));
    // QProcess::startDetached(explorerCmd);
    // #endif
}

//导入配置，只需要实现，导入即可，将配置传递到全局配置中，没有则不导入
void MainWindow::on_actionimportConfig_triggered()
{
    //现在的导入就是设置一个路径即可，后续的可以通过这个路径来检索其他配置
    qDebug() << "=== 导入配置 ===";

    // 直接让用户选择JSON文件
    QString configFilePath = QFileDialog::getOpenFileName(
        this,
        "导入配置文件",
        "./config",
        "JSON配置文件 (*.json);;所有文件 (*)"
        );

    if (configFilePath.isEmpty()) {
        qDebug() << "用户取消了导入";
        return;
    }

    // 检查文件是否存在
    if (!QFile::exists(configFilePath)) {
        QMessageBox::warning(this, "错误", "文件不存在");
        return;
    }

    // 直接加载配置文件
    ConfigManager* cm = ConfigManager::instance();

    QString currentConfigPath = configFilePath;       //cm->getConfigFilePath();

    m_currentConfigFilePath = configFilePath;

    cm->setConfigFilePath(currentConfigPath);

    cm->importDetectionConfigs(currentConfigPath);

    m_detectionConfigs = cm->loadDetectionConfigsFromFile(currentConfigPath);

    //自动加载存储图像的路径
    QString savedPath = cm->LoadImageSavePathFromjsonFile();

    if (!savedPath.isEmpty()) {
        m_imageSavePath = savedPath;
        qDebug() << "加载图像保存路径:" << m_imageSavePath;
    } else {
        // 使用默认路径
        m_imageSavePath = QCoreApplication::applicationDirPath() + "/captureImage";
        qDebug() << "使用默认图像保存路径:" << m_imageSavePath;
    }

    //导入标定结果，此处需要增加一个名字 - 配置表的结构体，后续能够随时使用！
    //CalibrationResult LoadCalibrationResultFromjsonFile(const QString& projectName = "");

    // ✅ 添加成功提示
    QMessageBox::information(this, "成功", QString("配置文件导入成功！\n\n文件路径: %1").arg(configFilePath));

    //知道文件路径了，然后现在需要实现一个函数将其json文件解析出来，填写到对应的结构体中很难吗！
}

// ========== 原有的检测配置表菜单 ==========
void MainWindow::on_actiondecConfigTable_triggered()
{
    // 如果对话框已存在，直接显示
    // if (m_detectionConfigDialog) {
    //     m_detectionConfigDialog->show();   //实现显示是，对配置进行重新加载即可
    //     m_detectionConfigDialog->raise();
    //     m_detectionConfigDialog->activateWindow();
    //     return;
    // }

    // 创建检测配置对话框
    m_detectionConfigDialog = new DetectionConfigDialog(this);
    m_detectionConfigDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_detectionConfigDialog->setWindowFlags(Qt::Window);
    m_detectionConfigDialog->setWindowTitle("编辑检测配置");

    connect(m_detectionConfigDialog, &DetectionConfigDialog::destroyed, [this]() {
        m_detectionConfigDialog = nullptr;
        qDebug() << "检测配置对话框已销毁";
    });

    ConfigManager* cm = ConfigManager::instance();

    //单例从文件中获取新的配置，然后实现新的加载！
    //cm->

    // 加载配置
    if(m_currentConfigFilePath != "")  //导入配置时，这个就应该是知道的
    {
        loadConfigToDialog(m_detectionConfigDialog);
    }

    // // 连接确认信号   关闭时自动对其进行配置保存
    // connect(m_detectionConfigDialog, &DetectionConfigDialog::accepted, [this]() {
    //     if (m_detectionConfigDialog) {
    //         saveConfigFromDialog(m_detectionConfigDialog);
    //         QMessageBox::information(this, "成功", "配置已保存");
    //     }
    // });

    m_detectionConfigDialog->show();
}


void MainWindow::on_actionUserManual_triggered()
{
    // 获取程序运行目录
    QString appDir = QCoreApplication::applicationDirPath();

    // HTML文件路径
    QString helpFilePath = appDir + "/docs/help.html";

    // 检查文件是否存在
    if (QFile::exists(helpFilePath)) {
        // 用系统默认浏览器打开
        QUrl helpUrl = QUrl::fromLocalFile(helpFilePath);
        bool success = QDesktopServices::openUrl(helpUrl);

        if (!success) {
            QMessageBox::warning(this, "打开帮助失败",
                                 "无法打开帮助文档，请检查默认浏览器设置。");
        }
    } else {
        // 如果文件不存在，提示用户
        QMessageBox::warning(this, "帮助文件不存在",
                             QString("帮助文件不存在:\n%1\n\n请确保帮助文档已安装。").arg(helpFilePath));

        // 可以选择创建一个默认的帮助文件
        // createDefaultHelpFile(helpFilePath);
    }
}


void MainWindow::on_actionbaseinfo_triggered()
{
    // 创建自定义关于对话框
    QDialog aboutDialog(this);
    aboutDialog.setWindowTitle("关于");
    aboutDialog.setFixedSize(500, 450);
    aboutDialog.setWindowFlags(aboutDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
    aboutDialog.setStyleSheet(
        "QDialog {"
        "    background-color: #1a1a2e;"
        "    border-radius: 10px;"
        "}"
        "QLabel { color: #e0e0e0; }"
        );

    QVBoxLayout* mainLayout = new QVBoxLayout(&aboutDialog);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(30, 25, 30, 25);

    // ===== 1. 应用图标 =====
    QLabel* iconLabel = new QLabel();
    QPixmap iconPixmap = QPixmap(":/icons/app_icon.png");
    if (iconPixmap.isNull()) {
        // 如果没有图标，创建一个默认的
        iconPixmap = QPixmap(80, 80);
        iconPixmap.fill(Qt::transparent);
        QPainter painter(&iconPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(74, 138, 244));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(0, 0, 80, 80, 15, 15);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 24, QFont::Bold));
        painter.drawText(iconPixmap.rect(), Qt::AlignCenter, "AI");
    }
    iconLabel->setPixmap(iconPixmap.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    iconLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(iconLabel);

    // ===== 2. 软件名称 =====
    QLabel* titleLabel = new QLabel("AI 视觉检测系统");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel {"
        "    font-size: 22px;"
        "    font-weight: bold;"
        "    color: #4a8af4;"
        "    padding-top: 5px;"
        "}"
        );
    mainLayout->addWidget(titleLabel);

    // ===== 3. 版本信息 =====
    QLabel* versionLabel = new QLabel("版本 V1.0.0");
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet(
        "QLabel {"
        "    color: #8ab4f8;"
        "    font-size: 14px;"
        "}"
        );
    mainLayout->addWidget(versionLabel);

    // ===== 4. 构建信息 =====
    QLabel* buildLabel = new QLabel(
        QString("构建日期: %1 %2")
            .arg(__DATE__)
            .arg(__TIME__)
        );
    buildLabel->setAlignment(Qt::AlignCenter);
    buildLabel->setStyleSheet(
        "QLabel {"
        "    color: #8080a0;"
        "    font-size: 11px;"
        "}"
        );
    mainLayout->addWidget(buildLabel);

    // ===== 5. 分隔线 =====
    QFrame* line1 = new QFrame();
    line1->setFrameShape(QFrame::HLine);
    line1->setStyleSheet("QFrame { background-color: #2a2a4a; max-height: 1px; }");
    mainLayout->addWidget(line1);

    // ===== 6. 软件描述 =====
    QLabel* descLabel = new QLabel(
        "基于深度学习的工业视觉检测平台\n"
        "支持多相机实时采集、智能检测、数据统计"
        );
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet(
        "QLabel {"
        "    color: #c0c0c0;"
        "    font-size: 13px;"
        "    line-height: 1.6;"
        "}"
        );
    mainLayout->addWidget(descLabel);

    // ===== 7. 技术栈 =====
    QLabel* techLabel = new QLabel(
        "<span style='color: #8080a0; font-size: 11px;'>"
        "核心技术: LibTorch · OpenCV · Qt 6 · Modbus"
        "</span>"
        );
    techLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(techLabel);

    // ===== 8. 分隔线 =====
    QFrame* line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    line2->setStyleSheet("QFrame { background-color: #2a2a4a; max-height: 1px; }");
    mainLayout->addWidget(line2);

    // ===== 9. 版权信息 =====
    QLabel* copyrightLabel = new QLabel(
        "© 雕琢技术. All Rights Reserved."
        );
    copyrightLabel->setAlignment(Qt::AlignCenter);
    copyrightLabel->setStyleSheet(
        "QLabel {"
        "    color: #606080;"
        "    font-size: 10px;"
        "}"
        );
    mainLayout->addWidget(copyrightLabel);

    // ===== 10. 联系方式 =====
    QLabel* contactLabel = new QLabel(
        "技术支持: 2062534023@qq.com"
        );
    contactLabel->setAlignment(Qt::AlignCenter);
    contactLabel->setStyleSheet(
        "QLabel {"
        "    color: #606080;"
        "    font-size: 10px;"
        "}"
        );
    mainLayout->addWidget(contactLabel);

    // ===== 11. 按钮 =====
    QPushButton* closeBtn = new QPushButton("确定");
    closeBtn->setFixedWidth(120);
    closeBtn->setFixedHeight(35);
    closeBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #4a8af4;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #5a9af4; }"
        "QPushButton:pressed { background-color: #3a7ae4; }"
        );
    connect(closeBtn, &QPushButton::clicked, &aboutDialog, &QDialog::accept);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    aboutDialog.exec();
}

void MainWindow::on_actionReportIssue_triggered()
{
    showFeedbackDialog();
}

void MainWindow::showFeedbackDialog()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("报告问题");
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setStyleSheet(
        "QMessageBox {"
        "    background-color: #1a1a2e;"
        "}"
        "QLabel {"
        "    color: #e0e0e0;"
        "    font-size: 13px;"
        "}"
        "QPushButton {"
        "    background-color: #4a8af4;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 8px 25px;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #5a9af4; }"
        );

    QString info =
        "📧 反馈邮箱: 2062534023@qq.com\n\n"
        "💬 QQ群: 415878691\n\n"
        "⏰ 回复时间: 工作日 24小时内";

    msgBox.setText(info);
    msgBox.setStandardButtons(QMessageBox::Ok);

    msgBox.exec();  // ✅ 必须用 exec()，不能用 show()
}


void MainWindow::on_actionsavepathconfig_triggered()
{
    showSetImagePathDialog();
}

void MainWindow::showSetImagePathDialog()
{
    // 创建自定义对话框
    QDialog dialog(this);
    dialog.setWindowTitle("设置图像保存路径");
    dialog.setFixedSize(900, 300);
    dialog.setStyleSheet(
        "QDialog {"
        "    background-color: #1a1a2e;"
        "    border-radius: 10px;"
        "}"
        "QLabel {"
        "    color: #e0e0e0;"
        "    font-size: 13px;"
        "}"
        "QLineEdit {"
        "    background-color: #2a2a4a;"
        "    color: #e0e0e0;"
        "    border: 1px solid #3a3a5a;"
        "    border-radius: 6px;"
        "    padding: 8px 12px;"
        "    font-size: 13px;"
        "}"
        "QLineEdit:focus {"
        "    border-color: #4a8af4;"
        "}"
        "QPushButton {"
        "    background-color: #4a8af4;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 8px 20px;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #5a9af4; }"
        "QPushButton#browseBtn {"
        "    background-color: #2a2a4a;"
        "    color: #e0e0e0;"
        "}"
        "QPushButton#browseBtn:hover { background-color: #3a3a5a; }"
        );

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(30, 25, 30, 25);

    // ===== 标题 =====
    QLabel* titleLabel = new QLabel("📁 选择图像保存路径");
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #4a8af4;");
    mainLayout->addWidget(titleLabel);

    // ===== 当前路径显示 =====
    QLabel* currentLabel = new QLabel("当前路径:");
    mainLayout->addWidget(currentLabel);

    QHBoxLayout* pathLayout = new QHBoxLayout();
    QLineEdit* pathEdit = new QLineEdit();

    // 显示当前保存的路径，如果没有则显示默认路径
    if (m_imageSavePath.isEmpty()) {
        QString defaultPath = QCoreApplication::applicationDirPath() + "/captureImage";
        pathEdit->setText(defaultPath);
        pathEdit->setPlaceholderText("请选择保存路径...");
    } else {
        pathEdit->setText(m_imageSavePath);
    }

    pathLayout->addWidget(pathEdit, 1);

    QPushButton* browseBtn = new QPushButton("浏览...");
    browseBtn->setObjectName("browseBtn");
    browseBtn->setFixedWidth(100);
    connect(browseBtn, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(
            &dialog,
            "选择图像保存目录",
            pathEdit->text(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
            );
        if (!dir.isEmpty()) {
            pathEdit->setText(dir);
        }
    });
    pathLayout->addWidget(browseBtn);
    mainLayout->addLayout(pathLayout);

    // ===== 提示信息 =====
    QLabel* hintLabel = new QLabel("💡 所有采集的图像将保存到所选目录下");
    hintLabel->setStyleSheet("color: #8080a0; font-size: 12px;");
    mainLayout->addWidget(hintLabel);

    // ===== 按钮 =====
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton* cancelBtn = new QPushButton("取消");
    cancelBtn->setFixedWidth(100);
    cancelBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent;"
        "    color: #8080a0;"
        "    border: none;"
        "    padding: 8px 20px;"
        "    font-size: 13px;"
        "}"
        "QPushButton:hover { color: #e0e0e0; }"
        );
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    QPushButton* saveBtn = new QPushButton("✅ 保存路径");
    saveBtn->setFixedWidth(120);
    connect(saveBtn, &QPushButton::clicked, [&]() {
        QString newPath = pathEdit->text().trimmed();
        if (newPath.isEmpty()) {
            QMessageBox::warning(&dialog, "提示", "请选择有效的保存路径");
            return;
        }

        // 检查路径是否有效
        QDir dir(newPath);
        if (!dir.exists()) {
            // 尝试创建目录
            if (!dir.mkpath(".")) {
                QMessageBox::warning(&dialog, "错误", "无法创建目录，请检查路径是否正确");
                return;
            }
        }

        // 保存路径
        m_imageSavePath = newPath;

        ConfigManager* cm = ConfigManager::instance();

        QString path = cm->getConfigFilePath();

        cm->SaveImageSavePathtojsonFile(m_imageSavePath,path);

        QMessageBox::information(&dialog, "成功",
                                 QString("✅ 图像保存路径已设置:\n%1").arg(m_imageSavePath));

        dialog.accept();
    });
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);

    dialog.exec();
}

void MainWindow::setupFileCleaner()
{
    m_fileCleaner = new FileCleaner(this);

    // 设置监控路径（图像保存目录）
    QString imagePath = m_imageSavePath;
    m_fileCleaner->setMonitorPath(imagePath);

    // 设置阈值 50%
    m_fileCleaner->setThresholdPercent(50);

    // 设置清理比例 50%（清理一半旧文件）
    m_fileCleaner->setCleanRatio(0.5);

    // 设置文件类型
    m_fileCleaner->setFileExtensions({"*.jpg", "*.jpeg", "*.png", "*.bmp"});

    // 启用递归扫描
    m_fileCleaner->setRecursive(true);

    // 设置检查间隔 5分钟
    m_fileCleaner->setCheckInterval(300000);

    // 启用自动清理
    m_fileCleaner->setAutoCleanEnabled(true);

    // 连接信号
    connect(m_fileCleaner, &FileCleaner::diskUsageWarning,
            this, [this](double current, double threshold) {
                qDebug() << "⚠️ 磁盘使用率警告:" << current << "% >= " << threshold << "%";
                // 可以在这里显示通知
            });

    connect(m_fileCleaner, &FileCleaner::cleanFinished,
            this, [this](int count, double freedMB) {
                qDebug() << "✅ 清理完成: 删除" << count << "个文件，释放" << freedMB << "MB";
                // 可以显示通知
                QMessageBox::information(this, "磁盘清理",
                                         QString("已清理 %1 个旧文件\n释放空间 %2 MB")
                                             .arg(count)
                                             .arg(freedMB, 0, 'f', 2));
            });

    connect(m_fileCleaner, &FileCleaner::errorOccurred,
            this, [this](const QString& error) {
                qWarning() << "❌ 文件清理器错误:" << error;
            });

    qDebug() << "✅ 文件清理器已初始化";
}

QString MainWindow::getImageSavePath()
{
    return m_imageSavePath;
}