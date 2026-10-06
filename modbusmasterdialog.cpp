// modbusmasterdialog.cpp
#include "modbusmasterdialog.h"
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QFont>
#include <QScrollBar>
#include <QApplication>
#include <QScreen>
#include <QFontDatabase>
#include <QHeaderView>

// ============================================================
// ModbusMasterWorker 实现
// ============================================================

ModbusMasterWorker::ModbusMasterWorker(QObject* parent)
    : QObject(parent)
    , m_serialPort(nullptr)
    , m_isRunning(false)
    , m_readTimer(nullptr)
    , m_timeoutTimer(nullptr)
    , m_waitingForResponse(false)
{
}

ModbusMasterWorker::~ModbusMasterWorker()
{
    stop();
    if (m_serialPort) {
        delete m_serialPort;
    }
    if (m_readTimer) {
        delete m_readTimer;
    }
    if (m_timeoutTimer) {
        delete m_timeoutTimer;
    }
}

void ModbusMasterWorker::setConfig(const ModbusMasterConfig& config)
{
    m_config = config;
}

void ModbusMasterWorker::start()
{
    if (m_isRunning) {
        emit debugInfo("主站已在运行", 2);
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
    m_serialPort->clear();

    if (!m_readTimer) {
        m_readTimer = new QTimer(this);
        connect(m_readTimer, &QTimer::timeout, this, &ModbusMasterWorker::readData);
    }
    m_readTimer->start(10);

    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout, [this]() {
            if (m_waitingForResponse) {
                m_waitingForResponse = false;
                emit errorOccurred("响应超时");
                emit debugInfo("⚠️ 响应超时", 3);
            }
        });
    }

    emit debugInfo(QString("主站启动成功 | %1").arg(m_config.toString()), 0);
    emit started();
}

void ModbusMasterWorker::stop()
{
    if (!m_isRunning) {
        return;
    }

    if (m_readTimer) {
        m_readTimer->stop();
    }
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }

    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
    }

    m_isRunning = false;
    m_waitingForResponse = false;
    m_buffer.clear();

    emit debugInfo("主站已停止", 0);
    emit stopped();
}

void ModbusMasterWorker::readData()
{
    if (!m_serialPort || !m_serialPort->isOpen() || !m_isRunning) {
        return;
    }

    QByteArray data = m_serialPort->readAll();
    if (data.isEmpty()) {
        return;
    }

    m_buffer.append(data);

    // 如果正在等待响应，检查是否收到完整帧
    if (m_waitingForResponse) {
        // 至少需要4字节（地址+功能码+数据+CRC）
        if (m_buffer.size() >= 4) {
            // 检查CRC
            if (verifyCRC(m_buffer)) {
                m_waitingForResponse = false;
                m_timeoutTimer->stop();
                m_responseBuffer = m_buffer;
                m_buffer.clear();
                emit debugInfo(QString("收到响应: %1").arg(QString(m_responseBuffer.toHex(' '))), 1);
            }
        }
        // 如果缓冲区太大，可能数据错误
        if (m_buffer.size() > 256) {
            m_buffer.clear();
            m_waitingForResponse = false;
            m_timeoutTimer->stop();
            emit errorOccurred("接收缓冲区溢出");
        }
    } else {
        // 非等待状态，只是打印调试信息
        emit debugInfo(QString("接收: %1").arg(QString(data.toHex(' '))), 1);
        m_buffer.clear();
    }
}

bool ModbusMasterWorker::sendRequest(const QByteArray& request)
{
    if (!m_serialPort || !m_serialPort->isOpen() || !m_isRunning) {
        emit errorOccurred("串口未打开");
        return false;
    }

    m_serialPort->write(request);
    m_serialPort->flush();

    m_waitingForResponse = true;
    m_buffer.clear();
    m_timeoutTimer->start(m_config.timeout);

    emit debugInfo(QString("发送请求: %1").arg(QString(request.toHex(' '))), 2);
    return true;
}

QByteArray ModbusMasterWorker::waitForResponse(int timeout)
{
    Q_UNUSED(timeout);
    return m_responseBuffer;
}

