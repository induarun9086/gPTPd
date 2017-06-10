#include "gptpcmn.h"

#ifndef LOG_H
#define LOG_H

#define GPTP_LOG_DEST_CONSOLE 0
#define GPTP_LOG_DEST_SYSLOG  1

#define GPTP_LOG_DEBUG LOG_DEBUG

void gPTP_openLog(int dest);
void gPTP_logMsg(int prio, char* format, ...);
void gPTP_closeLog(void);

#endif
