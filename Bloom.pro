QT += gui core

CONFIG += c++11

TARGET = HDRToneMap
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    Bloom.cpp \
    teapot.cpp \
    vboplane.cpp \
    vbosphere.cpp

HEADERS += \
    Bloom.h \
    teapotdata.h \
    teapot.h \
    vboplane.h \
    vbosphere.h

OTHER_FILES += \
    fshader.txt \
    vshader.txt

RESOURCES += \
    shaders.qrc

DISTFILES += \
    fshader.txt \
    vshader.txt
