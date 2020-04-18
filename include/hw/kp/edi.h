#ifndef EDI_H
#define EDI_H

#include "qemu/osdep.h"
#include "hw/qdev-core.h"

typedef struct KPEDIState {
    DeviceState parent_obj;

    uint64_t addr;
    qemu_irq irq;

    struct {
        uint32_t command;
        uint32_t pointer;
        uint32_t size;
        uint32_t interrupt;
        uint32_t id;
    } registers;
} KPEDIState;

#define TYPE_KP_EDI "kp-edi"
#define KP_EDI(obj) OBJECT_CHECK(KPEDIState, (obj), \
                                         TYPE_KP_EDI)

#define KP_EDI_ADDRESS_SPACE_SIZE 0x20

#endif