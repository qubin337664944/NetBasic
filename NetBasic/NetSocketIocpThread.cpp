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

void NetSocketIocpThread::init(qint32 p_nThreadID, NetSocketIocp *p_pobjNetSocketIocp, NetPacketManager* p_pobjNetPacketManager, NetKeepAliveThread *p_pobjNetKeepAliveThread)
{
    m_nThreadID = p_nThreadID;

    m_pobjNetSocketIocp = p_pobjNetSocketIocp;

    m_hIOCompletionPort = m_pobjNetSocketIocp->getIOCompletionPortHandle();

    m_pobjNetPacketManager = p_pobjNetPacketManager;

    m_pobjNetKeepAliveThread = p_pobjNetKeepAliveThread;
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

        DWORD dLastError = GetLastError();

        if ( NULL ==(DWORD)pSocketContext )
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("pSocketContext = null,quit"));
            break;
        }

        IO_CONTEXT* pIoContext = CONTAINING_RECORD(pOverlapped, IO_CONTEXT, m_Overlapped);
        bool bLockIndex = false;
        quint32 nIndex = pIoContext->m_nIndex;
        if(pIoContext->m_OpType != NET_POST_ACCEPT)
        {
            void* vpobjConText = NULL;
            if(m_pobjNetKeepAliveThread->lockIndexContext(nIndex, pIoContext->m_sockAccept, pIoContext->m_nSissionID, vpobjConText))
            {
                bLockIndex = true;

                if(vpobjConText == NULL)
                {
                    if(bLockIndex)
                    {
                        m_pobjNetKeepAliveThread->unlockIndex(nIndex);
                    }

                    RELEASE(pIoContext);
                    continue;
                }
            }
            else
            {
                RELEASE(pIoContext);
                continue;
            }
        }

        if(pIoContext->m_OpType == NET_POST_ACCEPT)
        {
            IO_CONTEXT* pNewIoContext = new IO_CONTEXT;
            if(!m_pobjNetSocketIocp->postAccept(pNewIoContext))
            {
                 RELEASE( pNewIoContext );
            }
        }
        else if(pIoContext->m_OpType == NET_POST_RECEIVE)
        {
            pSocketContext->appendReceiveContext(pIoContext);
        }
        else if(pIoContext->m_OpType == NET_POST_SEND)
        {
            pSocketContext->appendSendContext(pIoContext);
        }

        if( !bReturn )
        {
            NETLOG(NET_LOG_LEVEL_WORNING, QString("client disconnect dwErr, ip:%1 port:%2 socket:%3 posttype:%4 iosocket:%5 error:%6")
                   .arg(inet_ntoa(pSocketContext->m_ClientAddr.sin_addr))
                   .arg(ntohs(pSocketContext->m_ClientAddr.sin_port))
                   .arg(pSocketContext->m_Socket)
                   .arg(pIoContext->m_OpType)
                   .arg(pIoContext->m_sockAccept)
                   .arg(dLastError));

            if(pSocketContext->m_bKeepAliveTimeOut && dLastError == ERROR_CONNECTION_ABORTED)
            {
                RELEASE(pIoContext);
            }

            if((dLastError == WAIT_TIMEOUT) || (dLastError == ERROR_NETNAME_DELETED))//客户端没有正常退出
            {
                doDisConnect(pSocketContext, pIoContext);
            }
        }
        else
        {
            bool bRet = false;
            if(0 == dwBytesTransfered)
            {
                if(pIoContext->m_OpType == NET_POST_ACCEPT)
                {
                    bRet = doAccept(pSocketContext, pIoContext);
                }
                else
                {
                    NETLOG(NET_LOG_LEVEL_INFO, QString("client disconnect, ip:%1 port:%2 socket:%3 posttype:%4")
                           .arg(inet_ntoa(pSocketContext->m_ClientAddr.sin_addr))
                           .arg(ntohs(pSocketContext->m_ClientAddr.sin_port))
                           .arg(pSocketContext->m_Socket)
                           .arg(pIoContext->m_OpType));

                    bRet = false;
                }
            }
            else if(pIoContext->m_OpType == NET_POST_ACCEPT)
            {
                bRet = doAccept(pSocketContext, pIoContext);
            }
            else if(pIoContext->m_OpType == NET_POST_RECEIVE)
            {
                bRet = doReceive(pSocketContext, pIoContext, bLockIndex);
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

        if(bLockIndex)
        {
            m_pobjNetKeepAliveThread->unlockIndex(nIndex);
        }
    }
}

