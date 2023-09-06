#ifndef MODBUS_RTU_WORKER_H
#define MODBUS_RTU_WORKER_H

#include "worker.h"

#include <QObject>
#include <QSerialPort>

class modbus_rtu_worker : public worker
{
    Q_OBJECT
public:
    explicit modbus_rtu_worker(const QStringList& thread_params, QObject *parent = nullptr);
    ~modbus_rtu_worker();

signals:
    void sig_rcv(const QByteArray& frame);
    void sig_update_rtu_wdgt(const QString& dir, const QByteArray& frame);

public slots:
    void slot_quit_worker();
    void slot_tcp_to_rtu(const QByteArray& adu);

private:
    enum thread_params_idx{
        idx_name,
        idx_baudrate,
        idx_databit,
        idx_stopbit,
        idx_parity,
        idx_flowctrl,        
    };

    enum rtu_frame_idx{
        idx_addr,
        idx_func,
        idx_data,
    };

    enum rtu_func_code{
        fc_r_coil = 0x01,
        fc_r_discrete  = 0x02,
        fc_r_holding = 0x03,
        fc_r_input = 0x04,

        fc_w_single_coil = 0x05,
        fc_w_single_holding = 0x06,

        fc_w_multiple_coils = 0x0F,
        fc_w_multiple_holding = 0x10,
    };

private slots:
    void slot_ready_read();

private:
    QByteArray _cache;
    const quint8 _min_rtu_frame_length = 8;
    const quint8 _max_addr = 247;
    const quint16 _max_cache_size = 4096;
    QSerialPort* _serial = nullptr;    

private:
    quint8 get_addr(const QByteArray& rtu_frame);
    quint8 get_func_code(const QByteArray& rtu_frame);
    quint16 get_crc(const QByteArray& rtu_frame);

    // 判断功能码是否有效
    bool is_valid_func_code(const quint8 fc);

    // 判断是否是一个完整的rtu帧
    bool is_complete_rtu_frame(const QByteArray& rtu_frame);
    // 从cache中获取一个完整的rtu帧
    bool get_complete_rtu_frame(QByteArray& rtu_frame);

    bool check_crc(const QByteArray& rtu_frame);
    quint16 calc_modbus_rtu_crc(const QByteArray &data);

};

#endif // MODBUS_RTU_WORKER_H
