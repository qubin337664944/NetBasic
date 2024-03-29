#ifndef NETSOCKETEPOLLSSLTHREAD_H
#define NETSOCKETEPOLLSSLTHREAD_H

#ifndef WIN32

#include <QThread>
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

#include "NetInclude.h"

class NetSocketEpollSSL;
class EpollSSLPacket;
class NetKeepAliveThread;
class NetPacketManager;
class NetSocketEpollSSLThread : public QThread
{
public:
    NetSocketEpollSSLThread();
    ~NetSocketEpollSSLThread();

    bool init(qint32 p_nThreadID, qint32 p_nEpFd, qint32 p_nListenFd, void* p_pobjsslCtx, NetPacketManager* p_pobjNetPacketManager,
              NetKeepAliveThread* p_pobjNetKeepAliveThread);

protected:
    virtual	void	run();

public:
    bool doAccept(qint32 p_nListenFd, EpollSSLPacket* p_pobjEpollPacket);
    bool doReceive(qint32 p_nFd, EpollSSLPacket* p_pobjEpollPacket, bool& p_bIsLock);
    bool doSend(qint32 p_nFd, EpollSSLPacket* p_pobjEpollPacket, bool& p_bIsLock);

    bool doSSLHandshake(qint32 p_nFd, EpollSSLPacket* p_pobjEpollPacket, bool& p_bIsLock);

    void closeConnect(qint32 p_nFd, EpollSSLPacket* p_pobjEpollPacket);

private:
    NetPacketManager*           m_pobjNetPacketManager;
    NetKeepAliveThread*         m_pobjNetKeepAliveThread;

    qint32 m_nThreadID;
    qint32 m_nEpFd;
    qint32 m_nListenFd;

    void*  m_pobjsslCtx;

    struct epoll_event   m_pobjEvent[1];
    struct epoll_event   m_objEv;

    char    szDataTemp[MAX_BUFFER_LEN];
};
#endif
#endif // NETSOCKETEPOLLTHREAD_H
