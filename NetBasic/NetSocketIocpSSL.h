#ifndef NETSOCKETIOCPSSL_H
#define NETSOCKETIOCPSSL_H

#ifdef WIN32

#include <QCoreApplication>

#include <winsock2.h>
#include <MSWSock.h>
#include "NetEnum.h"
#include "NetInclude.h"
#include "NetSocketIocpSSLThread.h"

#include <QQueue>
#include <QVector>
#include <QMutex>
#include <QMap>
#include <QDebug>

#include "NetPacketBase.h"
#include "NetSocketBase.h"



// 释放指针宏
#define RELEASE(x)                      {if(x != NULL ){delete x;x=NULL;}}
// 释放句柄宏
#define RELEASE_HANDLE(x)               {if(x != NULL && x!=INVALID_HANDLE_VALUE){ CloseHandle(x);x = NULL;}}
// 释放Socket宏
#define RELEASE_SOCKET(x)               {if(x !=INVALID_SOCKET) { closesocket(x);x=INVALID_SOCKET;}}


struct IO_CONTEXT_SSL
{
    OVERLAPPED     m_Overlapped;                               // 每一个重叠网络操作的重叠结构(针对每一个Socket的每一个操作，都要有一个)
    SOCKET         m_sockAccept;                               // 这个网络操作所使用的Socket
    WSABUF         m_wsaBuf;                                   // WSA类型的缓冲区，用于给重叠操作传参数的
    char           m_szBuffer[MAX_BUFFER_LEN];                 // 这个是WSABUF里具体存字符的缓冲区
    qint32         m_OpType;                                   // 标识网络操作的类型(对应上面的枚举)
    bool           m_bIsCloseConnect;
    char*          m_szSendData;
    int            m_nSendDataSize;
    int            m_nSendIndex;
    NetPacketBase* m_pobjNetPacketBase;
    void*          m_pobjSocketContext;
    quint32        m_nSissionID;
    void*          m_pobjSSL;

    // 初始化
    IO_CONTEXT_SSL()
    {
        ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));
        ZeroMemory( m_szBuffer,MAX_BUFFER_LEN );
        m_sockAccept = INVALID_SOCKET;
        m_wsaBuf.buf = m_szBuffer;
        m_wsaBuf.len = MAX_BUFFER_LEN;
        m_OpType     = NET_POST_ERROR;
        m_bIsCloseConnect = false;
        m_szSendData  = NULL;
        m_nSendDataSize = 0;
        m_nSendIndex = 0;
        m_pobjNetPacketBase = NULL;
        m_pobjSocketContext = NULL;
        m_nSissionID = 0;
        m_pobjSSL = NULL;
    }

    // 释放掉Socket
    ~IO_CONTEXT_SSL()
    {
        if( m_sockAccept!=INVALID_SOCKET )
        {
            m_sockAccept = INVALID_SOCKET;
        }

        if(m_szSendData)
        {
            delete []m_szSendData;
            m_szSendData = NULL;
        }

        if(m_pobjNetPacketBase)
        {
            delete m_pobjNetPacketBase;
            m_pobjNetPacketBase = NULL;
        }
    }

    // 重置缓冲区内容
    void ResetBuffer()
    {
        ZeroMemory( m_szBuffer,MAX_BUFFER_LEN );
    }
};

struct SOCKET_CONTEXT_SSL
{
    SOCKET      m_Socket;                                  // 每一个客户端连接的Socket
    SOCKADDR_IN m_ClientAddr;                              // 客户端的地址

    IO_CONTEXT_SSL* m_pobjSendContext;
    IO_CONTEXT_SSL* m_pobjReceiveContext;

    QMap<IO_CONTEXT_SSL*, IO_CONTEXT_SSL*> m_mapSendContext;
    QMap<IO_CONTEXT_SSL*, IO_CONTEXT_SSL*> m_mapReceiveContext;

    QMutex m_objSendMutex;
    QMutex m_objReceiveMutex;
    QMutex m_objCloseMutex;

    bool m_bClosed;

