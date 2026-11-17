#ifndef TLMM_H
#define TLMM_H
/* SM8250 (kona) TLMM: three tiles WEST 0x0F100000, SOUTH 0x0F500000, NORTH 0x0F900000 (each 0x300000,
 * pipa DT / Silicium memory map). Pin n's registers are at <tile base> + n * 0x1000 (Linux
 * pinctrl-sm8250.c). One window 0x0F100000..0x0FC00000 is mapped; the tile of each pin is below. */

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TLMM_BLOCK_PHYS UINT64_C(0x0f100000)
#define TLMM_TILE0_OFFSET 0x0u

#define TLMM_PHYS (TLMM_BLOCK_PHYS + TLMM_TILE0_OFFSET)
#define TLMM_BYTES 0xB00000u
#define TLMM_PIN_CNT 180u
#define TLMM_PIN_STRIDE 0x1000u
#define TLMM_CFG_EGPIO_PRESENT 0x800u
#define TLMM_CFG_EGPIO_ENABLE 0x1000u
/* 'W' = WEST (+0x000000), 'S' = SOUTH (+0x400000), 'N' = NORTH (+0x800000), generated from Linux. */
#define TLMM_TILE_MAP "SSSSNNNNNNNNNNNNNNNNNNNNSSSSNNNNSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSNNNSSSSSSSSNNNNNNNNSSSSSSSNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNSSSSSSSSWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"
#define TLMM_TILE_OFFSET(pin) \
    (TLMM_TILE_MAP[(pin)] == 'S' ? 0x400000u : TLMM_TILE_MAP[(pin)] == 'N' ? 0x800000u : 0u)
#define TLMM_CTL(pin) (TLMM_TILE_OFFSET(pin) + (uint32_t)(pin) * TLMM_PIN_STRIDE)
#define TLMM_IO(pin) (TLMM_CTL(pin) + 4u)
/* Configuration bounds do not grant pin ownership. */

typedef struct tlmm_gpio {
    uint32_t pin;
    uint32_t state;
    uint32_t function;
    uint32_t direction;
    uint32_t pull;
    uint32_t drive;
} tlmm_gpio_t;

typedef struct tlmm_io {
    void* ctx;
    int (*lock)(void*);
    void (*unlock)(void*);
    int (*owns_pin)(void*, uint32_t pin);
    int (*read32)(void*, uint32_t offset, uint32_t* value);
    int (*write32)(void*, uint32_t offset, uint32_t value);
    void (*barrier)(void*);
} tlmm_io_t;

typedef enum tlmm_result {
    TLMM_OK = 0,
    TLMM_INVALID,
    TLMM_NOT_OWNED,
    TLMM_IO_FAILED,
    TLMM_VERIFY_FAILED,
    TLMM_ROLLBACK_FAILED
} tlmm_result_t;

/* populate the provider request in the exact Qualcomm ACPI order: [PinNumber, GPIOState,
 * FunctionSelect, Direction, PullValue, DriveStrength]. */

int tlmm_decode_dsd_t_fields(const uint32_t fields[6], tlmm_gpio_t* config);
/* integration must confirm the Molokai pin tile selection; DSDT _CRS and a PCI/ACPI res
 * assignment are not implicit ownership. */

/* still requires provider-owned TLMM MMIO and does not imply device/PoFx ownership. */

struct power_action;
tlmm_result_t tlmm_apply_power_action(const tlmm_io_t* io, const struct power_action* action);
tlmm_result_t tlmm_apply(const tlmm_io_t* io, const tlmm_gpio_t* config);
#ifdef __cplusplus
}
#endif
#endif
