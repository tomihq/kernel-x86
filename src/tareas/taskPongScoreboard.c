#include "task_lib.h"
#include "../i386.h"

#define WIDTH TASK_VIEWPORT_WIDTH
#define HEIGHT TASK_VIEWPORT_HEIGHT

#define SHARED_SCORE_BASE_VADDR (PAGE_ON_DEMAND_BASE_VADDR + 0xF00)
#define CANT_PONGS 3

#define SCOREBOARD_X_ORIGIN 5
#define SCOREBOARD_Y_ORIGIN 6
#define ROW_Y(Y) (Y) * 3
#define COLUMN_X(X) (X) * 10

void task(void) {
	screen pantalla;
	// ¿Una tarea debe terminar en nuestro sistema?
	while (true)
	{
	// Completar:
	// - Pueden definir funciones auxiliares para imprimir en pantalla
	// - Pueden usar `task_print`, `task_print_dec`, etc.

		task_print(pantalla, "Player 1", COLUMN_X(1) + SCOREBOARD_X_ORIGIN, SCOREBOARD_Y_ORIGIN, C_FG_BLACK | C_BG_WHITE);
		task_print(pantalla, "Player 2", COLUMN_X(2) + SCOREBOARD_X_ORIGIN, SCOREBOARD_Y_ORIGIN, C_FG_BLACK | C_BG_WHITE);

		for(size_t i = 0; i < CANT_PONGS; i++){
			uint32_t* ENVIRONMENT -> task_id_record = (uint32_t*) (SHARED_SCORE_BASE_VADDR + ((uint32_t) i  * sizeof(uint32_t)*2));

			task_print(pantalla, "Juego ", SCOREBOARD_X_ORIGIN, SCOREBOARD_Y_ORIGIN + ROW_Y(i + 1), C_FG_BLACK | C_BG_WHITE);
			task_print_dec(pantalla, i + 1, 2, SCOREBOARD_X_ORIGIN + 6, SCOREBOARD_Y_ORIGIN + ROW_Y(i + 1), C_FG_BLACK | C_BG_WHITE);

			task_print_dec(pantalla, ENVIRONMENT -> task_id_record[0], 2, COLUMN_X(1) + SCOREBOARD_X_ORIGIN + 3, SCOREBOARD_Y_ORIGIN + ROW_Y(i + 1), C_FG_WHITE);
			task_print_dec(pantalla, ENVIRONMENT -> task_id_record[1], 2, COLUMN_X(2) + SCOREBOARD_X_ORIGIN + 3, SCOREBOARD_Y_ORIGIN + ROW_Y(i + 1), C_FG_WHITE);
		}

		syscall_draw(pantalla);
	}
}
