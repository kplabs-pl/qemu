#include "hw/kp/args-device.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/log.h"

static uint64_t kp_args_device_register_read(void* opaque, hwaddr addr, unsigned size)
{
    KPArgsDeviceState* s = KP_ARGSDEVICE(opaque);
    if (s->cmdline[s->index] == '\0')
    {
        return 0;
    }

    return s->cmdline[s->index++];
}

static void kp_args_device_register_write(void* opaque, hwaddr addr, uint64_t adata, unsigned size)
{
    KPArgsDeviceState* s = KP_ARGSDEVICE(opaque);
    s->index = 0;
}

static const MemoryRegionOps RegisterOps = {
    .read = kp_args_device_register_read,
    .write = kp_args_device_register_write,
};

static void kp_args_device_realize(DeviceState* dev, Error** errp)
{
    KPArgsDeviceState* s = KP_ARGSDEVICE(dev);
    s->index = 0;
    if (!s->cmdline) {
        s->cmdline = g_strdup("");
    }

    MemoryRegion* system_memory = get_system_memory();
    MemoryRegion* region = g_new(MemoryRegion, 1);

    memory_region_init_io(region, NULL, &RegisterOps, s, "kp-args-device", 1);
    memory_region_add_subregion(system_memory, s->addr, region);
}

static Property kp_args_device_props[] = {
    DEFINE_PROP_UINT64("addr", KPArgsDeviceState, addr, 0x008000C7),
    DEFINE_PROP_STRING("cmdline", KPArgsDeviceState, cmdline),

    DEFINE_PROP_END_OF_LIST(),
};

static void kp_args_device_class_init(ObjectClass* klass, void* data)
{
    DeviceClass* dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, kp_args_device_props);

    dc->realize = kp_args_device_realize;
    dc->desc = "KP Labs - Program Argument Device";
    set_bit(DEVICE_CATEGORY_KP, dc->categories);
}

static TypeInfo kp_args_device_info = {
    .name = TYPE_KP_ARGS_DEVICE,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(KPArgsDeviceState),
    .class_init = kp_args_device_class_init,
};

static void kp_args_device_register_type(void)
{
    type_register_static(&kp_args_device_info);
}

type_init(kp_args_device_register_type)
