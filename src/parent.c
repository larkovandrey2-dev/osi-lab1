#include "os.h"

#include <stdio.h>
#include <string.h>

static int close_end(os_handle_t *handle)
{
    if (*handle == OS_INVALID_HANDLE) {
        return 0;
    }
    int result = os_close(*handle);
    *handle = OS_INVALID_HANDLE;
    if (result == -1) {
        os_report_error("close pipe");
    }
    return result;
}

static int read_exact(os_handle_t handle, char *buffer, size_t length)
{
    size_t total = 0;
    while (total < length) {
        size_t count;
        if (os_read(handle, buffer + total, length - total, &count) == -1) {
            os_report_error("read from child2");
            return -1;
        }
        if (count == 0) {
            fprintf(stderr, "Child2 closed its output before returning all data\n");
            return -1;
        }
        total += count;
    }
    return 0;
}

int main(void)
{
    os_pipe_t pipes[3] = {
        {OS_INVALID_HANDLE, OS_INVALID_HANDLE},
        {OS_INVALID_HANDLE, OS_INVALID_HANDLE},
        {OS_INVALID_HANDLE, OS_INVALID_HANDLE}
    };
    os_process_t children[2] = {{0, 0}, {0, 0}};
    char buffer[4096];
    int result = 1;

    if (os_init() == -1) {
        goto cleanup;
    }
    for (int i = 0; i < 3; ++i) {
        if (os_pipe_create(&pipes[i]) == -1) {
            os_report_error("create pipe");
            goto cleanup;
        }
    }
    if (os_process_start("./child1", pipes[0].read_end,
                         pipes[1].write_end, &children[0]) == -1) {
        os_report_error("start child1");
        goto cleanup;
    }
    if (os_process_start("./child2", pipes[1].read_end,
                         pipes[2].write_end, &children[1]) == -1) {
        os_report_error("start child2");
        goto cleanup;
    }

    fprintf(stderr, "Parent PID: %lu\nChild1 PID: %lu\nChild2 PID: %lu\n",
            os_current_process_id(), children[0].id, children[1].id);

    if (close_end(&pipes[0].read_end) == -1) goto cleanup;
    if (close_end(&pipes[1].read_end) == -1) goto cleanup;
    if (close_end(&pipes[1].write_end) == -1) goto cleanup;
    if (close_end(&pipes[2].write_end) == -1) goto cleanup;

    fprintf(stderr, "Enter strings (end input with EOF):\n");
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t length = strlen(buffer);
        if (os_write_all(pipes[0].write_end, buffer, length) == -1) {
            os_report_error("write to child1");
            goto cleanup;
        }
        if (read_exact(pipes[2].read_end, buffer, length) == -1) {
            goto cleanup;
        }
        if (fwrite(buffer, 1, length, stdout) != length || fflush(stdout) == EOF) {
            perror("print result");
            goto cleanup;
        }
    }
    if (ferror(stdin)) {
        perror("read input");
        goto cleanup;
    }

    result = 0;

cleanup:

    for (int i = 0; i < 3; ++i) {
        if (close_end(&pipes[i].write_end) == -1) result = 1;
        if (close_end(&pipes[i].read_end) == -1) result = 1;
    }
    for (int i = 0; i < 2; ++i) {
        if (children[i].id == 0) continue;
        int exit_code;
        if (os_process_wait(children[i], &exit_code) == -1) {
            os_report_error("wait for child");
            result = 1;
        } else if (exit_code != 0) {
            fprintf(stderr, "Child%d exited with code %d\n", i + 1, exit_code);
            result = 1;
        }
    }
    return result;
}
