#include "modbus_tcp_worker.h"

#ifdef Q_OS_WIN
#include <winsock2.h>
#else
#include <unistd.h>
#endif

namespace
{
void close_socket_descriptor(qintptr socketDescriptor)
{
    if(socketDescriptor == qintptr(-1)){
        return;
    }

#ifdef Q_OS_WIN
    ::closesocket(static_cast<SOCKET>(socketDescriptor));
#else
    ::close(static_cast<int>(socketDescriptor));
#endif
}
}

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
        client->abort();
        client->close();
        delete client;
    }
}

void modbus_tcp_server::incomingConnection(qintptr socketDescriptor)
{
    if(!_accepting_clients){
        reject_connection(socketDescriptor);
        return;
    }

    if(!_client){
        // 当有新的连接时，创建一个新的QTcpSocket来处理连接
        QTcpSocket* client = new QTcpSocket(this);
        client->setReadBufferSize(_max_buff_size);
        if(!client->setSocketDescriptor(socketDescriptor)){
            qWarning() << "Failed to bind accepted Modbus TCP socket descriptor:" << client->errorString();
            if(client->socketDescriptor() != -1){
                client->abort();
            }else{
                close_socket_descriptor(socketDescriptor);
            }
            delete client;
            return;
        }

        ++_session_id;
        if(_session_id == 0){
            _session_id = 1;
        }
        _pending_transactions = 0;
        _rx_buffer.clear();
        _client = client;

        // 为新的客户端连接设置数据到达处理函数
        connect(_client, &QTcpSocket::readyRead, this, &modbus_tcp_server::slot_read_ready);
        // 客户端断开
        connect(_client, &QTcpSocket::disconnected, this, &modbus_tcp_server::slot_client_disconnected);

        emit sig_client_connected_notify(_client->peerAddress().toString(), QString::number(_client->peerPort()));
    }else{
        reject_connection(socketDescriptor);
    }
}

void modbus_tcp_server::slot_set_accepting_clients(bool accepting)
{
    _accepting_clients = accepting;
}

void modbus_tcp_server::reject_connection(qintptr socketDescriptor)
{
    QTcpSocket socket;
    if(socket.setSocketDescriptor(socketDescriptor)){
        socket.abort();
    }else if(socket.socketDescriptor() != -1){
        socket.abort();
    }else{
        close_socket_descriptor(socketDescriptor);
    }
}

void modbus_tcp_server::slot_read_ready()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client || client != _client)
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
        if(_pending_transactions >= _max_pending_transactions){
            qWarning() << "Modbus TCP pending transaction queue is full. Rejecting request.";
            if(!send_gateway_exception(client, frame, 0x06)){
                return;
            }
            continue;
        }

        ++_pending_transactions;
        emit sig_rcv(client->peerAddress().toString(), QString::number(client->peerPort()), frame, _session_id);
    }
}

bool modbus_tcp_server::queue_response(QTcpSocket* client, const QByteArray& response)
{
    if(!client || response.isEmpty()){
        return false;
    }

    qint64 total_written = 0;
    while(total_written < response.size()){
        const qint64 written = client->write(response.constData() + total_written,
                                            response.size() - total_written);
        if(written <= 0){
            qWarning() << "Failed to queue Modbus TCP response:" << client->errorString();
            client->abort();
            return false;
        }
        total_written += written;
    }
    return true;
}

bool modbus_tcp_server::send_gateway_exception(QTcpSocket* client, const QByteArray& request_frame, quint8 exception_code)
{
    if(!client || request_frame.size() < _min_tcp_frame_length){
        return false;
    }

    const quint8 unit_id = static_cast<quint8>(request_frame.at(6));
    if(unit_id == 0){
        return true;
    }

    QByteArray response;
    response.append(request_frame.left(2));
    response.append(char(0x00));
    response.append(char(0x00));
    response.append(char(0x00));
    response.append(char(0x03));
    response.append(request_frame.at(6));
    response.append(static_cast<char>(static_cast<quint8>(request_frame.at(7)) | 0x80));
    response.append(static_cast<char>(exception_code));

    if(!queue_response(client, response)){
        return false;
    }
    emit sig_update_tcp_wdgt(QString("%1:%2").arg(client->peerAddress().toString(), QString::number(client->peerPort())), "->", response);
    return true;
}

void modbus_tcp_server::slot_client_disconnected()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if(!client || client != _client){
        if(client){
            client->deleteLater();
        }
        return;
    }
    const QString ip = client->peerAddress().toString();
    const QString port = QString::number(client->peerPort());
    const quint64 disconnected_session_id = _session_id;
    _rx_buffer.clear();
    _pending_transactions = 0;
    ++_session_id;
    if(_session_id == 0){
        _session_id = 1;
    }
    client->deleteLater();
    _client = nullptr;
    emit sig_client_disconnected_notify(ip, port, disconnected_session_id);
}

