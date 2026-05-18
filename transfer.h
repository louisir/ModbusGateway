#ifndef TRANSFER_H
#define TRANSFER_H

#include <QObject>

class transfer : public QObject
{
    Q_OBJECT
public:
    explicit transfer(QObject *parent = nullptr);

signals:
    void sig_rtu_to_tcp(const QByteArray& frame, quint64 tcp_session_id);
    void sig_tcp_to_rtu(const QByteArray& adu, const QByteArray& transaction_id, quint64 tcp_session_id);

public slots:
    void slot_rcv_from_rtu(const QByteArray& frame, const QByteArray& transaction_id, quint64 tcp_session_id);
    void slot_rcv_from_tcp(const QByteArray& frame, quint64 tcp_session_id);
};

#endif // TRANSFER_H
