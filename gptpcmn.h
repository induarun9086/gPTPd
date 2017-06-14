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
#include <time.h>

#ifndef GPTP_CMN_H
#define GPTP_CMN_H

/* Buffer sizes */
#define GPTP_TX_BUF_SIZE                  1024
#define GPTP_RX_BUF_SIZE                  4096
#define GPTP_IF_NAME_SIZE                 IFNAMSIZ
#define GPTP_CON_TS_BUF_SIZE              1024

/* List of timers */
#define GPTP_TIMER_DELAYREQ_RPT           0
#define GPTP_TIMER_DELAYREQ_TO            1
#define GPTP_NUM_TIMERS                   2
#define GPTP_TIMER_INVALID                GPTP_NUM_TIMERS

/* Event destinations */
#define GPTP_EVT_DEST_MASK                0xffff0000
#define GPTP_EVT_DEST_ALL                 0x00000000
#define GPTP_EVT_DEST_DM                  0x00010000
#define GPTP_EVT_DEST_BMC                 0x00020000
#define GPTP_EVT_DEST_MSTR                0x00040000
#define GPTP_EVT_DEST_SLV                 0x00080000

/* list of events */
#define GPTP_EVT_NONE                     (GPTP_EVT_DEST_ALL | 0x0)
#define GPTP_EVT_STATE_ENTRY              (GPTP_EVT_DEST_ALL | 0x1)
#define GPTP_EVT_STATE_EXIT               (GPTP_EVT_DEST_ALL | 0x2)

#define GPTP_EVT_DM_ENABLE                (GPTP_EVT_DEST_DM | 0x0)
#define GPTP_EVT_DM_PDELAY_REQ            (GPTP_EVT_DEST_DM | 0x1)
#define GPTP_EVT_DM_PDELAY_RESP           (GPTP_EVT_DEST_DM | 0x2)
#define GPTP_EVT_DM_PDELAY_RESP_FLWUP     (GPTP_EVT_DEST_DM | 0x3)
#define GPTP_EVT_DM_PDELAY_REQ_RPT        (GPTP_EVT_DEST_DM | 0x4)
#define GPTP_EVT_DM_PDELAY_REQ_TO         (GPTP_EVT_DEST_DM | 0x5)

/* GPTP types */
#define GPTP_ETHEDR_HDR_LEN               14
#define GPTP_HEADER_LEN                   34

#define GPTP_MSG_TYPE_PDELAY_REQ          0x02
#define GPTP_MSG_TYPE_PDELAY_RESP         0x03
#define GPTP_MSG_TYPE_PDELAY_RESP_FLWUP   0x0A

#define GPTP_BODY_OFFSET                  (GPTP_ETHEDR_HDR_LEN + GPTP_HEADER_LEN)

#define FALSE 0
#define TRUE  1

typedef unsigned long long int u64;
typedef unsigned int u32;
typedef unsigned short int u16;
typedef unsigned char u8;
typedef char bool;

#pragma pack(push, 1)

struct gPTPHdr {
	union tbf {
		struct th {
			union tb1 {
				u8 tsSpec;
				u8 msgType;
			} b1;
			union tb2 {
				u8 res1;
				u8 ptpVer;
			} b2;
			u16 msgLen;
			u8 domNo;
			u8 res2;
			u16 flags;
			u64 corrF;
			u32 res3;
			u8 srcPortIden[10];
			u16 seqNo;
			u8 ctrl;
			u8 logMsgInt;
		} f;
		unsigned char byte[GPTP_HEADER_LEN];
	} h;
};

struct gPTPts {
	struct ts {
		u32 msb;
		u16 lsb;
	} s;
	u32 ns;
};

#pragma pack(pop)

struct timer {
	u64 lastTS;
	u32 timeInterval;
	u32 timerEvt;
};

struct dmst {
	int state;
	u16 rxSeqNo;
	u16 txSeqNo;
	u32 delayReqInterval;
	u32 delayReqTimeOut;
	bool initalDelayReqSent;
};

struct gPTPd {
	int  sockfd;
	int  logLevel;
	bool daemonMode;
	
	u32   msrdDelay;

	char txBuf[GPTP_TX_BUF_SIZE];
	char rxBuf[GPTP_RX_BUF_SIZE];
	char ifName[GPTP_IF_NAME_SIZE];
	char tsBuf[GPTP_CON_TS_BUF_SIZE];

	struct iovec rxiov;
	struct msghdr txMsgHdr;
	struct msghdr rxMsgHdr;
	struct sockaddr_ll txSockAddress;
	struct sockaddr_ll rxSockAddress;

	struct gPTPts ts[6];
	struct timer timers[GPTP_NUM_TIMERS];
	struct dmst dm;
};


void gptp_initTxBuf(struct gPTPd* gPTPd);
void gptp_initRxBuf(struct gPTPd* gPTPd);

u64 gptp_getCurrMilliSecTS(void);
void gptp_startTimer(struct gPTPd* gPTPd, u32 timerId, u32 timeInterval, u32 timerEvt);
void gptp_stopTimer(struct gPTPd* gPTPd, u32 timerId);

#endif

