
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
		gPTP_logMsg(GPTP_LOG_NOTICE, "---> Assuming grandmaster role\n");
	} else if((gmMaster == FALSE) && (gPTPd->cs.state == CS_STATE_GRAND_MASTER)) {
		csHandleStateChange(gPTPd, CS_STATE_SLAVE);
		gPTP_logMsg(GPTP_LOG_NOTICE, "---> External grandmaster found\n");
	} else
		gPTP_logMsg(GPTP_LOG_WARNING, "gPTP cannot set state st: %d gmMaster: %d \n", gPTPd->cs.state, gmMaster);	
}

void csHandleEvent(struct gPTPd* gPTPd, int evtId)
{
	int diffsign = 1;
	struct timespec sync[4];

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
					getRxTS(gPTPd, &gPTPd->ts[7]);
					break;
				case GPTP_EVT_CS_SYNC_FLWUP_MSG:
					gptp_copyTSFromBuf(&gPTPd->ts[8], &gPTPd->rxBuf[GPTP_BODY_OFFSET]);
					gPTPd->ts[9].tv_sec = 0;
					gPTPd->ts[9].tv_nsec = gPTPd->msrdDelay;

					gptp_timespec_sum(&gPTPd->ts[8],&gPTPd->ts[9],&sync[0]);
					diffsign = gptp_timespec_absdiff(&gPTPd->ts[7],&sync[0],&sync[1]);

					if(clock_gettime(gPTPd->hwClkId, &gPTPd->ts[10]) < 0)
						gPTP_logMsg(GPTP_LOG_ERROR, "clock_getTime failure, clk_id:%d, err:%d\n", gPTPd->hwClkId, errno);

					gptp_timespec_diff(&gPTPd->ts[7],&gPTPd->ts[10],&sync[2]);
					gptp_timespec_sum(&sync[2],&sync[0],&sync[3]);

					if((sync[1].tv_sec > 10) || (diffsign == -1)) {
#ifdef GPTPD_BUILD_X_86
						if(clock_settime(CLOCK_REALTIME, &sync[3]) < 0)
#else
						if(clock_settime(gPTPd->hwClkId, &sync[3]) < 0)
#endif
							gPTP_logMsg(GPTP_LOG_ERROR, "clock_setTime failure, clk_id:%d, err:%d\n", gPTPd->hwClkId, errno);
					} else {
						gPTPd->tx.time.tv_sec  = sync[1].tv_sec;
						gPTPd->tx.time.tv_usec = sync[1].tv_nsec;

#ifdef GPTPD_BUILD_X_86
						gPTPd->tx.modes   = (ADJ_SETOFFSET | ADJ_NANO);
						if(clock_adjtime(CLOCK_REALTIME, &gPTPd->tx) < 0)
#else
						if(syscall(__NR_clock_adjtime, gPTPd->hwClkId, &gPTPd->tx) < 0)
#endif
							gPTP_logMsg(GPTP_LOG_ERROR, "clock_adjTime failure, clk_id:%d, err:%d\n", gPTPd->hwClkId, errno);
					}

					if(clock_gettime(gPTPd->hwClkId, &gPTPd->ts[11]) < 0)
						gPTP_logMsg(GPTP_LOG_ERROR, "clock_getTime failure, clk_id:%d, err:%d\n", gPTPd->hwClkId, errno);					

					gPTP_logMsg(GPTP_LOG_INFO, "@@@ SyncTxTime: %lld_%09ld\n", (s64)gPTPd->ts[8].tv_sec, gPTPd->ts[8].tv_nsec);
					gPTP_logMsg(GPTP_LOG_INFO, "@@@ SyncRxTime: %lld_%09ld\n", (s64)gPTPd->ts[7].tv_sec, gPTPd->ts[7].tv_nsec);
					gPTP_logMsg(GPTP_LOG_INFO, "@@@ lDelayTime: %lld_%09ld\n", (s64)gPTPd->ts[9].tv_sec, gPTPd->ts[9].tv_nsec);
					gPTP_logMsg(GPTP_LOG_INFO, "@@@ CurrSynOff: %lld_%09ld (%d)\n", (s64)sync[1].tv_sec, sync[1].tv_nsec, diffsign);
					gPTP_logMsg(GPTP_LOG_INFO, "@@@ prSyncTime: %lld_%09ld\n", (s64)gPTPd->ts[10].tv_sec, gPTPd->ts[10].tv_nsec);
					gPTP_logMsg(GPTP_LOG_NOTICE, "@@@ poSyncTime: %lld_%09ld\n", (s64)gPTPd->ts[11].tv_sec, gPTPd->ts[11].tv_nsec);
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
		gPTP_logMsg(GPTP_LOG_INFO, ">>> Sync (%d) sent\n", gPTPd->cs.syncSeqNo);
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
		gPTP_logMsg(GPTP_LOG_INFO, "=== SyncFollowup (%d) sent\n", gPTPd->cs.syncSeqNo++);
}


