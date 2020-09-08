#include "NetSocketEpollSSL.h"
#include "NetPacketManager.h"
#include "NetLog.h"
#include "NetKeepAliveThread.h"

#ifndef WIN32

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

QString NetSocketEpollSSL::g_strKeyPath = "ssl.key";
QString NetSocketEpollSSL::g_strCertPath = "ssl.crt";

static void sigpipe_handle(int signo)
{
     return;
}

NetSocketEpollSSL::NetSocketEpollSSL()
{
    m_nEpfd = 0;
    m_nListenfd = 0;

    m_pobjerrBio = NULL;
    m_pobjsslCtx = NULL;
}

NetSocketEpollSSL::~NetSocketEpollSSL()
{
    for(int i = 0; i < m_vecNetSocketEpollThread.size(); i++)
    {
        m_vecNetSocketEpollThread.at(i)->terminate();
        delete m_vecNetSocketEpollThread.at(i);
    }

    m_vecNetSocketEpollThread.clear();

    close(m_nListenfd);
    close(m_nEpfd);

    m_nListenfd = 0;
    m_nEpfd = 0;

    BIO_free((BIO*)m_pobjerrBio);
    SSL_CTX_free((SSL_CTX*)m_pobjsslCtx);
    ERR_free_strings();
}

bool NetSocketEpollSSL::init(const qint32 p_nThreadNum)
{
#ifndef WIN32
    signal(SIGPIPE, sigpipe_handle);
#endif

    SSL_load_error_strings ();
    int r = SSL_library_init ();
    if(!r)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("SSL_library_init failed,error:%1").arg(strerror(errno)));
        return false;
    }

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

    m_nListenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(m_nListenfd <= 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("creat socket failed,error:%1").arg(strerror(errno)));
        return false;
    }

    int nFlag = 1;
    int nRet = setsockopt(m_nListenfd, IPPROTO_TCP, TCP_NODELAY, (char *)&nFlag, sizeof(nFlag) );
    if (nRet < 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("setsockopt nodelay failed,error:%1,socket:%2").arg(strerror(errno)).arg(m_nListenfd));
        close(m_nListenfd);
        return false;
    }

    unsigned long argp = 1;
    nRet = ioctl(m_nListenfd, FIONBIO, (unsigned long*)&argp);
    if(nRet < 0)
    {
        close(m_nListenfd);
        NETLOG(NET_LOG_LEVEL_ERROR, QString("ioctl fionbio 1 failed,error:%1,socket:%2").arg(strerror(errno)).arg(m_nListenfd));
        return false;
    }

    m_nEpfd = epoll_create(EPOLL_SOCKET_MAX_SIZE);
    if(m_nEpfd <= 0)
    {
        close(m_nListenfd);
        NETLOG(NET_LOG_LEVEL_ERROR, QString("epoll_create %1 failed,error:%2").arg(EPOLL_SOCKET_MAX_SIZE).arg(strerror(errno)));
        return false;
    }

    for(int i = 0; i < p_nThreadNum; i++)
    {
        NetSocketEpollSSLThread* pobjNetSocketEpollSSLThread = new NetSocketEpollSSLThread;
        pobjNetSocketEpollSSLThread->init(i, m_nEpfd, m_nListenfd, m_pobjsslCtx);
        m_vecNetSocketEpollThread.append(pobjNetSocketEpollSSLThread);
    }

    return true;
}

bool NetSocketEpollSSL::start(const QString &p_strBindIP, const qint32 p_nPort)
{
    if(p_strBindIP.isEmpty() || p_nPort < 0 || p_nPort > 65535)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("bindip or port check failed,bind ip:%1,port:%2").arg(p_strBindIP).arg(p_nPort));
        return false;
    }

    ADDRINFO Hints, *AddrInfo;
    memset(&Hints, 0, sizeof(Hints));
    Hints.ai_family = AF_INET;
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_flags = AI_PASSIVE ;

    char szPort[32] = {0};
    sprintf(szPort, "%d", p_nPort);
    int nRet = getaddrinfo(p_strBindIP.toStdString().c_str(), szPort, &Hints, &AddrInfo);
    if (nRet != 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("getaddrinfo error,bind ip:%1,port:%2,error:%3").arg(p_strBindIP).arg(p_nPort).arg(strerror(errno)));
        return false;
    }

    int opt = 1;
    nRet = setsockopt(m_nListenfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    if(nRet < 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("setsockopt SO_REUSEADDR failed,socket:%1,error:%2").arg(m_nListenfd).arg(strerror(errno)));
        return false;
    }

    nRet = bind(m_nListenfd, AddrInfo->ai_addr, AddrInfo->ai_addrlen);
    if(nRet < 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("bind failed,socket:%1,error:%2").arg(m_nListenfd).arg(strerror(errno)));
        return false;
    }

    freeaddrinfo(AddrInfo);

    nRet = listen(m_nListenfd, LISTEN_SIZE);
    if(nRet < 0)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("listen socket:%1,error:%2").arg(m_nListenfd).arg(strerror(errno)));
        return false;
    }

    EpollSSLPacket *pobjEpollPacket = new EpollSSLPacket;
    pobjEpollPacket->nFd = m_nListenfd;
    struct epoll_event stEvent;
    stEvent.data.ptr = pobjEpollPacket;
    stEvent.events=EPOLLIN|EPOLLET;
    nRet = epoll_ctl(m_nEpfd, EPOLL_CTL_ADD, m_nListenfd,&stEvent);
    if(nRet < 0)
    {
        delete pobjEpollPacket;
        NETLOG(NET_LOG_LEVEL_ERROR, QString("epoll_ctl add failed, socket:%1,error:%2").arg(m_nListenfd).arg(strerror(errno)));
        return false;
    }

    for(int i = 0; i < m_vecNetSocketEpollThread.size(); i++)
    {
        m_vecNetSocketEpollThread.at(i)->start();
    }

    return true;
}

