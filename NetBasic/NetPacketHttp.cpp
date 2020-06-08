#include "NetPacketHttp.h"

NetPacketHttp::NetPacketHttp()
{
    m_strMethod.clear();
    m_strURL.clear();
    m_strProtocol = "HTTP/1.1";

    m_mapHttpHead.clear();

    m_bytHead.clear();
    m_bytData.clear();

    m_nContentLength = 0;

    m_nResultCode = 200;
    m_strResultMsg = "OK";
}

NetPacketHttp::~NetPacketHttp()
{
}
