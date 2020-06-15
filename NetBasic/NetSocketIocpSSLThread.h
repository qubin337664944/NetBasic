#ifndef NETSOCKETIOCPSSLTHREAD_H
#define NETSOCKETIOCPSSLTHREAD_H

#ifdef WIN32

#include <QThread>
#include <winsock2.h>

class NetSocketIocpSSL;
struct SOCKET_CONTEXT_SSL;
struct IO_CONTEXT_SSL;
class NetSocketIocpSSLThread : public QThread
{
public:
    NetSocketIocpSSLThread();

    void init(qint32 p_nThreadID, NetSocketIocpSSL *p_pobjNetSocketIocp, void*   p_pobjsslCtx = NULL);

protected:
    virtual	void	run();

private:
    bool doAccept(SOCKET_CONTEXT_SSL* pSocketContext, IO_CONTEXT_SSL* pIoContext );

    bool doReceive(SOCKET_CONTEXT_SSL* pSocketContext, IO_CONTEXT_SSL* pIoContext );

    bool doSend(SOCKET_CONTEXT_SSL* pSocketContext, IO_CONTEXT_SSL* pIoContext , qint32 p_nSendSuccessSize = 0);

    bool doDisConnect(SOCKET_CONTEXT_SSL* pSocketContext, IO_CONTEXT_SSL* pIoContext );

private:
    NetSocketIocpSSL* m_pobjNetSocketIocp;
    HANDLE m_hIOCompletionPort;

    qint32 m_nThreadID;

    void*   m_pobjsslCtx;
};

#endif
#endif // NETSOCKETIOCPTHREAD_H
