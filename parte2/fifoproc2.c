#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm-generic/uaccess.h>
#include <asm-generic/errno.h>
#include <linux/semaphore.h>
#include <linux/ftrace.h>
#include "cbuffer.h"

#define MAX_CBUFFER_LEN 32
#define MAX_KBUF 60 

cbuffer_t* cbuffer; /* Buffer circular */
static struct proc_dir_entry *proc_entry;

int prod_count = 0; /* Número de procesos que abrieron la entrada proc para escritura (productores) */						
int cons_count = 0; /* Número de procesos que abrieron la entrada proc para lectura (consumidores) */
							
struct semaphore mtx; /* para garantizar Exclusión Mutua */

struct semaphore queue_sem_prod; /* cola de espera para productor(es) */
struct semaphore queue_sem_cons; /* cola de espera para consumidor(es) */

int nr_prod_waiting=0; /* Número de procesos productores esperando */
int nr_cons_waiting=0; /* Número de procesos consumidores esperando */

/* Se invoca al hacer open() de entrada /proc */
/*static int fifoproc_open(struct inode *inode, struct file *file){
    
    if(file->f_mode & FMODE_READ)
    {
        if (down_interruptible(&mtx)){return -EINTR;}

        cons_count++;
        while (prod_count == 0){
			         
            up(&mtx);
            if(down_interruptible(&queue_sem_cons)){
                down_interruptible(&mtx);
                cons_count--;
                up(&mtx);
                return -EINTR;
            }
            if(down_interruptible(&mtx))
                return -EINTR;
        }  // while
        //up(&queue_sem_prod);  // aqui o en while ?
        up(&mtx);
    }
    else
    {
        if(down_interruptible(&mtx)){return -EINTR;}
        
        prod_count++;  
        
        while (cons_count == 0){
           //up(&queue_sem_cons);
           up(&mtx);
            if(down_interruptible(&queue_sem_prod)){
                down_interruptible(&mtx);
                prod_count--;
                up(&mtx);
                return -EINTR;
            }
            if(down_interruptible(&mtx))
                return -EINTR;
        }  // while
        //up(&queue_sem_cons);
        up(&mtx);
    }
    return 0;
}*/

static int fifoproc_open(struct inode *inode, struct file *file){
	
	// esperar a los dos que estén inicializados. 
	if(file->f_mode & FMODE_READ){
		
		/* Un consumidor abrió el FIFO */
		//lock(mtx);
		if (down_interruptible(&mtx)){return -EINTR;}
		/* nº procesos que abrieron entrada lectura*/		
		cons_count += 1;
		
		/*NO haya productor, nos bloqueamos*/
		/*while (prod_count < 1){
			cond_wait(cons,mtx);
		}*/
		
        while (prod_count == 0){
        	nr_cons_waiting++;
        	up(&mtx);
            if(down_interruptible(&queue_sem_cons)){
                down_interruptible(&mtx);
                nr_cons_waiting--;
                up(&mtx);
                return -EINTR;
            }
            if(down_interruptible(&mtx))
                return -EINTR;
        }  // while
		
		//cons_signal(prod_count);
		if(nr_prod_waiting > 0 ) {
            up(&queue_sem_prod);
            nr_prod_waiting--;
        }

        up(&mtx);
        //trace_printk("Open READ %i, '\n", strlen(kbuf));
		//unlock(mtx);
	
	}else{
		/* Un productor abrió el FIFO */
		//lock(mtx);
		if (down_interruptible(&mtx)){return -EINTR;}

		/* nº procesos que abrieron entrada escritura*/
		prod_count +=1;
		
		/*NO haya consumidor, nos bloqueamos*/
		/*while (cons_count < 1){
			cond_wait(prod,mtx);
		}*/
		while (cons_count == 0){
        	nr_prod_waiting++;
        	up(&mtx);
            if(down_interruptible(&queue_sem_prod)){
                down_interruptible(&mtx);
                nr_prod_waiting--;
                up(&mtx);
                return -EINTR;
            }
            if(down_interruptible(&mtx))
                return -EINTR;
        }  // while

		//cons_signal(prod_count);
		if(nr_cons_waiting > 0 ) {
            up(&queue_sem_cons);
            nr_cons_waiting--;
        }

        up(&mtx);
		//unlock(mtx);
		//trace_printk("Open Write \n");
	}
	return 0;
}

/* Se invoca al hacer close() de entrada /proc */
static int fifoproc_release(struct inode *inode, struct file *file){
    if(file->f_mode & FMODE_READ)
    {
        if(down_interruptible(&mtx)){return -EINTR;}

        clear_cbuffer_t(cbuffer);
        cons_count--;
        up(&mtx);
    }
    else
    {
       if(down_interruptible(&mtx)){return -EINTR;}
	   
	   clear_cbuffer_t(cbuffer);
       prod_count--;
       up(&mtx);
    }

    
    return 0;
}

