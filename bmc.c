
#define BEST_MASTER_CLOCK_SELECTION

#include "bmc.h"
#include "sync.h"

void initBMC(struct gPTPd* gPTPd)
{
	gPTPd->bmc.state = BMC_STATE_INIT;
	gPTPd->bmc.announceInterval = GPTP_ANNOUNCE_INTERVAL;
	gPTPd->bmc.announceTimeout  = GPTP_ANNOUNCE_TIMEOUT;
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
					updatePrioVectors(gPTPd);
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
					csSetState(gPTPd, TRUE);
					break;
				case GPTP_EVT_BMC_ANNOUNCE_MSG:
					if(updateAnnounceInfo(gPTPd) == TRUE)
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
					csSetState(gPTPd, FALSE);
					break;
				case GPTP_EVT_BMC_ANNOUNCE_MSG:
					if(updateAnnounceInfo(gPTPd) == TRUE)
						gptp_resetTimer(gPTPd, GPTP_TIMER_ANNOUNCE_TO);
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
	struct gPTPPrioVec *prio = (struct gPTPPrioVec*)&gPTPd->txBuf[GPTP_BODY_OFFSET];
	struct gPTPtlv *tlv;

	/* Fill gPTP header */
	gh->h.f.seqNo = gptp_chgEndianess16(gPTPd->bmc.annoSeqNo);
	gh->h.f.b1.msgType = (GPTP_TRANSPORT_L2 | GPTP_MSG_TYPE_ANNOUNCE);
	gh->h.f.flags = gptp_chgEndianess16(GPTP_FLAGS_NONE);

	gh->h.f.ctrl = GPTP_CONTROL_DELAY_ANNOUNCE;
	gh->h.f.logMsgInt = gptp_calcLogInterval(gPTPd->bmc.announceInterval / 1000);

	/* Add gPTP header size */
	txLen += sizeof(struct gPTPHdr);

	/* PTP body */
	memset(&gPTPd->txBuf[GPTP_BODY_OFFSET], 0, (GPTP_TX_BUF_SIZE - GPTP_BODY_OFFSET));
	prio->currUTCOff = gPTPd->bmc.portPrio.currUTCOff;
	prio->prio1 = gPTPd->bmc.portPrio.prio1;
	prio->clockQual.clockClass = gPTPd->bmc.portPrio.clockQual.clockClass;
	prio->clockQual.clockAccuracy = gPTPd->bmc.portPrio.clockQual.clockAccuracy;
	prio->clockQual.offsetScaledLogVariance = gptp_chgEndianess16(gPTPd->bmc.portPrio.clockQual.offsetScaledLogVariance);
	prio->prio2 = gPTPd->bmc.portPrio.prio2;
	memcpy(&prio->iden[0], &gPTPd->bmc.portPrio.iden[0], GPTP_PORT_IDEN_LEN);
	prio->stepsRem = gPTPd->bmc.portPrio.stepsRem;
	prio->clockSrc = gPTPd->bmc.portPrio.clockSrc;
	txLen += sizeof(struct gPTPPrioVec);

	/* Path trace TLV */
	tlv = (struct gPTPtlv *)&gPTPd->txBuf[txLen];
	tlv->type = gptp_chgEndianess16(GPTP_TLV_TYPE_PATH_TRACE);
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
	bool gmFound = FALSE;

	struct gPTPPrioVec *gnPrio = (struct gPTPPrioVec *)&gPTPd->rxBuf[GPTP_BODY_OFFSET];

	gPTP_logMsg(GPTP_LOG_DEBUG, "Gnprio: %x:%x:%x:%x:%x:%x:%x \n",
			    gnPrio->prio1, gnPrio->clockQual.clockClass, gnPrio->clockQual.clockAccuracy,
			    gptp_chgEndianess16(gnPrio->clockQual.offsetScaledLogVariance), gnPrio->prio2,
			    gnPrio->stepsRem, gnPrio->clockSrc);
	gPTP_logMsg(GPTP_LOG_DEBUG, "Gnprio: %x:%x:%x:%x:%x:%x:%x:%x \n",
			    gnPrio->iden[0], gnPrio->iden[1], gnPrio->iden[2], gnPrio->iden[3],
			    gnPrio->iden[4], gnPrio->iden[5], gnPrio->iden[6], gnPrio->iden[7]);
	gPTP_logMsg(GPTP_LOG_DEBUG, "Portprio: %x:%x:%x:%x:%x:%x:%x \n",
			    gPTPd->bmc.portPrio.prio1, gPTPd->bmc.portPrio.clockQual.clockClass,
			    gPTPd->bmc.portPrio.clockQual.clockAccuracy,
			    gPTPd->bmc.portPrio.clockQual.offsetScaledLogVariance,
			    gPTPd->bmc.portPrio.prio2,
			    gPTPd->bmc.portPrio.stepsRem,
			    gPTPd->bmc.portPrio.clockSrc);
	gPTP_logMsg(GPTP_LOG_DEBUG, "Portprio: %x:%x:%x:%x:%x:%x:%x:%x \n",
			    gPTPd->bmc.portPrio.iden[0], gPTPd->bmc.portPrio.iden[1],
			    gPTPd->bmc.portPrio.iden[2], gPTPd->bmc.portPrio.iden[3],
			    gPTPd->bmc.portPrio.iden[4], gPTPd->bmc.portPrio.iden[5],
			    gPTPd->bmc.portPrio.iden[6], gPTPd->bmc.portPrio.iden[7]);

	if(gnPrio->prio1 < gPTPd->bmc.portPrio.prio1)
		gmFound = TRUE;
	else if(gnPrio->clockQual.clockClass < gPTPd->bmc.portPrio.clockQual.clockClass)
		gmFound = TRUE;
	else if(gnPrio->clockQual.clockAccuracy < gPTPd->bmc.portPrio.clockQual.clockAccuracy)
		gmFound = TRUE;
	else if(gptp_chgEndianess16(gnPrio->clockQual.offsetScaledLogVariance) < gPTPd->bmc.portPrio.clockQual.offsetScaledLogVariance)
		gmFound = TRUE;
	else if(gnPrio->prio2 < gPTPd->bmc.portPrio.prio2)
		gmFound = TRUE;
	else if(gnPrio->stepsRem < gPTPd->bmc.portPrio.stepsRem)
		gmFound = TRUE;
	else if(gnPrio->clockSrc < gPTPd->bmc.portPrio.clockSrc)
		gmFound = TRUE;
	else {
		for(int i = 0; ((i < GPTP_PORT_IDEN_LEN) && (gmFound == FALSE)); i++) {
			if(gnPrio->iden[i] < gPTPd->bmc.portPrio.iden[i])
				gmFound = TRUE;
			else if(gnPrio->iden[i] > gPTPd->bmc.portPrio.iden[i])
				break;
			else
				continue;	
		}	
	}

	if(gmFound == TRUE) {
		gPTP_logMsg(GPTP_LOG_INFO, "High prio announce from: %x:%x:%x:%x:%x:%x:%x:%x \n",
			    gnPrio->iden[0], gnPrio->iden[1], gnPrio->iden[2], gnPrio->iden[3],
			    gnPrio->iden[4], gnPrio->iden[5], gnPrio->iden[6], gnPrio->iden[7]);
		memcpy(&gPTPd->bmc.gmPrio, gnPrio, sizeof(struct gPTPPrioVec));
	} else {
		gPTP_logMsg(GPTP_LOG_INFO, "Low prio announce from %x:%x:%x:%x:%x:%x:%x:%x \n",
			    gnPrio->iden[0], gnPrio->iden[1], gnPrio->iden[2], gnPrio->iden[3],
			    gnPrio->iden[4], gnPrio->iden[5], gnPrio->iden[6], gnPrio->iden[7]);
		memcpy(&gPTPd->bmc.gmPrio, &gPTPd->bmc.portPrio, sizeof(struct gPTPPrioVec));
	}
	
	return gmFound;
}
	
