#include "modbus_tcp_worker.h"

modbus_tcp_server::modbus_tcp_server(QObject* parent)
    : QTcpServer(parent)
{

}

modbus_tcp_server::~modbus_tcp_server()
{
    _client->close();
    _client->deleteLater();
}

void modbus_tcp_server::incomingConnection(qintptr socketDescriptor)
{
    if(!_client){
    // 当有新的连接时，创建一个新的QTcpSocket来处理连接
    _client = new QTcpSocket(this);
    _client->setSocketDescriptor(socketDescriptor);

    // 为新的客户端连接设置数据到达处理函数
    connect(_client, &QTcpSocket::readyRead, this, &modbus_tcp_server::slot_read_ready);
    // 关联客户端套接字的断开信号
    connect(_client, &QTcpSocket::disconnected, this, &modbus_tcp_server::slot_client_disconnected);
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
    emit sig_rcv(client->peerAddress().toString(), QString::number(client->peerPort()), data);
}

void modbus_tcp_server::slot_client_disconnected()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    client->deleteLater();
}

void modbus_tcp_server::slot_send(const QByteArray& frame)
{
    _client->write(frame);
    emit sig_update_tcp_wdgt(QString("%1:%2").arg(_client->peerAddress().toString(), QString::number(_client->peerPort())), "->", frame);
}

modbus_tcp_worker::modbus_tcp_worker(const QStringList& thread_params, QObject *parent)
    : worker{thread_params, parent}
{
    connect(&thread, &QThread::finished, this, &modbus_tcp_worker::slot_quit_worker);
    _modbus_tcp_server = new modbus_tcp_server(this);
    connect(_modbus_tcp_server, &modbus_tcp_server::sig_rcv, this, &modbus_tcp_worker::slot_rcv, Qt::DirectConnection);
    connect(_modbus_tcp_server, &modbus_tcp_server::sig_update_tcp_wdgt, this, &modbus_tcp_worker::slot_update_tcp_wdgt, Qt::DirectConnection);
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
    emit sig_send(frame);
}

void modbus_tcp_worker::slot_rcv(const QString& client_ip, const QString& client_port, const QByteArray& frame)
{
    emit sig_rcv(QString("%1:%2").arg(client_ip, client_port), frame);
    emit sig_update_tcp_wdgt(QString("%1:%2").arg(client_ip, client_port), "<-", frame);
}

void modbus_tcp_worker::slot_update_tcp_wdgt(const QString& client, const QString& dir, const QByteArray& frame)
{
    emit sig_update_tcp_wdgt(client, dir, frame);
}
