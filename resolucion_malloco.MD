# Resolución
Necesitamos varias cosas para resolver el ejercicio:
    a. Una "unidad" de manejo de memoria para cada tarea que se pueda reservar a través de Malloco. Las direcciones virtuales se mapearán desde `0xA10C0000` y las páginas físicas a utilizar se sacarán del área libre de tareas.
    Malloco tendrá que chequear que cada tarea no se haya excedido de los 4MB que puede solicitar. Si es así, retorna NULL. Si tenemos espacio, entonces tendríamos que indicar que acabamos de crear una nueva reserva (agregarle 1 al tamaño del array) y agregar el contenido al array. Devuelve el puntero al inicio de la página. 
    b. Dos SYSCALLS: una para `malloco` y otra para `chau`. Ambas SYSCALLS deberán tener su handler correspondiente que definiremos en la IDT que recibirán por la pila el size y la dircción virtual respectivamente. Para hacer el proceso haremos un salto desde ASM a C pasando los parámetros por la pila.
    c. Tendremos que modificar el page_fault_handler por varios motivos:
        - Si la tarea tiene en CR2 una dirección virtual que está en el rango de Malloco pero nunca la reservó entonces desalojamos la tarea. Para esto podemos usar algún estado como TASK_KILLED para que el Scheduler no la tome más en cuenta. Luego, en el mismo momento, ponemos todas todas sus reservas con estado = 2 (a liberar) para que el GARBAGE_COLLECTOR haga su trabajo **luego**. Llamamos al scheduler para que deshabilite esta tarea y llame a la otra. 
        - Si la tarea tiene en CR2 una dirección virtual que está en el rango de Malloco y la reservó entonces tomamos una nueva página de usuario del área de tareas, la dejamos en zero y la mapeamos con la virtual. Notar que el enunciado dice que tenemos que ir reservando gradualmente. Esto podemos hacerlo usando esMemoriaReservada(virt), asumiendo que la dirección virtual que nos mandan es el inicio de una página *(lo cual tendría sentido porque nosotros damos un puntero al inicio de la página, lo cual para reservarla físicamente tendría sentido que nos manden el inicio)* 
        - Si no pasa ninguno de estos casos queda el resto de on_demand *(que acá no aplica)*  
    d. Garbage Collector será una tarea aparte de nivel 0, la cual su directorio de páginas estará en región de memoria Kernel. Además, tendra su stack de nivel 0 únicamente. Necesitaremos una función que haga una especie de **mmu_create_task_kernel_init** pues no necesitamos ni shared ni tampoco crear el stack de nivel 3. Deberemos utilizar identity-mapping pues su código estará en región de memoria Kernel.
    Esta deberá ser llamada por una interrupción que maneje los 100 ticks o directamente que el propio scheduler haga ese control basado en el paso del tiempo.
    El trabajo del Garbage Collector será básicamente recorrer todas las reservas de esa tarea que las podemos pedir con dameReservas(int task_id) y chequear todas las reservas con estado = 2.
    Al estar en el Garbage Collector ni el CR3 ni el TR nos sirven pues esta es una tarea global, lo cual podríamos iterar en todo el array de reservas para cada tarea. A partir del task_id de cada tarea, ir a la GDT reconstruir la dirección de la TSS y obtener el CR3. 
    Luego, por cada dirección virtual que corresponda hacemos el unmap. Notar que acá como cada reserva puede ocupar más de una página, tenemos que contar cada página que cae en esa reserva.
    e. Chau tendremos que usar esMemoriaReservada(virt), esta nos dirá si está reservada mediante malloco. Si la respuesta es sí, entonces buscamos esa reserva de malloco y le clavamos el estado = 2. El resto se encargará el Garbage Collector. 

Ahora sí, veamos qué cambios necesitaríamos asumiendo que todo está implementado.
1.A. Definimos las SYSCALLS en la IDT.
```
isr.h 
    void _isr90();
    void _isr91();
idt.c
    IDT_ENTRY3(90); //malloco
    IDT_ENTRY3(91); //chau
```
1.B. Creamos las rutinas de atención que correspondan
```
global _isr90
_isr90:
  pushad ;preservo 
  push eax ;pusheo como parámetro la dirección virt que me mandan
  call malloco
  add esp, 4 ;vuelvo a alinear la pila
  mov [esp + offset_EAX], eax ;almaceno en eax el resultado de malloco así lo devuelvo
  popad
  iret  


global _isr91
_isr91:
    pushad ;preservo
    push eax ;preparo el parámetro
    call chau
    add esp, 4 ;vuelvo a alinear la pila
    popad
    iret

Como ninguna interrupción viene del pic no hace falta hacer picfinish.
```
1.C. Extendemos la rutina del reloj para que ahora luego de incrementar el tick de las tareas chequee si es el turno del garbage collector
```
extern check_garbage_collector
global _isr32
_isr32:
    pushad
    call pic_finish1
    call sched_next_task

    str cx
    cmp ax, cx
    je .fin

    jmp far [sched_task_offset]

    .fin:
    ; Se restauran los registros de proposito general desde el stack.
    call tasks_tick
    call check_garbage_collector ¡aca!
    call tasks_screen_update
    popad
    iret
```
donde el código de check_garbage_collector está en sched.c
```
void check_garbage_collector(void){
  if (ENVIRONMENT->tick_count % 100 == 0) {
        sched_tasks[GARBAGE_COLLECTOR_TASK_ID].state = TASK_RUNNABLE;
    }
}
```

