#include "log.h"

#define CS_STATE_INIT                    0
#define CS_STATE_GRAND_MASTER            1
#define CS_STATE_SLAVE                   2

void initCS(struct gPTPd* gPTPd);
void unintCS(struct gPTPd* gPTPd);
void csHandleEvent(struct gPTPd* gPTPd, int evtId);

#ifdef CLOCK_SYNCHRONIZATION
static void csHandleStateChange(struct gPTPd* gPTPd, int toState);
static void sendSync(struct gPTPd* gPTPd);
static void sendSyncFlwup(struct gPTPd* gPTPd);
#endif
