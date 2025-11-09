/* ** por compatibilidad se omiten tildes **
================================================================================
 TRABAJO PRACTICO 3 - System Programming - ORGANIZACION DE COMPUTADOR II - FCEN
================================================================================

  Definicion de funciones del manejador de memoria
*/

#include "mmu.h"
#include "i386.h"

#include "kassert.h"

static pd_entry_t* kpd = (pd_entry_t*)KERNEL_PAGE_DIR;
static pt_entry_t* kpt = (pt_entry_t*)KERNEL_PAGE_TABLE_0;

static const uint32_t identity_mapping_end = 0x003FFFFF;
static const uint32_t user_memory_pool_end = 0x02FFFFFF;

static paddr_t next_free_kernel_page = 0x100000;
static paddr_t next_free_user_page = 0x400000;
static paddr_t next_free_on_demand_page = 0x3000000;

/**
 * kmemset asigna el valor c a un rango de memoria interpretado
 * como un rango de bytes de largo n que comienza en s
 * @param s es el puntero al comienzo del rango de memoria
 * @param c es el valor a asignar en cada byte de s[0..n-1]
 * @param n es el tamaño en bytes a asignar
 * @return devuelve el puntero al rango modificado (alias de s)
*/
static inline void* kmemset(void* s, int c, size_t n) {
  uint8_t* dst = (uint8_t*)s;
  for (size_t i = 0; i < n; i++) {
    dst[i] = c;
  }
  return dst;
}

/**
 * zero_page limpia el contenido de una página que comienza en addr
 * @param addr es la dirección del comienzo de la página a limpiar
*/
static inline void zero_page(paddr_t addr) {
  kmemset((void*)addr, 0x00, PAGE_SIZE);
}


void mmu_init(void) {}

/**
 * mmu_next_free_kernel_page devuelve la dirección física de la próxima página de kernel disponible. 
 * Las páginas se obtienen en forma incremental, siendo la primera: next_free_kernel_page
 * @return devuelve la dirección de memoria de comienzo de la próxima página libre de kernel
 */
paddr_t mmu_next_free_kernel_page(void) {
  paddr_t temp = next_free_kernel_page;
  next_free_kernel_page += PAGE_SIZE;
  return temp;
}

/**
 * mmu_next_free_user_page devuelve la dirección de la próxima página de usuarix disponible
 * @return devuelve la dirección de memoria de comienzo de la próxima página libre de usuarix
 */
paddr_t mmu_next_free_user_page(void) {
  paddr_t temp = next_free_user_page;
  next_free_user_page += PAGE_SIZE;
  return temp;
}

/**
* @deprecated
* mmu_next_free_on_demand_page devuelve la dirección de la próxima página de memoria bajo demanda disponible
* @return devuelve la dirección de memoria de comienzo de la próxima página libre bajo demanda
*/
paddr_t mmu_next_free_on_demand_page(void) {
  paddr_t temp = next_free_on_demand_page;
  next_free_on_demand_page += PAGE_SIZE;
  return temp;
}

/**
 * mmu_init_kernel_dir inicializa las estructuras de paginación vinculadas al kernel y
 * realiza el identity mapping
 * @return devuelve la dirección de memoria de la página donde se encuentra el directorio
 * de páginas usado por el kernel
 */
paddr_t mmu_init_kernel_dir(void) {
  zero_page((paddr_t)(uintptr_t) kpd);

  kpd[0] = (pd_entry_t) {
    .pt = ((uint32_t)kpt >> 12) & 0xFFFFF,
    .attrs = MMU_W | MMU_P
  };

  for (size_t i = 0; i < 1024; i++) {
    kpt[i] = (pt_entry_t){
      .page = i,
      .attrs = MMU_W | MMU_P
    };
  }

  return (paddr_t)(uintptr_t)kpd;
}


/**
 * mmu_map_page agrega las entradas necesarias a las estructuras de paginación de modo de que
 * la dirección virtual virt se traduzca en la dirección física phy con los atributos definidos en attrs
 * @param cr3 el contenido que se ha de cargar en un registro CR3 al realizar la traducción
 * @param virt la dirección virtual que se ha de traducir en phy
 * @param phy la dirección física que debe ser accedida (dirección de destino)
 * @param attrs los atributos a asignar en la entrada de la tabla de páginas
 */
void mmu_map_page(uint32_t cr3, vaddr_t virt, paddr_t phy, uint32_t attrs) {
  pd_entry_t* pd = (pd_entry_t *) cr3;
  uint32_t pd_index = (virt >> 22) & 0x3FF; //indice en PageDirectory
  uint32_t pt_index = (virt >> 12) & 0x3FF; //indice en PageTable

  if (!(pd[pd_index].attrs & MMU_P)) {
    paddr_t new_pt = mmu_next_free_kernel_page();
    zero_page(new_pt);
    pd[pd_index].pt = new_pt >> 12;
    pd[pd_index].attrs = MMU_P | MMU_W | (attrs & MMU_U);
  }

  pt_entry_t* pt = (pt_entry_t*)(pd[pd_index].pt << 12);
  pt[pt_index].page = phy >> 12;
  pt[pt_index].attrs = attrs | MMU_P;
  tlbflush();

}
/*
static pd_entry_t* kpd = (pd_entry_t*)KERNEL_PAGE_DIR;
static pt_entry_t* kpt = (pt_entry_t*)KERNEL_PAGE_TABLE_0;
*/
/**
 * mmu_unmap_page elimina la entrada vinculada a la dirección virt en la tabla de páginas correspondiente
 * @param virt la dirección virtual que se ha de desvincular
 * @return la dirección física de la página desvinculada
 */
