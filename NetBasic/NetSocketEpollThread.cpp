#include "NetSocketEpollThread.h"
#include "NetPacketManager.h"
#include "NetSocketEpoll.h"
#include <QDebug>
#include "NetLog.h"

#ifndef WIN32

NetSocketEpollThread::NetSocketEpollThread()
{
}

bool NetSocketEpollThread::init(qint32 p_nThreadID, qint32 p_nEpFd, qint32 p_nListenFd)
{
    m_nThreadID = p_nThreadID;
    m_nEpFd = p_nEpFd;
    m_nListenFd = p_nListenFd;

    return true;
}

void	NetSocketEpollThread::run()
{
    while(1)
    {
        int nfds = epoll_wait(m_nEpFd, m_pobjEvent, 1, 5000);
        int i = 0;
        for(i=0;i<nfds;++i)
        {
            EpollPacket *pobjPacket = (EpollPacket *)m_pobjEvent[i].data.ptr;
            int sockfd = pobjPacket->nFd;
            if ( sockfd < 0 )
            {
                NETLOG(NET_LOG_LEVEL_WORNING, QString("thread:%1,epoll_wait socket < 0,errorn:%2").arg(m_nThreadID).arg(strerror(errno)));
                continue;
            }

            if(sockfd == m_nListenFd)
            {
                NETLOG(NET_LOG_LEVEL_TRACE, QString("thread:%1,socket:%2,epoll_wait ListenFd").arg(m_nThreadID).arg(sockfd));
                doAccept(m_nListenFd, pobjPacket);
                continue;
            }

            if(m_pobjEvent[i].events&EPOLLRDHUP || m_pobjEvent[i].events&EPOLLERR || m_pobjEvent[i].events&EPOLLHUP)
            {
                NETLOG(NET_LOG_LEVEL_TRACE, QString("thread:%1,socket:%2,epoll_wait EPOLLRDHUP,aborted").arg(m_nThreadID).arg(sockfd));
                closeConnect(sockfd, pobjPacket);
                continue;
            }

            if(m_pobjEvent[i].events&EPOLLIN)
            {
                NETLOG(NET_LOG_LEVEL_TRACE, QString("thread:%1,socket:%2,epoll_wait EPOLLIN").arg(m_nThreadID).arg(sockfd));
                if(!doReceive(sockfd, pobjPacket))
                {
                    closeConnect(sockfd, pobjPacket);
                }
                continue;
            }

            if(m_pobjEvent[i].events&EPOLLOUT)
            {
                NETLOG(NET_LOG_LEVEL_TRACE, QString("thread:%1,socket:%2,epoll_wait EPOLLOUT").arg(m_nThreadID).arg(sockfd));
                if(!doSend(sockfd, pobjPacket))
                {
                    closeConnect(sockfd, pobjPacket);
                }
                continue;
            }
        }
    }
}

bool NetSocketEpollThread::doAccept(qint32 p_nListenFd, EpollPacket* p_pobjEpollPacket)
{
    while(1)
    {
        sockaddr_storage From;
        socklen_t addrlen = sizeof(From);
        int connfd = accept(p_nListenFd, (sockaddr *)&From, &addrlen);
        if(connfd== -1 && errno == EAGAIN)
        {
            break;
        }

        char szFromIP[128];
        char szFromPort[32];
        int nRet = getnameinfo((sockaddr *)&From, addrlen,
            szFromIP, sizeof(szFromIP),
            szFromPort, sizeof(szFromPort), NI_NUMERICHOST | NI_NUMERICSERV);

        if(nRet < 0)
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,getnameinfo failed,error:%3").arg(m_nThreadID).arg(connfd).arg(strerror(errno)));
            close(connfd);
            continue;
        }

        unsigned long argp = 1;
        nRet = ioctl(connfd, FIONBIO, (unsigned long*)&argp);
        if(nRet==-1)
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,ioctl fionbio 1,error:%3").arg(m_nThreadID).arg(connfd).arg(strerror(errno)));
            close(connfd);
            continue;
        }

        int nflag = 1;
        nRet = setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (char *)&nflag, sizeof(nflag));
        if(nRet < 0)
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,setsockopt tcp nodelay,error:%3").arg(m_nThreadID).arg(connfd).arg(strerror(errno)));
            close(connfd);
            continue;
        }

        EpollPacket *pobjPacket = new EpollPacket;
        pobjPacket->nFd = connfd;
        pobjPacket->nSendIndex = 0;

        struct epoll_event   objEv;
        objEv.data.ptr = pobjPacket;
        objEv.events=EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
        nRet = epoll_ctl(m_nEpFd,EPOLL_CTL_ADD,connfd,&objEv);
        if(nRet < 0)
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,epoll_ctl add,error:%3").arg(m_nThreadID).arg(connfd).arg(strerror(errno)));
            delete pobjPacket;
            close(connfd);
            continue;
        }

        NETLOG(NET_LOG_LEVEL_INFO, QString("thread:%1,receive a connect,ip:%2,port:%3,socket:%4").arg(m_nThreadID).arg(szFromIP).arg(szFromPort).arg(connfd));
    }

    return true;
}

