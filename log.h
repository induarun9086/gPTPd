#include "gptpcmn.h"

#ifndef GPTP_LOG_H
#define GPTP_LOG_H

#define GPTP_LOG_DEST_CONSOLE 0
#define GPTP_LOG_DEST_SYSLOG  1

#define GPTP_LOG_ERROR		LOG_ERR
#define GPTP_LOG_WARNING	LOG_WARNING
#define GPTP_LOG_NOTICE		LOG_NOTICE
#define GPTP_LOG_INFO		LOG_INFO
#define GPTP_LOG_DEBUG		LOG_DEBUG

#define GPTP_LOG_COLOR_ERROR    "\x1b[31;1m"
#define GPTP_LOG_COLOR_WARNING  "\x1b[35;1m"
#define GPTP_LOG_COLOR_NOTICE   "\x1b[34;1m"
#define GPTP_LOG_COLOR_INFO     "\x1b[33;1m"
#define GPTP_LOG_COLOR_DEBUG    "\x1b[30;1m"
#define GPTP_LOG_COLOR_RESET    "\x1b[0m"

#define GPTP_LOG_LVL_DEFAULT	GPTP_LOG_NOTICE

void gPTP_openLog(int dest, int logLvl);
void gPTP_logMsg(int prio, char* format, ...);
void gPTP_closeLog(void);

#endif
