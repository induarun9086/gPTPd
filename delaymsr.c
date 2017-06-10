
#define DELAY_MSR_MODULE

#include "delaymsr.h"

char txBuf[GPTP_SYNCMSG_ETH_FRAME_SIZE];
char rxBuf[GPTP_SYNCMSG_ETH_FRAME_SIZE];
char seq = 0;

void initDM(struct dmst* dm)
{
	int tsOpts;
	int errCode;
	struct ifreq ifreq;
	struct hwtstamp_config cfg;

	memset(&ifreq, 0, sizeof(ifreq));
	memset(&cfg, 0, sizeof(cfg));
	memset(dm, 0, sizeof(struct dmst));
	dm->state = DM_STATE_INIT;
	
	/* Create a socket */
	if (sock_create(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL), &dm->sock) < 0) {
		printk(KERN_WARNING "gPTP sendSyncMsg Socket creation fails \n");
		goto err;
	}

	/* Enable HW TS */
	strncpy(ifreq.ifr_name, "eth0", sizeof(ifreq.ifr_name) - 1);
	ifreq.ifr_data = (void *) &cfg;
	cfg.tx_type    = HWTSTAMP_TX_ON;
	cfg.rx_filter  = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
	if ((errCode == dm->sock->ops->ioctl(dm->sock, SIOCSHWTSTAMP, &ifreq)) != 0) {
		printk(KERN_WARNING "gPTP sendSyncMsg Set HW TS option fails %d\n", errCode);
		goto err;	
	}

	/* Set timestamp options */
	tsOpts = SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE | \
		 SOF_TIMESTAMPING_OPT_CMSG | SOF_TIMESTAMPING_OPT_ID;
	if ((errCode == dm->sock->ops->setsockopt(dm->sock, SOL_SOCKET, SO_TIMESTAMPING, (void *) &tsOpts, sizeof(tsOpts))) != 0) {
		printk(KERN_WARNING "gPTP sendSyncMsg Set TS option fails %d\n", errCode);
		goto err;	
	}

	/* Set error queue options */
	tsOpts = 1;
	if ((errCode == dm->sock->ops->setsockopt(dm->sock, SOL_SOCKET, SO_SELECT_ERR_QUEUE, (void *) &tsOpts, sizeof(tsOpts))) != 0) {
		printk(KERN_WARNING "gPTP sendSyncMsg Set ErrQ option fails %d\n", errCode);
		goto err;	
	}

	/* Index of the network device */
	dm->txSockAddress.sll_family = AF_PACKET;
	dm->txSockAddress.sll_protocol = htons(ETH_P_1588);
	dm->txSockAddress.sll_ifindex = 2;
	/* Address length*/
	dm->txSockAddress.sll_halen = ETH_ALEN;
	/* Destination MAC */
	dm->txSockAddress.sll_addr[0] = 0x01;
	dm->txSockAddress.sll_addr[1] = 0x80;
	dm->txSockAddress.sll_addr[2] = 0xC2;
	dm->txSockAddress.sll_addr[3] = 0x00;
	dm->txSockAddress.sll_addr[4] = 0x00;
	dm->txSockAddress.sll_addr[5] = 0x0E;

	/* Set the message header */
	dm->txMsgHdr.msg_control=NULL;
	dm->txMsgHdr.msg_controllen=0;
	dm->txMsgHdr.msg_flags=0;
	dm->txMsgHdr.msg_name=&dm->txSockAddress;
	dm->txMsgHdr.msg_namelen=sizeof(struct sockaddr_ll);
	dm->txMsgHdr.msg_iocb = NULL;

	/* Index of the network device */
	dm->rxSockAddress.sll_family = AF_PACKET;
	dm->rxSockAddress.sll_protocol = htons(ETH_P_1588);
	dm->rxSockAddress.sll_ifindex = 2;
	/* Address length*/
	dm->rxSockAddress.sll_halen = ETH_ALEN;
	/* Destination MAC */
	dm->rxSockAddress.sll_addr[0] = 0x04;
	dm->rxSockAddress.sll_addr[1] = 0xA3;
	dm->rxSockAddress.sll_addr[2] = 0x16;
	dm->rxSockAddress.sll_addr[3] = 0xAD;
	dm->rxSockAddress.sll_addr[4] = 0x3A;
	dm->rxSockAddress.sll_addr[5] = 0x33;

	/* Set the message header */
	dm->rxMsgHdr.msg_control=dm->tsBuf;
	dm->rxMsgHdr.msg_controllen=GPTP_SYNCMSG_ETH_FRAME_SIZE;
	dm->rxMsgHdr.msg_flags=0;
	dm->rxMsgHdr.msg_name=&dm->rxSockAddress;
	dm->rxMsgHdr.msg_namelen=sizeof(struct sockaddr_ll);
	dm->rxMsgHdr.msg_iocb = NULL;

