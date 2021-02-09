#include "NetPacketManager.h"
#include "NetEnum.h"
#include "NetProcotolParseHttp.h"
#include "NetPacketHttp.h"
#include "NetLog.h"

NetPacketManager::NetPacketManager()
{
    m_pMaster = NULL;
    m_fnSuccessReceivePacket = NULL;
}

bool NetPacketManager::init(qint32 p_nProtocolType, CallAppReceivePacket p_fnSuccessReceivePacket, void *p_pMaster)
{
    m_pMaster = p_pMaster;
    m_fnSuccessReceivePacket = p_fnSuccessReceivePacket;

    if(p_nProtocolType == NET_PROTOCOL_HTTP || p_nProtocolType == NET_PROTOCOL_HTTPS)
    {
        m_pobjNetProtocolParseBase = new NetProcotolParseHttp;
        m_nProtocolType = p_nProtocolType;
        return true;
    }

    return false;
}

void NetPacketManager::uninit()
{
    m_pMaster = NULL;
    m_fnSuccessReceivePacket = NULL;

    if(m_pobjNetProtocolParseBase)
    {
        delete m_pobjNetProtocolParseBase;
    }
}

NetPacketBase *NetPacketManager::allocPacket()
{
    if(m_nProtocolType == NET_PROTOCOL_HTTP || m_nProtocolType == NET_PROTOCOL_HTTPS)
    {
        return new NetPacketHttp;
    }

    return NULL;
}

bool NetPacketManager::processCallBack(NetPacketBase *p_pobjNetPacketBase)
{
    if(m_fnSuccessReceivePacket)
    {
        m_fnSuccessReceivePacket(p_pobjNetPacketBase, m_pMaster);
    }

    return true;
}

bool NetPacketManager::appendReceiveBuffer(NetPacketBase* p_pobjNetPacketBase, char *p_szData, qint32 p_nDataLen)
{
    bool bRet = m_pobjNetProtocolParseBase->parsePacket(p_pobjNetPacketBase, p_szData, p_nDataLen);
    if(!bRet)
    {
        return bRet;
    }

    return true;
}

bool NetPacketManager::prepareResponse(NetPacketBase *p_pobjNetPacketBase, QByteArray &p_bytResponse)
{
    if(m_pobjNetProtocolParseBase)
    {
        return m_pobjNetProtocolParseBase->prepareResponse(p_pobjNetPacketBase, p_bytResponse);
    }
    else
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("g_pobjNetProtocolParseBase is null"));
    }

    return false;
}
