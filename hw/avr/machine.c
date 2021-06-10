/*
 * QEMU Generic ATmega boards
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "atmega.h"
#include "boot.h"
#include "qom/object.h"
#include "qapi/visitor.h"

struct ATmegaMachineState {
    /*< private >*/
    MachineState parent_obj;
    /*< public >*/
    AtmegaMcuState mcu;
    uint64_t xtal_hz;
};
typedef struct ATmegaMachineState ATmegaMachineState;

struct ATmegaMachineClass {
    /*< private >*/
    MachineClass parent_class;
    /*< public >*/
    const char *mcu_type;
};
typedef struct ATmegaMachineClass ATmegaMachineClass;

#define TYPE_ATMEGA_MACHINE \
        MACHINE_TYPE_NAME("atmega")
DECLARE_OBJ_CHECKERS(ATmegaMachineState, ATmegaMachineClass,
                     ATMEGA_MACHINE, TYPE_ATMEGA_MACHINE)

static void atmega_machine_init(MachineState *machine)
{
    ATmegaMachineClass *amc = ATMEGA_MACHINE_GET_CLASS(machine);
    ATmegaMachineState *ams = ATMEGA_MACHINE(machine);

    object_initialize_child(OBJECT(machine), "mcu", &ams->mcu, amc->mcu_type);
    object_property_set_uint(OBJECT(&ams->mcu), "xtal-frequency-hz",
                             ams->xtal_hz, &error_abort);
    sysbus_realize(SYS_BUS_DEVICE(&ams->mcu), &error_abort);

    if (machine->firmware) {
        if (!avr_load_firmware(&ams->mcu.cpu, machine,
                               &ams->mcu.flash, machine->firmware)) {
            exit(1);
        }
    }
}

static void xtal_hz_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    ATmegaMachineState* m = ATMEGA_MACHINE(obj);
    visit_type_uint64(v, name, &m->xtal_hz, errp);
}

static void xtal_hz_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    ATmegaMachineState* m = ATMEGA_MACHINE(obj);
    visit_type_uint64(v, name, &m->xtal_hz, errp);
}

static void atmega_machine_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->init = atmega_machine_init;
    mc->default_cpus = 1;
    mc->min_cpus = mc->default_cpus;
    mc->max_cpus = mc->default_cpus;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;

    object_class_property_add(
        oc, "xtal-hz", "int",
        &xtal_hz_get, &xtal_hz_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "xtal-hz",
        "Crystal frequency in Hz"
    );
}

static void atmega644_big_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    ATmegaMachineClass *amc = ATMEGA_MACHINE_CLASS(oc);

    mc->desc      = "Big ATmega644",
    amc->mcu_type = TYPE_ATMEGA644_BIG_MCU;
};

static void atmega644_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    ATmegaMachineClass *amc = ATMEGA_MACHINE_CLASS(oc);

    mc->desc      = "ATmega644";
    amc->mcu_type = TYPE_ATMEGA644_MCU;
};

static void atmega324_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    ATmegaMachineClass *amc = ATMEGA_MACHINE_CLASS(oc);

    mc->desc      = "ATmega324";
    amc->mcu_type = TYPE_ATMEGA324_MCU;
};

static const TypeInfo atmega_machine_types[] = {
    {
        .name          = MACHINE_TYPE_NAME("atmega644-big"),
        .parent        = TYPE_ATMEGA_MACHINE,
        .class_init    = atmega644_big_class_init,
    }, {
        .name          = MACHINE_TYPE_NAME("atmega644"),
        .parent        = TYPE_ATMEGA_MACHINE,
        .class_init    = atmega644_class_init,
    }, {
        .name          = MACHINE_TYPE_NAME("atmega324"),
        .parent        = TYPE_ATMEGA_MACHINE,
        .class_init    = atmega324_class_init,
    }, {
        .name           = TYPE_ATMEGA_MACHINE,
        .parent         = TYPE_MACHINE,
        .instance_size  = sizeof(ATmegaMachineState),
        .class_size     = sizeof(ATmegaMachineClass),
        .class_init     = atmega_machine_class_init,
        .abstract       = true,
    }
};

DEFINE_TYPES(atmega_machine_types)
