; ** por compatibilidad se omiten tildes **
; ==============================================================================
; TALLER System Programming - Arquitectura y Organizacion de Computadoras - FCEN
; ==============================================================================

%include "print.mac"

global start


; COMPLETAR - Agreguen declaraciones extern según vayan necesitando
extern A20_enable
extern GDT_DESC
extern IDT_DESC

extern idt_init
extern pic_reset
extern pic_enable
extern mmu_init_kernel_dir
extern mmu_init_task_dir

;solo para fines de test
extern test_copy_page
extern test_on_demand_write_1
extern test_on_demand_write_2

; tareas
extern tss_init
extern tasks_screen_draw
extern sched_init
extern tasks_init

extern screen_draw_layout
%define C_FG_LIGHT_CYAN 0xB
%define STACK_TOP 0x25000
%define TASK_INITIAL_SELECTOR   (11 << 3) | (0 << 2) | 0
%define TASK_IDLE_SELECTOR      (12 << 3) | (0 << 2) | 0

; COMPLETAR - Definan correctamente estas constantes cuando las necesiten
%define CS_RING_0_SEL (1 << 3) | (0 << 2) | 0
%define DS_RING_0_SEL (3 << 3) | (0 << 2) | 0
;
; RING hace referencia a los llamados "anillos" de privilegio usados por la CPU para controlar qué código puede hacer qué cosa. 
; Van del 0 al 3, mas privilegiados a menos respectivamente.
;
; Como los selectores de descriptores tienen el siguiente protocolo:
; - [0:2) = RPL, el privilegio.
; - [2:3) = TI, que si es 0 referencia a un descriptor de la GDT, y si es 1 a la actual LDT.
; - [3:16) = Index, que apunta a uno de los 8192 descriptores de la tabla pertinente.
;
; Entonces los formamos con la formula: (Index << 3) | (TI << 2) | RPL
;
; Index lo obtenemos del valor de GDT_IDX_CODE_0 y GDT_IDX_DATA_0 respectivamente.
; TI es 0 pues sabemos que los descriptores estan en la GDT.

BITS 16
;; Saltear seccion de datos
jmp start

;;
;; Seccion de datos.
;; -------------------------------------------------------------------------- ;;
start_rm_msg db     'Iniciando kernel en Modo Real'
start_rm_len equ    $ - start_rm_msg

start_pm_msg db     'Iniciando kernel en Modo Protegido'
start_pm_len equ    $ - start_pm_msg

;;
;; Seccion de código.
;; -------------------------------------------------------------------------- ;;

;; Punto de entrada del kernel.
BITS 16
start:

    CLI

    mov ax, 0003h
    int 10h ; set mode 03h
    xor bx, bx
    mov ax, 1112h
    int 10h ; load 8x8 font

    print_text_rm start_rm_msg, start_rm_len, C_FG_LIGHT_CYAN, 0, 0

    call A20_enable

    LGDT [GDT_DESC]

    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CS_RING_0_SEL:modo_protegido


BITS 32
modo_protegido:
    ;seteo de segmentos y stack
    mov ax, DS_RING_0_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, STACK_TOP
    mov ebp, STACK_TOP

    ;salida por pantalla
    print_text_pm start_pm_msg, start_pm_len, C_FG_LIGHT_CYAN, 2, 0
    call screen_draw_layout

    ; directorio de páginas del kernel    
    call mmu_init_kernel_dir
    mov cr3, eax
    mov eax, cr0
    or eax, 1 << 31 ; tenemos que activar el bit 31 de CR0
    mov cr0, eax

    ; inicialización de las tss en la gdt, scheduler y tasks.
    call tss_init
    call sched_init
    call tasks_init

    ; interrupciones
    call idt_init 
    lidt [IDT_DESC]
    call pic_reset
    call pic_enable

    ;reloj del PIT
    mov ax, 8193182
    out 0x40, al
    rol ax, 8 
    out 0x40, al 

    ; dibujar layout
    call tasks_screen_draw

    ; tarea idle
    mov ax, TASK_INITIAL_SELECTOR
    ltr ax
    jmp TASK_IDLE_SELECTOR:0
    
    mov eax, 0xFFFF
    mov ebx, 0xFFFF
    mov ecx, 0xFFFF
    mov edx, 0xFFFF
    jmp $

;; -------------------------------------------------------------------------- ;;

%include "a20.asm"