
#define CLOCK_SYNCHRONIZATION

#include "sync.h"

void initCS(struct gPTPd* gPTPd)
{
	gPTPd->cs.state = CS_STATE_INIT;
	gPTPd->cs.syncInterval = 2000;
	gPTPd->cs.syncTimeout = 32000;
}

void unintCS(struct gPTPd* gPTPd)
{
	
}

void csHandleEvent(struct gPTPd* gPTPd, int evtId)
{
	gPTP_logMsg(GPTP_LOG_INFO, "gPTP csHandleEvent st: %d evt: 0x%x \n", gPTPd->cs.state, evtId);
	
	switch(gPTPd->cs.state) {

		case CS_STATE_INIT:
			switch (evtId) {
				case GPTP_EVT_STATE_ENTRY:
					break;
				case GPTP_EVT_CS_ENABLE:
					csHandleStateChange(gPTPd, CS_STATE_GRAND_MASTER);
					break;
				case GPTP_EVT_STATE_EXIT:
					break;
				default:
					break;
			}
			break;

		case CS_STATE_GRAND_MASTER:
			switch (evtId) {
				case GPTP_EVT_STATE_ENTRY:
					gptp_startTimer(gPTPd, GPTP_TIMER_SYNC_RPT, gPTPd->cs.syncInterval, GPTP_EVT_CS_SYNC_RPT);
					break;
				case GPTP_EVT_CS_SYNC_MSG:
					break;
				case GPTP_EVT_CS_SYNC_RPT:
					sendSync(gPTPd);
					break;
				case GPTP_EVT_STATE_EXIT:
					gptp_stopTimer(gPTPd, GPTP_TIMER_SYNC_RPT);
					break;
				default:
					break;
			}
			break;

		case CS_STATE_SLAVE:
			switch (evtId) {
				case GPTP_EVT_STATE_ENTRY:
					gptp_startTimer(gPTPd, GPTP_TIMER_SYNC_TO, gPTPd->cs.syncTimeout, GPTP_EVT_CS_SYNC_TO);
					break;
				case GPTP_EVT_CS_SYNC_MSG:
					break;
				case GPTP_EVT_CS_SYNC_TO:
					csHandleStateChange(gPTPd, CS_STATE_GRAND_MASTER);
					break;
				case GPTP_EVT_STATE_EXIT:
					gptp_stopTimer(gPTPd, GPTP_TIMER_SYNC_TO);
					break;
				default:
					break;
			}
			break;

		default:
			break;
	}
}

static void csHandleStateChange(struct gPTPd* gPTPd, int toState)
{
	csHandleEvent(gPTPd, GPTP_EVT_STATE_EXIT);
	gPTPd->cs.state = toState;
	csHandleEvent(gPTPd, GPTP_EVT_STATE_ENTRY);
}

static void sendSync(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr *gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];
	struct gPTPAnnoSt *annoSt = (struct gPTPAnnoSt*)&gPTPd->txBuf[GPTP_BODY_OFFSET];
	struct gPTPtlv *tlv;

	/* Fill gPTP header */
	gh->h.f.seqNo = gptp_chgEndianess16(gPTPd->cs.syncSeqNo);
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
		gPTP_logMsg(GPTP_LOG_INFO, ">>> Announce (%d) sent\n", gPTPd->cs.syncSeqNo++);
}

static void sendSyncFlwup(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr *gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];
	struct gPTPAnnoSt *annoSt = (struct gPTPAnnoSt*)&gPTPd->txBuf[GPTP_BODY_OFFSET];
	struct gPTPtlv *tlv;

	/* Fill gPTP header */
	gh->h.f.seqNo = gptp_chgEndianess16(gPTPd->cs.syncSeqNo);
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
		gPTP_logMsg(GPTP_LOG_DEBUG, "SyncFollowup Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_INFO, ">>> SyncFollowup (%d) sent\n", gPTPd->cs.syncSeqNo++);
}


