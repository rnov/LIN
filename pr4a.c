#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/list.h> 
#include <linux/semaphore.h>
#include <asm-generic/uaccess.h>
#include <asm-generic/errno.h>
#include "cbuffer.h"

#define MAX_KBUFF 256
#define DATA_TYPE unsigned int

#define Foreach(pos, aux, list)            \
     {                                    \
        struct list_head *pos, *aux;       \
        list_item_t *tmp;                \
    list_for_each_safe(pos, aux, list) {

#define Endforeach() }}

/* Work descriptor */
struct work_struct my_work;

/* Timer Structure that describes the kernel timer */
struct timer_list my_timer;

/* Buffer circular */
cbuffer_t* cbuffer; 

/* Spin lock for cbuffer */
DEFINE_SPINLOCK(mr_lock);

/* Semaphores */
/* Macro init sem at 1 */
DEFINE_SEMAPHORE(sem);
struct semaphore queue_sem;

/* Linked List Node */
typedef struct {
    unsigned int data;
    struct list_head links;
}list_item_t;

/* Linked list */
struct list_head mylist; 

/* operations proc structs */
static struct proc_dir_entry *proc_entry_conf;
static struct proc_dir_entry *proc_entry_timer;

/* Default glob var val */
static unsigned long timer_p = 500;
static unsigned int max_rand = 3000, emergency = 80;

/* cpu flags used in spin locks*/
static unsigned long cpu_flag;
/* Nº of processes waiting */
static int nr_proc_waiting = 0; 

/* work function (my_wq_function)*/
static void copy_items_into_list( struct work_struct *work ) {	

	list_item_t *new_node;
	unsigned int num;
	int buffer_size;
	
	spin_lock_irqsave(&mr_lock, cpu_flag);
	buffer_size = size_cbuffer_t(cbuffer);
	spin_unlock_irqrestore(&mr_lock,cpu_flag);

	down_interruptible(&sem);
	/* while there're nºs in buffer fill the list */
	while(buffer_size > 0){
		new_node = vmalloc(sizeof(list_item_t));
		
		spin_lock_irqsave(&mr_lock, cpu_flag);
		printk(KERN_INFO "Removed from buffer: %u \n", *(head_cbuffer_t(cbuffer)));
		num = *(head_cbuffer_t(cbuffer));
		remove_cbuffer_t(cbuffer);
		spin_unlock_irqrestore(&mr_lock, cpu_flag);
		
		new_node->data = num;
		list_add_tail(&(new_node->links), &mylist);
		buffer_size--;
	}
	/* wakes up waiting process (modtimer_read) (Upper layer) */
	if(nr_proc_waiting >0){
		up(&queue_sem);
		nr_proc_waiting--;
		}

	up(&sem);	
}

/* Work's handler function (my_timer_function) (top half) */
static void fire_timer(unsigned long data) {    
    int buffer_size;
    unsigned int rand_val = get_random_int()%max_rand; 
    int current_cpu = smp_processor_id();
	
	printk(KERN_INFO "Generated number: %u \n", rand_val);
    
    spin_lock_irqsave(&mr_lock, cpu_flag);
    insert_cbuffer_t(cbuffer, rand_val);
    buffer_size = size_cbuffer_t(cbuffer);
    spin_unlock_irqrestore(&mr_lock,cpu_flag);  
    
    if((buffer_size/(double)MAX_SIZE * 100) >= emergency){
		/* Enqueue work in different cpu (bottom-half) */
		if(current_cpu){
			schedule_work_on(0,&my_work);
		}else{
			schedule_work_on(1,&my_work);
		}
	}	
    /* Re-activate the timer one second from now */
	mod_timer( &(my_timer), jiffies + timer_p); 
}

static ssize_t config_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    char kbuff[MAX_KBUFF];
    char* buffpos = kbuff;

    if((*off) > 0)
        return 0;
	
	buffpos += sprintf(buffpos, "timer_period_ms=%lu \nemergency_threshold=%u \nmax_random=%u \n",timer_p ,emergency, max_rand);
    
    if(buffpos > &kbuff[0] + 1)
        buffpos--; // Go back one byte to overwrite last colon
 
    size_t written = min(len, buffpos - &kbuff[0]);
    kbuff[written] = '\0';
    
    if(copy_to_user(buf, kbuff, written))
        return -EINVAL;
    
    *off += written;
    return written;
}

static ssize_t config_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {

	char kbuf[MAX_KBUFF];
	if(len >= MAX_KBUFF)
		return -ENOMEM;    

	/* Transfer data from user to kernel space */
	if (copy_from_user(&kbuf, buf, len)>0)         
	   return -EFAULT;
	   
	kbuf[len]='\0';

	sscanf(kbuf, "emergency_threshold %u", &emergency);
	sscanf(kbuf, "timer_period_ms %lu", &timer_p);
	sscanf(kbuf, "max_random %u", &max_rand); 
         
    (*off)+=len;

    return len;
}

static const struct file_operations proc_entry_fops_config = {
    .read = config_read,
    .write = config_write,    
};

/* get from list and saves in buffer to be printed  */
static ssize_t modtimer_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    char kbuff[MAX_KBUFF] = {"\n"};  // first element "\n" MAX_KBUFF
    char* buffpos = kbuff+1;
    // max_elems kbuff can hold, -2B: \n \0
    int max_elems = MAX_KBUFF -2 / (sizeof(DATA_TYPE)+1) , count = 0; // MAX_KBUFF -2 (first \n and \0) , 35 for test
        
	if(down_interruptible(&sem)){return -EINTR;}
    
    /* while the list is empty ...*/
    while(list_empty(&mylist)){
		nr_proc_waiting++;
		up(&sem);
		if(down_interruptible(&queue_sem)){
			down(&sem);
			nr_proc_waiting--;
			up(&sem);
			return -EINTR;
			}
		if(down_interruptible(&sem)){return -EINTR;}
	}

    Foreach(pos, aux, &mylist)
        tmp = list_entry(pos, list_item_t,links);
		printk("Items in list removed: %u \n",tmp->data);
		list_del(pos);
		// checks whether there is enough space for nºs in kbuff
                if(count < max_elems){	
                    buffpos += sprintf(buffpos, "%u\n", tmp->data);
                }else{
                    printk("max nº of items in buffer reached: %i cout: %i \n",max_elems,count);
                }
                vfree(tmp);
        count++;
	Endforeach()
	
	up(&sem);
	
    if(buffpos > &kbuff[0] + 1)
        buffpos--; // Go back one byte to overwrite last colon
 
    size_t written = min(len, buffpos - &kbuff[0]);
    kbuff[written] = '\0';

    if(copy_to_user(buf, kbuff, written))
        return -EINVAL;
        
    *off += written;

    return written;
}