bool NetSocketIocpThread::doAccept(SOCKET_CONTEXT *pSocketContext, IO_CONTEXT *pIoContext)
{
    SOCKADDR_IN* ClientAddr = NULL;
    SOCKADDR_IN* LocalAddr = NULL;
    int remoteLen = sizeof(SOCKADDR_IN);
    int localLen = sizeof(SOCKADDR_IN);

    m_pobjNetSocketIocp->m_lpfnGetAcceptExSockAddrs(pIoContext->m_wsaBuf.buf, 0,
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
        return false;
    }

    NetKeepAliveInfo objNetKeepAliveInfo;
    objNetKeepAliveInfo.nSocket = pIoContext->m_sockAccept;
    objNetKeepAliveInfo.bCheckReceiveTime = true;
    objNetKeepAliveInfo.bCheckSendTime = false;
    objNetKeepAliveInfo.nReceiveTimeOutS = RECEIVE_PACKET_TIMEOUT_S;
    objNetKeepAliveInfo.pobjExtend = pNewSocketContext;
    quint32 nSissionID = 0;
    quint32 nIndex = 0;
    if(!m_pobjNetKeepAliveThread->addAlive(objNetKeepAliveInfo, nSissionID, nIndex))
    {
        NETLOG(NET_LOG_LEVEL_WORNING, QString("add socket to keep alive failed, socket:%1")
               .arg(pIoContext->m_sockAccept));

        RELEASE( pNewSocketContext );

        return false;
    }
    else
    {
        pNewSocketContext->m_nIndex = nIndex;
        pNewSocketContext->m_nSissionID = nSissionID;
        pIoContext->m_nSissionID = nSissionID;
        pIoContext->m_nIndex = nIndex;
    }

    bool bIsLock = false;
    if(!bIsLock)
    {
        void* vpobjConText = NULL;
        if(!m_pobjNetKeepAliveThread->lockIndexContext(pIoContext->m_nIndex, objNetKeepAliveInfo.nSocket, pIoContext->m_nSissionID, vpobjConText))
        {
            NETLOG(NET_LOG_LEVEL_WORNING, QString("lockIndexContext failed, socket:%1")
                   .arg(objNetKeepAliveInfo.nSocket));

            RELEASE( pNewSocketContext );

            return false;
        }

        bIsLock = true;
    }

    bool bRet = doReceive(pNewSocketContext, pIoContext, bIsLock);
    if(bIsLock)
    {
        m_pobjNetKeepAliveThread->unlockIndex(nIndex);
    }

    return bRet;
}

