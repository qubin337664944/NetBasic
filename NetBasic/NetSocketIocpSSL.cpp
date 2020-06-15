#include "NetSocketIocpSSL.h"
#include "NetSocketIocpSSLThread.h"
#include "NetPacketManager.h"
#include <QDebug>
#include "NetLog.h"
#include "NetKeepAliveThread.h"

#ifdef WIN32

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

QString NetSocketIocpSSL::g_strKeyPath = "ssl.key";
QString NetSocketIocpSSL::g_strCertPath = "ssl.crt";

NetSocketIocpSSL::NetSocketIocpSSL()
{
}

NetSocketIocpSSL::~NetSocketIocpSSL()
{
    for (int i = 0; i < m_vecNetSocketIocpThread.size(); i++)
    {
        // 通知所有的完成端口操作退出
        PostQueuedCompletionStatus(m_hIOCompletionPort, 0, (DWORD)NULL, NULL);
    }

    // 关闭IOCP句柄
    RELEASE_HANDLE(m_hIOCompletionPort);

    // 关闭监听Socket
    RELEASE(m_pListenContext);
}

bool NetSocketIocpSSL::init(const qint32 p_nThreadNum)
{
    WSADATA wsaData;
    int nResult;
    nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (NO_ERROR != nResult)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("WSAStartup 2.2 failed, errorcode:%1").arg(WSAGetLastError()));
        return false;
    }

    m_hIOCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );

    if ( NULL == m_hIOCompletionPort)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("CreateIoCompletionPort failed, errorcode:%1").arg(WSAGetLastError()));
        return false;
    }

    SSL_load_error_strings ();
    int r = SSL_library_init ();
    if(!r)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("SSL_library_init failed,error:%1").arg(strerror(errno)));
        return false;
    }

    OpenSSL_add_ssl_algorithms();//对SSL进行初始化

    m_pobjsslCtx = (void*)SSL_CTX_new (SSLv23_method ());
    if(m_pobjsslCtx == NULL)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("SSL_CTX_new = null,error:%1").arg(strerror(errno)));
        return false;
    }

    m_pobjerrBio = (void*)BIO_new_fd(2, BIO_NOCLOSE);

    r = SSL_CTX_use_certificate_file((SSL_CTX*)m_pobjsslCtx, g_strCertPath.toStdString().c_str(), SSL_FILETYPE_PEM);
    if(r <= 0)
    {
        unsigned long ulErr = ERR_get_error(); // 获取错误号
        char szErrMsg[1024] = { 0 };
        char *pTmp = NULL;
        ERR_load_crypto_strings();
        pTmp = ERR_error_string(ulErr, szErrMsg); // 格式：error:errId:库:函数:原因
        const char* re = ERR_reason_error_string(ulErr);
        if (re != NULL){
            qDebug()<<  szErrMsg;
        }

        NETLOG(NET_LOG_LEVEL_ERROR, QString("SSL_CTX_use_certificate_file <= 0,error:%1,cert file:%2").arg(strerror(errno)).arg(g_strCertPath));
        return false;
    }

    r = SSL_CTX_use_PrivateKey_file((SSL_CTX*)m_pobjsslCtx, g_strKeyPath.toStdString().c_str(), SSL_FILETYPE_PEM);
    if(r <= 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("SSL_CTX_use_PrivateKey_file <= 0,error:%1,key file:%2").arg(strerror(errno)).arg(g_strKeyPath));
        return false;
    }

    r = SSL_CTX_check_private_key((SSL_CTX*)m_pobjsslCtx);
    if(!r)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("SSL_CTX_check_private_key failed,error:%1").arg(strerror(errno)));
        return false;
    }

//    //设置加密列表
//    r = SSL_CTX_set_cipher_list((SSL_CTX*)m_pobjsslCtx,"RC4-MD5");
//    if(r <= 0)
//    {
//        NETLOG(NET_LOG_LEVEL_ERROR, QString("SSL_CTX_set_cipher_list failed,error:%1").arg(strerror(errno)));
//        return false;
//    }

    for(int i = 0; i < p_nThreadNum; i++)
    {
        NetSocketIocpSSLThread* pobjNetSocketIocpThread = new NetSocketIocpSSLThread;
        pobjNetSocketIocpThread->init(i, this, m_pobjsslCtx);
        pobjNetSocketIocpThread->start();
        m_vecNetSocketIocpThread.append(pobjNetSocketIocpThread);
    }

    return true;
}

