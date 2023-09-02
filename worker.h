#ifndef WORKER_H
#define WORKER_H

#include <QThread>
#include <QVector>
#include <QDateTime>
#include <QMutex>
#include <QWaitCondition>
#include <QUuid>
#include <QTimer>
#include <QQueue>

class worker : public QObject
{
    Q_OBJECT
public:
    explicit worker(const QStringList& thread_params, QObject *parent = nullptr) : QObject(parent), _thread_params(thread_params) {

    }

    ~worker() {
        thread.quit();
        thread.wait();
    }
protected:
    QThread thread;
    QStringList _thread_params;
    quint32 _wait_timeout = 1;
    quint8 _failure_count = 0;
    const quint8 _max_failure = 3;
};

#endif // WORKER_H
