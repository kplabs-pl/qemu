#include "hw/kp/edi.h"
#include "target/arm/cpu-qom.h"
#include "target/arm/cpu.h"
#include "hw/intc/armv7m_nvic.h"
#include "hw/irq.h"
#include "qapi/error.h"

static void trigger_irq(KPEDIState* s)
{
    if(s->irq != NULL)
    {
        qemu_irq_pulse(s->irq);
    }
}

static void set_irq(KPEDIState* s)
{
    if(s->registers.interrupt == UINT32_MAX)
    {
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

void kp_edi_init_arch(KPEDIState *s)
{
    s->registers.id = 0xDEADCAFE;
    s->trigger_irq = trigger_irq;
    s->set_irq = set_irq;
    s->register_ops = &KpEdiRegisterOpsReg32Ptr32;
    s->address_space_size = KP_EDI_ADDRESS_SPACE_SIZE_REG32_PTR32;
}
