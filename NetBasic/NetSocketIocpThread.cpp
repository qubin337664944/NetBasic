#include "NetSocketIocpThread.h"
#include "NetSocketIocp.h"
#include "NetPacketManager.h"
#include <QDebug>
#include "NetLog.h"
#include "NetKeepAliveThread.h"

#ifdef WIN32

NetSocketIocpThread::NetSocketIocpThread()
{
    m_pobjNetSocketIocp = NULL;
    m_nThreadID = 0;
}

void NetSocketIocpThread::init(qint32 p_nThreadID, NetSocketIocp *p_pobjNetSocketIocp)
{
    m_nThreadID = p_nThreadID;

    m_pobjNetSocketIocp = p_pobjNetSocketIocp;

    m_hIOCompletionPort = m_pobjNetSocketIocp->getIOCompletionPortHandle();
}

void NetSocketIocpThread::run()
{
    OVERLAPPED           *pOverlapped = NULL;
    SOCKET_CONTEXT      *pSocketContext = NULL;
    DWORD                dwBytesTransfered = 0;

    while(1)
    {
        bool bReturn = GetQueuedCompletionStatus(
            m_hIOCompletionPort,
            &dwBytesTransfered,
            (PULONG_PTR)&pSocketContext,
            &pOverlapped,
            INFINITE);

        if ( NULL ==(DWORD)pSocketContext )
        {
            break;
        }

        IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, IO_CONTEXT, m_Overlapped);

        if( !bReturn )
        {
            DWORD dwErr = GetLastError();
            if(ERROR_NETNAME_DELETED == dwErr)
            {
                NETLOG(NET_LOG_LEVEL_INFO, QString("client disconnect dwErr, ip:%1 port:%2 socket:%3 posttype:%4")
                       .arg(inet_ntoa(pSocketContext->m_ClientAddr.sin_addr))
                       .arg(ntohs(pSocketContext->m_ClientAddr.sin_port))
                       .arg(pSocketContext->m_Socket)
                       .arg(pIoContext->m_OpType));

                doDisConnect(pSocketContext, pIoContext);
                continue;
            }

            if(WAIT_TIMEOUT == dwErr)
            {
                continue;
            }
        }

        if(bReturn)
        {
            if(0 == dwBytesTransfered)
            {
                NETLOG(NET_LOG_LEVEL_INFO, QString("client disconnect, ip:%1 port:%2 socket:%3 posttype:%4")
                       .arg(inet_ntoa(pSocketContext->m_ClientAddr.sin_addr))
                       .arg(ntohs(pSocketContext->m_ClientAddr.sin_port))
                       .arg(pSocketContext->m_Socket)
                       .arg(pIoContext->m_OpType));

                doDisConnect(pSocketContext, pIoContext);

                continue;
            }

            bool bRet = false;

            if(pIoContext->m_OpType == NET_POST_ACCEPT)
            {
                bRet = doAccept(pSocketContext, pIoContext);
            }
            else if(pIoContext->m_OpType == NET_POST_RECEIVE)
            {
                bRet = doReceive(pSocketContext, pIoContext);
            }
            else if(pIoContext->m_OpType == NET_POST_SEND)
            {
                bRet = doSend(pSocketContext, pIoContext, dwBytesTransfered);
            }

            if(!bRet)
            {
                doDisConnect(pSocketContext, pIoContext);
            }
        }
    }
}

