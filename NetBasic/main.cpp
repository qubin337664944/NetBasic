#include <QCoreApplication>

#include "NetSocketIocp.h"
#include "NetPacketHttp.h"
#include "NetServerInterface.h"
#include <QDebug>
#include <QFile>

static void HttpCall(NetPacketBase* p_pobjPacket, void* p_pMaster)
{
    NetPacketHttp* pobjPacketHttp = (NetPacketHttp*)p_pobjPacket;
    qDebug()<< pobjPacketHttp->m_bytData.size();

    NetServerInterface* pobjNetInterface = (NetServerInterface*)p_pMaster;

    NetPacketHttp* pobjResPacket =  new NetPacketHttp;
    pobjResPacket->m_pobjSSL = pobjPacketHttp->m_pobjSSL;
    pobjResPacket->m_bKeepAlive = true;
    pobjResPacket->m_nSocket = pobjPacketHttp->m_nSocket;
    pobjResPacket->m_nSissionID = pobjPacketHttp->m_nSissionID;
    pobjResPacket->m_nIndex = pobjPacketHttp->m_nIndex;

    pobjResPacket->m_mapHttpHead.insert("Server", "nginx");
    pobjResPacket->m_mapHttpHead.insert("Connection", "keep-alive");
    pobjResPacket->m_mapHttpHead.insert("Content-Type", "text/plain");

    QByteArray bytReturn(9,'a');
    pobjResPacket->m_bytData = bytReturn;

    pobjNetInterface->send(pobjResPacket);

    delete pobjResPacket;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    NetServerInterface objNetServerInterface;
    NetServerInterface::setAppLogCallBack(NET_LOG_LEVEL_ERROR,NULL);
    //NetServerInterface::setSslKeyCertPath("G:\\1122\\server.key", "G:\\1122\\server.crt");
    if(!objNetServerInterface.init(NET_PROTOCOL_HTTPS, 10, HttpCall, &objNetServerInterface))
    {
        qDebug()<<"objNetInterface.init error";
        return a.exec();
    }

    if(!objNetServerInterface.start("0.0.0.0", 443))
    {
        qDebug()<<"objNetInterface.start error";
        return a.exec();
    }

    return a.exec();
}
