#ifndef MESSAGEQUEUE_H
#define MESSAGEQUEUE_H

#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QDateTime>

// 消息结构
struct ModbusMessage {
    quint32 taskId;           // 任务ID（自增）
    quint16 registerAddress;  // 寄存器地址（固定0）
    quint16 registerValue;    // 寄存器值
    QDateTime timestamp;      // 时间戳
    int projectId;            // 项目ID（从配置表获取）
    bool processed;           // 是否已处理

    ModbusMessage() : taskId(0), registerAddress(0), registerValue(0), projectId(0), processed(false) {}

    ModbusMessage(quint32 id, quint16 addr, quint16 value)
        : taskId(id), registerAddress(addr), registerValue(value),
        timestamp(QDateTime::currentDateTime()), projectId(0), processed(false) {}
};

// 线程安全的消息队列
class MessageQueue : public QObject
{
    Q_OBJECT

public:
    static MessageQueue* instance();

    // 入队（生产者调用）
    void enqueue(const ModbusMessage& msg);

    // 出队（消费者调用，阻塞直到有消息）
    ModbusMessage dequeue();

    // 非阻塞出队
    bool tryDequeue(ModbusMessage& msg, int timeoutMs = 0);

    // 获取队列大小
    int size() const;

    // 清空队列
    void clear();

    // 设置最大队列长度
    void setMaxSize(int maxSize);

signals:
    void messageEnqueued(const ModbusMessage& msg);
    void queueSizeChanged(int size);

private:
    MessageQueue(QObject* parent = nullptr);
    ~MessageQueue();

    QQueue<ModbusMessage> m_queue;
    mutable QMutex m_mutex;
    QWaitCondition m_condition;
    int m_maxSize;
    quint32 m_nextTaskId;

    static MessageQueue* m_instance;
};

#endif // MESSAGEQUEUE_H