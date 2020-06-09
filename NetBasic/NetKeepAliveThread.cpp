#include "NetKeepAliveThread.h"
#include "NetSocketIocp.h"
#include "NetSocketEpollSSL.h"
#include "NetLog.h"

NetKeepAliveInfo* NetKeepAliveThread::g_vpobjNetKeepAliveInfo = NULL;
qint32   NetKeepAliveThread::g_nNetKeepAliveInfoSize = 0;
QMutex NetKeepAliveThread::g_objKeepAliveMutex;

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

        for(int i = 0; i < g_nNetKeepAliveInfoSize; i++)
        {
            if(g_vpobjNetKeepAliveInfo[i].bCheckSendTime && g_vpobjNetKeepAliveInfo[i].bIsAlive)
            {
               if(g_vpobjNetKeepAliveInfo[i].objLastSendTime.secsTo(objDateTime) > g_vpobjNetKeepAliveInfo[i].nSendTimeOutS)
               {
                    NETLOG(NET_LOG_LEVEL_WORNING, QString("socket:%1, send time out:%2 s").arg(g_vpobjNetKeepAliveInfo[i].nSocket).arg(g_vpobjNetKeepAliveInfo[i].nSendTimeOutS));
#ifdef WIN32
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
#else
                    close(g_vpobjNetKeepAliveInfo[i].nSocket);
#endif
                    QMutexLocker objLoceker(&g_objKeepAliveMutex);
                    g_vpobjNetKeepAliveInfo[i].init();
                    continue;
               }
            }

            if(g_vpobjNetKeepAliveInfo[i].bCheckReceiveTime && g_vpobjNetKeepAliveInfo[i].bIsAlive)
            {
               if(g_vpobjNetKeepAliveInfo[i].objLastReceiveTime.secsTo(objDateTime) > g_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS)
               {
                    NETLOG(NET_LOG_LEVEL_WORNING, QString("socket:%1, receive time out:%2 s").arg(g_vpobjNetKeepAliveInfo[i].nSocket).arg(g_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS));
#ifdef WIN32
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
#else
                    close(g_vpobjNetKeepAliveInfo[i].nSocket);
#endif
                    QMutexLocker objLoceker(&g_objKeepAliveMutex);
                    g_vpobjNetKeepAliveInfo[i].init();
                    continue;
               }
            }
        }

        QThread::msleep(100);
    }
}

bool NetKeepAliveThread::init(qint32 p_nMaxQueueSize)
{
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

    for(int i = 0; i < g_nNetKeepAliveInfoSize; i++)
    {
        g_vpobjNetKeepAliveInfo[i].init();
    }

    return true;
}

bool NetKeepAliveThread::addAlive(const NetKeepAliveInfo &p_objNetKeepAliveInfo)
{
    QMutexLocker objLoceker(&g_objKeepAliveMutex);
    for(int i = 0; i < g_nNetKeepAliveInfoSize; i++)
    {
        if(!g_vpobjNetKeepAliveInfo[i].bIsAlive)
        {
            g_vpobjNetKeepAliveInfo[i].nSocket = p_objNetKeepAliveInfo.nSocket;
            g_vpobjNetKeepAliveInfo[i].bCheckReceiveTime = p_objNetKeepAliveInfo.bCheckReceiveTime;
            g_vpobjNetKeepAliveInfo[i].bCheckSendTime = p_objNetKeepAliveInfo.bCheckSendTime;
            g_vpobjNetKeepAliveInfo[i].bIsAlive = true;
            g_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS = p_objNetKeepAliveInfo.nReceiveTimeOutS;
            g_vpobjNetKeepAliveInfo[i].nSendTimeOutS = p_objNetKeepAliveInfo.nSendTimeOutS;
            g_vpobjNetKeepAliveInfo[i].objLastReceiveTime = p_objNetKeepAliveInfo.objLastReceiveTime;
            g_vpobjNetKeepAliveInfo[i].bCheckSendTime = p_objNetKeepAliveInfo.bCheckSendTime;
            g_vpobjNetKeepAliveInfo[i].pobjExtend = p_objNetKeepAliveInfo.pobjExtend;

            return true;
        }
    }

    return false;
}

bool NetKeepAliveThread::delAlive(const quint64 p_nSocket)
{
    QMutexLocker objLoceker(&g_objKeepAliveMutex);
    for(int i = 0; i < g_nNetKeepAliveInfoSize; i++)
    {
        if(g_vpobjNetKeepAliveInfo[i].nSocket == p_nSocket)
        {
            g_vpobjNetKeepAliveInfo[i].init();
            return true;
        }
    }

    return false;
}

bool NetKeepAliveThread::setCheckSend(const quint64 p_nSocket, const bool p_bCheck, const qint32 p_nSendTimeout, void* p_objContxt)
{
    for(int i = 0; i < g_nNetKeepAliveInfoSize; i++)
    {
        if(g_vpobjNetKeepAliveInfo[i].nSocket == p_nSocket && g_vpobjNetKeepAliveInfo[i].bIsAlive)
        {
            if(p_bCheck)
            {
                g_vpobjNetKeepAliveInfo[i].objLastSendTime = QDateTime::currentDateTime();
                g_vpobjNetKeepAliveInfo[i].nSendTimeOutS = p_nSendTimeout;
            }

#ifdef WIN32
            SOCKET_CONTEXT* pobjContext = (SOCKET_CONTEXT*)g_vpobjNetKeepAliveInfo[i].pobjExtend;
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
#endif
            g_vpobjNetKeepAliveInfo[i].bCheckSendTime = p_bCheck;
            return true;
        }
    }

    return false;
}

bool NetKeepAliveThread::setCheckReceive(const quint64 p_nSocket, const bool p_bCheck, const qint32 p_nReceiveTimeout, void* p_objContxt)
{
    for(int i = 0; i < g_nNetKeepAliveInfoSize; i++)
    {
        if(g_vpobjNetKeepAliveInfo[i].nSocket == p_nSocket && g_vpobjNetKeepAliveInfo[i].bIsAlive)
        {
            if(p_bCheck)
            {
                g_vpobjNetKeepAliveInfo[i].objLastReceiveTime = QDateTime::currentDateTime();
                g_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS = p_nReceiveTimeout;
            }

#ifdef WIN32
            SOCKET_CONTEXT* pobjContext = (SOCKET_CONTEXT*)g_vpobjNetKeepAliveInfo[i].pobjExtend;
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
#endif

            g_vpobjNetKeepAliveInfo[i].bCheckReceiveTime = p_bCheck;
            return true;
        }
    }

    return false;
}

bool NetKeepAliveThread::modifySendTime(const quint64 p_nSocket)
{
    for(int i = 0; i < g_nNetKeepAliveInfoSize; i++)
    {
        if(g_vpobjNetKeepAliveInfo[i].nSocket == p_nSocket && g_vpobjNetKeepAliveInfo[i].bIsAlive)
        {
            g_vpobjNetKeepAliveInfo[i].objLastSendTime = QDateTime::currentDateTime();
            return true;
        }
    }

    return false;
}

bool NetKeepAliveThread::modifyReceiveTime(const quint64 p_nSocket)
{
    for(int i = 0; i < g_nNetKeepAliveInfoSize; i++)
    {
        if(g_vpobjNetKeepAliveInfo[i].nSocket == p_nSocket && g_vpobjNetKeepAliveInfo[i].bIsAlive)
        {
            g_vpobjNetKeepAliveInfo[i].objLastReceiveTime = QDateTime::currentDateTime();
            return true;
        }
    }

    return false;
}
