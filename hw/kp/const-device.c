#include "hw/kp/const-device.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/log.h"
#include "trace/trace-hw_kp.h"

static uint64_t kp_const_device_register_read(void* opaque, hwaddr addr, unsigned size)
{
    KPConstDeviceState* s = KP_CONSTDEVICE(opaque);

    if (addr != 0)
    {
        trace_kp_const_device_error_reg_read(addr);
        return 0;
    }

    return s->value;
}

static void kp_const_device_register_write(void* opaque, hwaddr addr, uint64_t adata, unsigned size)
{
    trace_kp_const_device_error_reg_write(addr, adata);
}

static const MemoryRegionOps RegisterOps = {
    .read = kp_const_device_register_read,
    .write = kp_const_device_register_write,
};

static void kp_const_device_realize(DeviceState* dev, Error** errp)
{
    KPConstDeviceState* s = KP_CONSTDEVICE(dev);

    MemoryRegion* system_memory = get_system_memory();
    MemoryRegion* region = g_new(MemoryRegion, 1);

    memory_region_init_io(region, NULL, &RegisterOps, s, "kp-const-device", s->size);
    memory_region_add_subregion(system_memory, s->addr, region);
}

static Property kp_const_device_props[] = {
    DEFINE_PROP_UINT64("addr", KPConstDeviceState, addr, 0x008000C7),
    DEFINE_PROP_UINT8("size", KPConstDeviceState, size, 1),
    DEFINE_PROP_UINT64("value", KPConstDeviceState, value, 0),

    DEFINE_PROP_END_OF_LIST(),
};

static void kp_const_device_class_init(ObjectClass* klass, void* data)
{
    DeviceClass* dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, kp_const_device_props);

    dc->realize = kp_const_device_realize;
    dc->desc = "KP Labs - Const Value Device";
    set_bit(DEVICE_CATEGORY_KP, dc->categories);
}

static TypeInfo kp_const_device_info = {
    .name = TYPE_KP_CONST_DEVICE,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(KPConstDeviceState),
    .class_init = kp_const_device_class_init,
};

static void kp_const_device_register_type(void)
{
    type_register_static(&kp_const_device_info);
}

type_init(kp_const_device_register_type)
