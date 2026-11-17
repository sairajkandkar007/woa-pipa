/* Copyright (c) 2026 Nika Okromtchedlishvili */
#pragma once
#include <stdint.h>

#define QMP_PACKET_SIZE 64u

enum qmp_result {
    QMP_OK,
    QMP_INVALID,
    QMP_NOT_READY,
    QMP_TIMEOUT,
    QMP_IO_ERROR,
    QMP_BUSY
};

typedef struct qmp_io {
    uint32_t (*read)(void* context, uint32_t offset);
    void (*write)(void* context, uint32_t offset, uint32_t value);
    int (*kick)(void* context);
    uint64_t (*time_ms)(void* context);
    void (*delay)(void* context);
} qmp_io_t;

typedef struct qmp {
    qmp_io_t io;
    void* context;
    uint32_t window;
    uint32_t offset;
    uint32_t capacity;
    uint8_t opened;
    uint8_t faulted;
} qmp_t;

/* the caller serializes access and retains the mapping through each call. */
void qmp_init(qmp_t* qmp, const qmp_io_t* io, void* context, uint32_t window);
enum qmp_result qmp_send(qmp_t* qmp, const char* message, uint32_t length);
void qmp_close(qmp_t* qmp);
