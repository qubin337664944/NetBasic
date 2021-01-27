#include "NetSocketIocpSSLThread.h"
#include "NetSocketIocpSSL.h"
#include "NetPacketManager.h"
#include <QDebug>
#include "NetLog.h"
#include "NetKeepAliveThread.h"

#ifdef WIN32

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

NetSocketIocpSSLThread::NetSocketIocpSSLThread()
{
    m_pobjNetSocketIocp = NULL;
    m_nThreadID = 0;
    m_pobjsslCtx = NULL;
}

void NetSocketIocpSSLThread::init(qint32 p_nThreadID, NetSocketIocpSSL *p_pobjNetSocketIocp, void *p_pobjsslCtx)
{
    m_nThreadID = p_nThreadID;

    m_pobjNetSocketIocp = p_pobjNetSocketIocp;

    m_hIOCompletionPort = m_pobjNetSocketIocp->getIOCompletionPortHandle();

    m_pobjsslCtx = p_pobjsslCtx;
}

void NetSocketIocpSSLThread::run()
{
    OVERLAPPED           *pOverlapped = NULL;
    SOCKET_CONTEXT_SSL   *pSocketContext = NULL;
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

        IO_CONTEXT_SSL* pIoContext = CONTAINING_RECORD(pOverlapped, IO_CONTEXT_SSL, m_Overlapped);

        bool bLockIndex = false;
        quint32 nIndex = pIoContext->m_nIndex;
        if(pIoContext->m_OpType != NET_POST_ACCEPT)
        {
            void* vpobjConText = NULL;
            if(NetKeepAliveThread::lockIndexContext(nIndex, pIoContext->m_sockAccept, pIoContext->m_nSissionID, vpobjConText))
            {
                bLockIndex = true;

                if(vpobjConText == NULL)
                {
                    if(bLockIndex)
                    {
                        NetKeepAliveThread::unlockIndex(nIndex);
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

        if(pIoContext->m_OpType == NET_POST_RECEIVE)
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
                if(pIoContext->m_OpType == NET_POST_ACCEPT)
                {
                    closesocket(pIoContext->m_sockAccept);

                    if(!m_pobjNetSocketIocp->postAccept(pIoContext))
                    {
                         RELEASE( pIoContext );
                    }
                }
                else if(pIoContext->m_OpType == NET_POST_RECEIVE)
                {
                    doDisConnect(pSocketContext, pIoContext);
                }
                else if(pIoContext->m_OpType == NET_POST_SEND)
                {
                    doDisConnect(pSocketContext, pIoContext);
                }
            }

            if(bLockIndex)
            {
                NetKeepAliveThread::unlockIndex(nIndex);
            }

            continue;
        }

        if(bReturn)
        {
            if(pIoContext->m_OpType == NET_POST_ACCEPT)
            {
                IO_CONTEXT_SSL* pNewIoContext = new IO_CONTEXT_SSL;
                if(!m_pobjNetSocketIocp->postAccept(pNewIoContext))
                {
                     RELEASE( pNewIoContext );
                }
            }

            if(0 == dwBytesTransfered)
            {
                NETLOG(NET_LOG_LEVEL_INFO, QString("client disconnect, ip:%1 port:%2 socket:%3 posttype:%4")
                       .arg(inet_ntoa(pSocketContext->m_ClientAddr.sin_addr))
                       .arg(ntohs(pSocketContext->m_ClientAddr.sin_port))
                       .arg(pSocketContext->m_Socket)
                       .arg(pIoContext->m_OpType));

                doDisConnect(pSocketContext, pIoContext);

                if(bLockIndex)
                {
                    NetKeepAliveThread::unlockIndex(nIndex);
                }
                continue;
            }

            bool bRet = false;

            if(pIoContext->m_OpType == NET_POST_ACCEPT)
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

            if(bLockIndex)
            {
                NetKeepAliveThread::unlockIndex(nIndex);
            }
        }
    }
}

bool NetSocketIocpSSLThread::doAccept(SOCKET_CONTEXT_SSL *pSocketContext, IO_CONTEXT_SSL *pIoContext)
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

    SOCKET_CONTEXT_SSL* pNewSocketContext = new SOCKET_CONTEXT_SSL;

    pNewSocketContext->m_pobjSSL = (void*)SSL_new((SSL_CTX*)m_pobjsslCtx);
    if(pNewSocketContext->m_pobjSSL == NULL)
    {
        RELEASE( pNewSocketContext );
        return false;
    }

    SSL_set_accept_state((SSL*)pNewSocketContext->m_pobjSSL);

    pNewSocketContext->m_pobjBioSend = (void*)BIO_new(BIO_s_mem());
    pNewSocketContext->m_pobjBioReceive = (void*)BIO_new(BIO_s_mem());

    SSL_set_bio((SSL*)pNewSocketContext->m_pobjSSL, (BIO*)pNewSocketContext->m_pobjBioReceive, (BIO*)pNewSocketContext->m_pobjBioSend);

    pNewSocketContext->m_Socket = pIoContext->m_sockAccept;
    memcpy(&(pNewSocketContext->m_ClientAddr), ClientAddr, sizeof(SOCKADDR_IN));

    if( false == m_pobjNetSocketIocp->associateWithIOCP( pNewSocketContext ) )
    {
        SSL_shutdown ((SSL*)pNewSocketContext->m_pobjSSL);
        SSL_free((SSL*)pNewSocketContext->m_pobjSSL);

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
    if(!NetKeepAliveThread::addAlive(objNetKeepAliveInfo, nSissionID, nIndex))
    {
        NETLOG(NET_LOG_LEVEL_WORNING, QString("add socket to keep alive failed, socket:%1")
               .arg(pIoContext->m_sockAccept));

        SSL_shutdown ((SSL*)pNewSocketContext->m_pobjSSL);
        SSL_free((SSL*)pNewSocketContext->m_pobjSSL);

        RELEASE( pNewSocketContext );

        return false;
    }
    else
    {
        pNewSocketContext->m_nSissionID = nSissionID;
        pNewSocketContext->m_nIndex = nIndex;
        pIoContext->m_nSissionID = nSissionID;
        pIoContext->m_pobjSSL = pNewSocketContext->m_pobjSSL;
        pIoContext->m_nIndex = nIndex;
    }

    bool bIsLock = false;

    if(!bIsLock)
    {
        void* vpobjConText = NULL;
        if(!NetKeepAliveThread::lockIndexContext(pIoContext->m_nIndex, objNetKeepAliveInfo.nSocket, pIoContext->m_nSissionID, vpobjConText))
        {
            NETLOG(NET_LOG_LEVEL_WORNING, QString("lockIndexContext failed, socket:%1")
                   .arg(objNetKeepAliveInfo.nSocket));

            SSL_shutdown ((SSL*)pNewSocketContext->m_pobjSSL);
            SSL_free((SSL*)pNewSocketContext->m_pobjSSL);
            RELEASE( pNewSocketContext );

            return false;
        }

        bIsLock = true;
    }

    bool bRet = doReceive(pNewSocketContext, pIoContext, bIsLock);
    if(bIsLock)
    {
        NetKeepAliveThread::unlockIndex(nIndex);
    }

    return bRet;
}

bool NetSocketIocpSSLThread::doReceive(SOCKET_CONTEXT_SSL *pSocketContext, IO_CONTEXT_SSL *pIoContext, bool& p_bIsLock)
{
    if(pIoContext->m_pobjNetPacketBase == NULL)
    {
        pIoContext->m_pobjNetPacketBase =  NetPacketManager::allocPacket();
        pIoContext->m_pobjNetPacketBase->m_nSocket = pIoContext->m_sockAccept;
        pIoContext->m_pobjNetPacketBase->m_nSissionID = pIoContext->m_nSissionID;
        pIoContext->m_pobjNetPacketBase->m_pobjSSL = pSocketContext->m_pobjSSL;
        pIoContext->m_pobjNetPacketBase->m_nIndex = pSocketContext->m_nIndex;
    }

    NETLOG(NET_LOG_LEVEL_INFO, QString("doReceive success, Receive size:%1,posttype:%2,socket:%3")
           .arg(pIoContext->m_Overlapped.InternalHigh)
           .arg(pIoContext->m_OpType)
           .arg(pSocketContext->m_Socket));

    int nSSLBytes = 0;
    if(pIoContext->m_Overlapped.InternalHigh > 0)
    {
        int nBioWrite = 0;
        for(;;)
        {
            nSSLBytes = BIO_write((BIO*)pSocketContext->m_pobjBioReceive, pIoContext->m_wsaBuf.buf, pIoContext->m_Overlapped.InternalHigh);
            if(nSSLBytes > 0)
            {
                nBioWrite+=nSSLBytes;
            }

            if(nBioWrite == pIoContext->m_Overlapped.InternalHigh)
            {
                break;
            }

            if(nSSLBytes <= 0)
            {
                return false;
            }
        }
    }

    bool bIsHandShake = pSocketContext->m_bIsHandShake;
    for (;;)
    {
        pIoContext->ResetBuffer();

        nSSLBytes = SSL_read((SSL*)pSocketContext->m_pobjSSL, pIoContext->m_szBuffer, MAX_BUFFER_LEN - 1);

        // 判断一下握手操作是否已完成
        if (!pSocketContext->m_bIsHandShake && SSL_is_init_finished((SSL*)pSocketContext->m_pobjSSL))
        {
            bIsHandShake = true;
        }

        if (nSSLBytes > 0)
        {
            NetPacketManager::appendReceiveBuffer(pIoContext->m_pobjNetPacketBase, pIoContext->m_szBuffer, nSSLBytes);
            if(pIoContext->m_pobjNetPacketBase->m_bIsReceiveEnd)
            {
                if(!NetKeepAliveThread::setCheckReceive(pIoContext->m_sockAccept, pIoContext->m_nSissionID, pIoContext->m_nIndex, false))
                {
                    NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckReceive failed, socket:%1").arg(pIoContext->m_sockAccept));
                    RELEASE(pIoContext);
                    return false;
                }

                NetKeepAliveThread::unlockIndex(pIoContext->m_nIndex);
                p_bIsLock = false;

                NetPacketManager::processCallBack(pIoContext->m_pobjNetPacketBase);
                RELEASE(pIoContext);
                return true;
            }
        }
        else
        {
            break;
        }
    }

    bool bSendIOCP = false;
    if (!pSocketContext->m_bIsHandShake)
    {
        pSocketContext->m_bIsHandShake = bIsHandShake;
        // 循环读取直到读完
        QByteArray bytSend;
        for (;;)
        {
            pIoContext->ResetBuffer();

            nSSLBytes = BIO_read((BIO*)pSocketContext->m_pobjBioSend, pIoContext->m_szBuffer, MAX_BUFFER_LEN - 1);
            if (nSSLBytes > 0)
            {
                bytSend.append(pIoContext->m_szBuffer, nSSLBytes);
                continue;
            }
            else
            {
                break;
            }
        }

        if(bytSend.size() > 0)
        {
            pIoContext->m_OpType = NET_POST_SEND;
            pIoContext->m_wsaBuf.buf = new char[bytSend.size()+1];
            pIoContext->m_wsaBuf.len = bytSend.size();
            pIoContext->m_nSissionID = pSocketContext->m_nSissionID;
            pIoContext->m_pobjSSL = pSocketContext->m_pobjSSL;
            pIoContext->m_sockAccept = pSocketContext->m_Socket;
            pIoContext->m_nIndex = pSocketContext->m_nIndex;

            memcpy(pIoContext->m_wsaBuf.buf, bytSend.data(), bytSend.length());

            pIoContext->m_szSendData = pIoContext->m_wsaBuf.buf;
            pIoContext->m_nSendDataSize = pIoContext->m_wsaBuf.len;
            pIoContext->m_nSendIndex = 0;
            pIoContext->m_bIsCloseConnect = false;

            pSocketContext->appendSendContext(pIoContext);
            bool bRet = m_pobjNetSocketIocp->postSend(pIoContext);
            if(!bRet)
            {
                pSocketContext->cancelSendContext(pIoContext);
                return false;
            }
            else
            {
                bSendIOCP = true;
            }
        }
    }

    if(!bSendIOCP)
    {
        pSocketContext->appendReceiveContext(pIoContext);
        bool bRet = m_pobjNetSocketIocp->postRecv(pIoContext);
        if(!bRet)
        {
            pSocketContext->cancelReceiveContext(pIoContext);
        }

        return bRet;
    }


    return true;
}

bool NetSocketIocpSSLThread::doSend(SOCKET_CONTEXT_SSL *pSocketContext, IO_CONTEXT_SSL *pIoContext, qint32 p_nSendSuccessSize)
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

    if(!NetKeepAliveThread::setCheckSend(pSocketContext->m_Socket, pSocketContext->m_nSissionID, pSocketContext->m_nIndex, false))
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

    if(!NetKeepAliveThread::setCheckReceive(pSocketContext->m_Socket, pSocketContext->m_nSissionID, pSocketContext->m_nIndex, true, 30))
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

bool NetSocketIocpSSLThread::doDisConnect(SOCKET_CONTEXT_SSL *pSocketContext, IO_CONTEXT_SSL *pIoContext)
{
    if(pSocketContext == m_pobjNetSocketIocp->m_pListenContext)
    {
        RELEASE(pIoContext);
        return true;
    }

    if(!pSocketContext->closeContext())
    {
        return false;
    }

    if(NetKeepAliveThread::delAlive(pSocketContext->m_Socket, pSocketContext->m_nSissionID, pSocketContext->m_nIndex))
    {
        if(pSocketContext->m_pobjSSL)
        {
            SSL_shutdown ((SSL*)pSocketContext->m_pobjSSL);
            SSL_free((SSL*)pSocketContext->m_pobjSSL);
            pSocketContext->m_pobjSSL = NULL;
        }

        pSocketContext->closeSocket();

        RELEASE(pIoContext);
        RELEASE(pSocketContext);
    }

    return true;
}

#endif
