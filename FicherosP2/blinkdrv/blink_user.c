#include <stdio.h>
#include <string.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	int fd;
	const char buf [] = {"1:0x111100,2:0x111100"};
	fd=open("/dev/usb/blinkstick0",O_WRONLY);
	if(fd > 0 ){
		if(write(fd,buf,sizeof(buf))== -1) 
			printf("Write error \n");
	}
	else 
		printf("open error \n");
	
	printf("%i \n",fd);
	close(fd);
	
	return 0;
}

