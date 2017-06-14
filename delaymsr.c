
#define DELAY_MSR_MODULE

#include "delaymsr.h"

void initDM(struct gPTPd* gPTPd)
{
	gPTPd->dm.state = DM_STATE_INIT;
	gPTPd->dm.delayReqInterval = 1000;
	gPTPd->dm.delayReqTimeOut  = 8000;
	gPTPd->dm.initalDelayReqSent = FALSE;
}

void unintDM(struct gPTPd* gPTPd)
{
	
}

void dmHandleEvent(struct gPTPd* gPTPd, int evtId)
{
	gPTP_logMsg(GPTP_LOG_INFO, "gPTP dmHandleEvent st: %d evt: 0x%x \n", gPTPd->dm.state, evtId);
	
	switch(gPTPd->dm.state) {

		case DM_STATE_INIT:
			switch (evtId) {
				case GPTP_EVT_STATE_ENTRY:
					break;
				case GPTP_EVT_DM_ENABLE:
					gPTPd->dm.initalDelayReqSent = FALSE;
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
					if(gPTPd->dm.initalDelayReqSent == TRUE) {
						gptp_startTimer(gPTPd, GPTP_TIMER_DELAYREQ_RPT, gPTPd->dm.delayReqInterval, GPTP_EVT_DM_PDELAY_REQ_RPT);
						break;
					}
				case GPTP_EVT_DM_PDELAY_REQ_RPT:
					gptp_stopTimer(gPTPd, GPTP_TIMER_DELAYREQ_RPT);
					memset(&gPTPd->ts[0], 0, sizeof(struct gPTPts) * 4);
					sendDelayReq(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[0]);
					gPTPd->dm.initalDelayReqSent = TRUE;
					dmHandleStateChange(gPTPd, DM_STATE_DELAY_RESP_WAIT);
					break;
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd, &gPTPd->ts[4]);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[5]);
					sendDelayRespFlwUp(gPTPd);
					getTxTS(gPTPd, NULL);
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
					getTxTS(gPTPd, NULL);
					break;
				case GPTP_EVT_DM_PDELAY_REQ_TO:
					memset(&gPTPd->ts[0], 0, sizeof(struct gPTPts) * 4);
					sendDelayReq(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[0]);
					break;
				case GPTP_EVT_DM_PDELAY_RESP:
					gptp_stopTimer(gPTPd, GPTP_TIMER_DELAYREQ_TO);
					memcpy(&gPTPd->ts[1], &gPTPd->rxBuf[GPTP_BODY_OFFSET], sizeof(struct gPTPts));
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
					getTxTS(gPTPd, NULL);
					break;
				case GPTP_EVT_DM_PDELAY_RESP_FLWUP:
					getRxTS(gPTPd, NULL);
					memcpy(&gPTPd->ts[2], &gPTPd->rxBuf[GPTP_BODY_OFFSET], sizeof(struct gPTPts));
					gPTPd->msrdDelay = ((gPTPd->ts[3].ns - gPTPd->ts[0].ns) - (gPTPd->ts[2].ns - gPTPd->ts[1].ns)) / 2;
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

void dmHandleStateChange(struct gPTPd* gPTPd, int toState)
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
	gPTPd->dm.txSeqNo++;
	gh->h.f.seqNo = gPTPd->dm.txSeqNo;
	gh->h.f.b1.msgType = ((0x01 << 4) | GPTP_MSG_TYPE_PDELAY_REQ);

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x7f;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	txLen += 20;

	if ((err = sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll))) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "PDelayReq Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_INFO, ">>> PDelayReq (%d) sent\n", gPTPd->dm.txSeqNo);
}

static void sendDelayResp(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gh->h.f.seqNo = gPTPd->dm.rxSeqNo;
	gh->h.f.b1.msgType = ((0x01 << 4) | GPTP_MSG_TYPE_PDELAY_RESP);

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x7f;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memcpy(&gPTPd->txBuf[txLen], &gPTPd->ts[4], sizeof(struct gPTPts));
	txLen += 20;

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
	gh->h.f.seqNo = gPTPd->dm.rxSeqNo;
	gh->h.f.b1.msgType = ((0x01 << 4) | GPTP_MSG_TYPE_PDELAY_RESP_FLWUP);

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x7f;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memcpy(&gPTPd->txBuf[txLen], &gPTPd->ts[5], sizeof(struct gPTPts));
	txLen += 20;

	if ((err = sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll))) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "PDelayRespFlwUp Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_INFO, "~~~ PDelayRespFlwUp (%d) sent\n", gPTPd->dm.rxSeqNo);
}

static void getTxTS(struct gPTPd* gPTPd, struct gPTPts* ts)
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
										ts->ns = rts[i].tv_nsec;
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


static void getRxTS(struct gPTPd* gPTPd, struct gPTPts* ts)
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
							ts->ns = rts[i].tv_nsec;
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


