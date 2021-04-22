#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "hw/core/cpu.h"
#include "exec/address-spaces.h"
#include <nanomsg/pair.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>
#include "hw/sysbus.h"
#include "hw/kp/edi.h"
#include "trace/trace-hw_kp.h"
#include "edi-list.h"

static void kp_edi_init(Object* obj)
{
    KPEDIState *s = KP_EDI(obj);
    kp_edi_init_arch(s);

    s->connection_state = edi_connection_state_disconnected;
    s->socket = -1;
    s->communication_mode = edi_mode_pair;
    s->socket_read_handle = 0;
    s->socket_write_handle = 0;
    kp_edi_chunk_list_init(&s->receive_list);
    kp_edi_chunk_list_init(&s->send_list);
}

static void kp_edi_realize(DeviceState* dev, Error** errp)
{
    KPEDIState *s = KP_EDI(dev);

    trace_kp_edi_realize(s->name, s->addr);

    s->irq = NULL;
    s->registers.interrupt = UINT32_MAX;

    MemoryRegion *system_memory = get_system_memory();

    MemoryRegion *region = g_new(MemoryRegion, 1);

    memory_region_init_io(region, NULL, s->register_ops, s, "kp-edi", s->address_space_size);

    memory_region_add_subregion(system_memory, s->addr, region);
}

static Property kp_edi_props[] = {
    DEFINE_PROP_UINT64("addr", KPEDIState, addr, 0xF0000010),
    DEFINE_PROP_STRING("name", KPEDIState, name),
    DEFINE_PROP_END_OF_LIST(),
};

static void kp_edi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

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
