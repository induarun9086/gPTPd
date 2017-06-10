#include "log.h"

int logDest = GPTP_LOG_DEST_CONSOLE;

void gPTP_openLog(int dest)
{
	logDest = dest;
	if(dest == GPTP_LOG_DEST_SYSLOG) {
		openlog("gPTPd", LOG_CONS, LOG_DAEMON);
	}
}

void gPTP_logMsg(int prio, char* format, ...)
{
	va_list args;
	va_start(args, format);
	if(logDest == GPTP_LOG_DEST_SYSLOG)
		vsyslog(prio, format, args);
	else
		vprintf(format, args);
	va_end(args);
}

void gPTP_closeLog(void)
{
	if(logDest == GPTP_LOG_DEST_SYSLOG)
		closelog();
}
