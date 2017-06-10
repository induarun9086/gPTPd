
#include "log.h"

static void gptp_init(void);
static void gptp_exit(void);

static void gptp_init(void)
{
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
                
        /* Open logs */
	gPTP_openLog();       
                
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

#ifdef GPTPD_BUILD_X_86
	gPTP_logMsg(LOG_DEBUG, "gPTP Init x86\n");
#else
	gPTP_logMsg(LOG_DEBUG, "gPTP Init bbb\n");
#endif
}

static void gptp_start(void)
{
	
}

static void gptp_exit(void)
{
	gPTP_logMsg(LOG_DEBUG, "gPTP Exit \n");
	gPTP_closeLog();
}

int main(int argc, char* argv[])
{
	gptp_init();

	gptp_start();

	/* Event loop */
        while (1) {
           sleep(30);
        }

	gptp_exit();

	return 0;
}

