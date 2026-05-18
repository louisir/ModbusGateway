#include "modbus_rtu_worker.h"

modbus_rtu_worker::modbus_rtu_worker(const QStringList& thread_params, GatewayMode mode, QObject *parent)
    : worker{thread_params, parent},
      _mode(mode)
{
    if(!start_worker_thread("slot_start_worker")){
        if(_last_error.isEmpty()){
            _last_error = "Failed to start Modbus RTU worker thread.";
        }
    }
}

modbus_rtu_worker::~modbus_rtu_worker()
{
    stop();
}

void modbus_rtu_worker::stop()
{
    stop_worker_thread("slot_quit_worker");
}

bool modbus_rtu_worker::slot_start_worker()
{
    if(_thread_params.size() <= idx_flowctrl){
        _last_error = "Invalid Modbus RTU configuration.";
        return false;
    }

    _serial = new QSerialPort(this);
    _serial->setPortName(_thread_params[idx_name]);

    bool baud_ok = false;
    bool data_bits_ok = false;
    bool stop_bits_ok = false;
    bool parity_ok = false;
    bool flow_ctrl_ok = false;
    const qint32 baud_rate = _thread_params[idx_baudrate].toInt(&baud_ok);
    const auto data_bits = static_cast<QSerialPort::DataBits>(_thread_params[idx_databit].toInt(&data_bits_ok));
    const auto stop_bits = static_cast<QSerialPort::StopBits>(_thread_params[idx_stopbit].toInt(&stop_bits_ok));
    const auto parity = static_cast<QSerialPort::Parity>(_thread_params[idx_parity].toInt(&parity_ok));
    const auto flow_ctrl = static_cast<QSerialPort::FlowControl>(_thread_params[idx_flowctrl].toInt(&flow_ctrl_ok));

    if(!baud_ok || !data_bits_ok || !stop_bits_ok || !parity_ok || !flow_ctrl_ok ||
        !_serial->setBaudRate(baud_rate) ||
        !_serial->setDataBits(data_bits) ||
        !_serial->setStopBits(stop_bits) ||
        !_serial->setParity(parity) ||
        !_serial->setFlowControl(flow_ctrl)){
        _last_error = QString("invalid serial config: name = %1, BaudRate = %2, DataBits = %3, StopBits = %4, Parity = %5, FlowControl = %6")
                          .arg(
                              _thread_params[idx_name],
                              _thread_params[idx_baudrate],
                              _thread_params[idx_databit],
                              _thread_params[idx_stopbit],
                              _thread_params[idx_parity],
                              _thread_params[idx_flowctrl]
                              );
        delete _serial;
        _serial = nullptr;
        return false;
    }

    _serial->setReadBufferSize(2048);
    if(!_serial->open(QIODevice::ReadWrite)){
        _last_error = QString("open serial failed: name = %1, BaudRate = %2, DataBits = %3, StopBits = %4, Parity = %5, FlowControl = %6, error = %7")
                          .arg(
                              _thread_params[idx_name],
                              _thread_params[idx_baudrate],
                              _thread_params[idx_databit],
                              _thread_params[idx_stopbit],
                              _thread_params[idx_parity],
                              _thread_params[idx_flowctrl],
                              _serial->errorString()
                              );
        qWarning() << _last_error;
        delete _serial;
        _serial = nullptr;
        return false;
    }
    connect(_serial, &QSerialPort::readyRead, this, &modbus_rtu_worker::slot_ready_read, Qt::DirectConnection);

    _response_timer = new QTimer(this);
    _response_timer->setSingleShot(true);
    connect(_response_timer, &QTimer::timeout, this, &modbus_rtu_worker::slot_response_timeout);

    _response_guard_timer = new QTimer(this);
    _response_guard_timer->setSingleShot(true);
    connect(_response_guard_timer, &QTimer::timeout, this, &modbus_rtu_worker::slot_response_guard_finished);

    _running = true;
    return true;
}

void modbus_rtu_worker::slot_ready_read()
{
    const QByteArray data = _serial->readAll();
    if(data.isEmpty()){
        qWarning() << QString("serial recv empty data. serial port name = %1").arg(_thread_params[idx_name]);
        return;
    }

    if(_response_guard_active){
        qWarning() << "Discarding late Modbus RTU data after timeout:" << data.toHex();
        _cache.clear();
        if(_serial){
            _serial->clear(QSerialPort::Input);
        }
        if(_response_guard_timer){
            _response_guard_timer->start(_late_response_guard_ms);
        }
        return;
    }

    _cache.append(data);
    if(_cache.length() > _max_cache_size){
        qWarning() << "Modbus RTU receive cache overflow. Discarding buffered data.";
        _cache.clear();
        return;
    }

    QByteArray rtu_frame;
    while(get_complete_rtu_frame(rtu_frame)){
        handle_rtu_frame(rtu_frame);
        rtu_frame.clear();
    }
}

