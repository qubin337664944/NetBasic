#ifndef NETSERVERINTERFACE_H
#define NETSERVERINTERFACE_H

#include <QCoreApplication>

#include "NetInclude.h"
#include "NetEnum.h"
#include "NetSocketBase.h"
#include "NetPacketBase.h"

class NetServerInterface
{
public:
    NetServerInterface();
    ~NetServerInterface();

    static void setAppLogCallBack(const qint32 p_nLogLevel, CallAppLog p_fnApplog);

    static void setSslKetCertPath(const QString& p_strKeyPath, const QString& p_strCertPath);

    bool init(const qint32 p_nProtocol, const qint32 p_nThreadNum, CallAppReceivePacket p_fnAppReceivePacket, void* p_pMaster);

    bool start(const QString& p_strBindIP, const qint32 p_nPort);

    bool send(NetPacketBase* pobjNetPacketBase);

    void uninit();

private:
    NetSocketBase *m_pobjSocketBase;
};

#endif // NETINTERFACE_H