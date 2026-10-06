#include "modbusslavedialog.h"
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QFont>
#include <QScrollBar>
#include <QApplication>
#include <QScreen>
#include <QFontDatabase>
#include "configmanager.h"

// ============================================================
// ModbusSlaveWorker 实现
// ============================================================

ModbusSlaveWorker::ModbusSlaveWorker(QObject* parent)
    : QObject(parent)
    , m_serialPort(nullptr)
    , m_isRunning(false)
    , m_readTimer(nullptr)
{
    // 默认配置已经在结构体中初始化
}

ModbusSlaveWorker::~ModbusSlaveWorker()
{
    stop();
    if (m_serialPort) {
        delete m_serialPort;
    }
    if (m_readTimer) {
        delete m_readTimer;
    }
}

void ModbusSlaveWorker::setConfig(const ModbusConfig& config)
{
    m_config = config;
}

void ModbusSlaveWorker::setConfig(const QString& portName, int baudRate,
                                  QSerialPort::DataBits dataBits,
                                  QSerialPort::StopBits stopBits,
                                  QSerialPort::Parity parity,
                                  quint8 slaveId)
{
    m_config.portName = portName;
    m_config.baudRate = baudRate;
    m_config.dataBits = dataBits;
    m_config.stopBits = stopBits;
    m_config.parity = parity;
    m_config.slaveId = slaveId;
}

void ModbusSlaveWorker::start()
{
    if (m_isRunning) {
        emit debugInfo("从站已在运行", 2);
        return;
    }

    if (!m_config.isValid()) {
        emit errorOccurred("配置无效：" + m_config.toString());
        return;
    }

    if (!m_serialPort) {
        m_serialPort = new QSerialPort();
    }

    m_serialPort->setPortName(m_config.portName);
    m_serialPort->setBaudRate(m_config.baudRate);
    m_serialPort->setDataBits(m_config.dataBits);
    m_serialPort->setStopBits(m_config.stopBits);
    m_serialPort->setParity(m_config.parity);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        QString err = QString("打开串口失败: %1").arg(m_serialPort->errorString());
        emit errorOccurred(err);
        return;
    }

    m_isRunning = true;

    if (!m_readTimer) {
        m_readTimer = new QTimer(this);
        connect(m_readTimer, &QTimer::timeout, this, &ModbusSlaveWorker::readData);
    }
    m_readTimer->start(10);

    emit debugInfo(QString("从站启动成功 | %1").arg(m_config.toString()), 0);
    emit started();
}

void ModbusSlaveWorker::stop()
{
    if (!m_isRunning) {
        return;
    }

    if (m_readTimer) {
        m_readTimer->stop();
    }

    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
    }

    m_isRunning = false;
    m_buffer.clear();

    emit debugInfo("从站已停止", 0);
    emit stopped();
}

void ModbusSlaveWorker::readData()
{
    if (!m_serialPort || !m_serialPort->isOpen() || !m_isRunning) {
        return;
    }

    QByteArray data = m_serialPort->readAll();
    if (data.isEmpty()) {
        return;
    }

    m_buffer.append(data);
    emit debugInfo(QString("接收: %1").arg(QString(data.toHex(' '))), 1);

    // 处理完整帧 - 只支持功能码 0x03 和 0x06
    while (m_buffer.size() >= 4) {
        // 检查从站ID
        if ((quint8)m_buffer[0] != m_config.slaveId && (quint8)m_buffer[0] != 0) {
            m_buffer.remove(0, 1);
            continue;
        }

        if (m_buffer.size() < 4) {
            break;
        }

        quint8 functionCode = (quint8)m_buffer[1];
        int expectedLen = 0;

        // 只支持功能码 0x03(读寄存器) 和 0x06(写单个寄存器)
        switch(functionCode) {
        case 0x03:  // 读保持寄存器
            expectedLen = 8;  // 地址(2) + 数量(2) + CRC(2) = 6，但加上前面的ID和功能码共8字节
            break;
        case 0x06:  // 写单个寄存器
            expectedLen = 8;
            break;
        default:
            // 不支持的功能码，直接移除
            emit debugInfo(QString("不支持的功能码: 0x%1").arg(functionCode, 2, 16, QChar('0')), 2);
            m_buffer.remove(0, 1);
            continue;
        }

        if (m_buffer.size() < expectedLen) {
            break;
        }

        QByteArray request = m_buffer.left(expectedLen);
        m_buffer.remove(0, expectedLen);

        // 测试模式：跳过CRC校验
        // 正式使用时取消注释下面的CRC校验
        /*
        if (!verifyCRC(request)) {
            emit debugInfo("CRC校验失败", 2);
            continue;
        }
        */

        processRequest(request);
    }
}

