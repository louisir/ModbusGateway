#include "modbus_tcp_worker.h"

modbus_tcp_server::modbus_tcp_server(QObject* parent)
    : QTcpServer(parent)
{

}

modbus_tcp_server::~modbus_tcp_server()
{
    if(_client){
        QTcpSocket* client = _client;
        _client = nullptr;
        client->disconnect(this);
        client->disconnectFromHost();
        if(client->state() != QAbstractSocket::UnconnectedState){
            client->waitForDisconnected();
        }
        client->close();
        delete client;
    }
}

void modbus_tcp_server::incomingConnection(qintptr socketDescriptor)
{
    if(!_client){
        // 当有新的连接时，创建一个新的QTcpSocket来处理连接
        _client = new QTcpSocket(this);
        _client->setReadBufferSize(_max_buff_size);
        _client->setSocketDescriptor(socketDescriptor);

        // 为新的客户端连接设置数据到达处理函数
        connect(_client, &QTcpSocket::readyRead, this, &modbus_tcp_server::slot_read_ready);
        // 客户端断开
        connect(_client, &QTcpSocket::disconnected, this, &modbus_tcp_server::slot_client_disconnected);

        emit sig_client_connected_notify(_client->peerAddress().toString(), QString::number(_client->peerPort()));
    }else{
        QTcpSocket socket;
        socket.setSocketDescriptor(socketDescriptor);
        socket.disconnectFromHost();
        socket.waitForDisconnected();
    }
}

void modbus_tcp_server::slot_read_ready()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client)
        return;

    _rx_buffer.append(client->readAll());
    if(_rx_buffer.length() > _max_buff_size){
        qWarning() << "Modbus TCP receive buffer overflow. Discarding buffered data.";
        _rx_buffer.clear();
        return;
    }

    while(_rx_buffer.length() >= _min_tcp_frame_length){
        const quint16 protocol_id = (quint16(static_cast<quint8>(_rx_buffer.at(2))) << 8)
                                    | quint16(static_cast<quint8>(_rx_buffer.at(3)));
        const quint16 pdu_len = (quint16(static_cast<quint8>(_rx_buffer.at(4))) << 8)
                                | quint16(static_cast<quint8>(_rx_buffer.at(5)));
        if(protocol_id != 0 || pdu_len < 2 || pdu_len > 254){
            qWarning() << "Invalid Modbus TCP MBAP header. Resynchronizing stream.";
            _rx_buffer.remove(0, 1);
            continue;
        }

        const int frame_len = 6 + pdu_len;
        if(_rx_buffer.length() < frame_len){
            break;
        }

        QByteArray frame = _rx_buffer.left(frame_len);
        _rx_buffer.remove(0, frame_len);
        _trans_id_queue.enqueue(frame.left(2));
        emit sig_rcv(client->peerAddress().toString(), QString::number(client->peerPort()), frame);
    }
}

void modbus_tcp_server::slot_client_disconnected()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    assert(_client == client);
    const QString ip = client->peerAddress().toString();
    const QString port = QString::number(client->peerPort());
    _rx_buffer.clear();
    _trans_id_queue.clear();
    client->deleteLater();
    _client = nullptr;
    emit sig_client_disconnected_notify(ip, port);
}

void modbus_tcp_server::slot_send(const QByteArray& frame)
{
    if(!_client){
        return;
    }    
    if(frame.size() < _min_tcp_frame_length){
        qWarning() << "Invalid Modbus TCP response frame length.";
        return;
    }
    if(_trans_id_queue.isEmpty()){
        qWarning() << "No pending Modbus TCP transaction id for response.";
        return;
    }

    QByteArray response = frame;
    response.replace(0, 2, _trans_id_queue.dequeue());
    _client->write(response);
    emit sig_update_tcp_wdgt(QString("%1:%2").arg(_client->peerAddress().toString(), QString::number(_client->peerPort())), "->", response);
    return;
}

void modbus_tcp_server::slot_discard_pending_transaction()
{
    if(!_trans_id_queue.isEmpty()){
        _trans_id_queue.dequeue();
    }
}

modbus_tcp_worker::modbus_tcp_worker(const QStringList& thread_params, QObject *parent)
    : worker{thread_params, parent}
{
    connect(&thread, &QThread::finished, this, &modbus_tcp_worker::slot_quit_worker);
    _modbus_tcp_server = new modbus_tcp_server(this);
    connect(_modbus_tcp_server, &modbus_tcp_server::sig_rcv, this, &modbus_tcp_worker::slot_rcv, Qt::DirectConnection);
    connect(_modbus_tcp_server, &modbus_tcp_server::sig_update_tcp_wdgt, this, &modbus_tcp_worker::slot_update_tcp_wdgt, Qt::DirectConnection);
    connect(_modbus_tcp_server, &modbus_tcp_server::sig_client_connected_notify, this, &modbus_tcp_worker::slot_client_connected_notify, Qt::DirectConnection);
    connect(_modbus_tcp_server, &modbus_tcp_server::sig_client_disconnected_notify, this, &modbus_tcp_worker::slot_client_disconnected_notify, Qt::DirectConnection);
    connect(this, &modbus_tcp_worker::sig_send, _modbus_tcp_server, &modbus_tcp_server::slot_send, Qt::DirectConnection);
    QHostAddress ip(_thread_params[idx_ip]);
    if(ip.isNull()){
        _last_error = QString("Invalid Modbus TCP listen address: %1").arg(_thread_params[idx_ip]);
        return;
    }
    bool port_ok = false;
    quint16 port = _thread_params[idx_port].toUShort(&port_ok);
    if(!port_ok || port == 0){
        _last_error = QString("Invalid Modbus TCP listen port: %1").arg(_thread_params[idx_port]);
        return;
    }
    if (!_modbus_tcp_server->listen(ip, port))
    {
        _last_error = _modbus_tcp_server->errorString();
        if(_last_error.isEmpty()){
            _last_error = "Failed to start Modbus TCP server.";
        }
        return;
    }
    _running = true;
}

modbus_tcp_worker::~modbus_tcp_worker()
{

}

void modbus_tcp_worker::slot_quit_worker()
{
    _modbus_tcp_server->close();
    _running = false;
}

void modbus_tcp_worker::slot_rtu_to_tcp(const QByteArray& frame)
{
    emit sig_send(frame);
}

void modbus_tcp_worker::slot_rcv(const QString& client_ip, const QString& client_port, const QByteArray& frame)
{
    emit sig_rcv(frame);
    emit sig_update_tcp_wdgt(QString("%1:%2").arg(client_ip, client_port), "<-", frame);
}

void modbus_tcp_worker::slot_update_tcp_wdgt(const QString& client, const QString& dir, const QByteArray& frame)
{
    emit sig_update_tcp_wdgt(client, dir, frame);
}

void modbus_tcp_worker::slot_client_connected_notify(const QString& ip, const QString& port)
{
    emit sig_update_client_status(QString("%1:%2 connected").arg(ip, port));
}

void modbus_tcp_worker::slot_client_disconnected_notify(const QString& ip, const QString& port)
{
    emit sig_update_client_status(QString("%1:%2 disconnected").arg(ip, port));
    emit sig_client_disconnected();
}

void modbus_tcp_worker::slot_discard_pending_transaction()
{
    _modbus_tcp_server->slot_discard_pending_transaction();
}

bool modbus_tcp_worker::is_running() const
{
    return _running;
}

QString modbus_tcp_worker::last_error() const
{
    return _last_error;
}
