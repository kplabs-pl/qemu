#ifndef KP_CONST_DEVICE_H
#define KP_CONST_DEVICE_H

/**
 * KPConstDevice is a simple device that always returns the same contents.
 * Only one register is used for communication:
 *
 * - reading always returns constant value
 * - writing is not supported
 */

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"

typedef struct KPConstDeviceState {
    DeviceState parent_obj;

    uint64_t addr;
    uint8_t size;
    uint64_t value;
} KPConstDeviceState;

#define TYPE_KP_CONST_DEVICE "kp-const-device"
#define KP_CONSTDEVICE(obj) OBJECT_CHECK(KPConstDeviceState, (obj), \
                                         TYPE_KP_CONST_DEVICE)

#endif