Con esto deberíamos terminar suponiendo que el código de garbage_collector, malloc y chau están implementados.

Cuando se quiera acceder a una región de memoria que todavía no haya sido mapeada a una página física va a caer en el page_fault_handler. La dirección que falló me cae en CR2 así que no tengo que cambiar nada de la rutina de atención pero sí del código de C. 

```
mmu.c
    bool page_fault_handler(vaddr_t virt){
        ... prev
          /* lo comento porque no tengo esMemoriaReservada ni dameReservas
    if(virt >= MALLOCO_MEM_START_VIRTUAL  && virt <= MALLOCO_MEM_END_VIRTUAL){
      if(esMemoriaReservada(virt) == 1){
          paddr_r phy = next_free_user_page();
          zero_page(phy);
          mmu_map_page(virt, phy);
          reservas_por_tarea* reservas = dameReservas(ENVIRONMENT -> task_id);
          for(int i = 0; i< reservas -> reservas_size; i++){
            reserva_t reserva = reservas -> array_reservas[i];
            if(reserva -> virt == virt){
              reserva -> estado = 1; 
            }
          }
      }else{
           reservas_por_tarea* reservas = dameReservas(ENVIRONMENT -> task_id);
           for(int i = 0; i<reservas -> reservas_size; i++){
              reserva_t reserva = reservas -> array_reservas[i];
              reserva -> estado = 2; 
           }
      }
    }
    */
        ...lo que sigue
    }
Notar que MALLOCO_MEM_START_VIRTUAL arranca en 0xA10C0000 y MALLOCO_MEM_END_VIRTUAL sería 0xA10C0000 + 4MB más adelante y que ENVIRONMENT -> task_id es la variable que usa el scheduler que te dice el id de la tarea actual

tasks.c
    typedef enum {
    TASK_SLOT_FREE,
    TASK_RUNNABLE,
    TASK_PAUSED,
    TASK_KILLED !acá
    } task_state_t;


```

2. Necesitamos indicar qué cambios necesitamos para llamar al garbage_collector y dar una posible implementación.

```
task_defines.h
    #define MAX_TASKS     5 // agrego una tarea mas para el garbage collector
tasks.c
typedef enum {
  TASK_A = 0,
  TASK_B = 1,
  TASK_C = 2
} tipo_e;

static paddr_t task_code_start[2] = {
    [TASK_A] = TASK_A_CODE_START,
    [TASK_B] = TASK_B_CODE_START,
    [TASK_C] = TASK_C_CODE_START //asumimos que es una dirección válida y existente de la implementación de garbage_collector
};

void tasks_init(void) {
  int8_t task_id;
  ... anterior
  task_id = create_kernel_task(TASK_C);
  sched_enable_task(task_id);
}

static int8_t create_kernel_task(tipo_e tipo) {
  size_t gdt_id;
  for (gdt_id = GDT_TSS_START; gdt_id < GDT_COUNT; gdt_id++) {
    if (gdt[gdt_id].p == 0) {
      break;
    }
  }
  kassert(gdt_id < GDT_COUNT, "No hay entradas disponibles en la GDT");

  int8_t task_id = sched_add_task(gdt_id << 3);
  tss_tasks[task_id] = tss_create_kernel_task(task_code_start[tipo]);
  gdt[gdt_id] = tss_gdt_entry_for_task(&tss_tasks[task_id]);
  return task_id;
}

tss.c

tss_t tss_create_kernel_task(paddr_t code_start) {
  uint32_t cr3 = mmu_init_task_kernel_dir(code_start);
  vaddr_t code_virt = code_start;
  vaddr_t stack_base = mmu_next_free_kernel_page();
  addr_t stack_top = stack_base + PAGE_SIZE;
  return (tss_t) {
       .cr3 = cr3,
        .eip = code_virt,
        .esp = stack_top,
        .ebp = stack_top,
        .cs = GDT_CODE_0_SEL,
        .ds = GDT_DATA_0_SEL,
        .es = GDT_DATA_0_SEL,
        .fs = GDT_DATA_0_SEL,
        .gs = GDT_DATA_0_SEL,
        .ss = GDT_DATA_0_SEL,
        .ss0 = GDT_DATA_0_SEL,
        .esp0 = stack_top,
        .eflags = EFLAGS_IF
  };
}

Consideraciones:
1. La tarea al ser de Kernel usamos segmentos de Kernel.
2. La pila de nivel 0 es única, y le dejamos una página.
3. El code_virt = code_start porque el código del Garbage Collector está en región de memoria Kernel y funciona con identity mapping.

mmu.c

paddr_t mmu_init_task_kernel_dir(paddr_t phy_start) {
  paddr_t pd_phys = mmu_next_free_kernel_page(); 
  pd_entry_t* pd = (pd_entry_t*) pd_phys;
  zero_page(pd_phys);
  uint32_t cr3 = (uint32_t)pd_phys; 

  for (size_t i = 0; i <= identity_mapping_end / PAGE_SIZE; i++) {
      mmu_map_page(cr3, i * PAGE_SIZE, i * PAGE_SIZE, MMU_P | MMU_W);
  }

  //hacemos el identity mapping del código del garbage collector
  mmu_map_page(cr3, phy_start, phy_start, MMU_P | MMU_W);
  mmu_map_page(cr3, phy_start + PAGE_SIZE, phy_start + PAGE_SIZE, MMU_P | MMU_W);


  return cr3;

}

Notar que eliminamos lo shared y el mapeo de páginas de código virtual que pueda acceder un usuario. Esto es porque ahora nuestro código vive en región de memoria Kernel y es accedido por el Kernel a través de identity mapping.


```

