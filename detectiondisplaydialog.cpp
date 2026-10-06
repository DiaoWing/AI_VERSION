#include "detectiondisplaydialog.h"
#include "parammanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QPainter>
#include <QKeyEvent>
#include <QMenu>
#include <QAction>
#include <QProgressDialog>
#include <QDir>


// ==================== ClickableImageLabel 实现 ====================
ClickableImageLabel::ClickableImageLabel(QWidget* parent)
    : QLabel(parent)
{
    setCursor(Qt::PointingHandCursor);
    setAlignment(Qt::AlignCenter);
    setScaledContents(false);
    setStyleSheet(
        "QLabel {"
        "    background-color: #0d0d1a;"
        "    border-radius: 3px;"
        "    color: #404060;"
        "    font-size: 14px;"
        "}"
        );
}

ClickableImageLabel::~ClickableImageLabel()
{
}

void ClickableImageLabel::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    emit doubleClicked();
}

// ==================== DetectionDisplayDialog 实现 ====================

DetectionDisplayDialog::DetectionDisplayDialog(QWidget* parent)
    : QDialog(parent)
    , m_isInitialized(false)
    , m_isFullScreen(false)
    , m_fullScreenContainer(nullptr)
    , m_fullScreenLabel(nullptr)
    , m_fullScreenTitle(nullptr)
    , m_fullScreenHint(nullptr)
    , m_contextMenu(nullptr)
    , m_detectSingleAction(nullptr)
    , m_detectBatchAction(nullptr)
{
    setWindowTitle("检测结果显示");
    resize(1400, 800);
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);
    setStyleSheet("QDialog { background-color: #1a1a2e; }");

    setupUI();
    setupDetectionWorker();
    setupContextMenu();
}

DetectionDisplayDialog::~DetectionDisplayDialog()
{
    if (m_detectionWorker) {
        m_detectionWorker->stop();
    }
    if (m_detectionThread) {
        m_detectionThread->quit();
        m_detectionThread->wait();
        delete m_detectionThread;
    }
}

void DetectionDisplayDialog::setupUI()
{
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    setupLeftPanel();
    setupRightPanel();

    mainLayout->addWidget(m_leftPanel, 8);
    mainLayout->addWidget(m_rightPanel, 2);

    setupFullScreenContainer();
}

void DetectionDisplayDialog::setupContextMenu()
{
    m_contextMenu = new QMenu(this);

    m_detectSingleAction = new QAction("检测单张图像", this);
    m_detectBatchAction = new QAction("批量检测图像", this);

    m_contextMenu->addAction(m_detectSingleAction);
    m_contextMenu->addAction(m_detectBatchAction);

    connect(m_detectSingleAction, &QAction::triggered,
            this, &DetectionDisplayDialog::onDetectSingleImage);
    connect(m_detectBatchAction, &QAction::triggered,
            this, &DetectionDisplayDialog::onDetectBatchImages);

    // 为左侧面板设置右键菜单
    m_leftPanel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_leftPanel, &QWidget::customContextMenuRequested,
            this, &DetectionDisplayDialog::onCustomContextMenuRequested);
}

void DetectionDisplayDialog::setupFullScreenContainer()
{
    m_fullScreenContainer = new QWidget(m_leftPanel);
    m_fullScreenContainer->setGeometry(m_leftPanel->rect());
    m_fullScreenContainer->setStyleSheet(
        "QWidget {"
        "    background-color: #0d0d1a;"
        "    border: 2px solid #4a8af4;"
        "    border-radius: 8px;"
        "}"
        );
    m_fullScreenContainer->hide();

    QVBoxLayout* layout = new QVBoxLayout(m_fullScreenContainer);
    layout->setSpacing(10);
    layout->setContentsMargins(20, 20, 20, 20);

    m_fullScreenTitle = new QLabel(m_fullScreenContainer);
    m_fullScreenTitle->setAlignment(Qt::AlignCenter);
    m_fullScreenTitle->setStyleSheet(
        "QLabel {"
        "    color: #8ab4f8;"
        "    font-size: 16px;"
        "    font-weight: bold;"
        "    padding: 5px;"
        "}"
        );
    layout->addWidget(m_fullScreenTitle);

    m_fullScreenLabel = new ClickableImageLabel(m_fullScreenContainer);
    m_fullScreenLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_fullScreenLabel->setMinimumHeight(400);
    m_fullScreenLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #0d0d1a;"
        "    border: 1px solid #2a2a4a;"
        "    border-radius: 5px;"
        "    color: #404060;"
        "    font-size: 14px;"
        "}"
        );
    layout->addWidget(m_fullScreenLabel, 1);

    connect(m_fullScreenLabel, &ClickableImageLabel::doubleClicked, [this]() {
        exitFullScreenMode();
    });

    m_fullScreenHint = new QLabel("双击图像或按 ESC 键退出全屏", m_fullScreenContainer);
    m_fullScreenHint->setAlignment(Qt::AlignCenter);
    m_fullScreenHint->setStyleSheet(
        "QLabel {"
        "    color: #8080a0;"
        "    font-size: 12px;"
        "    padding: 5px;"
        "}"
        );
    layout->addWidget(m_fullScreenHint);
}