void modbus_rtu_worker::slot_quit_worker()
{
    slot_clear_pending_requests();
    _response_quarantine.clear();
    _response_guard_active = false;
    if(_response_guard_timer){
        _response_guard_timer->stop();
    }
    if(_serial){
        _serial->close();
        delete _serial;
        _serial = nullptr;
    }
    if(_response_timer){
        delete _response_timer;
        _response_timer = nullptr;
    }
    if(_response_guard_timer){
        delete _response_guard_timer;
        _response_guard_timer = nullptr;
    }
    _running = false;
}

void modbus_rtu_worker::slot_tcp_to_rtu(const QByteArray& adu, const QByteArray& transaction_id, quint64 tcp_session_id)
{
    if(_mode == GatewayMode::RtuToTcp){
        handle_tcp_response_adu(adu, transaction_id, tcp_session_id);
        return;
    }

    if(tcp_session_id == 0 || tcp_session_id <= _last_disconnected_tcp_session_id){
        emit sig_discard_tcp_transaction(tcp_session_id);
        return;
    }

    if(transaction_id.size() != 2){
        emit sig_discard_tcp_transaction(tcp_session_id);
        return;
    }

    pending_request request;
    request.adu = adu;
    request.transaction_id = transaction_id;
    request.tcp_session_id = tcp_session_id;

    quint8 exception_code = 0;
    if(!validate_request_adu(request.adu, exception_code)){
        qWarning() << "Invalid Modbus RTU ADU from TCP side:" << request.adu.toHex();
        reject_request(request, exception_code);
        return;
    }

    if(is_request_quarantined(request.adu)){
        qWarning() << "Rejecting Modbus RTU request during timeout recovery:" << request.adu.toHex();
        reject_request(request, 0x06);
        return;
    }

    if(_tx_queue.size() >= _max_tx_queue_size){
        qWarning() << "Modbus RTU request queue is full. Rejecting request.";
        reject_request(request, 0x06);
        return;
    }

    _tx_queue.enqueue(request);
    send_next_request();
}

void modbus_rtu_worker::slot_clear_pending_requests()
{
    if(_mode == GatewayMode::RtuToTcp){
        _tx_queue.clear();
        _pending_adu.clear();
        _pending_transaction_id.clear();
        _pending_tcp_session_id = 0;
        _waiting_response = false;
        _cache.clear();
        if(_response_timer){
            _response_timer->stop();
        }
        if(_serial){
            _serial->clear(QSerialPort::AllDirections);
        }
        return;
    }

    const bool abandoned_pending_request = _waiting_response && !_pending_adu.isEmpty();
    if(abandoned_pending_request){
        qWarning() << "Abandoning pending Modbus RTU request:" << _pending_adu.toHex();
        add_response_quarantine(_pending_adu);
    }

    _tx_queue.clear();
    _pending_adu.clear();
    _pending_transaction_id.clear();
    _pending_tcp_session_id = 0;
    _waiting_response = false;
    _cache.clear();
    if(_response_timer){
        _response_timer->stop();
    }
    if(_serial){
        _serial->clear(QSerialPort::AllDirections);
    }

    if(abandoned_pending_request){
        start_late_response_guard();
    }
}

void modbus_rtu_worker::slot_clear_pending_requests_for_session(quint64 tcp_session_id)
{
    if(_mode == GatewayMode::RtuToTcp){
        if(_waiting_response &&
            !_pending_adu.isEmpty() &&
            _pending_tcp_session_id == tcp_session_id){
            qWarning() << "Modbus TCP slave disconnected while an RTU request is pending:" << _pending_adu.toHex();
            emit_rtu_exception_response(_pending_adu, 0x0A);
            finish_pending_request();
        }
        return;
    }

    if(tcp_session_id > _last_disconnected_tcp_session_id){
        _last_disconnected_tcp_session_id = tcp_session_id;
    }

    QQueue<pending_request> retained_requests;
    while(!_tx_queue.isEmpty()){
        pending_request request = _tx_queue.dequeue();
        if(request.tcp_session_id != tcp_session_id){
            retained_requests.enqueue(request);
        }
    }
    _tx_queue = retained_requests;

    const bool abandoned_pending_request = _waiting_response &&
                                           !_pending_adu.isEmpty() &&
                                           _pending_tcp_session_id == tcp_session_id;
    if(abandoned_pending_request){
        qWarning() << "Abandoning pending Modbus RTU request for disconnected TCP session:" << _pending_adu.toHex();
        add_response_quarantine(_pending_adu);
        finish_pending_request();
        _cache.clear();
        if(_serial){
            _serial->clear(QSerialPort::AllDirections);
        }
        start_late_response_guard();
        return;
    }

    send_next_request();
}

