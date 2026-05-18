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

private:
    Ui::ModbusTcpWidget *ui;    
    void setIPAddr();
    void setPort();
};

#endif // MODBUSTCPWIDGET_H
