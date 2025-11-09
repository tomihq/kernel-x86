/* 3. Las tareas necesitan acceder al buffer de vídeo *(1).* Hay dos formas
    1. DMA: se mappea la dirección virtual `0xBABAB000` directamente al buffer de vídeo.
        1. Esto quiere decir que necesitamos que el directorio de esa tarea, mappeada en esa dirección virtual vaya al buffer de vídeo.
        2. Acá podemos usar `mmu_map_page`
    2. Por copia: No se hace ninguna referencia al buffer de vídeo original físicamente, sino que se hace una copia de esa página, y recién ahí, vinculamos la virtual con esa física. Cada tarea debe tener una copia única.
        1. Acá podemos usar `copy_page` y `mmu_map_page`.
        2. Como habla de que cada tarea tiene una copia única significa que cada *tarea* tendrá su región física correspondiente para almacenar la copia de la página del buffer de video.*/
#include "mmu.h"
#include "tasks.h"
#include "sched.h"

extern mmu_map_page

void deviceready(void){
    for(int i = 0; i<MAX_TASKS; i++){
        sched_entry_t* task = &sched_tasks[i];
        if(task -> mode == TASK_NO_VIDEO_BUFFER_ACCESS){
            continue; //si la tarea no es de cartucho.
        }

        //Si la tarea está bloqueada significa que nunca le dimos el device ready
        if(task -> state == TASK_BLOCKED){  
            if(task -> mode == TASK_DMA_VIDEO_BUFFER_ACCESS){ //quiere acceso directo.
                //necesito mappear la dirección virtual 0xBABAB000 a la dirección 0xF151C000
                //necesito antes encontrar el directorio de páginas de la tarea. voy a usar mmu_map_page void mmu_map_page(uint32_t cr3, vaddr_t virt, paddr_t phy, uint32_t attrs)
                //los permisos deberían ser presente, r/w para usuario.
                //No obstante, de todo esto se encarga: void buffer_dma(pd_entry_t* pd). Que le necesito mandar el puntero al CR3 de la tarea.
                int16_t task_selector = task -> selector;
                buffer_dma(CR3_TO_PAGE_DIR(task_selector_to_CR3(task_selector)));
                

            }

        }else { //Si la tarea está en cualquier otro estado y la tarea es de cartucho actualizo todo. 

        }

    }

}