bool NetSocketIocpSSL::start(const QString &p_strBindIP, const qint32 p_nPort)
{
    if(p_nPort<0 || p_nPort>65535)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("port is out of limit 0-65535,port:%1").arg(p_nPort));
        return false;
    }

    GUID GuidAcceptEx = WSAID_ACCEPTEX;
    GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;

    struct sockaddr_in ServerAddress;

    m_pListenContext = new SOCKET_CONTEXT_SSL;

    m_pListenContext->m_Socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET == m_pListenContext->m_Socket)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("WSASocket INVALID_SOCKET, errorcode:%1").arg(WSAGetLastError()));
        RELEASE(m_pListenContext);
        return false;
    }
    else
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("WSASocket success, errorcode:%1").arg(WSAGetLastError()));
    }

    if( NULL== CreateIoCompletionPort( (HANDLE)m_pListenContext->m_Socket, m_hIOCompletionPort,(DWORD)m_pListenContext, 0))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("CreateIoCompletionPort failed, errorcode:%1").arg(WSAGetLastError()));
        RELEASE_SOCKET( m_pListenContext->m_Socket );
        RELEASE(m_pListenContext);
        return false;
    }
    else
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("Listen Socket CreateIoCompletionPort success"));
    }

    ZeroMemory((char *)&ServerAddress, sizeof(ServerAddress));
    ServerAddress.sin_family = AF_INET;
    ServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    ServerAddress.sin_port = htons(p_nPort);

    if (SOCKET_ERROR == bind(m_pListenContext->m_Socket, (struct sockaddr *) &ServerAddress, sizeof(ServerAddress)))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("bind port %1 failed, errorcode:%2").arg(p_nPort).arg(WSAGetLastError()));
        RELEASE_SOCKET( m_pListenContext->m_Socket );
        RELEASE(m_pListenContext);
        return false;
    }
    else
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("bind port %1 success").arg(p_nPort));
    }

    if (SOCKET_ERROR == listen(m_pListenContext->m_Socket,LISTEN_SIZE))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("Listen failed, errorcode:%1").arg(WSAGetLastError()));
        RELEASE_SOCKET( m_pListenContext->m_Socket );
        RELEASE(m_pListenContext);
        return false;
    }
    else
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("Listen success"));
    }

    DWORD dwBytes = 0;
    if(SOCKET_ERROR == WSAIoctl(
        m_pListenContext->m_Socket,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &GuidAcceptEx,
        sizeof(GuidAcceptEx),
        &m_lpfnAcceptEx,
        sizeof(m_lpfnAcceptEx),
        &dwBytes,
        NULL,
        NULL))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("WSAIoctl AcceptEx failed, errorcode:%1").arg(WSAGetLastError()));
        RELEASE_SOCKET( m_pListenContext->m_Socket );
        RELEASE(m_pListenContext);
        return false;
    }

    if(SOCKET_ERROR == WSAIoctl(
        m_pListenContext->m_Socket,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &GuidGetAcceptExSockAddrs,
        sizeof(GuidGetAcceptExSockAddrs),
        &m_lpfnGetAcceptExSockAddrs,
        sizeof(m_lpfnGetAcceptExSockAddrs),
        &dwBytes,
        NULL,
        NULL))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("WSAIoctl GuidGetAcceptExSockAddrs failed, errorcode:%1").arg(WSAGetLastError()));
        RELEASE_SOCKET( m_pListenContext->m_Socket );
        RELEASE(m_pListenContext);
        return false;
    }

    for( int i = 0; i < LISTEN_SIZE; i++ )
    {
        IO_CONTEXT_SSL* pAcceptIoContext = new IO_CONTEXT_SSL;
        if( false==this->postAccept( pAcceptIoContext) )
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("postAccept error, errorcode:%1").arg(WSAGetLastError()));
            RELEASE(pAcceptIoContext);
            RELEASE_SOCKET( m_pListenContext->m_Socket );
            RELEASE(m_pListenContext);
            return false;
        }
    }

    return true;
}

