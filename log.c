#include "log.h"

int logStTS  = 0;
int logDest  = GPTP_LOG_DEST_CONSOLE;
int logLevel = GPTP_LOG_LVL_DEFAULT;

void gPTP_openLog(int dest, int logLvl)
{
	logDest  = dest;
	logLevel = logLvl;
	logStTS  = gptp_getCurrMilliSecTS();
	if(dest == GPTP_LOG_DEST_SYSLOG) {
		openlog("gPTPd", LOG_CONS, LOG_DAEMON);
	}
}

void gPTP_logMsg(int prio, char* format, ...)
{
	va_list args;
	va_start(args, format);
	if(prio <= logLevel) {
		if(logDest == GPTP_LOG_DEST_SYSLOG)
			vsyslog(prio, format, args);
		else {
			switch(prio) {
				case GPTP_LOG_ERROR:
				printf(GPTP_LOG_COLOR_ERROR);
				break;
				case GPTP_LOG_WARNING:
				printf(GPTP_LOG_COLOR_WARNING);
				break;
				case GPTP_LOG_NOTICE:
				printf(GPTP_LOG_COLOR_NOTICE);
				break;
				case GPTP_LOG_INFO:
				printf(GPTP_LOG_COLOR_INFO);
				break;
				case GPTP_LOG_DEBUG:
				printf(GPTP_LOG_COLOR_DEBUG);
				break;
				default:
				break;
			}
			printf("[%012llu] gPTPd: ", gptp_getCurrMilliSecTS()-logStTS);
			vprintf(format, args);
			printf(GPTP_LOG_COLOR_RESET);
		}
	}
	va_end(args);
}

void gPTP_closeLog(void)
{
	if(logDest == GPTP_LOG_DEST_SYSLOG)
		closelog();
}
