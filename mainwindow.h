#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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

private:
    Ui::MainWindow *ui;

    ModbusRtuWidget mbrtu_wdgt;
    ModbusTcpWidget mbtcp_wdgt;

    transfer _transfer;

    modbus_rtu_worker* rtu_worker = nullptr;
    modbus_tcp_worker* tcp_worker = nullptr;

};
#endif // MAINWINDOW_H