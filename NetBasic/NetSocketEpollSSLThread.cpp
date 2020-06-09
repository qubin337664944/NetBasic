#include "NetSocketEpollSSLThread.h"
#include "NetPacketManager.h"
#include "NetSocketEpollSSL.h"
#include <QDebug>
#include "NetLog.h"
#include "NetKeepAliveThread.h"

#ifndef WIN32

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


NetSocketEpollSSLThread::NetSocketEpollSSLThread()
{
}

NetSocketEpollSSLThread::~NetSocketEpollSSLThread()
{

}

bool NetSocketEpollSSLThread::init(qint32 p_nThreadID, qint32 p_nEpFd, qint32 p_nListenFd, void* p_pobjsslCtx)
{
    m_nThreadID = p_nThreadID;
    m_nEpFd = p_nEpFd;
    m_nListenFd = p_nListenFd;
    m_pobjsslCtx = p_pobjsslCtx;

    return true;
}

void	NetSocketEpollSSLThread::run()
{
    while(1)
    {
        int nfds = epoll_wait(m_nEpFd, m_pobjEvent, 1, 5000);
        int i = 0;
        for(i=0;i<nfds;++i)
        {
            EpollSSLPacket *pobjPacket = (EpollSSLPacket*)m_pobjEvent[i].data.ptr;
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
                NETLOG(NET_LOG_LEVEL_TRACE, QString("thread:%1,socket:%2,epoll_wait EPOLLRDHUP").arg(m_nThreadID).arg(sockfd));
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

bool NetSocketEpollSSLThread::doAccept(qint32 p_nListenFd, EpollSSLPacket* p_pobjEpollPacket)
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

        EpollSSLPacket *pobjPacket = new EpollSSLPacket;
        pobjPacket->nFd = connfd;
        pobjPacket->nSendIndex = 0;
        pobjPacket->bTcpConnected = true;
        pobjPacket->bSslConnected = false;

        NetKeepAliveInfo objNetKeepAliveInfo;
        objNetKeepAliveInfo.nSocket = connfd;
        objNetKeepAliveInfo.bCheckReceiveTime = true;
        objNetKeepAliveInfo.bCheckSendTime = false;
        objNetKeepAliveInfo.nReceiveTimeOutS = RECEIVE_PACKET_TIMEOUT_S;
        if(!NetKeepAliveThread::addAlive(objNetKeepAliveInfo))
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("add socket to keep alive failed, socket:%1")
                   .arg(connfd));

            delete pobjPacket;
            close(connfd);
            continue;
        }

        struct epoll_event   objEv;
        objEv.data.ptr = pobjPacket;
        objEv.events=EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
        nRet = epoll_ctl(m_nEpFd,EPOLL_CTL_ADD,connfd,&objEv);
        if(nRet < 0)
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,epoll_ctl add,error:%3").arg(m_nThreadID).arg(connfd).arg(strerror(errno)));
            delete pobjPacket;
            NetKeepAliveThread::delAlive(connfd);
            close(connfd);
            continue;
        }

        NETLOG(NET_LOG_LEVEL_INFO, QString("thread:%1,receive a connect,ip:%2,port:%3,socket:%4").arg(m_nThreadID).arg(szFromIP).arg(szFromPort).arg(connfd));
    }

    return true;
}

