#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>

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

signals:
    void sig_quit_worker();

private slots:
    void on_btn_run_clicked();

    void slot_update_rtu_wdgt(const QString& dir, const QByteArray& frame);
    void slot_update_tcp_wdgt(const QString& client_id, const QString& dir, const QByteArray& frame);

    void slot_update_client_status(const QString& notify);

private:
    Ui::MainWindow *ui;

    const quint32 max_item_count = 50;

    ModbusRtuWidget mbrtu_wdgt;
    ModbusTcpWidget mbtcp_wdgt;

    transfer _transfer;

    modbus_rtu_worker* rtu_worker = nullptr;
    modbus_tcp_worker* tcp_worker = nullptr;

    QLabel* label_status = nullptr;

};
#endif // MAINWINDOW_H
