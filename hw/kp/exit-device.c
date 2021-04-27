#include "hw/kp/exit-device.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/log.h"

static uint64_t kp_exit_device_register_read(void* opaque, hwaddr addr, unsigned size)
{
    return 0;
}

static void kp_exit_device_register_write(void* opaque, hwaddr addr, uint64_t adata, unsigned size)
{
    exit(adata);
}

static const MemoryRegionOps RegisterOps = {
    .read = kp_exit_device_register_read,
    .write = kp_exit_device_register_write,
};

static void kp_exit_device_realize(DeviceState* dev, Error** errp)
{
    KPExitDeviceState* s = KP_EXITDEVICE(dev);

    MemoryRegion* system_memory = get_system_memory();
    MemoryRegion* region = g_new(MemoryRegion, 1);

    memory_region_init_io(region, NULL, &RegisterOps, s, "kp-exit-device", 1);
    memory_region_add_subregion(system_memory, s->addr, region);
}

static Property kp_exit_device_props[] = {
    DEFINE_PROP_UINT64("addr", KPExitDeviceState, addr, 0x008000C7),

    DEFINE_PROP_END_OF_LIST(),
};

static void kp_exit_device_class_init(ObjectClass* klass, void* data)
{
    DeviceClass* dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, kp_exit_device_props);

    dc->realize = kp_exit_device_realize;
    dc->desc = "KP Labs - QEmu exit device";
    set_bit(DEVICE_CATEGORY_KP, dc->categories);
}

static TypeInfo kp_exit_device_info = {
    .name = TYPE_KP_EXIT_DEVICE,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(KPExitDeviceState),
    .class_init = kp_exit_device_class_init,
};

static void kp_exit_device_register_type(void)
{
    type_register_static(&kp_exit_device_info);
}

type_init(kp_exit_device_register_type)
