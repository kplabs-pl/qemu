#ifndef KP_ARGS_DEVICE_H
#define KP_ARGS_DEVICE_H

/**
 * KPArgsDevice is a simple device for passing arguments from the host system to the guest system without a semihosting.
 * Only one register is used for communication:
 *
 * - reading returns the current character at the internal index and increments this index
 * - writing anything will reset the internal index
 *
 * When null is encountered, the index stops incrementing.
 *
 * Command line example for arduino:
 *
 * @code
 * qemu-system-avr -machine arduino-uno -device kp-args-device,cmdline="some arguments" -bios compiled_program
 * @endcode
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"

typedef struct KPArgsDeviceState {
    DeviceState parent_obj;

    uint64_t addr;
    uint32_t index;
    char* cmdline;
} KPArgsDeviceState;

#define TYPE_KP_ARGS_DEVICE "kp-args-device"
#define KP_ARGSDEVICE(obj) OBJECT_CHECK(KPArgsDeviceState, (obj), \
                                         TYPE_KP_ARGS_DEVICE)

#endif