void modbus_tcp_server::slot_send(const QByteArray& frame, quint64 tcp_session_id)
{
    if(tcp_session_id != _session_id){
        return;
    }

    if(_pending_transactions > 0){
        --_pending_transactions;
    }else{
        qWarning() << "No pending Modbus TCP transaction for response.";
    }

    if(!_client){
        return;
    }
    if(frame.size() < _min_tcp_frame_length){
        qWarning() << "Invalid Modbus TCP response frame length.";
        return;
    }

    QByteArray response = frame;
    if(!queue_response(_client, response)){
        return;
    }
    emit sig_update_tcp_wdgt(QString("%1:%2").arg(_client->peerAddress().toString(), QString::number(_client->peerPort())), "->", response);
    return;
}

void modbus_tcp_server::slot_discard_pending_transaction(quint64 tcp_session_id)
{
    if(tcp_session_id != _session_id){
        return;
    }

    if(_pending_transactions > 0){
        --_pending_transactions;
    }else{
        qWarning() << "No pending Modbus TCP transaction to discard.";
    }
}

modbus_tcp_worker::modbus_tcp_worker(const QStringList& thread_params, QObject *parent)
    : worker{thread_params, parent}
{
    if(!start_worker_thread("slot_start_worker")){
        if(_last_error.isEmpty()){
            _last_error = "Failed to start Modbus TCP worker thread.";
        }
    }
}

modbus_tcp_worker::~modbus_tcp_worker()
{
    stop();
}

void modbus_tcp_worker::stop()
{
    stop_worker_thread("slot_quit_worker");
}

bool modbus_tcp_worker::slot_start_worker()
{
    if(_thread_params.size() <= idx_port){
        _last_error = "Invalid Modbus TCP configuration.";
        return false;
    }

    _modbus_tcp_server = new modbus_tcp_server(this);
    connect(_modbus_tcp_server, &modbus_tcp_server::sig_rcv, this, &modbus_tcp_worker::slot_rcv, Qt::DirectConnection);
    connect(_modbus_tcp_server, &modbus_tcp_server::sig_update_tcp_wdgt, this, &modbus_tcp_worker::slot_update_tcp_wdgt, Qt::DirectConnection);
    connect(_modbus_tcp_server, &modbus_tcp_server::sig_client_connected_notify, this, &modbus_tcp_worker::slot_client_connected_notify, Qt::DirectConnection);
    connect(_modbus_tcp_server, &modbus_tcp_server::sig_client_disconnected_notify, this, &modbus_tcp_worker::slot_client_disconnected_notify, Qt::DirectConnection);
    connect(this, &modbus_tcp_worker::sig_send, _modbus_tcp_server, &modbus_tcp_server::slot_send, Qt::DirectConnection);
    QHostAddress ip(_thread_params[idx_ip]);
    if(ip.isNull()){
        _last_error = QString("Invalid Modbus TCP listen address: %1").arg(_thread_params[idx_ip]);
        delete _modbus_tcp_server;
        _modbus_tcp_server = nullptr;
        return false;
    }
    bool port_ok = false;
    quint16 port = _thread_params[idx_port].toUShort(&port_ok);
    if(!port_ok || port == 0){
        _last_error = QString("Invalid Modbus TCP listen port: %1").arg(_thread_params[idx_port]);
        delete _modbus_tcp_server;
        _modbus_tcp_server = nullptr;
        return false;
    }
    if (!_modbus_tcp_server->listen(ip, port))
    {
        _last_error = _modbus_tcp_server->errorString();
        if(_last_error.isEmpty()){
            _last_error = "Failed to start Modbus TCP server.";
        }
        delete _modbus_tcp_server;
        _modbus_tcp_server = nullptr;
        return false;
    }
    _running = true;
    return true;
}

void modbus_tcp_worker::slot_quit_worker()
{
    if(_modbus_tcp_server){
        _modbus_tcp_server->slot_set_accepting_clients(false);
        _modbus_tcp_server->close();
        delete _modbus_tcp_server;
        _modbus_tcp_server = nullptr;
    }
    _running = false;
}

void modbus_tcp_worker::slot_set_accepting_clients(bool accepting)
{
    if(_modbus_tcp_server){
        _modbus_tcp_server->slot_set_accepting_clients(accepting);
    }
}

void modbus_tcp_worker::slot_rtu_to_tcp(const QByteArray& frame, quint64 tcp_session_id)
{
    if(_modbus_tcp_server){
        emit sig_send(frame, tcp_session_id);
    }
}

void modbus_tcp_worker::slot_rcv(const QString& client_ip, const QString& client_port, const QByteArray& frame, quint64 tcp_session_id)
{
    emit sig_rcv(frame, tcp_session_id);
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

void modbus_tcp_worker::slot_client_disconnected_notify(const QString& ip, const QString& port, quint64 tcp_session_id)
{
    emit sig_update_client_status(QString("%1:%2 disconnected").arg(ip, port));
    emit sig_client_disconnected(tcp_session_id);
}

void modbus_tcp_worker::slot_discard_pending_transaction(quint64 tcp_session_id)
{
    if(_modbus_tcp_server){
        _modbus_tcp_server->slot_discard_pending_transaction(tcp_session_id);
    }
}

bool modbus_tcp_worker::is_running() const
{
    return _running;
}

QString modbus_tcp_worker::last_error() const
{
    return _last_error;
}
