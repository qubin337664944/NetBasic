#include "NetClientInterface.h"

#ifdef USE_HTTPS
#include "openssl/ssl.h"
#include "openssl/err.h"
#endif

#include <QHostAddress>
#include <QTime>
#include <QThread>
#include <QTime>

#define MAX_NAME 65536

#ifndef WIN32
    #include <iostream>
    #include <sys/socket.h>
    #include <sys/epoll.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <stdio.h>
    #include <errno.h>
    #include <string.h>
    #include <stdlib.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <sys/time.h>
    #include <sys/resource.h>
    #include <netdb.h>
    #include <netinet/tcp.h>
    #include <linux/sockios.h>
    #include <arpa/inet.h>
    #include <sys/ioctl.h>
    #include <net/if.h>
    #include <netdb.h>
    #include <sys/select.h>
    #include <signal.h>
    #include <errno.h>
    #include <sys/time.h>
    #include <unistd.h>
    #include <assert.h>
    #include <signal.h>

    typedef  struct addrinfo						ADDRINFO;
#else
    #include <windows.h>
    #include <ws2tcpip.h>
    #include <winsock2.h>
    #include <errno.h>
    #include <assert.h>
    #define close(x)                closesocket(x);
#endif //WIN32

#ifdef USE_HTTPS
void* NetClientInterface::g_pSSLCtx = NULL;
QMutex NetClientInterface::g_mutexSSLInit;
static pthread_mutex_t *lockarray = NULL;
#include <openssl/crypto.h>
static void sigpipe_handle(int signo)
{
     return;
}

static void lock_callback(int mode, int type, const char *file, int line)
{
  if(mode & CRYPTO_LOCK)
  {
    pthread_mutex_lock(&(lockarray[type]));
  }
  else
  {
    pthread_mutex_unlock(&(lockarray[type]));
  }
}

static unsigned long thread_id(void)
{
  unsigned long ret;

  ret=(unsigned long)pthread_self();
  return ret;
}

static void init_locks(void)
{
  int i;

  lockarray=(pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() *
                                            sizeof(pthread_mutex_t));
  for(i=0; i<CRYPTO_num_locks(); i++)
  {
    pthread_mutex_init(&(lockarray[i]), NULL);
  }

  CRYPTO_set_id_callback((unsigned long (*)())thread_id);
  CRYPTO_set_locking_callback(lock_callback);
}

static void kill_locks(void)
{
  int i;

  CRYPTO_set_locking_callback(NULL);
  for(i=0; i<CRYPTO_num_locks(); i++)
  {
      pthread_mutex_destroy(&(lockarray[i]));
  }

  OPENSSL_free(lockarray);
}
#endif

NetClientInterface::NetClientInterface()
{
    initEnv();

#ifdef WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    WSAStartup(wVersionRequested, &wsaData);
#endif
}

NetClientInterface::~NetClientInterface()
{
    clearEnv(true, true);
    if(m_pMutexParameter != NULL)
    {
        delete m_pMutexParameter;
        m_pMutexParameter   =   NULL;
    }
    if(m_pMutexPerform != NULL)
    {
        delete m_pMutexPerform;
        m_pMutexPerform   =   NULL;
    }
#ifdef WIN32
    WSACleanup();
#endif
}

void NetClientInterface::initEnv()
{
    m_pMutexParameter   =   new QMutex();
    m_pMutexPerform =    new QMutex();
    m_nSocketFd = -1;
    m_nSendBandwidthLimitBytes = 0;

    m_bIsSupportHttps = false;
    m_bUseHttpsFlag = false;
    m_bKeepAlive = false;
    m_mapReqHead.clear();
    m_mapCustomReqHead.clear();

    changeBaseHead("Accept","image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, application/x-shockwave-flash, application/vnd.ms-excel, application/vnd.ms-powerpoint, application/msword, */*");
    changeBaseHead("Accept-Language","zh-cn");
    changeBaseHead("User-Agent","aws-sdk-dotnet-45/3.1.1.1 aws-sdk-dotnet-core/3.1.0.1 .NET_Runtime/4.0 .NET_Framework/4.0 OS/Microsoft_Windows_NT_6.1.7601_Service_Pack_1 ClientSync");
    changeBaseHead("Cache-Control","no-cache");

    clearEnv(true, true);

    setTimeOut(5000, 30000);

    m_strLastError = "OK";

#ifdef USE_HTTPS
    m_pSSL = NULL;
    m_bIsSupportHttps = true;

    if(g_pSSLCtx == NULL)
    {
       NetClientInterface::sslGoableInit();
    }
#endif
}

void NetClientInterface::clearEnv(bool p_bIsClearDebugInfo, const bool p_bIsCloseConnect)
{
    m_strMethod = "";
    m_strURL = "";
    m_strURLInterface = "";

    if(p_bIsCloseConnect)
    {
        closeConnect();

        m_strServerIP = "";
        m_nServerPort = 0;
    }

    if(p_bIsClearDebugInfo)
    {
        m_stHttpResult.nResultCode = -1;
        m_stHttpResult.strProtocol = "HTTP/1.1";
        m_stHttpResult.strResultMsg = "Accurad Error";

        m_bytReqInfo.clear();

        m_bytResHead.clear();
        m_bytResData.clear();
        m_mapResHead.clear();
        m_bytResAll.clear();

        memset(szRecvDataTempLev1, 0, RECEIVE_TEMP_SIZE);

        m_nParseURLTime = -1;
        m_nConnectTime = -1;
        m_nReqTime = -1;
        m_nResTime = -1;
    }
}

