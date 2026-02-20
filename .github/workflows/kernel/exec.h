// exec.h
#ifndef EXEC_H
#define EXEC_H
#include "kernel.h"

#define EXEC_OK             0
#define EXEC_ERR_NOT_FOUND  1
#define EXEC_ERR_BAD_FORMAT 2
#define EXEC_ERR_NO_FS      3
#define EXEC_ERR_NO_MEM     4

typedef struct {
    int32_t exit_code;
    int32_t error;
} exec_result_t;

exec_result_t exec_load(const char* filename);
#endif
