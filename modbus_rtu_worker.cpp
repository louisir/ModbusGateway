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
    connect(_serial, &QSerialPort::readyRead, this, &modbus_rtu_worker::slot_ready_read);
    this->moveToThread(&thread);
    thread.start();
}

modbus_rtu_worker::~modbus_rtu_worker()
{

}

void modbus_rtu_worker::slot_ready_read()
{
    QByteArray rtu_frame;
    rtu_frame.append(_serial->readAll());
    while (_serial->waitForReadyRead(_wait_timeout)) {
        rtu_frame.append(_serial->readAll());
    }
    emit sig_rcv(rtu_frame);
    emit sig_update_rtu_wdgt("->", rtu_frame);
}

void modbus_rtu_worker::slot_quit_worker()
{
    _serial->close();
}

void modbus_rtu_worker::slot_tcp_to_rtu(const QByteArray& adu)
{
    qDebug() << QString("tcp to rtu: %1").arg(adu.toHex());
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
        qDebug() << "Invalid Modbus RTU frame length.";
        return false;
    }
    qDebug() << rtu_frame.toHex();
    QByteArray adu = rtu_frame.left(rtu_frame.length() - 2); // 去除CRC校验字段
    quint16 rcv_crc = (static_cast<quint16>((rtu_frame.at(rtu_frame.length() - 2) << 8) | rtu_frame.at(rtu_frame.length() - 1)));
    quint16 calc_crc = calc_modbus_rtu_crc(adu);

    if (rcv_crc != calc_crc) {
        qDebug() << "CRC check failed. Discarding frame.";
        return false;
    }

    return true;
}

quint16 modbus_rtu_worker::calc_modbus_rtu_crc(const QByteArray &data)
{
    quint16 crc = 0xFFFF;

    for (int i = 0; i < data.size(); ++i) {
        crc ^= static_cast<quint16>(data.at(i));
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc = crc >> 1;
            }
        }
    }

    return ((crc & 0xFF) << 8) | ((crc >> 8) & 0xFF);
}