void ModbusSlaveWorker::processRequest(const QByteArray& request)
{
    quint8 functionCode = (quint8)request[1];
    QByteArray response;

    // 只处理功能码 0x03 和 0x06
    if (functionCode == 0x03) {
        // 读保持寄存器
        quint16 startAddr = ((quint8)request[2] << 8) | (quint8)request[3];
        quint16 count = ((quint8)request[4] << 8) | (quint8)request[5];

        emit debugInfo(QString("读寄存器: 地址=0x%1, 数量=%2")
                           .arg(startAddr, 4, 16, QChar('0'))
                           .arg(count), 0);

        // 限制读取数量
        if (count > 125) {
            count = 125;
        }

        QByteArray data;
        data.append((char)(count * 2));  // 数据字节数

        for (int i = 0; i < count; i++) {
            quint16 value = m_registers.value(startAddr + i, 0);
            data.append((char)(value >> 8));
            data.append((char)(value & 0xFF));
        }

        response = buildResponse(m_config.slaveId, 0x03, data);

    } else if (functionCode == 0x06) {
        // 写单个寄存器
        quint16 address = ((quint8)request[2] << 8) | (quint8)request[3];
        quint16 value = ((quint8)request[4] << 8) | (quint8)request[5];

        m_registers[address] = value;
        emit debugInfo(QString("写寄存器: 地址=0x%1, 值=0x%2")
                           .arg(address, 4, 16, QChar('0'))
                           .arg(value, 4, 16, QChar('0')), 0);

        // ===== 如果写入的是0号寄存器，发送信号 =====
        //接受零号寄存器的信号，进行对应的操作
        if (address == 0) {
            emit registerChanged(address, value);
        }

        response = request;  // 回显请求
    }

    // 发送响应
    if (!response.isEmpty()) {
        if (m_serialPort && m_serialPort->isOpen()) {
            m_serialPort->write(response);
            m_serialPort->flush();
            emit debugInfo(QString("发送响应: %1")
                               .arg(QString(response.toHex(' '))), 1);
        }
    }
}

QByteArray ModbusSlaveWorker::buildResponse(quint8 slaveId, quint8 functionCode, const QByteArray& data)
{
    QByteArray response;
    response.append((char)slaveId);
    response.append((char)functionCode);
    response.append(data);

    quint16 crc = calculateCRC(response);
    response.append((char)(crc & 0xFF));
    response.append((char)(crc >> 8));

    return response;
}

QByteArray ModbusSlaveWorker::buildErrorResponse(quint8 slaveId, quint8 functionCode, quint8 exceptionCode)
{
    QByteArray response;
    response.append((char)slaveId);
    response.append((char)(functionCode | 0x80));
    response.append((char)exceptionCode);

    quint16 crc = calculateCRC(response);
    response.append((char)(crc & 0xFF));
    response.append((char)(crc >> 8));

    return response;
}

quint16 ModbusSlaveWorker::calculateCRC(const QByteArray& data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); i++) {
        crc ^= (quint8)data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return crc;
}

bool ModbusSlaveWorker::verifyCRC(const QByteArray& data)
{
    if (data.size() < 2) return false;
    QByteArray withoutCRC = data.left(data.size() - 2);
    quint16 calcCRC = calculateCRC(withoutCRC);
    quint16 recvCRC = (quint8)data[data.size() - 2] | ((quint8)data[data.size() - 1] << 8);
    return calcCRC == recvCRC;
}

// ============================================================
// modbusSlaveDialog 实现
// ============================================================