/* Se invoca al hacer open() de entrada /proc */
static int modtimer_open(struct inode *inode, struct file *file) {
	try_module_get(THIS_MODULE);
	/* Activate the timer */
	add_timer(&my_timer);
	return 0;
}

/* Se invoca al hacer close() de entrada /proc */
static int modtimer_release(struct inode *inode, struct file *file) {	
	
	/* Wait until completion of the timer function (if it's currently running) and delete timer */  
	del_timer_sync(&my_timer);
	/* Wait until all jobs scheduled so far have finished */
	flush_scheduled_work();
	/* clear cbuffer*/
	clear_cbuffer_t(cbuffer);
	/* remove list */
     Foreach(pos, aux, &mylist)
        tmp = list_entry(pos, list_item_t,links);
		list_del(pos);
		vfree(tmp);
    Endforeach()
    
	module_put(THIS_MODULE);
	return 0;
}

static const struct file_operations proc_entry_fops_timer = {
    .read = modtimer_read,   
    .open = modtimer_open,
    .release = modtimer_release,
};

int init_timer_module( void ) {

/* Work Queue and Linked List (Bottom-half) */
	/* Initialize work structure (with function) */
	INIT_WORK(&my_work, copy_items_into_list);  // my_wq_function
	/* initialize *cbuffer */
	cbuffer = create_cbuffer_t();
	/* initializa the list */
	INIT_LIST_HEAD(&mylist);  

/* Timer (Top-half) */
	/* Create timer */
	init_timer(&my_timer);	
	/* Initialize timer funcion input*/
	my_timer.data=0;  	
	/* Set timer function*/
	my_timer.function=fire_timer;  /* func to run when timer expires */	
	/*glob var timer */
	my_timer.expires = jiffies + timer_p;  /* timer expires in timer_p ticks */	
	
/* Proc entry fops registrations (Upper layer) */
	/* entry proc reg. */
	proc_entry_conf = proc_create("modconfig",0666, NULL, &proc_entry_fops_config);
	proc_entry_timer = proc_create("modtimer",0666, NULL, &proc_entry_fops_timer);
	if(proc_entry_conf == NULL){  
        printk(KERN_INFO "timerMod: Can't create /proc/modconfig\n");
        //return -ENOMEM;       
	}
	if(proc_entry_timer == NULL){
        printk(KERN_INFO "timerMod: Can't create /proc/modtimer\n");
        return -ENOMEM;     
	}
	/* Init queue semaphore at 0*/
	sema_init(&queue_sem,0);	
	
	printk(KERN_INFO "timerMod: Module loaded\n");
	printk(KERN_INFO "timerMod: created fops /proc/modconfig\n");
	printk(KERN_INFO "timerMod: created fops /proc/modtimer\n");
	return 0;
}

void cleanup_timer_module( void ) {
	
	/* Destroy cbuffer*/
	destroy_cbuffer_t(cbuffer);
	/* remove proc entry modconfig */
	remove_proc_entry("modconfig",NULL);
	remove_proc_entry("modtimer",NULL);
	
	printk(KERN_INFO "timerMod: Module unloaded\n");
}

module_init( init_timer_module );
module_exit( cleanup_timer_module );

/* NOTA: sudo tail -f /var/log/kern.log */