void DetectionDisplayDialog::setupLeftPanel()
{
    m_leftPanel = new QWidget(this);
    m_leftPanel->setStyleSheet("background-color: #0d0d1a; border-radius: 8px;");
    m_leftPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout* leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setSpacing(5);
    leftLayout->setContentsMargins(10, 10, 10, 10);

    QLabel* titleLabel = new QLabel("检测图像预览", m_leftPanel);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #8ab4f8; font-size: 14px; font-weight: bold; padding: 5px;");
    leftLayout->addWidget(titleLabel);

    m_leftScrollArea = new QScrollArea(m_leftPanel);
    m_leftScrollArea->setWidgetResizable(true);
    m_leftScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_leftScrollArea->setStyleSheet(
        "QScrollArea {"
        "    border: 1px solid #2a2a4a;"
        "    border-radius: 5px;"
        "    background-color: #0d0d1a;"
        "}"
        "QScrollBar:vertical {"
        "    background: #1a1a2e;"
        "    width: 8px;"
        "    border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #4a6a8a;"
        "    border-radius: 4px;"
        "    min-height: 20px;"
        "}"
        );

    m_leftContent = new QWidget(m_leftScrollArea);
    m_leftContent->setStyleSheet("background-color: #0d0d1a;");
    m_leftContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_imageGridLayout = new QGridLayout(m_leftContent);
    m_imageGridLayout->setSpacing(8);
    m_imageGridLayout->setContentsMargins(10, 10, 10, 10);

    m_leftScrollArea->setWidget(m_leftContent);
    leftLayout->addWidget(m_leftScrollArea);
}

void DetectionDisplayDialog::setupRightPanel()
{
    m_rightPanel = new QWidget(this);
    m_rightPanel->setStyleSheet("background-color: #1a1a2e; border-radius: 8px;");
    m_rightPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_rightPanel->setMinimumWidth(250);
    m_rightPanel->setMaximumWidth(350);

    QVBoxLayout* rightLayout = new QVBoxLayout(m_rightPanel);
    rightLayout->setSpacing(8);
    rightLayout->setContentsMargins(10, 10, 10, 10);

    QLabel* titleLabel = new QLabel("参数配置", m_rightPanel);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #8ab4f8; font-size: 14px; font-weight: bold; padding: 5px;");
    rightLayout->addWidget(titleLabel);

    setupParamPanel();

    rightLayout->addWidget(m_paramPanel.projectCombo);
    rightLayout->addWidget(m_paramPanel.paramScrollArea, 1);
    rightLayout->addWidget(m_paramPanel.applyBtn);
}

void DetectionDisplayDialog::setupParamPanel()
{
    m_paramPanel.projectCombo = new QComboBox(this);
    m_paramPanel.projectCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_paramPanel.projectCombo->setMinimumHeight(35);
    m_paramPanel.projectCombo->setStyleSheet(
        "QComboBox {"
        "    background-color: #2a2a4a;"
        "    color: #e0e0e0;"
        "    border: 1px solid #3a3a5a;"
        "    border-radius: 5px;"
        "    padding: 8px;"
        "    font-size: 12px;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background-color: #2a2a4a;"
        "    color: #e0e0e0;"
        "    selection-background-color: #4a6a8a;"
        "}"
        );
    connect(m_paramPanel.projectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DetectionDisplayDialog::onProjectSelected);

    m_paramPanel.paramScrollArea = new QScrollArea(this);
    m_paramPanel.paramScrollArea->setWidgetResizable(true);
    m_paramPanel.paramScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_paramPanel.paramScrollArea->setStyleSheet(
        "QScrollArea {"
        "    border: 1px solid #2a2a4a;"
        "    border-radius: 5px;"
        "    background-color: #1a1a2e;"
        "}"
        "QScrollBar:vertical {"
        "    background: #1a1a2e;"
        "    width: 6px;"
        "    border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #4a6a8a;"
        "    border-radius: 3px;"
        "    min-height: 20px;"
        "}"
        );

    m_paramPanel.paramContent = new QWidget(m_paramPanel.paramScrollArea);
    m_paramPanel.paramContent->setStyleSheet("background-color: #1a1a2e;");
    m_paramPanel.paramContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    m_paramPanel.paramLayout = new QFormLayout(m_paramPanel.paramContent);
    m_paramPanel.paramLayout->setSpacing(10);
    m_paramPanel.paramLayout->setContentsMargins(10, 10, 10, 10);
    m_paramPanel.paramLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_paramPanel.paramScrollArea->setWidget(m_paramPanel.paramContent);

    m_paramPanel.applyBtn = new QPushButton("应用参数", this);
    m_paramPanel.applyBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_paramPanel.applyBtn->setMinimumHeight(35);
    m_paramPanel.applyBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #4a8af4;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 5px;"
        "    padding: 8px 15px;"
        "    font-weight: bold;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #5a9af4; }"
        "QPushButton:pressed { background-color: #3a7ae4; }"
        );
    connect(m_paramPanel.applyBtn, &QPushButton::clicked, this, &DetectionDisplayDialog::onApplyParams);
}

