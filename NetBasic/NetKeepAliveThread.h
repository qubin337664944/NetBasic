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

    bool init(qint32 p_nMaxQueueSize, qint32 p_nProtocolType);

    bool addAlive(const NetKeepAliveInfo& p_objNetKeepAliveInfo, quint32& p_nSissionID, quint32& p_nIndex);
    bool delAlive(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex);
    bool setCheckSend(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex, const bool p_bCheck = true, const qint32 p_nSendTimeout = 30, void* p_objContxt = NULL);
    bool setCheckReceive(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex, const bool p_bCheck = true, const qint32 p_nReceiveTimeout = 30, void* p_objContxt = NULL);
    bool closeConnect(const quint64 p_nSocket, const quint32 p_nSissionID, const quint32 p_nIndex);

    bool lockIndex(const quint32 p_nIndex);
    bool tryLock(const quint32 p_nIndex, const qint32 p_nTryTimeOutMs = 5000);
    bool unlockIndex(const quint32 p_nIndex);

    bool lockIndexContext(const quint32 p_nIndex, const quint64 p_nSocket, const quint32 p_nSissionID, void* &p_pobjExtend);
    bool unlockIndexContext(const quint32 p_nIndex, void* &p_pobjExtend);

    bool getExtend(const quint32 p_nIndex, void* &p_pobjExtend);

public:
    NetKeepAliveInfo* m_vpobjNetKeepAliveInfo;
    quint32   m_nNetKeepAliveInfoSize;
    QMutex m_objKeepAliveMutex;
    quint32 m_nSissionID;
    qint32 m_nProtocolType;
};

#endif // NETKEEPALIVETHREAD_H