bool NetSocketIocpSSL::send(NetPacketBase *p_pobjNetPacketBase)
{
    QByteArray bytSend;

    if(!NetPacketManager::prepareResponse(p_pobjNetPacketBase, bytSend))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("prepareResponse failed"));
        return false;
    }

    int nSendAllLength = 0;
    while(1)
    {
        int nSendLen = SSL_write((SSL*)p_pobjNetPacketBase->m_pobjSSL, bytSend.data() + nSendAllLength, bytSend.length() - nSendAllLength);
        if(nSendLen > 0)
        {
            nSendAllLength += nSendLen;
        }

        if(nSendAllLength == bytSend.length())
        {
            break;
        }

        if(nSendLen <= 0)
        {
            return false;
        }
    }

    IO_CONTEXT_SSL* pobjIoContext = new IO_CONTEXT_SSL;

    void* pobjExtend = NULL;
    if(!NetKeepAliveThread::getExtend(p_pobjNetPacketBase->m_nSocket, p_pobjNetPacketBase->m_nSissionID, pobjExtend))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("getExtend failed,socket:%1,sissionId:%2").arg(p_pobjNetPacketBase->m_nSocket).arg(p_pobjNetPacketBase->m_nSissionID));
        return false;
    }

    if(pobjExtend == NULL)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("getExtend failed,pobjSocketContext = null,socket:%1,sissionId:%2").arg(p_pobjNetPacketBase->m_nSocket).arg(p_pobjNetPacketBase->m_nSissionID));
        return false;
    }

    SOCKET_CONTEXT_SSL* pobjSocketContext = (SOCKET_CONTEXT_SSL*)pobjExtend;

    QByteArray bytSSLSend;
    bytSSLSend.reserve(bytSend.length() + 1);
    while(1)
    {
        int nSSLBytes = BIO_read((BIO*)pobjSocketContext->m_pobjBioSend, pobjIoContext->m_szBuffer, MAX_BUFFER_LEN - 1);
        if(nSSLBytes > 0)
        {
            bytSSLSend.append(pobjIoContext->m_szBuffer, nSSLBytes);
        }
        else
        {
            break;
        }
    }

    pobjIoContext->m_wsaBuf.buf = new char[bytSSLSend.size()+1];
    pobjIoContext->m_wsaBuf.len = bytSSLSend.size();
    memcpy(pobjIoContext->m_wsaBuf.buf, bytSSLSend.data(), bytSSLSend.length());
    pobjIoContext->m_sockAccept = p_pobjNetPacketBase->m_nSocket;
    pobjIoContext->m_nSissionID = p_pobjNetPacketBase->m_nSissionID;
    pobjIoContext->m_pobjSSL = p_pobjNetPacketBase->m_pobjSSL;

    if(p_pobjNetPacketBase->m_bKeepAlive)
    {
        pobjIoContext->m_bIsCloseConnect = false;
    }
    else
    {
        pobjIoContext->m_bIsCloseConnect = true;
    }

    pobjIoContext->m_OpType = NET_POST_SEND;

    pobjIoContext->m_szSendData = pobjIoContext->m_wsaBuf.buf;
    pobjIoContext->m_nSendDataSize = pobjIoContext->m_wsaBuf.len;
    pobjIoContext->m_nSendIndex = 0;


    bool bRet = postSend(pobjIoContext);
    if(!bRet)
    {
        delete pobjIoContext;
        pobjIoContext = NULL;
    }

    return bRet;
}