modbusSlaveDialog::modbusSlaveDialog(QWidget *parent)
    : QDialog(parent)
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_isConnected(false)
    , m_isDialogShown(false)
{
    setWindowTitle("Modbus RTU 从站");
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);

    setupUI();
    refreshPorts();

    addDebugInfo("Modbus 从站对话框已启动", 0);
    addDebugInfo("只支持功能码: 0x03(读寄存器), 0x06(写寄存器)", 0);
}

modbusSlaveDialog::~modbusSlaveDialog()
{
    if (m_worker) {
        m_worker->stop();
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
    }
}

void modbusSlaveDialog::setupUI()
{
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    createConfigGroup();
    createControlGroup();
    createDebugGroup();
    createStatusBar();

    mainLayout->addWidget(m_configGroup);
    mainLayout->addWidget(m_controlGroup);
    mainLayout->addWidget(m_debugGroup, 1);
    mainLayout->addWidget(m_statusLabel);

    resize(750, 600);

    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
}

void modbusSlaveDialog::createConfigGroup()
{
    m_configGroup = new QGroupBox("串口配置", this);
    QGridLayout* layout = new QGridLayout(m_configGroup);
    layout->setSpacing(8);

    QLabel* labelPort = new QLabel("串口号:", m_configGroup);
    m_comboPort = new QComboBox(m_configGroup);
    m_comboPort->setMinimumWidth(150);
    layout->addWidget(labelPort, 0, 0);
    layout->addWidget(m_comboPort, 0, 1);

    QLabel* labelId = new QLabel("从站ID:", m_configGroup);
    m_spinSlaveId = new QSpinBox(m_configGroup);
    m_spinSlaveId->setRange(1, 247);
    m_spinSlaveId->setValue(1);
    layout->addWidget(labelId, 0, 2);
    layout->addWidget(m_spinSlaveId, 0, 3);

    QLabel* labelBaud = new QLabel("波特率:", m_configGroup);
    m_comboBaud = new QComboBox(m_configGroup);
    m_comboBaud->addItems({"9600", "19200", "38400", "57600", "115200"});
    m_comboBaud->setCurrentText("9600");
    layout->addWidget(labelBaud, 1, 0);
    layout->addWidget(m_comboBaud, 1, 1);

    QLabel* labelDataBits = new QLabel("数据位:", m_configGroup);
    m_comboDataBits = new QComboBox(m_configGroup);
    m_comboDataBits->addItems({"8", "7"});
    m_comboDataBits->setCurrentText("8");
    layout->addWidget(labelDataBits, 1, 2);
    layout->addWidget(m_comboDataBits, 1, 3);

    QLabel* labelStopBits = new QLabel("停止位:", m_configGroup);
    m_comboStopBits = new QComboBox(m_configGroup);
    m_comboStopBits->addItems({"1", "2"});
    m_comboStopBits->setCurrentText("1");
    layout->addWidget(labelStopBits, 2, 0);
    layout->addWidget(m_comboStopBits, 2, 1);

    QLabel* labelParity = new QLabel("校验位:", m_configGroup);
    m_comboParity = new QComboBox(m_configGroup);
    m_comboParity->addItems({"无", "奇校验", "偶校验"});
    m_comboParity->setCurrentText("无");
    layout->addWidget(labelParity, 2, 2);
    layout->addWidget(m_comboParity, 2, 3);

    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(3, 1);
}

