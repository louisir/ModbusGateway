#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    mbrtu_wdgt = new ModbusRtuWidget(ui->centralwidget);
    mbtcp_wdgt = new ModbusTcpWidget(ui->centralwidget);

    ui->horizontalLayout->insertWidget(0, mbrtu_wdgt);
    ui->horizontalLayout->setStretch(0, 3);
    ui->horizontalLayout->setStretch(1, 17);
    ui->horizontalLayout_2->insertWidget(0, mbtcp_wdgt);
    ui->horizontalLayout_2->setStretch(0, 3);
    ui->horizontalLayout_2->setStretch(1, 17);

    label_status = new QLabel("No Modbus-TCP master", this);
    ui->statusbar->addWidget(label_status);

    this->setWindowTitle(QString("Modbus-TCP to Modbus-RTU soft gateway - by louis / louis.androidor@gmail.com"));
}

MainWindow::~MainWindow()
{
    stop_workers();
    delete ui;
}

void MainWindow::on_btn_run_clicked()
{
    assert(mbrtu_wdgt->isEnabled() == mbtcp_wdgt->isEnabled());
    assert((tcp_worker == nullptr) == (rtu_worker == nullptr));

    if(rtu_worker || tcp_worker){
        stop_workers();
        set_config_widgets_enabled(true);
        ui->btn_run->setText("运行");
        label_status->setText("No Modbus-TCP master");
        return;
    }

    if(start_workers()){
        set_config_widgets_enabled(false);
        ui->btn_run->setText("停止");
    }else{
        stop_workers();
        set_config_widgets_enabled(true);
        ui->btn_run->setText("运行");
    }
}

void MainWindow::slot_update_rtu_wdgt(const QString& dir, const QByteArray& frame)
{
    if(ui->lst_wdgt_rtu->count() >= (int)max_item_count){
        ui->lst_wdgt_rtu->clear();
        ui->lst_wdgt_tcp->clear();
    }
    QString current_date_time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QListWidgetItem *item = new QListWidgetItem(QString("%1 %2 %3").arg(current_date_time, dir, frame.toHex()));
    ui->lst_wdgt_rtu->insertItem(0, item);
}

void MainWindow::slot_update_tcp_wdgt(const QString& client_id, const QString& dir, const QByteArray& frame)
{
    if(ui->lst_wdgt_tcp->count() >= (int)max_item_count){
        ui->lst_wdgt_rtu->clear();
        ui->lst_wdgt_tcp->clear();
    }
    QString current_date_time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QListWidgetItem *item = new QListWidgetItem(QString("%1 @ %2 %3 %4").arg(current_date_time, client_id, dir, frame.toHex()));
    ui->lst_wdgt_tcp->insertItem(0, item);
}

void MainWindow::slot_update_client_status(const QString& notify)
{
    label_status->setText(notify);
}

void MainWindow::set_config_widgets_enabled(bool enabled)
{
    mbrtu_wdgt->setEnabled(enabled);
    mbtcp_wdgt->setEnabled(enabled);
}

void MainWindow::stop_workers()
{
    if(rtu_worker){
        rtu_worker->slot_quit_worker();
    }
    if(tcp_worker){
        tcp_worker->slot_quit_worker();
    }

    delete rtu_worker;
    rtu_worker = nullptr;
    delete tcp_worker;
    tcp_worker = nullptr;
}

bool MainWindow::start_workers()
{
    assert(!rtu_worker);
    assert(!tcp_worker);

    rtu_worker = new modbus_rtu_worker(mbrtu_wdgt->get_params());
    if(!rtu_worker->is_running()){
        const QString error = rtu_worker->last_error().isEmpty() ? "Failed to start Modbus RTU worker." : rtu_worker->last_error();
        label_status->setText(error);
        return false;
    }

    tcp_worker = new modbus_tcp_worker(mbtcp_wdgt->get_params());
    if(!tcp_worker->is_running()){
        const QString error = tcp_worker->last_error().isEmpty() ? "Failed to start Modbus TCP server." : tcp_worker->last_error();
        label_status->setText(error);
        return false;
    }

    connect(rtu_worker, &modbus_rtu_worker::sig_rcv, &_transfer, &transfer::slot_rcv_from_rtu, Qt::QueuedConnection);
    connect(rtu_worker, &modbus_rtu_worker::sig_update_rtu_wdgt, this, &MainWindow::slot_update_rtu_wdgt, Qt::QueuedConnection);
    connect(rtu_worker, &modbus_rtu_worker::sig_discard_tcp_transaction, tcp_worker, &modbus_tcp_worker::slot_discard_pending_transaction, Qt::QueuedConnection);
    connect(&_transfer, &transfer::sig_tcp_to_rtu, rtu_worker, &modbus_rtu_worker::slot_tcp_to_rtu, Qt::QueuedConnection);

    connect(tcp_worker, &modbus_tcp_worker::sig_rcv, &_transfer, &transfer::slot_rcv_from_tcp, Qt::QueuedConnection);
    connect(tcp_worker, &modbus_tcp_worker::sig_update_tcp_wdgt, this, &MainWindow::slot_update_tcp_wdgt, Qt::QueuedConnection);
    connect(tcp_worker, &modbus_tcp_worker::sig_update_client_status, this, &MainWindow::slot_update_client_status, Qt::QueuedConnection);
    connect(tcp_worker, &modbus_tcp_worker::sig_client_disconnected, rtu_worker, &modbus_rtu_worker::slot_clear_pending_requests, Qt::QueuedConnection);
    connect(&_transfer, &transfer::sig_rtu_to_tcp, tcp_worker, &modbus_tcp_worker::slot_rtu_to_tcp, Qt::QueuedConnection);

    label_status->setText("No Modbus-TCP master");
    return true;
}