bool NetClientInterface::setUseHttps(bool p_bUseHttpsFlag)
{
    QMutexLocker objLocker(m_pMutexParameter);

    if(!m_bIsSupportHttps)
    {
        m_strLastError = QString("this NetClientInterface.so dosn't compile whith https");
        qDebug()<<m_strLastError;
        return false;
    }

    m_bUseHttpsFlag = p_bUseHttpsFlag;
    return true;
}

void NetClientInterface::setSendBandwidthLimit(const qint32 p_nSendBandwidthLimitBits)
{
    QMutexLocker objLocker(m_pMutexParameter);

    if(p_nSendBandwidthLimitBits > 0)
    {
        m_nSendBandwidthLimitBytes = p_nSendBandwidthLimitBits/8;
    }
}

void NetClientInterface::setTimeOut(const qint32 p_nConnectTimeOutMs, const qint32 p_nRequestTimeOutMs)
{
    QMutexLocker objLocker(m_pMutexParameter);

    m_nConnectTimeOutMs = p_nConnectTimeOutMs;
    m_nRequestTimeOutMs = p_nRequestTimeOutMs;
}

void NetClientInterface::clearReqHead()
{
    QMutexLocker objLocker(m_pMutexParameter);
    m_mapCustomReqHead.clear();
}

bool NetClientInterface::appendReqHead(const QString& p_strHeadKey, const QString& p_strHeadValue)
{
    QMutexLocker objLocker(m_pMutexParameter);

    if(m_mapReqHead.contains(p_strHeadKey))
    {
        m_mapReqHead[p_strHeadKey] = p_strHeadValue;
        return true;
    }

    if(m_mapCustomReqHead.contains(p_strHeadKey))
    {
        m_mapCustomReqHead[p_strHeadKey] = p_strHeadValue;
        return true;
    }

    m_mapCustomReqHead.insert(p_strHeadKey, p_strHeadValue);
    return true;
}

const QMap<QString, QString> NetClientInterface::getReqHead()
{
    QMutexLocker objLocker(m_pMutexParameter);

    QMap<QString, QString> mapHead = m_mapReqHead;
    QMap<QString, QString>::Iterator itrCustom = m_mapCustomReqHead.begin();
    while (itrCustom != m_mapCustomReqHead.end())
    {
        mapHead.insert(itrCustom.key(), itrCustom.value());
        ++itrCustom;
    }

    return mapHead;
}

bool NetClientInterface::changeBaseHead(const QString &p_strHeadKey, const QString &p_strHeadValue)
{
    QMutexLocker objLocker(m_pMutexParameter);

    if(m_mapCustomReqHead.contains(p_strHeadKey))
    {
        m_mapCustomReqHead[p_strHeadKey] = p_strHeadValue;
        return true;
    }

    if(m_mapReqHead.contains(p_strHeadKey))
    {
        m_mapReqHead[p_strHeadKey] = p_strHeadValue;
        return true;
    }

    m_mapReqHead.insert(p_strHeadKey, p_strHeadValue);
    return true;
}

const QMap<QString, QString> &NetClientInterface::getResHead()
{
    return m_mapResHead;
}

const QByteArray &NetClientInterface::getReqInfo()
{
    return m_bytReqInfo;
}

const QByteArray &NetClientInterface::getResHeadByte()
{
    return m_bytResHead;
}

const QByteArray& NetClientInterface::getResInfo()
{
    return m_bytResData;
}

const QByteArray& NetClientInterface::getResAll()
{
    return m_bytResAll;
}

bool NetClientInterface::parseURL(const QString &p_strURL, const bool p_bKeepAlive)
{
    QTime time;
    time.start();

    QString strServerAddr = p_strURL;
    QString strURLHead;
    qint32 nPort = 80;
    m_strURL = p_strURL;

    if(p_strURL.startsWith("https"))
    {
        if(!m_bIsSupportHttps)
        {
            m_strLastError = QString("this NetClientInterface.so dosn't compile whith https:%1").arg(p_strURL);
            qDebug()<<m_strLastError;
            return false;
        }
        m_bUseHttpsFlag = true;
    }
    else if(p_strURL.startsWith("http"))
    {
        m_bUseHttpsFlag = false;
    }

    if(m_bUseHttpsFlag)
    {
        strURLHead = "https://";
        nPort = 443;
    }
    else
    {
        strURLHead = "http://";
    }

    if(p_strURL.startsWith(strURLHead))
    {
        strServerAddr.replace(strURLHead, "");
        if(!strServerAddr.contains("/"))
        {
            m_strURLInterface = "/";
        }
        else
        {
            m_strURLInterface = strServerAddr.right(strServerAddr.length() - strServerAddr.indexOf("/"));
        }

        strServerAddr = strServerAddr.left(strServerAddr.indexOf("/"));
    }

    appendReqHead("Host", strServerAddr);

    if(strServerAddr.contains(":"))
    {
        QStringList strIpPortList = strServerAddr.split(":");
        if(strIpPortList.size() == 2)
        {
            int nSpilitPort = strIpPortList.at(1).toInt();
            if(nSpilitPort <= 0 || nSpilitPort > 65535)
            {
                m_strLastError = QString("parseURL port is out of limit : %1").arg(nSpilitPort);
                return false;
            }

            nPort = nSpilitPort;
        }

        strServerAddr = strIpPortList.at(0);
    }

    if(QHostAddress(strServerAddr).toIPv4Address()==0)
    {
        char ip[256];
        memset(ip, 0, 256);
        if(!domainNameToIp(strServerAddr.toStdString().c_str(),ip))
        {
            return false;
        }

        strServerAddr = (QString)ip;
    }

    if(p_bKeepAlive && m_nSocketFd > 0)
    {
        if(m_strServerIP == strServerAddr && m_nServerPort == nPort)
        {
            m_nParseURLTime = time.elapsed();
            return true;
        }
        else
        {
           closeConnect();
        }
    }

    m_strServerIP = strServerAddr;
    m_nServerPort = nPort;

    m_nParseURLTime = time.elapsed();
    return true;
}

