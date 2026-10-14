#include "os.h"

#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>

int os_init(void)
{
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 ||
        _setmode(_fileno(stdout), _O_BINARY) == -1) {
        perror("set binary mode");
        return -1;
    }
    return 0;
}

void os_report_error(const char *message)
{
    fprintf(stderr, "%s: Windows error %lu\n", message,
            (unsigned long)GetLastError());
}

unsigned long os_current_process_id(void)
{
    return (unsigned long)GetCurrentProcessId();
}

int os_pipe_create(os_pipe_t *pipe)
{
    if (pipe == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    HANDLE read_end, write_end;

    if (!CreatePipe(&read_end, &write_end, NULL, 4096)) {
        return -1;
    }
    pipe->read_end = (intptr_t)read_end;
    pipe->write_end = (intptr_t)write_end;
    return 0;
}

int os_process_start(const char *program, os_handle_t input,
                     os_handle_t output, os_process_t *process)
{
    if (program == NULL || process == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    char path[MAX_PATH];
    int length = snprintf(path, sizeof(path), "%s.exe", program);
    if (length < 0 || (size_t)length >= sizeof(path)) {
        SetLastError(ERROR_BUFFER_OVERFLOW);
        return -1;
    }
    for (int i = 0; i < length; ++i) {
        if (path[i] == '/') path[i] = '\\';
    }

    HANDLE error_output = GetStdHandle(STD_ERROR_HANDLE);
    if (error_output == NULL || error_output == INVALID_HANDLE_VALUE) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }
    HANDLE originals[3] = {(HANDLE)input, (HANDLE)output, error_output};
    HANDLE inherited[3] = {NULL, NULL, NULL};
    HANDLE current = GetCurrentProcess();
    STARTUPINFOA start = {0};
    PROCESS_INFORMATION info = {0};
    DWORD error = ERROR_SUCCESS;
    int result = -1;

    for (int i = 0; i < 3; ++i) {
        if (!DuplicateHandle(current, originals[i], current, &inherited[i],
                             0, TRUE, DUPLICATE_SAME_ACCESS)) {
            error = GetLastError();
            goto cleanup;
        }
    }
    start.cb = sizeof(start);
    start.dwFlags = STARTF_USESTDHANDLES;
    start.hStdInput = inherited[0];
    start.hStdOutput = inherited[1];
    start.hStdError = inherited[2];
    if (!CreateProcessA(path, NULL, NULL, NULL, TRUE, 0, NULL, NULL,
                        &start, &info)) {
        error = GetLastError();
        goto cleanup;
    }

    process->handle = (intptr_t)info.hProcess;
    process->id = (unsigned long)info.dwProcessId;
    result = 0;
    if (!CloseHandle(info.hThread)) {
        error = GetLastError();
        result = -1;
    }

cleanup:
    for (int i = 0; i < 3; ++i) {
        if (inherited[i] != NULL && !CloseHandle(inherited[i])) {
            DWORD close_error = GetLastError();
            os_report_error("close inherited handle copy");
            if (error == ERROR_SUCCESS) error = close_error;
            result = -1;
        }
    }
    if (result == -1) SetLastError(error);
    return result;
}

int os_write_all(os_handle_t handle, const void *data, size_t size)
{
    if (data == NULL && size != 0) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    const unsigned char *buffer = data;
    size_t total = 0;
    while (total < size) {
        size_t remaining = size - total;
        DWORD requested = remaining > MAXDWORD ? MAXDWORD : (DWORD)remaining;
        DWORD written;
        if (!WriteFile((HANDLE)handle, buffer + total, requested, &written, NULL)) {
            return -1;
        }
        if (written == 0) {
            SetLastError(ERROR_WRITE_FAULT);
            return -1;
        }
        total += written;
    }
    return 0;
}

int os_read(os_handle_t handle, void *buffer, size_t size, size_t *bytes_read)
{
    if (buffer == NULL || bytes_read == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return -1;
    }
    DWORD requested = size > MAXDWORD ? MAXDWORD : (DWORD)size;
    DWORD count;
    if (!ReadFile((HANDLE)handle, buffer, requested, &count, NULL)) {
        if (GetLastError() == ERROR_BROKEN_PIPE) {
            *bytes_read = 0;
            return 0;
        }
        return -1;
    }
    *bytes_read = count;
    return 0;
}

int os_close(os_handle_t handle)
{
    return CloseHandle((HANDLE)handle) ? 0 : -1;
}

int os_process_wait(os_process_t process, int *exit_code)
{
    HANDLE handle = (HANDLE)process.handle;
    DWORD code = 0;
    DWORD error = ERROR_SUCCESS;
    DWORD wait = WaitForSingleObject(handle, INFINITE);
    if (wait != WAIT_OBJECT_0) {
        error = wait == WAIT_FAILED ? GetLastError() : ERROR_GEN_FAILURE;
    } else if (!GetExitCodeProcess(handle, &code)) {
        error = GetLastError();
    }
    if (!CloseHandle(handle)) {
        DWORD close_error = GetLastError();
        os_report_error("close process handle");
        if (error == ERROR_SUCCESS) error = close_error;
    }
    if (error != ERROR_SUCCESS) {
        SetLastError(error);
        return -1;
    }

    if (exit_code != NULL) *exit_code = code <= 255 ? (int)code : 1;
    return 0;
}
