#include "modbustcpwidget.h"
#include "ui_modbustcpwidget.h"
#include "modbusassistant.h"

#include <QNetworkInterface>
#include <QtAlgorithms>

ModbusTcpWidget::ModbusTcpWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ModbusTcpWidget)
{
    ui->setupUi(this);
    this->setLayout(ui->gridLayout);
    setIPAddr();
    setPort();
}

ModbusTcpWidget::~ModbusTcpWidget()
{
    delete ui;
}

void ModbusTcpWidget::setIPAddr()
{
    QList<QString> ipv4Addresses;
    foreach (QNetworkInterface interface, QNetworkInterface::allInterfaces())
    {
        // 获取此接口上的所有IP地址条目
        foreach (QNetworkAddressEntry entry, interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                // 将IPv4地址添加到ComboBox中
                ipv4Addresses.append(entry.ip().toString());
            }
        }
    }

    #if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
        // 在Qt 5.15或更高版本中，qSort()方法被标记为已弃用。因此，在这些版本中使用std::sort()算法。
        qSort(ipv4Addresses);
    #else
        // 在Qt 5.15或更高版本中，使用std::sort()算法。
        std::sort(ipv4Addresses.begin(), ipv4Addresses.end());
    #endif
    ui->comboBox_IPAddr->clear();
    foreach (QString ipv4Address, ipv4Addresses) {
        ui->comboBox_IPAddr->addItem(ipv4Address);
    }

    return;
}

void ModbusTcpWidget::setPort()
{
    // 创建一个匹配端口号规则的正则表达式
    QRegularExpression regex(ModbusAssistant::m_regExp4PortNumber);
    // 创建一个QRegularExpressionValidator对象，用于限制输入
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(regex, this);
    // 将验证器设置为QLineEdit的输入验证器
    ui->lineEdit_Port->setValidator(validator);
    return;
}

QStringList ModbusTcpWidget::get_params() const
{
    QStringList params;
    params << ui->comboBox_IPAddr->currentText()
           << ui->lineEdit_Port->text();
    return params;
}