bool ModbusMasterWorker::verifyCRC(const QByteArray& data)
{
    if (data.size() < 2) return false;
    QByteArray withoutCRC = data.left(data.size() - 2);
    quint16 calcCRC = calculateCRC(withoutCRC);
    quint16 recvCRC = (quint8)data[data.size() - 2] | ((quint8)data[data.size() - 1] << 8);
    return calcCRC == recvCRC;
}

quint16 ModbusMasterWorker::calculateCRC(const QByteArray& data)
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

QByteArray ModbusMasterWorker::buildRequest(quint8 functionCode, const QByteArray& data)
{
    QByteArray request;
    request.append((char)m_config.slaveId);
    request.append((char)functionCode);
    request.append(data);

    quint16 crc = calculateCRC(request);
    request.append((char)(crc & 0xFF));
    request.append((char)(crc >> 8));

    return request;
}

bool ModbusMasterWorker::parseResponse(const QByteArray& response, quint8 expectedFunctionCode)
{
    if (response.size() < 4) {
        emit errorOccurred("响应太短");
        return false;
    }

    quint8 slaveId = (quint8)response[0];
    quint8 functionCode = (quint8)response[1];

    if (slaveId != m_config.slaveId) {
        emit errorOccurred(QString("从站ID不匹配: 期望%1, 实际%2")
                               .arg(m_config.slaveId).arg(slaveId));
        return false;
    }

    // 检查是否是错误响应
    if (functionCode & 0x80) {
        quint8 exceptionCode = (quint8)response[2];
        QString errorMsg;
        switch(exceptionCode) {
        case 0x01: errorMsg = "非法功能码"; break;
        case 0x02: errorMsg = "非法数据地址"; break;
        case 0x03: errorMsg = "非法数据值"; break;
        case 0x04: errorMsg = "从站设备故障"; break;
        default: errorMsg = QString("未知异常码: 0x%1").arg(exceptionCode, 2, 16, QChar('0'));
        }
        emit errorOccurred(QString("从站错误: %1").arg(errorMsg));
        return false;
    }

    if (functionCode != expectedFunctionCode) {
        emit errorOccurred(QString("功能码不匹配: 期望0x%1, 实际0x%2")
                               .arg(expectedFunctionCode, 2, 16, QChar('0'))
                               .arg(functionCode, 2, 16, QChar('0')));
        return false;
    }

    return true;
}

// ===== Modbus 寄存器功能码实现 =====

void ModbusMasterWorker::readHoldingRegisters(quint16 startAddr, quint16 count)
{
    if (!m_isRunning) {
        emit errorOccurred("主站未启动");
        return;
    }

    if (count < 1 || count > 125) {
        emit errorOccurred("读取数量必须在1-125之间");
        return;
    }

    QByteArray data;
    data.append((char)(startAddr >> 8));
    data.append((char)(startAddr & 0xFF));
    data.append((char)(count >> 8));
    data.append((char)(count & 0xFF));

    QByteArray request = buildRequest(0x03, data);

    if (sendRequest(request)) {
        // 异步处理响应
    }
}

void ModbusMasterWorker::readInputRegisters(quint16 startAddr, quint16 count)
{
    if (!m_isRunning) {
        emit errorOccurred("主站未启动");
        return;
    }

    if (count < 1 || count > 125) {
        emit errorOccurred("读取数量必须在1-125之间");
        return;
    }

    QByteArray data;
    data.append((char)(startAddr >> 8));
    data.append((char)(startAddr & 0xFF));
    data.append((char)(count >> 8));
    data.append((char)(count & 0xFF));

    QByteArray request = buildRequest(0x04, data);

    if (sendRequest(request)) {
        // 异步处理响应
    }
}

void ModbusMasterWorker::writeSingleRegister(quint16 address, quint16 value)
{
    if (!m_isRunning) {
        emit errorOccurred("主站未启动");
        return;
    }

    QByteArray data;
    data.append((char)(address >> 8));
    data.append((char)(address & 0xFF));
    data.append((char)(value >> 8));
    data.append((char)(value & 0xFF));

    QByteArray request = buildRequest(0x06, data);

    if (sendRequest(request)) {
        // 异步处理响应
    }
}

