#ifndef NETSOCKETIOCP_H
#define NETSOCKETIOCP_H

#ifdef WIN32

#include <QCoreApplication>

#include <winsock2.h>
#include <MSWSock.h>
#include "NetEnum.h"
#include "NetInclude.h"
#include "NetSocketIocpThread.h"

#include <QQueue>
#include <QVector>

#include "NetPacketBase.h"
#include "NetSocketBase.h"



// 释放指针宏
#define RELEASE(x)                      {if(x != NULL ){delete x;x=NULL;}}
// 释放句柄宏
#define RELEASE_HANDLE(x)               {if(x != NULL && x!=INVALID_HANDLE_VALUE){ CloseHandle(x);x = NULL;}}
// 释放Socket宏
#define RELEASE_SOCKET(x)               {if(x !=INVALID_SOCKET) { closesocket(x);x=INVALID_SOCKET;}}


struct IO_CONTEXT
{
    OVERLAPPED     m_Overlapped;                               // 每一个重叠网络操作的重叠结构(针对每一个Socket的每一个操作，都要有一个)
    SOCKET         m_sockAccept;                               // 这个网络操作所使用的Socket
    WSABUF         m_wsaBuf;                                   // WSA类型的缓冲区，用于给重叠操作传参数的
    char           m_szBuffer[MAX_BUFFER_LEN];                 // 这个是WSABUF里具体存字符的缓冲区
    qint32         m_OpType;                                   // 标识网络操作的类型(对应上面的枚举)
    bool           m_bIsCloseConnect;

    // 初始化
    IO_CONTEXT()
    {
        ZeroMemory(&m_Overlapped, sizeof(m_Overlapped));
        ZeroMemory( m_szBuffer,MAX_BUFFER_LEN );
        m_sockAccept = INVALID_SOCKET;
        m_wsaBuf.buf = m_szBuffer;
        m_wsaBuf.len = MAX_BUFFER_LEN;
        m_OpType     = NET_POST_ERROR;
        m_bIsCloseConnect = false;
    }

    // 释放掉Socket
    ~IO_CONTEXT()
    {
        if( m_sockAccept!=INVALID_SOCKET )
        {
            m_sockAccept = INVALID_SOCKET;
        }
    }

    // 重置缓冲区内容
    void ResetBuffer()
    {
        ZeroMemory( m_szBuffer,MAX_BUFFER_LEN );
    }
};

struct SOCKET_CONTEXT
{
    SOCKET      m_Socket;                                  // 每一个客户端连接的Socket
    SOCKADDR_IN m_ClientAddr;                              // 客户端的地址
};

class NetSocketIocp : public NetSocketBase
{
public:
    NetSocketIocp();

    virtual bool init(const qint32 p_nThreadNum);

    virtual bool start(const QString& p_strBindIP, const qint32 p_nPort);

    virtual bool send(NetPacketBase* p_pobjNetPacketBase);


    bool postAccept( IO_CONTEXT* pAcceptIoContext );


    bool postRecv( IO_CONTEXT* pIoContext );


    bool postSend( IO_CONTEXT* pIoContext );


    HANDLE getIOCompletionPortHandle();

    bool associateWithIOCP( SOCKET_CONTEXT *pSocketContext );

public:
    QString m_strBindIP;
    qint32 m_nPort;

    HANDLE                       m_hIOCompletionPort;           // 完成端口的句柄
    LPFN_ACCEPTEX                m_lpfnAcceptEx;                // AcceptEx 和 GetAcceptExSockaddrs 的函数指针，用于调用这两个扩展函数
    LPFN_GETACCEPTEXSOCKADDRS    m_lpfnGetAcceptExSockAddrs;

    QVector<NetSocketIocpThread*> m_vecNetSocketIocpThread;

    SOCKET_CONTEXT*              m_pListenContext;              // 用于监听的Socket的Context信息
};
#endif
#endif // NETSOCKETIOCP_H
