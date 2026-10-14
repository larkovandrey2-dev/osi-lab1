#include "os.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int os_init(void)
{
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal SIGPIPE");
        return -1;
    }
    return 0;
}

void os_report_error(const char *message)
{
    perror(message);
}

static int set_close_on_exec(int fd)
{
    int flags = fcntl(fd, F_GETFD);

    if (flags == -1) {
        return -1;
    }

    if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
        return -1;
    }

    return 0;
}
unsigned long os_current_process_id(void)
{
    return (unsigned long)getpid();
}

int os_pipe_create(os_pipe_t *pipe_handle)
{
    if (pipe_handle == NULL) {
        errno = EINVAL;
        return -1;
    }

    int fd[2];

    if (pipe(fd) == -1) {
        return -1;
    }

    if (set_close_on_exec(fd[0]) == -1 ||
        set_close_on_exec(fd[1]) == -1) {
        int saved_errno = errno;

        if (close(fd[0]) == -1) perror("close pipe read end");
        if (close(fd[1]) == -1) perror("close pipe write end");

        errno = saved_errno;
        return -1;
    }

    pipe_handle->read_end = fd[0];
    pipe_handle->write_end = fd[1];

    return 0;
}

int os_process_start(const char *program, os_handle_t input,
                     os_handle_t output, os_process_t *process)
{
    if (program == NULL || process == NULL) {
        errno = EINVAL;
        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        return -1;
    }

    if (pid == 0) {
        if (dup2((int)input, STDIN_FILENO) == -1) {
            perror("dup2 stdin");
            _exit(126);
        }

        if (dup2((int)output, STDOUT_FILENO) == -1) {
            perror("dup2 stdout");
            _exit(126);
        }

        execl(program, program, (char *)NULL);
        perror("execl");
        _exit(127);
    }
    process->handle = (intptr_t)pid;
    process->id = (unsigned long)pid;

    return 0;
}

int os_write_all(os_handle_t handle, const void *data, size_t size)
{
    if (data == NULL && size != 0) {
        errno = EINVAL;
        return -1;
    }

    const unsigned char *buffer = data;

    size_t total_written = 0;

    while (total_written < size) {
        ssize_t written = write((int)handle, buffer + total_written,
                                size - total_written);

        if (written == -1) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (written == 0) {
            errno = EIO;
            return -1;
        }

        total_written += (size_t)written;
    }

    return 0;
}

int os_read(os_handle_t handle, void *buffer, size_t size, size_t *bytes_read)
{
    if (buffer == NULL || bytes_read == NULL) {
        errno = EINVAL;
        return -1;
    }

    while (1) {
        ssize_t result = read((int)handle, buffer, size);

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        *bytes_read = (size_t)result;

        return 0;
    }
}

int os_close(os_handle_t handle)
{
    return close((int)handle);
}

int os_process_wait(os_process_t process, int *exit_code)
{
    int status;

    pid_t result;

    do {
        result = waitpid((pid_t)process.handle, &status, 0);
    }
    while (result == -1 && errno == EINTR);

    if (result == -1) {
        return -1;
    }

    if (exit_code != NULL) {
        if (WIFEXITED(status)) {
            *exit_code = WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status)) {
            *exit_code = 128 + WTERMSIG(status);
        }
        else {
            *exit_code = -1;
        }
    }

    return 0;
}