void NetClientInterface::closeConnect()
{
    if(m_nSocketFd > 0)
    {
#ifdef USE_HTTPS
        if(m_bUseHttpsFlag && (SSL*)m_pSSL)
        {
            SSL_shutdown((SSL*)m_pSSL);
            SSL_free((SSL*)m_pSSL);
            m_pSSL = NULL;
        }
#endif
        close(m_nSocketFd);
        m_nSocketFd = -1;
    }
}

bool NetClientInterface::request(const QString &p_strMethod, const QString &p_strURL, const QByteArray &p_bytReqInfo, QByteArray &p_bytResInfo, const bool p_bKeepAlive)
{
    QMutexLocker objLocker(m_pMutexPerform);

    m_dStartDateTime = QDateTime::currentDateTime();

    clearEnv(true, !p_bKeepAlive);

    if(!parseURL(p_strURL, p_bKeepAlive))
    {
        goto failed;
    }

    if(!p_bKeepAlive || m_nSocketFd < 0)
    {
        if(!connectServer(m_strServerIP, m_nServerPort))
        {
            goto failed;
        }
    }

    if(!prepareReqInfo(p_strMethod, p_bytReqInfo, p_bKeepAlive))
    {
        goto failed;
    }

    if(!sendReq())
    {
        closeConnect();

        if(!p_bKeepAlive)
        {
            goto failed;
        }

        if(!connectServer(m_strServerIP, m_nServerPort))
        {
            goto failed;
        }

        if(!sendReq())
        {
            goto failed;
        }
    }

    if(!receiveRes())
    {
        goto failed;
    }

    goto success;

failed :
    clearEnv(false, true);
    return false;

success :
    p_bytResInfo = m_bytResData;
    clearEnv(false, !p_bKeepAlive);
    if(m_mapResHead.contains("connection"))
    {
        QString strConnection = m_mapResHead["connection"].toLower();
        if(strConnection == "close")
        {
            m_strLastError = QString("Receive Head Connection is: close, so close the socket");
            closeConnect();
        }
    }

    return true;
}

const NetHttpResult &NetClientInterface::getHttpRequestResult()
{
    return m_stHttpResult;
}

qint32 NetClientInterface::getParseURLTime()
{
    return m_nParseURLTime;
}

qint32 NetClientInterface::getConnectTime()
{
    return m_nConnectTime;
}

qint32 NetClientInterface::getReqTime()
{
    return m_nReqTime;
}

qint32 NetClientInterface::getResTime()
{
    return m_nResTime;
}

const QString &NetClientInterface::getLastError()
{
    return m_strLastError;
}

bool NetClientInterface::connectServer(const QString &p_strIP, const qint32 p_nPort)
{
    QTime time;
    time.start();

    if(p_strIP.isEmpty() || p_nPort <= 0 || p_nPort > 65535)
    {
        m_strLastError = QString("connect port is out of limit : %1").arg(p_nPort);
        return false;
    }

    int nfd = socket(AF_INET, SOCK_STREAM, 0);
    if(nfd == -1)
    {
        m_strLastError = QString("socket creat failed : %1").arg(strerror(errno));
        return false;
    }

    int nflag = 1;
    int nRet = 0;
    nRet = setsockopt(nfd, IPPROTO_TCP, TCP_NODELAY, (char *)&nflag, sizeof(nflag) );
    if (nRet < 0)
    {
        close(nfd);
        m_strLastError = QString("set socket opt TCP_NODELAY failed : %1").arg(strerror(errno));
        return false;
    }

    // set SO_LINGER so socket closes gracefully
    struct linger ling;
    ling.l_onoff = 1;
    ling.l_linger = 2;

#ifdef WIN32
    if (setsockopt(nfd, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling)) != 0)
    {
        close(nfd);
        m_strLastError = QString("set socket opt SO_LINGER failed : %1").arg(strerror(errno));
        return false;
    }

    const int iOpt = 1;
    const DWORD dwOpt = 1;
    if (setsockopt(nfd, SOL_SOCKET, SO_KEEPALIVE, (char *)&iOpt, sizeof(iOpt)) != 0)
    {
        close(nfd);
        m_strLastError = QString("set socket opt SO_KEEPALIVE failed : %1").arg(strerror(errno));
        return false;
    }

    tcp_keepalive  klive;
    klive.onoff = 1; // 启用保活
    klive.keepalivetime = 3 * 1000;
    klive.keepaliveinterval = 3 * 1000;
    nRet = WSAIoctl(nfd, SIO_KEEPALIVE_VALS,&klive,sizeof(tcp_keepalive),NULL,0,(unsigned long *)&dwOpt,0,NULL);
    if(nRet == SOCKET_ERROR)
    {
        close(nfd);
        m_strLastError = QString("set socket opt SIO_KEEPALIVE_VALS failed : %1").arg(strerror(errno));
        return false;
    }
