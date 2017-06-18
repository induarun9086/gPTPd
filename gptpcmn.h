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
#define GPTP_TIMER_ANNOUNCE_RPT           2
#define GPTP_TIMER_ANNOUNCE_TO            3
#define GPTP_TIMER_SYNC_RPT               4
#define GPTP_TIMER_SYNC_TO                5
#define GPTP_NUM_TIMERS                   6
#define GPTP_TIMER_INVALID                GPTP_NUM_TIMERS

/* Event destinations */
#define GPTP_EVT_DEST_MASK                0xffff0000
#define GPTP_EVT_DEST_ALL                 0x00000000
#define GPTP_EVT_DEST_DM                  0x00010000
#define GPTP_EVT_DEST_BMC                 0x00020000
#define GPTP_EVT_DEST_CS                  0x00040000

/* list of common events */
#define GPTP_EVT_NONE                     (GPTP_EVT_DEST_ALL | 0x0)
#define GPTP_EVT_STATE_ENTRY              (GPTP_EVT_DEST_ALL | 0x1)
#define GPTP_EVT_STATE_EXIT               (GPTP_EVT_DEST_ALL | 0x2)

/* list of delay msr events */
#define GPTP_EVT_DM_ENABLE                (GPTP_EVT_DEST_DM | 0x0)
#define GPTP_EVT_DM_PDELAY_REQ            (GPTP_EVT_DEST_DM | 0x1)
#define GPTP_EVT_DM_PDELAY_RESP           (GPTP_EVT_DEST_DM | 0x2)
#define GPTP_EVT_DM_PDELAY_RESP_FLWUP     (GPTP_EVT_DEST_DM | 0x3)
#define GPTP_EVT_DM_PDELAY_REQ_RPT        (GPTP_EVT_DEST_DM | 0x4)
#define GPTP_EVT_DM_PDELAY_REQ_TO         (GPTP_EVT_DEST_DM | 0x5)

/* list of best master clock events */
#define GPTP_EVT_BMC_ENABLE               (GPTP_EVT_DEST_BMC | 0x0)
#define GPTP_EVT_BMC_ANNOUNCE_RPT         (GPTP_EVT_DEST_BMC | 0x4)
#define GPTP_EVT_BMC_ANNOUNCE_TO          (GPTP_EVT_DEST_BMC | 0x5)
#define GPTP_EVT_BMC_ANNOUNCE_MSG         (GPTP_EVT_DEST_BMC | 0x6)

/* list of best master clock events */
#define GPTP_EVT_CS_ENABLE                (GPTP_EVT_DEST_CS | 0x0)
#define GPTP_EVT_CS_SYNC_RPT              (GPTP_EVT_DEST_CS | 0x4)
#define GPTP_EVT_CS_SYNC_TO               (GPTP_EVT_DEST_CS | 0x5)
#define GPTP_EVT_CS_SYNC_MSG              (GPTP_EVT_DEST_CS | 0x6)
#define GPTP_EVT_CS_SYNC_FLWUP_MSG        (GPTP_EVT_DEST_CS | 0x7)

/* GPTP message types */
#define GPTP_MSG_TYPE_SYNC                0x00
#define GPTP_MSG_TYPE_PDELAY_REQ          0x02
#define GPTP_MSG_TYPE_PDELAY_RESP         0x03
#define GPTP_MSG_TYPE_SYNC_FLWUP          0x08
#define GPTP_MSG_TYPE_PDELAY_RESP_FLWUP   0x0A
#define GPTP_MSG_TYPE_ANNOUNCE            0x0B
#define GPTP_MSG_TYPE_SIGNAL              0x0C

/* GPTP constants */
#define GPTP_VERSION_NO                   0x02
#define GPTP_TRANSPORT_L2                 0x10

#define GPTP_CONTROL_SYNC                 0x00
#define GPTP_CONTROL_SYNC_FLWUP           0x02
#define GPTP_CONTROL_DELAY_ANNOUNCE       0x05

#define GPTP_TLV_TYPE_ORG_EXT             0x0003
#define GPTP_TLV_TYPE_PATH_TRACE          0x0008

#define GPTP_LOG_MSG_INT_NOCHANGE         0x80
#define GPTP_LOG_MSG_INT_INIT             0x7E
#define GPTP_LOG_MSG_INT_MAX              0x7F

