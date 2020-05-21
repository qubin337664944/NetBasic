#include "NetLog.h"
#include <QDebug>
#include <QDateTime>

CallAppLog NetLog::g_fnAppLogCallBack = NULL;
qint32     NetLog::g_nLogLevel = NET_LOG_LEVEL_TRACE;

NetLog::NetLog()
{
}

void NetLog::initCallBack(qint32 p_nLogLevel, CallAppLog p_fnAppLogCallBack)
{
    g_nLogLevel = p_nLogLevel;
    g_fnAppLogCallBack = p_fnAppLogCallBack;
}

void NetLog::writLog(qint32 p_nLogLevel, const QString &p_strLog)
{
    if(g_fnAppLogCallBack)
    {
        g_fnAppLogCallBack(p_nLogLevel, p_strLog);
    }

    if(PRINTLOG)
    {
        char szLeve[16];
        switch(p_nLogLevel)
        {
            case NET_LOG_LEVEL_ERROR:
            {
                strcpy(szLeve, "error:");
                break;
            }
            case NET_LOG_LEVEL_WORNING:
            {
                strcpy(szLeve, "warn:");
                break;
            }
            case NET_LOG_LEVEL_INFO:
            {
                strcpy(szLeve, "info:");
                break;
            }
            case NET_LOG_LEVEL_TRACE:
            {
                strcpy(szLeve, "trace:");
                break;
            }
            default:
                break;
        }

        QString strLog = QString("%1 %2 %3").arg(szLeve).arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")).arg(p_strLog);
        qDebug()<<strLog;
    }
}