#else
    if (setsockopt(nfd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) != 0)
    {
        close(nfd);
        m_strLastError = QString("set socket opt SO_LINGER failed : %1").arg(strerror(errno));
        return false;
    }

    int keepAlive = 1;//设定KeepAlive
    int keepIdle = 3;//开始首次KeepAlive探测前的TCP空闭时间
    int keepInterval = 3;//两次KeepAlive探测间的时间间隔
    int keepCount = 3;//判定断开前的KeepAlive探测次数

    if(setsockopt(nfd,SOL_SOCKET,SO_KEEPALIVE,(const char *)&keepAlive,sizeof(keepAlive)) == -1)
    {
        close(nfd);
        m_strLastError = QString("set socket opt SO_KEEPALIVE failed : %1").arg(strerror(errno));
        return false;
    }

    if(setsockopt(nfd,SOL_TCP,TCP_KEEPIDLE,(const char *)&keepIdle,sizeof(keepIdle)) == -1)
    {
        close(nfd);
        m_strLastError = QString("set socket opt TCP_KEEPIDLE failed : %1").arg(strerror(errno));
        return false;
    }

    if(setsockopt(nfd,SOL_TCP,TCP_KEEPINTVL,(const char *)&keepInterval,sizeof(keepInterval)) == -1)
    {
        close(nfd);
        m_strLastError = QString("set socket opt TCP_KEEPINTVL failed : %1").arg(strerror(errno));
        return false;
    }

    if(setsockopt(nfd,SOL_TCP,TCP_KEEPCNT,(const char *)&keepCount,sizeof(keepCount)) == -1)
    {
        close(nfd);
        m_strLastError = QString("set socket opt TCP_KEEPCNT failed : %1").arg(strerror(errno));
        return false;
    }
#endif //WIN32

    unsigned long argp = 1;
    #if (defined(WIN32) || defined(_WIN32_WCE))
         nRet = ioctlsocket(nfd, FIONBIO, (unsigned long*)&argp);
    #else
        nRet = ioctl(nfd, FIONBIO, (unsigned long*)&argp);
    #endif
    if(nRet==-1)
    {
        close(nfd);
        m_strLastError = QString("socket ioctl FIONBIO failed : %1").arg(strerror(errno));
        return false;
    }

    ADDRINFO Hints, *AddrInfo;
    memset(&Hints, 0, sizeof(Hints));
    Hints.ai_family = AF_INET;
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_flags = AI_NUMERICHOST;

    char szPort[32] = {0};
    sprintf(szPort, "%d", p_nPort);

    nRet = ::getaddrinfo(p_strIP.toStdString().c_str(),szPort,&Hints, &AddrInfo);
    if (nRet != 0)
    {
      close(nfd);
      m_strLastError = QString("socket ioctl FIONBIO failed : %1").arg(strerror(errno));
      return false;
    }

    ::connect(nfd, AddrInfo->ai_addr, AddrInfo->ai_addrlen);
    freeaddrinfo(AddrInfo);
    //超时等待连接是否成功
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(nfd, &rset);

    struct timeval objTimeout;
    struct timeval* pTimeout = NULL;
    if (m_nConnectTimeOutMs >= 0)
    {
        objTimeout.tv_sec = m_nConnectTimeOutMs/1000;
        objTimeout.tv_usec = (m_nConnectTimeOutMs % 1000) * 1000;
        pTimeout = &objTimeout;
    }

    #if (defined(WIN32) || defined(_WIN32_WCE))
        nRet = select(0, 0, &rset, 0, pTimeout);
    #else
        nRet = select(nfd + 1, 0, &rset, 0, pTimeout);
    #endif

    if ( nRet <= 0 )
    {
        close(nfd);
        m_strLastError = QString("connect %1:%2 timeout:%3 ms,error=%4").arg(p_strIP).arg(p_nPort).arg(m_nConnectTimeOutMs).arg(strerror(errno));
        return false;
    }

    int error = 0;
    int nLen = sizeof(error);

    #if (defined(WIN32) || defined(_WIN32_WCE))
        nRet = getsockopt(nfd,SOL_SOCKET,SO_ERROR,(char*)&error,&nLen);
    #else
        nRet = getsockopt(nfd,SOL_SOCKET,SO_ERROR,(char*)&error,(socklen_t *)(&nLen));
    #endif

    if(nRet < 0 || error != 0)
    {
        close(nfd);
        m_strLastError = QString("socket getsockopt SO_ERROR failed : maybe port unreachable,addr=%1:%2,error=%3").arg(p_strIP).arg(p_nPort).arg(strerror(error));
        return false;
    }

    //连接成功之后，再设置为阻塞模式
    argp = 0;
    #if (defined(WIN32) || defined(_WIN32_WCE))
        nRet = ioctlsocket(nfd, FIONBIO, (unsigned long*)&argp);
    #else
        nRet = ioctl(nfd, FIONBIO, (unsigned long*)&argp);
    #endif
    if(nRet==-1)
    {
        close(nfd);
        m_strLastError = QString("socket ioctl FIONBIO failed : %1").arg(strerror(errno));
        return false;
    }

    m_nSocketFd = nfd;
