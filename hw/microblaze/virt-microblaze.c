#include "qemu/osdep.h"
#include "qapi/error.h"
#include "sysemu/sysemu.h"
#include "cpu.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "qapi/visitor.h"
#include "hw/kp/posix-device.h"

#include "boot.h"

typedef struct {
    MachineState parent_obj;

    MicroBlazeCPU cpu;
    uint64_t ram_base;
    uint64_t vectors_base;
    uint64_t intc_base;
    uint64_t timer_base;
    uint64_t posix_base;
    uint32_t timer_irq;
    DeviceState* intc_device;
} VirtMicroblazeMachineState;

#define TYPE_VIRT_MICROBLAZE_MACHINE MACHINE_TYPE_NAME("virt-microblaze")

#define VIRT_MICROBLAZE_MACHINE(obj) \
    OBJECT_CHECK(VirtMicroblazeMachineState, obj, TYPE_VIRT_MICROBLAZE_MACHINE)

static void virt_microblaze_init(MachineState *ms) {
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(ms);
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();

    uint64_t effective_vectors_base = m->vectors_base;
    if(effective_vectors_base == UINT64_MAX)
    {
        effective_vectors_base = m->ram_base;
    }
    else
    {
        if(!((m->ram_base <= effective_vectors_base) && (effective_vectors_base < (m->ram_base + ms->ram_size - 0x50))))
        {
            error_printf("Vector table base address must fit inside machine's RAM");
            exit(1);
            return;
        }
    }

    object_initialize_child(OBJECT(m), "cpu", &m->cpu, TYPE_MICROBLAZE_CPU);
    object_property_set_str(OBJECT(&m->cpu), "version", "9.6", &error_abort);
    object_property_set_uint(OBJECT(&m->cpu), "base-vectors",
                            effective_vectors_base, &error_abort);
    qdev_realize(DEVICE(&m->cpu), NULL, &error_abort);
    
    memory_region_init_ram(ram, NULL, "virt-microblaze.ram", ms->ram_size,
                           &error_fatal);
    memory_region_add_subregion(system_memory, m->ram_base, ram);

    DeviceState *dev;

    //create interrupt controller
    dev = qdev_new("xlnx.xps-intc");
    qdev_prop_set_uint32(dev, "kind-of-intr", 0xffffffff);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, m->intc_base);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                       qdev_get_gpio_in(DEVICE(&m->cpu), MB_CPU_IRQ));
    m->intc_device = dev;

    // create timer
    dev = qdev_new("xlnx.xps-timer");
    qdev_prop_set_uint32(dev, "one-timer-only", 0);
    qdev_prop_set_uint32(dev, "clock-frequency", 100 * 1000000);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, m->timer_base);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                       qdev_get_gpio_in(m->intc_device, m->timer_irq));

    // create kp-posix
    dev = qdev_new(TYPE_KP_POSIX);
    qdev_prop_set_uint64(dev, "addr", m->posix_base);
    qdev_realize_and_unref(dev, NULL, &error_fatal);

    microblaze_load_kernel(&m->cpu, m->ram_base, ms->ram_size,
                           ms->initrd_filename,
                           ms->dtb,
                           NULL);
}

static void ram_base_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint64(v, name, &m->ram_base, errp);
}

static void ram_base_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint64(v, name, &m->ram_base, errp);
}

static void vectors_base_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint64(v, name, &m->vectors_base, errp);
}

static void vectors_base_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint64(v, name, &m->vectors_base, errp);
}

static void intc_base_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint64(v, name, &m->intc_base, errp);
}

static void intc_base_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint64(v, name, &m->intc_base, errp);
}

static void timer_base_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint64(v, name, &m->timer_base, errp);
}

static void timer_base_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint64(v, name, &m->timer_base, errp);
}

static void timer_irq_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint32(v, name, &m->timer_irq, errp);
}

static void timer_irq_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint32(v, name, &m->timer_irq, errp);
}


static void virt_microblaze_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->desc = "Virtual Microblaze";
    mc->init = virt_microblaze_init;

    object_class_property_add(
        oc, "ram-base", "int",
        &ram_base_get, &ram_base_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "ram-base",
        "RAM base address"
    );

    object_class_property_add(
        oc, "vectors-base", "int",
        &vectors_base_get, &vectors_base_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "vectors-base",
        "Vector table base address"
    );

    object_class_property_add(
        oc, "intc-base", "int",
        &intc_base_get, &intc_base_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "intc-base",
        "Interrupt controller base address"
    );

    object_class_property_add(
        oc, "timer-base", "int",
        &timer_base_get, &timer_base_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "timer-base",
        "Timer base address"
    );

    object_class_property_add(
        oc, "timer-irq", "int",
        &timer_irq_get, &timer_irq_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "timer-irq",
        "Timer IRQ number"
    );
}

static void virt_microblaze_instance_init(Object* obj)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    m->ram_base = 0x00000000;
    m->vectors_base = UINT64_MAX;
    m->intc_base = 0x80000000;
    m->timer_base = 0x80010000;
    m->posix_base = 0xA0001000;
    m->timer_irq = 0;

    object_property_add_link(OBJECT(m), "intc_device", TYPE_DEVICE,
                             (Object**)&m->intc_device, NULL,
                             OBJ_PROP_LINK_STRONG);
}

static const TypeInfo virt_microblaze_type = {
    .name = TYPE_VIRT_MICROBLAZE_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(VirtMicroblazeMachineState),
    .class_init = virt_microblaze_class_init,
    .instance_init = virt_microblaze_instance_init,
};


static void virt_microblaze_machine_init(void)
{
    type_register_static(&virt_microblaze_type);
}

type_init(virt_microblaze_machine_init)
