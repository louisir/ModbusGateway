#include "transfer.h"

#include <QDebug>
#include <QIODevice>

transfer::transfer(QObject *parent)
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

void transfer::slot_rcv_from_tcp(const QString& client_id, const QByteArray& frame)
{
    QByteArray adu = frame.mid(6);
    QByteArray modbus_rtu_frame = adu;
    quint16 crc = calc_modbus_rtu_crc(adu);
    QDataStream stream(&modbus_rtu_frame, QIODevice::Append);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << crc;
    qDebug() << modbus_rtu_frame.toHex();
    emit sig_tcp_to_rtu(modbus_rtu_frame);
}

quint16 transfer::calc_modbus_rtu_crc(const QByteArray &data)
{
    quint16 crc = 0xFFFF; // Initial value

    for (char byte : data) {
        crc ^= byte; // XOR with byte

        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001; // XOR with polynomial if LSB is 1
            else
                crc >>= 1;
        }
    }

    return crc;
}