#ifdef USE_HTTPS
    if(m_bUseHttpsFlag)
    {
        if(!sslConnect(m_nSocketFd))
        {
            close(nfd);
            m_nSocketFd = -1;
            m_strLastError = QString("sslConnect failed");
            return false;
        }
    }
#endif
    m_nConnectTime = time.elapsed();

    return true;
}

bool NetClientInterface::sslGoableInit()
{
#ifdef USE_HTTPS
    QMutexLocker objLocker(&g_mutexSSLInit);

    if(g_pSSLCtx != NULL)
    {
        return true;
    }

#ifndef WIN32
    signal(SIGPIPE, sigpipe_handle);
#endif

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    g_pSSLCtx = (SSL_CTX*)SSL_CTX_new(SSLv23_client_method());
    if (g_pSSLCtx == NULL)
    {
        ERR_print_errors_fp(stdout);
        //m_strLastError = QString("sslGoableInit failed : g_pSSLCtx = null");
        return false;
    }

    init_locks();
#endif
    return true;
}

bool NetClientInterface::sslConnect(const int p_nfd)
{
    #ifdef USE_HTTPS
    if(g_pSSLCtx == NULL)
    {
        m_strLastError = QString("sslConnect failed , ssl goable is unint g_pSSLCtx = NULL");
        return false;
    }

    m_pSSL = (SSL*)SSL_new((SSL_CTX*)g_pSSLCtx);
    if(m_pSSL == NULL)
    {
        m_strLastError = QString("sslConnect failed , SSL_new return null pointer");
        return false;
    }

    SSL_set_fd((SSL*)m_pSSL, p_nfd);

    if (SSL_connect((SSL*)m_pSSL) == -1)
    {
        SSL_shutdown((SSL*)m_pSSL);
        SSL_free((SSL*)m_pSSL);
        m_pSSL = NULL;

        ERR_load_ERR_strings();
        ERR_load_crypto_strings();
        unsigned long ulErr = ERR_get_error(); // 获取错误号
        char szErrMsg[1024] = {0};
        ERR_error_string(ulErr,szErrMsg);

        m_strLastError = QString("sslConnect failed : %1").arg(szErrMsg);
        return false;
    }
    #endif
    return true;
}



bool NetClientInterface::prepareReqInfo(const QString &p_strMethod, const QByteArray &p_bytReqInfo, const bool p_bKeepAlive)
{
    m_bytReqInfo.clear();

    if(p_strMethod == "GET")
    {
        m_bytReqInfo.append("GET ");
    }
    else if(p_strMethod == "POST")
    {
        m_bytReqInfo.append("POST ");
    }
    else if(p_strMethod == "PUT")
    {
        m_bytReqInfo.append("PUT ");
    }
    else if(p_strMethod == "HEAD")
    {
        m_bytReqInfo.append("HEAD ");
    }
    else
    {
        m_bytReqInfo.append(QString("%1 ").arg(p_strMethod));
    }

    if(p_bytReqInfo.size() == 0)
    {
        m_mapReqHead.remove("Content-Type");
        m_mapCustomReqHead.remove("Content-Type");
    }
    else
    {
        if(!m_mapCustomReqHead.contains("Content-Type") && !m_mapReqHead.contains("Content-Type"))
        {
            appendReqHead("Content-Type","application/x-www-form-urlencoded");
        }
    }

    m_strMethod =   p_strMethod;
    m_bytReqInfo.append(m_strURLInterface);
    m_bytReqInfo.append(" HTTP/1.1\r\n");

    if(p_bKeepAlive)
    {
        appendReqHead("Connection", "Keep-Alive");
    }
    else
    {
        appendReqHead("Connection", "Close");
    }

    QMap<QString, QString>::Iterator itr = m_mapReqHead.begin();
    while (itr != m_mapReqHead.end())
    {
        QString strHead = QString("%1: %2\r\n").arg(itr.key()).arg(itr.value());
        m_bytReqInfo.append(strHead);
        itr++;
    }

    QMap<QString, QString>::Iterator itrCustom = m_mapCustomReqHead.begin();
    while (itrCustom != m_mapCustomReqHead.end())
    {
        QString strHead = QString("%1: %2\r\n").arg(itrCustom.key()).arg(itrCustom.value());
        m_bytReqInfo.append(strHead);
        itrCustom++;
    }

    if(p_bytReqInfo.size() > 0)
    {
        QString strHead = QString("Content-Length: %1\r\n").arg(p_bytReqInfo.length());
        m_bytReqInfo.append(strHead);
        m_bytReqInfo.append("\r\n");
        m_bytReqInfo.append(p_bytReqInfo);
    }
    else
    {
        m_bytReqInfo.append("\r\n");
    }

    return true;
}

