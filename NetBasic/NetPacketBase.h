#ifndef NETPACKETBASE_H
#define NETPACKETBASE_H

#include <QByteArray>

class NetPacketBase
{
public:
    NetPacketBase();
    virtual ~NetPacketBase();

public:
    bool m_bKeepAlive;
    qint32 m_nPacketID;
    QByteArray m_bytReceiveAllDate;
    bool m_bIsReceiveEnd;
    quint64 m_nSocket;
    quint32 m_nSissionID;
    quint32 m_nIndex;

    qint32 m_nStep;

    QByteArray m_bytSendAllDate;
    qint32 m_nSendIndex;

    void* m_pobjSSL;
};

#endif // NETPACKETBASE_H