bool modbus_rtu_worker::is_running() const
{
    return _running;
}

QString modbus_rtu_worker::last_error() const
{
    return _last_error;
}

void modbus_rtu_worker::slot_response_timeout()
{
    if(!_waiting_response || _pending_adu.isEmpty()){
        return;
    }

    const QByteArray request_adu = _pending_adu;
    if(_mode == GatewayMode::RtuToTcp){
        qWarning() << "Modbus TCP slave response timeout.";
        emit_rtu_exception_response(request_adu, 0x0B);
        finish_pending_request();
        return;
    }

    const QByteArray transaction_id = _pending_transaction_id;
    const quint64 tcp_session_id = _pending_tcp_session_id;
    qWarning() << "Modbus RTU response timeout.";
    add_response_quarantine(request_adu);
    finish_pending_request();
    emit_gateway_exception_response(request_adu, transaction_id, tcp_session_id, 0x0B);
    start_late_response_guard();
}

void modbus_rtu_worker::slot_response_guard_finished()
{
    _response_guard_active = false;
    _cache.clear();
    if(_serial){
        _serial->clear(QSerialPort::Input);
    }
    if(_mode == GatewayMode::RtuToTcp){
        return;
    }
    send_next_request();
}

bool modbus_rtu_worker::check_crc(const QByteArray& rtu_frame)
{
    if (rtu_frame.length() < _min_rtu_frame_length) {
        qWarning() << "Invalid Modbus RTU frame length.";
        return false;
    }
    QByteArray adu = rtu_frame.left(rtu_frame.length() - 2); // 去除CRC校验字段
    quint16 rcv_crc = (quint16(static_cast<quint8>(rtu_frame.at(rtu_frame.length() - 2))) << 8)
                       | quint16(static_cast<quint8>(rtu_frame.at(rtu_frame.length() - 1)));
    quint16 calc_crc = calc_modbus_rtu_crc(adu);

    if (rcv_crc != calc_crc) {
        qWarning() << "CRC check failed. Discarding frame.";
        return false;
    }

    return true;
}

quint8 modbus_rtu_worker::get_addr(const QByteArray& rtu_frame)
{
    quint8 addr = static_cast<quint8>(rtu_frame.at(idx_addr));
    if(addr == 0 || addr > _max_addr){
        return 0;
    }
    return addr;
}

quint8 modbus_rtu_worker::get_func_code(const QByteArray& rtu_frame)
{
    quint8 func_code = static_cast<quint8>(rtu_frame.at(idx_func));
    if(is_valid_func_code(func_code)){
        return func_code;
    }
    return 0;
}

quint16 modbus_rtu_worker::get_crc(const QByteArray& rtu_frame)
{
    quint8 crc_h = static_cast<quint8>(rtu_frame.at(rtu_frame.length() - 2));
    quint8 crc_l = static_cast<quint8>(rtu_frame.at(rtu_frame.length() - 1));
    return (quint16(crc_h) << 8) | quint16(crc_l);
}

bool modbus_rtu_worker::is_complete_rtu_frame(const QByteArray& rtu_frame)
{
    if(rtu_frame.length() >= _min_rtu_frame_length && get_addr(rtu_frame) != 0 && get_func_code(rtu_frame) != 0){
        quint16 crc_calc_value = calc_modbus_rtu_crc(rtu_frame.left(rtu_frame.length() - 2));
        quint16 crc_rcv_value = get_crc(rtu_frame);
        if(crc_calc_value == crc_rcv_value){
            return true;
        }
    }
    return false;
}