err:
	return;
}

void unintDM(struct dmst* dm)
{
	sock_release(dm->sock);
}

void dmHandleEvent(struct dmst* dm, int evtId)
{
	printk(KERN_INFO "gPTP dmHandleEvent st: %d evt: %d \n", dm->state, evtId);
	
	switch(dm->state) {

		case DM_STATE_INIT:
		case DM_STATE_IDLE:
	
			switch (evtId) {
				case GPTP_EVT_ONE_SEC_TICK:
					sendDelayReq(dm);
					dmHandleStateChange(dm, DM_STATE_DELAY_REQ_TX);
					break;
				default:
					break;
			}

			break;

		case DM_STATE_DELAY_REQ_TX:
	
			switch (evtId) {
				case GPTP_EVT_ONE_SEC_TICK:
					getDelayReqTS(dm);
					break;
				default:
					break;
			}

			break;
	}
}

void dmHandleStateChange(struct dmst* dm, int toState)
{
	dm->state = toState;
}

static int sendDelayReq(struct dmst* dm)
{
	int res = -1;
	int dataOff;
	int errCode;
	struct ethhdr *eh = (struct ethhdr *)txBuf;
	struct iovec txiov;
	mm_segment_t oldfs;

	/* Initialize it */
	memset(txBuf, 0, GPTP_SYNCMSG_ETH_FRAME_SIZE);

	/* Fill in the Ethernet header */
	eh->h_dest[0] = 0x01;
	eh->h_dest[1] = 0x80;
	eh->h_dest[2] = 0xC2;
	eh->h_dest[3] = 0x00;
	eh->h_dest[4] = 0x00;
	eh->h_dest[5] = 0x0E;
	eh->h_source[0] = 0x04;
	eh->h_source[1] = 0xA3;
	eh->h_source[2] = 0x16;
	eh->h_source[3] = 0xAD;
	eh->h_source[4] = 0x3A;
	eh->h_source[5] = 0x33;

	/* Fill in Ethertype field */
	eh->h_proto = htons(ETH_P_1588);

	/* Get data offset */
	dataOff = sizeof(struct ethhdr);

	/* Fill PTP payload data */
	txBuf[dataOff++] = 0x02;
	txBuf[dataOff++] = 0x02;

	txBuf[dataOff++] = 0x00;
	txBuf[dataOff++] = 0x36;

	txBuf[dataOff++] = 0x00;
	txBuf[dataOff++] = 0x00;

	txBuf[dataOff++] = 0x00;
	txBuf[dataOff++] = 0x00;

	txBuf[dataOff++] = 0x00; txBuf[dataOff++] = 0x00; txBuf[dataOff++] = 0x00;
	txBuf[dataOff++] = 0x00; txBuf[dataOff++] = 0x00; txBuf[dataOff++] = 0x00;
	txBuf[dataOff++] = 0x00; txBuf[dataOff++] = 0x00;

	txBuf[dataOff++] = 0x00; txBuf[dataOff++] = 0x00; txBuf[dataOff++] = 0x00;
	txBuf[dataOff++] = 0x00; 

	txBuf[dataOff++] = 0x04; txBuf[dataOff++] = 0xA3; txBuf[dataOff++] = 0x16;
	txBuf[dataOff++] = 0xFF; txBuf[dataOff++] = 0xFE; txBuf[dataOff++] = 0xAD;
	txBuf[dataOff++] = 0x3A; txBuf[dataOff++] = 0x33; txBuf[dataOff++] = 0x00;
	txBuf[dataOff++] = 0x01;

	seq++;
	txBuf[dataOff++] = 0x00; txBuf[dataOff++] = seq;

	txBuf[dataOff++] = 0x05;
	txBuf[dataOff++] = 0x7f;

	/* PTP body */
	dataOff += 20;

	/* Set the output buffer */
	txiov.iov_base = txBuf;
	txiov.iov_len = dataOff;
	iov_iter_init(&dm->txMsgHdr.msg_iter, WRITE | ITER_KVEC, &txiov, 1, dataOff );

	oldfs=get_fs();
	set_fs(KERNEL_DS);

	/* Send packet */
	if ((errCode = sock_sendmsg(dm->sock, &dm->txMsgHdr)) <= 0) {
		printk(KERN_WARNING "gPTP sendSyncMsg Socket transmission fails %d \n", errCode);
		goto fserr;
	} else {
		res = 0;
		printk(KERN_WARNING "gPTP sendSyncMsg Socket transmission success \n");
	}
	
fserr:
	set_fs(oldfs);

	return res;		
}

