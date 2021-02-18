#include "NetKeepAliveThread.h"
#include "NetSocketIocp.h"
#include "NetSocketIocpSSL.h"
#include "NetSocketEpollSSL.h"
#include "NetLog.h"
#include <QDebug>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef WIN32
#define close(x)                closesocket(x);
#endif

NetKeepAliveThread::NetKeepAliveThread()
{
    m_nSissionID = 1;
}

NetKeepAliveThread::~NetKeepAliveThread()
{
    uninit();
}

void NetKeepAliveThread::run()
{
    while(1)
    {
        QDateTime objDateTime = QDateTime::currentDateTime();

        qint32 nConnectedSize = 0;
        for(int i = 0; i < m_nNetKeepAliveInfoSize; i++)
        {
            if(m_vpobjNetKeepAliveInfo[i].bIsAlive)
            {
                nConnectedSize++;
            }

            if(m_vpobjNetKeepAliveInfo[i].bCheckSendTime && m_vpobjNetKeepAliveInfo[i].bIsAlive)
            {
               if(m_vpobjNetKeepAliveInfo[i].objLastSendTime.secsTo(objDateTime) > m_vpobjNetKeepAliveInfo[i].nSendTimeOutS)
               {
                    NETLOG(NET_LOG_LEVEL_WORNING, QString("socket:%1, send time out:%2 s").arg(m_vpobjNetKeepAliveInfo[i].nSocket).arg(m_vpobjNetKeepAliveInfo[i].nSendTimeOutS));
                    if(m_vpobjNetKeepAliveInfo[i].bIsAlive)
                    {
                        if(!NetKeepAliveThread::tryLock(i, 5))
                        {
                            continue;
                        }

                        if(!m_vpobjNetKeepAliveInfo[i].bIsAlive)
                        {
                            NetKeepAliveThread::unlockIndex(i);
                            continue;
                        }
#ifdef WIN32
                        if(m_nProtocolType == NET_PROTOCOL_HTTPS)
                        {
                            SOCKET_CONTEXT_SSL* pobjContext  = (SOCKET_CONTEXT_SSL*)m_vpobjNetKeepAliveInfo[i].pobjExtend;
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
                            SOCKET_CONTEXT* pobjContext  = (SOCKET_CONTEXT*)m_vpobjNetKeepAliveInfo[i].pobjExtend;
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
                        close(m_vpobjNetKeepAliveInfo[i].nSocket);
                        if(m_nProtocolType == NET_PROTOCOL_HTTPS)
                        {
                            SSL* pobjSSL = (SSL*)m_vpobjNetKeepAliveInfo[i].pobjExtend;
                            if(pobjSSL)
                            {
                                SSL_shutdown (pobjSSL);
                                SSL_free(pobjSSL);
                                m_vpobjNetKeepAliveInfo[i].pobjExtend = NULL;
                            }
                        }
#endif
                        m_vpobjNetKeepAliveInfo[i].init();

                        NetKeepAliveThread::unlockIndex(i);
                        continue;
                    }
               }
            }

            if(m_vpobjNetKeepAliveInfo[i].bCheckReceiveTime && m_vpobjNetKeepAliveInfo[i].bIsAlive)
            {
               if(m_vpobjNetKeepAliveInfo[i].objLastReceiveTime.secsTo(objDateTime) > m_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS)
               {
                    NETLOG(NET_LOG_LEVEL_WORNING, QString("socket:%1, receive time out:%2 s").arg(m_vpobjNetKeepAliveInfo[i].nSocket).arg(m_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS));
                    if(m_vpobjNetKeepAliveInfo[i].bIsAlive)
                    {
                        if(!NetKeepAliveThread::tryLock(i, 5))
                        {
                            continue;
                        }

                        if(!m_vpobjNetKeepAliveInfo[i].bIsAlive)
                        {
                            NetKeepAliveThread::unlockIndex(i);
                            continue;
                        }
#ifdef WIN32
                        if(m_nProtocolType == NET_PROTOCOL_HTTPS)
                        {
                            SOCKET_CONTEXT_SSL* pobjContext  = (SOCKET_CONTEXT_SSL*)m_vpobjNetKeepAliveInfo[i].pobjExtend;
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
                            SOCKET_CONTEXT* pobjContext  = (SOCKET_CONTEXT*)m_vpobjNetKeepAliveInfo[i].pobjExtend;
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
                        close(m_vpobjNetKeepAliveInfo[i].nSocket);
                        if(m_nProtocolType == NET_PROTOCOL_HTTPS)
                        {
                            SSL* pobjSSL = (SSL*)m_vpobjNetKeepAliveInfo[i].pobjExtend;
                            if(pobjSSL)
                            {
                                SSL_shutdown (pobjSSL);
                                SSL_free(pobjSSL);
                                m_vpobjNetKeepAliveInfo[i].pobjExtend = NULL;
                            }
                        }
#endif
                        m_vpobjNetKeepAliveInfo[i].init();
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
    m_nProtocolType = p_nProtocolType;

    if(p_nMaxQueueSize <= 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("NetKeepAliveThread init p_nMaxQueueSize <= 0"));
        return false;
    }

    m_vpobjNetKeepAliveInfo = new NetKeepAliveInfo[p_nMaxQueueSize];
    m_nNetKeepAliveInfoSize = p_nMaxQueueSize;

    if(m_vpobjNetKeepAliveInfo == NULL)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("m_vpobjNetKeepAliveInfo = NULL"));
        return false;
    }

    for(quint32 i = 0; i < m_nNetKeepAliveInfoSize; i++)
    {
        NetKeepAliveThread::lockIndex(i);
        m_vpobjNetKeepAliveInfo[i].init();
        NetKeepAliveThread::unlockIndex(i);
    }

    return true;
}

void NetKeepAliveThread::uninit()
{
    if(!isFinished())
    {
        terminate();
    }

    if(m_vpobjNetKeepAliveInfo)
    {
       delete [] m_vpobjNetKeepAliveInfo;
       m_vpobjNetKeepAliveInfo = NULL;
    }

    m_nNetKeepAliveInfoSize = 0;
}

bool NetKeepAliveThread::addAlive(const NetKeepAliveInfo &p_objNetKeepAliveInfo, quint32 &p_nSissionID, quint32 &p_nIndex)
{
    p_nSissionID = 0;

    for(quint32 i = 0; i < m_nNetKeepAliveInfoSize; i++)
    {
        if(!m_vpobjNetKeepAliveInfo[i].bIsAlive)
        {
            if(!NetKeepAliveThread::tryLock(i,5))
            {
                continue;
            }

            if(m_vpobjNetKeepAliveInfo[i].bIsAlive)
            {
                NetKeepAliveThread::unlockIndex(i);
                continue;
            }

            m_vpobjNetKeepAliveInfo[i].nSocket = p_objNetKeepAliveInfo.nSocket;
            m_vpobjNetKeepAliveInfo[i].bCheckReceiveTime = p_objNetKeepAliveInfo.bCheckReceiveTime;
            m_vpobjNetKeepAliveInfo[i].bCheckSendTime = p_objNetKeepAliveInfo.bCheckSendTime;
            m_vpobjNetKeepAliveInfo[i].bIsAlive = true;
            m_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS = p_objNetKeepAliveInfo.nReceiveTimeOutS;
            m_vpobjNetKeepAliveInfo[i].nSendTimeOutS = p_objNetKeepAliveInfo.nSendTimeOutS;
            m_vpobjNetKeepAliveInfo[i].objLastReceiveTime = p_objNetKeepAliveInfo.objLastReceiveTime;
            m_vpobjNetKeepAliveInfo[i].bCheckSendTime = p_objNetKeepAliveInfo.bCheckSendTime;
            m_vpobjNetKeepAliveInfo[i].pobjExtend = p_objNetKeepAliveInfo.pobjExtend;


            {
                QMutexLocker objLoceker(&m_objKeepAliveMutex);
                p_nSissionID = m_nSissionID++;
                if(p_nSissionID > 999999999)
                {
                    m_nSissionID = 1;
                }
            }

            m_vpobjNetKeepAliveInfo[i].nSissionID = p_nSissionID;

            p_nIndex = i;
            NetKeepAliveThread::unlockIndex(i);
            return true;
        }
    }

    return false;
}

bool NetKeepAliveThread::delAlive(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex)
{
    if(m_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    if(m_vpobjNetKeepAliveInfo[p_nIndex].nSocket == p_nSocket && m_vpobjNetKeepAliveInfo[p_nIndex].bIsAlive
            && m_vpobjNetKeepAliveInfo[p_nIndex].nSissionID > 0 && m_vpobjNetKeepAliveInfo[p_nIndex].nSissionID == p_nSissionID)
    {
#ifndef WIN32
        close(p_nSocket);
#endif
        m_vpobjNetKeepAliveInfo[p_nIndex].init();
        return true;
    }

    return false;
}

bool NetKeepAliveThread::setCheckSend(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex, const bool p_bCheck, const qint32 p_nSendTimeout, void* p_objContxt)
{
    if(m_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    if(m_vpobjNetKeepAliveInfo[p_nIndex].nSocket == p_nSocket && m_vpobjNetKeepAliveInfo[p_nIndex].bIsAlive
            && m_vpobjNetKeepAliveInfo[p_nIndex].nSissionID > 0 && m_vpobjNetKeepAliveInfo[p_nIndex].nSissionID == p_nSissionID)
    {
        if(p_bCheck)
        {
            m_vpobjNetKeepAliveInfo[p_nIndex].objLastSendTime = QDateTime::currentDateTime();
            m_vpobjNetKeepAliveInfo[p_nIndex].nSendTimeOutS = p_nSendTimeout;
        }

    #ifdef WIN32
        if(m_nProtocolType == NET_PROTOCOL_HTTPS)
        {
            SOCKET_CONTEXT_SSL* pobjContext = (SOCKET_CONTEXT_SSL*)m_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
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
            SOCKET_CONTEXT* pobjContext = (SOCKET_CONTEXT*)m_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
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
        m_vpobjNetKeepAliveInfo[p_nIndex].bCheckSendTime = p_bCheck;
        return true;
    }

    return false;
}

bool NetKeepAliveThread::setCheckReceive(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex, const bool p_bCheck, const qint32 p_nReceiveTimeout, void* p_objContxt)
{
    if(m_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    if(m_vpobjNetKeepAliveInfo[p_nIndex].nSocket == p_nSocket && m_vpobjNetKeepAliveInfo[p_nIndex].bIsAlive
            && p_nSissionID > 0 && m_vpobjNetKeepAliveInfo[p_nIndex].nSissionID == p_nSissionID)
    {

        if(p_bCheck)
        {
            m_vpobjNetKeepAliveInfo[p_nIndex].objLastReceiveTime = QDateTime::currentDateTime();
            m_vpobjNetKeepAliveInfo[p_nIndex].nReceiveTimeOutS = p_nReceiveTimeout;
        }

#ifdef WIN32
        if(m_nProtocolType == NET_PROTOCOL_HTTPS)
        {
            SOCKET_CONTEXT_SSL* pobjContext = (SOCKET_CONTEXT_SSL*)m_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
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
            SOCKET_CONTEXT* pobjContext = (SOCKET_CONTEXT*)m_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
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

        m_vpobjNetKeepAliveInfo[p_nIndex].bCheckReceiveTime = p_bCheck;

        return true;
    }

    return false;
}

bool NetKeepAliveThread::closeConnect(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex)
{
    if(m_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    if(m_vpobjNetKeepAliveInfo[p_nIndex].bIsAlive && m_vpobjNetKeepAliveInfo[p_nIndex].nSocket == p_nSocket
            && m_vpobjNetKeepAliveInfo[p_nIndex].nSissionID == p_nSissionID)
    {
        NetKeepAliveThread::lockIndex(p_nIndex);
        if(!m_vpobjNetKeepAliveInfo[p_nIndex].bIsAlive)
        {
            NetKeepAliveThread::unlockIndex(p_nIndex);
            return true;
        }

#ifdef WIN32
        if(m_nProtocolType == NET_PROTOCOL_HTTPS)
        {
            SOCKET_CONTEXT_SSL* pobjContext  = (SOCKET_CONTEXT_SSL*)m_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
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
            SOCKET_CONTEXT* pobjContext  = (SOCKET_CONTEXT*)m_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
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
        close(m_vpobjNetKeepAliveInfo[p_nIndex].nSocket);
        if(m_nProtocolType == NET_PROTOCOL_HTTPS)
        {
            SSL* pobjSSL = (SSL*)m_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
            if(pobjSSL)
            {
                SSL_shutdown (pobjSSL);
                SSL_free(pobjSSL);
                m_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend = NULL;
            }
        }
#endif
        m_vpobjNetKeepAliveInfo[p_nIndex].init();
        NetKeepAliveThread::unlockIndex(p_nIndex);
        return true;
    }

    return true;
}

bool NetKeepAliveThread::lockIndex(const quint32 p_nIndex)
{
    if(m_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    m_vpobjNetKeepAliveInfo[p_nIndex].objExtendMutex.lock();
    return true;
}

bool NetKeepAliveThread::tryLock(const quint32 p_nIndex, const qint32 p_nTryTimeOutMs)
{
    if(m_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    return m_vpobjNetKeepAliveInfo[p_nIndex].objExtendMutex.tryLock(p_nTryTimeOutMs);
}

bool NetKeepAliveThread::unlockIndex(const quint32 p_nIndex)
{
    if(m_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    m_vpobjNetKeepAliveInfo[p_nIndex].objExtendMutex.unlock();
    return true;
}

bool NetKeepAliveThread::lockIndexContext(const quint32 p_nIndex, const quint64 p_nSocket, const quint32 p_nSissionID, void* &p_pobjExtend)
{
    if(m_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    m_vpobjNetKeepAliveInfo[p_nIndex].objExtendMutex.lock();

    if(m_vpobjNetKeepAliveInfo[p_nIndex].nSocket == p_nSocket && m_vpobjNetKeepAliveInfo[p_nIndex].nSissionID == p_nSissionID)
    {
        p_pobjExtend = m_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
        return true;
    }

    m_vpobjNetKeepAliveInfo[p_nIndex].objExtendMutex.unlock();
    return false;
}

bool NetKeepAliveThread::getExtend(const quint32 p_nIndex, void* &p_pobjExtend)
{
    if(m_nNetKeepAliveInfoSize < p_nIndex - 1 && p_nIndex != 0)
    {
        return false;
    }

    p_pobjExtend = m_vpobjNetKeepAliveInfo[p_nIndex].pobjExtend;
    return true;
}