bool modbus_rtu_worker::get_complete_rtu_frame(QByteArray& rtu_frame)
{
    int first_candidate = -1;
    bool incomplete_first_candidate = false;

    for(int i = 0; i < _cache.length(); i++){
        quint8 byte_addr = static_cast<quint8>(_cache.at(i));
        if(byte_addr == 0x00 || byte_addr > _max_addr){
            continue;
        }

        if(first_candidate < 0){
            first_candidate = i;
        }

        const int frame_len = expected_rtu_frame_length(_cache, i);
        if(frame_len == 0){
            if(i == 0 || (first_candidate == 0 && incomplete_first_candidate)){
                incomplete_first_candidate = true;
                continue;
            }
            if(i > 0){
                _cache.remove(0, i);
            }
            return false;
        }
        if(frame_len < 0){
            continue;
        }
        if(i + frame_len > _cache.length()){
            if(i == 0 || (first_candidate == 0 && incomplete_first_candidate)){
                incomplete_first_candidate = true;
                continue;
            }
            if(i > 0){
                _cache.remove(0, i);
            }
            return false;
        }

        QByteArray t_rtu_frame = _cache.mid(i, frame_len);
        if(is_complete_rtu_frame(t_rtu_frame)){
            rtu_frame = t_rtu_frame;
            _cache.remove(0, i + frame_len);
            return true;
        }
    }

    if(first_candidate > 0){
        _cache.remove(0, first_candidate);
    }else if(first_candidate == 0 && !incomplete_first_candidate){
        _cache.remove(0, 1);
    }else if(first_candidate < 0){
        _cache.clear();
    }
    return false;
}

bool modbus_rtu_worker::is_valid_func_code(const quint8 fc)
{
    const quint8 normal_fc = fc & 0x7F;
    return normal_fc == fc_r_coil ||
           normal_fc == fc_r_discrete ||
           normal_fc == fc_r_holding ||
           normal_fc == fc_r_input ||
           normal_fc == fc_w_single_coil ||
           normal_fc == fc_w_single_holding ||
           normal_fc == fc_w_multiple_coils ||
           normal_fc == fc_w_multiple_holding;
}

quint16 modbus_rtu_worker::calc_modbus_rtu_crc(const QByteArray& data)
{
    static const quint16 wCRCTable[] = {
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
        0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
        0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
        0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
        0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
        0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
        0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
        0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
        0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
        0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
        0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
        0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
        0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
        0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
        0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
        0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
        0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
        0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
        0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
        0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
        0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
        0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
        0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
        0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
        0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
        0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
        0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
        0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
        0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
        0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040 };

    quint8 nTemp;
    quint16 wCRCWord = 0xFFFF;
    quint16 wLength = data.length();
    const quint8* nData = (const quint8*)data.data();
    while (wLength--)
    {
        nTemp = *nData++ ^ wCRCWord;
        wCRCWord >>= 8;
        wCRCWord  ^= wCRCTable[(nTemp & 0xFF)];
    }
    // 高低字节交换
    return (wCRCWord<<8) | (wCRCWord>>8);
} // End: CRC16

QByteArray modbus_rtu_worker::append_crc(const QByteArray& adu)
{
    QByteArray modbus_rtu_frame = adu;
    quint16 crc = calc_modbus_rtu_crc(adu);
    modbus_rtu_frame.append(static_cast<quint8>(crc >> 8));
    modbus_rtu_frame.append(static_cast<quint8>(crc & 0xFF));
    return modbus_rtu_frame;
}

int modbus_rtu_worker::expected_rtu_frame_length(const QByteArray& data, int offset)
{
    if(_mode == GatewayMode::RtuToTcp){
        return expected_rtu_request_frame_length(data, offset);
    }

    if(data.length() - offset < 2){
        return 0;
    }

    const quint8 fc = static_cast<quint8>(data.at(offset + idx_func));
    if(!is_valid_func_code(fc)){
        return -1;
    }

    if(fc & 0x80){
        return 5;
    }

    switch(fc){
    case fc_r_coil:
    case fc_r_discrete:
    case fc_r_holding:
    case fc_r_input:
        if(data.length() - offset < 3){
            return 0;
        }
        return 3 + static_cast<quint8>(data.at(offset + idx_data)) + 2;
    case fc_w_single_coil:
    case fc_w_single_holding:
    case fc_w_multiple_coils:
    case fc_w_multiple_holding:
        return 8;
    default:
        return -1;
    }
}

int modbus_rtu_worker::expected_rtu_request_frame_length(const QByteArray& data, int offset)
{
    if(data.length() - offset < 2){
        return 0;
    }

    const quint8 fc = static_cast<quint8>(data.at(offset + idx_func));
    if(!is_valid_func_code(fc) || (fc & 0x80)){
        return -1;
    }

    switch(fc){
    case fc_r_coil:
    case fc_r_discrete:
    case fc_r_holding:
    case fc_r_input:
    case fc_w_single_coil:
    case fc_w_single_holding:
        return 8;
    case fc_w_multiple_coils:
    case fc_w_multiple_holding:
        if(data.length() - offset < 7){
            return 0;
        }
        return 9 + static_cast<quint8>(data.at(offset + 6));
    default:
        return -1;
    }
}

