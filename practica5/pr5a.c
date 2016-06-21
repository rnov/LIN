#include <linux/module.h> /* Requiered by all modules */
#include <linux/kernel.h> /* KERN_INFO */
#include <linux/proc_fs.h> /* File_system_operations */
#include <linux/list.h>
#include <asm/uaccess.h> /* user space <-> kenel */
#include <linux/ftrace.h> /* debugging */
#include <linux/string.h>
#include <linux/spinlock.h>  
/* sudo tail -f /var/log/kern.log */
MODULE_LICENSE("GPL"); 

#define MAX_MOD_NAME 12

#define Foreach(it, tmp, list, type, name)  \
     {                                    \
        struct list_head *it, *tmp;       \
        type *name;                \
    list_for_each_safe(it, tmp, list) {

#define Endforeach() }}

/* Nodos de la lista */
typedef struct {
    int data;
    struct list_head links;
}list_item_t;

typedef struct {
    spinlock_t mr_lock;
    struct list_head headEntry;   
    char entry_name[MAX_MOD_NAME];
    struct list_head links;
}list_entries_t;

struct list_head control_list; /* list to delete entries and their lists */

static struct proc_dir_entry *proc_entry_ctr;
struct proc_dir_entry *proc_parent = NULL;   

/* Write list */
static ssize_t list_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {

    char kbuf[len]; int num;
    // remove + 10 dig -> 16
    if(len >= 17)
	return -ENOMEM;    
    /* Transfer data from user to kernel space */
    if (copy_from_user(&kbuf, buf, len)>0)         
       return -EFAULT;
       
    kbuf[len]='\0';
	
	list_entries_t* list_entry =( list_entries_t*) PDE_DATA(filp->f_inode);
	
    if(sscanf(kbuf, "add %i ", &num) == 1) {  // add command
        trace_printk("Executing command 'add %i'\n", num);
        
        list_item_t *new_node;
        new_node = vmalloc(sizeof(list_item_t));
        new_node->data = num;
		/*SPIN_LOCK*/	
		spin_lock(&(list_entry->mr_lock)); 
		list_add_tail(&(new_node->links), &(list_entry->headEntry));
		spin_unlock(&(list_entry->mr_lock));
		/*SPIN_UNLOCK*/
    
    }else if(sscanf(kbuf, "remove %i ", &num) == 1){  // remove command, 
        trace_printk("Executing command 'remove %i'\n", num);
		/*SPIN_LOCK*/
		spin_lock(&(list_entry->mr_lock));
		Foreach(pos, aux, &(list_entry->headEntry),list_item_t, elem) 
		elem = list_entry(pos, list_item_t, links);
			  
		trace_printk("At elem '%i' (@%p)\n", elem->data, elem);

		if(elem->data == num){
			trace_printk("Found elem '%i'. Removing...\n", elem->data);
			list_del(pos);
			vfree(elem);
			}
		Endforeach()
		spin_unlock(&(list_entry->mr_lock));
		/*SPIN_UNLOCK*/
		
    }else if(strcmp(kbuf, "cleanup\n") == 0){
        trace_printk("Executing command 'cleanup'\n");
		/*SPIN_LOCK*/
		spin_lock(&(list_entry->mr_lock));
		Foreach(pos, aux, &(list_entry->headEntry), list_item_t, elem)
			elem = list_entry(pos, list_item_t, links);
			trace_printk("At elem '%i' (@%p)\n", elem->data, elem);
			list_del(pos);
			vfree(elem);
        Endforeach()
        spin_unlock(&(list_entry->mr_lock));
        /*SPIN_UNLOCK*/
    }else
        printk(KERN_INFO "Unknown command");

    (*off)+=len;

    return len;
}

/* Read list */
static ssize_t list_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    char kbuff[1024] = {};
    char* buffpos = &kbuff[0]; int max_elems= 256, count= 0;

    if((*off) > 0)
        return 0;
	
	list_entries_t* list_entry =( list_entries_t*) PDE_DATA(filp->f_inode);
	
	/*SPIN_LOCK*/
	spin_lock(&(list_entry->mr_lock));   
    Foreach(pos, aux, &(list_entry->headEntry), list_item_t, elem)
        elem = list_entry(pos, list_item_t, links);
        trace_printk("At elem '%i' (@%p)\n", elem->data, elem);
        
        if(count < max_elems){	
			buffpos += sprintf(buffpos, "%i\n", elem->data);
		}else{
			printk("max nº of items in buffer reached: %i cout: %i \n",max_elems,count);
		}
        count++;
    Endforeach()
    spin_unlock(&(list_entry->mr_lock));
    /*SPIN_UNLOCK*/
    if(buffpos > &kbuff[0] + 1)
        buffpos--; // Go back one byte to overwrite last colon
 
    size_t written = min(len, buffpos - &kbuff[0]);

    kbuff[written] = '\0';
	printk("kbuff size %i, len: %i \n", strlen(kbuff), len);
    trace_printk("Output: '%s'", kbuff);

    if(copy_to_user(buf, kbuff, written))
        return -EINVAL;
    
    *off += written;
    return written;
}

/* /proc op  */
static const struct file_operations proc_entry_fops_default = {
    .read = list_read,
    .write = list_write,  
};

/* creates and inits entries */
void create_entry(const char* entrys_name){
	
	struct proc_dir_entry *proc_entry;
	list_entries_t *new_node;  // nueva struct guarda datos de la entrada
	// init list_entries_t struct   
    new_node = vmalloc(sizeof(list_entries_t));
	proc_entry = proc_create_data(entrys_name, 0666, proc_parent, &proc_entry_fops_default, new_node);
	if(!proc_entry)
		 printk(KERN_INFO "Couldn't create %s entry \n",entrys_name);	
		
	INIT_LIST_HEAD(&(new_node->headEntry));
	spin_lock_init(&(new_node->mr_lock));  // init the spin lock in the struct
	strcpy(new_node->entry_name, entrys_name); 
	
	printk(KERN_INFO "created entry : %s \n",new_node->entry_name);	
	list_add_tail(&(new_node->links), &control_list); // add new struct to the control list	 
}

/* Write control */
static ssize_t control_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
	char kbuf[len], entryName[MAX_MOD_NAME];
    // remove + 10 dig -> 16
    if(len >= 17)
		return -ENOMEM;    
    /* Transfer data from user to kernel space */
    if (copy_from_user(&kbuf, buf, len)>0)         
       return -EFAULT;
       
    kbuf[len]='\0';
	
    if(sscanf(kbuf, "create %s ", entryName) == 1) {  // create entry
		create_entry(entryName); 
    
    }else if(sscanf(kbuf, "delete %s ", entryName) == 1){  // delete entry		
		
		// semaphores
		remove_proc_entry(entryName, proc_parent);
		// entry released
		Foreach(pos, aux, &control_list, list_entries_t, auxCtr)
			// iterating list_entries
			auxCtr = list_entry(pos, list_entries_t, links);	  
			if(!strcmp(auxCtr->entry_name, entryName)){
				printk(KERN_INFO "releasing entry %s \n", auxCtr->entry_name);
				// iterating list_item_t	
				Foreach(pos2, aux2, &(auxCtr->headEntry), list_item_t, elem)
					elem = list_entry(pos2, list_item_t, links);
					list_del(pos2);
					vfree(elem);	
				Endforeach()	
				list_del(pos);
				vfree(auxCtr);
			}
		Endforeach()			
		
    }else
        printk(KERN_INFO "Unknown command");

    (*off)+=len;
    return len;
}