/*NO confundir el buff que nos pasan con el cbuffer que leemos*/
/*done*/
static ssize_t fifoproc_read(struct file *filp, char __user *buff, size_t len, loff_t *off){
	
	char kbuffer[MAX_KBUF] ={};
		
	if((*off) > 0)
      return 0; 
	
	//trace_printk("READ size len: %i  %i '\n", len,strlen(kbuffer));
	//if (len > MAX_CBUFFER_LEN || len > MAX_KBUF) { return -ENOSPC;}
	
	//lock(mtx);
	if (down_interruptible(&mtx)){return -EINTR;}
		
	while (size_cbuffer_t(cbuffer) < MAX_CBUFFER_LEN && prod_count > 0){
		//cond_wait(cons,mtx); queue_sem_cons -> const
		nr_cons_waiting++;
		up(&mtx);
		if(down_interruptible(&queue_sem_cons)){
			down_interruptible(&mtx);
			nr_cons_waiting--;
			up(&mtx);
			return -EINTR;
		}
		if(down_interruptible(&mtx))
			return -EINTR;
	}  // while
	
	/* Detectar fin de comunicación por error (consumidor cierra FIFO antes) */
	if (prod_count==0 && is_empty_cbuffer_t(cbuffer) != 0) {up(&mtx); return 0;}
	
	/*len chars de cbuffer a kbuf*/
	remove_items_cbuffer_t(cbuffer,kbuffer,MAX_CBUFFER_LEN);
	
	/*Despertar productor bloqueado*/
	//cond_signal(prod); 
	if(nr_prod_waiting >0){  
		up(&queue_sem_prod);
		nr_prod_waiting--;
		}
	//unlock(mtx);
	up(&mtx);
	
	// returns to user space 
	if(copy_to_user(buff,kbuffer,MAX_CBUFFER_LEN)){return -EFAULT;}
	
	
	(*off) += len;
	/*nº bytes leídos (>0 se podra volver a llamar, 0 eof)*/
	return len;
}

/*NO confundir el buff que nos pasan con el cbuffer al que escribimos*/
/*done*/
static ssize_t fifoproc_write(struct file *file, const char __user *buff, size_t len, loff_t *off){
	
	char kbuffer[MAX_KBUF]={};
	
	if(copy_from_user(kbuffer,(*buff).data ,(*buff).nr_bytes)) {return -EFAULT;}
	
	/*nº bytes mns*/
	unsigned int nr_bytes = (*buff).nr_bytes;
	
	if(nr_bytes> MAX_CBUFFER_LEN || nr_bytes> MAX_KBUF) {return -ENOSPC;}
	//trace_printk("WRITE size len: %i \n", len);
		
	//lock(mtx);
	if (down_interruptible(&mtx)){return -EINTR;}
		
	/* Esperar hasta que haya hueco para insertar (debe haber consumidores) */
	while (nr_gaps_cbuffer_t(cbuffer)< nr_bytes && cons_count > 0){
		//cond_wait(prod,mtx);  // queue_sem_prod -> prod
		nr_prod_waiting++;
		up(&mtx);
		/*Bloqueo en sem de cola*/
		if(down_interruptible(&queue_sem_prod)){
			down_interruptible(&mtx);
			nr_prod_waiting--;
			up(&mtx);
			return -EINTR;
		}
		/*Adquiere el mutex*/
		if(down_interruptible(&mtx)){return -EINTR;}	
	}  // while
	
	/* Detectar fin de comunicación por error (consumidor cierra FIFO antes) */
	if(cons_count==0){up(&mtx); return -EPIPE;}
	
	/*len bytes from kbuffer to cbuffer*/
	insert_items_cbuffer_t(cbuffer,kbuffer,nr_bytes);
	
	/* Despertar consumidor bloqueado */
	//cond_signal(cons);
	if(nr_cons_waiting > 0){  // si hay consumidores en cola ...
		up(&queue_sem_cons);
		nr_cons_waiting--;
	}
	//unlock(mtx);
	up(&mtx);
	
	(*off)+= len;
	
	/*nº bytes escritos*/
	return len;	  
}

/*done*/
static const struct file_operations proc_entry_fops = {
    .read = fifoproc_read,
    .write = fifoproc_write,    
    .open = fifoproc_open,
    .release = fifoproc_release,
};

/*done*/
int init_fifoproc_module( void ){
	
	/* initialize *cbuffer */
	cbuffer = create_cbuffer_t(MAX_CBUFFER_LEN);  
	
	/*initializing semaphores*/
	sema_init(&mtx, 1);
	sema_init(&queue_sem_prod,0);
	sema_init(&queue_sem_cons,0);
	
	if(!cbuffer)
		return -ENOMEM;
	
	proc_entry = proc_create("NUESTRO_PIPE",0666, NULL, &proc_entry_fops);
	
	if (proc_entry == NULL) {
		destroy_cbuffer_t(cbuffer);
		printk(KERN_INFO "Modulo prodcons no pudo cargarse .\n");
		return -ENOMEM;
	}
	printk(KERN_INFO "Modulo cargado: prodcons.\n");
	
	return 0;
}

/*done*/
void exit_fifoproc_module( void ){
	remove_proc_entry("NUESTRO_PIPE", NULL);
	destroy_cbuffer_t(cbuffer);
	printk(KERN_INFO "Modulo descargado: prodcons.\n");
}

/* Funciones de inicialización y descarga del módulo */
module_init(init_fifoproc_module);
module_exit(exit_fifoproc_module);