/* GPTP Clock values */
#define GPTP_DEFAULT_CLOCK_PRIO1          250
#define GPTP_DEFAULT_CLOCK_CLASS          248
#define GPTP_DEFAULT_CLOCK_ACCURACY       254
#define GPTP_DEFAULT_OFFSET_VARIANCE      0x4100
#define GPTP_DEFAULT_CLOCK_PRIO2          250
#define GPTP_DEFAULT_STEPS_REMOVED        0

/* Clock types */
#define GPTP_CLOCK_TYPE_INT_OSC           0xA0

/* GPTP flags */
#define GPTP_FLAGS_NONE                   0x0000
#define GPTP_FLAGS_TWO_STEP               0x0001

/* GPTP sizes */
#define GPTP_TS_LEN                       10
#define GPTP_PORT_IDEN_LEN                10
#define GPTP_CLOCK_IDEN_LEN               8
#define GPTP_ETHEDR_HDR_LEN               14
#define GPTP_HEADER_LEN                   34
#define GPTP_BODY_OFFSET                  (GPTP_ETHEDR_HDR_LEN + GPTP_HEADER_LEN)

/* Default timeouts */
#define GPTP_PDELAY_REQ_INTERVAL          2000
#define GPTP_PDELAY_REQ_TIMEOUT           8000
#define GPTP_ANNOUNCE_INTERVAL            2000
#define GPTP_ANNOUNCE_TIMEOUT             8000
#define GPTP_SYNC_INTERVAL                2000
#define GPTP_SYNC_TIMEOUT                 32000

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
			u8 srcPortIden[GPTP_PORT_IDEN_LEN];
			u16 seqNo;
			u8 ctrl;
			u8 logMsgInt;
		} f;
		unsigned char byte[GPTP_HEADER_LEN];
	} h;
};

struct gPTPTS {
	struct s48b {
		u16 msb;
		u32 lsb;
	} s;
	u32 ns;
};

struct gPTPPrioVec {
	u8  res1[10];
	u16 currUTCOff;
	u8  res2;
	u8  prio1;
	struct clockQual {
		u8  clockClass;
		u8  clockAccuracy;
		u16 offsetScaledLogVariance;
	}clockQual;
	u8  prio2;
	u8  iden[GPTP_PORT_IDEN_LEN];
	u16 stepsRem;
	u8  clockSrc;
};

struct gPTPscaledNs {
	u8 ns[12];
};

struct gPTPtlv {
	u16 type;
	u16 len;
};

struct gPTPOrgExt {
	u8 orgType[3];
	u8 orgSubType[3];
	u32 csRateOff;
	u16 gmTBInd;
	struct gPTPscaledNs lastGMPhChg;
	u32 gmFreqChg;
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
	u8   reqPortIden[GPTP_PORT_IDEN_LEN];
};

struct bmcst {
	int state;
	u16 annoSeqNo;
	u32 announceInterval;
	u32 announceTimeout;
	struct gPTPPrioVec portPrio;
	struct gPTPPrioVec gmPrio;
};

struct csst {
	int state;
	u16 syncSeqNo;
	u32 syncInterval;
	u32 syncTimeout;
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

	struct ifreq if_idx;
	struct ifreq if_mac;
	struct ifreq if_hw;

	struct iovec rxiov;
	struct msghdr txMsgHdr;
	struct msghdr rxMsgHdr;
	struct sockaddr_ll txSockAddress;
	struct sockaddr_ll rxSockAddress;

	struct timespec ts[7];
	struct timer timers[GPTP_NUM_TIMERS];
	struct dmst dm;
	struct bmcst bmc;
	struct csst cs;
};


void gptp_initTxBuf(struct gPTPd* gPTPd);
void gptp_initRxBuf(struct gPTPd* gPTPd);

u64 gptp_getCurrMilliSecTS(void);
void gptp_startTimer(struct gPTPd* gPTPd, u32 timerId, u32 timeInterval, u32 timerEvt);
void gptp_stopTimer(struct gPTPd* gPTPd, u32 timerId);
void gptp_resetTimer(struct gPTPd* gPTPd, u32 timerId);

void gptp_timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result);
void gptp_copyTSFromBuf(struct timespec *ts, u8 *src);
void gptp_copyTSToBuf(struct timespec *ts, u8 *dest);

u16 gptp_chgEndianess16(u16 val);
u8 gptp_calcLogInterval(u32 time);

void getTxTS(struct gPTPd* gPTPd, struct timespec* ts);
void getRxTS(struct gPTPd* gPTPd, struct timespec* ts);

#endif