bool NetClientInterface::sendReq()
{
    if(m_nSocketFd <= 0 || m_bytReqInfo.size() == 0)
    {
        return false;
    }

    if(!checkConnected())
    {
        return false;
    }

    qint32 nWaitReqTimeOutMs = m_nRequestTimeOutMs - m_dStartDateTime.msecsTo(QDateTime::currentDateTime());

    QTime time;
    time.start();

    bool bRet = base_send(m_nSocketFd, nWaitReqTimeOutMs);

    m_nReqTime = time.elapsed();

    if(!bRet)
    {
        return false;
    }

    return bRet;
}

bool NetClientInterface::checkConnected()
{
    char msg[1] = {0};
    int err = 0;
    unsigned long argp = 1;
    int nRet = 0;
#if (defined(WIN32) || defined(_WIN32_WCE))
    nRet = ioctlsocket(m_nSocketFd, FIONBIO, (unsigned long*)&argp);
#else
    nRet = ioctl(m_nSocketFd, FIONBIO, (unsigned long*)&argp);
#endif
    if(nRet==-1)
    {
        m_strLastError = QString("ioctlsocket FIONBIO 1 false:%1").arg(strerror(nRet));
        return false;
    }

#ifdef USE_HTTPS
    if(m_bUseHttpsFlag)
    {
        err = SSL_read((SSL*)m_pSSL, msg, sizeof(msg));
    }
    else
#endif
    {
        err = recv(m_nSocketFd, msg, sizeof(msg), MSG_PEEK);
    }

    if(err == 0 || (err < 0 && errno != EAGAIN && errno != 0))
    {
        m_strLastError = QString("connect lost,checkConnected false:recv return=%1,errno=%2,strerror=%3").arg(err).arg(errno).arg(strerror(errno));
        return false;
    }

    argp = 0;
#if (defined(WIN32) || defined(_WIN32_WCE))
    nRet = ioctlsocket(m_nSocketFd, FIONBIO, (unsigned long*)&argp);
#else
    nRet = ioctl(m_nSocketFd, FIONBIO, (unsigned long*)&argp);
#endif

    if(nRet == -1)
    {
        m_strLastError = QString("ioctlsocket FIONBIO 0 false:%1").arg(strerror(nRet));
        return false;
    }

    return true;
}

bool NetClientInterface::receiveRes()
{
    if(m_nSocketFd <= 0)
    {
        return false;
    }

    qint32 nWaitResTimeOutMs = m_nRequestTimeOutMs - m_dStartDateTime.msecsTo(QDateTime::currentDateTime());

    QTime time;
    time.start();

    bool bRet = base_recv(m_nSocketFd, nWaitResTimeOutMs);

    m_nResTime = time.elapsed();

    if(!bRet)
    {
        return false;
    }

    return bRet;
}

bool NetClientInterface::domainNameToIp( const char *domainName, char ip[65] )
{
    static struct in_addr addr;
    memset(&addr,0,sizeof(in_addr));

    struct hostent * phe = NULL;

#ifdef WIN32
    phe = gethostbyname( domainName );
#else
    unsigned size = 16384;// 16K should be large enough to handle everything pointed to by a struct hostent,from dcmtk
    char tmp[size];
    memset(tmp, 0, 16384);
    hostent buf;
    int err = 0;
    int nRet = gethostbyname_r( domainName, &buf, tmp, size, &phe, &err );
    if(nRet != 0)
    {
        m_strLastError = QString("dns parse failed domainName = %1,error=%2,ret=%3").arg(domainName).arg(strerror(err)).arg(nRet);
        return false;
    }
#endif

    if ( phe == 0 || phe->h_addr_list[ 0 ] == 0 )
    {
        memset(ip,0,65*sizeof(char));
        m_strLastError = QString("dns parse failed domainName = %1 : %2").arg(domainName).arg(strerror(errno));
        return false;
    }

    memcpy( &addr, phe->h_addr_list[ 0 ], sizeof( struct in_addr ) );
    strcpy(ip, inet_ntoa( addr ));

    return true;
}

bool NetClientInterface::base_send(int p_nfd, const int p_nTimeout)
{
    if(p_nfd < 0 || p_nTimeout < 0)
    {
        m_strLastError = QString("base_send p_nfd < 0 || p_nTimeout < 0");
        return false;
    }

    struct timeval  objTimeval;
    int nLen = sizeof(objTimeval);
    objTimeval.tv_sec = p_nTimeout/1000;
    objTimeval.tv_usec = p_nTimeout%1000*1000;
    setsockopt(p_nfd, SOL_SOCKET,SO_SNDTIMEO,(char*)&objTimeval,nLen);

    int nCount = 0;
    int nSendLength = m_bytReqInfo.size();
    int len = m_bytReqInfo.size();
    int nFlag = 0;
#ifdef Q_OS_WIN
    nFlag = 0;
#else
    nFlag = MSG_NOSIGNAL;
#endif

    QDateTime time = QDateTime::currentDateTime();
    QDateTime timeSendLimit = time;
    while (true)
    {
        if(nSendLength > RECEIVE_TEMP_SIZE)
        {
            nSendLength = RECEIVE_TEMP_SIZE;
        }

        int SendLen =0;
#ifdef USE_HTTPS
        if(m_bUseHttpsFlag)
        {
            SendLen = SSL_write((SSL*)m_pSSL, m_bytReqInfo.data() + nCount, nSendLength);
        }
        else
#endif
        {
            SendLen = ::send(p_nfd, m_bytReqInfo.data() + nCount, nSendLength, nFlag);
        }

        if (SendLen > 0)
        {
            nCount += SendLen;
            nSendLength = len - nCount;

            if(m_nSendBandwidthLimitBytes > 0)
            {
                qint32 nNeedMs = SendLen/m_nSendBandwidthLimitBytes;
                QDateTime objCurrentTime = QDateTime::currentDateTime();
                qint32 nUseTime = timeSendLimit.msecsTo(objCurrentTime);
                if(nUseTime < nNeedMs)
                {
                    QThread::msleep(nNeedMs - nUseTime);
                }
                timeSendLimit = objCurrentTime;
            }
        }
        else if(SendLen == -1 && errno == EINTR)
        {
            QThread::msleep(1);
        }
        else if(SendLen == -1 && errno != 0)
        {
            m_strLastError = QString("base_send send failed : errno = %1, errno string = %2").arg(errno).arg(strerror(errno));
            return false;
        }

        if (len == nCount)
        {
            return true;
        }

        if(time.msecsTo(QDateTime::currentDateTime()) > p_nTimeout)
        {
            m_strLastError = QString("base_send time out : %1").arg(m_nRequestTimeOutMs);
            return false;
        }
    }

    return false;
}

