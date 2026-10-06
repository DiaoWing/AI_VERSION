#include "businessworker.h"
#include <QDebug>
#include <QtConcurrent/QtConcurrent>

BusinessWorker::BusinessWorker(QObject* parent)
    : QObject(parent)
    , m_isRunning(false)
{
}

BusinessWorker::~BusinessWorker()
{
    stop();
}

void BusinessWorker::start()
{
    if (m_isRunning) {
        return;
    }

    m_isRunning = true;
    emit debugInfo("业务处理线程已启动");

    QtConcurrent::run(std::bind(&BusinessWorker::processTasks, this));
}

void BusinessWorker::stop()
{
    m_isRunning = false;
    emit debugInfo("业务处理线程已停止");
}

void BusinessWorker::processTasks()
{
    MessageQueue* queue = MessageQueue::instance();

    while (m_isRunning) {
        // 阻塞等待消息，超时500ms
        ModbusMessage msg;
        if (queue->tryDequeue(msg, 500)) {
            handleMessage(msg);
        }
    }
}

//处理传递到对列中的数据
void BusinessWorker::handleMessage(const ModbusMessage& msg)
{
    emit debugInfo(QString("处理任务: ID=%1, 值=0x%2")
                       .arg(msg.taskId)
                       .arg(msg.registerValue, 4, 16, QChar('0')));

    int projectId = 0;
    switch (msg.registerValue) {
    case 0x0001:
        projectId = 1;
        break;
    case 0x0002:
        projectId = 2;
        break;
    case 0x0003:
        projectId = 3;
        break;
    case 0x0004:
        projectId = 4;
        break;
    default:
        emit debugInfo(QString("未知的寄存器值: 0x%1，忽略")
                           .arg(msg.registerValue, 4, 16, QChar('0')));
        return;
    }

    emit debugInfo(QString("匹配到项目ID: %1，触发拍照").arg(projectId));

    // ===== 触发拍照信号 =====
    emit triggerCapture(projectId, msg);

    emit taskProcessed(msg);
}
