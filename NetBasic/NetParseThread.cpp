#include "NetParseThread.h"

NetParseThread::NetParseThread()
{
    m_nThreadId = 0;
    m_pobjMutex = NULL;
    m_pobjWaitCondition = NULL;
    bPacketCompleteCallBack = false;
}

void NetParseThread::init(const qint32 p_nThreadId, QMutex *p_pobjWaitMutex, QWaitCondition *p_pobjWaitCondition, const bool p_bPacketCompleteCallBack)
{
    m_nThreadId = p_nThreadId;
    m_pobjMutex = p_pobjWaitMutex;
    m_pobjWaitCondition = p_pobjWaitCondition;
    bPacketCompleteCallBack = p_bPacketCompleteCallBack;
}


void NetParseThread::run()
{

}
