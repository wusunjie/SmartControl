TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.c \
    connection.c


unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += libevent
unix: PKGCONFIG += libxml-2.0

HEADERS += \
    connection.h \
    list.h
