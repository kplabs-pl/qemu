#include "hw/kp/edi.h"
#include "hw/irq.h"
#include "qapi/error.h"
#include "cpu.h"
#include "hw/sparc/grlib.h"

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
        SPARCCPU* cpu = SPARC_CPU(first_cpu);
        void* irq_manager = cpu->env.irq_manager;
        if(s->irq != NULL)
        {
            qemu_free_irq(s->irq);
        }
        s->irq = qemu_allocate_irq(grlib_irqmp_set_irq, irq_manager, s->registers.interrupt);
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
