#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "hw/core/cpu.h"
#include "exec/memory.h"
#include "hw/irq.h"
#include "exec/address-spaces.h"
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>
#include "target/arm/cpu-qom.h"
#include "target/arm/cpu.h"
#include "hw/intc/armv7m_nvic.h"
#include "hw/sysbus.h"
#include "qapi/error.h"
#include "hw/kp/edi.h"

static uint64_t kp_edi_register_read(void *opaque, hwaddr addr, unsigned size)
{
    KPEDIState *s = (KPEDIState*)opaque;

    switch(addr)
    {
        case 0x00:
            return s->registers.command;
        case 0x04:
            return s->registers.pointer;
        case 0x08:
            return s->registers.size;
        case 0x0C:
            return s->registers.interrupt;
        case 0x10:
            return s->registers.id;
        default:
            qemu_log("EDI: read invalid register %lX\n", addr);
            return 0xDEADBEEF;
    }
}


static void trigger_irq(KPEDIState* s)
{
    qemu_log("EDI: triggering irq %d\n", s->registers.interrupt);
   
    if(s->irq != NULL) 
    {
        qemu_irq_raise(s->irq);
    }
}

static void clear_irq(KPEDIState* s)
{
    qemu_log("EDI: clearing irq %d\n", s->registers.interrupt);
   
    if(s->irq != NULL) 
    {
        qemu_irq_lower(s->irq);
    }
}

static void set_irq(KPEDIState* s)
{
    if(s->registers.interrupt == 0)
    {
        qemu_irq_lower(s->irq);
        s->irq = NULL;
    }
    else 
    {
        ARMCPU *cpu = ARM_CPU(first_cpu);
        NVICState* nvic = (NVICState*)cpu->env.nvic;
        
        gchar *propname = g_strdup_printf("unnamed-gpio-in[%u]", s->registers.interrupt);

        Object* irqObj = object_property_get_link(OBJECT(nvic), propname, &error_abort);

        g_free(propname);

        s->irq = (qemu_irq)irqObj;
    }
}

static void kp_edi_register_write(void *opaque, hwaddr addr, uint64_t data, unsigned size)
{
    KPEDIState *s = (KPEDIState*)opaque;

    switch(addr)
    {
        case 0x00:
            s->registers.command = (uint32_t)data;
            if(data == 0x12)
            {
                trigger_irq(s);
            }
            if(data == 0x13)
            {
                clear_irq(s);
            }
            break;
        case 0x04:
            s->registers.pointer = (uint32_t)data;
            break;
        case 0x08:
            s->registers.size = (uint32_t)data;
            break;
        case 0x0C:
            s->registers.interrupt = (uint32_t)data;
            set_irq(s);
            break;
        default:
            qemu_log("EDI: write invalid register %lX\n", addr);
    }
}

static void kp_edi_reset(DeviceState* dev)
{
    // TODO: reset
}

static void kp_edi_init(Object* obj)
{
    KPEDIState *s = KP_EDI(obj);
    s->registers.id = 0xDEADCAFE;
}

static const MemoryRegionOps RegisterOps = {
    .read = kp_edi_register_read,
    .write = kp_edi_register_write,
};

static void kp_edi_realize(DeviceState* dev, Error** errp)
{
    KPEDIState *s = KP_EDI(dev);

    s->irq = NULL;

    MemoryRegion *system_memory = get_system_memory();

    MemoryRegion *region = g_new(MemoryRegion, 1);

    memory_region_init_io(region, NULL, &RegisterOps, s, "kp-edi", KP_EDI_ADDRESS_SPACE_SIZE);

    memory_region_add_subregion(system_memory, s->addr, region);
}

static Property kp_edi_props[] = {
    DEFINE_PROP_UINT64("addr", KPEDIState, addr, 0xF0000010),
    DEFINE_PROP_END_OF_LIST(),
};

static void kp_edi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = kp_edi_reset;
    dc->realize = kp_edi_realize;

    device_class_set_props(dc, kp_edi_props);

    dc->desc = "KP Labs - External Device Interface";
    set_bit(DEVICE_CATEGORY_KP, dc->categories);
}

static TypeInfo kp_edi_info = {
    .name = TYPE_KP_EDI,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(KPEDIState),
    .class_init = kp_edi_class_init,
    .instance_init = kp_edi_init
};

static void kp_edi_register_type(void)
{
    type_register_static(&kp_edi_info);
}

type_init(kp_edi_register_type)