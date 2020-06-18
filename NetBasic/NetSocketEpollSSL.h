#ifndef NETSOCKETEPOLLSSL_H
#define NETSOCKETEPOLLSSL_H

#ifndef WIN32
#include <QCoreApplication>
#include <QString>
#include "NetSocketBase.h"

#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <linux/sockios.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
typedef  struct addrinfo						ADDRINFO;

#include "NetSocketEpollSSLThread.h"
#include <QVector>
#include <QMap>
#include <QMutex>

struct EpollSSLPacket
{
    QByteArray bytSendData;
    int nSendIndex;
    int nFd;

    void *pobjSsl;
    bool bTcpConnected;
    bool bSslConnected;

    bool bKeepAlive;

    int nSissionID;
    quint32 nIndex;

    NetPacketBase* pobjNetPacketBase;

    EpollSSLPacket()
    {
        init();
    }

    ~EpollSSLPacket()
    {
        if(pobjNetPacketBase)
        {
            delete pobjNetPacketBase;
            pobjNetPacketBase = NULL;
        }
    }

    void init()
    {
        bytSendData.clear();
        nSendIndex = 0;
        nFd = 0;

        pobjSsl = NULL;
        bTcpConnected = false;
        bSslConnected = false;

        bKeepAlive = false;

        nSissionID = 0;
        nIndex = 0;

        pobjNetPacketBase = NULL;
    }
};

class NetPacketBase;
class NetSocketEpollSSL : public NetSocketBase
{
public:
    NetSocketEpollSSL();
    ~NetSocketEpollSSL();

    virtual bool init(const qint32 p_nThreadNum);
    virtual bool start(const QString& p_strBindIP, const qint32 p_nPort);
    virtual bool send(NetPacketBase* p_pobjNetPacketBase);

public:
    static QString g_strKeyPath;
    static QString g_strCertPath;

private:
    int                         m_nEpfd;
    int                         m_nListenfd;
    void*                     m_pobjsslCtx;
    void*                      m_pobjerrBio;

    QVector<NetSocketEpollSSLThread*> m_vecNetSocketEpollThread;
};
#endif
#endif // NETSOCKETEPOLL_H
