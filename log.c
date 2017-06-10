#include "log.h"

void gPTP_openLog(void)
{
	openlog("gPTPd", LOG_CONS, LOG_DAEMON);
}

void gPTP_logMsg(int prio, char* format, ...)
{
	va_list args;
	va_start(args, format);
	vsyslog(prio, format, args);
	va_end(args);
}

void gPTP_closeLog(void)
{
	closelog();
}