void ModbusMasterWorker::writeMultipleRegisters(quint16 startAddr, const QList<quint16>& values)
{
    if (!m_isRunning) {
        emit errorOccurred("主站未启动");
        return;
    }

    if (values.isEmpty() || values.size() > 123) {
        emit errorOccurred("写入数量必须在1-123之间");
        return;
    }

    quint16 count = values.size();

    QByteArray data;
    data.append((char)(startAddr >> 8));
    data.append((char)(startAddr & 0xFF));
    data.append((char)(count >> 8));
    data.append((char)(count & 0xFF));
    data.append((char)(count * 2));  // 字节数

    for (quint16 val : values) {
        data.append((char)(val >> 8));
        data.append((char)(val & 0xFF));
    }

    QByteArray request = buildRequest(0x10, data);

    if (sendRequest(request)) {
        // 异步处理响应
    }
}

// ============================================================
// ModbusMasterDialog 实现
// ============================================================

ModbusMasterDialog::ModbusMasterDialog(QWidget* parent)
    : QDialog(parent)
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_isConnected(false)
    , m_isDialogShown(false)
{
    setWindowTitle("Modbus RTU 主站");
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);

    setupUI();
    onRefreshPorts();

    addDebugInfo("Modbus 主站对话框已启动", 0);
    addDebugInfo("支持功能码: 0x03(读保持寄存器), 0x04(读输入寄存器)", 0);
    addDebugInfo("支持功能码: 0x06(写单个寄存器), 0x10(写多个寄存器)", 0);
}

ModbusMasterDialog::~ModbusMasterDialog()
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