bool NetSocketIocpThread::doAccept(SOCKET_CONTEXT *pSocketContext, IO_CONTEXT *pIoContext)
{
    SOCKADDR_IN* ClientAddr = NULL;
    SOCKADDR_IN* LocalAddr = NULL;
    int remoteLen = sizeof(SOCKADDR_IN);
    int localLen = sizeof(SOCKADDR_IN);

    m_pobjNetSocketIocp->m_lpfnGetAcceptExSockAddrs(pIoContext->m_wsaBuf.buf, pIoContext->m_wsaBuf.len - ((sizeof(SOCKADDR_IN)+16)*2),
        sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);

    NETLOG(NET_LOG_LEVEL_INFO, QString("new client connected, ip:%1 port:%2 socket:%3")
           .arg(inet_ntoa(ClientAddr->sin_addr))
           .arg(ntohs(ClientAddr->sin_port))
           .arg(pIoContext->m_sockAccept));

    SOCKET_CONTEXT* pNewSocketContext = new SOCKET_CONTEXT;
    pNewSocketContext->m_Socket = pIoContext->m_sockAccept;
    memcpy(&(pNewSocketContext->m_ClientAddr), ClientAddr, sizeof(SOCKADDR_IN));

    if( false == m_pobjNetSocketIocp->associateWithIOCP( pNewSocketContext ) )
    {
        RELEASE( pNewSocketContext );

        pIoContext->ResetBuffer();
        return m_pobjNetSocketIocp->postAccept(pIoContext);
    }

    NetKeepAliveInfo objNetKeepAliveInfo;
    objNetKeepAliveInfo.nSocket = pIoContext->m_sockAccept;
    objNetKeepAliveInfo.bCheckReceiveTime = true;
    objNetKeepAliveInfo.bCheckSendTime = false;
    objNetKeepAliveInfo.nReceiveTimeOutS = RECEIVE_PACKET_TIMEOUT_S;
    if(!NetKeepAliveThread::addAlive(objNetKeepAliveInfo))
    {
        NETLOG(NET_LOG_LEVEL_WORNING, QString("add socket to keep alive failed, socket:%1")
               .arg(pIoContext->m_sockAccept));
    }

    if(pIoContext->m_Overlapped.InternalHigh > 0)
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("accept socket, receive size:%1 socket:%2")
               .arg(pIoContext->m_Overlapped.InternalHigh)
               .arg(pIoContext->m_sockAccept));

        if(pIoContext->m_pobjNetPacketBase == NULL)
        {
            pIoContext->m_pobjNetPacketBase =  NetPacketManager::allocPacket();
            pIoContext->m_pobjNetPacketBase->m_nSocket = pIoContext->m_sockAccept;
        }

        NetPacketManager::appendReceiveBuffer(pIoContext->m_pobjNetPacketBase, pIoContext->m_wsaBuf.buf, pIoContext->m_Overlapped.InternalHigh);
        if(pIoContext->m_pobjNetPacketBase->m_bIsReceiveEnd)
        {
            if(!NetKeepAliveThread::setCheckReceive(pIoContext->m_sockAccept, false))
            {
                NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckReceive failed, socket:%1").arg(pIoContext->m_sockAccept));
            }

            NetPacketManager::processCallBack(pIoContext->m_pobjNetPacketBase);

            pIoContext->ResetBuffer();

            delete pIoContext->m_pobjNetPacketBase;
            pIoContext->m_pobjNetPacketBase = NULL;
            return m_pobjNetSocketIocp->postAccept(pIoContext);
        }
    }

    IO_CONTEXT* pNewIoContext = new IO_CONTEXT;
    pNewIoContext->m_OpType = NET_POST_RECEIVE;
    pNewIoContext->m_sockAccept = pNewSocketContext->m_Socket;
    if( false == m_pobjNetSocketIocp->postRecv(pNewIoContext) )
    {
        RELEASE(pNewIoContext);
        return false;
    }

    pIoContext->ResetBuffer();

    return m_pobjNetSocketIocp->postAccept(pIoContext);
}

