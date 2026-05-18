#include "transfer.h"

//#include <QDebug>

namespace
{
constexpr int min_tcp_frame_length = 8;
constexpr int min_rtu_frame_length = 5;
constexpr int max_modbus_adu_length = 254;
}

transfer::transfer(QObject *parent)
    : QObject(parent)
{

}

void transfer::slot_rcv_from_rtu(const QByteArray& frame, const QByteArray& transaction_id, quint64 tcp_session_id)
{
    if(frame.length() < min_rtu_frame_length ||
        frame.length() > max_modbus_adu_length + 2 ||
        transaction_id.size() != 2){
        return;
    }

    QByteArray adu = frame.left(frame.length() - 2);
    if(adu.length() < 2 || adu.length() > max_modbus_adu_length){
        return;
    }

    const quint16 adu_len = static_cast<quint16>(adu.length());
    QByteArray mbap;
    mbap.append(transaction_id);
    mbap.append(char(0x00));
    mbap.append(char(0x00));
    mbap.append(static_cast<char>((adu_len >> 8) & 0xFF));
    mbap.append(static_cast<char>(adu_len & 0xFF));
    QByteArray modbus_tcp_frame;
    modbus_tcp_frame.append(mbap);
    modbus_tcp_frame.append(adu);
    emit sig_rtu_to_tcp(modbus_tcp_frame, tcp_session_id);
}

void transfer::slot_rcv_from_tcp(const QByteArray& frame, quint64 tcp_session_id)
{
    if(frame.length() < min_tcp_frame_length){
        return;
    }

    const quint16 adu_len = (quint16(static_cast<quint8>(frame.at(4))) << 8)
                            | quint16(static_cast<quint8>(frame.at(5)));
    if(adu_len < 2 || adu_len > max_modbus_adu_length || frame.length() != 6 + adu_len){
        return;
    }

    emit sig_tcp_to_rtu(frame.mid(6, adu_len), frame.left(2), tcp_session_id);
}