void ModbusMasterDialog::setupUI()
{
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // 配置组
    m_configGroup = new QGroupBox("串口配置", this);
    QGridLayout* configLayout = new QGridLayout(m_configGroup);
    configLayout->setSpacing(8);

    QLabel* labelPort = new QLabel("串口号:", m_configGroup);
    m_comboPort = new QComboBox(m_configGroup);
    m_comboPort->setMinimumWidth(150);
    configLayout->addWidget(labelPort, 0, 0);
    configLayout->addWidget(m_comboPort, 0, 1);

    QLabel* labelId = new QLabel("从站ID:", m_configGroup);
    m_spinSlaveId = new QSpinBox(m_configGroup);
    m_spinSlaveId->setRange(1, 247);
    m_spinSlaveId->setValue(1);
    configLayout->addWidget(labelId, 0, 2);
    configLayout->addWidget(m_spinSlaveId, 0, 3);

    QLabel* labelTimeout = new QLabel("超时(ms):", m_configGroup);
    m_spinTimeout = new QSpinBox(m_configGroup);
    m_spinTimeout->setRange(100, 5000);
    m_spinTimeout->setValue(1000);
    m_spinTimeout->setSingleStep(100);
    configLayout->addWidget(labelTimeout, 0, 4);
    configLayout->addWidget(m_spinTimeout, 0, 5);

    QLabel* labelBaud = new QLabel("波特率:", m_configGroup);
    m_comboBaud = new QComboBox(m_configGroup);
    m_comboBaud->addItems({"9600", "19200", "38400", "57600", "115200"});
    m_comboBaud->setCurrentText("9600");
    configLayout->addWidget(labelBaud, 1, 0);
    configLayout->addWidget(m_comboBaud, 1, 1);

    QLabel* labelDataBits = new QLabel("数据位:", m_configGroup);
    m_comboDataBits = new QComboBox(m_configGroup);
    m_comboDataBits->addItems({"8", "7"});
    m_comboDataBits->setCurrentText("8");
    configLayout->addWidget(labelDataBits, 1, 2);
    configLayout->addWidget(m_comboDataBits, 1, 3);

    QLabel* labelStopBits = new QLabel("停止位:", m_configGroup);
    m_comboStopBits = new QComboBox(m_configGroup);
    m_comboStopBits->addItems({"1", "2"});
    m_comboStopBits->setCurrentText("1");
    configLayout->addWidget(labelStopBits, 1, 4);
    configLayout->addWidget(m_comboStopBits, 1, 5);

    QLabel* labelParity = new QLabel("校验位:", m_configGroup);
    m_comboParity = new QComboBox(m_configGroup);
    m_comboParity->addItems({"无", "奇校验", "偶校验"});
    m_comboParity->setCurrentText("无");
    configLayout->addWidget(labelParity, 2, 0);
    configLayout->addWidget(m_comboParity, 2, 1);

    configLayout->setColumnStretch(1, 1);
    configLayout->setColumnStretch(3, 1);
    configLayout->setColumnStretch(5, 1);

    mainLayout->addWidget(m_configGroup);

    // 控制组
    m_controlGroup = new QGroupBox("控制", this);
    QHBoxLayout* controlLayout = new QHBoxLayout(m_controlGroup);
    controlLayout->setSpacing(15);

    m_btnConnect = new QPushButton("连接", m_controlGroup);
    m_btnConnect->setMinimumWidth(100);
    m_btnConnect->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px 20px; border-radius: 5px; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:disabled { background-color: #cccccc; color: #666666; }"
        );
    connect(m_btnConnect, &QPushButton::clicked, this, &ModbusMasterDialog::onConnectClicked);

    m_btnDisconnect = new QPushButton("断开", m_controlGroup);
    m_btnDisconnect->setMinimumWidth(100);
    m_btnDisconnect->setEnabled(false);
    m_btnDisconnect->setStyleSheet(
        "QPushButton { background-color: #f44336; color: white; font-weight: bold; padding: 8px 20px; border-radius: 5px; }"
        "QPushButton:hover { background-color: #da190b; }"
        "QPushButton:disabled { background-color: #cccccc; color: #666666; }"
        );
    connect(m_btnDisconnect, &QPushButton::clicked, this, &ModbusMasterDialog::onDisconnectClicked);

    QPushButton* btnRefresh = new QPushButton("刷新端口", m_controlGroup);
    btnRefresh->setMinimumWidth(100);
    connect(btnRefresh, &QPushButton::clicked, this, &ModbusMasterDialog::onRefreshPorts);

    m_btnClearLog = new QPushButton("清空日志", m_controlGroup);
    m_btnClearLog->setMinimumWidth(100);
    connect(m_btnClearLog, &QPushButton::clicked, this, &ModbusMasterDialog::onClearLogClicked);

    controlLayout->addWidget(m_btnConnect);
    controlLayout->addWidget(m_btnDisconnect);
    controlLayout->addWidget(btnRefresh);
    controlLayout->addStretch();
    controlLayout->addWidget(m_btnClearLog);

    mainLayout->addWidget(m_controlGroup);

    // Tab控件
    m_tabWidget = new QTabWidget(this);

    // ===== 读寄存器标签页 =====
    QWidget* readTab = new QWidget();
    QVBoxLayout* readLayout = new QVBoxLayout(readTab);

    QHBoxLayout* readControlLayout = new QHBoxLayout();
    readControlLayout->addWidget(new QLabel("起始地址:"));
    m_spinReadStartAddr = new QSpinBox();
    m_spinReadStartAddr->setRange(0, 65535);
    m_spinReadStartAddr->setValue(0);
    m_spinReadStartAddr->setDisplayIntegerBase(16);
    readControlLayout->addWidget(m_spinReadStartAddr);

    readControlLayout->addWidget(new QLabel("数量:"));
    m_spinReadCount = new QSpinBox();
    m_spinReadCount->setRange(1, 125);
    m_spinReadCount->setValue(10);
    readControlLayout->addWidget(m_spinReadCount);

    m_btnReadHolding = new QPushButton("读保持寄存器");
    connect(m_btnReadHolding, &QPushButton::clicked, this, &ModbusMasterDialog::onReadHoldingRegisters);
    readControlLayout->addWidget(m_btnReadHolding);

    m_btnReadInput = new QPushButton("读输入寄存器");
    connect(m_btnReadInput, &QPushButton::clicked, this, &ModbusMasterDialog::onReadInputRegisters);
    readControlLayout->addWidget(m_btnReadInput);

    readControlLayout->addStretch();
    readLayout->addLayout(readControlLayout);

    m_tableReadResult = new QTableWidget();
    m_tableReadResult->setColumnCount(2);
    m_tableReadResult->setHorizontalHeaderLabels({"地址", "值"});
    m_tableReadResult->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    readLayout->addWidget(m_tableReadResult);

    m_tabWidget->addTab(readTab, "读寄存器");

    // ===== 写寄存器标签页 =====
    QWidget* writeTab = new QWidget();
    QVBoxLayout* writeLayout = new QVBoxLayout(writeTab);

    QGridLayout* writeControlLayout = new QGridLayout();
    writeControlLayout->addWidget(new QLabel("地址:"), 0, 0);
    m_spinWriteAddr = new QSpinBox();
    m_spinWriteAddr->setRange(0, 65535);
    m_spinWriteAddr->setValue(0);
    m_spinWriteAddr->setDisplayIntegerBase(16);
    writeControlLayout->addWidget(m_spinWriteAddr, 0, 1);

    writeControlLayout->addWidget(new QLabel("值:"), 0, 2);
    m_spinWriteValue = new QSpinBox();
    m_spinWriteValue->setRange(0, 65535);
    m_spinWriteValue->setValue(0);
    m_spinWriteValue->setDisplayIntegerBase(16);
    writeControlLayout->addWidget(m_spinWriteValue, 0, 3);

    m_btnWriteSingle = new QPushButton("写单个寄存器");
    connect(m_btnWriteSingle, &QPushButton::clicked, this, &ModbusMasterDialog::onWriteSingleRegister);
    writeControlLayout->addWidget(m_btnWriteSingle, 0, 4);

    m_btnWriteMultiple = new QPushButton("写多个寄存器(测试)");
    connect(m_btnWriteMultiple, &QPushButton::clicked, this, &ModbusMasterDialog::onWriteMultipleRegisters);
    writeControlLayout->addWidget(m_btnWriteMultiple, 0, 5);

    writeLayout->addLayout(writeControlLayout);

    m_textWriteResult = new QTextEdit();
    m_textWriteResult->setReadOnly(true);
    m_textWriteResult->setMaximumHeight(100);
    writeLayout->addWidget(m_textWriteResult);

    m_tabWidget->addTab(writeTab, "写寄存器");

    mainLayout->addWidget(m_tabWidget);

    // 调试信息
    m_debugGroup = new QGroupBox("调试信息", this);
    QVBoxLayout* debugLayout = new QVBoxLayout(m_debugGroup);
    debugLayout->setContentsMargins(8, 8, 8, 8);

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
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3c3c3c; border-radius: 4px; padding: 5px; }"
        );
    debugLayout->addWidget(m_textDebug);

    mainLayout->addWidget(m_debugGroup);

    // 状态栏
    m_statusLabel = new QLabel("就绪", this);
    m_statusLabel->setStyleSheet(
        "QLabel { background-color: #f0f0f0; padding: 5px 10px; border: 1px solid #d0d0d0; border-radius: 3px; }"
        );
    m_statusLabel->setAlignment(Qt::AlignCenter);

    mainLayout->addWidget(m_statusLabel);

    resize(850, 650);

    QRect screenGeometry = QApplication::primaryScreen()->geometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
}

