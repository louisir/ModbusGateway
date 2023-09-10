#ifndef MODBUSTCPWIDGET_H
#define MODBUSTCPWIDGET_H

#include <QWidget>

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

private:
    Ui::ModbusTcpWidget *ui;    
    void setIPAddr();
    void setPort();
};

#endif // MODBUSTCPWIDGET_H