bool NetSocketEpollSSL::send(NetPacketBase *p_pobjNetPacketBase)
{
    if(p_pobjNetPacketBase == NULL)
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("send Null Pointer"));
        return false;
    }

    EpollSSLPacket *pobjEpollSendPacket = new EpollSSLPacket;
    pobjEpollSendPacket->nFd = p_pobjNetPacketBase->m_nSocket;
    pobjEpollSendPacket->nSendIndex = 0;
    pobjEpollSendPacket->pobjSsl = (SSL*)p_pobjNetPacketBase->m_pobjSSL;
    pobjEpollSendPacket->nSissionID = p_pobjNetPacketBase->m_nSissionID;
    pobjEpollSendPacket->bKeepAlive = p_pobjNetPacketBase->m_bKeepAlive;
    pobjEpollSendPacket->nIndex = p_pobjNetPacketBase->m_nIndex;

    pobjEpollSendPacket->bSslConnected = true;
    pobjEpollSendPacket->bTcpConnected = true;

    if(!NetPacketManager::prepareResponse(p_pobjNetPacketBase, pobjEpollSendPacket->bytSendData))
    {
        delete pobjEpollSendPacket;
        NETLOG(NET_LOG_LEVEL_ERROR, QString("prepareResponse failed, socket:%1").arg(pobjEpollSendPacket->nFd));
        return false;
    }

    quint32 nIndex = pobjEpollSendPacket->nIndex;
    void* pobjContext = NULL;
    if(!NetKeepAliveThread::lockIndexContext(pobjEpollSendPacket->nIndex, pobjEpollSendPacket->nFd, pobjEpollSendPacket->nSissionID, pobjContext))
    {
        NETLOG(NET_LOG_LEVEL_ERROR, QString("lockIndexContext failed, post socket:%1").arg(p_pobjNetPacketBase->m_nSocket));
        delete pobjEpollSendPacket;
        return false;
    }

    if(!NetKeepAliveThread::setCheckSend(p_pobjNetPacketBase->m_nSocket, p_pobjNetPacketBase->m_nSissionID, p_pobjNetPacketBase->m_nIndex, true, p_pobjNetPacketBase->m_nTimeOutS))
    {
        NETLOG(NET_LOG_LEVEL_WORNING, QString("setCheckSend failed, post socket:%1").arg(p_pobjNetPacketBase->m_nSocket));
        delete pobjEpollSendPacket;
        NetKeepAliveThread::unlockIndex(nIndex);
        return false;
    }

    NetKeepAliveThread::unlockIndex(nIndex);

    struct epoll_event stEvent;
    memset(&stEvent, 0, sizeof(stEvent));
    stEvent.data.ptr = pobjEpollSendPacket;
    stEvent.events=EPOLLOUT | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
    int nRet = epoll_ctl(m_nEpfd, EPOLL_CTL_MOD, p_pobjNetPacketBase->m_nSocket,&stEvent);
    if(nRet < 0)
    {
        NetKeepAliveThread::setCheckSend(p_pobjNetPacketBase->m_nSocket, p_pobjNetPacketBase->m_nSissionID,p_pobjNetPacketBase->m_nIndex,  false);
        delete pobjEpollSendPacket;
        NETLOG(NET_LOG_LEVEL_ERROR, QString("epoll_ctl mod EPOLLOUT failed, socket:%1").arg(p_pobjNetPacketBase->m_nSocket));
        return false;
    }

    return true;
}
#endif