void DetectionDisplayDialog::setupDetectionWorker()
{
    m_detectionThread = new QThread(this);
    m_detectionWorker = new DetectionWorker();
    m_detectionWorker->moveToThread(m_detectionThread);

    connect(m_detectionThread, &QThread::started, [this]() {
        qDebug() << "[DetectionDisplay] 检测线程已启动";
    });

    //显示检测结果
    connect(m_detectionWorker, &DetectionWorker::detectionFinished,
            this, &DetectionDisplayDialog::onDetectionFinished);


    connect(m_detectionWorker, &DetectionWorker::debugInfo,
            this, &DetectionDisplayDialog::onDebugInfo);

    m_detectionThread->start();
}

void DetectionDisplayDialog::setDetectionConfigs(const QList<DetectionConfig>& configs)
{
    m_configs = configs;

    if (m_detectionWorker) {
        m_detectionWorker->setDetectionConfigs(configs);
    }

    updateImageLayout();

    m_paramPanel.projectCombo->clear();
    m_paramPanel.projectCombo->addItem("选择项目...");

    ParamManager* pm = ParamManager::instance();

    // ===== 新增：加载所有项目的JSON配置 =====
    // 尝试加载已保存的参数配置
    pm->loadAllProjectsFromJson();

    for (const DetectionConfig& config : configs) {
        if (!config.projectName.isEmpty()) {
            // 如果项目已存在且没有参数值，从JSON加载
            if (pm->hasProject(config.projectName)) {
                // 检查是否有参数值，如果没有则尝试加载JSON
                QMap<QString, QVariant> values = pm->getParamValues(config.projectName);
                if (values.isEmpty()) {
                    QString jsonPath = pm->getDefaultJsonPath(config.projectName);
                    if (QFile::exists(jsonPath)) {
                        pm->loadProjectParamsFromJson(jsonPath);
                        qDebug() << "[DetectionDisplay] 从JSON加载项目参数:" << config.projectName;
                    }
                }
            }

            m_paramPanel.projectCombo->addItem(config.projectName);
            qDebug() << "[DetectionDisplay] 添加项目:" << config.projectName
                     << "有参数:" << pm->hasProject(config.projectName);
        }
    }

    if (m_paramPanel.projectCombo->count() > 1) {
        m_paramPanel.projectCombo->setCurrentIndex(1);
    }
}