void ModbusMasterDialog::onRefreshPorts()
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

void ModbusMasterDialog::onConnectClicked()
{
    if (m_isConnected) return;

    ModbusMasterConfig config = getConfigFromUI();

    if (!config.isValid()) {
        QMessageBox::warning(this, "错误", "请选择有效的串口配置");
        return;
    }

    m_currentConfig = config;

    // 创建线程和worker
    m_workerThread = new QThread(this);
    m_worker = new ModbusMasterWorker();
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, [this, config]() {
        m_worker->setConfig(config);
        m_worker->start();
    });

    connect(m_worker, &ModbusMasterWorker::started, this, &ModbusMasterDialog::onSlaveStarted);
    connect(m_worker, &ModbusMasterWorker::stopped, this, &ModbusMasterDialog::onSlaveStopped);
    connect(m_worker, &ModbusMasterWorker::errorOccurred, this, &ModbusMasterDialog::onErrorOccurred);
    connect(m_worker, &ModbusMasterWorker::debugInfo, this, &ModbusMasterDialog::onDebugInfo);

    // 读取结果信号
    connect(m_worker, &ModbusMasterWorker::holdingRegistersRead,
            this, &ModbusMasterDialog::onHoldingRegistersRead);
    connect(m_worker, &ModbusMasterWorker::inputRegistersRead,
            this, &ModbusMasterDialog::onInputRegistersRead);
    connect(m_worker, &ModbusMasterWorker::writeCompleted,
            this, &ModbusMasterDialog::onWriteCompleted);
    connect(m_worker, &ModbusMasterWorker::writeMultipleCompleted,
            this, &ModbusMasterDialog::onWriteMultipleCompleted);

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_workerThread->start();

    m_btnConnect->setEnabled(false);
    m_statusLabel->setText("正在连接...");
    m_statusLabel->setStyleSheet(
        "QLabel { background-color: #fff3cd; padding: 5px 10px; border: 1px solid #ffc107; border-radius: 3px; color: #856404; }"
        );
    addDebugInfo(QString("正在连接... 配置: %1").arg(config.toString()), 0);
}

