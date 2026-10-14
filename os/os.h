#pragma once

#include <stddef.h>
#include <stdint.h>

typedef intptr_t os_handle_t;

#define OS_INVALID_HANDLE ((os_handle_t)-1)

typedef struct {
    os_handle_t read_end;
    os_handle_t write_end;
} os_pipe_t;

typedef struct {
    intptr_t handle;
    unsigned long id;
} os_process_t;

int os_pipe_create(os_pipe_t *pipe);

int os_init(void);
void os_report_error(const char *message);
unsigned long os_current_process_id(void);

int os_process_start(const char *program, os_handle_t input,
                     os_handle_t output, os_process_t *process);

int os_write_all(os_handle_t handle, const void *data, size_t size);

int os_read(os_handle_t handle, void *buffer, size_t size, size_t *bytes_read);

int os_close(os_handle_t handle);

int os_process_wait(os_process_t process, int *exit_code);
