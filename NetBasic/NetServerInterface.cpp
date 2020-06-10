#include "NetServerInterface.h"
#include "NetEnum.h"
#include "NetSocketIocp.h"
#include "NetSocketEpoll.h"
#include "NetPacketManager.h"
#include "NetSocketEpollSSL.h"
#include "NetLog.h"

NetServerInterface::NetServerInterface()
{
    m_pobjSocketBase = NULL;
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

void NetServerInterface::setSslKetCertPath(const QString &p_strKeyPath, const QString &p_strCertPath)
{
#ifdef WIN32
#else
    NetSocketEpollSSL::g_strKeyPath = p_strKeyPath;
    NetSocketEpollSSL::g_strCertPath = p_strCertPath;
#endif
}

bool NetServerInterface::init(const qint32 p_nProtocol, const qint32 p_nThreadNum, CallAppReceivePacket p_fnAppReceivePacket, void *p_pMaster)
{
    NetPacketManager::init(p_nProtocol, p_fnAppReceivePacket, p_pMaster);

    if(!objNetKeepAliveThread.init(KEEPALIVE_MAXSIZE))
    {
        return false;
    }

    if(KEEPALIVE_DETECT)
    {
        objNetKeepAliveThread.start();
    }

#ifdef WIN32
    if(p_nProtocol == NET_PROTOCOL_HTTP)
    {
        m_pobjSocketBase = new NetSocketIocp;
        return m_pobjSocketBase->init(p_nThreadNum);
    }
#else
    if(p_nProtocol == NET_PROTOCOL_HTTP)
    {
        m_pobjSocketBase = new NetSocketEpoll;
        return m_pobjSocketBase->init(p_nThreadNum);
    }
    else if(p_nProtocol == NET_PROTOCOL_HTTPS)
    {
        m_pobjSocketBase = new NetSocketEpollSSL;
        return m_pobjSocketBase->init(p_nThreadNum);
    }
#endif

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

void NetServerInterface::uninit()
{
    if(m_pobjSocketBase)
    {
        delete m_pobjSocketBase;
    }

    NetPacketManager::uninit();
}
