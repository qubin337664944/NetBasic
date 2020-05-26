#ifndef NETPACKETMANAGER_H
#define NETPACKETMANAGER_H

#include <QMap>
#include <QMutex>

#include "NetPacketBase.h"
#include "NetProtocolParseBase.h"
#include "NetInclude.h"

class NetPacketManager
{
public:
    NetPacketManager();

    static bool init(qint32 p_nProtocolType, CallAppReceivePacket p_fnSuccessReceivePacket, void* p_pMaster);
    static void uninit();

    static bool appendReceiveBuffer(quint64 p_nSocket, char* p_szData, qint32 p_nDataLen, bool& p_bIsPacketEnd, void* p_pobjSSL = NULL);
    static bool processCallBack(NetPacketBase* p_pobjNetPacketBase);

    static NetPacketBase* allocPacket();
    static bool appendReceiveBuffer(NetPacketBase* p_pobjNetPacketBase, char* p_szData, qint32 p_nDataLen);

    static bool prepareResponse(NetPacketBase* p_pobjNetPacketBase, QByteArray& p_bytResponse);
    static bool delPacket(quint64 p_nSocket);


public:
    static QMap<quint64, NetPacketBase*> g_mapPacket;
    static QMutex g_objPacketMutex;
    static NetProtocolParseBase* g_pobjNetProtocolParseBase;
    static CallAppReceivePacket g_fnSuccessReceivePacket;
    static qint32 g_nProtocolType;
    static void* g_pMaster;
};

#endif // NETPACKETMANAGER_H