bool NetSocketIocpThread::doReceive(SOCKET_CONTEXT *pSocketContext, IO_CONTEXT *pIoContext, bool& p_bIsLock)
{
    if(pIoContext->m_pobjNetPacketBase == NULL)
    {
        pIoContext->m_pobjNetPacketBase =  m_pobjNetPacketManager->allocPacket();
        pIoContext->m_pobjNetPacketBase->m_nSocket = pIoContext->m_sockAccept;
        pIoContext->m_pobjNetPacketBase->m_nSissionID = pIoContext->m_nSissionID;
        pIoContext->m_pobjNetPacketBase->m_nIndex = pIoContext->m_nIndex;
    }

    if(pIoContext->m_Overlapped.InternalHigh > 0)
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("doReceive success, receive size:%1 socket:%2")
               .arg(pIoContext->m_Overlapped.InternalHigh)
               .arg(pIoContext->m_sockAccept));

        m_pobjNetPacketManager->appendReceiveBuffer(pIoContext->m_pobjNetPacketBase, pIoContext->m_wsaBuf.buf, pIoContext->m_Overlapped.InternalHigh);
        if(pIoContext->m_pobjNetPacketBase->m_bIsReceiveEnd)
        {
            NETLOG(NET_LOG_LEVEL_INFO, QString("receive a packet end, socket:%1").arg(pIoContext->m_sockAccept));

            if(!m_pobjNetKeepAliveThread->setCheckReceive(pIoContext->m_sockAccept, pIoContext->m_nSissionID, pIoContext->m_nIndex, false))
            {
                NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckReceive failed, socket:%1").arg(pIoContext->m_sockAccept));
                delete pIoContext;
                return false;
            }

            m_pobjNetKeepAliveThread->unlockIndex(pIoContext->m_nIndex);
            p_bIsLock = false;

            m_pobjNetPacketManager->processCallBack(pIoContext->m_pobjNetPacketBase);

            delete pIoContext;
            return true;
        }
    }

    pSocketContext->appendReceiveContext(pIoContext);
    bool bRet = m_pobjNetSocketIocp->postRecv(pIoContext);
    if(!bRet)
    {
        pSocketContext->cancelReceiveContext(pIoContext);
    }

    return bRet;
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

        pSocketContext->appendSendContext(pIoContext);
        bool bRet = m_pobjNetSocketIocp->postSend(pIoContext);
        if(!bRet)
        {
            pSocketContext->cancelSendContext(pIoContext);
        }

        return bRet;
    }

    if(!m_pobjNetKeepAliveThread->setCheckSend(pSocketContext->m_Socket, pSocketContext->m_nSissionID, pSocketContext->m_nIndex, false))
    {
        NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckSend failed, socket:%1").arg(pIoContext->m_sockAccept));
        return false;
    }

    if(pIoContext->m_bIsCloseConnect)
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("doSend success, send size:%1,not keep alive, close connect, socket:%2, ip:%3, port:%4, posttype:%5")
               .arg(pIoContext->m_wsaBuf.len)
               .arg(pSocketContext->m_Socket)
               .arg(inet_ntoa(pSocketContext->m_ClientAddr.sin_addr))
               .arg(ntohs(pSocketContext->m_ClientAddr.sin_port))
               .arg(pIoContext->m_OpType));

        return false;
    }

    if(!m_pobjNetKeepAliveThread->setCheckReceive(pSocketContext->m_Socket, pSocketContext->m_nSissionID, pSocketContext->m_nIndex, true, RECEIVE_PACKET_TIMEOUT_S))
    {
        NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckReceive failed, socket:%1").arg(pIoContext->m_sockAccept));
        return false;
    }

    pIoContext->m_OpType = NET_POST_RECEIVE;
    if(pIoContext->m_szSendData)
    {
        delete [] pIoContext->m_szSendData;
        pIoContext->m_szSendData = NULL;
    }
    pIoContext->m_nSendIndex = 0;
    pIoContext->m_nSendDataSize = 0;

    pSocketContext->appendReceiveContext(pIoContext);
    bool bRet = m_pobjNetSocketIocp->postRecv(pIoContext);
    if(!bRet)
    {
        pSocketContext->cancelReceiveContext(pIoContext);
    }

    return bRet;
}

bool NetSocketIocpThread::doDisConnect(SOCKET_CONTEXT *pSocketContext, IO_CONTEXT *pIoContext)
{
    if(pSocketContext == NULL || pIoContext == NULL)
    {
        return false;
    }

    if(pIoContext->m_OpType == NET_POST_ACCEPT)
    {
        closesocket(pIoContext->m_sockAccept);
        RELEASE( pIoContext );
        return true;
    }

    if(pSocketContext == m_pobjNetSocketIocp->m_pListenContext)
    {
        RELEASE(pIoContext);
        return true;
    }

    if(!pSocketContext->closeContext())
    {
        return false;
    }

    if(m_pobjNetKeepAliveThread->delAlive(pSocketContext->m_Socket, pSocketContext->m_nSissionID, pSocketContext->m_nIndex))
    {
        pSocketContext->closeSocket();

        RELEASE(pIoContext);
        RELEASE(pSocketContext);
    }

    return true;
}

#endif
