TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.c \
    utils.c \
    conn.c \
    dapclient.c


unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += libevent
unix: PKGCONFIG += libxml-2.0
unix: PKGCONFIG += libcrypto

HEADERS += \
    list.h \
    utils.h \
    conn.h \
    dapclient.h
