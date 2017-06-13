
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
	gPTP_logMsg(GPTP_LOG_DEBUG, "gPTP dmHandleEvent st: %d evt: 0x%x \n", gPTPd->dm.state, evtId);
	
	switch(gPTPd->dm.state) {

		case DM_STATE_INIT:
			switch (evtId) {
				case GPTP_EVT_DM_ENABLE:
					dmHandleStateChange(gPTPd, DM_STATE_IDLE);
					break;
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd, &gPTPd->ts[1]);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[2]);
					sendDelayRespFlwUp(gPTPd);
					getTxTS(gPTPd, NULL);
					break;
				default:
					break;
			}
			break;

		case DM_STATE_IDLE:
			switch (evtId) {
				case GPTP_EVT_ONE_SEC_TICK:
					memset(&gPTPd->ts[0], 0, sizeof(struct gPTPts) * 4);
					sendDelayReq(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[0]);
					dmHandleStateChange(gPTPd, DM_STATE_DELAY_RESP_WAIT);
					break;
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd, &gPTPd->ts[1]);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[2]);
					sendDelayRespFlwUp(gPTPd);
					getTxTS(gPTPd, NULL);
					break;
				default:
					break;
			}
			break;

		case DM_STATE_DELAY_RESP_WAIT:
			switch (evtId) {
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd, &gPTPd->ts[1]);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[2]);
					sendDelayRespFlwUp(gPTPd);
					getTxTS(gPTPd, NULL);
					break;
				case GPTP_EVT_DM_PDELAY_RESP:
					memcpy(&gPTPd->ts[1], &gPTPd->rxBuf[GPTP_BODY_OFFSET], sizeof(struct gPTPts));
					getRxTS(gPTPd, &gPTPd->ts[3]);
					dmHandleStateChange(gPTPd, DM_STATE_DELAY_RESP_FLWUP_WAIT);
					break;
				default:
					break;
			}
			break;

		case DM_STATE_DELAY_RESP_FLWUP_WAIT:
			switch (evtId) {
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd, &gPTPd->ts[1]);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd, &gPTPd->ts[2]);
					sendDelayRespFlwUp(gPTPd);
					getTxTS(gPTPd, NULL);
					break;
				case GPTP_EVT_DM_PDELAY_RESP_FLWUP:
					getRxTS(gPTPd, NULL);
					memcpy(&gPTPd->ts[2], &gPTPd->rxBuf[GPTP_BODY_OFFSET], sizeof(struct gPTPts));
					gPTPd->msrdDelay = ((gPTPd->ts[3].ns - gPTPd->ts[0].ns) - (gPTPd->ts[2].ns - gPTPd->ts[1].ns)) / 2;
					gPTP_logMsg(GPTP_LOG_INFO, "gPTP msrdDelay: %d\n", gPTPd->msrdDelay);
					dmHandleStateChange(gPTPd, DM_STATE_IDLE);
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
	gPTPd->dm.state = toState;
}

static void sendDelayReq(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gPTPd->seq++;
	gh->h.f.seqNo = gPTPd->seq;
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
		gPTP_logMsg(GPTP_LOG_INFO, "PDelayReq sent\n");
}

static void sendDelayResp(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gPTPd->seq++;
	gh->h.f.seqNo = gPTPd->seq;
	gh->h.f.b1.msgType = ((0x01 << 4) | GPTP_MSG_TYPE_PDELAY_RESP);

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x7f;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memcpy(&gPTPd->txBuf[txLen], &gPTPd->ts[1], sizeof(struct gPTPts));
	txLen += 20;

	if ((err = sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll))) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "PDelayResp Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_INFO, "PDelayResp sent\n");
}

static void sendDelayRespFlwUp(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gPTPd->seq++;
	gh->h.f.seqNo = gPTPd->seq;
	gh->h.f.b1.msgType = ((0x01 << 4) | GPTP_MSG_TYPE_PDELAY_RESP_FLWUP);

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x7f;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memcpy(&gPTPd->txBuf[txLen], &gPTPd->ts[2], sizeof(struct gPTPts));
	txLen += 20;

	if ((err = sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll))) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "PDelayRespFlwUp Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_INFO, "PDelayRespFlwUp sent\n");
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
			gPTP_logMsg(GPTP_LOG_ERROR, "Poll failed %d\n", res);
			break;
		} else if (!(pfd.revents & sk_revents)) {
			gPTP_logMsg(GPTP_LOG_ERROR, "poll for tx timestamp woke up on non ERR event");
			break;
		} else {
			gPTP_logMsg(GPTP_LOG_DEBUG, "Poll success\n");
			memset(gPTPd->tsBuf, 0, GPTP_CON_TS_BUF_SIZE);
			gPTPd->rxMsgHdr.msg_iov = &gPTPd->rxiov;
			gPTPd->rxMsgHdr.msg_iovlen = 1;
			gPTPd->rxMsgHdr.msg_control=gPTPd->tsBuf;
			gPTPd->rxMsgHdr.msg_controllen=GPTP_CON_TS_BUF_SIZE;
			gPTPd->rxMsgHdr.msg_flags=0;
			gPTPd->rxMsgHdr.msg_name=&gPTPd->rxSockAddress;
			gPTPd->rxMsgHdr.msg_namelen=sizeof(struct sockaddr_ll);
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
									gPTP_logMsg(GPTP_LOG_INFO, "%d: sec: %d nsec: %d \n", i, rts[i].tv_sec, rts[i].tv_nsec);
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
						gPTP_logMsg(GPTP_LOG_INFO, "%d: sec: %d nsec: %d \n", i, rts[i].tv_sec, rts[i].tv_nsec);
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


