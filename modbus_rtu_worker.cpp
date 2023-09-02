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
    QByteArray data;
    while (_serial->waitForReadyRead(_wait_timeout)) {
        data = _serial->readAll();
        emit sig_rcv(data);
        emit sig_update_rtu_wdgt("->", data);
    }

    if (_serial->error() == QSerialPort::TimeoutError && data.isEmpty()) {
        qDebug() << "Read timeout occurred.";
    }
}

void modbus_rtu_worker::slot_quit_worker()
{
    _serial->close();
}

void modbus_rtu_worker::slot_tcp_to_rtu(const QByteArray& frame)
{
    qDebug() << QString("tcp to rtu: %1").arg(frame.toHex());
    _serial->write(frame);
    emit sig_update_rtu_wdgt("<-", frame);
}
