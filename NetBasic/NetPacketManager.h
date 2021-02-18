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
    ~NetPacketManager();

    bool init(qint32 p_nProtocolType, CallAppReceivePacket p_fnSuccessReceivePacket, void* p_pMaster);
    void uninit();

    bool processCallBack(NetPacketBase* p_pobjNetPacketBase);

    NetPacketBase* allocPacket();
    bool appendReceiveBuffer(NetPacketBase* p_pobjNetPacketBase, char* p_szData, qint32 p_nDataLen);

    bool prepareResponse(NetPacketBase* p_pobjNetPacketBase, QByteArray& p_bytResponse);

private:
    NetProtocolParseBase* m_pobjNetProtocolParseBase;
    CallAppReceivePacket m_fnSuccessReceivePacket;
    qint32 m_nProtocolType;
    void* m_pMaster;
};

#endif // NETPACKETMANAGER_H
