
#define DELAY_MSR_MODULE

#include "delaymsr.h"

void initDM(struct gPTPd* gPTPd)
{
	gPTPd->dm.state = DM_STATE_INIT;
}

void unintDM(struct gPTPd* gPTPd)
{
	
}

void dmHandleEvent(struct gPTPd* gPTPd, int evtId)
{
	gPTP_logMsg(GPTP_LOG_DEBUG, "gPTP dmHandleEvent st: %d evt: %d \n", gPTPd->dm.state, evtId);
	
	switch(gPTPd->dm.state) {

		case DM_STATE_INIT:
		case DM_STATE_IDLE:
	
			switch (evtId) {
				case GPTP_EVT_ONE_SEC_TICK:
					sendDelayReq(gPTPd);
					dmHandleStateChange(gPTPd, DM_STATE_DELAY_REQ_TX);
					break;
				default:
					break;
			}

			break;

		case DM_STATE_DELAY_REQ_TX:
	
			switch (evtId) {
				case GPTP_EVT_ONE_SEC_TICK:
					getDelayReqTS(gPTPd);
					break;
				default:
					break;
			}

			break;
	}
}

void dmHandleStateChange(struct gPTPd* gPTPd, int toState)
{
	gPTPd->dm.state = toState;
}

static void sendDelayReq(struct gPTPd* gPTPd)
{
	int txLen = 0;
	struct ethhdr *eh = (struct ethhdr *)gPTPd->txBuf;

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

	/* Get data offset */
	txLen = sizeof(struct ethhdr);

	/* Fill PTP payload data */
	gPTPd->txBuf[txLen++] = 0x02;
	gPTPd->txBuf[txLen++] = 0x02;

	gPTPd->txBuf[txLen++] = 0x00;
	gPTPd->txBuf[txLen++] = 0x36;

	gPTPd->txBuf[txLen++] = 0x00;
	gPTPd->txBuf[txLen++] = 0x00;

	gPTPd->txBuf[txLen++] = 0x00;
	gPTPd->txBuf[txLen++] = 0x00;

	gPTPd->txBuf[txLen++] = 0x00; gPTPd->txBuf[txLen++] = 0x00; gPTPd->txBuf[txLen++] = 0x00;
	gPTPd->txBuf[txLen++] = 0x00; gPTPd->txBuf[txLen++] = 0x00; gPTPd->txBuf[txLen++] = 0x00;
	gPTPd->txBuf[txLen++] = 0x00; gPTPd->txBuf[txLen++] = 0x00;

	gPTPd->txBuf[txLen++] = 0x00; gPTPd->txBuf[txLen++] = 0x00; gPTPd->txBuf[txLen++] = 0x00;
	gPTPd->txBuf[txLen++] = 0x00; 

	gPTPd->txBuf[txLen++] = 0x04; gPTPd->txBuf[txLen++] = 0xA3; gPTPd->txBuf[txLen++] = 0x16;
	gPTPd->txBuf[txLen++] = 0xFF; gPTPd->txBuf[txLen++] = 0xFE; gPTPd->txBuf[txLen++] = 0xAD;
	gPTPd->txBuf[txLen++] = 0x3A; gPTPd->txBuf[txLen++] = 0x33; gPTPd->txBuf[txLen++] = 0x00;
	gPTPd->txBuf[txLen++] = 0x01;

	gPTPd->seq++;
	gPTPd->txBuf[txLen++] = 0x00; gPTPd->txBuf[txLen++] = gPTPd->seq;

	gPTPd->txBuf[txLen++] = 0x05;
	gPTPd->txBuf[txLen++] = 0x7f;

	/* PTP body */
	txLen += 20;

	if (sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll)) < 0)
	    gPTP_logMsg(GPTP_LOG_DEBUG, "Send failed\n");	
}

static void getDelayReqTS(struct gPTPd* gPTPd)
{
	static short sk_events = POLLPRI;
	static short sk_revents = POLLPRI;
	int cnt = 0, res = 0, level, type;
	struct cmsghdr *cm;
	struct iovec iov = { gPTPd->rxBuf, GPTP_RX_BUF_SIZE };
	struct timespec *sw, *ts = NULL;
	struct pollfd pfd = { gPTPd->sockfd, sk_events, 0 };

	gPTPd->rxMsgHdr.msg_iov = &iov;
	gPTPd->rxMsgHdr.msg_iovlen = 1;

	res = poll(&pfd, 1, 1000);
	if (res < 1) {
		gPTP_logMsg(GPTP_LOG_DEBUG, "Poll failed %d\n", res);
	} else if (!(pfd.revents & sk_revents)) {
		gPTP_logMsg(GPTP_LOG_DEBUG, "poll for tx timestamp woke up on non ERR event");
	} else {
		gPTP_logMsg(GPTP_LOG_DEBUG, "Poll success\n");	
		cnt = recvmsg(gPTPd->sockfd, &gPTPd->rxMsgHdr, MSG_ERRQUEUE);
		if (cnt < 1)
			gPTP_logMsg(GPTP_LOG_DEBUG, "Recv failed\n");
		else
			for (cm = CMSG_FIRSTHDR(&gPTPd->rxMsgHdr); cm != NULL; cm = CMSG_NXTHDR(&gPTPd->rxMsgHdr, cm)) {
				level = cm->cmsg_level;
				type  = cm->cmsg_type;
				gPTP_logMsg(GPTP_LOG_DEBUG, "Lvl:%d Type: %d Size: %d (%d)\n", level, type, cm->cmsg_len, sizeof(struct timespec));
				if (SOL_SOCKET == level && SO_TIMESTAMPING == type) {
					if (cm->cmsg_len < sizeof(*ts) * 3) {
						printf("short SO_TIMESTAMPING message");
					} else {
						ts = (struct timespec *) CMSG_DATA(cm);
						gPTP_logMsg(GPTP_LOG_DEBUG, "sec: %d nsec: %d \n", ts[0].tv_sec, ts[0].tv_nsec);
						gPTP_logMsg(GPTP_LOG_DEBUG, "sec: %d nsec: %d \n", ts[1].tv_sec, ts[1].tv_nsec);
						gPTP_logMsg(GPTP_LOG_DEBUG, "sec: %d nsec: %d \n", ts[2].tv_sec, ts[2].tv_nsec);
					}
				}
				if (SOL_SOCKET == level && SO_TIMESTAMPNS == type) {
					if (cm->cmsg_len < sizeof(*sw)) {
						gPTP_logMsg(GPTP_LOG_DEBUG, "short SO_TIMESTAMPNS message");
					}
				}
			}
	}	
}

