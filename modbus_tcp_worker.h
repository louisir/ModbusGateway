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

public slots:
    void slot_send(const QByteArray& frame);

private slots:
    void slot_read_ready();
    void slot_client_disconnected();

private:
    QTcpSocket* _client = nullptr;
};

class modbus_tcp_worker : public worker
{
    Q_OBJECT
public:
    explicit modbus_tcp_worker(const QStringList& thread_params, QObject *parent = nullptr);
    ~modbus_tcp_worker();

signals:
    void sig_rcv(const QString& client_id, const QByteArray& frame);
    void sig_update_tcp_wdgt(const QString& client, const QString& dir, const QByteArray& frame);
    void sig_send(const QByteArray& frame);

public slots:
    void slot_quit_worker();
    void slot_rcv(const QString& client_ip, const QString& client_port, const QByteArray& frame);
    void slot_rtu_to_tcp(const QByteArray& frame);
    void slot_update_tcp_wdgt(const QString& client, const QString& dir, const QByteArray& frame);

private:
    enum thread_params_idx{
        idx_ip,
        idx_port,        
    };

private:
    modbus_tcp_server* _modbus_tcp_server = nullptr;
};

#endif // MODBUS_TCP_WORKER_H