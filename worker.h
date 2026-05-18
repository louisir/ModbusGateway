#ifndef WORKER_H
#define WORKER_H

#include <QCoreApplication>
#include <QThread>
#include <QVector>
#include <QDateTime>
#include <QMutex>
#include <QWaitCondition>
#include <QUuid>
#include <QTimer>
#include <QQueue>
#include <QMetaType>

class worker : public QObject
{
    Q_OBJECT
public:
    explicit worker(const QStringList& thread_params, QObject *parent = nullptr)
        : QObject(parent),
          _thread(new QThread),
          _owner_thread(QThread::currentThread()),
          _thread_params(thread_params) {
        qRegisterMetaType<QThread*>("QThread*");
    }

    virtual ~worker() {
        if(!_thread){
            return;
        }

        if(_thread->isRunning()){
            _thread->quit();
            if(QThread::currentThread() != _thread){
                _thread->wait();
            }
        }

        if(QThread::currentThread() == _thread || _thread->isRunning()){
            _thread->deleteLater();
        }else{
            delete _thread;
        }
        _thread = nullptr;
    }

protected:
    bool start_worker_thread(const char* init_method) {
        if(parent() || !_thread){
            return false;
        }
        if(_thread->isRunning()){
            return true;
        }

        moveToThread(_thread);
        _thread->start();

        bool ok = false;
        const bool invoked = QMetaObject::invokeMethod(
            this,
            init_method,
            Qt::BlockingQueuedConnection,
            Q_RETURN_ARG(bool, ok)
        );
        if(invoked && ok){
            return true;
        }

        move_to_owner_thread();
        _thread->quit();
        _thread->wait();
        return false;
    }

    void stop_worker_thread(const char* quit_method) {
        if(!_thread || !_thread->isRunning()){
            return;
        }

        // Drop queued cross-thread work so Stop cannot run stale requests before teardown.
        QCoreApplication::removePostedEvents(this);

        if(QThread::currentThread() == QObject::thread()){
            QMetaObject::invokeMethod(this, quit_method, Qt::DirectConnection);
            move_to_owner_thread();
            _thread->quit();
            return;
        }

        QMetaObject::invokeMethod(this, quit_method, Qt::BlockingQueuedConnection);
        move_to_owner_thread();
        _thread->quit();
        _thread->wait();
    }

protected slots:
    void slot_move_to_thread(QThread* target_thread) {
        moveToThread(target_thread);
    }

private:
    void move_to_owner_thread() {
        QThread* owner_thread = _owner_thread ? _owner_thread : QThread::currentThread();
        if(QObject::thread() == owner_thread){
            return;
        }
        if(QThread::currentThread() == QObject::thread()){
            slot_move_to_thread(owner_thread);
            return;
        }
        QMetaObject::invokeMethod(
            this,
            "slot_move_to_thread",
            Qt::BlockingQueuedConnection,
            Q_ARG(QThread*, owner_thread)
        );
    }

protected:
    QThread* _thread = nullptr;
    QThread* _owner_thread = nullptr;
    QStringList _thread_params;
    quint32 _wait_timeout = 1;
    quint8 _failure_count = 0;
    const quint8 _max_failure = 3;
};

#endif // WORKER_H