void DetectionDisplayDialog::updateImageLayout()
{
    QLayoutItem* child;
    while ((child = m_imageGridLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    m_displayItems.clear();
    m_projectNameToIndex.clear();

    int count = m_configs.size();
    if (count == 0) {
        QLabel* emptyLabel = new QLabel("暂无检测配置\n请先配置检测项目", m_leftContent);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #505070; font-size: 18px;");
        m_imageGridLayout->addWidget(emptyLabel, 0, 0);
        return;
    }

    int rows, cols;
    if (count <= 1) {
        rows = 1; cols = 1;
    } else if (count == 2) {
        rows = 1; cols = 2;
    } else if (count == 3) {
        rows = 1; cols = 3;
    } else if (count <= 4) {
        rows = 2; cols = 2;
    } else {
        rows = 2; cols = 3;
    }

    for (int i = 0; i < count; i++) {
        const DetectionConfig& config = m_configs[i];
        DisplayItem item;
        item.projectName = config.projectName;
        item.cameraName = QString("cam%1").arg(config.cameraId);
        item.hasImage = false;

        item.container = new QWidget(m_leftContent);
        item.container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        item.container->setStyleSheet(
            "QWidget {"
            "    background-color: #1a1a2e;"
            "    border: 1px solid #2a2a4a;"
            "    border-radius: 5px;"
            "}"
            );

        QVBoxLayout* containerLayout = new QVBoxLayout(item.container);
        containerLayout->setSpacing(5);
        containerLayout->setContentsMargins(5, 5, 5, 5);

        item.imageLabel = new ClickableImageLabel(item.container);
        item.imageLabel->setMinimumHeight(150);
        item.imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        item.imageLabel->setText("等待图像...");

        // 为图像标签设置右键菜单
        item.imageLabel->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(item.imageLabel, &QLabel::customContextMenuRequested,
                this, &DetectionDisplayDialog::onCustomContextMenuRequested);

        connect(item.imageLabel, &ClickableImageLabel::doubleClicked,
                [this, projectName = config.projectName]() {
                    onImageDoubleClicked(projectName);
                });

        containerLayout->addWidget(item.imageLabel, 1);

        item.nameLabel = new QLabel(config.projectName, item.container);
        item.nameLabel->setAlignment(Qt::AlignCenter);
        item.nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        item.nameLabel->setStyleSheet(
            "QLabel {"
            "    color: #8ab4f8;"
            "    font-size: 12px;"
            "    font-weight: bold;"
            "    padding: 5px;"
            "}"
            );
        containerLayout->addWidget(item.nameLabel);

        int row = i / cols;
        int col = i % cols;

        m_imageGridLayout->addWidget(item.container, row, col);
        m_displayItems.append(item);
        m_projectNameToIndex[config.projectName] = i;
    }

    for (int i = 0; i < rows; i++) {
        m_imageGridLayout->setRowStretch(i, 1);
    }
    for (int i = 0; i < cols; i++) {
        m_imageGridLayout->setColumnStretch(i, 1);
    }
}

void DetectionDisplayDialog::updateImageDisplay(const QString& projectName, const QPixmap& pixmap)
{
    if (!m_projectNameToIndex.contains(projectName)) return;

    int index = m_projectNameToIndex[projectName];
    if (index < 0 || index >= m_displayItems.size()) return;

    DisplayItem& item = m_displayItems[index];
    item.hasImage = true;
    item.lastImage = pixmap;

    if (pixmap.isNull()) {
        item.imageLabel->setText("图像加载失败");
        item.hasImage = false;
    } else {
        showImageOnLabel(item.imageLabel, pixmap);
        m_lastResults[projectName] = pixmap;
    }
}

void DetectionDisplayDialog::showImageOnLabel(QLabel* label, const QPixmap& pixmap)
{
    if (!label || pixmap.isNull()) return;

    QSize labelSize = label->size();

    if (labelSize.width() < 10 || labelSize.height() < 10) {
        QWidget* parent = label->parentWidget();
        if (parent) {
            labelSize = parent->size();
            QMargins margins = parent->contentsMargins();
            labelSize -= QSize(margins.left() + margins.right() + 20,
                               margins.top() + margins.bottom() + 20);
        }
    }

    if (labelSize.width() < 50 || labelSize.height() < 50) {
        labelSize = QSize(400, 300);
    }

    QPixmap scaled = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    label->setPixmap(scaled);
}

// ==================== 全屏显示管理 ====================

void DetectionDisplayDialog::enterFullScreenMode(const QString& projectName)
{
    if (m_isFullScreen) return;

    if (!m_projectNameToIndex.contains(projectName)) return;

    int index = m_projectNameToIndex[projectName];
    if (index < 0 || index >= m_displayItems.size()) return;

    const DisplayItem& item = m_displayItems[index];
    if (!item.hasImage || item.lastImage.isNull()) {
        QMessageBox msgBox(QMessageBox::Information,
                           "提示",
                           QString("项目 '%1' 暂无检测图像\n请先执行检测操作").arg(projectName),
                           QMessageBox::Ok,
                           this);
        msgBox.setStyleSheet(
            "QMessageBox {"
            "    background-color: white;"
            "}"
            "QPushButton {"
            "    background-color: #4a8af4;"
            "    color: white;"
            "    border: none;"
            "    border-radius: 4px;"
            "    padding: 6px 25px;"
            "    font-weight: bold;"
            "    min-width: 80px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #5a9af4;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #3a7ae4;"
            "}"
            );
        msgBox.exec();
        return;
    }

    m_fullScreenProjectName = projectName;
    m_fullScreenTitle->setText(QString("🔍 %1 - 全屏查看").arg(projectName));

    m_fullScreenContainer->raise();
    m_fullScreenContainer->show();
    m_leftScrollArea->hide();

    m_isFullScreen = true;

    QTimer::singleShot(30, this, [this, projectName]() {
        if (m_isFullScreen && m_fullScreenProjectName == projectName) {
            int idx = m_projectNameToIndex[projectName];
            if (idx >= 0 && idx < m_displayItems.size()) {
                const DisplayItem& item = m_displayItems[idx];
                if (!item.lastImage.isNull()) {
                    showImageOnLabel(m_fullScreenLabel, item.lastImage);
                }
            }
        }
    });

    qDebug() << "[DetectionDisplay] 进入全屏模式:" << projectName;
}

void DetectionDisplayDialog::exitFullScreenMode()
{
    if (!m_isFullScreen) return;

    m_fullScreenContainer->hide();
    m_leftScrollArea->show();

    m_isFullScreen = false;
    m_fullScreenProjectName.clear();

    qDebug() << "[DetectionDisplay] 退出全屏模式";
}

bool DetectionDisplayDialog::isFullScreenMode() const
{
    return m_isFullScreen;
}

// ==================== 双击处理 ====================

void DetectionDisplayDialog::onImageDoubleClicked(const QString& projectName)
{
    qDebug() << "[DetectionDisplay] 双击图像:" << projectName;

    if (m_isFullScreen) {
        exitFullScreenMode();
        if (m_fullScreenProjectName == projectName) {
            return;
        }
    }

    enterFullScreenMode(projectName);
}

// ==================== 键盘事件 ====================

void DetectionDisplayDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && m_isFullScreen) {
        exitFullScreenMode();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

// ==================== 窗口大小变化 ====================

void DetectionDisplayDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);

    if (m_fullScreenContainer) {
        m_fullScreenContainer->setGeometry(m_leftPanel->rect());
    }
}

// ==================== 右键菜单 ====================

