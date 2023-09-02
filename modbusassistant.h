
#ifndef MODBUSASSISTANT_H
#define MODBUSASSISTANT_H


#include <QString>
#include <QColor>
#include <QIcon>
#include <QMessageBox>

class ModbusAssistant
{
public:
    ModbusAssistant();

    enum ModbusAddrIndex{
        Coil,
        Discrete,
        Placeholder,
        Input,
        Holding,
        ModbusAddrTypeTotal
    };

    static const quint16 m_maxAddrCount;                            // 最大地址数量
    static const QColor m_addrTypeColors[ModbusAddrTypeTotal];      // modbus地址与背景色的对应
    static const QString m_regExp4ModbusAddr;                       // modbus地址输入规则的正则表达式
    static const QString m_regExp4PortNumber;                       // 端口号输入规则的正则表达式
    static const QString m_regExp4ValueRange;                       // 取值范围的正则表达式
    static const QString m_regExp4Frequency;                        // 生成频率的正则表达式

    static ModbusAddrIndex GetIndexFromModbusAddr(const QString& modbusAddr);
    static QString ModbusAddr2RegAddr(const QString& modbusAddr);
    static QColor GetColorByModbusAddr(const QString& modbusAddr);
    static void ThrowMsgBox(const QMessageBox::Icon& icon, const QString& msg, const QString& title);
};

#endif // MODBUSASSISTANT_H
