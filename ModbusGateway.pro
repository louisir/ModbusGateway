QT       += core gui network serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    modbus_rtu_worker.cpp \
    modbus_tcp_worker.cpp \
    modbusassistant.cpp \
    modbusrtuwidget.cpp \
    modbustcpwidget.cpp \
    transfer.cpp \
    worker.cpp

HEADERS += \
    mainwindow.h \
    modbus_rtu_worker.h \
    modbus_tcp_worker.h \
    modbusassistant.h \
    modbusrtuwidget.h \
    modbustcpwidget.h \
    transfer.h \
    worker.h

FORMS += \
    mainwindow.ui \
    modbusrtuwidget.ui \
    modbustcpwidget.ui

TRANSLATIONS += \
    ModbusGateway_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=

RESOURCES += \
    resource.qrc
RC_FILE += \
    ModbusGateway.rc