void modbusSlaveDialog::createControlGroup()
{
    m_controlGroup = new QGroupBox("控制", this);
    QHBoxLayout* layout = new QHBoxLayout(m_controlGroup);
    layout->setSpacing(15);

    m_btnConnect = new QPushButton("连接", m_controlGroup);
    m_btnConnect->setMinimumWidth(100);
    m_btnConnect->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    font-weight: bold;"
        "    padding: 8px 20px;"
        "    border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #45a049;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #cccccc;"
        "    color: #666666;"
        "}"
        );
    connect(m_btnConnect, &QPushButton::clicked, this, &modbusSlaveDialog::onConnectClicked);

    m_btnDisconnect = new QPushButton("断开", m_controlGroup);
    m_btnDisconnect->setMinimumWidth(100);
    m_btnDisconnect->setEnabled(false);
    m_btnDisconnect->setStyleSheet(
        "QPushButton {"
        "    background-color: #f44336;"
        "    color: white;"
        "    font-weight: bold;"
        "    padding: 8px 20px;"
        "    border-radius: 5px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #da190b;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #cccccc;"
        "    color: #666666;"
        "}"
        );
    connect(m_btnDisconnect, &QPushButton::clicked, this, &modbusSlaveDialog::onDisconnectClicked);

    QPushButton* btnRefresh = new QPushButton("刷新端口", m_controlGroup);
    btnRefresh->setMinimumWidth(100);
    connect(btnRefresh, &QPushButton::clicked, this, &modbusSlaveDialog::refreshPorts);

    m_btnClearLog = new QPushButton("清空日志", m_controlGroup);
    m_btnClearLog->setMinimumWidth(100);
    connect(m_btnClearLog, &QPushButton::clicked, this, &modbusSlaveDialog::onClearLogClicked);

    layout->addWidget(m_btnConnect);
    layout->addWidget(m_btnDisconnect);
    layout->addWidget(btnRefresh);
    layout->addStretch();
    layout->addWidget(m_btnClearLog);
}

void modbusSlaveDialog::createDebugGroup()
{
    m_debugGroup = new QGroupBox("调试信息", this);
    QVBoxLayout* layout = new QVBoxLayout(m_debugGroup);
    layout->setContentsMargins(8, 8, 8, 8);

    m_textDebug = new QTextEdit(m_debugGroup);
    m_textDebug->setReadOnly(true);

    QFont font;
#ifdef Q_OS_WIN
    font = QFont("Consolas", 9);
    if (!QFontDatabase::hasFamily("Consolas")) {
        font = QFont("Courier New", 9);
    }
#else
    font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (font.family().isEmpty()) {
        font = QFont("Monospace", 9);
    }
#endif
    m_textDebug->setFont(font);

    m_textDebug->setStyleSheet(
        "QTextEdit {"
        "    background-color: #1e1e1e;"
        "    color: #d4d4d4;"
        "    border: 1px solid #3c3c3c;"
        "    border-radius: 4px;"
        "    padding: 5px;"
        "}"
        "QTextEdit:focus {"
        "    border: 1px solid #4a90d9;"
        "}"
        );
    layout->addWidget(m_textDebug);
}

void modbusSlaveDialog::createStatusBar()
{
    m_statusLabel = new QLabel("就绪", this);
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #f0f0f0;"
        "    padding: 5px 10px;"
        "    border: 1px solid #d0d0d0;"
        "    border-radius: 3px;"
        "}"
        );
    m_statusLabel->setAlignment(Qt::AlignCenter);
}

ModbusConfig modbusSlaveDialog::getConfigFromUI() const
{
    ModbusConfig config;

    config.portName = m_comboPort->currentData().toString();
    config.baudRate = m_comboBaud->currentText().toInt();
    config.slaveId = static_cast<quint8>(m_spinSlaveId->value());

    // 数据位
    config.dataBits = (m_comboDataBits->currentText() == "8")
                          ? QSerialPort::Data8
                          : QSerialPort::Data7;

    // 停止位
    config.stopBits = (m_comboStopBits->currentText() == "1")
                          ? QSerialPort::OneStop
                          : QSerialPort::TwoStop;

    // 校验位
    QString parityText = m_comboParity->currentText();
    if (parityText == "无") {
        config.parity = QSerialPort::NoParity;
    } else if (parityText == "奇校验") {
        config.parity = QSerialPort::OddParity;
    } else {
        config.parity = QSerialPort::EvenParity;
    }

    return config;
}