static int getDelayReqTS(struct dmst* dm)
{
	int res = -1;
	int errCode;
	struct iovec rxiov;
	struct cmsghdr *cmsghdr;
	struct cmsghdr *pcmsghdr;
	int cmsgcount = 0;

	/* Set the output buffer */
	rxiov.iov_base = rxBuf;
	rxiov.iov_len = GPTP_SYNCMSG_ETH_FRAME_SIZE;
	iov_iter_init(&dm->rxMsgHdr.msg_iter, READ | ITER_KVEC, &rxiov, 1, GPTP_SYNCMSG_ETH_FRAME_SIZE);

	/* Destination MAC */
	dm->rxSockAddress.sll_addr[0] = 0x01;
	dm->rxSockAddress.sll_addr[1] = 0x80;
	dm->rxSockAddress.sll_addr[2] = 0xC2;
	dm->rxSockAddress.sll_addr[3] = 0x00;
	dm->rxSockAddress.sll_addr[4] = 0x00;
	dm->rxSockAddress.sll_addr[5] = 0x0E;	
	
	/* Receive packet */
	/* rxcount = 0;
	if (((errCode = sock_recvmsg(dm->sock, &dm->rxMsgHdr, GPTP_SYNCMSG_ETH_FRAME_SIZE, 0)) > 0) && (rxcount < 10)) {
		res = 0;
		rxcount++;
		printk(KERN_WARNING "gPTP getDelayReqTS Socket reception success with msgsize %d cmsgsize %d \n", errCode, dm->rxMsgHdr.msg_controllen);
		cmsgcount = 0;
		cmsghdr = CMSG_FIRSTHDR(&dm->rxMsgHdr);
		while ((cmsghdr) && (cmsgcount < 3)) {
			printk(KERN_WARNING "gPTP getDelayReqTS Cmsg lvl:%d type:%d len:%d", cmsghdr->cmsg_level, cmsghdr->cmsg_type, cmsghdr->cmsg_len);
			cmsgcount++;
			pcmsghdr = cmsghdr;
			cmsghdr = CMSG_NXTHDR(&dm->rxMsgHdr, pcmsghdr);	
		}
	} else {
		printk(KERN_WARNING "gPTP getDelayReqTS Socket reception fails %d \n", errCode);
	} */

	msleep(1500);
	
	if ((errCode = sock_recvmsg(dm->sock, &dm->rxMsgHdr, GPTP_SYNCMSG_ETH_FRAME_SIZE, MSG_ERRQUEUE)) > 0) {
		res = 0;
		printk(KERN_WARNING "gPTP getDelayReqTS Err Socket reception success with msgsize %d cmsgsize %d \n", errCode, dm->rxMsgHdr.msg_controllen);
		cmsghdr = CMSG_FIRSTHDR(&dm->rxMsgHdr);
		cmsgcount = 0;
		cmsghdr = CMSG_FIRSTHDR(&dm->rxMsgHdr);
		while ((cmsghdr) && (cmsgcount < 3)) {
			printk(KERN_WARNING "gPTP getDelayReqTS Err Cmsg lvl:%d type:%d len:%d", cmsghdr->cmsg_level, cmsghdr->cmsg_type, cmsghdr->cmsg_len);
			cmsgcount++;
			pcmsghdr = cmsghdr;
			cmsghdr = CMSG_NXTHDR(&dm->rxMsgHdr, pcmsghdr);	
		}
	} else {
		printk(KERN_WARNING "gPTP getDelayReqTS Err Socket reception fails %d \n", errCode);
	}

	return res;
}

