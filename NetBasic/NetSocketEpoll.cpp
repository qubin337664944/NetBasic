#include "NetSocketEpoll.h"
#include "NetPacketManager.h"
#include "NetLog.h"
#include "NetKeepAliveThread.h"
#include <QDebug>

#ifndef WIN32

NetSocketEpoll::NetSocketEpoll()
{
}

NetSocketEpoll::~NetSocketEpoll()
{
    for(int i = 0; i < m_vecNetSocketEpollThread.size(); i++)
    {
        m_vecNetSocketEpollThread.at(i)->terminate();
        delete m_vecNetSocketEpollThread.at(i);
    }

    m_vecNetSocketEpollThread.clear();

    close(m_nListenfd);
    close(m_nEpfd);

    m_nListenfd = 0;
    m_nEpfd = 0;
}

bool NetSocketEpoll::init(const qint32 p_nThreadNum)
{
    m_nListenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(m_nListenfd <= 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("creat socket failed,error:%1").arg(strerror(errno)));
        return false;
    }

    int nFlag = 1;
    int nRet = setsockopt(m_nListenfd, IPPROTO_TCP, TCP_NODELAY, (char *)&nFlag, sizeof(nFlag) );
    if (nRet < 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("setsockopt TCP_NODELAY,socket:%1, error:%2").arg(m_nListenfd).arg(strerror(errno)));
        close(m_nListenfd);
        return false;
    }

    unsigned long argp = 1;
    nRet = ioctl(m_nListenfd, FIONBIO, (unsigned long*)&argp);
    if(nRet < 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("ioctl FIONBIO 1 failed,socket:%1, error:%2").arg(m_nListenfd).arg(strerror(errno)));
        close(m_nListenfd);
        return false;
    }

    m_nEpfd = epoll_create(EPOLL_SOCKET_MAX_SIZE);
    if(m_nEpfd <= 0)
    {
        close(m_nListenfd);
        NETLOG(NET_LOG_LEVEL_ERROR, QString("epoll_create %1 failed,error:%2").arg(EPOLL_SOCKET_MAX_SIZE).arg(strerror(errno)));
        return false;
    }

    for(int i = 0; i < p_nThreadNum; i++)
    {
        NetSocketEpollThread* pobjNetSocketEpollThread = new NetSocketEpollThread;
        pobjNetSocketEpollThread->init(i, m_nEpfd, m_nListenfd);
        m_vecNetSocketEpollThread.append(pobjNetSocketEpollThread);
    }

    return true;
}

bool NetSocketEpoll::start(const QString &p_strBindIP, const qint32 p_nPort)
{
    if(p_strBindIP.isEmpty() || p_nPort < 0 || p_nPort > 65535)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("bindip or port check failed,bind ip:%1,port:%2").arg(p_strBindIP).arg(p_nPort));
        return false;
    }

    ADDRINFO Hints, *AddrInfo;
    memset(&Hints, 0, sizeof(Hints));
    Hints.ai_family = AF_INET;
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_flags = AI_PASSIVE ;

    char szPort[32] = {0};
    sprintf(szPort, "%d", p_nPort);
    int nRet = getaddrinfo(p_strBindIP.toStdString().c_str(), szPort, &Hints, &AddrInfo);
    if (nRet != 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("getaddrinfo failed,bind ip:%1,port:%2,error:%3").arg(p_strBindIP).arg(p_nPort).arg(strerror(errno)));
        return false;
    }

    int opt = 1;
    nRet = setsockopt(m_nListenfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    if(nRet < 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("setsockopt SO_REUSEADDR failed,socket:%1,error:%2").arg(m_nListenfd).arg(strerror(errno)));
        return false;
    }

    nRet = bind(m_nListenfd, AddrInfo->ai_addr, AddrInfo->ai_addrlen);
    if(nRet < 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("bind failed,socket:%1,error:%2").arg(m_nListenfd).arg(strerror(errno)));
        return false;
    }

    freeaddrinfo(AddrInfo);

    nRet = listen(m_nListenfd, LISTEN_SIZE);
    if(nRet < 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("listen failed,socket:%1,error:%2").arg(m_nListenfd).arg(strerror(errno)));
        return false;
    }

    EpollPacket *pobjEpollPacket = new EpollPacket;
    pobjEpollPacket->nFd = m_nListenfd;
    struct epoll_event stEvent;
    stEvent.data.ptr = pobjEpollPacket;
    stEvent.events=EPOLLIN|EPOLLET;
    nRet = epoll_ctl(m_nEpfd, EPOLL_CTL_ADD, m_nListenfd,&stEvent);
    if(nRet < 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("epoll_ctl add failed,socket:%1,error:%2").arg(m_nListenfd).arg(strerror(errno)));
        delete pobjEpollPacket;
        return false;
    }

    for(int i = 0; i < m_vecNetSocketEpollThread.size(); i++)
    {
        m_vecNetSocketEpollThread.at(i)->start();
    }

    return true;
}

