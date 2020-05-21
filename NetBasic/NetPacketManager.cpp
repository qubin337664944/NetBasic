#include "NetPacketManager.h"
#include "NetEnum.h"
#include "NetProcotolParseHttp.h"
#include "NetPacketHttp.h"
#include "NetLog.h"

QMap<quint64, NetPacketBase*> NetPacketManager::g_mapPacket;
QMutex NetPacketManager::g_objPacketMutex;
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

bool NetPacketManager::appendReceiveBuffer(quint64 p_nSocket, char *p_szData, qint32 p_nDataLen, bool &p_bIsPacketEnd, void* p_pobjSSL)
{
    p_bIsPacketEnd = false;

    g_objPacketMutex.lock();
    NetPacketBase* pobjNetPacketBase = NULL;
    if(g_mapPacket.contains(p_nSocket))
    {
        pobjNetPacketBase = g_mapPacket[p_nSocket];
    }
    else
    {
        if(g_nProtocolType == NET_PROTOCOL_HTTP || g_nProtocolType == NET_PROTOCOL_HTTPS)
        {
            pobjNetPacketBase = new NetPacketHttp;
            pobjNetPacketBase->m_nSocket = p_nSocket;
            pobjNetPacketBase->m_pobjSSL = p_pobjSSL;
            g_mapPacket.insert(p_nSocket, pobjNetPacketBase);
        }
    }
    g_objPacketMutex.unlock();

    bool bRet = g_pobjNetProtocolParseBase->parsePacket(pobjNetPacketBase, p_szData, p_nDataLen);
    if(!bRet)
    {
        g_objPacketMutex.lock();
        g_mapPacket.remove(p_nSocket);
        g_objPacketMutex.unlock();

        delete pobjNetPacketBase;

        return bRet;
    }

    if(bRet && pobjNetPacketBase->m_bIsReceiveEnd)
    {
        p_bIsPacketEnd = true;

        if(g_fnSuccessReceivePacket)
        {
            g_fnSuccessReceivePacket(pobjNetPacketBase, g_pMaster);
        }

        g_objPacketMutex.lock();
        g_mapPacket.remove(p_nSocket);
        g_objPacketMutex.unlock();

        delete pobjNetPacketBase;
    }

    return bRet;
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

bool NetPacketManager::delPacket(quint64 p_nSocket)
{
    g_objPacketMutex.lock();

    if(g_mapPacket.contains(p_nSocket))
    {
       g_mapPacket.remove(p_nSocket);
    }

    g_objPacketMutex.unlock();

    return true;
}
