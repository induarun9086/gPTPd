
#define DELAY_MSR_MODULE

#include "delaymsr.h"

void initDM(struct gPTPd* gPTPd)
{
	gPTPd->dm.state = DM_STATE_INIT;
	gPTPd->dm.delayReqInterval = 2000;
	gPTPd->dm.delayReqTimeOut  = 8000;
}

void unintDM(struct gPTPd* gPTPd)
{
	
}

void dmHandleEvent(struct gPTPd* gPTPd, int evtId)
{
	struct timespec diff[3];

	gPTP_logMsg(GPTP_LOG_INFO, "gPTP dmHandleEvent st: %d evt: 0x%x \n", gPTPd->dm.state, evtId);
	
	switch(gPTPd->dm.state) {

		case DM_STATE_INIT:
			switch (evtId) {
				case GPTP_EVT_STATE_ENTRY:
					break;
				case GPTP_EVT_DM_ENABLE:
					dmHandleStateChange(gPTPd, DM_STATE_IDLE);
					break;
				case GPTP_EVT_STATE_EXIT:
					break;
				default:
					break;
			}
			break;

		case DM_STATE_IDLE:
			switch (evtId) {
				case GPTP_EVT_STATE_ENTRY:
					gptp_startTimer(gPTPd, GPTP_TIMER_DELAYREQ_RPT, gPTPd->dm.delayReqInterval, GPTP_EVT_DM_PDELAY_REQ_RPT);
					break;
				case GPTP_EVT_DM_PDELAY_REQ_RPT:
					gptp_stopTimer(gPTPd, GPTP_TIMER_DELAYREQ_RPT);
					memset(&gPTPd->ts[0], 0, sizeof(struct timespec) * 4);
					sendDelayReq(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[0]);
					dmHandleStateChange(gPTPd, DM_STATE_DELAY_RESP_WAIT);
					break;
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd, &gPTPd->ts[4]);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[5]);
					sendDelayRespFlwUp(gPTPd);
					break;
				case GPTP_EVT_STATE_EXIT:
					break;
				default:
					break;
			}
			break;

		case DM_STATE_DELAY_RESP_WAIT:
			switch (evtId) {
				case GPTP_EVT_STATE_ENTRY:
					gptp_startTimer(gPTPd, GPTP_TIMER_DELAYREQ_TO, gPTPd->dm.delayReqTimeOut, GPTP_EVT_DM_PDELAY_REQ_TO);
					break;
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd, &gPTPd->ts[4]);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[5]);
					sendDelayRespFlwUp(gPTPd);
					break;
				case GPTP_EVT_DM_PDELAY_REQ_TO:
					memset(&gPTPd->ts[0], 0, sizeof(struct timespec) * 4);
					sendDelayReq(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[0]);
					break;
				case GPTP_EVT_DM_PDELAY_RESP:
					gptp_stopTimer(gPTPd, GPTP_TIMER_DELAYREQ_TO);
					gptp_copyTSFromBuf(&gPTPd->ts[1], &gPTPd->rxBuf[GPTP_BODY_OFFSET]);
					getRxTS(gPTPd, &gPTPd->ts[3]);
					dmHandleStateChange(gPTPd, DM_STATE_DELAY_RESP_FLWUP_WAIT);
					break;
				case GPTP_EVT_STATE_EXIT:
					break;
				default:
					break;
			}
			break;

		case DM_STATE_DELAY_RESP_FLWUP_WAIT:
			switch (evtId) {
				case GPTP_EVT_STATE_ENTRY:
					break;
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd, &gPTPd->ts[4]);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[5]);
					sendDelayRespFlwUp(gPTPd);
					break;
				case GPTP_EVT_DM_PDELAY_RESP_FLWUP:
					gptp_copyTSFromBuf(&gPTPd->ts[2], &gPTPd->rxBuf[GPTP_BODY_OFFSET]);
					for(int i = 0; i < 4; i++)
						gPTP_logMsg(GPTP_LOG_NOTICE, "@@@ t%d: %llu_%lu\n", (i+1), (u64)gPTPd->ts[i].tv_sec, gPTPd->ts[i].tv_nsec);
					gptp_timespec_diff(&gPTPd->ts[0],&gPTPd->ts[3],&diff[0]);
					gptp_timespec_diff(&gPTPd->ts[1],&gPTPd->ts[2],&diff[1]);
					gptp_timespec_diff(&diff[1],&diff[0],&diff[2]);
					gPTPd->msrdDelay = diff[2].tv_nsec/ 2;
					gPTP_logMsg(GPTP_LOG_NOTICE, "-------------------------->gPTP msrdDelay: %d\n", gPTPd->msrdDelay);
					dmHandleStateChange(gPTPd, DM_STATE_IDLE);
					break;
				case GPTP_EVT_STATE_EXIT:
					break;
				default:
					break;
			}
			break;

		default:
			break;
	}
}