bool modbus_rtu_worker::is_response_for_pending_request(const QByteArray& rtu_frame)
{
    if(!_waiting_response || _pending_adu.length() < 2 || rtu_frame.length() < _min_rtu_frame_length){
        return false;
    }

    const QByteArray response_adu = rtu_frame.left(rtu_frame.length() - 2);
    if(response_adu.length() < 3){
        return false;
    }

    const quint8 request_addr = static_cast<quint8>(_pending_adu.at(idx_addr));
    const quint8 request_fc = static_cast<quint8>(_pending_adu.at(idx_func));
    const quint8 response_addr = static_cast<quint8>(response_adu.at(idx_addr));
    const quint8 response_fc = static_cast<quint8>(response_adu.at(idx_func));

    if(response_addr != request_addr){
        return false;
    }

    if(response_fc == (request_fc | 0x80)){
        return response_adu.length() == 3 && static_cast<quint8>(response_adu.at(idx_data)) != 0;
    }

    if(response_fc != request_fc){
        return false;
    }

    return is_normal_response_for_pending_request(response_adu);
}

bool modbus_rtu_worker::is_normal_response_for_pending_request(const QByteArray& response_adu)
{
    const quint8 request_fc = static_cast<quint8>(_pending_adu.at(idx_func));

    switch(request_fc){
    case fc_r_coil:
    case fc_r_discrete:
    case fc_r_holding:
    case fc_r_input: {
        const int expected_byte_count = expected_read_response_byte_count(_pending_adu);
        if(expected_byte_count < 0 || response_adu.length() != 3 + expected_byte_count){
            return false;
        }
        return static_cast<quint8>(response_adu.at(idx_data)) == expected_byte_count;
    }
    case fc_w_single_coil:
    case fc_w_single_holding:
        return response_adu == _pending_adu;
    case fc_w_multiple_coils:
    case fc_w_multiple_holding:
        return _pending_adu.length() >= 6 &&
               response_adu.length() == 6 &&
               response_adu == _pending_adu.left(6);
    default:
        return false;
    }
}

bool modbus_rtu_worker::validate_request_adu(const QByteArray& adu, quint8& exception_code)
{
    exception_code = 0;
    if(adu.length() < 2){
        return false;
    }

    const quint8 addr = static_cast<quint8>(adu.at(idx_addr));
    const quint8 fc = static_cast<quint8>(adu.at(idx_func));
    if(addr > _max_addr){
        exception_code = 0x0A;
        return false;
    }
    if(!is_valid_func_code(fc) || (fc & 0x80)){
        exception_code = 0x01;
        return false;
    }

    const bool broadcast = addr == 0;
    switch(fc){
    case fc_r_coil:
    case fc_r_discrete:
    case fc_r_holding:
    case fc_r_input: {
        if(broadcast || adu.length() != 6){
            exception_code = 0x03;
            return false;
        }
        const quint16 start_addr = read_be_u16(adu, 2);
        const quint16 quantity = read_be_u16(adu, 4);
        const quint16 max_quantity = (fc == fc_r_coil || fc == fc_r_discrete) ? 2000 : 125;
        if(quantity == 0 || quantity > max_quantity || !is_valid_address_range(start_addr, quantity)){
            exception_code = 0x03;
            return false;
        }
        return true;
    }
    case fc_w_single_coil: {
        if(adu.length() != 6){
            exception_code = 0x03;
            return false;
        }
        const quint16 value = read_be_u16(adu, 4);
        if(value != 0x0000 && value != 0xFF00){
            exception_code = 0x03;
            return false;
        }
        return true;
    }
    case fc_w_single_holding:
        if(adu.length() != 6){
            exception_code = 0x03;
            return false;
        }
        return true;
    case fc_w_multiple_coils: {
        if(adu.length() < 7){
            exception_code = 0x03;
            return false;
        }
        const quint16 start_addr = read_be_u16(adu, 2);
        const quint16 quantity = read_be_u16(adu, 4);
        const quint8 byte_count = static_cast<quint8>(adu.at(6));
        const quint16 expected_byte_count = static_cast<quint16>((quantity + 7) / 8);
        if(quantity == 0 || quantity > 1968 || !is_valid_address_range(start_addr, quantity) ||
            byte_count != expected_byte_count || adu.length() != 7 + byte_count){
            exception_code = 0x03;
            return false;
        }
        return true;
    }
    case fc_w_multiple_holding: {
        if(adu.length() < 7){
            exception_code = 0x03;
            return false;
        }
        const quint16 start_addr = read_be_u16(adu, 2);
        const quint16 quantity = read_be_u16(adu, 4);
        const quint8 byte_count = static_cast<quint8>(adu.at(6));
        const quint16 expected_byte_count = static_cast<quint16>(quantity * 2);
        if(quantity == 0 || quantity > 123 || !is_valid_address_range(start_addr, quantity) ||
            byte_count != expected_byte_count || adu.length() != 7 + byte_count){
            exception_code = 0x03;
            return false;
        }
        return true;
    }
    default:
        exception_code = 0x01;
        return false;
    }
}

