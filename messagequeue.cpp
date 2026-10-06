#include "messagequeue.h"
#include <QDebug>

MessageQueue* MessageQueue::m_instance = nullptr;

MessageQueue* MessageQueue::instance()
{
    if (!m_instance) {
        m_instance = new MessageQueue();
    }
    return m_instance;
}

MessageQueue::MessageQueue(QObject* parent)
    : QObject(parent)
    , m_maxSize(100)
    , m_nextTaskId(1)
{
}

MessageQueue::~MessageQueue()
{
    clear();
}

void MessageQueue::enqueue(const ModbusMessage& msg)
{
    //检测信号入队
    QMutexLocker locker(&m_mutex);

    // 如果队列满了，丢弃最旧的消息，要看一下对应的最大阈值是多少才行！
    if (m_queue.size() >= m_maxSize) {
        ModbusMessage old = m_queue.dequeue();
        qWarning() << "队列已满，丢弃旧消息: 任务ID=" << old.taskId;
    }

    ModbusMessage newMsg = msg;
    newMsg.taskId = m_nextTaskId++;
    newMsg.timestamp = QDateTime::currentDateTime();

    //：将一个消息对象（newMsg）添加到队列（m_queue）的末尾。
    m_queue.enqueue(newMsg);
    m_condition.wakeOne();

    emit messageEnqueued(newMsg);    //检测信号 ！
    emit queueSizeChanged(m_queue.size());

    qDebug() << "消息入队: 任务ID=" << newMsg.taskId
             << ", 寄存器值=0x" << QString::number(newMsg.registerValue, 16);
}

ModbusMessage MessageQueue::dequeue()
{
    QMutexLocker locker(&m_mutex);

    while (m_queue.isEmpty()) {
        m_condition.wait(&m_mutex);
    }

    ModbusMessage msg = m_queue.dequeue();
    emit queueSizeChanged(m_queue.size());

    return msg;
}

bool MessageQueue::tryDequeue(ModbusMessage& msg, int timeoutMs)
{
    QMutexLocker locker(&m_mutex);

    if (m_queue.isEmpty()) {
        if (timeoutMs <= 0) {
            return false;
        }
        bool ok = m_condition.wait(&m_mutex, timeoutMs);
        if (!ok || m_queue.isEmpty()) {
            return false;
        }
    }

    msg = m_queue.dequeue();
    emit queueSizeChanged(m_queue.size());
    return true;
}

int MessageQueue::size() const
{
    QMutexLocker locker(&m_mutex);
    return m_queue.size();
}

void MessageQueue::clear()
{
    QMutexLocker locker(&m_mutex);
    m_queue.clear();
    emit queueSizeChanged(0);
}

void MessageQueue::setMaxSize(int maxSize)
{
    QMutexLocker locker(&m_mutex);
    m_maxSize = maxSize;
}