static void dmHandleStateChange(struct gPTPd* gPTPd, int toState)
{
	dmHandleEvent(gPTPd, GPTP_EVT_STATE_EXIT);
	gPTPd->dm.state = toState;
	dmHandleEvent(gPTPd, GPTP_EVT_STATE_ENTRY);
}

static void sendDelayReq(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gh->h.f.seqNo = gptp_chgEndianess16(gPTPd->dm.txSeqNo);
	gh->h.f.b1.msgType = ((0x01 << 4) | GPTP_MSG_TYPE_PDELAY_REQ);
	gh->h.f.flags = gptp_chgEndianess16(0x0000);

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x01;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memset(&gPTPd->txBuf[GPTP_BODY_OFFSET], 0, (GPTP_TX_BUF_SIZE - GPTP_BODY_OFFSET));
	txLen += 20;

	/* Insert length */
	gh->h.f.msgLen = gptp_chgEndianess16(txLen - sizeof(struct ethhdr));

	if ((err = sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll))) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "PDelayReq Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_INFO, ">>> PDelayReq (%d) sent\n", gPTPd->dm.txSeqNo++);
}

static void sendDelayResp(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gh->h.f.seqNo = gptp_chgEndianess16(gPTPd->dm.rxSeqNo);
	gh->h.f.b1.msgType = ((0x01 << 4) | GPTP_MSG_TYPE_PDELAY_RESP);
	gh->h.f.flags = gptp_chgEndianess16(0x0001);

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x7f;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memset(&gPTPd->txBuf[GPTP_BODY_OFFSET], 0, (GPTP_TX_BUF_SIZE - GPTP_BODY_OFFSET));
	gptp_copyTSToBuf(&gPTPd->ts[4], &gPTPd->txBuf[txLen]);
	txLen += GPTP_TS_LEN;
	memcpy(&gPTPd->txBuf[txLen], &gPTPd->dm.reqPortIden[0], GPTP_PORT_IDEN_LEN);
	txLen += GPTP_PORT_IDEN_LEN;

	/* Insert length */
	gh->h.f.msgLen = gptp_chgEndianess16(txLen - sizeof(struct ethhdr));

	if ((err = sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll))) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "PDelayResp Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_INFO, "=== PDelayResp (%d) sent\n", gPTPd->dm.rxSeqNo);
}

static void sendDelayRespFlwUp(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gh->h.f.seqNo = gptp_chgEndianess16(gPTPd->dm.rxSeqNo);
	gh->h.f.b1.msgType = ((0x01 << 4) | GPTP_MSG_TYPE_PDELAY_RESP_FLWUP);
	gh->h.f.flags = gptp_chgEndianess16(0x0000);

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x7f;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memset(&gPTPd->txBuf[GPTP_BODY_OFFSET], 0, (GPTP_TX_BUF_SIZE - GPTP_BODY_OFFSET));
	gptp_copyTSToBuf(&gPTPd->ts[5], &gPTPd->txBuf[txLen]);
	txLen += GPTP_TS_LEN;
	memcpy(&gPTPd->txBuf[txLen], &gPTPd->dm.reqPortIden[0], GPTP_PORT_IDEN_LEN);
	txLen += GPTP_PORT_IDEN_LEN;

	/* Insert length */
	gh->h.f.msgLen = gptp_chgEndianess16(txLen - sizeof(struct ethhdr));

	if ((err = sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll))) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "PDelayRespFlwUp Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_INFO, "~~~ PDelayRespFlwUp (%d) sent\n", gPTPd->dm.rxSeqNo);
}