    bool m_bKeepAliveTimeOut;

    quint32 m_nSissionID;

    void* m_pobjSSL;
    void* m_pobjBioSend;
    void* m_pobjBioReceive;
    bool m_bIsHandShake;

    SOCKET_CONTEXT_SSL()
    {
        m_bClosed = false;
        m_bKeepAliveTimeOut = false;
        m_nSissionID = 0;

        m_pobjSSL = NULL;
        m_pobjBioSend = NULL;
        m_pobjBioReceive = NULL;
        m_bIsHandShake = false;
    }

    ~SOCKET_CONTEXT_SSL()
    {
    }

    void closeSocket()
    {
        QMutexLocker objLocker(&m_objCloseMutex);
        if(m_bClosed)
        {
            return;
        }

        closesocket(m_Socket);
        m_bClosed = true;
    }

    bool appendSendContext(IO_CONTEXT_SSL* pobjContext)
    {
        QMutexLocker objLocker(&m_objSendMutex);
        if(m_mapSendContext.contains(pobjContext))
        {
            m_mapSendContext.remove(pobjContext);
        }
        else
        {
            m_mapSendContext.insert(pobjContext, pobjContext);
        }

        return true;
    }

    bool cancelSendContext(IO_CONTEXT_SSL* pobjContext)
    {
        QMutexLocker objLocker(&m_objSendMutex);
        if(m_mapSendContext.contains(pobjContext))
        {
            m_mapSendContext.remove(pobjContext);
        }

        return true;
    }

    bool appendReceiveContext(IO_CONTEXT_SSL* pobjContext)
    {
        QMutexLocker objLocker(&m_objReceiveMutex);
        if(m_mapReceiveContext.contains(pobjContext))
        {
            m_mapReceiveContext.remove(pobjContext);
        }
        else
        {
            m_mapReceiveContext.insert(pobjContext, pobjContext);
        }
        return true;
    }

    bool cancelReceiveContext(IO_CONTEXT_SSL* pobjContext)
    {
        QMutexLocker objLocker(&m_objReceiveMutex);
        if(m_mapReceiveContext.contains(pobjContext))
        {
            m_mapReceiveContext.remove(pobjContext);
        }

        return true;
    }

    bool closeContext()
    {
        QMutexLocker objLockerSend(&m_objSendMutex);
        if(m_mapSendContext.size() > 0)
        {
            return false;
        }

        QMutexLocker objLockerReceive(&m_objReceiveMutex);
        if(m_mapReceiveContext.size() > 0)
        {
            return false;
        }

        return true;
    }
};

class NetSocketIocpSSL : public NetSocketBase
{
public:
    NetSocketIocpSSL();
    ~NetSocketIocpSSL();

    virtual bool init(const qint32 p_nThreadNum);

    virtual bool start(const QString& p_strBindIP, const qint32 p_nPort);

    virtual bool send(NetPacketBase* p_pobjNetPacketBase);

    bool postAccept( IO_CONTEXT_SSL* pAcceptIoContext);


    bool postRecv( IO_CONTEXT_SSL* pIoContext );


    bool postSend( IO_CONTEXT_SSL* pIoContext );


    HANDLE getIOCompletionPortHandle();

    bool associateWithIOCP( SOCKET_CONTEXT_SSL *pSocketContext );

public:
    static QString g_strKeyPath;
    static QString g_strCertPath;

public:
    QString m_strBindIP;
    qint32 m_nPort;

    HANDLE                       m_hIOCompletionPort;           // 完成端口的句柄
    LPFN_ACCEPTEX                m_lpfnAcceptEx;                // AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
    LPFN_GETACCEPTEXSOCKADDRS    m_lpfnGetAcceptExSockAddrs;

    QVector<NetSocketIocpSSLThread*> m_vecNetSocketIocpThread;

    SOCKET_CONTEXT_SSL*              m_pListenContext;              // 用于监听的Socket的Context信息

    void*                       m_pobjsslCtx;
    void*                       m_pobjerrBio;
};
#endif
#endif // NETSOCKETIOCP_H
