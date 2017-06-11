#include "gptpcmn.h"

#ifndef LOG_H
#define LOG_H

#define GPTP_LOG_DEST_CONSOLE 0
#define GPTP_LOG_DEST_SYSLOG  1

#define GPTP_LOG_ERROR		LOG_ERR
#define GPTP_LOG_WARNING	LOG_WARNING
#define GPTP_LOG_INFO		LOG_INFO
#define GPTP_LOG_DEBUG		LOG_DEBUG

#define GPTP_LOG_LVL_DEFAULT	GPTP_LOG_INFO

void gPTP_openLog(int dest, int logLvl);
void gPTP_logMsg(int prio, char* format, ...);
void gPTP_closeLog(void);

#endif
