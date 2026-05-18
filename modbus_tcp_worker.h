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
    void sig_rcv(const QString& client_ip, const QString& client_port, const QByteArray& frame, quint64 tcp_session_id);
    void sig_update_tcp_wdgt(const QString& client, const QString& dir, const QByteArray& frame);
    void sig_client_connected_notify(const QString& ip, const QString& port);
    void sig_client_disconnected_notify(const QString& ip, const QString& port, quint64 tcp_session_id);

public slots:
    void slot_set_accepting_clients(bool accepting);
    void slot_send(const QByteArray& frame, quint64 tcp_session_id);
    void slot_discard_pending_transaction(quint64 tcp_session_id);

private slots:
    void slot_read_ready();
    void slot_client_disconnected();

private:
    const quint8 _min_tcp_frame_length = 8;
    const quint32 _max_buff_size = 4096;
    const int _max_pending_transactions = 64;
    QTcpSocket* _client = nullptr;
    QByteArray _rx_buffer;
    quint64 _session_id = 0;
    int _pending_transactions = 0;
    bool _accepting_clients = false;

    void reject_connection(qintptr socketDescriptor);
    bool queue_response(QTcpSocket* client, const QByteArray& response);
    bool send_gateway_exception(QTcpSocket* client, const QByteArray& request_frame, quint8 exception_code);
};

class modbus_tcp_worker : public worker
{
    Q_OBJECT
public:
    explicit modbus_tcp_worker(const QStringList& thread_params, QObject *parent = nullptr);
    ~modbus_tcp_worker();
    void stop();
    bool is_running() const;
    QString last_error() const;

signals:
    void sig_rcv(const QByteArray& frame, quint64 tcp_session_id);
    void sig_update_tcp_wdgt(const QString& client, const QString& dir, const QByteArray& frame);
    void sig_send(const QByteArray& frame, quint64 tcp_session_id);
    void sig_update_client_status(const QString& notify);
    void sig_client_disconnected(quint64 tcp_session_id);

public slots:
    bool slot_start_worker();
    void slot_quit_worker();
    void slot_set_accepting_clients(bool accepting);
    void slot_rcv(const QString& client_ip, const QString& client_port, const QByteArray& frame, quint64 tcp_session_id);
    void slot_rtu_to_tcp(const QByteArray& frame, quint64 tcp_session_id);
    void slot_update_tcp_wdgt(const QString& client, const QString& dir, const QByteArray& frame);
    void slot_discard_pending_transaction(quint64 tcp_session_id);

private:
    void slot_client_connected_notify(const QString& ip, const QString& port);
    void slot_client_disconnected_notify(const QString& ip, const QString& port, quint64 tcp_session_id);
    enum thread_params_idx{
        idx_ip,
        idx_port,        
    };
private:    
    modbus_tcp_server* _modbus_tcp_server = nullptr;    
    bool _running = false;
    QString _last_error;
};

#endif // MODBUS_TCP_WORKER_H