static void getTxTS(struct gPTPd* gPTPd, struct timespec* ts)
{
	static short sk_events = POLLPRI;
	static short sk_revents = POLLPRI;
	int cnt = 0, res = 0, level, type;
	struct cmsghdr *cm;
	struct timespec *sw, *rts = NULL;
	struct pollfd pfd = { gPTPd->sockfd, sk_events, 0 };

	do {
		res = poll(&pfd, 1, 1000);
		if (res < 1) {
			gPTP_logMsg(GPTP_LOG_DEBUG, "Poll failed %d\n", res);
			break;
		} else if (!(pfd.revents & sk_revents)) {
			gPTP_logMsg(GPTP_LOG_ERROR, "poll for tx timestamp woke up on non ERR event");
			break;
		} else {
			gPTP_logMsg(GPTP_LOG_DEBUG, "Poll success\n");
			gptp_initRxBuf(gPTPd);
			cnt = recvmsg(gPTPd->sockfd, &gPTPd->rxMsgHdr, MSG_ERRQUEUE);
			if (cnt < 1)
				gPTP_logMsg(GPTP_LOG_ERROR, "Recv failed\n");
			else
				gPTP_logMsg(GPTP_LOG_DEBUG, "TxTs msg len: %d\n", cnt);
				for (cm = CMSG_FIRSTHDR(&gPTPd->rxMsgHdr); cm != NULL; cm = CMSG_NXTHDR(&gPTPd->rxMsgHdr, cm)) {
					level = cm->cmsg_level;
					type  = cm->cmsg_type;
					gPTP_logMsg(GPTP_LOG_DEBUG, "Lvl:%d Type: %d Size: %d (%d)\n", level, type, cm->cmsg_len, sizeof(struct timespec));
					if (SOL_SOCKET == level && SO_TIMESTAMPING == type) {
						if (cm->cmsg_len < sizeof(*ts) * 3) {
							gPTP_logMsg(GPTP_LOG_DEBUG, "short SO_TIMESTAMPING message");
						} else {
							rts = (struct timespec *) CMSG_DATA(cm);
							for(int i = 0; i < 3; i++)
								if((rts[i].tv_sec != 0) || (rts[i].tv_nsec != 0)) {
									if(ts != NULL) {
                                                                                ts->tv_sec =  rts[i].tv_sec;
										ts->tv_nsec = rts[i].tv_nsec;
									}							
									gPTP_logMsg(GPTP_LOG_INFO, "TxTS: %d: sec: %d nsec: %d \n", i, rts[i].tv_sec, rts[i].tv_nsec);
								}
						}
					}
					if (SOL_SOCKET == level && SO_TIMESTAMPNS == type) {
						if (cm->cmsg_len < sizeof(*sw)) {
							gPTP_logMsg(GPTP_LOG_DEBUG, "short SO_TIMESTAMPNS message");
						}
					}
				}
		}
	} while(1);
}


static void getRxTS(struct gPTPd* gPTPd, struct timespec* ts)
{
	int level, type;
	struct cmsghdr *cm;
	struct timespec *sw, *rts = NULL;

	for (cm = CMSG_FIRSTHDR(&gPTPd->rxMsgHdr); cm != NULL; cm = CMSG_NXTHDR(&gPTPd->rxMsgHdr, cm)) {
		level = cm->cmsg_level;
		type  = cm->cmsg_type;
		gPTP_logMsg(GPTP_LOG_DEBUG, "Lvl:%d Type: %d Size: %d (%d)\n", level, type, cm->cmsg_len, sizeof(struct timespec));
		if (SOL_SOCKET == level && SO_TIMESTAMPING == type) {
			if (cm->cmsg_len < sizeof(*ts) * 3) {
				gPTP_logMsg(GPTP_LOG_DEBUG, "short SO_TIMESTAMPING message");
			} else {
				rts = (struct timespec *) CMSG_DATA(cm);
				for(int i = 0; i < 3; i++)
					if((rts[i].tv_sec != 0) || (rts[i].tv_nsec != 0)) {
						if(ts != NULL) {
							ts->tv_sec = rts[i].tv_sec;
							ts->tv_nsec = rts[i].tv_nsec;
						}
						gPTP_logMsg(GPTP_LOG_INFO, "RxTS: %d: sec: %d nsec: %d \n", i, rts[i].tv_sec, rts[i].tv_nsec);
					}
			}
		}
		if (SOL_SOCKET == level && SO_TIMESTAMPNS == type) {
			if (cm->cmsg_len < sizeof(*sw)) {
				gPTP_logMsg(GPTP_LOG_DEBUG, "short SO_TIMESTAMPNS message");
			}
		}
	}
}




