#include <linux/module.h> 
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>
#include <linux/proc_fs.h>

#define ALL_LEDS_ON 0x7
#define ALL_LEDS_OFF 0

static struct proc_dir_entry *proc_entry;
static struct tty_driver* kbd_driver= NULL;

/* Get driver handler */
struct tty_driver* get_kbd_driver_handler(void){
   printk(KERN_INFO "modleds: loading\n");
   printk(KERN_INFO "modleds: fgconsole is %x\n", fg_console);
   return vc_cons[fg_console].d->port.tty->driver;
}

/* Set led state to that specified by mask */
static inline int set_leds(struct tty_driver* handler, unsigned int mask){
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
}

static ssize_t key_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
	
	unsigned char kbuf [4] = {}; /*No meter valores mayores a 4Bytes*/
	unsigned int aux;
	
	/* Transfer data from user to kernel space */
	if (copy_from_user(kbuf, buf, len )!=0)  
		return -ENOSPC;	
	
	kbuf[len] = '\0'; /* Add the `\0' */
	
	sscanf(kbuf,"%x",&aux);
	
	if(aux<0 || aux>7 ){  /* Fuera de rango */
		set_leds(kbd_driver,ALL_LEDS_OFF);		
	}else{
		set_leds(kbd_driver,aux);
		}
	
	*off+=len;        /* Update the file pointer */
	return len;
}

static const struct file_operations proc_entry_fops = {
	.write = key_write,    
};

static int __init modleds_init(void)
{	
   proc_entry = proc_create( "modleds", 0666, NULL, &proc_entry_fops);
   kbd_driver= get_kbd_driver_handler();
   return 0;
}

static void __exit modleds_exit(void){
    remove_proc_entry("modleds", NULL);
    set_leds(kbd_driver,ALL_LEDS_OFF); 
}

module_init(modleds_init);
module_exit(modleds_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modleds");
