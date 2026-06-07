#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"

__asm(".global __use_no_semihosting");

typedef int FILEHANDLE;

void _sys_exit(int return_code) {
	(void)return_code;
}

void _ttywrch(int ch) {
	(void)ch;
}

void _sys_command_string(char *cmd) {
	(void)cmd;
}

FILEHANDLE _sys_open(const char *name, int openmode) {
	(void)name;
	(void)openmode;
	return -1;
}

int _sys_close(FILEHANDLE fh) {
	(void)fh;
	return 0;
}

int _sys_write(FILEHANDLE fh, const unsigned char *buf, unsigned len, int mode) {
	(void)fh;
	(void)buf;
	(void)mode;
	return (int)len;
}

int _sys_read(FILEHANDLE fh, unsigned char *buf, unsigned len, int mode) {
	(void)fh;
	(void)buf;
	(void)mode;
	return (int)len;
}

int _sys_istty(FILEHANDLE fh) {
	(void)fh;
	return 1;
}

int _sys_seek(FILEHANDLE fh, long pos) {
	(void)fh;
	(void)pos;
	return -1;
}

int _sys_ensure(FILEHANDLE fh) {
	(void)fh;
	return 0;
}

long _sys_flen(FILEHANDLE fh) {
	(void)fh;
	return 0;
}

void *malloc(size_t size) {
	return pvPortMalloc(size);
}

void free(void *ptr) {
	vPortFree(ptr);
}
