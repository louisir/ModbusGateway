
#include "modbusassistant.h"

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>

const quint16 ModbusAssistant::m_maxAddrCount = 20;
const QColor ModbusAssistant::m_addrTypeColors[ModbusAddrTypeTotal] = {
    QColor::fromRgb(0x808080/*灰 - 线圈*/),
    QColor::fromRgb(0x28A745/*绿 - 离散量*/),
    QColor::fromRgb(0x000000/*黑 - 占位*/),
    QColor::fromRgb(0x007BFF/*蓝 - 输入寄存器*/),
    QColor::fromRgb(0xFFA500/*橙 - 保持寄存器*/)
};

// 匹配以0、1、3、4开头，中间3位为数字，最后以非0结尾，即排除了以0结尾的情况，以保证00000、10000、30000和40000被排除，
// 同时排除了01000、00100、00010、01100、00110、01110这六种匹配模式，后面两行的正则进行补充
const QString ModbusAssistant::m_regExp4ModbusAddr = "[0134]\\d{3}[1-9]"
                                                     "|"
                                                     "[0134][1-9]0{3}|[0134][1-9][1-9]0{2}|[0134][1-9][1-9][1-9]0"
                                                     "|"
                                                     "[0134]0{2}[1-9]0|[0134][1-9]0{2}0|[0134]0[1-9]00";

// 匹配1到65535之间的正整数
const QString ModbusAssistant::m_regExp4PortNumber = "^([1-9]|[1-9]\\d{1,3}|[1-5]\\d{4}|6[0-4]\\d{3}|65[0-4]\\d{2}|655[0-2]\\d|6553[0-5])$";

// 取值范围正则表达式
const QString ModbusAssistant::m_regExp4ValueRange = "^[0-9,\\[\\]\\(\\)\\|\\&!]+$";

// 生成频率正则表达式
const QString ModbusAssistant::m_regExp4Frequency = "^[0-9+\\-*/()]+$";

ModbusAssistant::ModbusAssistant()
{

}

ModbusAssistant::ModbusAddrIndex ModbusAssistant::GetIndexFromModbusAddr(const QString& modbusAddr)
{
    quint8 highDigital = modbusAddr.first(1).toUInt();
    assert(highDigital == ModbusAssistant::Coil || highDigital == ModbusAssistant::Discrete || highDigital == ModbusAssistant::Input || highDigital == ModbusAssistant::Holding);
    return (ModbusAssistant::ModbusAddrIndex)highDigital;
}

QString ModbusAssistant::ModbusAddr2RegAddr(const QString& modbusAddr)
{
    quint16 base = (quint16)(ModbusAssistant::GetIndexFromModbusAddr(modbusAddr)) * 10000 + 1;
    quint16 regAddr = modbusAddr.toUInt() - base;
    QString decRegAddr = QString::number(regAddr).rightJustified(4, '0');
    QString hexRegAddr = "0x" + QString::number(regAddr, 16).toUpper().rightJustified(4, '0');
    return QString("%1 (%2)").arg(decRegAddr, hexRegAddr);
}

QColor ModbusAssistant::GetColorByModbusAddr(const QString& modbusAddr)
{
    return ModbusAssistant::m_addrTypeColors[ModbusAssistant::GetIndexFromModbusAddr(modbusAddr)];
}

void ModbusAssistant::ThrowMsgBox(const QMessageBox::Icon& icon, const QString& msg, const QString& title)
{
    QMessageBox msgBox;
    msgBox.setWindowIcon(QIcon(":/images/ModbusVis.png"));
    msgBox.setIcon(icon);
    msgBox.setText(msg);
    msgBox.setWindowTitle(title);
    msgBox.exec();
    return;
}