void modbusSlaveDialog::setConfigToUI(const ModbusConfig& config)
{
    // 设置端口
    int index = m_comboPort->findData(config.portName);
    if (index >= 0) {
        m_comboPort->setCurrentIndex(index);
    }

    // 设置波特率
    m_comboBaud->setCurrentText(QString::number(config.baudRate));

    // 设置从站ID
    m_spinSlaveId->setValue(config.slaveId);

    // 设置数据位
    m_comboDataBits->setCurrentText(config.dataBits == QSerialPort::Data8 ? "8" : "7");

    // 设置停止位
    m_comboStopBits->setCurrentText(config.stopBits == QSerialPort::OneStop ? "1" : "2");

    // 设置校验位
    QString parityText;
    if (config.parity == QSerialPort::NoParity) {
        parityText = "无";
    } else if (config.parity == QSerialPort::OddParity) {
        parityText = "奇校验";
    } else {
        parityText = "偶校验";
    }
    m_comboParity->setCurrentText(parityText);
}

ModbusConfig modbusSlaveDialog::getCurrentConfig() const
{
    return m_currentConfig;
}

void modbusSlaveDialog::setConfig(const ModbusConfig& config)
{
    m_currentConfig = config;
    setConfigToUI(config);
}

void modbusSlaveDialog::showDialog()
{
    if (m_isDialogShown) {
        activateWindow();
        raise();
        return;
    }

    m_isDialogShown = true;
    show();
}

void modbusSlaveDialog::closeEvent(QCloseEvent* event)
{
    hide();
    m_isDialogShown = false;
    event->ignore();
}

void modbusSlaveDialog::reject()
{
    hide();
    m_isDialogShown = false;
}

void modbusSlaveDialog::refreshPorts()
{
    m_comboPort->clear();
    const auto ports = QSerialPortInfo::availablePorts();
    if (ports.isEmpty()) {
        m_comboPort->addItem("无可用串口", "");
    } else {
        for (const QSerialPortInfo& info : ports) {
            QString display = QString("%1 (%2)")
            .arg(info.portName())
                .arg(info.description().isEmpty() ? "未知设备" : info.description());
            m_comboPort->addItem(display, info.portName());
        }
    }

    m_comboPort->addItem("--------- 虚拟串口 ---------", "");
    m_comboPort->addItem("COM7 (虚拟串口)", "COM7");
}

void modbusSlaveDialog::onConnectClicked()
{
    if (m_isConnected) {
        return;
    }

    // 从UI获取配置
    ModbusConfig config = getConfigFromUI();

    if (!config.isValid()) {
        QMessageBox::warning(this, "错误", "请选择有效的串口配置");
        return;
    }

    // 保存当前配置
    m_currentConfig = config;

    ConfigManager* cm = ConfigManager::instance();
    QString  currentPath = cm->getConfigFilePath();

    // 直接保存到文件
    bool saved = cm->saveModbusConfigToFile(currentPath, m_currentConfig);
    if (saved) {
        addDebugInfo("Modbus配置已保存到: " + currentPath, 1);
    }

    // 创建线程和worker
    m_workerThread = new QThread(this);
    m_worker = new ModbusSlaveWorker();
    m_worker->moveToThread(m_workerThread);

    // 使用结构体设置配置
    connect(m_workerThread, &QThread::started, [this, config]() {
        m_worker->setConfig(config);
        m_worker->start();
    });

    connect(m_worker, &ModbusSlaveWorker::started, this, &modbusSlaveDialog::onSlaveStarted);
    connect(m_worker, &ModbusSlaveWorker::stopped, this, &modbusSlaveDialog::onSlaveStopped);
    connect(m_worker, &ModbusSlaveWorker::errorOccurred, this, &modbusSlaveDialog::onErrorOccurred);
    connect(m_worker, &ModbusSlaveWorker::debugInfo, this, &modbusSlaveDialog::onDebugInfo);

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_workerThread->start();

    m_btnConnect->setEnabled(false);
    m_btnDisconnect->setEnabled(false);
    m_statusLabel->setText("正在连接...");
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #fff3cd;"
        "    padding: 5px 10px;"
        "    border: 1px solid #ffc107;"
        "    border-radius: 3px;"
        "    color: #856404;"
        "}"
        );
    addDebugInfo(QString("正在连接... 配置: %1").arg(config.toString()), 0);
}

void modbusSlaveDialog::onDisconnectClicked()
{
    if (m_worker) {
        m_worker->stop();
    }
}

