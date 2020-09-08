#include "NetPacketBase.h"
#include "NetEnum.h"

NetPacketBase::NetPacketBase()
{
    m_bKeepAlive = false;
    m_nPacketID = 0;
    m_bIsReceiveEnd = false;
    m_bytReceiveAllDate.clear();
    m_nStep = NET_PARSE_STEP_NOSTART;
    m_nSendIndex = 0;
    m_pobjSSL  = NULL;
    m_nSissionID = 0;
    m_nIndex = 0;
    m_nTimeOutS = 30;
}

NetPacketBase::~NetPacketBase()
{
    m_bKeepAlive = false;
    m_nPacketID = 0;
    m_bytReceiveAllDate.clear();
    m_bIsReceiveEnd = false;
    m_nSocket = 0;

    m_nStep = NET_PARSE_STEP_NOSTART;

    m_bytSendAllDate.clear();
    m_nSendIndex = 0;

    m_pobjSSL = NULL;
}

void NetPacketBase::copyConnectInfo(NetPacketBase *p_pobjPacketBase)
{
    p_pobjPacketBase->m_pobjSSL = m_pobjSSL;
    p_pobjPacketBase->m_nSocket = m_nSocket;
    p_pobjPacketBase->m_nSissionID = m_nSissionID;
    p_pobjPacketBase->m_nIndex = m_nIndex;
}