void ModbusMasterDialog::onDisconnectClicked()
{
    if (m_worker) {
        m_worker->stop();
    }
}

void ModbusMasterDialog::onClearLogClicked()
{
    m_textDebug->clear();
    addDebugInfo("日志已清空", 0);
}

void ModbusMasterDialog::onSlaveStarted()
{
    m_isConnected = true;
    m_btnConnect->setEnabled(false);
    m_btnDisconnect->setEnabled(true);
    m_statusLabel->setText("已连接 - 主站运行中");
    m_statusLabel->setStyleSheet(
        "QLabel { background-color: #d4edda; padding: 5px 10px; border: 1px solid #28a745; border-radius: 3px; color: #155724; }"
        );
    addDebugInfo("========== 主站已启动 ==========", 0);
    addDebugInfo(QString("当前配置: %1").arg(m_currentConfig.toString()), 0);

    // 禁用配置控件
    m_comboPort->setEnabled(false);
    m_comboBaud->setEnabled(false);
    m_comboDataBits->setEnabled(false);
    m_comboStopBits->setEnabled(false);
    m_comboParity->setEnabled(false);
    m_spinSlaveId->setEnabled(false);
    m_spinTimeout->setEnabled(false);
}

void ModbusMasterDialog::onSlaveStopped()
{
    m_isConnected = false;
    m_btnConnect->setEnabled(true);
    m_btnDisconnect->setEnabled(false);
    m_statusLabel->setText("已断开");
    m_statusLabel->setStyleSheet(
        "QLabel { background-color: #f8d7da; padding: 5px 10px; border: 1px solid #dc3545; border-radius: 3px; color: #721c24; }"
        );
    addDebugInfo("========== 主站已停止 ==========", 0);

    // 启用配置控件
    m_comboPort->setEnabled(true);
    m_comboBaud->setEnabled(true);
    m_comboDataBits->setEnabled(true);
    m_comboStopBits->setEnabled(true);
    m_comboParity->setEnabled(true);
    m_spinSlaveId->setEnabled(true);
    m_spinTimeout->setEnabled(true);

    if (m_worker) {
        disconnect(m_worker, nullptr, nullptr, nullptr);
        m_worker->stop();
    }

    if (m_workerThread) {
        disconnect(m_workerThread, nullptr, nullptr, nullptr);
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
        m_worker = nullptr;
    }
}

void ModbusMasterDialog::onErrorOccurred(const QString& error)
{
    addDebugInfo(QString("错误: %1").arg(error), 3);
}

void ModbusMasterDialog::onDebugInfo(const QString& info, int level)
{
    addDebugInfo(info, level);
}

// ===== Modbus 读取操作 =====

