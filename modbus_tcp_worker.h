#ifndef MODBUS_TCP_WORKER_H
#define MODBUS_TCP_WORKER_H

#include "worker.h"

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

class modbus_tcp_server : public QTcpServer
{
    Q_OBJECT
public:
    modbus_tcp_server(QObject* parent = nullptr);
    ~modbus_tcp_server();

protected:
    void incomingConnection(qintptr socketDescriptor) override;

signals:
    void sig_rcv(const QString& client_ip, const QString& client_port, const QByteArray& frame);
    void sig_update_tcp_wdgt(const QString& client, const QString& dir, const QByteArray& frame);
    void sig_client_connected_notify(const QString& ip, const QString& port);
    void sig_client_disconnected_notify(const QString& ip, const QString& port);

public slots:
    void slot_send(QByteArray& frame);

private slots:
    void slot_read_ready();
    void slot_client_disconnected();

private:
    const quint8 _min_tcp_frame_length = 12;
    const quint32 _max_buff_size = 4096;
    QTcpSocket* _client = nullptr;
    QQueue<QByteArray> _trans_id_queue;
};

class modbus_tcp_worker : public worker
{
    Q_OBJECT
public:
    explicit modbus_tcp_worker(const QStringList& thread_params, QObject *parent = nullptr);
    ~modbus_tcp_worker();

signals:
    void sig_rcv(const QByteArray& frame);
    void sig_update_tcp_wdgt(const QString& client, const QString& dir, const QByteArray& frame);
    void sig_send(QByteArray& frame);
    void sig_update_client_status(const QString& notify);

public slots:
    void slot_quit_worker();
    void slot_rcv(const QString& client_ip, const QString& client_port, const QByteArray& frame);
    void slot_rtu_to_tcp(const QByteArray& frame);
    void slot_update_tcp_wdgt(const QString& client, const QString& dir, const QByteArray& frame);

private:
    void slot_client_connected_notify(const QString& ip, const QString& port);
    void slot_client_disconnected_notify(const QString& ip, const QString& port);
    enum thread_params_idx{
        idx_ip,
        idx_port,        
    };
private:    
    modbus_tcp_server* _modbus_tcp_server = nullptr;    
};

#endif // MODBUS_TCP_WORKER_H
