#ifndef __debug_lockup_h_
#define __debug_lockup_h_


void irq_trace_stop(unsigned long flag);
void irq_trace_start(unsigned long flag);
void pr_lockup_info(int cpu);
void lockup_hook(int cpu);
void isr_in_hook(unsigned int c, unsigned long long *t,
	unsigned int i, void *a);
void isr_out_hook(unsigned int cpu, unsigned long long tin, unsigned int irq);
void irq_trace_en(int en);
void sirq_in_hook(unsigned int cpu, unsigned long long *tin, void *p);
void sirq_out_hook(unsigned int cpu, unsigned long long tin, void *p);
void aml_wdt_disable_dbg(void);
void  notrace __arch_cpu_idle_enter(void);
void  notrace __arch_cpu_idle_exit(void);
#endif