quint16 modbus_rtu_worker::read_be_u16(const QByteArray& data, int offset) const
{
    return (quint16(static_cast<quint8>(data.at(offset))) << 8)
           | quint16(static_cast<quint8>(data.at(offset + 1)));
}

bool modbus_rtu_worker::is_valid_address_range(quint16 start_addr, quint16 quantity) const
{
    return quantity > 0 && (quint32(start_addr) + quint32(quantity)) <= 0x10000u;
}

int modbus_rtu_worker::expected_read_response_byte_count(const QByteArray& request_adu) const
{
    if(request_adu.length() != 6){
        return -1;
    }

    const quint8 fc = static_cast<quint8>(request_adu.at(idx_func));
    const quint16 start_addr = read_be_u16(request_adu, 2);
    const quint16 quantity = read_be_u16(request_adu, 4);
    if(!is_valid_address_range(start_addr, quantity)){
        return -1;
    }

    switch(fc){
    case fc_r_coil:
    case fc_r_discrete:
        return (quantity + 7) / 8;
    case fc_r_holding:
    case fc_r_input:
        return quantity * 2;
    default:
        return -1;
    }
}

bool modbus_rtu_worker::write_full_frame(const QByteArray& rtu_frame)
{
    if(!_serial || !_serial->isOpen()){
        return false;
    }

    qint64 total_written = 0;
    while(total_written < rtu_frame.length()){
        const qint64 written = _serial->write(rtu_frame.constData() + total_written,
                                              rtu_frame.length() - total_written);
        if(written < 0){
            return false;
        }
        if(written == 0){
            if(!_serial->waitForBytesWritten(_serial_write_timeout_ms)){
                return false;
            }
            continue;
        }
        total_written += written;
    }

    while(_serial->bytesToWrite() > 0){
        if(!_serial->waitForBytesWritten(_serial_write_timeout_ms)){
            return false;
        }
    }

    return true;
}

void modbus_rtu_worker::reject_request(const pending_request& request, quint8 exception_code)
{
    if(request.adu.length() < 2 ||
        request.transaction_id.size() != 2 ||
        static_cast<quint8>(request.adu.at(idx_addr)) == 0 ||
        exception_code == 0){
        emit sig_discard_tcp_transaction(request.tcp_session_id);
        return;
    }

    emit_gateway_exception_response(request.adu, request.transaction_id, request.tcp_session_id, exception_code);
}

bool modbus_rtu_worker::dequeue_rejected_request(pending_request& request)
{
    quint8 exception_code = 0;
    if(!validate_request_adu(request.adu, exception_code)){
        qWarning() << "Invalid Modbus RTU ADU from TCP side:" << request.adu.toHex();
        reject_request(request, exception_code);
        return true;
    }

    prune_response_quarantine();
    if(is_request_quarantined(request.adu)){
        qWarning() << "Rejecting queued Modbus RTU request during timeout recovery:" << request.adu.toHex();
        reject_request(request, 0x06);
        return true;
    }

    return false;
}

void modbus_rtu_worker::add_response_quarantine(const QByteArray& request_adu)
{
    if(request_adu.length() < 2 || static_cast<quint8>(request_adu.at(idx_addr)) == 0){
        return;
    }

    prune_response_quarantine();

    const quint8 addr = static_cast<quint8>(request_adu.at(idx_addr));
    const quint8 func = static_cast<quint8>(request_adu.at(idx_func));
    const qint64 expires_at_ms = QDateTime::currentMSecsSinceEpoch() + _timeout_quarantine_ms;

    for(response_quarantine_entry& entry : _response_quarantine){
        if(entry.addr == addr && entry.func == func){
            entry.expires_at_ms = qMax(entry.expires_at_ms, expires_at_ms);
            return;
        }
    }

    response_quarantine_entry entry;
    entry.addr = addr;
    entry.func = func;
    entry.expires_at_ms = expires_at_ms;
    _response_quarantine.append(entry);
}

