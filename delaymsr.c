
#define DELAY_MSR_MODULE

#include "delaymsr.h"

void initDM(struct gPTPd* gPTPd)
{
	gPTPd->dm.state = DM_STATE_INIT;
	gPTPd->dm.delayReqInterval = GPTP_PDELAY_REQ_INTERVAL;
	gPTPd->dm.delayReqTimeOut  = GPTP_PDELAY_REQ_TIMEOUT;
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
						gPTP_logMsg(GPTP_LOG_INFO, "@@@ t%d: %llu_%lu\n", (i+1), (u64)gPTPd->ts[i].tv_sec, gPTPd->ts[i].tv_nsec);
					gptp_timespec_diff(&gPTPd->ts[0],&gPTPd->ts[3],&diff[0]);
					gptp_timespec_diff(&gPTPd->ts[1],&gPTPd->ts[2],&diff[1]);
					if(diff[1].tv_nsec > diff[0].tv_nsec) {
						gPTP_logMsg(GPTP_LOG_INFO, "Negative delay ignored 0:%lu 1:%lu\n", diff[0].tv_nsec, diff[1].tv_nsec);
					} else {
						gptp_timespec_diff(&diff[1],&diff[0],&diff[2]);
						if(diff[2].tv_nsec > 50000) {
							gPTP_logMsg(GPTP_LOG_INFO, "Abnormally large delay ignored %lu\n", (diff[2].tv_nsec / 2));
						} else {
							gPTPd->msrdDelay = diff[2].tv_nsec/ 2;
							gPTP_logMsg(GPTP_LOG_NOTICE, "--------------------------> gPTP msrdDelay: %lu\n", gPTPd->msrdDelay);
						}
					}
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
	gh->h.f.b1.msgType = (GPTP_TRANSPORT_L2 | GPTP_MSG_TYPE_PDELAY_REQ);
	gh->h.f.flags = gptp_chgEndianess16(GPTP_FLAGS_NONE);

	gh->h.f.ctrl = GPTP_CONTROL_DELAY_ANNOUNCE;
	gh->h.f.logMsgInt = gptp_calcLogInterval(gPTPd->dm.delayReqInterval / 1000);

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memset(&gPTPd->txBuf[GPTP_BODY_OFFSET], 0, (GPTP_TX_BUF_SIZE - GPTP_BODY_OFFSET));
	txLen += (GPTP_TS_LEN + GPTP_PORT_IDEN_LEN);

	/* Insert length */
	gh->h.f.msgLen = gptp_chgEndianess16(txLen - sizeof(struct ethhdr));

	if ((err = sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll))) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "PDelayReq Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_NOTICE, ">>> PDelayReq (%d) sent\n", gPTPd->dm.txSeqNo++);
}

static void sendDelayResp(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gh->h.f.seqNo = gptp_chgEndianess16(gPTPd->dm.rxSeqNo);
	gh->h.f.b1.msgType = (GPTP_TRANSPORT_L2 | GPTP_MSG_TYPE_PDELAY_RESP);
	gh->h.f.flags = gptp_chgEndianess16(GPTP_FLAGS_TWO_STEP);

	gh->h.f.ctrl = GPTP_CONTROL_DELAY_ANNOUNCE;
	gh->h.f.logMsgInt = GPTP_LOG_MSG_INT_MAX;

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
		gPTP_logMsg(GPTP_LOG_NOTICE, "=== PDelayResp (%d) sent\n", gPTPd->dm.rxSeqNo);
}

static void sendDelayRespFlwUp(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gh->h.f.seqNo = gptp_chgEndianess16(gPTPd->dm.rxSeqNo);
	gh->h.f.b1.msgType = (GPTP_TRANSPORT_L2 | GPTP_MSG_TYPE_PDELAY_RESP_FLWUP);
	gh->h.f.flags = gptp_chgEndianess16(GPTP_FLAGS_NONE);

	gh->h.f.ctrl = GPTP_CONTROL_DELAY_ANNOUNCE;
	gh->h.f.logMsgInt = GPTP_LOG_MSG_INT_MAX;

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
		gPTP_logMsg(GPTP_LOG_NOTICE, "=== PDelayRespFlwUp (%d) sent\n", gPTPd->dm.rxSeqNo);
}




