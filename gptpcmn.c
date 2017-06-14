
#include "gptpcmn.h"
#include "log.h"

void gptp_initTxBuf(struct gPTPd* gPTPd)
{
	struct ethhdr *eh = (struct ethhdr *)gPTPd->txBuf;
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Initialize it */
	memset(gPTPd->txBuf, 0, GPTP_TX_BUF_SIZE);

	/* Fill in the Ethernet header */
	eh->h_dest[0] = 0x01;
	eh->h_dest[1] = 0x80;
	eh->h_dest[2] = 0xC2;
	eh->h_dest[3] = 0x00;
	eh->h_dest[4] = 0x00;
	eh->h_dest[5] = 0x0E;
	eh->h_source[0] = 0x04;
	eh->h_source[1] = 0xA3;
	eh->h_source[2] = 0x16;
	eh->h_source[3] = 0xAD;
	eh->h_source[4] = 0x3A;
	eh->h_source[5] = 0x33;

	/* Fill in Ethertype field */
	eh->h_proto = htons(ETH_P_1588);

	/* Fill common gPTP header fields */
	gh->h.f.b1.tsSpec      = (0x01 << 4);
	gh->h.f.b2.ptpVer      = 0x02;
	gh->h.f.srcPortIden[0] = 0x04;
	gh->h.f.srcPortIden[1] = 0xA3;
	gh->h.f.srcPortIden[2] = 0x16;
	gh->h.f.srcPortIden[3] = 0xFF;
	gh->h.f.srcPortIden[4] = 0xFE;
	gh->h.f.srcPortIden[5] = 0xAD;
	gh->h.f.srcPortIden[6] = 0x3A;
	gh->h.f.srcPortIden[7] = 0x33;
	gh->h.f.srcPortIden[8] = 0x00;
	gh->h.f.srcPortIden[9] = 0x01;
}

void gptp_initRxBuf(struct gPTPd* gPTPd)
{
	memset(gPTPd->rxBuf, 0, GPTP_RX_BUF_SIZE);
	memset(gPTPd->tsBuf, 0, GPTP_CON_TS_BUF_SIZE);
	gPTPd->rxMsgHdr.msg_iov = &gPTPd->rxiov;
	gPTPd->rxMsgHdr.msg_iovlen = 1;
	gPTPd->rxMsgHdr.msg_control=gPTPd->tsBuf;
	gPTPd->rxMsgHdr.msg_controllen=GPTP_CON_TS_BUF_SIZE;
	gPTPd->rxMsgHdr.msg_flags=0;
	gPTPd->rxMsgHdr.msg_name=&gPTPd->rxSockAddress;
	gPTPd->rxMsgHdr.msg_namelen=sizeof(struct sockaddr_ll);
}

u64 gptp_getCurrMilliSecTS(void)
{
	u64 currTickTS = 0;
	struct timespec ts = {0};

	clock_gettime(CLOCK_MONOTONIC, &ts);
	currTickTS = ((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000));

	return currTickTS;
}

void gptp_startTimer(struct gPTPd* gPTPd, u32 timerId, u32 timeInterval, u32 timerEvt)
{
	gPTPd->timers[timerId].timeInterval = timeInterval;
	gPTPd->timers[timerId].timerEvt = timerEvt;
	gPTPd->timers[timerId].lastTS = gptp_getCurrMilliSecTS();
}

void gptp_stopTimer(struct gPTPd* gPTPd, u32 timerId)
{
	gPTPd->timers[timerId].timeInterval = 0;
	gPTPd->timers[timerId].timerEvt = GPTP_TIMER_INVALID;
	gPTPd->timers[timerId].lastTS = 0;
}
