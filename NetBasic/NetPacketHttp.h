#ifndef NETPACKETHTTP_H
#define NETPACKETHTTP_H

#include <QMap>
#include <QString>

#include "NetPacketBase.h"

class NetPacketHttp : public NetPacketBase
{
public:
    NetPacketHttp();
    virtual ~NetPacketHttp();


public:
    QString m_strMethod;
    QString m_strURL;
    QString m_strProtocol;

    QMap<QString, QString> m_mapHttpHead;

    QByteArray m_bytHead;
    QByteArray m_bytData;

    qint32 m_nContentLength;

    qint32 m_nResultCode;
    QString m_strResultMsg;
};

#endif // NETPACKETHTTP_H
