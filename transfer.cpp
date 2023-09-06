#include "transfer.h"

#include <QDebug>
#include <QIODevice>

transfer::transfer(QObject */*parent*/)
{

}

void transfer::slot_rcv_from_rtu(const QByteArray& frame)
{
    QByteArray adu = frame.left(frame.length() - 2);
    quint16 adu_len = adu.length();
    QByteArray mbap;
    mbap.append(4, 0x00);
    QDataStream stream(&mbap, QIODevice::Append);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << adu_len;
    QByteArray modbus_tcp_frame;
    modbus_tcp_frame.append(mbap);
    modbus_tcp_frame.append(adu);
    emit sig_rtu_to_tcp(modbus_tcp_frame);
}

void transfer::slot_rcv_from_tcp(const QByteArray& frame)
{
    emit sig_tcp_to_rtu(frame.mid(6));
}


