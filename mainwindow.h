#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>

class QEvent;
class QListWidget;

#include "gateway_mode.h"
#include "modbusrtuwidget.h"
#include "modbustcpwidget.h"
#include "modbus_rtu_worker.h"
#include "modbus_tcp_worker.h"
#include "transfer.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void on_btn_run_clicked();
    void slot_gateway_mode_changed();

    void slot_update_rtu_wdgt(const QString& dir, const QByteArray& frame);
    void slot_update_tcp_wdgt(const QString& client_id, const QString& dir, const QByteArray& frame);

    void slot_update_client_status(const QString& notify);

private:
    void set_config_widgets_enabled(bool enabled);
    void stop_workers();
    bool start_workers();
    GatewayMode current_gateway_mode() const;
    QString idle_status_text() const;
    void update_gateway_mode_ui();
    void setup_log_list(QListWidget* list_widget);
    void copy_selected_log_items(QListWidget* list_widget) const;

private:
    Ui::MainWindow *ui;

    const quint32 max_item_count = 50;

    ModbusRtuWidget* mbrtu_wdgt = nullptr;
    ModbusTcpWidget* mbtcp_wdgt = nullptr;

    transfer* _transfer = nullptr;

    modbus_rtu_worker* rtu_worker = nullptr;
    modbus_tcp_worker* tcp_worker = nullptr;

    QLabel* label_status = nullptr;

};
#endif // MAINWINDOW_H