paddr_t mmu_unmap_page(uint32_t cr3, vaddr_t virt) {
  uint32_t pd_index = (virt >> 22) & 0x3FF;
  uint32_t pt_index = (virt >> 12) & 0x3FF;
  pd_entry_t* pd = (pd_entry_t*) cr3;
  

  if (!(pd[pd_index].attrs & MMU_P)) {
    return 0;
  }

  pt_entry_t* pt = (pt_entry_t*) (pd[pd_index].pt << 12);
  paddr_t phy = pt[pt_index].page << 12;

  pt[pt_index].attrs = 0;
  pt[pt_index].page  = 0;
 
  tlbflush();

  return phy;

}

#define DST_VIRT_PAGE 0xA00000
#define SRC_VIRT_PAGE 0xB00000

/**
 * copy_page copia el contenido de la página física localizada en la dirección src_addr a la página física ubicada en dst_addr
 * @param dst_addr la dirección a cuya página queremos copiar el contenido
 * @param src_addr la dirección de la página cuyo contenido queremos copiar
 *
 * Esta función mapea ambas páginas a las direcciones SRC_VIRT_PAGE y DST_VIRT_PAGE, respectivamente, realiza
 * la copia y luego desmapea las páginas. Usar la función rcr3 definida en i386.h para obtener el cr3 actual
 */
void copy_page(paddr_t dst_addr, paddr_t src_addr) {
  uint32_t cr3 = rcr3();

  mmu_map_page(cr3, SRC_VIRT_PAGE, src_addr, MMU_W); 
  mmu_map_page(cr3, DST_VIRT_PAGE, dst_addr, MMU_W);


  uint32_t* src = (uint32_t*)(uintptr_t) SRC_VIRT_PAGE;
  uint32_t* dst = (uint32_t*)(uintptr_t) DST_VIRT_PAGE;
  size_t words = PAGE_SIZE / sizeof(uint32_t);

  //copiar contenido byte a byte
   for (size_t i = 0; i < words; i++) {
    dst[i] = src[i];
  }

  //desmapeo las paginas virtuales anteriormente creadas.
  mmu_unmap_page(cr3, SRC_VIRT_PAGE);
  mmu_unmap_page(cr3, DST_VIRT_PAGE);
}

void test_copy_page(){
  paddr_t src = mmu_next_free_kernel_page(); 
  paddr_t dst = mmu_next_free_kernel_page();

  uint8_t *p = (uint8_t*)src;
  for (int i = 0; i < PAGE_SIZE; i++) p[i] = 0xAA;
  copy_page(dst, src);

}

 /**
 * mmu_init_task_dir inicializa las estructuras de paginación vinculadas a una tarea cuyo código se encuentra en la dirección phy_start
 * @pararm phy_start es la dirección donde comienzan las dos páginas de código de la tarea asociada a esta llamada
 * @return el contenido que se ha de cargar en un registro CR3 para la tarea asociada a esta llamada
 */
paddr_t mmu_init_task_dir(paddr_t phy_start) {
  paddr_t pd_phys = mmu_next_free_kernel_page(); 
  pd_entry_t* pd = (pd_entry_t*) pd_phys;
  zero_page(pd_phys);
  uint32_t cr3 = (uint32_t)pd_phys; 

  for (size_t i = 0; i <= identity_mapping_end / PAGE_SIZE; i++) {
      mmu_map_page(cr3, i * PAGE_SIZE, i * PAGE_SIZE, MMU_P | MMU_W);
  }

  mmu_map_page(cr3, TASK_CODE_VIRTUAL, phy_start, MMU_P | MMU_U);
  mmu_map_page(cr3, TASK_CODE_VIRTUAL + PAGE_SIZE, phy_start + PAGE_SIZE, MMU_P | MMU_U); 

  paddr_t stack_phys = mmu_next_free_user_page();
  mmu_map_page(cr3, TASK_STACK_TOP, stack_phys, MMU_P | MMU_W | MMU_U);

  mmu_map_page(cr3, TASK_SHARED_PAGE, SHARED, MMU_P | MMU_W | MMU_U);

  return cr3;

}

// COMPLETAR: devuelve true si se atendió el page fault y puede continuar la ejecución 
// y false si no se pudo atender
// EL ON_DEMAND_MEM_START_VRITUAL y ON_DEMAND_MEM_END_VIRTUAL es nada más un rango que se pone en direcciones virtuales para entender que si alguien llegó a esos lugares es porque solicita una nueva página virtual para acceder a una región específica de la memoria física.
bool page_fault_handler(vaddr_t virt) {
    print("Atendiendo page fault...", 0, 0, C_FG_WHITE | C_BG_BLACK);
    uint32_t cr3 = rcr3();

    if (virt >= ON_DEMAND_MEM_START_VIRTUAL && virt <= ON_DEMAND_MEM_END_VIRTUAL) {
        //uso solamente next_free_on_demand_page porque solo hay una unica página disponible fisicamente para bajo demanda. 
        paddr_t phys_page = next_free_on_demand_page;
        vaddr_t aligned = virt & 0xFFFFF000;
        mmu_map_page(cr3, aligned, phys_page, MMU_W | MMU_U);
        return true; 
    }

    return false; 
}

void test_on_demand_write_1(){
  page_fault_handler(0x07000000);
}

void test_on_demand_write_2(){
  page_fault_handler(0x07000000);
}