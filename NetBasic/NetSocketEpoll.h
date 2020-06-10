#ifndef NETSOCKETEPOLL_H
#define NETSOCKETEPOLL_H

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


#include "NetSocketEpollThread.h"
#include <QVector>
#include <QMap>
#include <QMutex>
#include "NetPacketBase.h"

struct EpollPacket
{
    QByteArray bytSendData;
    int nSendIndex;
    int nFd;
    int nSissionID;

    bool bKeepAlive;

    NetPacketBase* pobjNetPacketBase;

    EpollPacket()
    {
        init();
    }
    ~EpollPacket()
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
        bKeepAlive = false;
        pobjNetPacketBase = NULL;
        nSissionID = 0;
    }
};

class NetPacketBase;
class NetSocketEpoll : public NetSocketBase
{
public:
    NetSocketEpoll();
    ~NetSocketEpoll();

    virtual bool init(const qint32 p_nThreadNum);
    virtual bool start(const QString& p_strBindIP, const qint32 p_nPort);
    virtual bool send(NetPacketBase* p_pobjNetPacketBase);

private:
    int                         m_nEpfd;
    int                         m_nListenfd;

    QVector<NetSocketEpollThread*> m_vecNetSocketEpollThread;
};
#endif
#endif // NETSOCKETEPOLL_H
