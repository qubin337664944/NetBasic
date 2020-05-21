#ifndef NETPROCOTOLPARSEHTTP_H
#define NETPROCOTOLPARSEHTTP_H

#include "NetProtocolParseBase.h"
#include "NetPacketBase.h"

class NetProtocolParseBase;
class NetProcotolParseHttp : public NetProtocolParseBase
{
public:
    NetProcotolParseHttp();

    virtual bool parsePacket(NetPacketBase* p_pobjHttpPacket, char* p_szAppendData, qint32 p_nAppendDataLen);
    virtual bool prepareResponse(NetPacketBase* p_pobjHttpPacket, QByteArray& p_bytResByte);
};

#endif // NETPROCOTOLPARSEHTTP_H
