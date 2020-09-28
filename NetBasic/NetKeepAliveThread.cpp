#include "NetKeepAliveThread.h"
#include "NetSocketIocp.h"
#include "NetSocketIocpSSL.h"
#include "NetSocketEpollSSL.h"
#include "NetLog.h"
#include <QDebug>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

NetKeepAliveInfo* NetKeepAliveThread::g_vpobjNetKeepAliveInfo = NULL;
quint32   NetKeepAliveThread::g_nNetKeepAliveInfoSize = 0;
quint32 NetKeepAliveThread::g_nSissionID = 1;
QMutex NetKeepAliveThread::g_objKeepAliveMutex;
qint32 NetKeepAliveThread::g_nProtocolType;

#ifdef WIN32
#define close(x)                closesocket(x);
#endif

NetKeepAliveThread::NetKeepAliveThread()
{
}

void NetKeepAliveThread::run()
{
    while(1)
    {
        QDateTime objDateTime = QDateTime::currentDateTime();

        qint32 nConnectedSize = 0;
        for(int i = 0; i < g_nNetKeepAliveInfoSize; i++)
        {
            if(g_vpobjNetKeepAliveInfo[i].bIsAlive)
            {
                nConnectedSize++;
            }

            if(g_vpobjNetKeepAliveInfo[i].bCheckSendTime && g_vpobjNetKeepAliveInfo[i].bIsAlive)
            {
               if(g_vpobjNetKeepAliveInfo[i].objLastSendTime.secsTo(objDateTime) > g_vpobjNetKeepAliveInfo[i].nSendTimeOutS)
               {
                    NETLOG(NET_LOG_LEVEL_WORNING, QString("socket:%1, send time out:%2 s").arg(g_vpobjNetKeepAliveInfo[i].nSocket).arg(g_vpobjNetKeepAliveInfo[i].nSendTimeOutS));
                    if(g_vpobjNetKeepAliveInfo[i].bIsAlive)
                    {
                        if(!NetKeepAliveThread::tryLock(i, 5))
                        {
                            continue;
                        }

                        if(!g_vpobjNetKeepAliveInfo[i].bIsAlive)
                        {
                            NetKeepAliveThread::unlockIndex(i);
                            continue;
                        }
#ifdef WIN32
                        if(g_nProtocolType == NET_PROTOCOL_HTTPS)
                        {
                            SOCKET_CONTEXT_SSL* pobjContext  = (SOCKET_CONTEXT_SSL*)g_vpobjNetKeepAliveInfo[i].pobjExtend;
                            if(pobjContext)
                            {
                                if(pobjContext->m_pobjSSL)
                                {
                                    SSL_shutdown ((SSL*)pobjContext->m_pobjSSL);
                                    SSL_free((SSL*)pobjContext->m_pobjSSL);
                                }

                                pobjContext->m_bKeepAliveTimeOut = true;
                                pobjContext->closeSocket();
                                if(pobjContext->closeContext())
                                {
                                    RELEASE(pobjContext);
                                }
                            }
                        }
                        else
                        {
                            SOCKET_CONTEXT* pobjContext  = (SOCKET_CONTEXT*)g_vpobjNetKeepAliveInfo[i].pobjExtend;
                            if(pobjContext)
                            {
                                pobjContext->m_bKeepAliveTimeOut = true;
                                pobjContext->closeSocket();
                                if(pobjContext->closeContext())
                                {
                                    RELEASE(pobjContext);
                                }
                            }
                        }
#else
                        close(g_vpobjNetKeepAliveInfo[i].nSocket);
                        if(g_nProtocolType == NET_PROTOCOL_HTTPS)
                        {
                            SSL* pobjSSL = (SSL*)g_vpobjNetKeepAliveInfo[i].pobjExtend;
                            if(pobjSSL)
                            {
                                SSL_shutdown (pobjSSL);
                                SSL_free(pobjSSL);
                                g_vpobjNetKeepAliveInfo[i].pobjExtend = NULL;
                            }
                        }
#endif
                        g_vpobjNetKeepAliveInfo[i].init();

                        NetKeepAliveThread::unlockIndex(i);
                        continue;
                    }
               }
            }

            if(g_vpobjNetKeepAliveInfo[i].bCheckReceiveTime && g_vpobjNetKeepAliveInfo[i].bIsAlive)
            {
               if(g_vpobjNetKeepAliveInfo[i].objLastReceiveTime.secsTo(objDateTime) > g_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS)
               {
                    NETLOG(NET_LOG_LEVEL_WORNING, QString("socket:%1, receive time out:%2 s").arg(g_vpobjNetKeepAliveInfo[i].nSocket).arg(g_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS));
                    if(g_vpobjNetKeepAliveInfo[i].bIsAlive)
                    {
                        if(!NetKeepAliveThread::tryLock(i, 5))
                        {
                            continue;
                        }

                        if(!g_vpobjNetKeepAliveInfo[i].bIsAlive)
                        {
                            NetKeepAliveThread::unlockIndex(i);
                            continue;
                        }
#ifdef WIN32
                        if(g_nProtocolType == NET_PROTOCOL_HTTPS)
                        {
                            SOCKET_CONTEXT_SSL* pobjContext  = (SOCKET_CONTEXT_SSL*)g_vpobjNetKeepAliveInfo[i].pobjExtend;
                            if(pobjContext)
                            {
                                if(pobjContext->m_pobjSSL)
                                {
                                    SSL_shutdown ((SSL*)pobjContext->m_pobjSSL);
                                    SSL_free((SSL*)pobjContext->m_pobjSSL);
                                }

                                pobjContext->m_bKeepAliveTimeOut = true;
                                pobjContext->closeSocket();
                                if(pobjContext->closeContext())
                                {
                                    RELEASE(pobjContext);
                                }
                            }
                        }
                        else
                        {
                            SOCKET_CONTEXT* pobjContext  = (SOCKET_CONTEXT*)g_vpobjNetKeepAliveInfo[i].pobjExtend;
                            if(pobjContext)
                            {
                                pobjContext->m_bKeepAliveTimeOut = true;
                                pobjContext->closeSocket();
                                if(pobjContext->closeContext())
                                {
                                    RELEASE(pobjContext);
                                }
                            }
                        }
#else
                        close(g_vpobjNetKeepAliveInfo[i].nSocket);
                        if(g_nProtocolType == NET_PROTOCOL_HTTPS)
                        {
                            SSL* pobjSSL = (SSL*)g_vpobjNetKeepAliveInfo[i].pobjExtend;
                            if(pobjSSL)
                            {
                                SSL_shutdown (pobjSSL);
                                SSL_free(pobjSSL);
                                g_vpobjNetKeepAliveInfo[i].pobjExtend = NULL;
                            }
                        }
#endif
                        g_vpobjNetKeepAliveInfo[i].init();
                        NetKeepAliveThread::unlockIndex(i);
                        continue;
                    }
               }
            }
        }

        //qDebug()<<"Current Connected:"<<nConnectedSize;
        QThread::msleep(1000);
    }
}