void ModbusMasterDialog::onReadHoldingRegisters()
{
    if (!m_isConnected || !m_worker) {
        QMessageBox::warning(this, "提示", "请先连接主站");
        return;
    }

    quint16 startAddr = static_cast<quint16>(m_spinReadStartAddr->value());
    quint16 count = static_cast<quint16>(m_spinReadCount->value());

    addDebugInfo(QString(">>> 读保持寄存器: 地址=0x%1, 数量=%2")
                     .arg(startAddr, 4, 16, QChar('0')).arg(count), 0);

    m_worker->readHoldingRegisters(startAddr, count);
}

void ModbusMasterDialog::onReadInputRegisters()
{
    if (!m_isConnected || !m_worker) {
        QMessageBox::warning(this, "提示", "请先连接主站");
        return;
    }

    quint16 startAddr = static_cast<quint16>(m_spinReadStartAddr->value());
    quint16 count = static_cast<quint16>(m_spinReadCount->value());

    addDebugInfo(QString(">>> 读输入寄存器: 地址=0x%1, 数量=%2")
                     .arg(startAddr, 4, 16, QChar('0')).arg(count), 0);

    m_worker->readInputRegisters(startAddr, count);
}

void ModbusMasterDialog::onWriteSingleRegister()
{
    if (!m_isConnected || !m_worker) {
        QMessageBox::warning(this, "提示", "请先连接主站");
        return;
    }

    quint16 addr = static_cast<quint16>(m_spinWriteAddr->value());
    quint16 value = static_cast<quint16>(m_spinWriteValue->value());

    addDebugInfo(QString(">>> 写单个寄存器: 地址=0x%1, 值=0x%2")
                     .arg(addr, 4, 16, QChar('0')).arg(value, 4, 16, QChar('0')), 0);

    m_worker->writeSingleRegister(addr, value);
}

void ModbusMasterDialog::onWriteMultipleRegisters()
{
    if (!m_isConnected || !m_worker) {
        QMessageBox::warning(this, "提示", "请先连接主站");
        return;
    }

    quint16 addr = static_cast<quint16>(m_spinWriteAddr->value());
    QList<quint16> values;
    // 写入5个测试值
    for (int i = 0; i < 5; i++) {
        values.append(static_cast<quint16>(i * 0x1111));
    }

    addDebugInfo(QString(">>> 写多个寄存器: 地址=0x%1, 数量=%2")
                     .arg(addr, 4, 16, QChar('0')).arg(values.size()), 0);

    m_worker->writeMultipleRegisters(addr, values);
}

// ===== 读取结果回调 =====

void ModbusMasterDialog::onHoldingRegistersRead(quint16 startAddr, const QList<quint16>& values)
{
    addDebugInfo(QString("<<< 读保持寄存器完成: 地址=0x%1, 数量=%2")
                     .arg(startAddr, 4, 16, QChar('0')).arg(values.size()), 0);

    m_tableReadResult->setRowCount(values.size());
    for (int i = 0; i < values.size(); i++) {
        quint16 addr = startAddr + i;
        m_tableReadResult->setItem(i, 0, new QTableWidgetItem(QString("0x%1").arg(addr, 4, 16, QChar('0'))));
        m_tableReadResult->setItem(i, 1, new QTableWidgetItem(QString("0x%1 (%2)")
                                                                  .arg(values[i], 4, 16, QChar('0'))
                                                                  .arg(values[i])));
    }
    m_tableReadResult->resizeColumnsToContents();
}

void ModbusMasterDialog::onInputRegistersRead(quint16 startAddr, const QList<quint16>& values)
{
    addDebugInfo(QString("<<< 读输入寄存器完成: 地址=0x%1, 数量=%2")
                     .arg(startAddr, 4, 16, QChar('0')).arg(values.size()), 0);

    m_tableReadResult->setRowCount(values.size());
    for (int i = 0; i < values.size(); i++) {
        quint16 addr = startAddr + i;
        m_tableReadResult->setItem(i, 0, new QTableWidgetItem(QString("0x%1").arg(addr, 4, 16, QChar('0'))));
        m_tableReadResult->setItem(i, 1, new QTableWidgetItem(QString("0x%1 (%2)")
                                                                  .arg(values[i], 4, 16, QChar('0'))
                                                                  .arg(values[i])));
    }
    m_tableReadResult->resizeColumnsToContents();
}

