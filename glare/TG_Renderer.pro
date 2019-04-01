#-------------------------------------------------
#
# Project created by QtCreator 2019-02-14T19:55:35
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = TG_Renderer
TEMPLATE = app

#QMAKE_CXXFLAGS += -std=c++0x
win32 {
#Replace by your windows opencl path
LIBS += -L"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v9.2/lib/x64" -lOpenCL
INCLUDEPATH += "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v9.2/include/"
} else {
LIBS += -lOpenCL
}

INCLUDEPATH += $$PWD/../include

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

SOURCES += \
        main.cpp \
    TGViewWidget.cpp \
    TGViewWindow.cpp \
    TemporalGlareRenderer.cpp

HEADERS += \
    TGViewWidget.h \
    TGViewWindow.h \
    TemporalGlareRenderer.h

FORMS += \
        mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    render.cl

