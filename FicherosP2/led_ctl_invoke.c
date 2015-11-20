#include <linux/errno.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include <stdio.h>
#ifdef __i386__
#define __NR_LEDCTL 353
#else
#define __NR_LEDCTL 316
#endif

long ledctl_invoke(unsigned int leds) {
    return (long) syscall(__NR_LEDCTL, leds);
}
int main(int argc, char* argv[]) {
    unsigned int leds;
    
    if(sscanf(argv[1], "%x", &leds) == 1)
    	return ledctl_invoke(leds);
    else
	return -1;
}
