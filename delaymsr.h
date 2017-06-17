#include "log.h"

#ifndef DELAY_MSR_H
#define DELAY_MSR_H

#define DM_STATE_INIT			0
#define DM_STATE_IDLE			1
#define DM_STATE_DELAY_RESP_WAIT	2
#define DM_STATE_DELAY_RESP_FLWUP_WAIT	3

void initDM(struct gPTPd* gPTPd);
void unintDM(struct gPTPd* gPTPd);
void dmHandleEvent(struct gPTPd* gPTPd, int evtId);

#ifdef DELAY_MSR_MODULE
static void dmHandleStateChange(struct gPTPd* gPTPd, int toState);
static void sendDelayReq(struct gPTPd* gPTPd);
static void sendDelayResp(struct gPTPd* gPTPd);
static void sendDelayRespFlwUp(struct gPTPd* gPTPd);
static void getTxTS(struct gPTPd* gPTPd, struct timespec* ts);
static void getRxTS(struct gPTPd* gPTPd, struct timespec* ts);
#endif

#endif
