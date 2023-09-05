#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->horizontalLayout->insertWidget(0, &this->mbrtu_wdgt);
    ui->horizontalLayout->setStretch(0, 3);
    ui->horizontalLayout->setStretch(1, 17);
    ui->horizontalLayout_2->insertWidget(0, &this->mbtcp_wdgt);
    ui->horizontalLayout_2->setStretch(0, 3);
    ui->horizontalLayout_2->setStretch(1, 17);
    ui->centralwidget->setLayout(ui->verticalLayout);

    this->setWindowTitle(QString("ModbusRTU to ModbusTCP 软网关 by louis / louis.androidor@gmail.com"));
}

MainWindow::~MainWindow()
{
    delete ui;
    if(!rtu_worker || !tcp_worker){
        return;
    }
    emit this->sig_quit_worker();
    delete rtu_worker;
    rtu_worker = nullptr;
    delete tcp_worker;
    tcp_worker = nullptr;
}

void MainWindow::on_btn_run_clicked()
{
    assert(mbrtu_wdgt.isEnabled() == mbtcp_wdgt.isEnabled());
    assert((tcp_worker == nullptr) == (rtu_worker == nullptr));
    bool status = mbrtu_wdgt.isEnabled();
    mbrtu_wdgt.setEnabled(!status);
    mbtcp_wdgt.setEnabled(!status);
    ui->btn_run->setText(!status ? "运行" : "停止");
    if(status){
        assert(!rtu_worker);
        assert(!tcp_worker);

        rtu_worker = new modbus_rtu_worker(mbrtu_wdgt.get_params());
        connect(this, &MainWindow::sig_quit_worker, rtu_worker, &modbus_rtu_worker::slot_quit_worker, Qt::QueuedConnection);
        connect(rtu_worker, &modbus_rtu_worker::sig_rcv, &_transfer, &transfer::slot_rcv_from_rtu, Qt::QueuedConnection);
        connect(rtu_worker, &modbus_rtu_worker::sig_update_rtu_wdgt, this, &MainWindow::slot_update_rtu_wdgt, Qt::QueuedConnection);
        connect(&_transfer, &transfer::sig_tcp_to_rtu, rtu_worker, &modbus_rtu_worker::slot_tcp_to_rtu, Qt::QueuedConnection);

        tcp_worker = new modbus_tcp_worker(mbtcp_wdgt.get_params());
        connect(this, &MainWindow::sig_quit_worker, tcp_worker, &modbus_tcp_worker::slot_quit_worker, Qt::QueuedConnection);
        connect(tcp_worker, &modbus_tcp_worker::sig_rcv, &_transfer, &transfer::slot_rcv_from_tcp, Qt::QueuedConnection);
        connect(tcp_worker, &modbus_tcp_worker::sig_update_tcp_wdgt, this, &MainWindow::slot_update_tcp_wdgt, Qt::QueuedConnection);
        connect(&_transfer, &transfer::sig_rtu_to_tcp, tcp_worker, &modbus_tcp_worker::slot_rtu_to_tcp, Qt::QueuedConnection);
    }else{
        if(!rtu_worker || !tcp_worker){
            return;
        }
        emit this->sig_quit_worker();
        delete rtu_worker;
        rtu_worker = nullptr;
        delete tcp_worker;
        tcp_worker = nullptr;
    }
}

void MainWindow::slot_update_rtu_wdgt(const QString& dir, const QByteArray& frame)
{
    if(ui->lst_wdgt_rtu->count() >= max_item_count){
        ui->lst_wdgt_rtu->clear();
        ui->lst_wdgt_tcp->clear();
    }
    QString current_date_time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QListWidgetItem *item = new QListWidgetItem(QString("%1 %2 %3").arg(current_date_time, dir, frame.toHex()));
    ui->lst_wdgt_rtu->insertItem(0, item);
}

void MainWindow::slot_update_tcp_wdgt(const QString& client_id, const QString& dir, const QByteArray& frame)
{
    if(ui->lst_wdgt_tcp->count() >= max_item_count){
        ui->lst_wdgt_rtu->clear();
        ui->lst_wdgt_tcp->clear();
    }
    QString current_date_time = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QListWidgetItem *item = new QListWidgetItem(QString("%1 @ %2 %3 %4").arg(current_date_time, client_id, dir, frame.toHex()));
    ui->lst_wdgt_tcp->insertItem(0, item);
}
