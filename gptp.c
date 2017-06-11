
#include "delaymsr.h"

struct gPTPd gPTPd;

static void gptp_init(int argc, char* argv[]);
static void gptp_setup(void);
static void gptp_start(void);
static void gptp_initTxBuf(void);
static int gptp_parseMsg(void);
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
	
	initDM(&gPTPd);
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

#ifdef GPTPD_BUILD_X_86
	gPTP_logMsg(GPTP_LOG_INFO, "gPTP Init x86 %s %s\n", __DATE__, __TIME__);
#else
	gPTP_logMsg(GPTP_LOG_INFO, "gPTP Init bbb %s %s\n", __DATE__, __TIME__);
#endif
}

static void gptp_start(void)
{
	struct ifreq if_idx;
	struct ifreq if_mac;
	struct ifreq if_hw;
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
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, gPTPd.ifName, GPTP_IF_NAME_SIZE-1);
	if (ioctl(gPTPd.sockfd, SIOCGIFINDEX, &if_idx) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SIOCGIFINDEX err:%d\n", errno);
	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, gPTPd.ifName, GPTP_IF_NAME_SIZE-1);
	if (ioctl(gPTPd.sockfd, SIOCGIFHWADDR, &if_mac) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SIOCGIFHWADDR err:%d\n", errno);

#ifndef GPTPD_BUILD_X_86

	/* Set HW timestamp */
	memset(&if_hw, 0, sizeof(struct ifreq));
	memset(&hwcfg, 0, sizeof(struct hwtstamp_config));
	strncpy(if_hw.ifr_name, gPTPd.ifName, GPTP_IF_NAME_SIZE-1);
	hwcfg.tx_type = HWTSTAMP_TX_ON;
	hwcfg.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
	if_hw.ifr_data = (void*)&hwcfg;
	if (ioctl(gPTPd.sockfd, SIOCSHWTSTAMP, &if_hw) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SIOCSHWTSTAMP err:%d\n", errno);
	else
	    gPTP_logMsg(GPTP_LOG_INFO, "HW tx:%d rxFilter: %d \n", hwcfg.tx_type, hwcfg.rx_filter);

	/* Set timestamp options */
	tsOpts = SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE | \
		 SOF_TIMESTAMPING_OPT_CMSG | SOF_TIMESTAMPING_OPT_ID;
	if (setsockopt(gPTPd.sockfd, SOL_SOCKET, SO_TIMESTAMPING, &tsOpts, sizeof(tsOpts)) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SO_TIMESTAMPING err:%d\n", errno);

#else

	/* Set timestamp options */
	tsOpts = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_TX_SCHED | SOF_TIMESTAMPING_SOFTWARE | \
		 SOF_TIMESTAMPING_OPT_CMSG | SOF_TIMESTAMPING_OPT_ID;
	if (setsockopt(gPTPd.sockfd, SOL_SOCKET, SO_TIMESTAMPING, &tsOpts, sizeof(tsOpts)) < 0)
	    gPTP_logMsg(GPTP_LOG_ERROR, "SO_TIMESTAMPING err:%d\n", errno);

#endif

	/* Set rx timeout options */
	rxTimeout.tv_sec = 100;
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
	gPTPd.txSockAddress.sll_ifindex = if_idx.ifr_ifindex;
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
	gPTPd.rxSockAddress.sll_ifindex = if_idx.ifr_ifindex;
	/* Address length*/
	gPTPd.rxSockAddress.sll_halen = ETH_ALEN;
	/* Destination MAC */
	gPTPd.txSockAddress.sll_addr[0] = 0x01;
	gPTPd.txSockAddress.sll_addr[1] = 0x80;
	gPTPd.txSockAddress.sll_addr[2] = 0xC2;
	gPTPd.txSockAddress.sll_addr[3] = 0x00;
	gPTPd.txSockAddress.sll_addr[4] = 0x00;
	gPTPd.txSockAddress.sll_addr[5] = 0x0E;

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

	/* Start the other state machines */
	//dmHandleEvent(&gPTPd, GPTP_EVT_DM_ENABLE);
	
}

static void gptp_initTxBuf(void)
{
	struct ethhdr *eh = (struct ethhdr *)gPTPd.txBuf;
	struct gPTPHdr * gh = (struct gPTPHdr *)&gPTPd.txBuf[sizeof(struct ethhdr)];

	/* Initialize it */
	memset(gPTPd.txBuf, 0, GPTP_TX_BUF_SIZE);

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

	/* Fill common gPTP header fields */
	gh->h.f.b1.tsSpec      = (0x01 << 4);
	gh->h.f.b2.ptpVer      = 0x02;
	gh->h.f.srcPortIden[0] = 0x04;
	gh->h.f.srcPortIden[1] = 0xA3;
	gh->h.f.srcPortIden[2] = 0x16;
	gh->h.f.srcPortIden[3] = 0xFF;
	gh->h.f.srcPortIden[4] = 0xFE;
	gh->h.f.srcPortIden[5] = 0xAD;
	gh->h.f.srcPortIden[6] = 0x3A;
	gh->h.f.srcPortIden[7] = 0x33;
	gh->h.f.srcPortIden[8] = 0x00;
	gh->h.f.srcPortIden[9] = 0x01;
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
				evt = GPTP_EVT_DM_PDELAY_REQ;
				break;
			default:
				break;
		}
	};

	return evt;
}

static void gptp_exit(void)
{
	unintDM(&gPTPd);
	close(gPTPd.sockfd);
	gPTP_logMsg(GPTP_LOG_INFO, "gPTP Exit \n");
	gPTP_closeLog();
}

int main(int argc, char* argv[])
{
	/* Initialize */
	gptp_init(argc, argv);

	/* Setup the module */
	gptp_setup();

	/* Initialize tx */
	gptp_initTxBuf();

	/* Start operations */
	gptp_start();

	/* Event loop */
        while (1) {
		int cnt = 0;
		int evt = GPTP_EVT_NONE;

		/* Wait for GPTP events/messages */
		memset(gPTPd.rxBuf, 0, GPTP_RX_BUF_SIZE);
		cnt = recvmsg(gPTPd.sockfd, &gPTPd.rxMsgHdr, 0);
	
		gPTP_logMsg(GPTP_LOG_DEBUG, "gPTP recvmsg %d %d\n", cnt, errno);

		if (cnt >= 1)
			evt = gptp_parseMsg(); 
		else if((cnt == -1) && (errno == EAGAIN))
			evt = GPTP_EVT_ONE_SEC_TICK;
		else
			sleep(1);

		/* Handle the events when available */
		if (evt != GPTP_EVT_NONE) {
			switch(evt & GPTP_EVT_DEST_MASK) {
				case GPTP_EVT_DEST_DM:
					dmHandleEvent(&gPTPd, evt);
					break;
				default:
					dmHandleEvent(&gPTPd, evt);
					break;
			}
		}
        }

	/* Cleanup */
	gptp_exit();

	return 0;
}

