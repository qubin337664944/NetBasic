#include "NetSocketIocp.h"
#include "NetPacketManager.h"
#include <QDebug>
#include "NetLog.h"

#ifdef WIN32

NetSocketIocp::NetSocketIocp()
{
}

bool NetSocketIocp::init(const qint32 p_nThreadNum)
{
    WSADATA wsaData;
    int nResult;
    nResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    // 错误(一般都不可能出现)
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

    for(int i = 0; i < p_nThreadNum; i++)
    {
        NetSocketIocpThread* pobjNetSocketIocpThread = new NetSocketIocpThread;
        pobjNetSocketIocpThread->init(i, this);
        pobjNetSocketIocpThread->start();
        m_vecNetSocketIocpThread.append(pobjNetSocketIocpThread);
    }

    return true;
}

bool NetSocketIocp::start(const QString &p_strBindIP, const qint32 p_nPort)
{
    if(p_nPort<0 || p_nPort>65535)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("port is out of limit 0-65535,port:%1").arg(p_nPort));
        return false;
    }

    // AcceptEx 和 GetAcceptExSockaddrs 的GUID，用于导出函数指针
    GUID GuidAcceptEx = WSAID_ACCEPTEX;
    GUID GuidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;

    // 服务器地址信息，用于绑定Socket
    struct sockaddr_in ServerAddress;

    // 生成用于监听的Socket的信息
    m_pListenContext = new SOCKET_CONTEXT;

    // 需要使用重叠IO，必须得使用WSASocket来建立Socket，才可以支持重叠IO操作
    m_pListenContext->m_Socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (INVALID_SOCKET == m_pListenContext->m_Socket)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("WSASocket INVALID_SOCKET, errorcode:%1").arg(WSAGetLastError()));
        return false;
    }
    else
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("WSASocket success, errorcode:%1").arg(WSAGetLastError()));
    }

    // 将Listen Socket绑定至完成端口中
    if( NULL== CreateIoCompletionPort( (HANDLE)m_pListenContext->m_Socket, m_hIOCompletionPort,(DWORD)m_pListenContext, 0))
    {
        RELEASE_SOCKET( m_pListenContext->m_Socket );
        NETLOG(NET_LOG_LEVEL_ERROR, QString("CreateIoCompletionPort failed, errorcode:%1").arg(WSAGetLastError()));
        return false;
    }
    else
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("Listen Socket CreateIoCompletionPort success"));
    }

    // 填充地址信息
    ZeroMemory((char *)&ServerAddress, sizeof(ServerAddress));
    ServerAddress.sin_family = AF_INET;
    // 这里可以绑定任何可用的IP地址，或者绑定一个指定的IP地址
    ServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    //ServerAddress.sin_addr.s_addr = inet_addr(CStringA(m_strIP).GetString());
    ServerAddress.sin_port = htons(p_nPort);

    // 绑定地址和端口
    if (SOCKET_ERROR == bind(m_pListenContext->m_Socket, (struct sockaddr *) &ServerAddress, sizeof(ServerAddress)))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("bind port %1 failed, errorcode:%2").arg(p_nPort).arg(WSAGetLastError()));
        return false;
    }
    else
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("bind port %1 success").arg(p_nPort));
    }

    // 开始进行监听
    if (SOCKET_ERROR == listen(m_pListenContext->m_Socket,SOMAXCONN))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("Listen failed, errorcode:%1").arg(WSAGetLastError()));
        return false;
    }
    else
    {
        NETLOG(NET_LOG_LEVEL_INFO, QString("Listen success"));
    }

    // 使用AcceptEx函数，因为这个是属于WinSock2规范之外的微软另外提供的扩展函数
    // 所以需要额外获取一下函数的指针，
    // 获取AcceptEx函数指针
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
        return false;
    }

    // 获取GetAcceptExSockAddrs函数指针，也是同理
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
        return false;
    }



    for( int i = 0; i < 5; i++ )
    {
        // 新建一个IO_CONTEXT
        IO_CONTEXT* pAcceptIoContext = new IO_CONTEXT;

        if( false==this->postAccept( pAcceptIoContext ) )
        {
            RELEASE(pAcceptIoContext);
            NETLOG(NET_LOG_LEVEL_ERROR, QString("postAccept error, errorcode:%1").arg(WSAGetLastError()));
            return false;
        }
    }

    return true;
}

