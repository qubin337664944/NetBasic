#include "NetKeepAliveThread.h"
#include "NetSocketIocp.h"
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
                    delAlive(g_vpobjNetKeepAliveInfo[i].nSocket);
                    continue;
               }
            }

            if(g_vpobjNetKeepAliveInfo[i].bCheckReceiveTime && g_vpobjNetKeepAliveInfo[i].bIsAlive)
            {
               if(g_vpobjNetKeepAliveInfo[i].objLastReceiveTime.secsTo(objDateTime) > g_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS)
               {
                    NETLOG(NET_LOG_LEVEL_WORNING, QString("socket:%1, receive time out:%2 s").arg(g_vpobjNetKeepAliveInfo[i].nSocket).arg(g_vpobjNetKeepAliveInfo[i].nReceiveTimeOutS));
                    delAlive(g_vpobjNetKeepAliveInfo[i].nSocket);
                    continue;
               }
            }
        }

        QThread::msleep(1000);
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
            close(p_nSocket);

            g_vpobjNetKeepAliveInfo[i].init();
            return true;
        }
    }

    return false;
}

bool NetKeepAliveThread::setCheckSend(const quint64 p_nSocket, const bool p_bCheck, const qint32 p_nSendTimeout)
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

            g_vpobjNetKeepAliveInfo[i].bCheckSendTime = p_bCheck;
            return true;
        }
    }

    return false;
}

bool NetKeepAliveThread::setCheckReceive(const quint64 p_nSocket, const bool p_bCheck, const qint32 p_nReceiveTimeout)
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
