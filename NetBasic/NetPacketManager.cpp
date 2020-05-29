#include "NetPacketManager.h"
#include "NetEnum.h"
#include "NetProcotolParseHttp.h"
#include "NetPacketHttp.h"
#include "NetLog.h"

NetProtocolParseBase* NetPacketManager::g_pobjNetProtocolParseBase = NULL;
CallAppReceivePacket NetPacketManager::g_fnSuccessReceivePacket;
qint32 NetPacketManager::g_nProtocolType;
void* NetPacketManager::g_pMaster = NULL;

NetPacketManager::NetPacketManager()
{
}

bool NetPacketManager::init(qint32 p_nProtocolType, CallAppReceivePacket p_fnSuccessReceivePacket, void *p_pMaster)
{
    g_pMaster = p_pMaster;
    g_fnSuccessReceivePacket = p_fnSuccessReceivePacket;

    if(p_nProtocolType == NET_PROTOCOL_HTTP || p_nProtocolType == NET_PROTOCOL_HTTPS)
    {
        g_pobjNetProtocolParseBase = new NetProcotolParseHttp;
        g_nProtocolType = p_nProtocolType;
        return true;
    }

    return false;
}

void NetPacketManager::uninit()
{
    g_pMaster = NULL;
    g_fnSuccessReceivePacket = NULL;

    delete g_pobjNetProtocolParseBase;
}

NetPacketBase *NetPacketManager::allocPacket()
{
    if(g_nProtocolType == NET_PROTOCOL_HTTP || g_nProtocolType == NET_PROTOCOL_HTTPS)
    {
        return new NetPacketHttp;
    }

    return NULL;
}

bool NetPacketManager::processCallBack(NetPacketBase *p_pobjNetPacketBase)
{
    if(g_fnSuccessReceivePacket)
    {
        g_fnSuccessReceivePacket(p_pobjNetPacketBase, g_pMaster);
    }

    return true;
}

bool NetPacketManager::appendReceiveBuffer(NetPacketBase* p_pobjNetPacketBase, char *p_szData, qint32 p_nDataLen)
{
    bool bRet = g_pobjNetProtocolParseBase->parsePacket(p_pobjNetPacketBase, p_szData, p_nDataLen);
    if(!bRet)
    {
        return bRet;
    }

    return true;
}

bool NetPacketManager::prepareResponse(NetPacketBase *p_pobjNetPacketBase, QByteArray &p_bytResponse)
{
    if(g_pobjNetProtocolParseBase)
    {
        return g_pobjNetProtocolParseBase->prepareResponse(p_pobjNetPacketBase, p_bytResponse);
    }
    else
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("g_pobjNetProtocolParseBase is null"));
    }

    return false;
}
