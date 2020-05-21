#ifndef NETPROTOCOLPARSEBASE_H
#define NETPROTOCOLPARSEBASE_H

#include <QByteArray>

class NetPacketBase;
class NetProtocolParseBase
{
public:
    NetProtocolParseBase();
    virtual ~NetProtocolParseBase(){}

    virtual bool parsePacket(NetPacketBase* p_pobjHttpPacket, char* p_szAppendData, qint32 p_nAppendDataLen) = 0;
    virtual bool prepareResponse(NetPacketBase* p_pobjHttpPacket, QByteArray& p_bytResByte) = 0;
};

#endif // NETPROTOCOLPARSEBASE_H