bool NetKeepAliveThread::init(qint32 p_nMaxQueueSize, qint32 p_nProtocolType)
{
    g_nProtocolType = p_nProtocolType;

    if(p_nMaxQueueSize <= 0)
    {
        return false;
    }

    g_vpobjNetKeepAliveInfo = new NetKeepAliveInfo[p_nMaxQueueSize];
    g_nNetKeepAliveInfoSize = p_nMaxQueueSize;

    if(g_vpobjNetKeepAliveInfo == NULL)
    {
        return false;
    }

    for(quint32 i = 0; i < g_nNetKeepAliveInfoSize; i++)
    {
        NetKeepAliveThread::lockIndex(i);
        g_vpobjNetKeepAliveInfo[i].init();
        NetKeepAliveThread::unlockIndex(i);
    }

    return true;
}

bool NetKeepAliveThread::addAlive(const NetKeepAliveInfo &p_objNetKeepAliveInfo, quint32 &p_nSissionID, quint32 &p_nIndex)
{
    p_nSissionID = 0;

    for(quint32 i = 0; i < g_nNetKeepAliveInfoSize; i++)
    {
        if(!g_vpobjNetKeepAliveInfo[i].bIsAlive)
        {
            if(!NetKeepAliveThread::tryLock(i,5))
            {
                continue;
            }

            if(g_vpobjNetKeepAliveInfo[i].bIsAlive)
            {
                NetKeepAliveThread::unlockIndex(i);
                continue;
            }

            g_vpobjNetKeepAliveInfo[i].nSocket = p_objNetKeepAliveInfo.nSocket;
            g_vpobjNetKeepAliveInfo[i].bCheckReceiveTime = p_objNetKeepAliveInfo.bCheckReceiveTime;
            g_vpobjNetKeepAliveInfo[i].bCheckSendTime = p_objNetKeepAliveInfo.bCheckSendTime;
            g_vpobjNetKeepAliveInfo[i].bIsAlive = true;
            g_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS = p_objNetKeepAliveInfo.nReceiveTimeOutS;
            g_vpobjNetKeepAliveInfo[i].nSendTimeOutS = p_objNetKeepAliveInfo.nSendTimeOutS;
            g_vpobjNetKeepAliveInfo[i].objLastReceiveTime = p_objNetKeepAliveInfo.objLastReceiveTime;
            g_vpobjNetKeepAliveInfo[i].bCheckSendTime = p_objNetKeepAliveInfo.bCheckSendTime;
            g_vpobjNetKeepAliveInfo[i].pobjExtend = p_objNetKeepAliveInfo.pobjExtend;


            {
                QMutexLocker objLoceker(&g_objKeepAliveMutex);
                p_nSissionID = g_nSissionID++;
                if(p_nSissionID > 99999999)
                {
                    g_nSissionID = 1;
                }
            }

            g_vpobjNetKeepAliveInfo[i].nSissionID = p_nSissionID;

            p_nIndex = i;
            NetKeepAliveThread::unlockIndex(i);
            return true;
        }
    }

    return false;
}

