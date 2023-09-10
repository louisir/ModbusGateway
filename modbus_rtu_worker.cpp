#include "modbus_rtu_worker.h"

modbus_rtu_worker::modbus_rtu_worker(const QStringList& thread_params, QObject *parent)
    : worker{thread_params, parent}
{
    connect(&thread, &QThread::finished, this, &modbus_rtu_worker::slot_quit_worker);

    _serial = new QSerialPort(this);
    _serial->setPortName(_thread_params[idx_name]);
    _serial->setBaudRate((QSerialPort::BaudRate)_thread_params[idx_baudrate].toUInt());
    _serial->setDataBits((QSerialPort::DataBits)_thread_params[idx_databit].toUInt());
    _serial->setStopBits((QSerialPort::StopBits)_thread_params[idx_stopbit].toUInt());
    _serial->setParity((QSerialPort::Parity)_thread_params[idx_parity].toUInt());
    _serial->setFlowControl((QSerialPort::FlowControl)_thread_params[idx_flowctrl].toUInt());
    _serial->setReadBufferSize(2048);
    if(!_serial->open(QIODevice::ReadWrite)){
        qWarning() << QString("open serial failed: name = %1, BaudRate = %2, DataBits = %3, StopBits = %4, Parity = %5, FlowControl = %6")
                          .arg(
                              _thread_params[idx_name],
                              _thread_params[idx_baudrate],
                              _thread_params[idx_databit],
                              _thread_params[idx_stopbit],
                              _thread_params[idx_parity],
                              _thread_params[idx_flowctrl]
                              );
        return;
    }
    connect(_serial, &QSerialPort::readyRead, this, &modbus_rtu_worker::slot_ready_read, Qt::DirectConnection);
    this->moveToThread(&thread);
    thread.start();
}

modbus_rtu_worker::~modbus_rtu_worker()
{

}

void modbus_rtu_worker::slot_ready_read()
{
    if(_cache.length() > _max_cache_size){
        _cache.clear();
    }
    QByteArray rtu_frame;
    rtu_frame.append(_serial->readAll());
    while (_serial->waitForReadyRead(_wait_timeout)) {
        rtu_frame.append(_serial->readAll());
    }    
    if(rtu_frame.isEmpty()){
        qWarning() << QString("serial recv empty data. serial port name = %1").arg(_thread_params[idx_name]);
        return;
    }else{
        if(rtu_frame.length() >= _min_rtu_frame_length){
            if(is_complete_rtu_frame(rtu_frame)){
                emit sig_rcv(rtu_frame);
                emit sig_update_rtu_wdgt("->", rtu_frame);
            }else{
                _cache.append(rtu_frame);
                rtu_frame.clear();
                if(get_complete_rtu_frame(rtu_frame)){
                    emit sig_rcv(rtu_frame);
                    emit sig_update_rtu_wdgt("->", rtu_frame);
                }
            }
            return;
        }else{
            _cache.append(rtu_frame);
        }
    }
}

void modbus_rtu_worker::slot_quit_worker()
{
    _serial->close();
}

void modbus_rtu_worker::slot_tcp_to_rtu(const QByteArray& adu)
{
    QByteArray modbus_rtu_frame = adu;
    quint16 crc = calc_modbus_rtu_crc(adu);
    modbus_rtu_frame.append(static_cast<quint8>(crc >> 8));
    modbus_rtu_frame.append(static_cast<quint8>(crc & 0xFF));
    _serial->write(modbus_rtu_frame);
    emit sig_update_rtu_wdgt("<-", modbus_rtu_frame);
}

bool modbus_rtu_worker::check_crc(const QByteArray& rtu_frame)
{
    if (rtu_frame.length() < 8) {
        qWarning() << "Invalid Modbus RTU frame length.";
        return false;
    }
    QByteArray adu = rtu_frame.left(rtu_frame.length() - 2); // 去除CRC校验字段
    quint16 rcv_crc = (static_cast<quint16>((rtu_frame.at(rtu_frame.length() - 2) << 8) | rtu_frame.at(rtu_frame.length() - 1)));
    quint16 calc_crc = calc_modbus_rtu_crc(adu);

    if (rcv_crc != calc_crc) {
        qWarning() << "CRC check failed. Discarding frame.";
        return false;
    }

    return true;
}

quint8 modbus_rtu_worker::get_addr(const QByteArray& rtu_frame)
{
    quint8 addr = rtu_frame.at(idx_addr);
    if(addr == 0 || addr > _max_addr){
        return 0;
    }
    return addr;
}

quint8 modbus_rtu_worker::get_func_code(const QByteArray& rtu_frame)
{
    quint8 func_code = rtu_frame.at(idx_func);
    if(is_valid_func_code(func_code)){
        return func_code;
    }
    return 0;
}

quint16 modbus_rtu_worker::get_crc(const QByteArray& rtu_frame)
{
    quint8 crc_h = rtu_frame.at(rtu_frame.length() - 2);
    quint8 crc_l = rtu_frame.at(rtu_frame.length() - 1);
    return (quint16(crc_h) << 8) | quint16(crc_l);
}

bool modbus_rtu_worker::is_complete_rtu_frame(const QByteArray& rtu_frame)
{
    if(get_addr(rtu_frame) != 0 && get_func_code(rtu_frame) != 0){
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
    for(int i = 0; i < _cache.length(); i++){
        quint8 byte_addr = _cache.at(i);
        if(byte_addr > 0x00 && byte_addr <= _max_addr){
            if(i + 1 == _cache.length()){
                return false;
            }else{
                quint8 byte_fc = _cache.at(i + 1);
                if(!is_valid_func_code(byte_fc)){
                    continue;
                }else{
                    if(i + 2 == _cache.length()){
                        return false;
                    }
                    quint8 byte_data_len = _cache.at(i + 2);
                    if(i + 3 + byte_data_len + 2 > _cache.length()){
                        return false;
                    }else{
                        QByteArray t_rtu_frame = _cache.mid(i, 3 + byte_data_len + 2);
                        if(is_complete_rtu_frame(t_rtu_frame)){
                            rtu_frame = t_rtu_frame;
                            _cache.remove(0, i + 3 + byte_data_len + 2);
                            return true;
                        }else{
                            continue;
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool modbus_rtu_worker::is_valid_func_code(const quint8 fc)
{
    quint8 e_fc = fc - 0x80;
    if(fc == fc_r_coil ||
        fc == fc_r_discrete ||
        fc == fc_r_holding ||
        fc == fc_r_input ||
        fc == fc_w_single_coil ||
        fc == fc_w_single_holding ||
        fc == fc_w_multiple_coils ||
        fc == fc_w_multiple_holding){
        return true;
    }else if(e_fc == fc_r_coil ||
               e_fc == fc_r_discrete ||
               e_fc == fc_r_holding ||
               e_fc == fc_r_input ||
               e_fc == fc_w_single_coil ||
               e_fc == fc_w_single_holding ||
               e_fc == fc_w_multiple_coils ||
               e_fc == fc_w_multiple_holding){
        return true;
    }else{
        return false;
    }
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
