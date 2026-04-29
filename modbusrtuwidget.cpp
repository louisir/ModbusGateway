#include "modbusrtuwidget.h"
#include "ui_modbusrtuwidget.h"

#include <QtSerialPort/QSerialPortInfo>

#include <QSysInfo>

ModbusRtuWidget::ModbusRtuWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ModbusRtuWidget),
    m_StopBits({
        { QSerialPort::OneStop, tr("1") },
        { QSerialPort::OneAndHalfStop, tr("1.5") },
        { QSerialPort::TwoStop, tr("2") },
    }),
    m_Parity({
        { QSerialPort::NoParity, tr("无校验") },
        { QSerialPort::EvenParity, tr("偶校验") },
        { QSerialPort::OddParity, tr("奇校验") },
        { QSerialPort::SpaceParity, tr("空格校验") },
        { QSerialPort::MarkParity, tr("标记校验") }
    }),
    m_FlowCtrl({
        { QSerialPort::NoFlowControl, tr("无流控") },
        { QSerialPort::HardwareControl, tr("硬流控(RTS/CTS)") },
        { QSerialPort::SoftwareControl, tr("软流控(XON/XOFF)") }
    })
{
    ui->setupUi(this);

    setSerialPortNames();
    setBaudRate();
    setDataBits();
    setStopBits();
    setParity();
    setFlowControl();
}

ModbusRtuWidget::~ModbusRtuWidget()
{
    delete ui;
}

void ModbusRtuWidget::setSerialPortNames()
{
    QStringList portNames;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& portInfo : ports){
        portNames.append(portInfo.portName());
    }
    std::sort(portNames.begin(), portNames.end());
    ui->comboBox_SerialName->clear();
    ui->comboBox_SerialName->addItems(portNames);
}

void ModbusRtuWidget::setBaudRate()
{
    QList<qint32> baudRates = QSerialPortInfo::standardBaudRates();
    std::sort(baudRates.begin(), baudRates.end());
    ui->comboBox_BaudRate->clear();
    for (int i = 0; i < baudRates.count(); i++) {
        const QString baudRate = QString::number(baudRates.at(i));
        ui->comboBox_BaudRate->addItem(baudRate, baudRates.at(i));
    }
    const int defaultIndex = ui->comboBox_BaudRate->findData(9600);
    if(defaultIndex >= 0){
        ui->comboBox_BaudRate->setCurrentIndex(defaultIndex);
    }
}

void ModbusRtuWidget::setDataBits()
{
    QList<QSerialPort::DataBits> dataBits;
    dataBits << QSerialPort::Data5 << QSerialPort::Data6 << QSerialPort::Data7 << QSerialPort::Data8;
    std::sort(dataBits.begin(), dataBits.end());
    ui->comboBox_DataBits->clear();
    for (int i = 0; i < dataBits.count(); i++) {
        const QString dataBit = QString::number(dataBits.at(i));
        ui->comboBox_DataBits->addItem(dataBit, static_cast<int>(dataBits.at(i)));
    }
    const int defaultIndex = ui->comboBox_DataBits->findData(static_cast<int>(QSerialPort::Data8));
    if(defaultIndex >= 0){
        ui->comboBox_DataBits->setCurrentIndex(defaultIndex);
    }
}

void ModbusRtuWidget::setStopBits()
{
    ui->comboBox_StopBits->clear();
    for (auto it = m_StopBits.begin(); it != m_StopBits.end(); ++it) {
        // 非windows平台上不支持停止位为1.5
        if (QSysInfo::kernelType() != "winnt" && it->first == QSerialPort::OneAndHalfStop) {
            continue;
        }
        ui->comboBox_StopBits->addItem(it->second, static_cast<int>(it->first));
    }
}

void ModbusRtuWidget::setFlowControl()
{
    ui->comboBox_FlowCtrl->clear();
    for (auto it = m_FlowCtrl.begin(); it != m_FlowCtrl.end(); ++it) {
        ui->comboBox_FlowCtrl->addItem(it->second, static_cast<int>(it->first));
    }
}

void ModbusRtuWidget::setParity()
{
    ui->comboBox_Parity->clear();
    for (auto it = m_Parity.begin(); it != m_Parity.end(); ++it) {
        ui->comboBox_Parity->addItem(it->second, static_cast<int>(it->first));
    }
}

QStringList ModbusRtuWidget::get_params() const
{
    QStringList params;
    params << ui->comboBox_SerialName->currentText()
           << ui->comboBox_BaudRate->currentData().toString()
           << ui->comboBox_DataBits->currentData().toString()
           << ui->comboBox_StopBits->currentData().toString()
           << ui->comboBox_Parity->currentData().toString()
           << ui->comboBox_FlowCtrl->currentData().toString();
    return params;
}
