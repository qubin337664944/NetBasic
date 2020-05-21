#ifndef NETENUM_H
#define NETENUM_H


enum EnumNetProtocol
{
    NET_PROTOCOL_HTTP,
    NET_PROTOCOL_HTTPS,
    NET_PROTOCOL_NONE
};

enum EnumNetSocketBase
{
    NET_SOCKET_IOCP,
    NET_SOCKET_EPOLL
};

enum EnumNetPostType
{
    NET_POST_ACCEPT,
    NET_POST_SEND,
    NET_POST_RECEIVE,
    NET_POST_ERROR
};

enum EnumNetPacketParseStep
{
    NET_PARSE_STEP_NOSTART,
    NET_PARSE_STEP_HEAD_OK,
    NET_PARSE_STEP_DATA_OK
};

enum EnumLogLevel
{
    NET_LOG_LEVEL_ERROR,
    NET_LOG_LEVEL_WORNING,
    NET_LOG_LEVEL_INFO,
    NET_LOG_LEVEL_TRACE
};


#endif // NETENUM_H
