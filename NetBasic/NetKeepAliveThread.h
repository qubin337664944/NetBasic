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
    }
};

class NetKeepAliveThread : public QThread
{
public:
    NetKeepAliveThread();

protected:
    virtual	void	run();

public:

    static bool init(qint32 p_nMaxQueueSize);

    static bool addAlive(const NetKeepAliveInfo& p_objNetKeepAliveInfo);
    static bool delAlive(const quint64 p_nSocket);
    static bool setCheckSend(const quint64 p_nSocket, const bool p_bCheck = true, const qint32 p_nSendTimeout = 30);
    static bool setCheckReceive(const quint64 p_nSocket, const bool p_bCheck = true, const qint32 p_nReceiveTimeout = 30);
    static bool modifySendTime(const quint64 p_nSocket);
    static bool modifyReceiveTime(const quint64 p_nSocket);

public:
    static NetKeepAliveInfo* g_vpobjNetKeepAliveInfo;
    static qint32   g_nNetKeepAliveInfoSize;
    static QMutex g_objKeepAliveMutex;
};

#endif // NETKEEPALIVETHREAD_H
