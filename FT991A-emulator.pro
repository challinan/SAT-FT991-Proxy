QT       += core gui serialport network

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    CivProtocol.cpp \
    CivProxyController.cpp \
    Ft991CatCodec.cpp \
    Ft991Client.cpp \
    Ft991Protocol.cpp \
    RadioCore.cpp \
    RadioCoreBackend.cpp \
    SerialPort.cpp \
    ft991a_proxy.cpp \
    idle_sleep_notifications.c \
    main.cpp \
    mainwindow.cpp \
    config_object.cpp

HEADERS += \
    CivProtocol.h \
    CivProxyController.h \
    CivTypes.h \
    Ft991CatCodec.h \
    Ft991Client.h \
    Ft991Protocol.h \
    RadioBackend.h \
    RadioCore.h \
    RadioCoreBackend.h \
    RadioProtocol.h \
    RadioState.h \
    RadioTypes.h \
    SerialPort.h \
    ft991a_proxy.h \
    mainwindow.h \
    config_object.h

FORMS += \
    mainwindow.ui \
    configdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# GStreamer support
macx: LIBS += -L/Library/Frameworks/GStreamer.framework/Libraries -lgstreamer-1.0.0 -lglib-2.0.0 -lgobject-2.0.0
INCLUDEPATH += /Library/Frameworks/GStreamer.framework/Headers

DISTFILES += \
    Readme.md \
    TODO.txt

RESOURCES += \
    ds-digital.qrc