bool NetSocketIocpThread::doReceive(SOCKET_CONTEXT *pSocketContext, IO_CONTEXT *pIoContext)
{
    if(pIoContext->m_pobjNetPacketBase == NULL)
    {
        pIoContext->m_pobjNetPacketBase =  NetPacketManager::allocPacket();
        pIoContext->m_pobjNetPacketBase->m_nSocket = pIoContext->m_sockAccept;
    }

    if(pIoContext->m_Overlapped.InternalHigh > 0)
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("doReceive success, receive size:%1 socket:%2")
               .arg(pIoContext->m_Overlapped.InternalHigh)
               .arg(pIoContext->m_sockAccept));

        NetPacketManager::appendReceiveBuffer(pIoContext->m_pobjNetPacketBase, pIoContext->m_wsaBuf.buf, pIoContext->m_Overlapped.InternalHigh);
        if(pIoContext->m_pobjNetPacketBase->m_bIsReceiveEnd)
        {
            NETLOG(NET_LOG_LEVEL_INFO, QString("receive a packet end, socket:%1").arg(pIoContext->m_sockAccept));

            if(!NetKeepAliveThread::setCheckReceive(pIoContext->m_sockAccept, false))
            {
                NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckReceive failed, socket:%1").arg(pIoContext->m_sockAccept));
            }

            NetPacketManager::processCallBack(pIoContext->m_pobjNetPacketBase);

            delete pIoContext;
            return true;
        }
    }

    return m_pobjNetSocketIocp->postRecv(pIoContext);
}

bool NetSocketIocpThread::doSend(SOCKET_CONTEXT *pSocketContext, IO_CONTEXT *pIoContext, qint32 p_nSendSuccessSize)
{
    NETLOG(NET_LOG_LEVEL_INFO, QString("doSend success, send size:%1,posttype:%2,socket:%3")
           .arg(p_nSendSuccessSize)
           .arg(pIoContext->m_OpType)
           .arg(pSocketContext->m_Socket));

    pIoContext->m_nSendIndex += p_nSendSuccessSize;

    if(pIoContext->m_nSendIndex != pIoContext->m_nSendDataSize)
    {
        pIoContext->m_wsaBuf.buf = pIoContext->m_szSendData + pIoContext->m_nSendIndex;
        pIoContext->m_wsaBuf.len = pIoContext->m_nSendDataSize - pIoContext->m_nSendIndex;

        return m_pobjNetSocketIocp->postSend(pIoContext);
    }

    if(!NetKeepAliveThread::setCheckSend(pSocketContext->m_Socket, false))
    {
        NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckSend failed, socket:%1").arg(pIoContext->m_sockAccept));
    }

    if(pIoContext->m_bIsCloseConnect)
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("doSend success, send size:%1,not keep alive, close connect, socket:%2, ip:%3, port:%4, posttype:%5")
               .arg(pIoContext->m_wsaBuf.len)
               .arg(pSocketContext->m_Socket)
               .arg(inet_ntoa(pSocketContext->m_ClientAddr.sin_addr))
               .arg(ntohs(pSocketContext->m_ClientAddr.sin_port))
               .arg(pIoContext->m_OpType));

        doDisConnect(pSocketContext, pIoContext);
        return true;
    }

    if(!NetKeepAliveThread::setCheckReceive(pSocketContext->m_Socket, true, 30))
    {
        NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckReceive failed, socket:%1").arg(pIoContext->m_sockAccept));
    }

    pIoContext->m_OpType = NET_POST_RECEIVE;
    if(pIoContext->m_szSendData)
    {
        delete [] pIoContext->m_szSendData;
        pIoContext->m_szSendData = NULL;
    }
    pIoContext->m_nSendIndex = 0;
    pIoContext->m_nSendDataSize = 0;

    return m_pobjNetSocketIocp->postRecv(pIoContext);
}

bool NetSocketIocpThread::doDisConnect(SOCKET_CONTEXT *pSocketContext, IO_CONTEXT *pIoContext)
{
    NetPacketManager::delPacket(pSocketContext->m_Socket);

    NetKeepAliveThread::delAlive(pSocketContext->m_Socket);

    closesocket(pSocketContext->m_Socket);
    RELEASE(pSocketContext);
    RELEASE(pIoContext);

    return true;
}

#endif
