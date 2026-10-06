#ifndef MODBUSSLAVEDIALOG_H
#define MODBUSSLAVEDIALOG_H

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
#include <QMap>


// ===== Modbus 配置结构体 =====
struct ModbusConfig {
    QString portName;
    int baudRate;
    QSerialPort::DataBits dataBits;
    QSerialPort::StopBits stopBits;
    QSerialPort::Parity parity;
    quint8 slaveId;

    // 默认构造函数
    ModbusConfig()
        : portName("COM1")
        , baudRate(9600)
        , dataBits(QSerialPort::Data8)
        , stopBits(QSerialPort::OneStop)
        , parity(QSerialPort::NoParity)
        , slaveId(1)
    {}

    // 带参数的构造函数
    ModbusConfig(const QString& port, int baud,
                 QSerialPort::DataBits data,
                 QSerialPort::StopBits stop,
                 QSerialPort::Parity p,
                 quint8 id)
        : portName(port)
        , baudRate(baud)
        , dataBits(data)
        , stopBits(stop)
        , parity(p)
        , slaveId(id)
    {}

    // 复制构造函数
    ModbusConfig(const ModbusConfig& other)
        : portName(other.portName)
        , baudRate(other.baudRate)
        , dataBits(other.dataBits)
        , stopBits(other.stopBits)
        , parity(other.parity)
        , slaveId(other.slaveId)
    {}

    // 赋值运算符
    ModbusConfig& operator=(const ModbusConfig& other) {
        if (this != &other) {
            portName = other.portName;
            baudRate = other.baudRate;
            dataBits = other.dataBits;
            stopBits = other.stopBits;
            parity = other.parity;
            slaveId = other.slaveId;
        }
        return *this;
    }

    // 验证配置是否有效
    bool isValid() const {
        return !portName.isEmpty() &&
               portName != "无可用串口" &&
               portName != "--------- 虚拟串口 ---------" &&
               baudRate > 0 &&
               slaveId >= 1 && slaveId <= 247;
    }

    // 获取配置的字符串描述（用于调试）
    QString toString() const {
        QString dataBitsStr = (dataBits == QSerialPort::Data8) ? "8" : "7";
        QString stopBitsStr = (stopBits == QSerialPort::OneStop) ? "1" : "2";
        QString parityStr;
        if (parity == QSerialPort::NoParity) {
            parityStr = "无";
        } else if (parity == QSerialPort::OddParity) {
            parityStr = "奇校验";
        } else {
            parityStr = "偶校验";
        }

        return QString("端口: %1, 波特率: %2, 数据位: %3, 停止位: %4, 校验: %5, ID: %6")
            .arg(portName)
            .arg(baudRate)
            .arg(dataBitsStr)
            .arg(stopBitsStr)
            .arg(parityStr)
            .arg(slaveId);
    }

    // 判断两个配置是否相等
    bool equals(const ModbusConfig& other) const {
        return portName == other.portName &&
               baudRate == other.baudRate &&
               dataBits == other.dataBits &&
               stopBits == other.stopBits &&
               parity == other.parity &&
               slaveId == other.slaveId;
    }


};

// ===== Modbus 从站工作线程 =====
class ModbusSlaveWorker : public QObject
{
    Q_OBJECT

public:
    explicit ModbusSlaveWorker(QObject* parent = nullptr);
    ~ModbusSlaveWorker();

    // 使用结构体设置配置
    void setConfig(const ModbusConfig& config);

    // 保留原有接口以保持兼容性
    void setConfig(const QString& portName, int baudRate,
                   QSerialPort::DataBits dataBits,
                   QSerialPort::StopBits stopBits,
                   QSerialPort::Parity parity,
                   quint8 slaveId);

public slots:
    void start();
    void stop();

signals:
    void started();
    void stopped();
    void errorOccurred(const QString& error);
    void debugInfo(const QString& info, int level = 0);
    void registerChanged(quint16 address, quint16 value);

private slots:
    void readData();

private:
    void processRequest(const QByteArray& request);
    QByteArray buildResponse(quint8 slaveId, quint8 functionCode, const QByteArray& data);
    QByteArray buildErrorResponse(quint8 slaveId, quint8 functionCode, quint8 exceptionCode);
    quint16 calculateCRC(const QByteArray& data);
    bool verifyCRC(const QByteArray& data);

    QSerialPort* m_serialPort;
    QByteArray m_buffer;

    ModbusConfig m_config;  // 使用结构体替代多个成员变量

    bool m_isRunning;
    QMap<quint16, quint16> m_registers;  // 保持寄存器

    QTimer* m_readTimer;
};

// ===== Modbus 从站对话框 =====
class modbusSlaveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit modbusSlaveDialog(QWidget *parent = nullptr);
    ~modbusSlaveDialog();

    void showDialog();

    // 获取当前配置
    ModbusConfig getCurrentConfig() const;

    // 设置配置（更新UI）
    void setConfig(const ModbusConfig& config);

signals:
    void slaveStarted();  // 添加这个信号

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onClearLogClicked();
    void onSlaveStarted();
    void onSlaveStopped();
    void onErrorOccurred(const QString& error);
    void onDebugInfo(const QString& info, int level);

private:
    void setupUI();
    void addDebugInfo(const QString& msg, int level = 0);
    void updateUIState(bool connected);
    void refreshPorts();
    void createConfigGroup();
    void createControlGroup();
    void createDebugGroup();
    void createStatusBar();

    // 从UI控件读取配置
    ModbusConfig getConfigFromUI() const;

    // 更新UI控件显示配置
    void setConfigToUI(const ModbusConfig& config);

    QGroupBox* m_configGroup;
    QGroupBox* m_controlGroup;
    QGroupBox* m_debugGroup;

    QComboBox* m_comboPort;
    QComboBox* m_comboBaud;
    QComboBox* m_comboDataBits;
    QComboBox* m_comboStopBits;
    QComboBox* m_comboParity;
    QSpinBox* m_spinSlaveId;

    QPushButton* m_btnConnect;
    QPushButton* m_btnDisconnect;
    QPushButton* m_btnClearLog;

    QTextEdit* m_textDebug;
    QLabel* m_statusLabel;

public:
    QThread* m_workerThread;
    ModbusSlaveWorker* m_worker;
    bool m_isConnected;
    bool m_isDialogShown;

    ModbusConfig m_currentConfig;  // 当前配置
};

#endif // MODBUSSLAVEDIALOG_H