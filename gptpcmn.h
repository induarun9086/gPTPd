#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <linux/if_packet.h>
#include <netinet/ether.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdarg.h>
#include <poll.h>

#ifndef GPTP_CMN_H
#define GPTP_CMN_H

/* Buffer sizes */
#define GPTP_TX_BUF_SIZE                  1024
#define GPTP_RX_BUF_SIZE                  1024
#define GPTP_IF_NAME_SIZE                 IFNAMSIZ
#define GPTP_CON_TS_BUF_SIZE              512

/* list of events */
#define GPTP_EVT_NONE                     0
#define GPTP_EVT_ONE_SEC_TICK             1

#define FALSE 0
#define TRUE  1

typedef char bool;

struct dmst {
	int state;
};

struct gPTPd {
	int  sockfd;
	int  seq;
	bool daemonMode;

	char txBuf[GPTP_TX_BUF_SIZE];
	char rxBuf[GPTP_RX_BUF_SIZE];
	char ifName[GPTP_IF_NAME_SIZE];
	char tsBuf[GPTP_CON_TS_BUF_SIZE];

	struct msghdr txMsgHdr;
	struct msghdr rxMsgHdr;
	struct sockaddr_ll txSockAddress;
	struct sockaddr_ll rxSockAddress;

	struct dmst dm;
};

#endif