/* /proc op  */
static const struct file_operations proc_entry_fops_control = {   
    .write = control_write, // create/remove entries   
};

/* Init module */
int init_list_module( void )
{
    // create entry (dir) within /proc
    proc_parent = proc_mkdir("multilist",NULL);             
    if(!proc_parent){
        return -ENOMEM;
        printk(KERN_INFO "multilist: Can't create /proc entry\n");
    }
    // create control entry within multilist
    proc_entry_ctr = proc_create("control", 0666, proc_parent, &proc_entry_fops_control); 
    if(!proc_entry_ctr){
        return -ENOMEM;
        printk(KERN_INFO "multilist: Can't create /proc entry\n");
    }   
	/* init cotrol's list head and create and init default entry */
	INIT_LIST_HEAD(&control_list);  
	printk(KERN_INFO "multilist: Module loaded\n");
	printk(KERN_INFO "created entry : control \n");
	create_entry("default");
	
    return 0;
}

/* Remove module */
void exit_list_module( void )
{		
	Foreach(pos, aux, &control_list, list_entries_t, auxCtr)
		// iterating list_entries
		auxCtr = list_entry(pos, list_entries_t, links);	  
		// first remove entry, its structures afterward
		remove_proc_entry(auxCtr->entry_name, proc_parent);
		
		printk(KERN_INFO "releasing entry %s \n", auxCtr->entry_name);
		// iterating list_item_t	
		Foreach(pos2, aux2, &(auxCtr->headEntry), list_item_t, elem)
			elem = list_entry(pos2, list_item_t, links);
			list_del(pos2);
			vfree(elem);	
		Endforeach()	
		list_del(pos);
		vfree(auxCtr);
	Endforeach() 

	remove_proc_entry("control", proc_parent);	
	remove_proc_entry("multilist", NULL);
	printk(KERN_INFO "multilist: Module unloaded.\n");
}

