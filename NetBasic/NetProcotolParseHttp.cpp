#include "NetProcotolParseHttp.h"
#include "NetPacketHttp.h"
#include "NetEnum.h"
#include "NetLog.h"

#include <QStringList>
#include <QDebug>

NetProcotolParseHttp::NetProcotolParseHttp()
{
}

bool NetProcotolParseHttp::parsePacket(NetPacketBase *p_pobjHttpPacket, char *p_szAppendData, qint32 p_nAppendDataLen)
{
    if(p_pobjHttpPacket == NULL)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("HttpPacket is null"));
        return false;
    }

    NetPacketHttp* pobjHttpPacket = (NetPacketHttp*)p_pobjHttpPacket;

    pobjHttpPacket->m_bytReceiveAllDate.append(p_szAppendData, p_nAppendDataLen);

    if(pobjHttpPacket->m_nStep == NET_PARSE_STEP_HEAD_OK)
    {
        pobjHttpPacket->m_bytData.append(p_szAppendData, p_nAppendDataLen);
        if(pobjHttpPacket->m_bytData.length() >= pobjHttpPacket->m_nContentLength)
        {
            pobjHttpPacket->m_nStep = NET_PARSE_STEP_DATA_OK;
            pobjHttpPacket->m_bIsReceiveEnd = true;
            return true;
        }
        else
        {
            pobjHttpPacket->m_bIsReceiveEnd = false;
            return true;
        }
    }

    if(pobjHttpPacket->m_bytReceiveAllDate.contains("\r\n\r\n") && pobjHttpPacket->m_nStep < NET_PARSE_STEP_HEAD_OK)
    {
        pobjHttpPacket->m_bytHead = pobjHttpPacket->m_bytReceiveAllDate.left(pobjHttpPacket->m_bytReceiveAllDate.indexOf("\r\n\r\n"));
        pobjHttpPacket->m_bytData = pobjHttpPacket->m_bytReceiveAllDate.right(pobjHttpPacket->m_bytReceiveAllDate.length() -
                                                                                     pobjHttpPacket->m_bytReceiveAllDate.indexOf("\r\n\r\n")
                                                                                     - 4);

        QString strAllHead = pobjHttpPacket->m_bytHead;
        QStringList strHeadList = strAllHead.split("\r\n");

        for(int i = 0; i < strHeadList.size(); i++)
        {
           QString strRecvHead  = strHeadList.at(i);
           if(!strRecvHead.contains(":"))
           {
              QStringList strResult = strRecvHead.split(" ");
              if(strResult.size() >= 3)
              {
                 pobjHttpPacket->m_strMethod = strResult.at(0).trimmed();
                 pobjHttpPacket->m_strURL = strResult.at(1).trimmed();
                 pobjHttpPacket->m_strProtocol = strResult.at(2).trimmed();
              }
           }

           QStringList strHead = strRecvHead.split(": ");
           if(strHead.size() == 2)
           {
               pobjHttpPacket->m_mapHttpHead.insert(strHead.at(0).toLower(), strHead.at(1));
           }
        }

        pobjHttpPacket->m_nStep = NET_PARSE_STEP_HEAD_OK;

        if(pobjHttpPacket->m_mapHttpHead.contains("content-length"))
        {
            int nReveiveLength =  pobjHttpPacket->m_mapHttpHead["content-length"].toInt();
            pobjHttpPacket->m_nContentLength = nReveiveLength;
            if(nReveiveLength == 0)
            {
                pobjHttpPacket->m_nStep = NET_PARSE_STEP_DATA_OK;
                pobjHttpPacket->m_bIsReceiveEnd = true;
                return true;
            }

            pobjHttpPacket->m_bytData.reserve(nReveiveLength + 1);
            pobjHttpPacket->m_bytReceiveAllDate.reserve(pobjHttpPacket->m_bytHead.size() + nReveiveLength + 3);

            if(pobjHttpPacket->m_bytData.length() == pobjHttpPacket->m_nContentLength)
            {
                pobjHttpPacket->m_nStep = NET_PARSE_STEP_DATA_OK;
                pobjHttpPacket->m_bIsReceiveEnd = true;
                return true;
            }
        }
        else
        {
            pobjHttpPacket->m_nStep = NET_PARSE_STEP_DATA_OK;
            pobjHttpPacket->m_bIsReceiveEnd = true;
            return true;
        }
    }

    return true;
}

bool NetProcotolParseHttp::prepareResponse(NetPacketBase* p_pobjHttpPacket, QByteArray& p_bytResByte)
{
    if(p_pobjHttpPacket == NULL)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("HttpPacket is null"));
        return false;
    }

    p_bytResByte.clear();

    NetPacketHttp* pobjHttpPacket = (NetPacketHttp*)p_pobjHttpPacket;

    p_bytResByte.append(pobjHttpPacket->m_strProtocol);
    p_bytResByte.append(" ");
    p_bytResByte.append(QString::number(pobjHttpPacket->m_nResultCode));
    p_bytResByte.append(" ");
    p_bytResByte.append(pobjHttpPacket->m_strResultMsg);
    p_bytResByte.append("\r\n");

    QMap<QString, QString>::Iterator itr = pobjHttpPacket->m_mapHttpHead.begin();
    while (itr != pobjHttpPacket->m_mapHttpHead.end())
    {
        QString strHead = QString("%1: %2\r\n").arg(itr.key()).arg(itr.value());
        p_bytResByte.append(strHead);
        itr++;
    }

    if(pobjHttpPacket->m_bytData.size() > 0)
    {
        p_bytResByte.append(QString("Content-Length: %1\r\n\r\n").arg(pobjHttpPacket->m_bytData.size()));
        p_bytResByte.append(pobjHttpPacket->m_bytData);
    }

    return true;
}