bool NetClientInterface::base_recv(int p_nfd, const int p_nTimeout)
{
    if(p_nfd < 0 || p_nTimeout < 0)
    {
        m_strLastError = QString("base_recv p_nfd < 0 || p_nTimeout < 0");
        return false;
    }

    struct timeval  objTimeval;
    int nLen = sizeof(objTimeval);
    objTimeval.tv_sec = p_nTimeout/1000;
    objTimeval.tv_usec = p_nTimeout%1000*1000;
    setsockopt(p_nfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&objTimeval,nLen);

    int nReveiveLength = 0;

    int nFlag = 0;
#ifdef Q_OS_WIN
    nFlag = 0;
#else
    nFlag = MSG_NOSIGNAL;
#endif

    QDateTime time = QDateTime::currentDateTime();

    bool bIsRecvData = false;

    while (true)
    {
        memset(szRecvDataTempLev1, 0, RECEIVE_TEMP_SIZE);

        int nLen = 0;
        if(!bIsRecvData)
        {
#ifdef USE_HTTPS
           if(m_bUseHttpsFlag)
           {
               nLen = SSL_read((SSL*)m_pSSL, szRecvDataTempLev1, RECEIVE_TEMP_SIZE);
           }
           else
#endif
           {
               nLen = ::recv(m_nSocketFd, szRecvDataTempLev1, RECEIVE_TEMP_SIZE, nFlag);
           }

           if(nLen > 0)
           {
               m_bytResAll.append(szRecvDataTempLev1, nLen);
               if(m_bytResAll.startsWith("HTTP/1.1 100 Continue\r\n\r\n"))
               {
                   m_bytResAll = m_bytResAll.replace("HTTP/1.1 100 Continue\r\n\r\n","");
               }

               if(m_bytResAll.contains("\r\n\r\n") && !bIsRecvData)
               {
                    m_bytResHead = m_bytResAll.left(m_bytResAll.indexOf("\r\n\r\n"));
                    m_bytResData = m_bytResAll.right(m_bytResAll.length() - m_bytResAll.indexOf("\r\n\r\n") - 4);
                    QString strAllHead = m_bytResHead;
                    QStringList strHeadList = strAllHead.split("\r\n");

                    for(int i = 0; i < strHeadList.size(); i++)
                    {
                       QString strRecvHead  = strHeadList.at(i);
                       if(!strRecvHead.contains(":"))
                       {
                          QStringList strResult = strRecvHead.split(" ");
                          if(strResult.size() >= 3)
                          {
                             m_stHttpResult.strProtocol = strResult.at(0).trimmed();
                             m_stHttpResult.nResultCode = strResult.at(1).trimmed().toInt();
                             m_stHttpResult.strResultMsg = strResult.at(2).trimmed();
                          }
                       }

                       QStringList strHead = strRecvHead.split(": ");
                       if(strHead.size() == 2)
                       {
                           m_mapResHead.insert(strHead.at(0).toLower(), strHead.at(1));
                       }
                    }

                    if(m_mapResHead.contains("content-length"))
                    {
                      bIsRecvData = true;
                      nReveiveLength = m_mapResHead["content-length"].toInt();
                      if(nReveiveLength == 0 || m_strMethod == "HEAD")
                      {
                           return true;
                      }

                      if(m_bytResData.length() == nReveiveLength)
                      {
                          return true;
                      }

                      m_bytResData.reserve(nReveiveLength + 1);
                      m_bytResAll.reserve(m_bytResHead.size() + nReveiveLength + 3);
                    }
                    else  if(m_strMethod  ==  "DELETE")
                    {
                        return true;
                    }
                    else if(m_mapResHead.contains("transfer-encoding") && m_mapResHead["transfer-encoding"] == "chunked")
                    {
                        qint32 nWaitResTimeOutMs = m_nRequestTimeOutMs - m_dStartDateTime.msecsTo(QDateTime::currentDateTime());
                        return base_recv_block(p_nfd, nWaitResTimeOutMs, m_bytResData);
                    }
               }
           }
        }
        else
        {
#ifdef USE_HTTPS
           if(m_bUseHttpsFlag)
           {
               nLen = SSL_read((SSL*)m_pSSL, szRecvDataTempLev1, RECEIVE_TEMP_SIZE);
           }
           else
#endif
           {
               nLen = ::recv(m_nSocketFd, szRecvDataTempLev1, RECEIVE_TEMP_SIZE, nFlag);
           }

           if(nLen > 0)
           {
                m_bytResData.append(szRecvDataTempLev1, nLen);
                m_bytResAll.append(szRecvDataTempLev1, nLen);
           }
        }

        if(m_bytResData.size() == nReveiveLength && bIsRecvData)
        {
            return true;
        }

        if(nLen == -1 && errno == EAGAIN)
        {
            QThread::msleep(1);
        }
        else if(nLen == -1 && errno != 0)
        {
            m_strLastError = QString("base_recv recv failed : errno = %1, errno string = %2").arg(errno).arg(strerror(errno));
            return false;
        }

        if(time.msecsTo(QDateTime::currentDateTime()) > p_nTimeout)
        {
            m_strLastError = QString("base_recv time out : %1").arg(m_nRequestTimeOutMs);
            return false;
        }
    }

    m_strLastError = QString("unknow error");
    return false;
}

