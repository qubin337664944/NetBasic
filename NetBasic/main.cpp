#include <QCoreApplication>

#include "NetSocketIocp.h"
#include "NetPacketHttp.h"
#include "NetServerInterface.h"
#include <QDebug>

static void HttpCall(NetPacketBase* p_pobjPacket, void* p_pMaster)
{
    NetPacketHttp* pobjPacketHttp = (NetPacketHttp*)p_pobjPacket;
    qDebug()<< pobjPacketHttp->m_bytData.size();

    NetServerInterface* pobjNetInterface = (NetServerInterface*)p_pMaster;

    NetPacketHttp* pobjResPacket =  new NetPacketHttp;
    pobjResPacket->m_pobjSSL = pobjPacketHttp->m_pobjSSL;
    pobjResPacket->m_bKeepAlive = false;
    pobjResPacket->m_nSocket = pobjPacketHttp->m_nSocket;
    pobjResPacket->m_mapHttpHead.insert("Server", "nginx");
    pobjResPacket->m_mapHttpHead.insert("Connection", "close");
    pobjResPacket->m_mapHttpHead.insert("Content-Type", "text/plain");
    pobjResPacket->m_bytData = "hello world~";

    pobjNetInterface->send(pobjResPacket);

    delete pobjResPacket;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    NetServerInterface objNetServerInterface;
    NetServerInterface::setAppLogCallBack(NET_LOG_LEVEL_TRACE,NULL);
    if(!objNetServerInterface.init(NET_PROTOCOL_HTTP, 1, HttpCall, &objNetServerInterface))
    {
        qDebug()<<"objNetInterface.init error";
    }
    objNetServerInterface.start("0.0.0.0", 80);

    return a.exec();
}
