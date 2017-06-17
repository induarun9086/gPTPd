#include "log.h"

#define BMC_STATE_INIT                    0
#define BMC_STATE_GRAND_MASTER            1
#define BMC_STATE_SLAVE                   2

void initBMC(struct gPTPd* gPTPd);
void unintBMC(struct gPTPd* gPTPd);
void bmcHandleEvent(struct gPTPd* gPTPd, int evtId);

#ifdef BEST_MASTER_CLOCK_SELECTION
static void bmcHandleStateChange(struct gPTPd* gPTPd, int toState);
static void sendAnnounce(struct gPTPd* gPTPd);
static bool updateAnnounceInfo(struct gPTPd* gPTPd);
#endif