void DetectionDisplayDialog::onCustomContextMenuRequested(const QPoint& pos)
{
    Q_UNUSED(pos);

    if (m_configs.isEmpty()) {
        QMessageBox::warning(this, "提示", "没有检测配置，请先配置检测项目");
        return;
    }

    // ✅ 通过发送信号的对象来判断是哪个项目
    QObject* senderObj = sender();
    if (senderObj) {
        // 如果是图像标签触发的
        if (QLabel* label = qobject_cast<QLabel*>(senderObj)) {
            // 查找对应的项目名
            for (const DisplayItem& item : m_displayItems) {
                if (item.imageLabel == label) {
                    m_contextMenuProjectName = item.projectName;
                    break;
                }
            }
        }
        // 如果是左侧面板触发的，使用当前选中的项目
        else if (senderObj == m_leftPanel) {
            m_contextMenuProjectName = m_paramPanel.projectCombo->currentText();
        }
    }

    if (m_contextMenuProjectName.isEmpty() || m_contextMenuProjectName == "选择项目...") {
        QMessageBox::warning(this, "提示", "无法确定要检测的项目");
        return;
    }

    // 同时更新下拉框选中状态
    int index = m_paramPanel.projectCombo->findText(m_contextMenuProjectName);
    if (index > 0) {
        m_paramPanel.projectCombo->setCurrentIndex(index);
    }

    m_contextMenu->exec(QCursor::pos());
}

QString DetectionDisplayDialog::getCurrentProjectName() const
{
    QString projectName = m_paramPanel.projectCombo->currentText();
    if (projectName.isEmpty() || projectName == "选择项目...") {
        return QString();
    }
    return projectName;
}


//右键传入对应的图像，对其执行对应的检测
void DetectionDisplayDialog::onDetectSingleImage()
{
    QString projectName = getCurrentProjectName();
    if (projectName.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先在右侧选择检测项目");
        return;
    }

    QString imagePath = QFileDialog::getOpenFileName(
        this,
        "选择要检测的图像",
        "",
        "图像文件 (*.png *.jpg *.jpeg *.bmp *.tiff)"
        );

    if (imagePath.isEmpty()) {
        return;
    }

    performDetectionOnImage(imagePath, projectName);
}


void DetectionDisplayDialog::onDetectBatchImages()
{
    QString projectName = getCurrentProjectName();
    if (projectName.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先在右侧选择检测项目");
        return;
    }

    QString folderPath = QFileDialog::getExistingDirectory(
        this,
        "选择包含图像的文件夹",
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    if (folderPath.isEmpty()) {
        return;
    }

    QDir dir(folderPath);
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.tiff" << "*.tif";
    QStringList imageFiles = dir.entryList(filters, QDir::Files);

    if (imageFiles.isEmpty()) {
        QMessageBox::information(this, "提示", "文件夹中没有找到图像文件");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "批量检测确认",
        QString("将检测 %1 张图像，是否继续？").arg(imageFiles.size()),
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply != QMessageBox::Yes) {
        return;
    }

    // ===== 修改：逐个提交并实时更新 =====
    m_batchDetectionCount = 0;
    m_batchTotalCount = imageFiles.size();

    // 显示开始提示
    QMessageBox::information(this, "批量检测",
                             QString("开始批量检测 %1 张图像\n检测结果将实时更新").arg(imageFiles.size()));

    // 逐个提交任务
    for (int i = 0; i < imageFiles.size(); i++) {
        QString fullPath = dir.absoluteFilePath(imageFiles[i]);

        // 提交检测任务
        performDetectionOnImage(fullPath, projectName);

        // 每提交一张图像，更新状态
        m_batchDetectionCount = i + 1;
        qDebug() << "[Batch] 已提交第" << m_batchDetectionCount << "/" << m_batchTotalCount
                 << "张图像:" << imageFiles[i];

        // 允许UI及时响应用户操作
        QApplication::processEvents();
    }

    qDebug() << "[Batch] 所有图像已提交，共" << imageFiles.size() << "张";
}


// ===== 图像转换函数 =====
cv::Mat  qImageToMat(const QImage& image)
{
    if (image.isNull()) {
        return cv::Mat();
    }

    // ✅ 核心修复：统一转换为 RGB888
    QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3,
                (void*)rgb.constBits(), rgb.bytesPerLine());
    cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
    return mat.clone();
}


void DetectionDisplayDialog::performDetectionOnImage(const QString& imagePath, const QString& projectName)
{
    //直接不转换，使用mat 不就行了？
    //cv::Mat img = cv::imread(imagePath);
    QImage image(imagePath);
    if (image.isNull()) {
        qDebug() << "[DetectionDisplay] 无法加载图像:" << imagePath;
        return;
    }

    // ✅ 打印图像格式信息
    qDebug() << "QImage 原始格式:" << image.format();
    qDebug() << "QImage 尺寸:" << image.size();
    qDebug() << "QImage 通道数:" << (image.depth() / 8);

    // 保存原始 QImage 看看
    image.save("E:/temp_pic/qimage_original.png", "PNG");
    qDebug() << "已保存原始 QImage";

    cv::Mat mat = qImageToMat(image);  // 使用你已有的转换函数
   // cv::imwrite("E:/temp_pic/fadf_isme.png",mat);

    DetectionTask task;
    task.groupId = getgroupIdByName(projectName); //
    task.projectName = projectName;
    task.image = image;

    if (m_detectionWorker) {
        m_detectionWorker->addTask(task);
        qDebug() << "[DetectionDisplay] 检测任务已提交:" << task.groupId
                 << "项目:" << projectName;
    }
}