bool NetSocketIocpSSL::postAccept(IO_CONTEXT_SSL *pAcceptIoContext)
{
    DWORD dwBytes = 0;
    pAcceptIoContext->m_OpType = NET_POST_ACCEPT;

    pAcceptIoContext->m_wsaBuf.buf = pAcceptIoContext->m_szBuffer;
    pAcceptIoContext->m_wsaBuf.len = MAX_BUFFER_LEN;

    pAcceptIoContext->ResetBuffer();

    WSABUF *p_wbuf   = &pAcceptIoContext->m_wsaBuf;
    OVERLAPPED *p_ol = &pAcceptIoContext->m_Overlapped;

    pAcceptIoContext->m_sockAccept  = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if( INVALID_SOCKET==pAcceptIoContext->m_sockAccept )
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("WSASocket INVALID_SOCKET, errorcode:%1").arg(WSAGetLastError()));
        return false;
    }
    NETLOG(NET_LOG_LEVEL_INFO, QString("WSASocket success, socket:%1").arg(pAcceptIoContext->m_sockAccept));

    if(FALSE == m_lpfnAcceptEx( m_pListenContext->m_Socket, pAcceptIoContext->m_sockAccept, p_wbuf->buf, p_wbuf->len - ((sizeof(SOCKADDR_IN)+16)*2),
                                sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &dwBytes, p_ol))
    {
        if(WSA_IO_PENDING != WSAGetLastError())
        {

            NETLOG(NET_LOG_LEVEL_ERROR, QString("post AcceptEx failed, errorcode:%1, listen socket:%2,accept socket:%3")
                   .arg(WSAGetLastError()).arg(m_pListenContext->m_Socket).arg(pAcceptIoContext->m_sockAccept));

            return false;
        }
    }

    NETLOG(NET_LOG_LEVEL_INFO, QString("post AcceptEx success, listen socket:%1,accept socket:%2")
           .arg(m_pListenContext->m_Socket).arg(pAcceptIoContext->m_sockAccept));

    return true;
}

bool NetSocketIocpSSL::postRecv(IO_CONTEXT_SSL *pIoContext)
{
    DWORD dwFlags = 0;
    DWORD dwBytes = 0;

    pIoContext->m_wsaBuf.buf = pIoContext->m_szBuffer;
    pIoContext->m_wsaBuf.len = MAX_BUFFER_LEN;

    WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
    OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

    pIoContext->ResetBuffer();
    pIoContext->m_OpType = NET_POST_RECEIVE;

    int nBytesRecv = WSARecv( pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL );
    if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("post WSARecv failed, errorcode:%1, post socket:%2").arg(WSAGetLastError()).arg(pIoContext->m_sockAccept));

        return false;
    }

    NETLOG(NET_LOG_LEVEL_INFO, QString("post WSARecv success, post socket:%1").arg(pIoContext->m_sockAccept));

    return true;
}

bool NetSocketIocpSSL::postSend(IO_CONTEXT_SSL *pIoContext)
{
    DWORD dwFlags = 0;
    DWORD dwBytes = 0;
    WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
    OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

    pIoContext->ResetBuffer();

    if(!NetKeepAliveThread::setCheckSend(pIoContext->m_sockAccept, pIoContext->m_nSissionID, true, SEND_PACKET_TIMEOUT_S, pIoContext))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("setCheckSend failed, post socket:%1").arg(pIoContext->m_sockAccept));
        return false;
    }

    int nBytesSend = WSASend(pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, dwFlags, p_ol, NULL );
    if ((SOCKET_ERROR == nBytesSend) && (WSA_IO_PENDING != WSAGetLastError()))
    {
        if(!NetKeepAliveThread::setCheckSend(pIoContext->m_sockAccept, pIoContext->m_nSissionID, false, SEND_PACKET_TIMEOUT_S, pIoContext))
        {
            NETLOG(NET_LOG_LEVEL_ERROR, QString("setCheckSend failed, post socket:%1").arg(pIoContext->m_sockAccept));
        }

        NETLOG(NET_LOG_LEVEL_ERROR, QString("post WSASend failed, errorcode:%1, post socket:%2").arg(WSAGetLastError()).arg(pIoContext->m_sockAccept));
        return false;
    }

    NETLOG(NET_LOG_LEVEL_INFO, QString("post WSASend success, post socket:%1").arg(pIoContext->m_sockAccept));
    return true;
}

bool NetSocketIocpSSL::associateWithIOCP( SOCKET_CONTEXT_SSL *pSocketContext )
{
    HANDLE hTemp = CreateIoCompletionPort((HANDLE)pSocketContext->m_Socket, m_hIOCompletionPort, (DWORD)pSocketContext, 0);

    if (NULL == hTemp)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("CreateIoCompletionPort failed, errorcode:%1, socket:%2").arg(WSAGetLastError()).arg(pSocketContext->m_Socket));
        return false;
    }

    NETLOG(NET_LOG_LEVEL_INFO, QString("CreateIoCompletionPort success, socket:%1").arg(pSocketContext->m_Socket));
    return true;
}

HANDLE NetSocketIocpSSL::getIOCompletionPortHandle()
{
    return m_hIOCompletionPort;
}

#endif
