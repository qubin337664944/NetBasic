#ifndef NETSOCKETBASE_H
#define NETSOCKETBASE_H

#include <QCoreApplication>
#include <QString>

#include "NetPacketBase.h"

class NetSocketBase
{
public:
    NetSocketBase();
    virtual ~NetSocketBase(){}

    virtual bool init(const qint32 p_nThreadNum) = 0;
    virtual bool start(const QString& p_strBindIP, const qint32 p_nPort) = 0;
    virtual bool send(NetPacketBase* p_pobjNetPacketBase) = 0;
};

#endif // NETSOCKETBASE_H
