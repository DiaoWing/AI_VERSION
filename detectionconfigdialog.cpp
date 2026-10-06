#include "detectionconfigdialog.h"
#include "parammanager.h"
#include "ConfigManager.h"  // 添加头文件
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QPainter>
#include <QDir>
#include <QImageReader>
#include <QProgressDialog>
#include <QApplication>
#include <QDateTime>

// DetectionConfigDialog 实现
DetectionConfigDialog::DetectionConfigDialog(QWidget *parent)
    : QDialog(parent)
    , m_loadedFromManager(false)
{
    // 初始化选项
    m_channelOptions = {"不使用", "触发", "常亮"};
    m_cameraOptions = {"1", "2", "3", "4"};
    m_controllerOptions = {"1", "2", "3", "4"};
    m_calibratedOptions = {"否", "是"};
    m_groupOptions = {"1", "2", "3", "4", "5", "6"};
    //初始化当前可以执行的检测项目，此处填充之后，就可以添加到对应的配置中
    m_projectOptions = {"空项目", "海岸线检测"};

    setupUI();
    setupTable();

    // 创建右键菜单
    m_contextMenu = new QMenu(this);
    m_detectSingleAction = new QAction("检测单张图像", this);
    m_detectFolderAction = new QAction("检测文件夹", this);
    m_contextMenu->addAction(m_detectSingleAction);
    m_contextMenu->addAction(m_detectFolderAction);

    connect(m_detectSingleAction, &QAction::triggered, this, &DetectionConfigDialog::onDetectSingleImage);
    connect(m_detectFolderAction, &QAction::triggered, this, &DetectionConfigDialog::onDetectFolder);
    connect(m_tableWidget, &QTableWidget::customContextMenuRequested,
            this, &DetectionConfigDialog::onCustomContextMenuRequested);

    // 加载保存的配置
    //从文件中加载才行
    loadFromConfigManager();
}

DetectionConfigDialog::~DetectionConfigDialog()
{
}

