CROSSCC=/home/indumathi/Documents/GSoc/tools/gcc/gcc-linaro-6.3.1-2017.02-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc

x86: log.c gptp.c
	gcc -o gptpdx86 -DGPTPD_BUILD_X_86 log.c gptp.c

bbb: log.c gptp.c
	$(CROSSCC) -o gptpd log.c gptp.c

.PHONY: clean

clean:
	rm -f gptpdx86 gptpd
	
