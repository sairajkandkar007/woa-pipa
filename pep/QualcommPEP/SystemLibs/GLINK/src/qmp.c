/* Copyright (c) 2026 Nika Okromtchedlishvili */
#include "qmp.h"

enum {
    DESC_MAGIC = 0x00,
    DESC_VERSION = 0x04,
    REMOTE_LINK = 0x0c,
    REMOTE_LINK_ACK = 0x10,
    REMOTE_CHANNEL = 0x14,
    REMOTE_CHANNEL_ACK = 0x18,
    LOCAL_LINK = 0x24,
    LOCAL_LINK_ACK = 0x28,
    LOCAL_CHANNEL = 0x2c,
    LOCAL_CHANNEL_ACK = 0x30,
    LOCAL_SIZE = 0x34,
    LOCAL_OFFSET = 0x38,
    DESC_SIZE = 0x3c,
    MAGIC = 0x4d41494c,
    STATE_UP = 0x0000ffff,
    STATE_DOWN = 0xffff0000,
    TIMEOUT_MS = 1000
};

static uint32_t read_reg(qmp_t* qmp, uint32_t offset)
{
    return qmp->io.read(qmp->context, offset);
}

static void write_reg(qmp_t* qmp, uint32_t offset, uint32_t value)
{
    qmp->io.write(qmp->context, offset, value);
}

static int valid_desc(qmp_t* qmp)
{
    return qmp->window >= DESC_SIZE && read_reg(qmp, DESC_MAGIC) == MAGIC &&
           read_reg(qmp, DESC_VERSION) == 1;
}

static enum qmp_result wait_reg(qmp_t* qmp, uint32_t offset, uint32_t value)
{
    uint64_t start = qmp->io.time_ms(qmp->context);

    for (;;) {
        if (read_reg(qmp, offset) == value)
            return QMP_OK;
        if (qmp->io.time_ms(qmp->context) - start >= TIMEOUT_MS)
            return QMP_TIMEOUT;
        qmp->io.delay(qmp->context);
    }
}

static enum qmp_result kick(qmp_t* qmp)
{
    return qmp->io.kick(qmp->context) ? QMP_IO_ERROR : QMP_OK;
}

void qmp_init(qmp_t* qmp, const qmp_io_t* io, void* context, uint32_t window)
{
    qmp->io = *io;
    qmp->context = context;
    qmp->window = window;
    qmp->offset = 0;
    qmp->capacity = 0;
    qmp->opened = 0;
    qmp->faulted = 0;
}

void qmp_close(qmp_t* qmp)
{
    if (!qmp || !qmp->opened)
        return;

    /* an unacknowledged packet stays intact for the peer. */
    if (valid_desc(qmp) && read_reg(qmp, LOCAL_OFFSET) == qmp->offset &&
        read_reg(qmp, LOCAL_SIZE) == qmp->capacity && read_reg(qmp, qmp->offset) == 0) {
        write_reg(qmp, LOCAL_CHANNEL, STATE_DOWN);
        write_reg(qmp, LOCAL_LINK, STATE_DOWN);
        (void)kick(qmp);
    }
    qmp->opened = 0;
}

static enum qmp_result open_channel(qmp_t* qmp)
{
    enum qmp_result result;
    uint32_t offset;
    uint32_t capacity;

    if (!valid_desc(qmp))
        return QMP_NOT_READY;

    offset = read_reg(qmp, LOCAL_OFFSET);
    capacity = read_reg(qmp, LOCAL_SIZE);
    if ((offset & 3u) || offset < DESC_SIZE || offset > qmp->window ||
        capacity < QMP_PACKET_SIZE + 4u || capacity > qmp->window - offset)
        return QMP_INVALID;
    if (read_reg(qmp, offset) != 0)
        return QMP_BUSY;

    qmp->offset = offset;
    qmp->capacity = capacity;
    write_reg(qmp, REMOTE_LINK_ACK, read_reg(qmp, REMOTE_LINK));
    write_reg(qmp, LOCAL_LINK, STATE_UP);
    result = kick(qmp);
    if (result == QMP_OK)
        result = wait_reg(qmp, LOCAL_LINK_ACK, STATE_UP);
    if (result != QMP_OK)
        goto fail;

    write_reg(qmp, LOCAL_CHANNEL, STATE_UP);
    result = kick(qmp);
    if (result == QMP_OK)
        result = wait_reg(qmp, REMOTE_CHANNEL, STATE_UP);
    if (result != QMP_OK)
        goto fail;

    write_reg(qmp, REMOTE_CHANNEL_ACK, STATE_UP);
    result = kick(qmp);
    if (result == QMP_OK)
        result = wait_reg(qmp, LOCAL_CHANNEL_ACK, STATE_UP);
    if (result != QMP_OK)
        goto fail;

    qmp->opened = 1;
    return QMP_OK;

fail:
    write_reg(qmp, LOCAL_CHANNEL, STATE_DOWN);
    write_reg(qmp, LOCAL_LINK, STATE_DOWN);
    (void)kick(qmp);
    return result;
}

enum qmp_result qmp_send(qmp_t* qmp, const char* message, uint32_t length)
{
    enum qmp_result result;

    if (!qmp || !message || !length || length > QMP_PACKET_SIZE || !qmp->io.read ||
        !qmp->io.write || !qmp->io.kick || !qmp->io.time_ms || !qmp->io.delay)
        return QMP_INVALID;
    if (qmp->faulted)
        return QMP_NOT_READY;
    if (!qmp->opened) {
        result = open_channel(qmp);
        if (result != QMP_OK)
            return result;
    }

    if (!valid_desc(qmp) || read_reg(qmp, LOCAL_OFFSET) != qmp->offset ||
        read_reg(qmp, LOCAL_SIZE) != qmp->capacity || read_reg(qmp, LOCAL_LINK_ACK) != STATE_UP ||
        read_reg(qmp, LOCAL_CHANNEL_ACK) != STATE_UP || read_reg(qmp, REMOTE_CHANNEL) != STATE_UP) {
        qmp->faulted = 1;
        return QMP_NOT_READY;
    }
    if (read_reg(qmp, qmp->offset) != 0)
        return QMP_BUSY;

    /* message RAM supports 32-bit accesses only. */
    for (uint32_t i = 0; i < QMP_PACKET_SIZE; i += 4) {
        uint32_t value = 0;

        for (uint32_t j = 0; j < 4 && i + j < length; ++j)
            value |= (uint32_t)(uint8_t)message[i + j] << (j * 8);
        write_reg(qmp, qmp->offset + 4u + i, value);
    }
    write_reg(qmp, qmp->offset, QMP_PACKET_SIZE);
    (void)read_reg(qmp, qmp->offset);
    result = kick(qmp);
    if (result == QMP_OK)
        result = wait_reg(qmp, qmp->offset, 0);
    if (result != QMP_OK)
        qmp->faulted = 1;
    return result;
}
