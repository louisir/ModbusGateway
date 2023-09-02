#ifndef MODBUS_RTU_WORKER_H
#define MODBUS_RTU_WORKER_H

#include "worker.h"

#include <QObject>
#include <QSerialPort>

class modbus_rtu_worker : public worker
{
    Q_OBJECT
public:
    explicit modbus_rtu_worker(const QStringList& thread_params, QObject *parent = nullptr);
    ~modbus_rtu_worker();

signals:
    void sig_rcv(const QByteArray& frame);
    void sig_update_rtu_wdgt(const QString& dir, const QByteArray& frame);

public slots:
    void slot_quit_worker();
    void slot_tcp_to_rtu(const QByteArray& frame);

private:
    enum thread_params_idx{
        idx_name,
        idx_baudrate,
        idx_databit,
        idx_stopbit,
        idx_parity,
        idx_flowctrl,        
    };

private slots:
    void slot_ready_read();

private:
    QSerialPort* _serial = nullptr;
};

#endif // MODBUS_RTU_WORKER_H
