#ifndef __BUFFER_H__
#define __BUFFER_H__

void buffer_dma(pd_entry_t* pd); //dado page directory realiza mappeo del buffer en modo DMA
void buffer_copy(pd_entry_t* pd, paddr_t phys, vaddr_t virt);  //dado page directory de una tarea realiza la copia del buffer a la direccion física pasada por parámetro y realiza el mapeo a la dirección virtual pasado por parámetro (copy_page + mmu_map_page)
#endif //  __BUFFER_H__
