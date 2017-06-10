
#include "log.h"

struct gPTPd gPTPd;

static void gptp_init(int argc, char* argv[]);
static void gptp_setup(void);
static void gptp_start(void);
static void gptp_exit(void);

static void gptp_init(int argc, char* argv[])
{
	/* Initialize */
	memset(&gPTPd, 0, sizeof(struct gPTPd));

	/* Setup default parameters */
	gPTPd.daemonMode = TRUE;

	/* Parse command line arguments */
	if(argc >= 2) {
		if(!strcmp(argv[1], "-N")) {
			gPTPd.daemonMode = FALSE;
		}
	}
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
	
}

static void gptp_exit(void)
{
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
           sleep(30);
        }

	/* Cleanup */
	gptp_exit();

	return 0;
}

