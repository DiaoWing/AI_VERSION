#ifndef BUSINESSWORKER_H
#define BUSINESSWORKER_H

#include <QObject>
#include <QThread>
#include "messagequeue.h"

class BusinessWorker : public QObject
{
    Q_OBJECT

public:
    explicit BusinessWorker(QObject* parent = nullptr);
    ~BusinessWorker();

public slots:
    void start();
    void stop();

signals:
    void taskProcessed(const ModbusMessage& msg);
    void errorOccurred(const QString& error);
    void debugInfo(const QString& info);
    void triggerCapture(int projectId, const ModbusMessage& msg);

private slots:
    void processTasks();

private:
    void handleMessage(const ModbusMessage& msg);

    bool m_isRunning;
};

#endif // BUSINESSWORKER_H
