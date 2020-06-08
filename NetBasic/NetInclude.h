#ifndef NETINCLUDE_H
#define NETINCLUDE_H

#include <QCoreApplication>
#include <NetPacketBase.h>

typedef void (*CallAppLog)(qint32 p_nLogLevel, const QString& p_strLog);

typedef void (*CallAppReceivePacket)(NetPacketBase* p_pobjPacket, void* p_pMaster);


#define MAX_BUFFER_LEN 8192

#define PRINTLOG 1

#define EPOLL_SOCKET_MAX_SIZE 32768

#define KEEPALIVE_MAXSIZE 10000

#define RECEIVE_PACKET_TIMEOUT_S 30
#define SEND_PACKET_TIMEOUT_S 30

#define LISTEN_SIZE 30

#endif // NETINCLUDE_H
