#include "qemu/osdep.h"
#include "qapi/error.h"
#include "sysemu/sysemu.h"
#include "cpu.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "qapi/visitor.h"
#include "hw/kp/args-device.h"
#include "hw/kp/exit-device.h"

#include "boot.h"

typedef struct {
    MachineState parent_obj;

    MicroBlazeCPU cpu;
    uint64_t ram_base;
    uint64_t intc_base;
    uint64_t timer_base;
    uint64_t uart_base;
    uint64_t args_base;
    uint64_t exit_base;
    uint32_t timer_irq;
    uint32_t uart_irq;
    DeviceState* intc_device;
} VirtMicroblazeMachineState;

#define TYPE_VIRT_MICROBLAZE_MACHINE MACHINE_TYPE_NAME("virt-microblaze")

#define VIRT_MICROBLAZE_MACHINE(obj) \
    OBJECT_CHECK(VirtMicroblazeMachineState, obj, TYPE_VIRT_MICROBLAZE_MACHINE)

static void virt_microblaze_init(MachineState *ms) {
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(ms);
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();

    object_initialize_child(OBJECT(m), "cpu", &m->cpu, TYPE_MICROBLAZE_CPU);
    object_property_set_str(OBJECT(&m->cpu), "version", "9.6", &error_abort);
    object_property_set_uint(OBJECT(&m->cpu), "base-vectors",
                             m->ram_base, &error_abort);
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

    // create uartlite
    dev = qdev_new("xlnx.xps-uartlite");
    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, m->uart_base);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                       qdev_get_gpio_in(m->intc_device, m->uart_irq));

    // create timer
    dev = qdev_new("xlnx.xps-timer");
    qdev_prop_set_uint32(dev, "one-timer-only", 0);
    qdev_prop_set_uint32(dev, "clock-frequency", 100 * 1000000);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, m->timer_base);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                       qdev_get_gpio_in(m->intc_device, m->timer_irq));

    // create kp-args
    dev = qdev_new(TYPE_KP_ARGS_DEVICE);
    qdev_prop_set_uint64(dev, "addr", m->args_base);
    qdev_prop_set_string(dev, "cmdline", ms->kernel_cmdline);
    qdev_realize_and_unref(dev, NULL, &error_fatal);

    // create kp-exit
    dev = qdev_new(TYPE_KP_EXIT_DEVICE);
    qdev_prop_set_uint64(dev, "addr", m->exit_base);
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

static void uart_base_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint64(v, name, &m->uart_base, errp);
}

static void uart_base_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint64(v, name, &m->uart_base, errp);
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

static void uart_irq_set(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint32(v, name, &m->uart_irq, errp);
}

static void uart_irq_get(Object *obj, Visitor *v, const char *name, void *opaque, Error **errp)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    visit_type_uint32(v, name, &m->uart_irq, errp);
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
        oc, "uart-base", "int",
        &uart_base_get, &uart_base_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "uart-base",
        "UART base address"
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

    object_class_property_add(
        oc, "uart_irq", "int",
        &uart_irq_get, &uart_irq_set, NULL,
        NULL
    );

    object_class_property_set_description(
        oc, "uart_irq",
        "UART IRQ number"
    );
}

static void virt_microblaze_instance_init(Object* obj)
{
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(obj);
    m->ram_base = 0x00000000;
    m->intc_base = 0x80000000;
    m->timer_base = 0x80010000;
    m->uart_base = 0x80020000;
    m->args_base = 0xA0001000;
    m->exit_base = 0xA0002000;
    m->timer_irq = 0;
    m->uart_irq = 1;

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
