#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdarg.h>

/* Buffer sizes */
#define GPTP_TX_BUF_SIZE                  1024
#define GPTP_RX_BUF_SIZE                  1024

/* list of events */
#define GPTP_EVT_NONE                     0
#define GPTP_EVT_ONE_SEC_TICK             1

/* Ethernet payload sizes */
#define GPTP_SYNCMSG_ETH_FRAME_SIZE       512

struct gPTPd {
	int sockfd;
	char txBuf[GPTP_TX_BUF_SIZE];
	char rxBuf[GPTP_RX_BUF_SIZE];
};
