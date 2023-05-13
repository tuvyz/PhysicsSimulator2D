#-------------------------------------------------
#
# Project created by QtCreator 2023-04-08T12:40:54
#
#-------------------------------------------------
DEFINES += __STDC_FORMAT_MACROS


QT       += core gui
QT       += core gui network
QT       += widgets
QT       += multimedia
QT       += opengl
QT       += openglwidgets

CONFIG += c++11

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = balls
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
    ball.cpp \
        main.cpp \
        mainwindow.cpp \
    glgraphics.cpp \
    global.cpp \
    ffclasses.cpp \
    physsim2d.cpp \
    physsimwidget.cpp

HEADERS += \
    ball.h \
        mainwindow.h \
    glgraphics.h \
    global.h \
    ffclasses.h \
    physsim2d.h \
    physsimwidget.h

FORMS += \
        mainwindow.ui


# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


# ffmpeg библиотеки
unix:!macx: LIBS += -L /usr/include/x86_64-linux-gnu -lavcodec -lavformat -lswscale -lavutil\

win32: {
LIBS += -lopengl32
QMAKE_CXXFLAGS += -D__STDC_CONSTANT_MACROS
INCLUDEPATH += H:/Programs/Development/Libs/ffmpeg/include
LIBS += -LH:/Programs/Development/Libs/ffmpeg/bin
LIBS += -lavcodec-58 \
 -lavdevice-58 \
 -lavfilter-7 \
 -lavformat-58 \
 -lavutil-56 \
 -lpostproc-55 \
 -lswresample-3 \
 -lswscale-5 \
}


RESOURCES += \
    res.qrc

DISTFILES += \
    PhysSimWidget.qml