// ==================== 参数面板 ====================

void DetectionDisplayDialog::onProjectSelected(int index)
{
    if (!m_paramPanel.paramLayout) {
        qDebug() << "[DetectionDisplay] paramLayout 为空！";
        return;
    }

    clearParamWidgets();

    if (index <= 0) {
        return;
    }

    QString projectName = m_paramPanel.projectCombo->currentText();
    if (projectName.isEmpty() || projectName == "选择项目...") {
        return;
    }

    loadProjectParams(projectName);
}

void DetectionDisplayDialog::loadProjectParams(const QString& projectName)
{
    ParamManager* pm = ParamManager::instance();

    qDebug() << "[DetectionDisplay] 加载项目参数:" << projectName;

    if (!pm->hasProject(projectName)) {
        QLabel* label = new QLabel("该项目暂无参数配置", m_paramPanel.paramContent);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("color: #8080a0; font-size: 14px;");
        m_paramPanel.paramLayout->addRow(label);
        return;
    }

    QList<ParamDefinition> paramDefs = pm->getParamDefs(projectName);
    QMap<QString, QVariant> paramValues = pm->getParamValues(projectName);

    rebuildParamWidgets(paramDefs);

    for (auto it = paramValues.begin(); it != paramValues.end(); ++it) {
        if (m_paramPanel.paramWidgets.contains(it.key())) {
            QWidget* widget = m_paramPanel.paramWidgets[it.key()];

            if (QSpinBox* spinBox = qobject_cast<QSpinBox*>(widget)) {
                spinBox->setValue(it.value().toInt());
            } else if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(widget)) {
                spinBox->setValue(it.value().toDouble());
            } else if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(widget)) {
                checkBox->setChecked(it.value().toBool());
            } else if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget)) {
                lineEdit->setText(it.value().toString());
            } else if (QComboBox* comboBox = qobject_cast<QComboBox*>(widget)) {
                int idx = comboBox->findText(it.value().toString());
                if (idx >= 0) comboBox->setCurrentIndex(idx);
            }
        }
    }
}

void DetectionDisplayDialog::rebuildParamWidgets(const QList<ParamDefinition>& paramDefs)
{
    clearParamWidgets();

    if (paramDefs.isEmpty()) {
        QLabel* label = new QLabel("无可用参数", m_paramPanel.paramContent);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("color: #8080a0; font-size: 14px;");
        m_paramPanel.paramLayout->addRow(label);
        return;
    }

    for (const ParamDefinition& def : paramDefs) {
        QWidget* valueWidget = nullptr;
        QString displayName = def.displayName.isEmpty() ? def.name : def.displayName;

        if (def.type == "int") {
            QSpinBox* spinBox = new QSpinBox(m_paramPanel.paramContent);
            spinBox->setRange(def.minValue.toInt(), def.maxValue.toInt());
            spinBox->setValue(def.defaultValue.toInt());
            spinBox->setMinimumHeight(30);
            spinBox->setStyleSheet(
                "QSpinBox {"
                "    background-color: #2a2a4a;"
                "    color: #e0e0e0;"
                "    border: 1px solid #3a3a5a;"
                "    border-radius: 3px;"
                "    padding: 4px;"
                "}"
                "QSpinBox::up-button, QSpinBox::down-button {"
                "    background-color: #3a3a5a;"
                "    border: none;"
                "}"
                );
            connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, &DetectionDisplayDialog::onParamValueChanged);
            valueWidget = spinBox;

        } else if (def.type == "double") {
            QDoubleSpinBox* spinBox = new QDoubleSpinBox(m_paramPanel.paramContent);
            spinBox->setRange(def.minValue.toDouble(), def.maxValue.toDouble());
            spinBox->setDecimals(2);
            spinBox->setValue(def.defaultValue.toDouble());
            spinBox->setMinimumHeight(30);
            spinBox->setStyleSheet(
                "QDoubleSpinBox {"
                "    background-color: #2a2a4a;"
                "    color: #e0e0e0;"
                "    border: 1px solid #3a3a5a;"
                "    border-radius: 3px;"
                "    padding: 4px;"
                "}"
                "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
                "    background-color: #3a3a5a;"
                "    border: none;"
                "}"
                );
            connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &DetectionDisplayDialog::onParamValueChanged);
            valueWidget = spinBox;

        } else if (def.type == "bool") {
            QCheckBox* checkBox = new QCheckBox(m_paramPanel.paramContent);
            checkBox->setChecked(def.defaultValue.toBool());
            checkBox->setMinimumHeight(30);
            checkBox->setStyleSheet("color: #e0e0e0;");
            connect(checkBox, &QCheckBox::toggled,
                    this, &DetectionDisplayDialog::onParamValueChanged);
            valueWidget = checkBox;

        } else if (def.type == "enum") {
            QComboBox* comboBox = new QComboBox(m_paramPanel.paramContent);
            comboBox->addItems(def.enumValues);
            int idx = comboBox->findText(def.defaultValue.toString());
            if (idx >= 0) comboBox->setCurrentIndex(idx);
            comboBox->setMinimumHeight(30);
            comboBox->setStyleSheet(
                "QComboBox {"
                "    background-color: #2a2a4a;"
                "    color: #e0e0e0;"
                "    border: 1px solid #3a3a5a;"
                "    border-radius: 3px;"
                "    padding: 4px;"
                "}"
                "QComboBox::drop-down { border: none; }"
                "QComboBox QAbstractItemView {"
                "    background-color: #2a2a4a;"
                "    color: #e0e0e0;"
                "    selection-background-color: #4a6a8a;"
                "}"
                );
            connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &DetectionDisplayDialog::onParamValueChanged);
            valueWidget = comboBox;

        } else {
            QLineEdit* lineEdit = new QLineEdit(def.defaultValue.toString(), m_paramPanel.paramContent);
            lineEdit->setMinimumHeight(30);
            lineEdit->setStyleSheet(
                "QLineEdit {"
                "    background-color: #2a2a4a;"
                "    color: #e0e0e0;"
                "    border: 1px solid #3a3a5a;"
                "    border-radius: 3px;"
                "    padding: 4px;"
                "}"
                );
            connect(lineEdit, &QLineEdit::textChanged,
                    this, &DetectionDisplayDialog::onParamValueChanged);
            valueWidget = lineEdit;
        }

        if (valueWidget) {
            valueWidget->setProperty("paramName", def.name);
            m_paramPanel.paramWidgets[def.name] = valueWidget;
            m_paramPanel.currentValues[def.name] = def.defaultValue;

            QLabel* label = new QLabel(displayName + ":", m_paramPanel.paramContent);
            label->setStyleSheet("color: #c0c0c0; font-size: 12px;");
            label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            label->setMinimumHeight(30);
            m_paramPanel.paramLayout->addRow(label, valueWidget);
        }
    }
}

