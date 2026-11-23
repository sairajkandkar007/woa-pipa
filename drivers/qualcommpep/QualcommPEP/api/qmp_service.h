/* Copyright (c) 2026 Nika Okromtchedlishvili */
#pragma once
#include <stdint.h>

#define QMP_INTERFACE_GUID                                                                         \
    {0x61e00e5b, 0x0f85, 0x414e, {0x86, 0x94, 0x9d, 0x8c, 0x6d, 0x9a, 0x4b, 0xe9}}
#define QMP_INTERFACE_VERSION 1u
#define QMP_MESSAGE_SIZE 64u

typedef struct qmp_interface {
    uint16_t size;
    uint16_t version;
    void* context;
    void (*reference)(void* context);
    void (*dereference)(void* context);
    /* passive level; success means the peer consumed the mailbox. */
    int32_t (*send)(void* context, const char* message, uint32_t length);
} qmp_interface_t;
