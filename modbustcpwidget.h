#ifndef MODBUSTCPWIDGET_H
#define MODBUSTCPWIDGET_H

#include <QWidget>

#include "gateway_mode.h"

namespace Ui {
class ModbusTcpWidget;
}

class ModbusTcpWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ModbusTcpWidget(QWidget *parent = nullptr);
    ~ModbusTcpWidget();

    QStringList get_params() const;
    void set_gateway_mode(GatewayMode mode);

signals:
    void sig_clear_log_requested();

private slots:
    void on_btn_clear_log_clicked();

private:
    Ui::ModbusTcpWidget *ui;    
    void setIPAddr();
    void setPort();
};

#endif // MODBUSTCPWIDGET_H
