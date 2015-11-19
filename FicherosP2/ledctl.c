#include <linux/syscalls.h> /* For SYSCALL_DEFINEi() */
#include <linux/kernel.h>

#include <asm-generic/errno.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>



SYSCALL_DEFINE1(ledctl, unsigned int,leds)
{	
	struct tty_driver* kbd_driver= NULL;
	
	kbd_driver= get_kbd_driver_handler();
	
	if(aux<0 || aux>7 )
		return -1;
			
	set_leds(kbd_driver,aux);
	return 0;
}
