#include <QCoreApplication>

#include "NetSocketIocp.h"
#include "NetPacketHttp.h"
#include "NetServerInterface.h"
#include <QDebug>
#include <QFile>

#include <QMutex>
static qint64 g_nCount = 0;
static QMutex g_Mutex;

class TestRes : public QThread
{
    void run()
    {
        sleep(1);

        m_pobjResPacket->m_mapHttpHead.insert("Server", "nginx");
        m_pobjResPacket->m_mapHttpHead.insert("Connection", "keep-alive");
        m_pobjResPacket->m_mapHttpHead.insert("Content-Type", "text/plain");

        QByteArray bytReturn("aaaaaa");
        m_pobjResPacket->m_bytData = bytReturn;

        m_pobjNetInterface->send(m_pobjResPacket);
        delete m_pobjResPacket;
    }

public:
    NetServerInterface* m_pobjNetInterface;
    NetPacketHttp* m_pobjResPacket;
};

static void HttpCallThread(NetPacketBase* p_pobjPacket, void* p_pMaster)
{
    NetPacketHttp* pobjPacketHttp = (NetPacketHttp*)p_pobjPacket;
    //qDebug()<<pobjPacketHttp->m_bytReceiveAllDate.data();
    //qDebug()<< pobjPacketHttp->m_bytData.size();

    NetServerInterface* pobjNetInterface = (NetServerInterface*)p_pMaster;

    NetPacketHttp* pobjResPacket =  new NetPacketHttp;

    pobjResPacket->m_bKeepAlive = false;
    pobjResPacket->m_nTimeOutS = 50;
    pobjPacketHttp->copyConnectInfo(pobjResPacket);

    TestRes* pRes = new TestRes;
    pRes->m_pobjNetInterface = pobjNetInterface;
    pRes->m_pobjResPacket = pobjResPacket;

    pRes->start();
}

static void HttpCall(NetPacketBase* p_pobjPacket, void* p_pMaster)
{
    NetPacketHttp* pobjPacketHttp = (NetPacketHttp*)p_pobjPacket;
    qDebug()<<pobjPacketHttp->m_bytReceiveAllDate.data();
    //qDebug()<< pobjPacketHttp->m_bytData.size();

    NetServerInterface* pobjNetInterface = (NetServerInterface*)p_pMaster;

    NetPacketHttp* pobjResPacket =  new NetPacketHttp;

    pobjResPacket->m_bKeepAlive = true;
    pobjResPacket->m_nTimeOutS = 50;
    pobjPacketHttp->copyConnectInfo(pobjResPacket);

    pobjResPacket->m_mapHttpHead.insert("Server", "nginx");
    pobjResPacket->m_mapHttpHead.insert("Connection", "keep-alive");
    pobjResPacket->m_mapHttpHead.insert("Content-Type", "text/plain");

    QByteArray bytReturn(9, 'a');
    pobjResPacket->m_bytData = bytReturn;


    pobjNetInterface->send(pobjResPacket);

    {
        //g_Mutex.lock();
        g_nCount++;
        //g_Mutex.unlock();
    }

    delete pobjResPacket;
}

class TestCount : public QThread
{
    void run()
    {
        while(1)
        {
            qDebug() << "g_nCount:"<<g_nCount;
            sleep(1);
        }
    }
};

//http
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    NetServerInterface objNetServerInterface;
    NetServerInterface::setAppLogCallBack(NET_LOG_LEVEL_ERROR,NULL);
    NetServerInterface::setSslKeyCertPath("G:\\1122\\server.key", "G:\\1122\\server.crt");
    if(!objNetServerInterface.init(NET_PROTOCOL_HTTPS, 4, HttpCall, &objNetServerInterface))
    {
        qDebug()<<"objNetInterface.init error";
        return a.exec();
    }

    if(!objNetServerInterface.start("0.0.0.0", 443))
    {
        qDebug()<<"objNetInterface.start error";
        return a.exec();
    }

    TestCount objTestCount;
    objTestCount.start();

    return a.exec();
}

//https
//int main(int argc, char *argv[])
//{
//    QCoreApplication a(argc, argv);

//    NetServerInterface objNetServerInterface;
//    NetServerInterface::setAppLogCallBack(NET_LOG_LEVEL_ERROR,NULL);
//    NetServerInterface::setSslKeyCertPath("G:\\1122\\server.key", "G:\\1122\\server.crt");
//    if(!objNetServerInterface.init(NET_PROTOCOL_HTTPS, 4, HttpCall, &objNetServerInterface))
//    {
//        qDebug()<<"objNetInterface.init error";
//        return a.exec();
//    }

//    if(!objNetServerInterface.start("0.0.0.0", 443))
//    {
//        qDebug()<<"objNetInterface.start error";
//        return a.exec();
//    }

//    TestCount objTestCount;
//    objTestCount.start();

//    return a.exec();
//}
