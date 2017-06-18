
#include "delaymsr.h"
#include "bmc.h"
#include "sync.h"

struct gPTPd gPTPd;

static void gptp_init(int argc, char* argv[]);
static void gptp_setup(void);
static void gptp_start(void);
static int gptp_parseMsg(void);
static void gptp_handleEvent(int evt);
static void gptp_exit(void);

static void gptp_init(int argc, char* argv[])
{
	int argIdx = 1;
	int logLvl = GPTP_LOG_LVL_DEFAULT;

	/* Initialize */
	memset(&gPTPd, 0, sizeof(struct gPTPd));

	/* Setup default parameters */
	gPTPd.logLevel = GPTP_LOG_LVL_DEFAULT;
	gPTPd.daemonMode = TRUE;
	strcpy(&gPTPd.ifName[0], "eth0");

	/* Parse command line arguments */
	while((argIdx+1) <= argc) {
		if(argv[argIdx][0] == '-') {
			switch(argv[argIdx][1]) {
				case 'N':
				case 'n':
					gPTPd.daemonMode = FALSE;
					break;
				case 'I':
				case 'i':
					strcpy(&gPTPd.ifName[0], &argv[argIdx][2]);
					break;
				case 'l':
				case 'L':
					sscanf(&argv[argIdx][2], "%d", &logLvl);
					if((logLvl < 0) || (logLvl > GPTP_LOG_DEBUG))
						logLvl = GPTP_LOG_LVL_DEFAULT;
					gPTPd.logLevel = logLvl;
				default:
					break;
			}
		}
		argIdx++;
	}
	
	/* Initialize modules */
	initDM(&gPTPd);
	initBMC(&gPTPd);
	initCS(&gPTPd);

	/* Init the state machines */
	dmHandleEvent(&gPTPd, GPTP_EVT_STATE_ENTRY);
	bmcHandleEvent(&gPTPd, GPTP_EVT_STATE_ENTRY);
}

static void gptp_setup(void)
{
	/* Check if starting in daemon mode */
	if(gPTPd.daemonMode == TRUE) {
		/* Our process ID and Session ID */
		pid_t pid, sid;
		
		/* Fork off the parent process */
		pid = fork();
		if (pid < 0) {
		        exit(EXIT_FAILURE);
		}
		/* If we got a good PID, then
		   we can exit the parent process. */
		if (pid > 0) {
		        exit(EXIT_SUCCESS);
		}

		/* Change the file mode mask */
		umask(0);       
		        
		/* Create a new SID for the child process */
		sid = setsid();
		if (sid < 0) {
		        /* Log the failure */
		        exit(EXIT_FAILURE);
		}
		
		/* Change the current working directory */
		if ((chdir("/")) < 0) {
		        /* Log the failure */
		        exit(EXIT_FAILURE);
		}
		
		/* Close out the standard file descriptors */
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		/* Open logs */
		gPTP_openLog(GPTP_LOG_DEST_SYSLOG, gPTPd.logLevel);
	} else {
		/* Open logs */
		gPTP_openLog(GPTP_LOG_DEST_CONSOLE, gPTPd.logLevel);
	}

	gPTP_logMsg(GPTP_LOG_NOTICE, "-------------------------------------\n");
#ifdef GPTPD_BUILD_X_86
	gPTP_logMsg(GPTP_LOG_NOTICE, "gPTP Init x86 %s %s\n", __DATE__, __TIME__);
#else
	gPTP_logMsg(GPTP_LOG_NOTICE, "gPTP Init bbb %s %s\n", __DATE__, __TIME__);
#endif
	gPTP_logMsg(GPTP_LOG_NOTICE, "-------------------------------------\n\n");

#if 0
	gPTP_logMsg(GPTP_LOG_DEBUG, "sizeof(time_t): %d\n", sizeof(time_t));
	gPTP_logMsg(GPTP_LOG_DEBUG, "sizeof(gPTPHdr): %d\n", sizeof(struct gPTPHdr));
	gPTP_logMsg(GPTP_LOG_DEBUG, "sizeof(gPTPHdr): %d\n\n", sizeof(struct ethhdr));
#endif
}

