#ifndef NETSOCKETBASE_H
#define NETSOCKETBASE_H

#include <QCoreApplication>
#include <QString>

#include "NetPacketBase.h"
#include "NetPacketManager.h"
#include "NetKeepAliveThread.h"

class NetSocketBase
{
public:
    NetSocketBase();
    virtual ~NetSocketBase(){}

    virtual bool init(const qint32 p_nThreadNum, NetPacketManager* p_pobjNetPacketManager,
                      NetKeepAliveThread* p_pobjNetKeepAliveThread, const QString& p_strKeyPath, const QString& p_strCertPath) = 0;

    virtual bool start(const QString& p_strBindIP, const qint32 p_nPort) = 0;
    virtual bool send(NetPacketBase* p_pobjNetPacketBase) = 0;
};

#endif // NETSOCKETBASE_H