bool NetSocketEpoll::send(NetPacketBase *p_pobjNetPacketBase)
{
    if(p_pobjNetPacketBase == NULL)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("send Null Pointer"));
        return false;
    }

    EpollPacket *pobjEpollSendPacket = new EpollPacket;
    pobjEpollSendPacket->nFd = p_pobjNetPacketBase->m_nSocket;
    pobjEpollSendPacket->nSendIndex = 0;
    pobjEpollSendPacket->bKeepAlive = p_pobjNetPacketBase->m_bKeepAlive;
    pobjEpollSendPacket->nSissionID = p_pobjNetPacketBase->m_nSissionID;
    pobjEpollSendPacket->nIndex = p_pobjNetPacketBase->m_nIndex;

    if(!NetPacketManager::prepareResponse(p_pobjNetPacketBase, pobjEpollSendPacket->bytSendData))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("prepareResponse failed,socket:%1").arg(pobjEpollSendPacket->nFd));
        delete pobjEpollSendPacket;
        return false;
    }

    quint32 nIndex = pobjEpollSendPacket->nIndex;
    void* pobjContext = NULL;
    if(!NetKeepAliveThread::lockIndexContext(pobjEpollSendPacket->nIndex, pobjEpollSendPacket->nFd, pobjEpollSendPacket->nSissionID, pobjContext))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("lockIndexContext failed, post socket:%1").arg(p_pobjNetPacketBase->m_nSocket));
        delete pobjEpollSendPacket;
        return false;
    }

    if(!NetKeepAliveThread::setCheckSend(p_pobjNetPacketBase->m_nSocket, p_pobjNetPacketBase->m_nSissionID, p_pobjNetPacketBase->m_nIndex, true, p_pobjNetPacketBase->m_nTimeOutS))
    {
        NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckSend failed, post socket:%1").arg(p_pobjNetPacketBase->m_nSocket));
        delete pobjEpollSendPacket;
        NetKeepAliveThread::unlockIndex(nIndex);
        return false;
    }
    NetKeepAliveThread::unlockIndex(nIndex);

    struct epoll_event stEvent;
    memset(&stEvent, 0, sizeof(stEvent));
    stEvent.data.ptr = pobjEpollSendPacket;
    stEvent.events=EPOLLOUT | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
    int nRet = epoll_ctl(m_nEpfd, EPOLL_CTL_MOD, p_pobjNetPacketBase->m_nSocket,&stEvent);
    if(nRet < 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("epoll_ctl mod EPOLLOUT failed,socket:%1,error:%2").arg(p_pobjNetPacketBase->m_nSocket).arg(strerror(errno)));
        delete pobjEpollSendPacket;
        NetKeepAliveThread::setCheckSend(p_pobjNetPacketBase->m_nSocket, p_pobjNetPacketBase->m_nSissionID, p_pobjNetPacketBase->m_nIndex,  false);
        return false;
    }

    return true;
}
#endif
