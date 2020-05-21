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