/* Declaracion de funciones init y cleanup */
module_init(init_list_module);
module_exit(exit_list_module);

/** Notas:
 *  
 * 	1- 	struct list_entries_t tine un campo 'entry_name' que registra el nombre de una entrada cuando es 
 * 		creada, así cuando descargamos la entrada con el comando 'delete' buscamos por nombre en la lista
 * 		y borramos, en este caso no se puede usar struct file *filp ya que la entrada a la que escribe es control
 * 		y no la que queremos borrar.
 * 
 * 	2-	void create_entry(): Es una fun auxiliar que sirve para crear una entrada, asociarla a una struct list_entries_t e
 * 							 inicializar la struct.
 *  
 *  3- 	He utilizado la macro de arriba para hacer el recorrido de la lista list_entries_t , el borrado de la lista list_item_t y posterior
 * 		borrado del nodo de list_entries_t, puede resultar un poco díficil de ver con la macro, por eso más abajo comentado esta el código 
 * 		sin macro.
 * 
 * 	4-	Los spin-locks privados cumplen la misma función que la parte A de la pr 3, varias entradas pueden escribir en una lista a la vez.
 * 		En la trans 6 deja claro que la fun. remove_proc_entry() es bloqueante, y en la nota que bloquea el flujo al kernel invocador, por lo que entiendo
 * 		que en caso de llamar la función bloquea la entrada en /proc hasta que es borrada, lo que no me quedó claro es si hay que utilizar semáforos para sincronizar la 
 * 		la llamada, ya que se podría estar borrando una entrada mientras otro proceso intenta escribir en ella. 
 * 		Lo que si he hecho al dar de baja una entrada es cambiar orden y primero borrar la entrada con remove_proc_entry() y después  borrar datos y liberar memoria.
 * 
 * char aux_name[MAX_MOD_NAME];	
	Foreach(pos, aux, &control_list, list_entries_t, auxCtr)
		// iterating list_entries
		auxCtr = list_entry(pos, list_entries_t, links);	  
		printk(KERN_INFO "releasing module %s \n", auxCtr->entry_name);
		// iterating list_item_t	
		Foreach(pos2, aux2, &(auxCtr->headEntry), list_item_t, elem)
			elem = list_entry(pos2, list_item_t, links);
			list_del(pos2);
			vfree(elem);	
		Endforeach()	
		strcpy(aux_name, auxCtr->entry_name);
		
		list_del(pos);
		vfree(auxCtr);
		// semaphores ?...
		remove_proc_entry(aux_name, proc_parent);
	Endforeach()
 * */

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