void modbus_rtu_worker::prune_response_quarantine()
{
    const qint64 now_ms = QDateTime::currentMSecsSinceEpoch();
    for(int i = _response_quarantine.size() - 1; i >= 0; --i){
        if(_response_quarantine.at(i).expires_at_ms <= now_ms){
            _response_quarantine.removeAt(i);
        }
    }
}

bool modbus_rtu_worker::is_request_quarantined(const QByteArray& request_adu)
{
    if(request_adu.length() < 2 || static_cast<quint8>(request_adu.at(idx_addr)) == 0){
        return false;
    }

    prune_response_quarantine();

    const quint8 addr = static_cast<quint8>(request_adu.at(idx_addr));
    const quint8 func = static_cast<quint8>(request_adu.at(idx_func));
    for(const response_quarantine_entry& entry : _response_quarantine){
        if(entry.addr == addr && entry.func == func){
            return true;
        }
    }
    return false;
}

void modbus_rtu_worker::handle_rtu_frame(const QByteArray& rtu_frame)
{
    if(_mode == GatewayMode::RtuToTcp){
        handle_rtu_request_frame(rtu_frame);
        return;
    }

    if(!is_response_for_pending_request(rtu_frame)){
        qWarning() << "Ignoring unexpected Modbus RTU frame:" << rtu_frame.toHex();
        return;
    }

    if(_response_timer){
        _response_timer->stop();
    }

    emit sig_rcv(rtu_frame, _pending_transaction_id, _pending_tcp_session_id);
    emit sig_update_rtu_wdgt("->", rtu_frame);
    finish_pending_request();
    send_next_request();
}

void modbus_rtu_worker::handle_rtu_request_frame(const QByteArray& rtu_frame)
{
    emit sig_update_rtu_wdgt("->", rtu_frame);

    if(_waiting_response){
        qWarning() << "Rejecting concurrent Modbus RTU request while a TCP response is pending:" << rtu_frame.toHex();
        emit_rtu_exception_response(rtu_frame.left(rtu_frame.length() - 2), 0x06);
        return;
    }

    const QByteArray request_adu = rtu_frame.left(rtu_frame.length() - 2);
    quint8 exception_code = 0;
    if(!validate_request_adu(request_adu, exception_code)){
        qWarning() << "Invalid Modbus RTU request from serial side:" << request_adu.toHex();
        emit_rtu_exception_response(request_adu, exception_code);
        return;
    }

    _pending_adu = request_adu;
    _pending_transaction_id = next_transaction_id();
    _pending_tcp_session_id = _reverse_tcp_session_id;
    _waiting_response = true;
    if(_response_timer){
        _response_timer->start(_response_timeout_ms);
    }

    emit sig_rcv(rtu_frame, _pending_transaction_id, _pending_tcp_session_id);
}

void modbus_rtu_worker::handle_tcp_response_adu(const QByteArray& adu, const QByteArray& transaction_id, quint64 tcp_session_id)
{
    if(!_waiting_response ||
        _pending_adu.isEmpty() ||
        transaction_id != _pending_transaction_id ||
        tcp_session_id != _pending_tcp_session_id){
        qWarning() << "Ignoring unexpected Modbus TCP response for RTU side:" << adu.toHex();
        return;
    }

    const QByteArray rtu_frame = append_crc(adu);
    if(!is_response_for_pending_request(rtu_frame)){
        qWarning() << "Invalid Modbus TCP response for pending RTU request:" << adu.toHex();
        emit_rtu_exception_response(_pending_adu, 0x0B);
        finish_pending_request();
        return;
    }

    if(!write_full_frame(rtu_frame)){
        qWarning() << "Failed to write Modbus RTU response frame.";
        finish_pending_request();
        return;
    }

    emit sig_update_rtu_wdgt("<-", rtu_frame);
    finish_pending_request();
}

