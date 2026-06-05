#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

__asm(".global __use_no_semihosting");

void _sys_exit(int return_code) {
	(void)return_code;
}

void _ttywrch(int ch) {
	(void)ch;
}

void _sys_command_string(char *cmd) {
	(void)cmd;
}

void *malloc(size_t size) {
	return pvPortMalloc(size);
}

void free(void *ptr) {
	vPortFree(ptr);
}
