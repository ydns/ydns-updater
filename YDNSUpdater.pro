#-------------------------------------------------
#
# Project created by QtCreator 2014-10-26T13:19:46
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = YDNSUpdater
TEMPLATE = app

macx {
    ICON = assets/ydns.icns
}

SOURCES +=\
        maindialog.cc \
    main.cc

HEADERS  += maindialog.h \
    version.h

FORMS    += maindialog.ui

RESOURCES += \
    ydns.qrc
