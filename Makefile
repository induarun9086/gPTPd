CC=$(CROSS_COMPILE)gcc

x86: log.c gptp.c
	$(CC) -g -o gptpdx86 -DGPTPD_BUILD_X_86 log.c gptp.c

bbb: log.c gptp.c
	$(CC) -g -o gptpd log.c gptp.c

.PHONY: clean

clean:
	rm -f *.o gptpdx86 gptpd
	
