/*pseudocódigo para el apartado B*/

/*Corregido*/
/* nr_gaps_cbuffer_t(cbuffer) < size_cbuffer_t(cbuffer) ->  read: numero huecos es igual al tamaño.
 * size_cbuffer_t -> Returns the number of elements in the buffer
 * Solución que me dio pero no le encuentro la lógica, creo que se equivoco
 * */
int fifo_proc_read(char* buff, int len) {
	
	char kbuffer[MAX_KBUF];
	
	if (len> MAX_CBUFFER_LEN || len> MAX_KBUF) { return Error;}
	
	lock(mtx);

	/* Esperar hasta que haya sufiente espacio ocupado para leer (Siempre debe haber productores) */
	/*nº elementos en buffer en menor al nº elem que queremos leer*/
	while (size_cbuffer_t(cbuffer) < len && prod_count > 0){
		cond_wait(cons,mtx);
	}
	
	/* Detectar fin de comunicación por error (consumidor cierra FIFO antes) */
	if (prod_count==0 && is_empty_cbuffer_t(cbuffer) != 0) {unlock(mtx); return 0;}
	
	remove_items_cbuffer_t(cbuffer,kbuf,len);
	
	// avisamos al prod.
	cond_signal(prod);
	
	unlock(mtx);
	
	// returns to user space 
	if(copy_to_user(kbuf,buff,len)){return Error;}
	
	return len;
}
// is_empty_cbuffer_t() == 0 read: el prod habrá cerrado y cerramos nosotros también.


// nr_gaps_cbuffer_t(cbuffer)<len -> write: menos libres de los que quiero escribir.
/* Dado */
int fifoproc_write(char* buff, int len) {
	
	char kbuffer[MAX_KBUF];
	
	if (len> MAX_CBUFFER_LEN || len> MAX_KBUF) { return Error;}
	
	if (copy_from_user(kbuffer,buff,len)) { return Error;}
	
	lock(mtx);
	
	/* Esperar hasta que haya hueco para insertar (debe haber consumidores) */
	while (nr_gaps_cbuffer_t(cbuffer)<len && cons_count>0){
		cond_wait(prod,mtx);
	}
	
	/* Detectar fin de comunicación por error (consumidor cierra FIFO antes) */
	if (cons_count==0) {unlock(mtx); return -EPIPE;}
	
	insert_items_cbuffer_t(cbuffer,kbuffer,len);
	
	/* Despertar a posible consumidor bloqueado */
	cond_signal(cons);
	
	unlock(mtx);
	
	return len;
}

/* Sin Corregir, sin acabar ? */
static int fifoproc_open(struct inode *inode, struct file *file){
	
	// esperar a los dos que estén inicializados. 
	if(file->f_mode & FMODE_READ){
		
		/* Un consumidor abrió el FIFO */
		lock(mtx);
		
		/* nº procesos que abrieron entrada lectura*/
		if(cons_count <1)
			cons_count +=1;
		
		/*NO haya productor, nos bloqueamos*/
		while (prod_count < 1){
			cond_wait(cons,mtx);
		}	
		
		unlock();
	}else{
		/* Un productor abrió el FIFO */
		lock(mtx);
		
		/* nº procesos que abrieron entrada escritura*/
		if(prod_count<1)
			prod_count +=1;
		
		/*NO haya consumidor, nos bloqueamos*/
		while (cons_count < 1){
			cond_wait(prod,mtx);
		}	
		unlock(mtx);
	}
	
	return 0;
}

/* Sin Hacer */
void fifoproc_release(struct inode *inode, struct file *file){
	/* Completar */
	// hacer al final dependen de las variables condición 
	//vaciar el buffer circular cuando los dos esten cerrados


}
