#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm-generic/uaccess.h>
#include <asm-generic/errno.h>
#include <linux/semaphore.h>
#include "cbuffer.h"

#define MAX_CBUFFER_LEN 50
#define MAX_KBUF  50

cbuffer_t* cbuffer; /* Buffer circular */

int prod_count = 0; /* Número de procesos que abrieron la entrada proc para escritura (productores) */						
int cons_count = 0; /* Número de procesos que abrieron la entrada proc para lectura (consumidores) */
							
struct semaphore mtx; /* para garantizar Exclusión Mutua */

struct semaphore queue_sem_prod; /* cola de espera para productor(es) */
struct semaphore queue_sem_cons; /* cola de espera para consumidor(es) */

int nr_prod_waiting=0; /* Número de procesos productores esperando */
int nr_cons_waiting=0; /* Número de procesos consumidores esperando */

/* Se invoca al hacer open() de entrada /proc */
static int fifoproc_open(struct inode *inode, struct file *file){

}

/* Se invoca al hacer close() de entrada /proc */
static int fifoproc_release(struct inode *inode, struct file *file){

}

/*NO confundir el buff que nos pasan con el cbuffer que leemos*/
/*done*/
static ssize_t fifoproc_read(struct file *filp, char __user *buff, size_t len, loff_t *off){
	char kbuffer[MAX_KBUF];
		
	if((*off) > 0)
      return 0;
	
	if (len> MAX_CBUFFER_LEN || len> MAX_KBUF) { return -ENOSPC;}
	
	//lock(mtx);
	if (down_interruptible(&mtx)){return -EINTR;}
		
	/* Esperar hasta que haya sufiente espacio ocupado para leer (Siempre debe haber productores) */
	/*nº elementos en buffer en menor al nº elem que queremos leer*/
	while (size_cbuffer_t(cbuffer) < len && prod_count > 0){
		//cond_wait(cons,mtx); queue_sem_cons -> const
		nr_cons_waiting++;
		up(&mtx);
		if(down_interruptible(&queue_sem_cons)){
			down(&mtx);
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
	remove_items_cbuffer_t(cbuffer,kbuf,len);
	
	/*Despertar productor bloqueado*/
	//cond_signal(prod); 
	if(nr_prod_waiting >0){  
		up(&queue_sem_prod);
		nr_prod_waiting--;
		}
	//unlock(mtx);
	up(&mtx);
	
	// returns to user space 
	if(copy_to_user(buff,kbuf,len)){return -EFAULT;}
	
	*off += len;
	
	return len;
}

/*NO confundir el buff que nos pasan con el cbuffer al que escribimos*/
/*done*/
static ssize_t fifoproc_write(struct file *file, const char *buff, size_t len, loff_t *off){
	
	char kbuffer[MAX_KBUF];
	
	if(len> MAX_CBUFFER_LEN || len> MAX_KBUF) {return -ENOSPC;}
	
	if(copy_from_user(kbuffer,buff,len)) {return -EFAULT;}
	
	(*off)+= len;
	
	//lock(mtx);
	if (down_interruptible(&mtx)){return -EINTR};
		
	/* Esperar hasta que haya hueco para insertar (debe haber consumidores) */
	while (nr_gaps_cbuffer_t(cbuffer)<len && cons_count>0){
		//cond_wait(prod,mtx);  // queue_sem_prod -> prod
		nr_prod_waiting++;
		up(&mtx);
		/*Bloqueo en sem de cola*/
		if(down_interruptible(&queue_sem_prod)){
			down(&mtx);
			nr_prod_waiting--;
			up(&mtx);
			return -EINTR;
		}
		/*Adquiere el mutex*/
		if(down_interruptible(&mtx)){return -EINTR};	
	}  // while
	
	/* Detectar fin de comunicación por error (consumidor cierra FIFO antes) */
	if(cons_count==0){up(&mtx); return -EPIPE;}
	
	/*len bytes from kbuffer to cbuffer*/
	insert_items_cbuffer_t(cbuffer,kbuffer,len);
	
	/* Despertar consumidor bloqueado */
	//cond_signal(cons);
	if(nr_cons_waiting > 0){  // si hay consumidores en cola ...
		up(&queue_sem_cons);
		nr_cons_waiting--;
	}
	//unlock(mtx);
	up(&mtx);
	
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
	init_Mutex(&mtx);
	sema_init(&queue_sem_prod,0);
	sema_init(&queue_sem_cons,0);
	
	if(!cbuffer)
		return -ENOMEM;
	
	proc_entry = proc_create("prodcons",0666, NULL, &proc_entry_fops);
	
	if (proc_entry == NULL) {
		destroy_cbuffer_t(cbuf);
		printk(KERN_INFO "Modulo prodcons no pudo cargarse .\n");
		return -ENOMEM;
	}
	printk(KERN_INFO "Modulo cargado: prodcons.\n");
	
	return 0;
}

/*done*/
void exit_fifoproc_module( void ){
	remove_proc_entry("prodcons", NULL);
	destroy_cbuffer_t(cbuf);
	printk(KERN_INFO "Modulo descargado: prodcons.\n");
}

/* Funciones de inicialización y descarga del módulo */
int init_module(init_fifoproc_module);
void cleanup_module(exit_fifoproc_module);
