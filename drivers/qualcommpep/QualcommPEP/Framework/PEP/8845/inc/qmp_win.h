/* Copyright (c) 2026 Nika Okromtchedlishvili */
#pragma once
#include <ntddk.h>
#include <wdf.h>

NTSTATUS qmp_win_init(WDFDEVICE dev);
NTSTATUS qmp_win_prepare(WDFDEVICE dev, WDFCMRESLIST raw, WDFCMRESLIST translated);
void qmp_win_start(WDFDEVICE dev);
void qmp_win_stop(WDFDEVICE dev);
void qmp_win_release(WDFDEVICE dev);
void qmp_win_cleanup(WDFDEVICE dev);
