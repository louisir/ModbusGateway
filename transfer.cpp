#include "transfer.h"

//#include <QDebug>
#include <QDataStream>
#include <QIODevice>

transfer::transfer(QObject */*parent*/)
{

}

void transfer::slot_rcv_from_rtu(const QByteArray& frame)
{
    if(frame.length() < 4){
        return;
    }

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
    if(frame.length() < 8){
        return;
    }

    const quint16 adu_len = (quint16(static_cast<quint8>(frame.at(4))) << 8)
                            | quint16(static_cast<quint8>(frame.at(5)));
    if(adu_len < 2 || frame.length() < 6 + adu_len){
        return;
    }

    emit sig_tcp_to_rtu(frame.mid(6, adu_len));
}


