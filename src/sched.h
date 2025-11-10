/* ** por compatibilidad se omiten tildes **
================================================================================
 TALLER System Programming - ORGANIZACION DE COMPUTADOR II - FCEN
================================================================================

  Declaracion de funciones del scheduler.
*/

#ifndef __SCHED_H__
#define __SCHED_H__

#include "types.h"
#include "task_defines.h"

/**
 * Estados posibles de una tarea en el scheduler:
 * - `TASK_SLOT_FREE`: No existe esa tarea
 * - `TASK_RUNNABLE`: La tarea se puede ejecutar
 * - `TASK_PAUSED`: La tarea se registró al scheduler pero está pausada
 * - TASK_BLOCKED: La tarea solicitó acceso al buffer con opendevice. Está esperando que se ejecute el deviceready.
 */
typedef enum {
  TASK_SLOT_FREE,
  TASK_RUNNABLE,
  TASK_PAUSED,
  TASK_BLOCKED
} task_state_t;

typedef enum {
  TASK_NO_VIDEO_BUFFER_ACCESS,
  TASK_DMA_VIDEO_BUFFER_ACCESS,
  TASK_COPY_VIDEO_BUFFER_ACCESS
} task_buffer_video_access_t;

/**
 * Estructura usada por el scheduler para guardar la información pertinente de
 * cada tarea.
 */
typedef struct {
  int16_t selector;
  task_state_t state;

  uint8_t accessModeToVideoBuffer; //0: no accede al buffer. 1: accede por DMA. 2: accede por copia (proporciona su propia virtual por ecx)
  vaddr_t vaddrToVideoBuffer; //dirección virtual que le permite a la tarea ingresar al buffer de vídeo.
} sched_entry_t;

extern int8_t current_task;

int8_t sched_add_task(uint16_t selector);
void sched_disable_task(int8_t task_id);
void sched_enable_task(int8_t task_id);

void sched_init();

uint16_t sched_next_task();
extern sched_entry_t sched_tasks[MAX_TASKS];

#endif //  __SCHED_H__