bool NetSocketEpollSSLThread::doReceive(qint32 p_nFd, EpollSSLPacket* p_pobjEpollPacket)
{
    if(p_pobjEpollPacket->pobjNetPacketBase == NULL)
    {
        p_pobjEpollPacket->pobjNetPacketBase =  NetPacketManager::allocPacket();
        p_pobjEpollPacket->pobjNetPacketBase->m_nSocket = p_nFd;
    }

    if(!p_pobjEpollPacket->bSslConnected)
    {
        return doSSLHandshake(p_nFd, p_pobjEpollPacket);
    }

    while(1)
    {
         int nLen = SSL_read((SSL*)p_pobjEpollPacket->pobjSsl, szDataTemp, MAX_BUFFER_LEN);
         if(nLen > 0)
         {
             NETLOG(NET_LOG_LEVEL_TRACE, QString("thread:%1,socket:%2,SSL_read size:%3 success").arg(m_nThreadID).arg(p_nFd).arg(nLen));
             p_pobjEpollPacket->pobjNetPacketBase->m_pobjSSL = p_pobjEpollPacket->pobjSsl;

             NetPacketManager::appendReceiveBuffer(p_pobjEpollPacket->pobjNetPacketBase, szDataTemp, nLen);
             if(p_pobjEpollPacket->pobjNetPacketBase->m_bIsReceiveEnd)
             {
                 if(!NetKeepAliveThread::setCheckReceive(p_nFd, false))
                 {
                     NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckReceive failed, socket:%1").arg(p_nFd));
                 }

                  NetPacketManager::processCallBack(p_pobjEpollPacket->pobjNetPacketBase);

                  if(p_pobjEpollPacket->pobjNetPacketBase)
                  {
                        delete p_pobjEpollPacket->pobjNetPacketBase;
                        p_pobjEpollPacket->pobjNetPacketBase = NULL;
                  }

                  delete p_pobjEpollPacket;
                  p_pobjEpollPacket = NULL;

                  return true;
             }
         }

         if((nLen == -1 && errno == EAGAIN))
         {
             struct epoll_event   objEv;
             objEv.data.ptr = p_pobjEpollPacket;
             objEv.events=EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
             int nRet = epoll_ctl(m_nEpFd,EPOLL_CTL_MOD,p_nFd,&objEv);
             if(nRet<0)
             {
                    NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,epoll_ctl mod,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
                    return false;
             }

             return true;
         }

         if(nLen == -1 && errno != 0)
         {
             NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,SSL_read len = -1,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
             return false;
         }

         if(nLen == 0)
         {
             NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,SSL_read len = 0,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
             return false;
         }
    }

    return true;
}

bool NetSocketEpollSSLThread::doSend(qint32 p_nFd, EpollSSLPacket *p_pobjEpollPacket)
{
    if(!p_pobjEpollPacket->bSslConnected)
    {
        return doSSLHandshake(p_nFd, p_pobjEpollPacket);
    }

    EpollSSLPacket* pobjEpollSendPacket = p_pobjEpollPacket;
    if(pobjEpollSendPacket->nSendIndex < pobjEpollSendPacket->bytSendData.size() - 1)
    {
        while(1)
        {
            int nLen = SSL_write((SSL*)p_pobjEpollPacket->pobjSsl, pobjEpollSendPacket->bytSendData.data() + pobjEpollSendPacket->nSendIndex,
                      pobjEpollSendPacket->bytSendData.length() - pobjEpollSendPacket->nSendIndex);
             if(nLen > 0)
             {
                    NETLOG(NET_LOG_LEVEL_TRACE, QString("thread:%1,socket:%2,SSL_write size:%3 success").arg(m_nThreadID).arg(p_nFd).arg(nLen));
                    pobjEpollSendPacket->nSendIndex += nLen;
             }

             if(pobjEpollSendPacket->nSendIndex == pobjEpollSendPacket->bytSendData.size())
             {
                 if(!NetKeepAliveThread::setCheckSend(p_nFd, false))
                 {
                      NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckSend failed, socket:%1").arg(p_nFd));
                      pobjEpollSendPacket->bCheck = false;
                      return false;
                 }

                 NETLOG(NET_LOG_LEVEL_INFO, QString("thread:%1,socket:%2,a packet send success,no keepalive,close socket").arg(m_nThreadID).arg(p_nFd));

                 if(pobjEpollSendPacket->bKeepAlive)
                 {
                     if(!NetKeepAliveThread::setCheckReceive(p_nFd, true))
                     {
                          pobjEpollSendPacket->bCheck = false;
                          NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckReceive failed, socket:%1").arg(p_nFd));
                          return false;
                     }

                     struct epoll_event   objEv;
                     objEv.data.ptr = p_pobjEpollPacket;
                     objEv.events=EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
                     int nRet = epoll_ctl(m_nEpFd,EPOLL_CTL_MOD,p_nFd,&objEv);
                     if(nRet < 0)
                     {
                         NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,epoll_ctl mod epoll in,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));

                         if(!NetKeepAliveThread::setCheckReceive(p_nFd, false))
                         {
                              pobjEpollSendPacket->bCheck = false;
                              NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckReceive failed, socket:%1").arg(p_nFd));
                              return false;
                         }

                         return false;
                     }

                     return true;
                 }
                 else
                 {
                     return false;
                 }
             }

             if(nLen == -1 && errno == EAGAIN)
             {
                 struct epoll_event   objEv;
                 objEv.data.ptr = p_pobjEpollPacket;
                 objEv.events=EPOLLOUT | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
                 int nRet = epoll_ctl(m_nEpFd,EPOLL_CTL_MOD,p_nFd,&objEv);
                 if(nRet<0)
                 {
                        NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,epoll_ctl mod epoll out,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
                        return false;
                 }

                 return true;
             }

             if(nLen == -1 && errno != 0)
             {
                  NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,SSL_write = -1,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
                  return false;
             }

             if(nLen == 0)
             {
                 NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,SSL_write = 0,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
                 return false;
             }
        }
    }

    return true;
}