static void updatePrioVectors(struct gPTPd* gPTPd)
{
	struct gPTPHdr *gh = (struct gPTPHdr *)&gPTPd->txBuf[sizeof(struct ethhdr)];

	gPTPd->bmc.portPrio.currUTCOff = 0;
	gPTPd->bmc.portPrio.prio1  = GPTP_DEFAULT_CLOCK_PRIO1;
	gPTPd->bmc.portPrio.clockQual.clockClass = GPTP_DEFAULT_CLOCK_CLASS;
	gPTPd->bmc.portPrio.clockQual.clockAccuracy = GPTP_DEFAULT_CLOCK_ACCURACY;
	gPTPd->bmc.portPrio.clockQual.offsetScaledLogVariance = GPTP_DEFAULT_OFFSET_VARIANCE;
	gPTPd->bmc.portPrio.prio2 = GPTP_DEFAULT_CLOCK_PRIO2;
	memcpy(&gPTPd->bmc.portPrio.iden[0], &gh->h.f.srcPortIden[0], GPTP_PORT_IDEN_LEN);
	gPTPd->bmc.portPrio.stepsRem = GPTP_DEFAULT_STEPS_REMOVED;
	gPTPd->bmc.portPrio.clockSrc = GPTP_CLOCK_TYPE_INT_OSC;

	memcpy(&gPTPd->bmc.gmPrio, &gPTPd->bmc.portPrio, sizeof(struct gPTPPrioVec));
}