void modbus_rtu_worker::send_next_request()
{
    if(_waiting_response || _response_guard_active || _tx_queue.isEmpty()){
        return;
    }
    if(!_serial || !_serial->isOpen()){
        qWarning() << "Modbus RTU serial port is not open.";
        while(!_tx_queue.isEmpty()){
            reject_request(_tx_queue.dequeue(), 0x0A);
        }
        return;
    }

    prune_response_quarantine();
    while(!_tx_queue.isEmpty()){
        pending_request next_request = _tx_queue.head();
        if(!dequeue_rejected_request(next_request)){
            break;
        }
        _tx_queue.dequeue();
    }
    if(_tx_queue.isEmpty()){
        return;
    }

    const pending_request request = _tx_queue.dequeue();
    _pending_adu = request.adu;
    _pending_transaction_id = request.transaction_id;
    _pending_tcp_session_id = request.tcp_session_id;
    const QByteArray modbus_rtu_frame = append_crc(_pending_adu);
    _serial->clear(QSerialPort::Input);
    if(!write_full_frame(modbus_rtu_frame)){
        qWarning() << "Failed to write complete Modbus RTU frame.";
        const QByteArray failed_adu = _pending_adu;
        const QByteArray transaction_id = _pending_transaction_id;
        const quint64 tcp_session_id = _pending_tcp_session_id;
        _serial->clear(QSerialPort::Output);
        add_response_quarantine(failed_adu);
        finish_pending_request();
        emit_gateway_exception_response(failed_adu, transaction_id, tcp_session_id, 0x0A);
        start_late_response_guard();
        return;
    }

    emit sig_update_rtu_wdgt("<-", modbus_rtu_frame);

    const quint8 addr = static_cast<quint8>(_pending_adu.at(idx_addr));
    if(addr == 0){
        const quint64 tcp_session_id = _pending_tcp_session_id;
        finish_pending_request();
        emit sig_discard_tcp_transaction(tcp_session_id);
        start_late_response_guard();
        return;
    }

    _waiting_response = true;
    if(_response_timer){
        _response_timer->start(_response_timeout_ms);
    }
}

void modbus_rtu_worker::finish_pending_request()
{
    _waiting_response = false;
    _pending_adu.clear();
    _pending_transaction_id.clear();
    _pending_tcp_session_id = 0;
    if(_response_timer){
        _response_timer->stop();
    }
}

void modbus_rtu_worker::start_late_response_guard()
{
    _response_guard_active = true;
    _cache.clear();
    if(_serial){
        _serial->clear(QSerialPort::Input);
    }

    if(_response_guard_timer && _late_response_guard_ms > 0){
        _response_guard_timer->start(_late_response_guard_ms);
        return;
    }

    slot_response_guard_finished();
}

void modbus_rtu_worker::emit_gateway_exception_response(const QByteArray& request_adu, const QByteArray& transaction_id, quint64 tcp_session_id, quint8 exception_code)
{
    if(request_adu.length() < 2 ||
        transaction_id.size() != 2 ||
        static_cast<quint8>(request_adu.at(idx_addr)) == 0 ||
        exception_code == 0){
        emit sig_discard_tcp_transaction(tcp_session_id);
        return;
    }

    QByteArray exception_adu;
    exception_adu.append(request_adu.at(idx_addr));
    exception_adu.append(static_cast<char>(static_cast<quint8>(request_adu.at(idx_func)) | 0x80));
    exception_adu.append(static_cast<char>(exception_code));
    emit sig_rcv(append_crc(exception_adu), transaction_id, tcp_session_id);
}

void modbus_rtu_worker::emit_rtu_exception_response(const QByteArray& request_adu, quint8 exception_code)
{
    if(request_adu.length() < 2 ||
        static_cast<quint8>(request_adu.at(idx_addr)) == 0 ||
        exception_code == 0){
        return;
    }

    QByteArray exception_adu;
    exception_adu.append(request_adu.at(idx_addr));
    exception_adu.append(static_cast<char>(static_cast<quint8>(request_adu.at(idx_func)) | 0x80));
    exception_adu.append(static_cast<char>(exception_code));

    const QByteArray rtu_frame = append_crc(exception_adu);
    if(!write_full_frame(rtu_frame)){
        qWarning() << "Failed to write Modbus RTU exception frame.";
        return;
    }
    emit sig_update_rtu_wdgt("<-", rtu_frame);
}

QByteArray modbus_rtu_worker::next_transaction_id()
{
    const quint16 transaction_id = _next_transaction_id++;
    if(_next_transaction_id == 0){
        _next_transaction_id = 1;
    }

    QByteArray result;
    result.append(static_cast<char>((transaction_id >> 8) & 0xFF));
    result.append(static_cast<char>(transaction_id & 0xFF));
    return result;
}
