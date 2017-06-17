
#define BEST_MASTER_CLOCK_SELECTION

#include "bmc.h"

void initBMC(struct gPTPd* gPTPd)
{
	gPTPd->bmc.state = BMC_STATE_INIT;
	gPTPd->bmc.announceInterval = 2000;
	gPTPd->bmc.announceTimeout = 32000;
}

void unintBMC(struct gPTPd* gPTPd)
{
	
}

void bmcHandleEvent(struct gPTPd* gPTPd, int evtId)
{
	gPTP_logMsg(GPTP_LOG_INFO, "gPTP bmcHandleEvent st: %d evt: 0x%x \n", gPTPd->bmc.state, evtId);
	
	switch(gPTPd->bmc.state) {

		case BMC_STATE_INIT:
			switch (evtId) {
				case GPTP_EVT_STATE_ENTRY:
					break;
				case GPTP_EVT_BMC_ENABLE:
					bmcHandleStateChange(gPTPd, BMC_STATE_GRAND_MASTER);
					break;
				case GPTP_EVT_STATE_EXIT:
					break;
				default:
					break;
			}
			break;

		case BMC_STATE_GRAND_MASTER:
			switch (evtId) {
				case GPTP_EVT_STATE_ENTRY:
					gptp_startTimer(gPTPd, GPTP_TIMER_ANNOUNCE_RPT, gPTPd->bmc.announceInterval, GPTP_EVT_BMC_ANNOUNCE_RPT);
					break;
				case GPTP_EVT_BMC_ANNOUNCE_MSG:
					if(updateAnnounceInfo(gPTPd) == FALSE)
						bmcHandleStateChange(gPTPd, BMC_STATE_SLAVE);
					break;
				case GPTP_EVT_BMC_ANNOUNCE_RPT:
					sendAnnounce(gPTPd);
					break;
				case GPTP_EVT_STATE_EXIT:
					gptp_stopTimer(gPTPd, GPTP_TIMER_ANNOUNCE_RPT);
					break;
				default:
					break;
			}
			break;

		case BMC_STATE_SLAVE:
			switch (evtId) {
				case GPTP_EVT_STATE_ENTRY:
					gptp_startTimer(gPTPd, GPTP_TIMER_ANNOUNCE_TO, gPTPd->bmc.announceTimeout, GPTP_EVT_BMC_ANNOUNCE_TO);
					break;
				case GPTP_EVT_BMC_ANNOUNCE_MSG:
					if(updateAnnounceInfo(gPTPd) == TRUE)
						bmcHandleStateChange(gPTPd, BMC_STATE_GRAND_MASTER);
					break;
				case GPTP_EVT_BMC_ANNOUNCE_TO:
					bmcHandleStateChange(gPTPd, BMC_STATE_GRAND_MASTER);
					break;
				case GPTP_EVT_STATE_EXIT:
					gptp_stopTimer(gPTPd, GPTP_TIMER_ANNOUNCE_TO);
					break;
				default:
					break;
			}
			break;

		default:
			break;
	}
}

static void bmcHandleStateChange(struct gPTPd* gPTPd, int toState)
{
	bmcHandleEvent(gPTPd, GPTP_EVT_STATE_EXIT);
	gPTPd->bmc.state = toState;
	bmcHandleEvent(gPTPd, GPTP_EVT_STATE_ENTRY);
}

static void sendAnnounce(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr *gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];
	struct gPTPAnnoSt *annoSt = (struct gPTPAnnoSt*)&gPTPd->txBuf[GPTP_BODY_OFFSET];
	struct gPTPtlv *tlv;

	/* Fill gPTP header */
	gh->h.f.seqNo = gptp_chgEndianess16(gPTPd->bmc.annoSeqNo);
	gh->h.f.b1.msgType = ((0x01 << 4) | GPTP_MSG_TYPE_ANNOUNCE);
	gh->h.f.flags = gptp_chgEndianess16(0x0000);

	gh->h.f.ctrl = 0x05;
	gh->h.f.logMsgInt = 0x01;

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memset(&gPTPd->txBuf[GPTP_BODY_OFFSET], 0, (GPTP_TX_BUF_SIZE - GPTP_BODY_OFFSET));
	annoSt->gmPrio1 = 250;
	annoSt->gmClockQual.clockClass = 248;
	annoSt->gmClockQual.clockAccuracy = 254;
	annoSt->gmClockQual.offsetScaledLogVariance = gptp_chgEndianess16(0x4100);
	annoSt->gmPrio2 = 250;
	memcpy(&annoSt->gmIden[0], &gh->h.f.srcPortIden[0], GPTP_PORT_IDEN_LEN);
	annoSt->stepsRem = 0;
	annoSt->clockSrc = 0xA0; /* Internal oscillator */
	txLen += sizeof(struct gPTPAnnoSt);

	/* Path trace TLVs */
	tlv = (struct gPTPtlv *)&gPTPd->txBuf[txLen];
	tlv->type = gptp_chgEndianess16(0x0008);
	tlv->len  = gptp_chgEndianess16(GPTP_CLOCK_IDEN_LEN);
	txLen += sizeof(struct gPTPtlv);
	memcpy(&gPTPd->txBuf[txLen], &gh->h.f.srcPortIden, GPTP_CLOCK_IDEN_LEN);
	txLen += GPTP_CLOCK_IDEN_LEN;

	/* Insert length */
	gh->h.f.msgLen = gptp_chgEndianess16(txLen - sizeof(struct ethhdr));

	if ((err = sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll))) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "Announce Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_INFO, ">>> Announce (%d) sent\n", gPTPd->bmc.annoSeqNo++);
}

static bool updateAnnounceInfo(struct gPTPd* gPTPd)
{
	return FALSE;
}


