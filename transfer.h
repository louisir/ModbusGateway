#ifndef TRANSFER_H
#define TRANSFER_H

#include <QObject>

class transfer : public QObject
{
    Q_OBJECT
public:
    explicit transfer(QObject *parent = nullptr);

signals:
    void sig_rtu_to_tcp(const QByteArray& frame);
    void sig_tcp_to_rtu(const QByteArray& adu);

public slots:
    void slot_rcv_from_rtu(const QByteArray& frame);
    void slot_rcv_from_tcp(const QByteArray& frame);
};

#endif // TRANSFER_H
