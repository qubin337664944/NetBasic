#ifndef NETLOG_H
#define NETLOG_H

#include "NetInclude.h"
#include "NetEnum.h"

class NetLog
{
public:
    NetLog();

    static void initCallBack(qint32 p_nLogLevel, CallAppLog p_fnAppLogCallBack);
    static void writLog(qint32 p_nLogLevel, const QString& p_strLog);

public:
    static CallAppLog g_fnAppLogCallBack;
    static qint32     g_nLogLevel;
};

#define NETLOG(_NETLOGLEVE_,_NETLOGINFO_)\
do{\
    if(_NETLOGLEVE_ <= NetLog::g_nLogLevel)\
    {\
        QString strLog = QString("%1 ,file[%2] line[%3]").arg(_NETLOGINFO_).arg(__FILE__).arg(__LINE__);\
        NetLog::writLog(_NETLOGLEVE_, strLog);\
    }\
}while(0)

#endif // NETLOG_H
