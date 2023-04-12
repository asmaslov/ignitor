QT += core gui serialport

QMAKE_CXXFLAGS += -std=c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = ignitor
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

win32 {
    INCLUDEPATH += "C:/Program Files (x86)/L-Card"
    LIBS += -L"C:/Program Files (x86)/L-Card/ltr/lib/mingw"
}

INCLUDEPATH += $$PWD/../firmware

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    ltr35.cpp

HEADERS += \
    mainwindow.h \
    ltr35.h

FORMS += \
    ignitor.ui

RESOURCES += \
    ignitor.qrc

LIBS += -lltrapi -lltr35api