static void gptp_start(void)
{
	struct ifreq if_opts;
	int tsOpts = 0;
	struct hwtstamp_config hwcfg;
	struct timeval rxTimeout;

	/* Open RAW socket to send on */
	if ((gPTPd.sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_1588))) == -1) {
		gPTP_logMsg(GPTP_LOG_ERROR, "gPTP Socket creation error %d %d\n", gPTPd.sockfd, errno);
		exit(EXIT_FAILURE);
	}

	/* Get the index of the interface to send on */
	memset(&gPTPd.if_idx, 0, sizeof(struct ifreq));
	strncpy(gPTPd.if_idx.ifr_name, gPTPd.ifName, GPTP_IF_NAME_SIZE-1);
	if (ioctl(gPTPd.sockfd, SIOCGIFINDEX, &gPTPd.if_idx) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SIOCGIFINDEX err:%d\n", errno);
	/* Get the MAC address of the interface to send on */
	memset(&gPTPd.if_mac, 0, sizeof(struct ifreq));
	strncpy(gPTPd.if_mac.ifr_name, gPTPd.ifName, GPTP_IF_NAME_SIZE-1);
	if (ioctl(gPTPd.sockfd, SIOCGIFHWADDR, &gPTPd.if_mac) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SIOCGIFHWADDR err:%d\n", errno);

#ifndef GPTPD_BUILD_X_86

	/* Set HW timestamp */
	memset(&gPTPd.if_hw, 0, sizeof(struct ifreq));
	memset(&hwcfg, 0, sizeof(struct hwtstamp_config));
	strncpy(gPTPd.if_hw.ifr_name, gPTPd.ifName, GPTP_IF_NAME_SIZE-1);
	hwcfg.tx_type = HWTSTAMP_TX_ON;
	hwcfg.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
	gPTPd.if_hw.ifr_data = (void*)&hwcfg;
	if (ioctl(gPTPd.sockfd, SIOCSHWTSTAMP, &gPTPd.if_hw) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SIOCSHWTSTAMP err:%d\n", errno);
	else
	    gPTP_logMsg(GPTP_LOG_DEBUG, "HW tx:%d rxFilter: %d \n", hwcfg.tx_type, hwcfg.rx_filter);

	/* Set timestamp options */
	tsOpts = SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE | \
		 SOF_TIMESTAMPING_OPT_CMSG | SOF_TIMESTAMPING_OPT_ID;
	if (setsockopt(gPTPd.sockfd, SOL_SOCKET, SO_TIMESTAMPING, &tsOpts, sizeof(tsOpts)) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SO_TIMESTAMPING err:%d\n", errno);

#else

	/* Set timestamp options */
	tsOpts = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE | \
		 SOF_TIMESTAMPING_OPT_CMSG | SOF_TIMESTAMPING_OPT_ID;
	if (setsockopt(gPTPd.sockfd, SOL_SOCKET, SO_TIMESTAMPING, &tsOpts, sizeof(tsOpts)) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SO_TIMESTAMPING err:%d\n", errno);

#endif

	/* Set rx timeout options */
	rxTimeout.tv_sec = 1;
	rxTimeout.tv_usec = 0;
	if (setsockopt(gPTPd.sockfd, SOL_SOCKET, SO_RCVTIMEO, &rxTimeout, sizeof(rxTimeout)) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SO_RCVTIMEO err:%d\n", errno);

	/* Set error queue options */
	tsOpts = 1;
	if (setsockopt(gPTPd.sockfd, SOL_SOCKET, SO_SELECT_ERR_QUEUE, &tsOpts, sizeof(tsOpts)) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SO_SELECT_ERR_QUEUE err:%d\n", errno);

	/* Set to promiscuous mode */
	strncpy(if_opts.ifr_name, gPTPd.ifName, GPTP_IF_NAME_SIZE-1);
	ioctl(gPTPd.sockfd, SIOCGIFFLAGS, &if_opts);
	if_opts.ifr_flags |= IFF_PROMISC;
	ioctl(gPTPd.sockfd, SIOCSIFFLAGS, &if_opts);

	/* Set reuse socket */
	tsOpts = 1;
	if (setsockopt(gPTPd.sockfd, SOL_SOCKET, SO_REUSEADDR, &tsOpts, sizeof(tsOpts)) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SO_REUSEADDR err:%d\n", errno);

	/* Bind to device */
	if (setsockopt(gPTPd.sockfd, SOL_SOCKET, SO_BINDTODEVICE, gPTPd.ifName, GPTP_IF_NAME_SIZE-1) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SO_BINDTODEVICE err:%d\n", errno);

	/* Index of the network device */
	gPTPd.txSockAddress.sll_family = AF_PACKET;
	gPTPd.txSockAddress.sll_protocol = htons(ETH_P_1588);
	gPTPd.txSockAddress.sll_ifindex = gPTPd.if_idx.ifr_ifindex;
	/* Address length*/
	gPTPd.txSockAddress.sll_halen = ETH_ALEN;
	/* Destination MAC */
	gPTPd.txSockAddress.sll_addr[0] = 0x01;
	gPTPd.txSockAddress.sll_addr[1] = 0x80;
	gPTPd.txSockAddress.sll_addr[2] = 0xC2;
	gPTPd.txSockAddress.sll_addr[3] = 0x00;
	gPTPd.txSockAddress.sll_addr[4] = 0x00;
	gPTPd.txSockAddress.sll_addr[5] = 0x0E;

	/* Set the message header */
	gPTPd.txMsgHdr.msg_control=NULL;
	gPTPd.txMsgHdr.msg_controllen=0;
	gPTPd.txMsgHdr.msg_flags=0;
	gPTPd.txMsgHdr.msg_name=&gPTPd.txSockAddress;
	gPTPd.txMsgHdr.msg_namelen=sizeof(struct sockaddr_ll);

	/* Index of the network device */
	gPTPd.rxSockAddress.sll_family = AF_PACKET;
	gPTPd.rxSockAddress.sll_protocol = htons(ETH_P_1588);
	gPTPd.rxSockAddress.sll_ifindex = gPTPd.if_idx.ifr_ifindex;
	/* Address length*/
	gPTPd.rxSockAddress.sll_halen = ETH_ALEN;
	/* Destination MAC */
	gPTPd.rxSockAddress.sll_addr[0] = 0x01;
	gPTPd.rxSockAddress.sll_addr[1] = 0x80;
	gPTPd.rxSockAddress.sll_addr[2] = 0xC2;
	gPTPd.rxSockAddress.sll_addr[3] = 0x00;
	gPTPd.rxSockAddress.sll_addr[4] = 0x00;
	gPTPd.rxSockAddress.sll_addr[5] = 0x0E;

	/* Set the message header */
	gPTPd.rxiov.iov_base = gPTPd.rxBuf;
	gPTPd.rxiov.iov_len  = GPTP_RX_BUF_SIZE;
	gPTPd.rxMsgHdr.msg_iov = &gPTPd.rxiov;
	gPTPd.rxMsgHdr.msg_iovlen = 1;
	gPTPd.rxMsgHdr.msg_control=gPTPd.tsBuf;
	gPTPd.rxMsgHdr.msg_controllen=GPTP_CON_TS_BUF_SIZE;
	gPTPd.rxMsgHdr.msg_flags=0;
	gPTPd.rxMsgHdr.msg_name=&gPTPd.rxSockAddress;
	gPTPd.rxMsgHdr.msg_namelen=sizeof(struct sockaddr_ll);	
}

static int gptp_parseMsg(void)
{
	int evt = GPTP_EVT_NONE;
	struct ethhdr * eh = (struct ethhdr *)&gPTPd.rxBuf[0];
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd.rxBuf[sizeof(struct ethhdr)];

	if(eh->h_proto == htons(ETH_P_1588)) {
		switch(gh->h.f.b1.msgType & 0x0f)
		{
			case GPTP_MSG_TYPE_PDELAY_REQ:
				gPTPd.dm.rxSeqNo = gptp_chgEndianess16(gh->h.f.seqNo);
				memcpy(&gPTPd.dm.reqPortIden[0], &gh->h.f.srcPortIden[0], GPTP_PORT_IDEN_LEN);
				gPTP_logMsg(GPTP_LOG_NOTICE, "gPTP PDelayReq (%d) Rcvd \n", gPTPd.dm.rxSeqNo);
				evt = GPTP_EVT_DM_PDELAY_REQ;
				break;
			case GPTP_MSG_TYPE_PDELAY_RESP:
				gPTP_logMsg(GPTP_LOG_NOTICE, "gPTP PDelayResp (%d) Rcvd \n", gptp_chgEndianess16(gh->h.f.seqNo));
				evt = GPTP_EVT_DM_PDELAY_RESP;
				break;
			case GPTP_MSG_TYPE_PDELAY_RESP_FLWUP:
				gPTP_logMsg(GPTP_LOG_NOTICE, "gPTP PDelayRespFlwUp (%d) Rcvd \n", gptp_chgEndianess16(gh->h.f.seqNo));
				evt = GPTP_EVT_DM_PDELAY_RESP_FLWUP;
				break;
			case GPTP_MSG_TYPE_ANNOUNCE:
				gPTP_logMsg(GPTP_LOG_NOTICE, "gPTP Announce (%d) Rcvd \n", gptp_chgEndianess16(gh->h.f.seqNo));
				evt = GPTP_EVT_BMC_ANNOUNCE_MSG;
				break;
			case GPTP_MSG_TYPE_SYNC:
				gPTP_logMsg(GPTP_LOG_NOTICE, "gPTP Sync (%d) Rcvd \n", gptp_chgEndianess16(gh->h.f.seqNo));
				evt = GPTP_EVT_CS_SYNC_MSG;
				break;
			case GPTP_MSG_TYPE_SYNC_FLWUP:
				gPTP_logMsg(GPTP_LOG_NOTICE, "gPTP SyncFlwUp (%d) Rcvd \n", gptp_chgEndianess16(gh->h.f.seqNo));
				evt = GPTP_EVT_DM_PDELAY_RESP_FLWUP;
				break;
			default:
				break;
		}
	};

	gPTP_logMsg(GPTP_LOG_DEBUG, "gPTP parseMsg %d 0x%x 0x%x\n", eh->h_proto, gh->h.f.b1.msgType, evt);

	return evt;
}

static void gptp_handleEvent(int evt)
{
	/* Handle the events when available */
	if (evt != GPTP_EVT_NONE) {
		switch(evt & GPTP_EVT_DEST_MASK) {
			case GPTP_EVT_DEST_DM:
				dmHandleEvent(&gPTPd, evt);
				break;
			case GPTP_EVT_DEST_BMC:
				bmcHandleEvent(&gPTPd, evt);
				break;
			case GPTP_EVT_DEST_CS:
				csHandleEvent(&gPTPd, evt);
				break;
			default:
				gPTP_logMsg(GPTP_LOG_ERROR, "gPTP unknown evt 0x%x\n", evt);
				break;
		}
	}
}

static void gptp_exit(void)
{
	unintCS(&gPTPd);
	unintBMC(&gPTPd);
	unintDM(&gPTPd);
	close(gPTPd.sockfd);
	gPTP_logMsg(GPTP_LOG_NOTICE, "gPTP Exit \n");
	gPTP_closeLog();
}

int main(int argc, char* argv[])
{
	/* Initialize */
	gptp_init(argc, argv);

	/* Setup the module */
	gptp_setup();

	/* Start operations */
	gptp_start();

	/* Initialize tx */
	gptp_initTxBuf(&gPTPd);

	/* Start the state machines */
	dmHandleEvent(&gPTPd, GPTP_EVT_DM_ENABLE);
	bmcHandleEvent(&gPTPd, GPTP_EVT_BMC_ENABLE);
	csHandleEvent(&gPTPd, GPTP_EVT_CS_ENABLE);

	/* Event loop */
        while (1) {
		int cnt = 0;
		int evt = GPTP_EVT_NONE;
		u64 currTickTS = gptp_getCurrMilliSecTS();

		gPTP_logMsg(GPTP_LOG_DEBUG, "\n");

		/* Check for any timer event */
		for(int i = 0; i < GPTP_NUM_TIMERS; i++) {
			if (gPTPd.timers[i].timeInterval > 0) {
				gPTP_logMsg(GPTP_LOG_DEBUG, "gPTP timer %d timeInt %lu timeEvt %d diffTS %ld\n",
					    i, gPTPd.timers[i].timeInterval, gPTPd.timers[i].timerEvt, (currTickTS - gPTPd.timers[i].lastTS));
				/* When the requested time elapsed for this timer */
				if((gPTPd.timers[i].lastTS + gPTPd.timers[i].timeInterval) < currTickTS)			
				{
					/* Update and handle the timer event */
					gPTPd.timers[i].lastTS = currTickTS;
					gptp_handleEvent(gPTPd.timers[i].timerEvt);
				}
			}
		}

		/* Wait for GPTP events/messages */
		gptp_initRxBuf(&gPTPd);
		cnt = recvmsg(gPTPd.sockfd, &gPTPd.rxMsgHdr, 0);

		gPTP_logMsg(GPTP_LOG_DEBUG, "gPTP recvmsg %d %d\n", cnt, errno);

		/* When data available parse the command */
		if (cnt >= 1) {
			evt = gptp_parseMsg();
			/* And handle the received command */
			gptp_handleEvent(evt);
		} else {
			sleep(1);
		}
        }

	/* Cleanup */
	gptp_exit();

	return 0;
}

