#include "MemoryUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uintptr_t get_lib_addr(const char* lib_name) {
    FILE* fp;
    char line[1024];
    uintptr_t addr = 0;
    char* p;

    fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, lib_name)) {
            addr = (uintptr_t)strtoul(line, &p, 16);
            break;
        }
    }

    fclose(fp);
    return addr;
}