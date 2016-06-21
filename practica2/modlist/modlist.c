#include <linux/module.h> /* Requerido por todos los modulos */
#include <linux/kernel.h> /* Definicion de KERN_INFO */
#include <linux/proc_fs.h> /* file_system_operations */
#include <linux/list.h> /* Hacemos uso de lista */
#include <asm/uaccess.h> /* Hacemos uso cpy espacio usuario<-> kenel*/
#include <linux/ftrace.h> /* debugging */

MODULE_LICENSE("GPL"); /* Licencia del modulo */

/* Nodos de la lista */
typedef struct {
    int data;
    struct list_head links;
}list_item_t;

struct list_head mylist; /* Lista enlazada */

static struct proc_dir_entry *proc_entry;

#define MAX_CHARS = 10;

#define Foreach(it, tmp, list)            \
     {                                    \
        struct list_head *it, *tmp;       \
        list_item_t *elem;                \
    list_for_each_safe(it, tmp, list) {

#define Endforeach() }}

/* To do*/
static ssize_t list_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    char kbuf[len]; int num;

    /* Transfer data from user to kernel space */
        if (copy_from_user(&kbuf, buf, len))  
        return -EFAULT;

    if(sscanf(kbuf, "add %i", &num) == 1) {  // add command
        trace_printk("Executing command 'add %i'\n", num);
        
        list_item_t *new_node;
        new_node = vmalloc(sizeof(list_item_t));
        new_node->data = num;
        
        list_add_tail(&(new_node->links), &mylist); 
    }else if(sscanf(kbuf, "remove %i", &num) == 1){  // remove command, 
        trace_printk("Executing command 'remove %i'\n", num);
        Foreach(pos, aux, &mylist)
            elem = list_entry(pos, list_item_t, links);
            trace_printk("At elem '%i' (@%p)\n", elem->data, elem);

            if(elem->data == num){
                trace_printk("Found elem '%i'. Removing...\n", elem->data);
                list_del(pos);
                vfree(elem);
            }
        Endforeach()
    }else if(sscanf(kbuf, "cleanup") == 0){  // cleanup command, 
        trace_printk("Executing command 'cleanup'\n");
        Foreach(pos, aux, &mylist)
            elem = list_entry(pos, list_item_t, links);
            
            trace_printk("At elem '%i' (@%p)\n", elem->data, elem);
            list_del(pos);
            vfree(elem);
        Endforeach()
    }else
        printk(KERN_INFO "Unknown command");

    (*off)+=len;

    return len;
}

/* To do*/
static ssize_t list_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    char kbuff[1024] = {};
    char* buffpos = &kbuff[0];

    if((*off) > 0)
        return 0;

    buffpos += sprintf(buffpos, "(");

    Foreach(pos, aux, &mylist)
        elem = list_entry(pos, list_item_t, links);
        trace_printk("At elem '%i' (@%p)\n", elem->data, elem);
        
        buffpos += sprintf(buffpos, "%i,", elem->data);
    Endforeach()

    if(buffpos > &kbuff[0] + 1)
        buffpos--; // Go back one byte to overwrite last colon

    buffpos += sprintf(buffpos, ")\n");

    size_t written = min(len, buffpos - &kbuff[0]);

    kbuff[written] = '\0';

    trace_printk("Output: '%s'", kbuff);

    if(copy_to_user(buf, kbuff, written))
        return -EINVAL;
    
    *off += written;

    return written;
}

/* Especifica operaciones /proc se implementan */
static const struct file_operations proc_entry_fops = {
    .read = list_read,
    .write = list_write,    
};

/* done?*/
int init_list_module( void )
{
    int ret = 0;
    proc_entry = proc_create( "modlist", 0666, NULL, &proc_entry_fops);
    if(proc_entry == NULL){
        ret = -ENOMEM;
        printk(KERN_INFO "modlist: Can't create /proc entry\n");
    }else{
        /* Inicializamos la lista*/
        INIT_LIST_HEAD(&mylist);
        printk(KERN_INFO "modlist: Module loaded\n");
    }
    
    return ret;
}

/* done*/
void exit_list_module( void )
{
  remove_proc_entry("modlist", NULL);
  struct list_head *pos, *aux;
  list_item_t *tmp;
  list_for_each_safe(pos, aux, &mylist){
      tmp = list_entry(pos, list_item_t, links);
      list_del(pos);
      vfree(tmp);
  }
  printk(KERN_INFO "modlist: Module unloaded.\n");
}


/* Declaracion de funciones init y cleanup */
module_init(init_list_module);
module_exit(exit_list_module);


/**
 * list_entry - get the struct for this entry
 * @ptr:    the &struct list_head pointer.
 * @type:   the type of the struct this is embedded in.
 * @member: the name of the list_struct within the struct.
 */

 /**
 * list_for_each_safe   -   iterate over a list safe against removal of list entry
 * @pos:    the &struct list_head to use as a loop counter.
 * @n:      another &struct list_head to use as temporary storage
 * @head:   the head for your list.
 */

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this, the entry is
 * in an undefined state.
 */

 /**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
