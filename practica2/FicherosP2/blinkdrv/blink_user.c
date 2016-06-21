#include <stdio.h>
#include <string.h>
#include <fcntl.h>


#define N_CHARS 87
#define N_COUNTRIES 7
int main(int argc, char **argv)
{	
	int fd, led_ct=-1, opt=-1,i=0, anw_c, right_c, aux_ct,aux_opt;
	char *ct_names[N_COUNTRIES] = {"Spain","France","Italy","Austria","Poland","Bulgaria","Ireland"};
	char sp [] = {"0:0x220000,1:0x220000,2:0x333300,3:0x333300,4:0x333300,5:0x333300,6:0x220000,7:0x220000"};
	char fr [] = {"0:0x000022,1:0x000022,2:0x222222,3:0x222222,4:0x220000,5:0x220000"};
	char it [] = {"0:0x002200,1:0x002200,2:0x222222,3:0x222222,4:0x220000,5:0x220000"};
	char aus [] = {"0:0x220000,1:0x220000,2:0x111111,3:0x111111,4:0x220000,5:0x220000"};
	char pl [] = {"0:0x222222,1:0x222222,2:0x222222,3:0x222222,4:0x220000,5:0x220000,6:0x220000,7:0x220000"};
	char bg [] = {"0:0x222222,1:0x222222,2:0x002200,3:0x002200,4:0x220000,5:0x220000"};
	char eyre [] = {"0:0x002200,1:0x002200,2:0x222222,3:0x222222,4:0x332200,5:0x332200"};
	char *ct_flags [N_COUNTRIES] = {sp,fr,it,aus,pl,bg,eyre};

	/*data to driver*/
	fd=open("/dev/usb/blinkstick0",O_WRONLY);
	if(fd > 0 ){
		srand(time(NULL));
		for(;i<4;i++){
			/*random options*/
			
			do{
				aux_ct = rand()%N_COUNTRIES;
				aux_opt = rand()%N_COUNTRIES;
			}while(aux_ct == aux_opt || aux_ct == led_ct || aux_opt == opt);
			led_ct = aux_ct;
			opt = aux_opt;

			if(write(fd,ct_flags[led_ct],N_CHARS)== -1)goto FAIL_WRITE;
			
			if(led_ct%2==0){
				printf("\na : %s		b : %s\n",ct_names[led_ct],ct_names[opt]);
				right_c = 'a';
			}else{
				printf("\na : %s		b : %s\n",ct_names[opt],ct_names[led_ct]);
				right_c = 'b';
			}
			system ("/bin/stty raw");
			/*get input buffer*/
			anw_c=getchar();
			/* use system call to set terminal behaviour to more normal behaviour */
			system ("/bin/stty cooked");
			if(anw_c == right_c)
				printf("\nCorrecto!\n");
			else
				printf("\nIncorrecto \n");		
		}
	}else{ 
		perror("Error: ");
		return -1;
	}
	
	if(write(fd,"",0)== -1) goto FAIL_WRITE;
	
	close(fd);
	return 0;
	
FAIL_WRITE:
	perror("Error!: ");
	close(fd);
	return -1;
}