El código de garbage_collector no blanqueará en absoluto el ticks_count porque es la rutina del reloj que le dice: che es tu turno y hasta que no termina de hacer lo que hace, no sigue otra tarea.
Notar que acá no tengo absolutamente nada de la información de las tareas porque estoy en otra tarea. Tengo que jugar con el MAX_TASKS y el array que tengo en memoria de reservas_por_tarea que asumo que está en algún lugar de la memoria. 

```
mmu.c
La vamos a necesitar para obtener el directorio de páginas de la tarea.

uint32_t get_cr3_by_selector(uint16_t selector){
  uint32_t index = selector >> 3;
  gdt_entry_t entry_gdt = gdt[index];
  uint32_t base = 
    (entry_gdt.base_0_15) |
    (entry_gdt.base_16_23 << 16) |
    (entry_gdt.base_24_31 << 24);
  return tss -> cr3;
}

sched.c
void garbage_collector(void){
        while(true){
            for(int i = 0; i<MAX_TASKS; i++){

             int16_t sched_task_selector = sched_tasks[i] -> selector;
             uint32_t cr3 = get_cr3_by_selector(sched_task_selector);
             int16_t task_id = sched_task_selector >> 3;
             //necesito desmapear todas las tareas del cr3 y despues en el array ponerlas en estado 3 si estan en estado 2
             reservas_por_tarea* reservas = dameReservas(task_id);
                
                for(int j = 0; j<reservas -> reservas_size; j++){
                  reserva_t reserva = reservas -> array_reservas[i];
                  if(reserva -> estado != 2){
                    continue;
                  }

                  //eliminamos TODAS las páginas físicas mappeadas desde la virtual en adelante.
                  for(int h = 0; h < reserva->tamanio / PAGE_SIZE; h++){
                        mmu_unmap_page(cr3, virt);
                        virt += PAGE_SIZE * j; 
                  }

                  reserva -> estado = 3; 
                }

            }
        }
}
```

3. a. La estructura que lleva el registro de las reservas podría estar tranquilamente en espacio de memoria Kernel porque es el único que debería tener acceso junto al Garbage Collector de privilegio 0 que tiene identity-mapping con esa región. 
b. Una implementación de Malloco podría ser
```
defines.h
#define MB_4 4194304

mmu.c
void* malloco(size_t size){
  reservas_por_tarea* reservas = sched_tasks[ENVIRONMENT -> task_id];
  uint32_t tamanio_maximo_reservado = 0; 
  uint32_t maxReservada = 0; 
  for(int i = 0; i < reservas -> reservas_size; i++){
    tamanio_maximo_reservado += reservas[i] -> tamanio;
    //tengo la maxima direccion virtual ocupada (base + lo que ocupó en bytes)
    maxReservada = reservas[i] -> virt + reservas[i] -> tamanio;
  } 
  if(tamanio_maximo_reservado > MB_4) return null;
  reserva_t reserva_nueva = {
    .virt = maxReservada,
    .tamanio = size, 
    .estado = 0
  }
  //se asume que array_reservas tiene espacio para poner algo nuevo. Es decir, de antemano se prepara ese espacio para que pueda ser escrito. 
  reservas -> array_reservas[reservas_size] = reserva_nueva;
  reservas -> reservas_size += 1; 

  return (void*) maxReservada; 
}
```

c. Una implementación de chau podría ser
