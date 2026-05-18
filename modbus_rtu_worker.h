#ifndef MODBUS_RTU_WORKER_H
#define MODBUS_RTU_WORKER_H

#include "gateway_mode.h"
#include "worker.h"

#include <QObject>
#include <QSerialPort>

class modbus_rtu_worker : public worker
{
    Q_OBJECT
public:
    explicit modbus_rtu_worker(const QStringList& thread_params, GatewayMode mode, QObject *parent = nullptr);
    ~modbus_rtu_worker();
    void stop();
    bool is_running() const;
    QString last_error() const;

signals:
    void sig_rcv(const QByteArray& frame, const QByteArray& transaction_id, quint64 tcp_session_id);
    void sig_update_rtu_wdgt(const QString& dir, const QByteArray& frame);
    void sig_discard_tcp_transaction(quint64 tcp_session_id);

public slots:
    void slot_quit_worker();
    void slot_tcp_to_rtu(const QByteArray& adu, const QByteArray& transaction_id, quint64 tcp_session_id);
    void slot_clear_pending_requests();
    void slot_clear_pending_requests_for_session(quint64 tcp_session_id);

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
    bool slot_start_worker();
    void slot_ready_read();
    void slot_response_timeout();
    void slot_response_guard_finished();

private:
    struct response_quarantine_entry
    {
        quint8 addr = 0;
        quint8 func = 0;
        qint64 expires_at_ms = 0;
    };

    struct pending_request
    {
        QByteArray adu;
        QByteArray transaction_id;
        quint64 tcp_session_id = 0;
    };

    QByteArray _cache;
    const quint8 _min_rtu_frame_length = 5;
    const quint8 _max_addr = 247;
    const quint16 _max_cache_size = 4096;
    const int _max_tx_queue_size = 64;
    const int _response_timeout_ms = 2000;
    const int _late_response_guard_ms = 500;
    const int _timeout_quarantine_ms = 4000;
    const int _serial_write_timeout_ms = 500;
    const quint64 _reverse_tcp_session_id = 0;
    QSerialPort* _serial = nullptr;
    QTimer* _response_timer = nullptr;
    QTimer* _response_guard_timer = nullptr;
    QQueue<pending_request> _tx_queue;
    QVector<response_quarantine_entry> _response_quarantine;
    QByteArray _pending_adu;
    QByteArray _pending_transaction_id;
    quint64 _pending_tcp_session_id = 0;
    quint64 _last_disconnected_tcp_session_id = 0;
    bool _waiting_response = false;
    bool _response_guard_active = false;
    bool _running = false;
    GatewayMode _mode = GatewayMode::TcpToRtu;
    quint16 _next_transaction_id = 1;
    QString _last_error;

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
    QByteArray append_crc(const QByteArray& adu);
    int expected_rtu_frame_length(const QByteArray& data, int offset);
    int expected_rtu_request_frame_length(const QByteArray& data, int offset);
    bool is_response_for_pending_request(const QByteArray& rtu_frame);
    bool is_normal_response_for_pending_request(const QByteArray& response_adu);
    bool validate_request_adu(const QByteArray& adu, quint8& exception_code);
    quint16 read_be_u16(const QByteArray& data, int offset) const;
    bool is_valid_address_range(quint16 start_addr, quint16 quantity) const;
    int expected_read_response_byte_count(const QByteArray& request_adu) const;
    bool write_full_frame(const QByteArray& rtu_frame);
    void reject_request(const pending_request& request, quint8 exception_code);
    bool dequeue_rejected_request(pending_request& request);
    void add_response_quarantine(const QByteArray& request_adu);
    void prune_response_quarantine();
    bool is_request_quarantined(const QByteArray& request_adu);
    void handle_rtu_frame(const QByteArray& rtu_frame);
    void handle_rtu_request_frame(const QByteArray& rtu_frame);
    void handle_tcp_response_adu(const QByteArray& adu, const QByteArray& transaction_id, quint64 tcp_session_id);
    void send_next_request();
    void finish_pending_request();
    void start_late_response_guard();
    void emit_gateway_exception_response(const QByteArray& request_adu, const QByteArray& transaction_id, quint64 tcp_session_id, quint8 exception_code);
    void emit_rtu_exception_response(const QByteArray& request_adu, quint8 exception_code);
    QByteArray next_transaction_id();

};

#endif // MODBUS_RTU_WORKER_H
