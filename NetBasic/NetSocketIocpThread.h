#ifndef NETSOCKETIOCPTHREAD_H
#define NETSOCKETIOCPTHREAD_H

#ifdef WIN32

#include <QThread>
#include <winsock2.h>

class NetSocketIocp;
struct SOCKET_CONTEXT;
struct IO_CONTEXT;
class NetPacketManager;
class NetKeepAliveThread;

class NetSocketIocpThread : public QThread
{
public:
    NetSocketIocpThread();

    void init(qint32 p_nThreadID, NetSocketIocp* p_pobjNetSocketIocp, NetPacketManager* p_pobjNetPacketManager, NetKeepAliveThread *p_pobjNetKeepAliveThread);

protected:
    virtual	void	run();

private:
    bool doAccept(SOCKET_CONTEXT* pSocketContext, IO_CONTEXT* pIoContext);

    bool doReceive(SOCKET_CONTEXT* pSocketContext, IO_CONTEXT* pIoContext, bool& p_bIsLock);

    bool doSend(SOCKET_CONTEXT* pSocketContext, IO_CONTEXT* pIoContext , qint32 p_nSendSuccessSize = 0);

    bool doDisConnect(SOCKET_CONTEXT* pSocketContext, IO_CONTEXT* pIoContext);

private:
    NetPacketManager* m_pobjNetPacketManager;
    NetKeepAliveThread* m_pobjNetKeepAliveThread;

    NetSocketIocp* m_pobjNetSocketIocp;
    HANDLE m_hIOCompletionPort;

    qint32 m_nThreadID;
};

#endif
#endif // NETSOCKETIOCPTHREAD_H