bool NetKeepAliveThread::delAlive(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex)
{
    if(g_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    if(g_vpobjNetKeepAliveInfo[p_nIndex].nSocket == p_nSocket && g_vpobjNetKeepAliveInfo[p_nIndex].bIsAlive
            && g_vpobjNetKeepAliveInfo[p_nIndex].nSissionID > 0 && g_vpobjNetKeepAliveInfo[p_nIndex].nSissionID == p_nSissionID)
    {
#ifndef WIN32
        close(p_nSocket);
#endif
        g_vpobjNetKeepAliveInfo[p_nIndex].init();
        return true;
    }

    return false;
}

bool NetKeepAliveThread::setCheckSend(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex, const bool p_bCheck, const qint32 p_nSendTimeout, void* p_objContxt)
{
    if(g_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    if(g_vpobjNetKeepAliveInfo[p_nIndex].nSocket == p_nSocket && g_vpobjNetKeepAliveInfo[p_nIndex].bIsAlive
            && g_vpobjNetKeepAliveInfo[p_nIndex].nSissionID > 0 && g_vpobjNetKeepAliveInfo[p_nIndex].nSissionID == p_nSissionID)
    {
        if(p_bCheck)
        {
            g_vpobjNetKeepAliveInfo[p_nIndex].objLastSendTime = QDateTime::currentDateTime();
            g_vpobjNetKeepAliveInfo[p_nIndex].nSendTimeOutS = p_nSendTimeout;
        }

    #ifdef WIN32
        if(g_nProtocolType == NET_PROTOCOL_HTTPS)
        {
            SOCKET_CONTEXT_SSL* pobjContext = (SOCKET_CONTEXT_SSL*)g_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
            if(p_objContxt != NULL && pobjContext != NULL)
            {
                if(p_bCheck)
                {
                    pobjContext->appendSendContext((IO_CONTEXT_SSL*)p_objContxt);
                }
                else
                {
                    pobjContext->cancelSendContext((IO_CONTEXT_SSL*)p_objContxt);
                }
            }
        }
        else
        {
            SOCKET_CONTEXT* pobjContext = (SOCKET_CONTEXT*)g_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
            if(p_objContxt != NULL && pobjContext != NULL)
            {
                if(p_bCheck)
                {
                    pobjContext->appendSendContext((IO_CONTEXT*)p_objContxt);
                }
                else
                {
                    pobjContext->cancelSendContext((IO_CONTEXT*)p_objContxt);
                }
            }
        }

    #endif
        g_vpobjNetKeepAliveInfo[p_nIndex].bCheckSendTime = p_bCheck;
        return true;
    }

    return false;
}

bool NetKeepAliveThread::setCheckReceive(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex, const bool p_bCheck, const qint32 p_nReceiveTimeout, void* p_objContxt)
{
    if(g_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    if(g_vpobjNetKeepAliveInfo[p_nIndex].nSocket == p_nSocket && g_vpobjNetKeepAliveInfo[p_nIndex].bIsAlive
            && p_nSissionID > 0 && g_vpobjNetKeepAliveInfo[p_nIndex].nSissionID == p_nSissionID)
    {

        if(p_bCheck)
        {
            g_vpobjNetKeepAliveInfo[p_nIndex].objLastReceiveTime = QDateTime::currentDateTime();
            g_vpobjNetKeepAliveInfo[p_nIndex].nReceiveTimeOutS = p_nReceiveTimeout;
        }

#ifdef WIN32
        if(g_nProtocolType == NET_PROTOCOL_HTTPS)
        {
            SOCKET_CONTEXT_SSL* pobjContext = (SOCKET_CONTEXT_SSL*)g_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
            if(p_objContxt != NULL && pobjContext != NULL)
            {
                if(p_bCheck)
                {
                    pobjContext->appendReceiveContext((IO_CONTEXT_SSL*)p_objContxt);
                }
                else
                {
                    pobjContext->cancelReceiveContext((IO_CONTEXT_SSL*)p_objContxt);
                }
            }
        }
        else
        {
            SOCKET_CONTEXT* pobjContext = (SOCKET_CONTEXT*)g_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
            if(p_objContxt != NULL && pobjContext != NULL)
            {
                if(p_bCheck)
                {
                    pobjContext->appendReceiveContext((IO_CONTEXT*)p_objContxt);
                }
                else
                {
                    pobjContext->cancelReceiveContext((IO_CONTEXT*)p_objContxt);
                }
            }
        }
#endif

        g_vpobjNetKeepAliveInfo[p_nIndex].bCheckReceiveTime = p_bCheck;

        return true;
    }

    return false;
}

bool NetKeepAliveThread::closeConnect(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex)
{
    if(g_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    if(g_vpobjNetKeepAliveInfo[p_nIndex].bIsAlive && g_vpobjNetKeepAliveInfo[p_nIndex].nSocket == p_nSocket
            && g_vpobjNetKeepAliveInfo[p_nIndex].nSissionID == p_nSissionID)
    {
        NetKeepAliveThread::lockIndex(p_nIndex);
        if(!g_vpobjNetKeepAliveInfo[p_nIndex].bIsAlive)
        {
            NetKeepAliveThread::unlockIndex(p_nIndex);
            return true;
        }

#ifdef WIN32
        if(g_nProtocolType == NET_PROTOCOL_HTTPS)
        {
            SOCKET_CONTEXT_SSL* pobjContext  = (SOCKET_CONTEXT_SSL*)g_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
            if(pobjContext)
            {
                if(pobjContext->m_pobjSSL)
                {
                    SSL_shutdown ((SSL*)pobjContext->m_pobjSSL);
                    SSL_free((SSL*)pobjContext->m_pobjSSL);
                }

                pobjContext->m_bKeepAliveTimeOut = true;
                pobjContext->closeSocket();
                if(pobjContext->closeContext())
                {
                    RELEASE(pobjContext);
                }
            }
        }
        else
        {
            SOCKET_CONTEXT* pobjContext  = (SOCKET_CONTEXT*)g_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
            if(pobjContext)
            {
                pobjContext->m_bKeepAliveTimeOut = true;
                pobjContext->closeSocket();
                if(pobjContext->closeContext())
                {
                    RELEASE(pobjContext);
                }
            }
        }
#else
        close(g_vpobjNetKeepAliveInfo[p_nIndex].nSocket);
        if(g_nProtocolType == NET_PROTOCOL_HTTPS)
        {
            SSL* pobjSSL = (SSL*)g_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
            if(pobjSSL)
            {
                SSL_shutdown (pobjSSL);
                SSL_free(pobjSSL);
                g_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend = NULL;
            }
        }
#endif
        g_vpobjNetKeepAliveInfo[p_nIndex].init();
        NetKeepAliveThread::unlockIndex(p_nIndex);
        return true;
    }

    return true;
}

bool NetKeepAliveThread::lockIndex(const quint32 p_nIndex)
{
    if(g_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    g_vpobjNetKeepAliveInfo[p_nIndex].objExtendMutex.lock();
    return true;
}

bool NetKeepAliveThread::tryLock(const quint32 p_nIndex, const qint32 p_nTryTimeOutMs)
{
    if(g_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    return g_vpobjNetKeepAliveInfo[p_nIndex].objExtendMutex.tryLock(p_nTryTimeOutMs);
}

bool NetKeepAliveThread::unlockIndex(const quint32 p_nIndex)
{
    if(g_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    g_vpobjNetKeepAliveInfo[p_nIndex].objExtendMutex.unlock();
    return true;
}

bool NetKeepAliveThread::lockIndexContext(const quint32 p_nIndex, const quint64 p_nSocket, const quint32 p_nSissionID, void* &p_pobjExtend)
{
    if(g_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    g_vpobjNetKeepAliveInfo[p_nIndex].objExtendMutex.lock();

    if(g_vpobjNetKeepAliveInfo[p_nIndex].nSocket == p_nSocket && g_vpobjNetKeepAliveInfo[p_nIndex].nSissionID == p_nSissionID)
    {
        p_pobjExtend = g_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
        return true;
    }

    g_vpobjNetKeepAliveInfo[p_nIndex].objExtendMutex.unlock();
    return false;
}

bool NetKeepAliveThread::getExtend(const quint32 p_nIndex, void* &p_pobjExtend)
{
    if(g_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    p_pobjExtend = g_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
    return true;
}
