#include <linux/kernel.h>

#include "gptpcmn.h"

#define DM_STATE_INIT            0
#define DM_STATE_IDLE            1
#define DM_STATE_DELAY_REQ_TX    2
#define DM_STATE_DELAY_RES_RX    3

struct dmst {
	int state;
	struct socket* sock;
	struct msghdr txMsgHdr;
	struct sockaddr_ll txSockAddress;
	struct msghdr rxMsgHdr;
	struct sockaddr_ll rxSockAddress;
	char tsBuf[GPTP_SYNCMSG_ETH_FRAME_SIZE];
};

void initDM(struct dmst* dm);
void unintDM(struct dmst* dm);
void dmHandleEvent(struct dmst* dm, int evtId);
void dmHandleStateChange(struct dmst* dm, int toState);

#ifdef DELAY_MSR_MODULE
static int sendDelayReq(struct dmst* dm);
static int getDelayReqTS(struct dmst* dm);
#endif
