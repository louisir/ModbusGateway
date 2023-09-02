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
    this->setLayout(ui->gridLayout_3);

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
    QStringList baudRateStrings;
    for (int i = 0; i < baudRates.count(); i++) {
        baudRateStrings.append(QString::number(baudRates.at(i)));
    }
    ui->comboBox_BaudRate->clear();
    ui->comboBox_BaudRate->addItems(baudRateStrings);
}

void ModbusRtuWidget::setDataBits()
{
    QList<QSerialPort::DataBits> dataBits;
    dataBits << QSerialPort::Data5 << QSerialPort::Data6 << QSerialPort::Data7 << QSerialPort::Data8;
    std::sort(dataBits.begin(), dataBits.end());
    QStringList dataBitStrings;
    for (int i = 0; i < dataBits.count(); i++) {
        dataBitStrings.append(QString::number(dataBits.at(i)));
    }
    ui->comboBox_DataBits->clear();
    ui->comboBox_DataBits->addItems(dataBitStrings);
}

void ModbusRtuWidget::setStopBits()
{
    ui->comboBox_StopBits->clear();
    for (auto it = m_StopBits.begin(); it != m_StopBits.end(); ++it) {
        // 非windows平台上不支持停止位为1.5
        if (QSysInfo::kernelType() != "winnt" && it->first == QSerialPort::OneAndHalfStop) {
            continue;
        }
        ui->comboBox_StopBits->addItem(it->second);
    }
}

void ModbusRtuWidget::setFlowControl()
{
    ui->comboBox_FlowCtrl->clear();
    for (auto it = m_FlowCtrl.begin(); it != m_FlowCtrl.end(); ++it) {
        ui->comboBox_FlowCtrl->addItem(it->second);
    }
}

void ModbusRtuWidget::setParity()
{
    ui->comboBox_Parity->clear();
    for (auto it = m_Parity.begin(); it != m_Parity.end(); ++it) {
        ui->comboBox_Parity->addItem(it->second);
    }
}

QStringList ModbusRtuWidget::get_params() const
{
    QStringList params;
    params << ui->comboBox_SerialName->currentText()
           << ui->comboBox_BaudRate->currentText()
           << ui->comboBox_DataBits->currentText()
           << ui->comboBox_StopBits->currentText()
           << QString::number(ui->comboBox_Parity->currentIndex())
           << QString::number(ui->comboBox_FlowCtrl->currentIndex());
    return params;
}