bool NetClientInterface::base_recv_block(int p_nfd, const int p_nTimeout, QByteArray &p_bByteArrary)
{
    QDateTime time = QDateTime::currentDateTime();

        int nFlag = 0;
    #ifdef Q_OS_WIN
        nFlag = 0;
    #else
        nFlag = MSG_NOSIGNAL;
    #endif

        bool bBlockEnd = true;
        qint32 nBlockLength = 0;
        QByteArray bytValueTemp;
        QByteArray bytResultData;
        QByteArray bytRemainder = "\r\n";
        bytRemainder.append(p_bByteArrary);

        while(true)
        {
            if(bytRemainder.startsWith("\r\n"))
            {
                bytRemainder = bytRemainder.right(bytRemainder.length() - 2);
            }

            if(bytRemainder.contains("\r\n") && bBlockEnd)
            {
                bBlockEnd = false;

                QByteArray bytDataLength =  bytRemainder.left(bytRemainder.indexOf("\r\n"));
                if(bytDataLength.size() > 0)
                {
                    bool bRet = false;
                    nBlockLength = bytDataLength.toInt(&bRet, 16);
                    if(!bRet)
                    {
                        m_strLastError = QString("http msg block length error:%1").arg(bytDataLength.data());
                        return false;
                    }

                    bytValueTemp = bytRemainder.right(bytRemainder.size() - bytRemainder.indexOf("\r\n") - 2);
                    if(nBlockLength == 0)
                    {
                        if(bytValueTemp == "\r\n")
                        {
                            p_bByteArrary = bytResultData;
                            return true;
                        }

                        nBlockLength = 2;
                    }

                    if(bytValueTemp.size() >= nBlockLength)
                    {
                        bytResultData.append(bytValueTemp.mid(0, nBlockLength));
                        if(bytValueTemp.size() > nBlockLength)
                        {
                            bytRemainder = bytValueTemp.right(bytValueTemp.size() - nBlockLength);
                        }
                        else
                        {
                            bytRemainder.clear();
                        }
                        bytValueTemp.clear();
                        nBlockLength = 0;
                        bBlockEnd = true;
                        continue;
                    }

                    bBlockEnd = false;
                }
            }

            if(nBlockLength == 0)
            {
                nBlockLength = 2;
            }

            int nLen = 0;
            QByteArray bytReceiveBlock;
            while (true)
            {
                memset(szRecvDataTempLev1, 0, RECEIVE_TEMP_SIZE);
                if(nBlockLength > 0)
                {
        #ifdef USE_HTTPS
                   if(m_bUseHttpsFlag)
                   {
                       nLen = SSL_read((SSL*)m_pSSL, szRecvDataTempLev1, RECEIVE_TEMP_SIZE);
                   }
                   else
        #endif
                   {
                       nLen = ::recv(p_nfd, szRecvDataTempLev1, RECEIVE_TEMP_SIZE, nFlag);
                   }

                   if(nLen > 0)
                   {
                        bytValueTemp.append(szRecvDataTempLev1, nLen);
                        bytReceiveBlock.append(szRecvDataTempLev1, nLen);
                        m_bytResAll.append(szRecvDataTempLev1, nLen);
                   }
                }

                if(bytValueTemp.size() >= nBlockLength)
                {
                    bytRemainder.append(bytReceiveBlock);
                    bytValueTemp.clear();
                    bytReceiveBlock.clear();

                    nBlockLength = 0;
                    bBlockEnd = true;
                    break;
                }

                if(nLen == -1 && errno == EAGAIN)
                {
                    QThread::msleep(1);
                }
                else if(nLen == -1 && errno != 0)
                {
                    m_strLastError = QString("base_recv recv failed : errno = %1, errno string = %2").arg(errno).arg(strerror(errno));
                    return false;
                }

                if(time.msecsTo(QDateTime::currentDateTime()) > p_nTimeout)
                {
                    m_strLastError = QString("base_recv time out : %1").arg(m_nRequestTimeOutMs);
                    return false;
                }
            }
        }
}