bool NetSocketEpollThread::doReceive(qint32 p_nFd, EpollPacket* p_pobjEpollPacket)
{
    while(1)
    {
         int nLen = ::recv(p_nFd, szDataTemp, MAX_BUFFER_LEN, MSG_NOSIGNAL);
         if(nLen > 0)
         {
             NETLOG(NET_LOG_LEVEL_TRACE, QString("thread:%1,socket:%2,receive size:%3 success").arg(m_nThreadID).arg(p_nFd).arg(nLen));
             bool bReceiveEnd = false;
             NetPacketManager::appendReceiveBuffer(p_nFd, szDataTemp, nLen, bReceiveEnd);
             if(bReceiveEnd)
             {
                 break;
             }
         }

         if(nLen == -1 && errno == EAGAIN)
         {
             struct epoll_event   objEv;
             objEv.data.ptr = p_pobjEpollPacket;
             objEv.events=EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
             int nRet = epoll_ctl(m_nEpFd,EPOLL_CTL_MOD,p_nFd,&objEv);
             if(nRet<0)
             {
                NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,epoll_ctl mod EPOLLIN,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
                return false;
             }

             return true;
         }

         if(nLen == -1 && errno != 0)
         {
             struct epoll_event   objEv;
             objEv.data.ptr = p_pobjEpollPacket;
             objEv.events=EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
             int nRet = epoll_ctl(m_nEpFd,EPOLL_CTL_MOD,p_nFd,&objEv);
             if(nRet<0)
             {
                 NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,epoll_ctl mod EPOLLIN,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
                 return false;
             }

             return true;
         }

         if(nLen == 0)
         {
             NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,recv return 0,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
             return false;
         }
    }

    return true;
}

bool NetSocketEpollThread::doSend(qint32 p_nFd, EpollPacket *p_pobjEpollPacket)
{
    EpollPacket* pobjEpollSendPacket = p_pobjEpollPacket;
    if(pobjEpollSendPacket->nSendIndex < pobjEpollSendPacket->bytSendData.size() - 1)
    {
        while(1)
        {
             int nLen = ::send(p_nFd, pobjEpollSendPacket->bytSendData.data() + pobjEpollSendPacket->nSendIndex,
                               pobjEpollSendPacket->bytSendData.length() - pobjEpollSendPacket->nSendIndex, MSG_NOSIGNAL);

             if(nLen > 0)
             {
                    NETLOG(NET_LOG_LEVEL_TRACE, QString("thread:%1,socket:%2,send size:%3 success").arg(m_nThreadID).arg(p_nFd).arg(nLen));
                    pobjEpollSendPacket->nSendIndex += nLen;
             }

             if(pobjEpollSendPacket->nSendIndex == pobjEpollSendPacket->bytSendData.size())
             {
                    struct epoll_event   objEv;
                    objEv.data.ptr = p_pobjEpollPacket;
                    objEv.events=EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
                    int nRet = epoll_ctl(m_nEpFd,EPOLL_CTL_MOD,p_nFd,&objEv);
                    if(nRet<0)
                    {
                        NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,epoll_ctl mod EPOLLIN,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
                        return false;
                    }
                    return true;
             }

             if(nLen == -1 && errno == EAGAIN)
             {
                 struct epoll_event   objEv;
                 objEv.data.ptr = p_pobjEpollPacket;
                 objEv.events=EPOLLOUT | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
                 int nRet = epoll_ctl(m_nEpFd,EPOLL_CTL_MOD,p_nFd,&objEv);
                 if(nRet<0)
                 {
                     NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,epoll_ctl mod EPOLLOUT,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
                     return false;
                 }

                 return true;
             }

             if(nLen == -1 && errno != 0)
             {
                 struct epoll_event   objEv;
                 objEv.data.ptr = p_pobjEpollPacket;
                 objEv.events=EPOLLOUT | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
                 int nRet = epoll_ctl(m_nEpFd,EPOLL_CTL_MOD,p_nFd,&objEv);
                 if(nRet<0)
                 {
                     NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,epoll_ctl mod EPOLLOUT,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
                     return false;
                 }

                 return true;
             }

             if(nLen == 0)
             {
                 NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,send return 0,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
                 return false;
             }
        }
    }

    return true;
}

void NetSocketEpollThread::closeConnect(qint32 p_nFd, EpollPacket *p_pobjEpollPacket)
{
    NETLOG(NET_LOG_LEVEL_INFO, QString("thread:%1,socket:%2,close connect").arg(m_nThreadID).arg(p_nFd));

    NetPacketManager::delPacket(p_nFd);

    close(p_nFd);

    if(p_pobjEpollPacket)
    {
        delete p_pobjEpollPacket;
    }
}
#endif
