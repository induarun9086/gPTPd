
#include "delaymsr.h"

struct gPTPd gPTPd;

static void gptp_init(int argc, char* argv[]);
static void gptp_setup(void);
static void gptp_start(void);
static void gptp_exit(void);

static void gptp_init(int argc, char* argv[])
{
	int argIdx = 1;

	/* Initialize */
	memset(&gPTPd, 0, sizeof(struct gPTPd));

	/* Setup default parameters */
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
		gPTP_openLog(GPTP_LOG_DEST_SYSLOG);
	} else {
		/* Open logs */
		gPTP_openLog(GPTP_LOG_DEST_CONSOLE);
	}

#ifdef GPTPD_BUILD_X_86
	gPTP_logMsg(GPTP_LOG_DEBUG, "gPTP Init x86\n");
#else
	gPTP_logMsg(GPTP_LOG_DEBUG, "gPTP Init bbb\n");
#endif
}

static void gptp_start(void)
{
	struct ifreq if_idx;
	struct ifreq if_mac;
	struct ifreq if_hw;
	int tsOpts = 0;
	struct hwtstamp_config hwcfg;

	/* Open RAW socket to send on */
	if ((gPTPd.sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_1588))) == -1) {
		gPTP_logMsg(GPTP_LOG_DEBUG, "gPTP Socket creation error %d %d\n", gPTPd.sockfd, errno);
		exit(EXIT_FAILURE);
	}

	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, gPTPd.ifName, GPTP_IF_NAME_SIZE-1);
	if (ioctl(gPTPd.sockfd, SIOCGIFINDEX, &if_idx) < 0)
	    gPTP_logMsg(GPTP_LOG_DEBUG, "SIOCGIFINDEX err:%d\n", errno);
	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, gPTPd.ifName, GPTP_IF_NAME_SIZE-1);
	if (ioctl(gPTPd.sockfd, SIOCGIFHWADDR, &if_mac) < 0)
	    gPTP_logMsg(GPTP_LOG_DEBUG, "SIOCGIFHWADDR err:%d\n", errno);

	/* Set HW timestamp */
	memset(&if_hw, 0, sizeof(struct ifreq));
	memset(&hwcfg, 0, sizeof(struct hwtstamp_config));
	strncpy(if_hw.ifr_name, gPTPd.ifName, GPTP_IF_NAME_SIZE-1);
	hwcfg.tx_type = HWTSTAMP_TX_ON;
	hwcfg.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
	if_hw.ifr_data = (void*)&hwcfg;
	if (ioctl(gPTPd.sockfd, SIOCSHWTSTAMP, &if_hw) < 0)
	    gPTP_logMsg(GPTP_LOG_DEBUG, "SIOCSHWTSTAMP err:%d\n", errno);
	else
	    gPTP_logMsg(GPTP_LOG_DEBUG, "HW tx:%d rxFilter: %d \n", hwcfg.tx_type, hwcfg.rx_filter);

#ifndef GPTPD_BUILD_X_86

	/* Set timestamp options */
	tsOpts = SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE | \
		 SOF_TIMESTAMPING_OPT_CMSG | SOF_TIMESTAMPING_OPT_ID;
	if (setsockopt(gPTPd.sockfd, SOL_SOCKET, SO_TIMESTAMPING, &tsOpts, sizeof(tsOpts)) < 0)
	    gPTP_logMsg(GPTP_LOG_DEBUG, "SO_TIMESTAMPING err:%d\n", errno);

#else

	/* Set timestamp options */
	tsOpts = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_SOFTWARE | \
		 SOF_TIMESTAMPING_OPT_CMSG | SOF_TIMESTAMPING_OPT_ID;
	if (setsockopt(gPTPd.sockfd, SOL_SOCKET, SO_TIMESTAMPING, &tsOpts, sizeof(tsOpts)) < 0)
	    gPTP_logMsg(GPTP_LOG_DEBUG, "SO_TIMESTAMPING err:%d\n", errno);

#endif

	/* Set error queue options */
	tsOpts = 1;
	if (setsockopt(gPTPd.sockfd, SOL_SOCKET, SO_SELECT_ERR_QUEUE, &tsOpts, sizeof(tsOpts)) < 0)
	    gPTP_logMsg(GPTP_LOG_DEBUG, "SO_SELECT_ERR_QUEUE err:%d\n", errno);

	/* Index of the network device */
	gPTPd.txSockAddress.sll_family = AF_PACKET;
	gPTPd.txSockAddress.sll_protocol = htons(ETH_P_1588);
	gPTPd.txSockAddress.sll_ifindex = 2;
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
	gPTPd.rxSockAddress.sll_ifindex = 2;
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
	gPTPd.rxMsgHdr.msg_control=gPTPd.tsBuf;
	gPTPd.rxMsgHdr.msg_controllen=GPTP_CON_TS_BUF_SIZE;
	gPTPd.rxMsgHdr.msg_flags=0;
	gPTPd.rxMsgHdr.msg_name=&gPTPd.rxSockAddress;
	gPTPd.rxMsgHdr.msg_namelen=sizeof(struct sockaddr_ll);
}

static void gptp_exit(void)
{
	unintDM(&gPTPd);
	close(gPTPd.sockfd);
	gPTP_logMsg(GPTP_LOG_DEBUG, "gPTP Exit \n");
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

	/* Event loop */
        while (1) {
		dmHandleEvent(&gPTPd, GPTP_EVT_ONE_SEC_TICK);
		sleep(1);
        }

	/* Cleanup */
	gptp_exit();

	return 0;
}

