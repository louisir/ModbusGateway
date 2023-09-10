#include "modbus_tcp_worker.h"

modbus_tcp_server::modbus_tcp_server(QObject* parent)
    : QTcpServer(parent)
{

}

modbus_tcp_server::~modbus_tcp_server()
{
    if(_client){
        _client->disconnectFromHost();
        _client->waitForDisconnected();
        _client->close();
        _client->deleteLater();
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
    // 读取客户端发送的数据
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client)
        return;

    QByteArray data = client->readAll();
    if(!data.isEmpty() && data.length() >= _min_tcp_frame_length){
        _trans_id_queue.enqueue(data.left(2));
        emit sig_rcv(client->peerAddress().toString(), QString::number(client->peerPort()), data);
    }
}

void modbus_tcp_server::slot_client_disconnected()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    assert(_client == client);
    client->deleteLater();
    _client = nullptr;
    emit sig_client_disconnected_notify(client->peerAddress().toString(), QString::number(client->peerPort()));
}

void modbus_tcp_server::slot_send(QByteArray& frame)
{
    if(!_client){
        return;
    }    
    // 检查 originalData 至少有两个字节
    if (frame.size() >= 2) {
        frame.replace(0, 2, _trans_id_queue.dequeue());
        _client->write(frame);
        emit sig_update_tcp_wdgt(QString("%1:%2").arg(_client->peerAddress().toString(), QString::number(_client->peerPort())), "->", frame);
    }
    return;
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
    quint16 port = _thread_params[idx_port].toUShort();
    if (!_modbus_tcp_server->listen(ip, port))
    {
//        emit error("Failed to start server.");
        qDebug() << "Failed to start server.";
        return;
    }
    this->moveToThread(&thread);
    thread.start();
}

modbus_tcp_worker::~modbus_tcp_worker()
{

}

void modbus_tcp_worker::slot_quit_worker()
{
    _modbus_tcp_server->close();
}

void modbus_tcp_worker::slot_rtu_to_tcp(const QByteArray& frame)
{
    QByteArray replicated_frame = QByteArray::fromRawData(frame.constData(), frame.size());
    emit sig_send(replicated_frame);
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
}