bool NetSocketEpollSSLThread::doSSLHandshake(qint32 p_nFd, EpollSSLPacket *p_pobjEpollPacket)
{
    if (p_pobjEpollPacket->pobjSsl == NULL)
    {
        p_pobjEpollPacket->pobjSsl = (void*)SSL_new ((SSL_CTX*)m_pobjsslCtx);
        if(p_pobjEpollPacket->pobjSsl == NULL)
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,SSL_new = NULL,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
            return false;
        }

        int r = SSL_set_fd((SSL*)p_pobjEpollPacket->pobjSsl, p_pobjEpollPacket->nFd);
        if(!r)
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,SSL_set_fd = 0,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
            return false;
        }

        SSL_set_accept_state((SSL*)p_pobjEpollPacket->pobjSsl);
    }

    int nRet = SSL_do_handshake((SSL*)p_pobjEpollPacket->pobjSsl);
    if (nRet == 1)
    {
        p_pobjEpollPacket->bSslConnected = true;

        struct epoll_event   objEv;
        objEv.data.ptr = p_pobjEpollPacket;
        objEv.events=EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
        int nRet = epoll_ctl(m_nEpFd,EPOLL_CTL_MOD,p_nFd,&objEv);
        if(nRet < 0)
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,epoll_ctl epoll in,error:%3").arg(m_nThreadID).arg(p_nFd).arg(strerror(errno)));
            return false;
        }

        return true;
    }

    int err = SSL_get_error((SSL*)p_pobjEpollPacket->pobjSsl, nRet);
    if (err == SSL_ERROR_WANT_WRITE)
    {
        p_pobjEpollPacket->bSslConnected = true;
        return doSend(p_nFd, p_pobjEpollPacket);
    }
    else if (err == SSL_ERROR_WANT_READ)
    {
        p_pobjEpollPacket->bSslConnected = true;
        return doReceive(p_nFd, p_pobjEpollPacket);
    }
    else
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("thread:%1,socket:%2,SSL_get_errorl:%3,error:%4").arg(m_nThreadID).arg(p_nFd).arg(err).arg(strerror(errno)));
        return false;
    }
}

void NetSocketEpollSSLThread::closeConnect(qint32 p_nFd, EpollSSLPacket *p_pobjEpollPacket)
{
    if(p_pobjEpollPacket->pobjSsl)
    {
        SSL_shutdown ((SSL*)p_pobjEpollPacket->pobjSsl);
        SSL_free((SSL*)p_pobjEpollPacket->pobjSsl);
    }

    NETLOG(NET_LOG_LEVEL_INFO, QString("thread:%1,socket:%2,close connect").arg(m_nThreadID).arg(p_nFd));

    if(p_pobjEpollPacket->bCheck)
    {
        NetKeepAliveThread::delAlive(p_nFd);
        close(p_nFd);
    }

    delete p_pobjEpollPacket;
}
#endif
