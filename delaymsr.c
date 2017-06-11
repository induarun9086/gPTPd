
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
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd);
					sendDelayRespFlwUp(gPTPd);
					break;
				default:
					break;
			}
			break;

		case DM_STATE_IDLE:
			switch (evtId) {
				case GPTP_EVT_ONE_SEC_TICK:
					sendDelayReq(gPTPd);
					getTxTS(gPTPd);
					dmHandleStateChange(gPTPd, DM_STATE_DELAY_RESP_WAIT);
					break;
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd);
					sendDelayRespFlwUp(gPTPd);
					break;
				default:
					break;
			}
			break;

		case DM_STATE_DELAY_RESP_WAIT:
			switch (evtId) {
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd);
					sendDelayRespFlwUp(gPTPd);
					break;
				case GPTP_EVT_DM_PDELAY_RESP:
					getRxTS(gPTPd);
					dmHandleStateChange(gPTPd, DM_STATE_DELAY_RESP_FLWUP_WAIT);
					break;
				default:
					break;
			}
			break;

		case DM_STATE_DELAY_RESP_FLWUP_WAIT:
			switch (evtId) {
				case GPTP_EVT_DM_PDELAY_REQ:
					getRxTS(gPTPd);
					sendDelayResp(gPTPd);
					getTxTS(gPTPd);
					sendDelayRespFlwUp(gPTPd);
					break;
				case GPTP_EVT_DM_PDELAY_RESP_FLWUP:
					getRxTS(gPTPd);
					dmHandleStateChange(gPTPd, DM_STATE_INIT);
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
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gPTPd->seq++;
	gh->h.f.seqNo = gPTPd->seq;
	gh->h.f.b1.msgType |= GPTP_MSG_TYPE_PDELAY_REQ;

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x7f;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	txLen += 20;

	if (sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll)) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "PDelayReq Send failed\n");	
	else
		gPTP_logMsg(GPTP_LOG_INFO, "PDelayReq sent\n");
}

static void sendDelayResp(struct gPTPd* gPTPd)
{
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gPTPd->seq++;
	gh->h.f.seqNo = gPTPd->seq;
	gh->h.f.b1.msgType |= GPTP_MSG_TYPE_PDELAY_REQ;

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x7f;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	txLen += 20;

	if (sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll)) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "PDelayResp Send failed\n");	
	else
		gPTP_logMsg(GPTP_LOG_INFO, "PDelayResp sent\n");
}

static void sendDelayRespFlwUp(struct gPTPd* gPTPd)
{
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	/* Fill gPTP header */
	gPTPd->seq++;
	gh->h.f.seqNo = gPTPd->seq;
	gh->h.f.b1.msgType |= GPTP_MSG_TYPE_PDELAY_REQ;

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x7f;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	txLen += 20;

	if (sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll)) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "PDelayRespFlwUp Send failed\n");	
	else
		gPTP_logMsg(GPTP_LOG_INFO, "PDelayRespFlwUp sent\n");
}

static void getTxTS(struct gPTPd* gPTPd)
{
	static short sk_events = POLLPRI;
	static short sk_revents = POLLPRI;
	int cnt = 0, res = 0, level, type;
	struct cmsghdr *cm;
	struct timespec *sw, *ts = NULL;
	struct pollfd pfd = { gPTPd->sockfd, sk_events, 0 };

	res = poll(&pfd, 1, 1000);
	if (res < 1) {
		gPTP_logMsg(GPTP_LOG_ERROR, "Poll failed %d\n", res);
	} else if (!(pfd.revents & sk_revents)) {
		gPTP_logMsg(GPTP_LOG_ERROR, "poll for tx timestamp woke up on non ERR event");
	} else {
		gPTP_logMsg(GPTP_LOG_DEBUG, "Poll success\n");	
		cnt = recvmsg(gPTPd->sockfd, &gPTPd->rxMsgHdr, MSG_ERRQUEUE);
		if (cnt < 1)
			gPTP_logMsg(GPTP_LOG_ERROR, "Recv failed\n");
		else
			for (cm = CMSG_FIRSTHDR(&gPTPd->rxMsgHdr); cm != NULL; cm = CMSG_NXTHDR(&gPTPd->rxMsgHdr, cm)) {
				level = cm->cmsg_level;
				type  = cm->cmsg_type;
				gPTP_logMsg(GPTP_LOG_DEBUG, "Lvl:%d Type: %d Size: %d (%d)\n", level, type, cm->cmsg_len, sizeof(struct timespec));
				if (SOL_SOCKET == level && SO_TIMESTAMPING == type) {
					if (cm->cmsg_len < sizeof(*ts) * 3) {
						gPTP_logMsg(GPTP_LOG_DEBUG, "short SO_TIMESTAMPING message");
					} else {
						ts = (struct timespec *) CMSG_DATA(cm);
						for(int i = 0; i < 3; i++)
							if((ts[i].tv_sec != 0) || (ts[i].tv_nsec != 0))
								gPTP_logMsg(GPTP_LOG_INFO, "%d: sec: %d nsec: %d \n", i, ts[i].tv_sec, ts[i].tv_nsec);
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


static void getRxTS(struct gPTPd* gPTPd)
{
	int level, type;
	struct cmsghdr *cm;
	struct timespec *sw, *ts = NULL;

	for (cm = CMSG_FIRSTHDR(&gPTPd->rxMsgHdr); cm != NULL; cm = CMSG_NXTHDR(&gPTPd->rxMsgHdr, cm)) {
		level = cm->cmsg_level;
		type  = cm->cmsg_type;
		gPTP_logMsg(GPTP_LOG_DEBUG, "Lvl:%d Type: %d Size: %d (%d)\n", level, type, cm->cmsg_len, sizeof(struct timespec));
		if (SOL_SOCKET == level && SO_TIMESTAMPING == type) {
			if (cm->cmsg_len < sizeof(*ts) * 3) {
				gPTP_logMsg(GPTP_LOG_DEBUG, "short SO_TIMESTAMPING message");
			} else {
				ts = (struct timespec *) CMSG_DATA(cm);
				for(int i = 0; i < 3; i++)
					if((ts[i].tv_sec != 0) || (ts[i].tv_nsec != 0))
						gPTP_logMsg(GPTP_LOG_INFO, "%d: sec: %d nsec: %d \n", i, ts[i].tv_sec, ts[i].tv_nsec);
			}
		}
		if (SOL_SOCKET == level && SO_TIMESTAMPNS == type) {
			if (cm->cmsg_len < sizeof(*sw)) {
				gPTP_logMsg(GPTP_LOG_DEBUG, "short SO_TIMESTAMPNS message");
			}
		}
	}
}


