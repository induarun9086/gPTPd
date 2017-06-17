
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
	eh->h_source[0] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[0];
	eh->h_source[1] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[1];
	eh->h_source[2] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[2];
	eh->h_source[3] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[3];
	eh->h_source[4] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[4];
	eh->h_source[5] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[5];

	/* Fill in Ethertype field */
	eh->h_proto = htons(ETH_P_1588);

	/* Fill common gPTP header fields */
	gh->h.f.b1.tsSpec      = (0x01 << 4);
	gh->h.f.b2.ptpVer      = 0x02;
	gh->h.f.srcPortIden[0] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[0];
	gh->h.f.srcPortIden[1] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[1];
	gh->h.f.srcPortIden[2] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[2];
	gh->h.f.srcPortIden[3] = 0xFF;
	gh->h.f.srcPortIden[4] = 0xFE;
	gh->h.f.srcPortIden[5] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[3];
	gh->h.f.srcPortIden[6] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[4];
	gh->h.f.srcPortIden[7] = ((u8 *)&gPTPd->if_mac.ifr_hwaddr.sa_data)[5];
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

void gptp_timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}

void gptp_copyTSFromBuf(struct timespec *ts, u8 *src)
{
	u8 *dest = (u8*)ts;
	struct gPTPTS *srcTS = (struct gPTPTS *)src;

	gPTP_logMsg(GPTP_LOG_DEBUG, "fb: src  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n",
		    src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7],
		    src[8], src[9], src[10], src[11]);

	if(sizeof(time_t) == 8) {
		ts->tv_sec  = (u64)(srcTS->s.lsb + ((u64)srcTS->s.msb << 32));
		ts->tv_nsec = srcTS->ns;
		gPTP_logMsg(GPTP_LOG_DEBUG, "fb: dest %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n",
		    	    dest[0], dest[1], dest[2], dest[3], dest[4], dest[5], dest[6], dest[7],
		    	    dest[8], dest[9], dest[10], dest[11]);
	} else {
		ts->tv_sec  = srcTS->s.lsb;
		ts->tv_nsec = srcTS->ns;
		gPTP_logMsg(GPTP_LOG_DEBUG, "fb: dest %02x %02x %02x %02x %02x %02x %02x %02x \n",
		   	    dest[0], dest[1], dest[2], dest[3], dest[4], dest[5], dest[6], dest[7]);
	}

	
}

void gptp_copyTSToBuf(struct timespec *ts, u8 *dest)
{
	u8 *src = (u8*)ts;
	struct gPTPTS *destTS = (struct gPTPTS *)dest;

	if(sizeof(time_t) == 8) {
		gPTP_logMsg(GPTP_LOG_DEBUG, "tb: src  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n",
		    	    src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7],
		    	    src[8], src[9], src[10], src[11]);
		destTS->s.msb = (u16)((u64)ts->tv_sec >> 32);
		destTS->s.lsb = (u32)ts->tv_sec;
		destTS->ns    = ts->tv_nsec;
	} else {

		gPTP_logMsg(GPTP_LOG_DEBUG, "tb: src %02x %02x %02x %02x %02x %02x %02x %02x \n",
		    	    src[0], src[1], src[2], src[3], src[4], src[5], src[6], src[7]);
		destTS->s.msb = 0;
		destTS->s.lsb = ts->tv_sec;
		destTS->ns    = ts->tv_nsec;
	}

	gPTP_logMsg(GPTP_LOG_DEBUG, "tb: dest  %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \n",
		    dest[0], dest[1], dest[2], dest[3], dest[4], dest[5], dest[6], dest[7],
		    dest[8], dest[9]);
}

u16 gptp_chgEndianess16(u16 val)
{
	return (((val & 0x00ff) << 8) | ((val & 0xff00) >> 8));
}

