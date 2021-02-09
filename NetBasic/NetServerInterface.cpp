#include "NetServerInterface.h"
#include "NetEnum.h"
#include "NetSocketIocp.h"
#include "NetSocketEpoll.h"
#include "NetPacketManager.h"
#include "NetSocketEpollSSL.h"
#include "NetSocketIocpSSL.h"
#include "NetLog.h"

NetServerInterface::NetServerInterface()
{
    m_pobjSocketBase = NULL;
    m_pobjNetKeepAliveThread = NULL;
    m_pobjNetPacketManager = NULL;
}

NetServerInterface::~NetServerInterface()
{
    uninit();
}

void NetServerInterface::setAppLogCallBack(const qint32 p_nLogLevel, CallAppLog p_fnApplog)
{
    NetLog::g_nLogLevel = p_nLogLevel;
    NetLog::g_fnAppLogCallBack = p_fnApplog;
}

bool NetServerInterface::init(const qint32 p_nProtocol, const qint32 p_nThreadNum, CallAppReceivePacket p_fnAppReceivePacket, void *p_pMaster, const QString& p_strKeyPath, const QString& p_strCertPath)
{
    if(m_pobjNetPacketManager == NULL)
    {
        m_pobjNetPacketManager = new NetPacketManager;
    }

    if(!m_pobjNetPacketManager->init(p_nProtocol, p_fnAppReceivePacket, p_pMaster))
    {
        return false;
    }

    if(m_pobjNetKeepAliveThread == NULL)
    {
        m_pobjNetKeepAliveThread = new NetKeepAliveThread;
    }

    if(!m_pobjNetKeepAliveThread->init(KEEPALIVE_MAXSIZE, p_nProtocol))
    {
        return false;
    }

    if(KEEPALIVE_DETECT)
    {
        m_pobjNetKeepAliveThread->start();
    }

    if(m_pobjSocketBase == NULL)
    {
        delete m_pobjSocketBase;
        m_pobjSocketBase = NULL;
    }

#ifdef WIN32
    if(p_nProtocol == NET_PROTOCOL_HTTP)
    {
        m_pobjSocketBase = new NetSocketIocp;
    }
    else if(p_nProtocol == NET_PROTOCOL_HTTPS)
    {
        m_pobjSocketBase = new NetSocketIocpSSL;
    }
#else
    if(p_nProtocol == NET_PROTOCOL_HTTP)
    {
        m_pobjSocketBase = new NetSocketEpoll;
    }
    else if(p_nProtocol == NET_PROTOCOL_HTTPS)
    {
        m_pobjSocketBase = new NetSocketEpollSSL;
    }
#endif

    if(m_pobjSocketBase)
    {
        return m_pobjSocketBase->init(p_nThreadNum, m_pobjNetPacketManager, m_pobjNetKeepAliveThread, p_strKeyPath, p_strCertPath);
    }

    return false;
}

bool NetServerInterface::start(const QString &p_strBindIP, const qint32 p_nPort)
{
    if(m_pobjSocketBase == NULL)
    {
        return false;
    }

    return m_pobjSocketBase->start(p_strBindIP, p_nPort);
}

bool NetServerInterface::send(NetPacketBase *pobjNetPacketBase)
{
    if(m_pobjSocketBase == NULL)
    {
        return false;
    }

    return m_pobjSocketBase->send(pobjNetPacketBase);
}

bool NetServerInterface::closeConnect(NetPacketBase *pobjNetPacketBase)
{
    if(m_pobjSocketBase == NULL)
    {
        return false;
    }

    return m_pobjNetKeepAliveThread->closeConnect(pobjNetPacketBase->m_nSocket, pobjNetPacketBase->m_nSissionID, pobjNetPacketBase->m_nIndex);
}

void NetServerInterface::uninit()
{
    if(m_pobjSocketBase)
    {
        delete m_pobjSocketBase;
        m_pobjSocketBase = NULL;
    }

    m_pobjNetPacketManager->uninit();

    if(m_pobjNetPacketManager)
    {
        delete m_pobjNetPacketManager;
        m_pobjNetPacketManager = NULL;
    }

    if(m_pobjNetKeepAliveThread)
    {
        delete m_pobjNetKeepAliveThread;
        m_pobjNetKeepAliveThread = NULL;
    }
}
