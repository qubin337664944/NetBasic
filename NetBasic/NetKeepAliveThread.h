#ifndef NETKEEPALIVETHREAD_H
#define NETKEEPALIVETHREAD_H

#include <QThread>
#include <QDateTime>
#include <QMutex>

struct NetKeepAliveInfo
{
    quint64 nSocket;
    QDateTime objLastSendTime;
    QDateTime objLastReceiveTime;
    bool bIsAlive;
    bool bCheckSendTime;
    bool bCheckReceiveTime;

    qint32 nSendTimeOutS;
    qint32 nReceiveTimeOutS;

    quint32 nSissionID;

    void* pobjExtend;
    QMutex objExtendMutex;

    NetKeepAliveInfo()
    {
        init();
    }

    void init()
    {
        nSocket = 0;
        objLastSendTime = QDateTime::currentDateTime();
        objLastReceiveTime = objLastSendTime;
        bIsAlive = false;
        bCheckSendTime = false;
        bCheckReceiveTime = false;
        nSendTimeOutS = 30;
        nReceiveTimeOutS = 30;
        nSissionID = 0;
        pobjExtend = NULL;
    }
};

class NetKeepAliveThread : public QThread
{
public:
    NetKeepAliveThread();

protected:
    virtual	void	run();

public:

    static bool init(qint32 p_nMaxQueueSize, qint32 p_nProtocolType);

    static bool addAlive(const NetKeepAliveInfo& p_objNetKeepAliveInfo, quint32& p_nSissionID, quint32& p_nIndex);
    static bool delAlive(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex);
    static bool setCheckSend(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex, const bool p_bCheck = true, const qint32 p_nSendTimeout = 30, void* p_objContxt = NULL);
    static bool setCheckReceive(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex, const bool p_bCheck = true, const qint32 p_nReceiveTimeout = 30, void* p_objContxt = NULL);

    static bool lockIndex(const quint32 p_nIndex);
    static bool unlockIndex(const quint32 p_nIndex);

    static bool lockIndexContext(const quint32 p_nIndex, const quint64 p_nSocket, const quint32 p_nSissionID, void* &p_pobjExtend);
    static bool unlockIndexContext(const quint32 p_nIndex, void* &p_pobjExtend);

    static bool getExtend(const quint32 p_nIndex, void* &p_pobjExtend);

public:
    static NetKeepAliveInfo* g_vpobjNetKeepAliveInfo;
    static qint32   g_nNetKeepAliveInfoSize;
    static QMutex g_objKeepAliveMutex;
    static quint32 g_nSissionID;
    static qint32 g_nProtocolType;
};

#endif // NETKEEPALIVETHREAD_H