void DetectionDisplayDialog::clearParamWidgets()
{
    if (m_paramPanel.paramLayout) {
        QLayoutItem* child;
        while ((child = m_paramPanel.paramLayout->takeAt(0)) != nullptr) {
            if (child->widget()) {
                child->widget()->deleteLater();
            }
            delete child;
        }
    }

    m_paramPanel.paramWidgets.clear();
    m_paramPanel.currentValues.clear();

    if (m_paramPanel.paramContent) {
        m_paramPanel.paramContent->update();
    }
}

void DetectionDisplayDialog::onParamValueChanged()
{
    collectCurrentParams();
}

void DetectionDisplayDialog::collectCurrentParams()
{
    for (auto it = m_paramPanel.paramWidgets.begin();
         it != m_paramPanel.paramWidgets.end(); ++it) {
        const QString& paramName = it.key();
        QWidget* widget = it.value();

        if (QSpinBox* spinBox = qobject_cast<QSpinBox*>(widget)) {
            m_paramPanel.currentValues[paramName] = spinBox->value();
        } else if (QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(widget)) {
            m_paramPanel.currentValues[paramName] = spinBox->value();
        } else if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(widget)) {
            m_paramPanel.currentValues[paramName] = checkBox->isChecked();
        } else if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget)) {
            m_paramPanel.currentValues[paramName] = lineEdit->text();
        } else if (QComboBox* comboBox = qobject_cast<QComboBox*>(widget)) {
            m_paramPanel.currentValues[paramName] = comboBox->currentText();
        }
    }
}

void DetectionDisplayDialog::onApplyParams()
{
    QString projectName = m_paramPanel.projectCombo->currentText();
    if (projectName.isEmpty() || projectName == "选择项目...") {
        QMessageBox::warning(this, "提示", "请先选择一个项目");
        return;
    }

    collectCurrentParams();

    ParamManager* pm = ParamManager::instance();
    pm->updateParamValues(projectName, m_paramPanel.currentValues);

    // ===== 新增：保存参数到JSON文件 =====
    if (pm->saveProjectParamsToJson(projectName)) {
        qDebug() << "[DetectionDisplay] 参数已保存到JSON:" << projectName;
        QMessageBox::information(this, "成功",
                                 QString("参数已保存\n项目: %1\n配置文件: %2_params.json")
                                     .arg(projectName).arg(projectName));
    } else {
        QMessageBox::warning(this, "警告",
                             QString("参数保存成功，但JSON文件写入失败\n项目: %1")
                                 .arg(projectName));
    }
}

// ==================== 检测触发 ====================

