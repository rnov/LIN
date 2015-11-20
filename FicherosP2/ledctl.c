#include <linux/syscalls.h> /* For SYSCALL_DEFINEi() */
#include <linux/kernel.h>

#include <asm-generic/errno.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>

/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask){
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
}

/* Get driver handler */
struct tty_driver* get_kbd_driver_handler(void){
   printk(KERN_INFO "modleds: loading\n");
   printk(KERN_INFO "modleds: fgconsole is %x\n", fg_console);
   return vc_cons[fg_console].d->port.tty->driver;
}

static unsigned int ledctl_to_set_leds(unsigned int mask)
{
	unsigned int scroll_lock = (mask >> 0) & 0xFFFFFFFE;
	unsigned int num_lock    = (mask >> 2) & 0xFFFFFFFE;
	unsigned int caps_lock   = (mask >> 1) & 0xFFFFFFFE;
	
	return (caps_lock << 2) | (num_lock << 1) | (scroll_lock);
}

SYSCALL_DEFINE1(ledctl, unsigned int,leds)
{	
	struct tty_driver* kbd_driver= NULL;
	
	kbd_driver= get_kbd_driver_handler();
	
	if(leds<0 || leds>7 )
		return -1;
			
	set_leds(kbd_driver,ledctl_to_set_leds(leds));
	return 0;
}
