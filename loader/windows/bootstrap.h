#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t complete;
    int32_t result;
    uint64_t module;
    uint32_t initialize_rva;
    uint32_t reserved;
    uint16_t path_length,path_capacity;
    uint32_t padding;
    uint64_t path_pointer;
} extend_remote_context;
static_assert(sizeof(extend_remote_context)==40,"APC context contract");
static_assert(offsetof(extend_remote_context,path_pointer)==32,"UNICODE_STRING alignment");
