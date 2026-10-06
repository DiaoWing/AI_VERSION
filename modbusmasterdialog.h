// modbusmasterdialog.h
#ifndef MODBUSMASTERDIALOG_H
#define MODBUSMASTERDIALOG_H

//只存在两个读写的寄存器的功能！

#include <QDialog>
#include <QThread>
#include <QSerialPort>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QCloseEvent>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QTableWidget>
#include <QMap>
#include <QTabWidget>

// ===== Modbus 主站配置结构体 =====
struct ModbusMasterConfig {
    QString portName;
    int baudRate;
    QSerialPort::DataBits dataBits;
    QSerialPort::StopBits stopBits;
    QSerialPort::Parity parity;
    quint8 slaveId;
    int timeout;  // 超时时间(ms)

    ModbusMasterConfig()
        : portName("COM1")
        , baudRate(9600)
        , dataBits(QSerialPort::Data8)
        , stopBits(QSerialPort::OneStop)
        , parity(QSerialPort::NoParity)
        , slaveId(1)
        , timeout(1000)
    {}

    bool isValid() const {
        return !portName.isEmpty() &&
               portName != "无可用串口" &&
               baudRate > 0 &&
               slaveId >= 1 && slaveId <= 247 &&
               timeout > 0;
    }

    QString toString() const {
        QString dataBitsStr = (dataBits == QSerialPort::Data8) ? "8" : "7";
        QString stopBitsStr = (stopBits == QSerialPort::OneStop) ? "1" : "2";
        QString parityStr;
        if (parity == QSerialPort::NoParity) parityStr = "无";
        else if (parity == QSerialPort::OddParity) parityStr = "奇校验";
        else parityStr = "偶校验";

        return QString("端口: %1, 波特率: %2, 数据位: %3, 停止位: %4, 校验: %5, ID: %6, 超时: %7ms")
            .arg(portName).arg(baudRate).arg(dataBitsStr)
            .arg(stopBitsStr).arg(parityStr).arg(slaveId).arg(timeout);
    }
};

// ===== Modbus 主站工作线程 =====
class ModbusMasterWorker : public QObject
{
    Q_OBJECT

public:
    explicit ModbusMasterWorker(QObject* parent = nullptr);
    ~ModbusMasterWorker();

    void setConfig(const ModbusMasterConfig& config);

public slots:
    void start();
    void stop();

    // Modbus 寄存器功能码
    void readHoldingRegisters(quint16 startAddr, quint16 count);
    void readInputRegisters(quint16 startAddr, quint16 count);
    void writeSingleRegister(quint16 address, quint16 value);
    void writeMultipleRegisters(quint16 startAddr, const QList<quint16>& values);

signals:
    void started();
    void stopped();
    void errorOccurred(const QString& error);
    void debugInfo(const QString& info, int level = 0);

    // 读取结果信号
    void holdingRegistersRead(quint16 startAddr, const QList<quint16>& values);
    void inputRegistersRead(quint16 startAddr, const QList<quint16>& values);
    void writeCompleted(quint16 address, quint16 value);
    void writeMultipleCompleted(quint16 startAddr, quint16 count);

private slots:
    void readData();

private:
    QByteArray buildRequest(quint8 functionCode, const QByteArray& data);
    bool sendRequest(const QByteArray& request);
    QByteArray waitForResponse(int timeout);
    bool verifyCRC(const QByteArray& data);
    quint16 calculateCRC(const QByteArray& data);
    bool parseResponse(const QByteArray& response, quint8 expectedFunctionCode);

    QSerialPort* m_serialPort;
    ModbusMasterConfig m_config;
    bool m_isRunning;
    QByteArray m_buffer;
    QTimer* m_readTimer;
    QTimer* m_timeoutTimer;
    bool m_waitingForResponse;
    QByteArray m_responseBuffer;
};

// ===== Modbus 主站对话框 =====
class ModbusMasterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModbusMasterDialog(QWidget *parent = nullptr);
    ~ModbusMasterDialog();

    void showDialog();

    ModbusMasterConfig getCurrentConfig() const;
    void setConfig(const ModbusMasterConfig& config);

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onClearLogClicked();
    void onRefreshPorts();

    void onReadHoldingRegisters();
    void onReadInputRegisters();
    void onWriteSingleRegister();
    void onWriteMultipleRegisters();

    void onSlaveStarted();
    void onSlaveStopped();
    void onErrorOccurred(const QString& error);
    void onDebugInfo(const QString& info, int level);

    void onHoldingRegistersRead(quint16 startAddr, const QList<quint16>& values);
    void onInputRegistersRead(quint16 startAddr, const QList<quint16>& values);
    void onWriteCompleted(quint16 address, quint16 value);
    void onWriteMultipleCompleted(quint16 startAddr, quint16 count);

private:
    void setupUI();
    void addDebugInfo(const QString& msg, int level = 0);
    void updateUIState(bool connected);
    ModbusMasterConfig getConfigFromUI() const;
    void setConfigToUI(const ModbusMasterConfig& config);

    // UI组件
    QGroupBox* m_configGroup;
    QGroupBox* m_controlGroup;
    QGroupBox* m_debugGroup;
    QTabWidget* m_tabWidget;

    // 配置控件
    QComboBox* m_comboPort;
    QComboBox* m_comboBaud;
    QComboBox* m_comboDataBits;
    QComboBox* m_comboStopBits;
    QComboBox* m_comboParity;
    QSpinBox* m_spinSlaveId;
    QSpinBox* m_spinTimeout;

    // 控制按钮
    QPushButton* m_btnConnect;
    QPushButton* m_btnDisconnect;
    QPushButton* m_btnClearLog;

    // 读寄存器标签页
    QSpinBox* m_spinReadStartAddr;
    QSpinBox* m_spinReadCount;
    QPushButton* m_btnReadHolding;
    QPushButton* m_btnReadInput;
    QTableWidget* m_tableReadResult;

    // 写寄存器标签页
    QSpinBox* m_spinWriteAddr;
    QSpinBox* m_spinWriteValue;
    QPushButton* m_btnWriteSingle;
    QPushButton* m_btnWriteMultiple;
    QTextEdit* m_textWriteResult;

    // 调试信息
    QTextEdit* m_textDebug;
    QLabel* m_statusLabel;

    // 工作线程
    QThread* m_workerThread;
    ModbusMasterWorker* m_worker;
    bool m_isConnected;
    bool m_isDialogShown;
    ModbusMasterConfig m_currentConfig;
};

#endif // MODBUSMASTERDIALOG_H