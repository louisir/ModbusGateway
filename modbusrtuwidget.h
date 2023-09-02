#ifndef MODBUSRTUWIDGET_H
#define MODBUSRTUWIDGET_H

#include <QWidget>
#include <QtSerialPort>

namespace Ui {
class ModbusRtuWidget;
}

class ModbusRtuWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ModbusRtuWidget(QWidget *parent = nullptr);
    ~ModbusRtuWidget();

    QStringList get_params() const;

private:
    Ui::ModbusRtuWidget *ui;

    void setSerialPortNames();
    void setBaudRate();
    void setDataBits();
    void setStopBits();    
    void setParity();
    void setFlowControl();

    QList<QPair<QSerialPort::StopBits, QString>> m_StopBits;
    QList<QPair<QSerialPort::Parity, QString>> m_Parity;
    QList<QPair<QSerialPort::FlowControl, QString>> m_FlowCtrl;
};

#endif // MODBUSRTUWIDGET_H
