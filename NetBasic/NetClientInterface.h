#ifndef NETCLIENTINTERFACE_H
#define NETCLIENTINTERFACE_H

#include <QMap>
#include <QByteArray>
#include <QString>
#include <QMutex>
#include <QDateTime>

#define RECEIVE_TEMP_SIZE   65536
#define USE_HTTPS

struct NetHttpResult
{
    QString strProtocol;
    qint32  nResultCode;
    QString strResultMsg;
};

class NetClientInterface
{
public:
    NetClientInterface();
    ~NetClientInterface();

    static bool sslGoableInit();

    bool setUseHttps(bool p_bUseHttpsFlag = false);
    void setSendBandwidthLimit(const qint32 p_nSendBandwidthLimitBits = 0);
    void setTimeOut(const qint32 p_nConnectTimeOutMs = 5000, const qint32 p_nRequestTimeOutMs = 30000);

    const QMap<QString, QString> getReqHead();
    bool changeBaseHead(const QString& p_strHeadKey, const QString& p_strHeadValue);
    bool appendReqHead(const QString& p_strHeadKey, const QString& p_strHeadValue);
    void clearReqHead();

    const QMap<QString, QString> &getResHead();

    bool request(const QString& p_strMethod, const QString& p_strURL, const QByteArray& p_bytReqInfo, QByteArray& p_bytResInfo, const bool p_bKeepAlive = false);
    const NetHttpResult& getHttpRequestResult();

public:
    qint32 getParseURLTime();
    qint32 getConnectTime();
    qint32 getReqTime();
    qint32 getResTime();

    const QByteArray& getReqInfo();
    const QByteArray& getResHeadByte();
    const QByteArray& getResInfo();
    const QByteArray& getResAll();

    const QString& getLastError();

private:
    bool parseURL(const QString& p_strURL, const bool p_bKeepAlive = false);
    bool connectServer(const QString& p_strIP, const qint32 p_nPort);
    bool prepareReqInfo(const QString &p_strMethod, const QByteArray &p_bytReqInfo, const bool p_bKeepAlive = false);
    bool sendReq();
    bool checkConnected();
    bool receiveRes();
    bool domainNameToIp( const char *domainName, char ip[65] );

    bool base_send(int p_nfd, const int p_nTimeout);
    bool base_recv(int p_nfd, const int p_nTimeout);

    bool base_recv_block(int p_nfd, const int p_nTimeout, QByteArray& p_bByteArrary);

    void initEnv();
    void clearEnv(bool p_bIsClearDebugInfo = true, const bool p_bIsCloseConnect = false);

    void closeConnect();

    bool sslConnect(const int p_nfd);


public:
    static    void       *g_pSSLCtx;
    static    QMutex         g_mutexSSLInit;

private:
    void                     *m_pSSL;
    bool                    m_bIsSupportHttps;
    qint32                 m_nSendBandwidthLimitBytes;
    bool                    m_bUseHttpsFlag;
    bool                    m_bKeepAlive;

    QMap<QString, QString>  m_mapReqHead;
    QMap<QString, QString>  m_mapCustomReqHead;
    QMap<QString, QString>  m_mapResHead;

    QString                   m_strMethod;
    QByteArray              m_bytReqInfo;

    QByteArray              m_bytResHead;
    QByteArray              m_bytResData;
    QByteArray              m_bytResAll;

    NetHttpResult       m_stHttpResult;

    QString                 m_strURL;
    QString                 m_strURLInterface;
    QString                 m_strServerIP;
    qint32                  m_nServerPort;

    qint32                  m_nConnectTimeOutMs;
    qint32                  m_nRequestTimeOutMs;

    QDateTime               m_dStartDateTime;

    int                     m_nSocketFd;

    char                    szRecvDataTempLev1[RECEIVE_TEMP_SIZE];

    qint32                  m_nParseURLTime;
    qint32                  m_nConnectTime;
    qint32                  m_nReqTime;
    qint32                  m_nResTime;

    QString                 m_strLastError;

    QMutex*               m_pMutexParameter;
    QMutex*               m_pMutexPerform;
};

#endif // NETCLIENTINTERFACE_H
