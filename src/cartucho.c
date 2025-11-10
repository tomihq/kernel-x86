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

extern sched_entry_t sched_tasks[MAX_TASKS];


void deviceready(void){
    for(int i = 0; i<MAX_TASKS; i++){
        sched_entry_t* task = &sched_tasks[i];
        if(task -> accessModeToVideoBuffer == TASK_NO_VIDEO_BUFFER_ACCESS){
            continue; 
        }

        if(task -> state == TASK_BLOCKED){  
            int16_t task_selector = task -> selector;
            if(task -> accessModeToVideoBuffer == TASK_DMA_VIDEO_BUFFER_ACCESS){ 
                buffer_dma(CR3_TO_PAGE_DIR(task_selector_to_CR3(task_selector)));
            }else{//va por copia
                paddr_t phys = mmu_next_free_user_page(); //se utiliza página porque necesitamos una copia por cada tarea.
                vaddr_t virt = task -> vaddrToVideoBuffer;
                buffer_copy(CR3_TO_PAGE_DIR(task_selector_to_CR3(task_selector)), phys, virt);
            }

            task -> state = TASK_RUNNABLE;
        }else { 
            if(task->accessModeToVideoBuffer == TASK_COPY_VIDEO_BUFFER_ACCESS){
            paddr_t destino = virt_to_phy(task_selector_to_CR3(task_selector), tarea->copyDir);
            copy_page((paddr_t)0xF151C000, destino);
            }
        }

    }

}