void modbusSlaveDialog::onClearLogClicked()
{
    m_textDebug->clear();
    addDebugInfo("日志已清空", 0);
}

void modbusSlaveDialog::onSlaveStarted()
{
    m_isConnected = true;
    m_btnConnect->setEnabled(false);
    m_btnDisconnect->setEnabled(true);
    m_statusLabel->setText("已连接 - 从站运行中");
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #d4edda;"
        "    padding: 5px 10px;"
        "    border: 1px solid #28a745;"
        "    border-radius: 3px;"
        "    color: #155724;"
        "}"
        );
    addDebugInfo("========== 从站已启动 ==========", 0);
    addDebugInfo(QString("当前配置: %1").arg(m_currentConfig.toString()), 0);
    addDebugInfo("支持功能码: 0x03(读寄存器), 0x06(写寄存器)", 0);

    m_comboPort->setEnabled(false);
    m_comboBaud->setEnabled(false);
    m_comboDataBits->setEnabled(false);
    m_comboStopBits->setEnabled(false);
    m_comboParity->setEnabled(false);
    m_spinSlaveId->setEnabled(false);

    // 发送信号通知连接成功
    emit slaveStarted();
}

void modbusSlaveDialog::onSlaveStopped()
{
    m_isConnected = false;
    m_btnConnect->setEnabled(true);
    m_btnDisconnect->setEnabled(false);
    m_statusLabel->setText("已断开");
    m_statusLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #f8d7da;"
        "    padding: 5px 10px;"
        "    border: 1px solid #dc3545;"
        "    border-radius: 3px;"
        "    color: #721c24;"
        "}"
        );
    addDebugInfo("========== 从站已停止 ==========", 0);

    m_comboPort->setEnabled(true);
    m_comboBaud->setEnabled(true);
    m_comboDataBits->setEnabled(true);
    m_comboStopBits->setEnabled(true);
    m_comboParity->setEnabled(true);
    m_spinSlaveId->setEnabled(true);

    // ===== 安全清理 =====
    if (m_worker) {
        // 断开所有信号连接（避免在删除过程中触发）
        disconnect(m_worker, nullptr, nullptr, nullptr);

        // 停止 worker
        m_worker->stop();
    }

    if (m_workerThread) {
        // 断开线程信号
        disconnect(m_workerThread, nullptr, nullptr, nullptr);

        // 退出线程
        m_workerThread->quit();
        m_workerThread->wait();

        // 删除线程（会连带删除 worker）
        delete m_workerThread;
        m_workerThread = nullptr;
        m_worker = nullptr;  // worker 已经被 thread 删除
    }
}

void modbusSlaveDialog::onErrorOccurred(const QString& error)
{
    addDebugInfo(QString("错误: %1").arg(error), 3);
    QMessageBox::critical(this, "错误", error);
    onSlaveStopped();
}

void modbusSlaveDialog::onDebugInfo(const QString& info, int level)
{
    addDebugInfo(info, level);
}

void modbusSlaveDialog::addDebugInfo(const QString& msg, int level)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString prefix;
    QString color;

    switch(level) {
    case 0: prefix = "[INFO]"; color = "#d4d4d4"; break;
    case 1: prefix = "[RECV]"; color = "#44aaff"; break;
    case 2: prefix = "[SEND]"; color = "#ffaa44"; break;
    case 3: prefix = "[ERR]";  color = "#ff4444"; break;
    default: prefix = "[INFO]"; color = "#d4d4d4"; break;
    }

    QString formatted = QString("[%1] %2 %3").arg(timestamp).arg(prefix).arg(msg);
    QString html = QString("<font color='%1'>%2</font>").arg(color).arg(formatted.toHtmlEscaped());

    m_textDebug->append(html);
    m_textDebug->moveCursor(QTextCursor::End);

    if (m_textDebug->document()->blockCount() > 5000) {
        QTextCursor cursor(m_textDebug->document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 1000);
        cursor.removeSelectedText();
    }
}

void modbusSlaveDialog::updateUIState(bool connected)
{
    m_isConnected = connected;
    m_btnConnect->setEnabled(!connected);
    m_btnDisconnect->setEnabled(connected);
}