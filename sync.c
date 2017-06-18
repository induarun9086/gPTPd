
#define CLOCK_SYNCHRONIZATION

#include "sync.h"

void initCS(struct gPTPd* gPTPd)
{
	gPTPd->cs.state = CS_STATE_INIT;
	gPTPd->cs.syncInterval = GPTP_SYNC_INTERVAL;
	gPTPd->cs.syncTimeout  = GPTP_SYNC_TIMEOUT;
}

void unintCS(struct gPTPd* gPTPd)
{
	
}

void csSetState(struct gPTPd* gPTPd, bool gmMaster)
{
	if((gmMaster == TRUE) && (gPTPd->cs.state != CS_STATE_GRAND_MASTER)) {
		csHandleStateChange(gPTPd, CS_STATE_GRAND_MASTER);
		gPTP_logMsg(GPTP_LOG_NOTICE, "--------------------------> Assuming grandmaster role\n");
	} else if((gmMaster == FALSE) && (gPTPd->cs.state == CS_STATE_GRAND_MASTER)) {
		csHandleStateChange(gPTPd, CS_STATE_SLAVE);
		gPTP_logMsg(GPTP_LOG_NOTICE, "--------------------------> External grandmaster found\n");
	} else
		gPTP_logMsg(GPTP_LOG_WARNING, "gPTP cannot set state st: %d gmMaster: %d \n", gPTPd->cs.state, gmMaster);	
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
					getTxTS(gPTPd, &gPTPd->ts[6]);
					sendSyncFlwup(gPTPd);
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

	/* Fill gPTP header */
	gh->h.f.seqNo = gptp_chgEndianess16(gPTPd->cs.syncSeqNo);
	gh->h.f.b1.msgType = (GPTP_TRANSPORT_L2 | GPTP_MSG_TYPE_SYNC);
	gh->h.f.flags = gptp_chgEndianess16(GPTP_FLAGS_TWO_STEP);

	gh->h.f.ctrl = GPTP_CONTROL_SYNC;
	gh->h.f.logMsgInt = gptp_calcLogInterval(gPTPd->cs.syncInterval / 1000);

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memset(&gPTPd->txBuf[GPTP_BODY_OFFSET], 0, (GPTP_TX_BUF_SIZE - GPTP_BODY_OFFSET));
	txLen += GPTP_TS_LEN;

	/* Insert length */
	gh->h.f.msgLen = gptp_chgEndianess16(txLen - sizeof(struct ethhdr));

	if ((err = sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll))) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "Sync Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_NOTICE, ">>> Sync (%d) sent\n", gPTPd->cs.syncSeqNo);
}

static void sendSyncFlwup(struct gPTPd* gPTPd)
{
	int err = 0;
	int txLen = sizeof(struct ethhdr);
	struct gPTPHdr *gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];
	struct gPTPtlv *tlv;
	struct gPTPOrgExt *orgExt;

	/* Fill gPTP header */
	gh->h.f.seqNo = gptp_chgEndianess16(gPTPd->cs.syncSeqNo);
	gh->h.f.b1.msgType = (GPTP_TRANSPORT_L2 | GPTP_MSG_TYPE_SYNC_FLWUP);
	gh->h.f.flags = gptp_chgEndianess16(GPTP_FLAGS_NONE);

	gh->h.f.ctrl = GPTP_CONTROL_SYNC_FLWUP;
	gh->h.f.logMsgInt = gptp_calcLogInterval(gPTPd->cs.syncInterval / 1000);

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memset(&gPTPd->txBuf[GPTP_BODY_OFFSET], 0, (GPTP_TX_BUF_SIZE - GPTP_BODY_OFFSET));
	gptp_copyTSToBuf(&gPTPd->ts[6], &gPTPd->txBuf[txLen]);
	txLen += GPTP_TS_LEN;

	/* Organization TLV */
	tlv = (struct gPTPtlv *)&gPTPd->txBuf[txLen];
	tlv->type = gptp_chgEndianess16(GPTP_TLV_TYPE_ORG_EXT);
	tlv->len  = gptp_chgEndianess16(sizeof(struct gPTPOrgExt));
	txLen += sizeof(struct gPTPtlv);
	orgExt = (struct gPTPOrgExt *)&gPTPd->txBuf[txLen];
	orgExt->orgType[0] = 0x00; orgExt->orgType[1] = 0x80; orgExt->orgType[2] = 0xC2;
	orgExt->orgSubType[0] = 0x00; orgExt->orgSubType[1] = 0x00; orgExt->orgSubType[2] = 0x01;
	txLen += sizeof(struct gPTPOrgExt);

	/* Insert length */
	gh->h.f.msgLen = gptp_chgEndianess16(txLen - sizeof(struct ethhdr));

	if ((err = sendto(gPTPd->sockfd, gPTPd->txBuf, txLen, 0, (struct sockaddr*)&gPTPd->txSockAddress, sizeof(struct sockaddr_ll))) < 0)
		gPTP_logMsg(GPTP_LOG_DEBUG, "SyncFollowup Send failed %d %d\n", err, errno);	
	else
		gPTP_logMsg(GPTP_LOG_NOTICE, "=== SyncFollowup (%d) sent\n", gPTPd->cs.syncSeqNo++);
}


