#-------------------------------------------------
#
# Project created by QtCreator 2020-04-27T10:35:18
#
#-------------------------------------------------

QT       += core network

QT       -= gui

TARGET = NetBasic
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

win32:{
    LIBS += -lwsock32 -lws2_32

    SSLLIBPATH = $$PWD/ssl/lib/Win32
    LIBS += -L$$SSLLIBPATH -llibeay32
    LIBS += -L$$SSLLIBPATH -lssleay32

    INCLUDEPATH += $$PWD/ssl/include/Win32
}
else:linux:{
    SSLLIBPATH = $$PWD/ssl/lib/Linux64
    LIBS += -L$$SSLLIBPATH -lssl
    LIBS += -L$$SSLLIBPATH -lcrypto
    LIBS += -ldl

    INCLUDEPATH += $$PWD/ssl/include/Linux64
}

SOURCES += \
    NetSocketBase.cpp \
    NetSocketIocp.cpp \
    NetSocketEpoll.cpp \
    NetProtocolParseBase.cpp \
    NetProcotolParseHttp.cpp \
    NetPacketBase.cpp \
    NetPacketHttp.cpp \
    NetServerInterface.cpp \
    NetLog.cpp \
    NetPacketManager.cpp \
    NetSocketIocpThread.cpp \
    NetSocketEpollThread.cpp \
    NetSocketEpollSSL.cpp \
    NetSocketEpollSSLThread.cpp \
    #testssl.cpp \
    main.cpp \
    NetClientInterface.cpp \
    NetKeepAliveThread.cpp \
    NetSocketIocpSSL.cpp \
    NetSocketIocpSSLThread.cpp

HEADERS += \
    NetSocketBase.h \
    NetSocketIocp.h \
    NetSocketEpoll.h \
    NetProtocolParseBase.h \
    NetProcotolParseHttp.h \
    NetPacketBase.h \
    NetPacketHttp.h \
    NetServerInterface.h \
    NetInclude.h \
    NetLog.h \
    NetPacketManager.h \
    NetEnum.h \
    NetSocketIocpThread.h \
    NetSocketEpollThread.h \
    NetSocketEpollSSL.h \
    NetSocketEpollSSLThread.h \
    NetClientInterface.h \
    NetKeepAliveThread.h \
    NetSocketIocpSSL.h \
    NetSocketIocpSSLThread.h