void ModbusMasterDialog::onWriteCompleted(quint16 address, quint16 value)
{
    addDebugInfo(QString("<<< 写寄存器成功: 地址=0x%1, 值=0x%2")
                     .arg(address, 4, 16, QChar('0')).arg(value, 4, 16, QChar('0')), 0);
    m_textWriteResult->append(QString("[成功] 地址 0x%1 = 0x%2 (%3)")
                                  .arg(address, 4, 16, QChar('0'))
                                  .arg(value, 4, 16, QChar('0'))
                                  .arg(value));
}

void ModbusMasterDialog::onWriteMultipleCompleted(quint16 startAddr, quint16 count)
{
    addDebugInfo(QString("<<< 写多个寄存器成功: 地址=0x%1, 数量=%2")
                     .arg(startAddr, 4, 16, QChar('0')).arg(count), 0);
    m_textWriteResult->append(QString("[成功] 写多个寄存器: 地址 0x%1, 数量 %2")
                                  .arg(startAddr, 4, 16, QChar('0'))
                                  .arg(count));
}

// ===== 辅助函数 =====

void ModbusMasterDialog::addDebugInfo(const QString& msg, int level)
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

void ModbusMasterDialog::updateUIState(bool connected)
{
    m_isConnected = connected;
    m_btnConnect->setEnabled(!connected);
    m_btnDisconnect->setEnabled(connected);
}

ModbusMasterConfig ModbusMasterDialog::getConfigFromUI() const
{
    ModbusMasterConfig config;
    config.portName = m_comboPort->currentData().toString();
    config.baudRate = m_comboBaud->currentText().toInt();
    config.slaveId = static_cast<quint8>(m_spinSlaveId->value());
    config.timeout = m_spinTimeout->value();

    config.dataBits = (m_comboDataBits->currentText() == "8")
                          ? QSerialPort::Data8
                          : QSerialPort::Data7;

    config.stopBits = (m_comboStopBits->currentText() == "1")
                          ? QSerialPort::OneStop
                          : QSerialPort::TwoStop;

    QString parityText = m_comboParity->currentText();
    if (parityText == "无") config.parity = QSerialPort::NoParity;
    else if (parityText == "奇校验") config.parity = QSerialPort::OddParity;
    else config.parity = QSerialPort::EvenParity;

    return config;
}

void ModbusMasterDialog::setConfigToUI(const ModbusMasterConfig& config)
{
    int index = m_comboPort->findData(config.portName);
    if (index >= 0) m_comboPort->setCurrentIndex(index);

    m_comboBaud->setCurrentText(QString::number(config.baudRate));
    m_spinSlaveId->setValue(config.slaveId);
    m_spinTimeout->setValue(config.timeout);
    m_comboDataBits->setCurrentText(config.dataBits == QSerialPort::Data8 ? "8" : "7");
    m_comboStopBits->setCurrentText(config.stopBits == QSerialPort::OneStop ? "1" : "2");

    QString parityText;
    if (config.parity == QSerialPort::NoParity) parityText = "无";
    else if (config.parity == QSerialPort::OddParity) parityText = "奇校验";
    else parityText = "偶校验";
    m_comboParity->setCurrentText(parityText);
}

void ModbusMasterDialog::showDialog()
{
    if (m_isDialogShown) {
        activateWindow();
        raise();
        return;
    }

    m_isDialogShown = true;
    show();
}

void ModbusMasterDialog::closeEvent(QCloseEvent* event)
{
    hide();
    m_isDialogShown = false;
    event->ignore();
}

void ModbusMasterDialog::reject()
{
    hide();
    m_isDialogShown = false;
}

ModbusMasterConfig ModbusMasterDialog::getCurrentConfig() const
{
    return m_currentConfig;
}

void ModbusMasterDialog::setConfig(const ModbusMasterConfig& config)
{
    m_currentConfig = config;
    setConfigToUI(config);
}