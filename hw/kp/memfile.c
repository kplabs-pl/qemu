#include "qemu/osdep.h"
#include "qemu/module.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "hw/core/cpu.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"

typedef struct KPMemfileState {
    DeviceState parent_obj;

    uint64_t addr;
    uint64_t region_length;
    char* file;
    bool read_only;
} KPMemfileState;

#define TYPE_KP_MEMFILE "kp-memfile"
#define KP_MEMFILE(obj) OBJECT_CHECK(KPMemfileState, (obj), \
                                         TYPE_KP_MEMFILE)

static void kp_memfile_realize(DeviceState *dev, Error **errp)
{
    KPMemfileState *s = KP_MEMFILE(dev);

    MemoryRegion *system_memory = get_system_memory();

    MemoryRegion *region = g_new(MemoryRegion, 1);

    memory_region_init_ram_from_file(region, NULL, "some_memory", s->region_length,
        0,
        RAM_SHARED,
        s->file,
        errp
    );

    memory_region_set_readonly(region, s->read_only);

    memory_region_add_subregion(system_memory, s->addr, region);
}

static void kp_memfile_unrealize(DeviceState *dev)
{
}

static Property kp_memfile_props[] = {
    DEFINE_PROP_UINT64("addr", KPMemfileState, addr, 0x60000000),
    DEFINE_PROP_UINT64("len", KPMemfileState, region_length, 0),
    DEFINE_PROP_STRING("file", KPMemfileState, file),
    DEFINE_PROP_BOOL("read-only", KPMemfileState, read_only, true),
    DEFINE_PROP_END_OF_LIST(),
};

static void kp_memfile_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    device_class_set_props(dc, kp_memfile_props);

    dc->realize = kp_memfile_realize;
    dc->unrealize = kp_memfile_unrealize;

    dc->desc = "KP Labs - File-backed memory region";
    set_bit(DEVICE_CATEGORY_KP, dc->categories);
}

static TypeInfo kp_memfile_info = {
    .name = TYPE_KP_MEMFILE,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(KPMemfileState),
    .class_init = kp_memfile_class_init,
};

static void kp_memfile_register_type(void)
{
    type_register_static(&kp_memfile_info);
}

type_init(kp_memfile_register_type)