bool NetSocketIocp::send(NetPacketBase *p_pobjNetPacketBase)
{
    QByteArray bytSend;

    if(!NetPacketManager::prepareResponse(p_pobjNetPacketBase, bytSend))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("prepareResponse failed"));
        return false;
    }

    IO_CONTEXT* pobjIoContext = new IO_CONTEXT;
    pobjIoContext->m_wsaBuf.buf = new char[bytSend.size()+1];
    pobjIoContext->m_wsaBuf.len = bytSend.size();
    memcpy(pobjIoContext->m_wsaBuf.buf, bytSend.data(), bytSend.length());
    pobjIoContext->m_sockAccept = p_pobjNetPacketBase->m_nSocket;
    pobjIoContext->m_bIsCloseConnect = true;
    pobjIoContext->m_OpType = NET_POST_SEND;

    return postSend(pobjIoContext);
}

bool NetSocketIocp::postAccept(IO_CONTEXT *pAcceptIoContext)
{
    DWORD dwBytes = 0;
    pAcceptIoContext->m_OpType = NET_POST_ACCEPT;
    WSABUF *p_wbuf   = &pAcceptIoContext->m_wsaBuf;
    OVERLAPPED *p_ol = &pAcceptIoContext->m_Overlapped;

    // 为以后新连入的客户端先准备好Socket( 这个是与传统accept最大的区别 )
    pAcceptIoContext->m_sockAccept  = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if( INVALID_SOCKET==pAcceptIoContext->m_sockAccept )
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("WSASocket INVALID_SOCKET, errorcode:%1").arg(WSAGetLastError()));
        return false;
    }

    NETLOG(NET_LOG_LEVEL_INFO, QString("WSASocket success, socket:%1").arg(pAcceptIoContext->m_sockAccept));

    // 投递AcceptEx
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

bool NetSocketIocp::postRecv(IO_CONTEXT *pIoContext)
{
    // 初始化变量
    DWORD dwFlags = 0;
    DWORD dwBytes = 0;
    WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
    OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

    pIoContext->ResetBuffer();
    pIoContext->m_OpType = NET_POST_RECEIVE;

    // 初始化完成后，，投递WSARecv请求
    int nBytesRecv = WSARecv( pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, &dwFlags, p_ol, NULL );

    // 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
    if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("post WSARecv failed, errorcode:%1, post socket:%2").arg(WSAGetLastError()).arg(pIoContext->m_sockAccept));

        return false;
    }

    NETLOG(NET_LOG_LEVEL_INFO, QString("post WSARecv success, post socket:%1").arg(pIoContext->m_sockAccept));

    return true;
}

bool NetSocketIocp::postSend(IO_CONTEXT *pIoContext)
{
    // 初始化变量
    DWORD dwFlags = 0;
    DWORD dwBytes = 0;
    WSABUF *p_wbuf   = &pIoContext->m_wsaBuf;
    OVERLAPPED *p_ol = &pIoContext->m_Overlapped;

    pIoContext->ResetBuffer();

    // 初始化完成后，，投递WSASend请求
    int nBytesSend = WSASend(pIoContext->m_sockAccept, p_wbuf, 1, &dwBytes, dwFlags, p_ol, NULL );

    // 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
    if ((SOCKET_ERROR == nBytesSend) && (WSA_IO_PENDING != WSAGetLastError()))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("post WSASend failed, errorcode:%1, post socket:%2").arg(WSAGetLastError()).arg(pIoContext->m_sockAccept));
        return false;
    }

    NETLOG(NET_LOG_LEVEL_INFO, QString("post WSASend success, post socket:%1").arg(pIoContext->m_sockAccept));
    return true;
}

bool NetSocketIocp::associateWithIOCP( SOCKET_CONTEXT *pSocketContext )
{
    // 将用于和客户端通信的SOCKET绑定到完成端口中
    HANDLE hTemp = CreateIoCompletionPort((HANDLE)pSocketContext->m_Socket, m_hIOCompletionPort, (DWORD)pSocketContext, 0);

    if (NULL == hTemp)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("CreateIoCompletionPort failed, errorcode:%1, socket:%2").arg(WSAGetLastError()).arg(pSocketContext->m_Socket));
        return false;
    }

    NETLOG(NET_LOG_LEVEL_INFO, QString("CreateIoCompletionPort success, socket:%1").arg(pSocketContext->m_Socket));
    return true;
}

HANDLE NetSocketIocp::getIOCompletionPortHandle()
{
    return m_hIOCompletionPort;
}

#endif