void DetectionConfigDialog::setupUI()
{
    setWindowTitle("检测配置管理");
    setModal(true);
    resize(850, 600);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    m_tableWidget = new QTableWidget(this);
    mainLayout->addWidget(m_tableWidget);

    QHBoxLayout* buttonLayout = new QHBoxLayout();

    QPushButton* addBtn = new QPushButton("添加", this);
    QPushButton* deleteBtn = new QPushButton("删除", this);
    QPushButton* clearBtn = new QPushButton("清空", this);
    QPushButton* saveBtn = new QPushButton("保存配置", this);
    QPushButton* loadBtn = new QPushButton("加载配置", this);
    QPushButton* confirmBtn = new QPushButton("确认", this);
    QPushButton* cancelBtn = new QPushButton("取消", this);

    addBtn->setFixedSize(80, 30);
    deleteBtn->setFixedSize(80, 30);
    clearBtn->setFixedSize(80, 30);
    saveBtn->setFixedSize(90, 30);
    loadBtn->setFixedSize(90, 30);
    confirmBtn->setFixedSize(80, 30);
    cancelBtn->setFixedSize(80, 30);

    buttonLayout->addWidget(addBtn);
    buttonLayout->addWidget(deleteBtn);
    buttonLayout->addWidget(clearBtn);
    buttonLayout->addSpacing(20);
    buttonLayout->addWidget(saveBtn);
    buttonLayout->addWidget(loadBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(confirmBtn);
    buttonLayout->addWidget(cancelBtn);

    mainLayout->addLayout(buttonLayout);

    connect(addBtn, &QPushButton::clicked, this, &DetectionConfigDialog::onAddClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &DetectionConfigDialog::onDeleteClicked);
    connect(clearBtn, &QPushButton::clicked, this, &DetectionConfigDialog::onClearClicked);
    connect(saveBtn, &QPushButton::clicked, this, &DetectionConfigDialog::onSaveClicked);
    connect(loadBtn, &QPushButton::clicked, this, &DetectionConfigDialog::onLoadClicked);
    connect(confirmBtn, &QPushButton::clicked, this, &DetectionConfigDialog::onConfirmClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &DetectionConfigDialog::onCancelClicked);
}

void DetectionConfigDialog::setupTable()
{
    QStringList headers = {
        "序号", "相机号", "检测项目名称", "灯光控制器",
        "通道1", "通道2", "通道3", "通道4", "标定", "群组ID"
    };

    m_tableWidget->setColumnCount(headers.size());
    m_tableWidget->setHorizontalHeaderLabels(headers);

    m_tableWidget->verticalHeader()->setVisible(false);
    m_tableWidget->setShowGrid(false);

    // 启用右键菜单
    m_tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    m_tableWidget->setStyleSheet(
        "QTableWidget {"
        "    gridline-color: transparent;"
        "    outline: none;"
        "    border: none;"
        "}"
        "QTableWidget::item {"
        "    border: none;"
        "    padding: 2px;"
        "}"
        "QTableWidget::item:selected {"
        "    background: #3399FF;"
        "    color: white;"
        "}"
        "QHeaderView::section {"
        "    border: none;"
        "    padding: 4px;"
        "}"
        );

    m_tableWidget->setColumnWidth(0, 50);
    m_tableWidget->setColumnWidth(1, 80);
    m_tableWidget->setColumnWidth(2, 120);
    m_tableWidget->setColumnWidth(3, 100);
    m_tableWidget->setColumnWidth(4, 80);
    m_tableWidget->setColumnWidth(5, 80);
    m_tableWidget->setColumnWidth(6, 80);
    m_tableWidget->setColumnWidth(7, 80);
    m_tableWidget->setColumnWidth(8, 80);
    m_tableWidget->setColumnWidth(9, 80);

    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QHeaderView* header = m_tableWidget->horizontalHeader();
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void DetectionConfigDialog::addRow(const DetectionConfig& config)
{
    int row = m_tableWidget->rowCount();
    m_tableWidget->insertRow(row);

    QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(row + 1));
    idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
    idItem->setTextAlignment(Qt::AlignCenter);
    m_tableWidget->setItem(row, 0, idItem);

    setCellComboBox(row, 1, m_cameraOptions, qBound(0, config.cameraId - 1, m_cameraOptions.size() - 1));

    int projectIndex = m_projectOptions.indexOf(config.projectName);
    if (projectIndex < 0) projectIndex = 0;
    setCellComboBox(row, 2, m_projectOptions, qBound(0, projectIndex, m_projectOptions.size() - 1));

    setCellComboBox(row, 3, m_controllerOptions, qBound(0, config.lightController - 1, m_controllerOptions.size() - 1));
    setCellComboBox(row, 4, m_channelOptions, qBound(0, config.channel1, m_channelOptions.size() - 1));
    setCellComboBox(row, 5, m_channelOptions, qBound(0, config.channel2, m_channelOptions.size() - 1));
    setCellComboBox(row, 6, m_channelOptions, qBound(0, config.channel3, m_channelOptions.size() - 1));
    setCellComboBox(row, 7, m_channelOptions, qBound(0, config.channel4, m_channelOptions.size() - 1));
    setCellComboBox(row, 8, m_calibratedOptions, config.calibrated ? 1 : 0);
    setCellComboBox(row, 9, m_groupOptions, qBound(0, config.groupId - 1, m_groupOptions.size() - 1));
}

void DetectionConfigDialog::setCellComboBox(int row, int col, const QStringList& items, int index)
{
    int validIndex = qBound(0, index, items.size() - 1);

    QComboBox* comboBox = new QComboBox(m_tableWidget);
    comboBox->addItems(items);
    comboBox->setCurrentIndex(validIndex);
    comboBox->setEditable(false);
    comboBox->setFocusPolicy(Qt::NoFocus);

    comboBox->setStyleSheet(
        "QComboBox {"
        "    border: none;"
        "    background: transparent;"
        "    outline: none;"
        "    padding: 2px 18px 2px 5px;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 16px;"
        "}"
        "QComboBox::down-arrow {"
        "    width: 10px;"
        "    height: 10px;"
        "}"
        );

    m_tableWidget->setCellWidget(row, col, comboBox);
}

DetectionConfig DetectionConfigDialog::getRowData(int row) const
{
    DetectionConfig config;
    config.id = row + 1;

    QComboBox* cameraCombo = qobject_cast<QComboBox*>(m_tableWidget->cellWidget(row, 1));
    if (cameraCombo) {
        config.cameraId = cameraCombo->currentText().toInt();
    }

    QComboBox* projectCombo = qobject_cast<QComboBox*>(m_tableWidget->cellWidget(row, 2));
    if (projectCombo) {
        config.projectName = projectCombo->currentText();
    }

    QComboBox* controllerCombo = qobject_cast<QComboBox*>(m_tableWidget->cellWidget(row, 3));
    if (controllerCombo) {
        config.lightController = controllerCombo->currentText().toInt();
    }

    for (int col = 4; col <= 7; col++) {
        QComboBox* channelCombo = qobject_cast<QComboBox*>(m_tableWidget->cellWidget(row, col));
        int value = channelCombo ? channelCombo->currentIndex() : 0;

        switch(col) {
        case 4: config.channel1 = value; break;
        case 5: config.channel2 = value; break;
        case 6: config.channel3 = value; break;
        case 7: config.channel4 = value; break;
        }
    }

    QComboBox* calibratedCombo = qobject_cast<QComboBox*>(m_tableWidget->cellWidget(row, 8));
    if (calibratedCombo) {
        config.calibrated = calibratedCombo->currentIndex() == 1;
    }

    QComboBox* groupCombo = qobject_cast<QComboBox*>(m_tableWidget->cellWidget(row, 9));
    if (groupCombo) {
        config.groupId = groupCombo->currentText().toInt();
    }

    return config;
}

void DetectionConfigDialog::updateRowNumbers()
{
    for (int i = 0; i < m_tableWidget->rowCount(); i++) {
        QTableWidgetItem* item = m_tableWidget->item(i, 0);
        if (item) {
            item->setText(QString::number(i + 1));
        }
    }
}

// ========== ConfigManager 集成 ==========
bool DetectionConfigDialog::saveToConfigManager()
{
    try {
        ConfigManager* config = ConfigManager::instance();
        if (!config) {
            qDebug() << "[DetectionConfigDialog] ConfigManager实例为空";
            return false;
        }

        // 收集所有配置
        QList<DetectionConfig> configs;
        for (int i = 0; i < m_tableWidget->rowCount(); i++) {
            configs.append(getRowData(i));
        }

        // ❌ 不要调用 setDetectionConfigs（会触发死锁）
        // config->setDetectionConfigs(configs);

        // ✅ 直接保存到文件（使用 appendDetectionConfigsToFile）
        QString currentPath = config->getConfigFilePath();
        if (currentPath.isEmpty()) {
            qDebug() << "[DetectionConfigDialog] 没有配置文件路径";
            return false;
        }

        bool success = config->appendDetectionConfigsToFile(currentPath, configs);
        if (success) {
            qDebug() << "[DetectionConfigDialog] 保存了" << configs.size() << "条配置到文件";
        }
        return success;

    } catch (const std::exception& e) {
        qDebug() << "[DetectionConfigDialog] 保存配置异常:" << e.what();
        return false;
    } catch (...) {
        qDebug() << "[DetectionConfigDialog] 保存配置未知异常";
        return false;
    }
}
bool DetectionConfigDialog::loadFromConfigManager()
{
    try {
        ConfigManager* config = ConfigManager::instance();
        if (!config) {
            qDebug() << "[DetectionConfigDialog] ConfigManager实例为空";
            return false;
        }

        QString path = config->getConfigFilePath();
        QList<DetectionConfig> configs = config->loadDetectionConfigsFromFile(path);

        if (configs.isEmpty()) {
            qDebug() << "[DetectionConfigDialog] ConfigManager中没有检测配置";
            return false;
        }

        // 清空表格
        m_tableWidget->setRowCount(0);

        // 加载配置
        for (const DetectionConfig& c : configs) {
            addRow(c);
        }
        updateRowNumbers();

        m_loadedFromManager = true;
        qDebug() << "[DetectionConfigDialog] 从ConfigManager加载了" << configs.size() << "条配置";

        // 同时加载项目参数到ParamManager
        ParamManager* pm = ParamManager::instance();
        if (pm) {
            for (const DetectionConfig& c : configs) {
                if (!c.projectName.isEmpty() && c.projectName != "空项目") {
                    QJsonObject params = config->getProjectParams(c.projectName);
                    if (!params.isEmpty()) {
                        // 将参数注册到ParamManager
                        QList<ParamDefinition> defs;
                        for (const QString& key : params.keys()) {
                            ParamDefinition def;
                            def.name = key;
                            def.displayName = key;
                            QJsonValue value = params[key];
                            if (value.isDouble()) {
                                def.type = "double";
                                def.defaultValue = value.toDouble();
                            } else if (value.isBool()) {
                                def.type = "bool";
                                def.defaultValue = value.toBool();
                            } else if (value.isString()) {
                                def.type = "string";
                                def.defaultValue = value.toString();
                            } else {
                                def.type = "int";
                                def.defaultValue = value.toInt();
                            }
                            defs.append(def);
                        }

                        ProjectParams projectParams;
                        projectParams.projectName = c.projectName;
                        projectParams.paramDefs = defs;
                        pm->registerProject(projectParams);
                    }
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        qDebug() << "[DetectionConfigDialog] 加载配置异常:" << e.what();
        return false;
    } catch (...) {
        qDebug() << "[DetectionConfigDialog] 加载配置未知异常";
        return false;
    }
}



// ========== 右键菜单相关函数 ==========

void DetectionConfigDialog::onCustomContextMenuRequested(const QPoint& pos)
{
    QTableWidgetItem* item = m_tableWidget->itemAt(pos);
    if (item) {
        m_contextMenu->exec(m_tableWidget->viewport()->mapToGlobal(pos));
    }
}

void DetectionConfigDialog::onDetectSingleImage()
{
    QString imagePath = QFileDialog::getOpenFileName(
        this,
        "选择要检测的图像",
        "",
        "图像文件 (*.png *.jpg *.jpeg *.bmp *.tiff)"
        );

    if (!imagePath.isEmpty()) {
        processImage(imagePath);
    }
}

void DetectionConfigDialog::onDetectFolder()
{
    QString folderPath = QFileDialog::getExistingDirectory(
        this,
        "选择包含图像的文件夹",
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    if (folderPath.isEmpty()) {
        return;
    }

    // 获取文件夹中所有图像文件
    QDir dir(folderPath);
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.tiff" << "*.tif";
    QStringList imageFiles = dir.entryList(filters, QDir::Files);

    if (imageFiles.isEmpty()) {
        QMessageBox::information(this, "提示", "文件夹中没有找到图像文件");
        return;
    }

    // 创建进度对话框
    QProgressDialog progress("正在检测图像...", "取消", 0, imageFiles.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);

    // 依次检测每张图像
    for (int i = 0; i < imageFiles.size(); i++) {
        if (progress.wasCanceled()) {
            break;
        }

        progress.setValue(i);
        progress.setLabelText(QString("正在检测: %1 (%2/%3)")
                                  .arg(imageFiles[i])
                                  .arg(i + 1)
                                  .arg(imageFiles.size()));

        QString fullPath = dir.absoluteFilePath(imageFiles[i]);
        processImage(fullPath);

        // 处理事件，保持界面响应
        QApplication::processEvents();
    }

    progress.setValue(imageFiles.size());
}

// ========== 检测核心函数 ==========

QPixmap DetectionConfigDialog::performDetection(const QString& imagePath, const DetectionConfig& config)
{
    QPixmap original(imagePath);
    if (original.isNull()) {
        return QPixmap();
    }

    // 创建副本用于绘制
    QPixmap result = original;
    QPainter painter(&result);

    // 设置画笔
    painter.setPen(QPen(Qt::red, 3));
    painter.setBrush(Qt::NoBrush);

    // 绘制检测框
    int width = result.width();
    int height = result.height();
    int margin = qMin(width, height) / 8;
    QRect rect(margin, margin, width - 2 * margin, height - 2 * margin);
    painter.drawRect(rect);

    // 画十字线
    painter.drawLine(width/2, margin, width/2, height - margin);
    painter.drawLine(margin, height/2, width - margin, height/2);

    // 在四个角画圆
    painter.setPen(QPen(Qt::green, 2));
    int radius = 30;
    QPoint corners[4] = {
        QPoint(margin + radius, margin + radius),
        QPoint(width - margin - radius, margin + radius),
        QPoint(margin + radius, height - margin - radius),
        QPoint(width - margin - radius, height - margin - radius)
    };
    for (const QPoint& p : corners) {
        painter.drawEllipse(p, radius, radius);
    }

    // 在中心显示项目名称
    painter.setPen(QPen(Qt::blue, 2));
    QFont font = painter.font();
    font.setPointSize(24);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter, config.projectName);

    // 在左上角添加信息
    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(QColor(255, 255, 255, 180));
    QRect infoRect(10, 10, 280, 80);
    painter.drawRect(infoRect);

    painter.setPen(QPen(Qt::black, 1));
    painter.setFont(QFont("Arial", 10));
    QString info = QString("项目: %1\n相机: %2")
                       .arg(config.projectName)
                       .arg(config.cameraId);
    painter.drawText(infoRect.adjusted(5, 5, -5, -5), info);

    painter.end();
    return result;
}

void DetectionConfigDialog::processImage(const QString& imagePath)
{
    // 检查是否选中了配置行
    int currentRow = m_tableWidget->currentRow();
    if (currentRow < 0) {
        QMessageBox::warning(this, "提示", "请先在表格中选择要使用的配置行");
        return;
    }

    // 获取当前行配置
    DetectionConfig config = getRowData(currentRow);

    // 执行检测
    QPixmap result = performDetection(imagePath, config);

    if (result.isNull()) {
        QMessageBox::warning(this, "错误", "无法处理图像: " + imagePath);
        return;
    }

    // 显示结果（简单显示在消息框中）
    QString fileName = QFileInfo(imagePath).fileName();
    QString resultPath = QFileInfo(imagePath).path() + "/result_" + fileName;
    result.save(resultPath);

    QMessageBox::information(this, "检测完成",
                             QString("检测完成！\n项目: %1\n图像: %2\n结果保存至: %3")
                                 .arg(config.projectName)
                                 .arg(fileName)
                                 .arg(resultPath));
}

// ========== 按钮槽函数 ==========

void DetectionConfigDialog::onAddClicked()
{
    addRow();
}

void DetectionConfigDialog::onDeleteClicked()
{
    int row = m_tableWidget->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "提示", "请先选择要删除的行");
        return;
    }
    if (QMessageBox::question(this, "确认", "确定删除选中的行？") == QMessageBox::Yes) {
        m_tableWidget->removeRow(row);
        updateRowNumbers();
    }
}

void DetectionConfigDialog::onClearClicked()
{
    if (QMessageBox::question(this, "确认", "确定清空所有数据？") == QMessageBox::Yes) {
        m_tableWidget->setRowCount(0);
    }
}

void DetectionConfigDialog::onSaveClicked()
{
    // 保存当前表格数据到内存
    saveTableToConfigs();

    // 更新到 ConfigManager
    ConfigManager* cm = ConfigManager::instance();

    if (m_configs.isEmpty()) {
        return;
    }

    // ===== 获取当前记录的路径 =====
    QString currentPath = cm->getConfigFilePath();
    cm->appendDetectionConfigsToFile(currentPath, m_configs); //追加的函数
}

void DetectionConfigDialog::onLoadClicked()
{
    ConfigManager* cm = ConfigManager::instance();

    QString currentPath = cm->getConfigFilePath();

    loadFromFile(currentPath);
    // QString filePath = QFileDialog::getOpenFileName(
    //     this,
    //     tr("导入检测配置"),
    //     QDir::currentPath(),
    //     tr("JSON文件 (*.json);;所有文件 (*.*)")
    //     );

    // if (filePath.isEmpty()) {
    //     return;
    // }

    // // 使用 ConfigManager 导入
    // ConfigManager* cm = ConfigManager::instance();
    // bool success = cm->importDetectionConfigs(filePath);

    // if (success) {
    //     // 更新显示
    //     m_configs = cm->getDetectionConfigs();
    //     updateTable();

    //     QMessageBox::information(this, tr("成功"),
    //                              tr("检测配置导入成功！共加载 %1 个配置。").arg(m_configs.size()));
    // } else {
    //     QMessageBox::warning(this, tr("错误"),
    //                          tr("导入检测配置失败！请检查文件格式是否正确。"));
    // }
}

void DetectionConfigDialog::onConfirmClicked()
{
    // ✅ 只关闭对话框，不保存配置
    qDebug() << "[DetectionConfigDialog] 用户点击确认，关闭对话框";
    accept();  // 直接关闭，返回 QDialog::Accepted
}

void DetectionConfigDialog::onCancelClicked()
{
    // ✅ 只关闭对话框，不保存配置
    qDebug() << "[DetectionConfigDialog] 用户点击取消，关闭对话框";
    reject();  // 直接关闭，返回 QDialog::Rejected
}


// ========== 文件保存/加载（保留作为备用） ==========

bool DetectionConfigDialog::saveToFile(const QString& filePath)
{
    QJsonArray arr;
    for (int i = 0; i < m_tableWidget->rowCount(); i++) {
        DetectionConfig c = getRowData(i);
        QJsonObject obj;
        obj["id"] = c.id;
        obj["cameraId"] = c.cameraId;
        obj["projectName"] = c.projectName;
        obj["lightController"] = c.lightController;
        obj["channel1"] = c.channel1;
        obj["channel2"] = c.channel2;
        obj["channel3"] = c.channel3;
        obj["channel4"] = c.channel4;
        obj["calibrated"] = c.calibrated;
        obj["groupId"] = c.groupId;
        arr.append(obj);
    }

    QJsonObject root;
    root["version"] = "1.0";
    root["configs"] = arr;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "错误", "无法保存文件: " + file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();

    QMessageBox::information(this, "成功", QString("已保存 %1 条配置").arg(arr.size()));
    return true;
}

bool DetectionConfigDialog::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法打开文件: " + file.errorString());
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isNull()) {
        QMessageBox::warning(this, "错误", "无效的JSON格式");
        return false;
    }

    QJsonObject root = doc.object();
    QJsonArray arr;

    // 尝试两种格式
    if (root.contains("detectionConfigs")) {
        // 格式1: {"detectionConfigs": {"configs": [...]}}
        QJsonObject detectionObj = root["detectionConfigs"].toObject();
        if (detectionObj.contains("configs")) {
            arr = detectionObj["configs"].toArray();
        }
    } else if (root.contains("configs")) {
        // 格式2: {"configs": [...]}
        arr = root["configs"].toArray();
    } else {
        QMessageBox::warning(this, "错误", "无法识别的文件格式");
        return false;
    }

    if (arr.isEmpty()) {
        QMessageBox::warning(this, "错误", "没有配置数据");
        return false;
    }

    // 清空表格
    m_tableWidget->setRowCount(0);

    // 加载数据
    for (const QJsonValue& v : arr) {
        QJsonObject obj = v.toObject();
        DetectionConfig c;
        c.id = obj["id"].toInt();
        c.cameraId = obj["cameraId"].toInt();
        c.projectName = obj["projectName"].toString();
        c.lightController = obj["lightController"].toInt();
        c.channel1 = obj["channel1"].toInt();
        c.channel2 = obj["channel2"].toInt();
        c.channel3 = obj["channel3"].toInt();
        c.channel4 = obj["channel4"].toInt();
        c.calibrated = obj["calibrated"].toBool();
        c.groupId = obj["groupId"].toInt();
        addRow(c);
    }
    updateRowNumbers();

    // 更新内存中的配置
    m_configs.clear();
    for (int i = 0; i < m_tableWidget->rowCount(); i++) {
        m_configs.append(getRowData(i));
    }

    QMessageBox::information(this, "成功", QString("已加载 %1 条配置").arg(arr.size()));
    return true;
}

void DetectionConfigDialog::setConfigs(const QList<DetectionConfig>& configs)
{
    m_tableWidget->setRowCount(0);
    for (const DetectionConfig& c : configs) {
        addRow(c);
    }
    updateRowNumbers();
}

// 辅助函数：保存表格数据到内存
void DetectionConfigDialog::saveTableToConfigs()
{
    m_configs.clear();
    for (int row = 0; row < m_tableWidget->rowCount(); ++row) {
        m_configs.append(getRowData(row));
    }
}

// 辅助函数：更新表格显示
void DetectionConfigDialog::updateTable()
{
    m_tableWidget->setRowCount(0);
    for (const DetectionConfig& config : m_configs) {
        addRow(config);
    }
    updateRowNumbers();
}