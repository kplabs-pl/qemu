#ifndef KP_EXIT_DEVICE_H
#define KP_EXIT_DEVICE_H

/**
 * KPExitDevice is a device for gracefully exiting qemu with desired exit code.
 * 
 * It provides only one register:
 *
 * - reading is not supported and always returns 0
 * - writing will close qemu with code given as the provided value
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"

typedef struct KPExitDeviceState {
    DeviceState parent_obj;

    uint64_t addr;
} KPExitDeviceState;

#define TYPE_KP_EXIT_DEVICE "kp-exit-device"
#define KP_EXITDEVICE(obj) OBJECT_CHECK(KPExitDeviceState, (obj), \
                                         TYPE_KP_EXIT_DEVICE)

#endif
