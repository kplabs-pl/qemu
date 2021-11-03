#include "qemu/osdep.h"
#include "qapi/error.h"
#include "sysemu/sysemu.h"
#include "cpu.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"

#include "boot.h"

typedef struct {
    MachineState parent_obj;

    MicroBlazeCPU cpu;
} VirtMicroblazeMachineState;

#define TYPE_VIRT_MICROBLAZE_MACHINE MACHINE_TYPE_NAME("virt-microblaze")

#define VIRT_MICROBLAZE_MACHINE(obj) \
    OBJECT_CHECK(VirtMicroblazeMachineState, obj, TYPE_VIRT_MICROBLAZE_MACHINE)

#define RAM_BASEADDR 0x80000000
#define UART_BASEADDR 0x40000000
#define INTC_BASEADDR 0x40010000
#define TIMER_BASEADDR 0x40020000

#define TIMER_IRQ 0

static void virt_microblaze_init(MachineState *ms) {
    VirtMicroblazeMachineState* m = VIRT_MICROBLAZE_MACHINE(ms);
    MemoryRegion *ram = g_new(MemoryRegion, 1);
    MemoryRegion *system_memory = get_system_memory();
    qemu_irq irq[32];

    object_initialize_child(OBJECT(m), "cpu", &m->cpu, TYPE_MICROBLAZE_CPU);
    object_property_set_str(OBJECT(&m->cpu), "version", "8.40.b", &error_abort);
    object_property_set_uint(OBJECT(&m->cpu), "base-vectors",
                             RAM_BASEADDR, &error_abort);
    qdev_realize(DEVICE(&m->cpu), NULL, &error_abort);
    
    memory_region_init_ram(ram, NULL, "virt-microblaze.ram", ms->ram_size,
                           &error_fatal);
    memory_region_add_subregion(system_memory, RAM_BASEADDR, ram);

    DeviceState *dev;
    SysBusDevice *s;

    //create interrupt controller
    dev = qdev_new("xlnx.xps-intc");
    qdev_prop_set_uint32(dev, "kind-of-intr", 1 << TIMER_IRQ);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, INTC_BASEADDR);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0,
                       qdev_get_gpio_in(DEVICE(&m->cpu), MB_CPU_IRQ));
    for (int i = 0; i < 32; i++) {
        irq[i] = qdev_get_gpio_in(dev, i);
    }

    // create uartlite
    dev = qdev_new("xlnx.xps-uartlite");
    s = SYS_BUS_DEVICE(dev);
    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    sysbus_realize_and_unref(s, &error_fatal);
    sysbus_mmio_map(s, 0, UART_BASEADDR);

    // create timer
    dev = qdev_new("xlnx.xps-timer");
    qdev_prop_set_uint32(dev, "one-timer-only", 0);
    qdev_prop_set_uint32(dev, "clock-frequency", 100 * 1000000);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, TIMER_BASEADDR);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[TIMER_IRQ]);

    microblaze_load_kernel(&m->cpu, RAM_BASEADDR, ms->ram_size,
                           ms->initrd_filename,
                           ms->dtb,
                           NULL);
}

static void virt_microblaze_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);
    mc->desc = "Virtual Microblaze";
    mc->init = virt_microblaze_init;
}

static void virt_microblaze_instance_init(Object* obj)
{
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