void DetectionDisplayDialog::onTriggerDetection(const QString& projectName, const QImage& image)
{
    qDebug() << "[DetectionDisplay] 触发检测:" << projectName;

    //检查配置表中是否存在对应的检测项目，然后在对其进行操作！
    if (!m_projectNameToIndex.contains(projectName)) {
        qDebug() << "[DetectionDisplay] 项目不存在:" << projectName;
        return;
    }

    DetectionTask task;
    task.groupId = getgroupIdByName(projectName);
    task.projectName =  projectName; //群组ID吗
    //增加一个显示序号，但是不需要，后续能够通过直接查找！
    task.image = image;


    if (m_detectionWorker) {
        m_detectionWorker->addTask(task);
    }
}

void DetectionDisplayDialog::onDetectionFinished(const DetectionResult& result)
{
    QString projectName = result.projectName;

    if (projectName.isEmpty()) {
        QString projectName = getProjectNameById(result.groupId);
        qDebug() << "[DetectionDisplay] 未知项目ID:" << result.groupId;
        if(projectName.isEmpty())
            return;
    }

    if (!result.success || result.resultImage.isNull()) {
        if (m_projectNameToIndex.contains(projectName)) {
            int index = m_projectNameToIndex[projectName];
            if (index >= 0 && index < m_displayItems.size()) {
                m_displayItems[index].imageLabel->setText("检测失败");
                m_displayItems[index].hasImage = false;
            }
        }

        // 批量检测失败也记录
        if (m_batchDetectionCount > 0) {
            qDebug() << "[Batch] 检测失败: 项目=" << projectName;
        }
        return;
    }

    // ===== 核心修改：实时更新UI =====
    QPixmap pixmap = QPixmap::fromImage(result.resultImage);
    updateImageDisplay(projectName, pixmap);

    // 强制立即更新UI
    QApplication::processEvents();

    // 批量检测进度提示（每5张或最后一张时输出）
    if (m_batchDetectionCount > 0) {
        static int lastLogCount = 0;
        int currentCount = m_batchDetectionCount;

        if (currentCount % 5 == 0 || currentCount == m_batchTotalCount) {
            qDebug() << "[Batch] 检测进度:" << currentCount << "/" << m_batchTotalCount;
            lastLogCount = currentCount;
        }
    }

    qDebug() << "[DetectionDisplay] 已更新结果:" << projectName;
}

void DetectionDisplayDialog::onDebugInfo(const QString& info)
{
    qDebug() << "[DetectionWorker]" << info;
}

void DetectionDisplayDialog::onManualDetect()
{
    QString imagePath = QFileDialog::getOpenFileName(
        this,
        "选择测试图像",
        "",
        "图像文件 (*.png *.jpg *.jpeg *.bmp)"
        );

    if (imagePath.isEmpty()) return;

    QImage image(imagePath);
    if (image.isNull()) {
        QMessageBox::warning(this, "错误", "无法加载图像");
        return;
    }

    if (m_configs.isEmpty()) {
        QMessageBox::warning(this, "提示", "没有检测配置");
        return;
    }

    onTriggerDetection(m_configs[0].projectName, image);
}

// ==================== 辅助函数 ====================

int DetectionDisplayDialog::getgroupIdByName(const QString& projectName)
{
    for (const DetectionConfig& config : m_configs) {
        if (config.projectName == projectName) {
            return config.groupId;
        }
    }
    return -1;
}

//获取序号通过name,显示的排序
int DetectionDisplayDialog::getNumberByName(const QString& projectName)
{
    for (const DetectionConfig& config : m_configs) {
        if (config.projectName == projectName) {
            return config.id; //序号
        }
    }
    return -1;
}

QString DetectionDisplayDialog::getProjectNameById(int projectId)
{
    for (const DetectionConfig& config : m_configs) {
        if (config.groupId == projectId) {
            return config.projectName;
        }
    }
    return QString();
}

QString DetectionDisplayDialog::getCameraNameByProject(const QString& projectName)
{
    for (const DetectionConfig& config : m_configs) {
        if (config.projectName == projectName) {
            return QString("Camera_%1").arg(config.cameraId);
        }
    }
    return "Unknown";
}

QString DetectionDisplayDialog::getButtonStyle(const QString& color)
{
    return QString(
               "QPushButton {"
               "    background-color: %1;"
               "    color: white;"
               "    border: none;"
               "    border-radius: 5px;"
               "    padding: 8px 15px;"
               "    font-weight: bold;"
               "}"
               "QPushButton:hover { background-color: %2; }"
               "QPushButton:pressed { background-color: %3; }"
               ).arg(color).arg(color).arg(color);
}

void DetectionDisplayDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
}

void DetectionDisplayDialog::updateResult(const DetectionResult& result)
{
    if (!result.success || result.resultImage.isNull()) {
        qDebug() << "[DetectionDisplay] 检测结果无效";
        return;
    }

    QString projectName = getProjectNameById(result.groupId);
    if (projectName.isEmpty()) {
        qDebug() << "[DetectionDisplay] 未知项目ID:" << result.groupId;
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(result.resultImage);
    updateImageDisplay(projectName, pixmap);

    qDebug() << "[DetectionDisplay] 已更新结果:" << projectName;
}