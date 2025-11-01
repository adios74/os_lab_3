#ifndef LAB3_H
#define LAB3_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SHM_SIZE 4096
#define MAX_STRINGS 10
#define MAX_STRING_SIZE 100

typedef struct {
    uint32_t length;
    char text[MAX_STRING_SIZE];
    bool processed;
} string_data_t;

typedef struct {
    string_data_t strings[MAX_STRINGS];
    int count;
    bool server_finished;
} shared_data_t;

#endif