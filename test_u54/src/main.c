
/* Based on SiFive Welcome example:  Pogodaev I.V  3 July 2023*/

#include <stdio.h>
#include <metal/cpu.h>

#include <metal/uart.h>
#include <string.h>

#define RTC_FREQ    32768

struct metal_cpu *cpu;
struct metal_interrupt *cpu_intr, *tmr_intr;
int tmr_id;

// mutex related
volatile unsigned int custom_lock;

// uart related
struct metal_uart *uart0;

// messages
char *uart0_msg = "Main Uart0.\n";
int uart0_msg_len;
char *irq_msg = "IRQ Handler - Uart0.\n";
int irq_msg_len;

void display_banner (void) {

    printf("\n");
    printf("               Welcome to SiFive!\n");

}

void send_message (struct metal_uart *uart, char *msg, int len) {
   // transmitting message
   for (int i = 0; i < len; i++) {
	 metal_uart_putc(uart, msg[i]);
	 while (metal_uart_txready(uart) != 0) {};
   }
}

void lock(volatile unsigned int *lock) {
	int t0, t1;
	__asm__ __volatile__(
	        "   li %1, 1\n"
	        "1:"
	        "   lw %2, %0\n"
	        "   bnez %2, 1b\n"
	        "   amoswap.w.aq %2, %1, %0\n"
	        "   bnez %2, 1b\n"
	        : "+A" (*lock), "=&r" (t0), "=&r" (t1)
	    	:: "memory"
	    );
}

void unlock(volatile unsigned int *lock) {
	__asm__ __volatile__(
	        "   amoswap.w.rl x0, x0, %0\n"
	        : "=A" (*lock)
	    	:: "memory"
	    );
}

void timer_isr (int id, void *data) {

    // Disable Timer interrupt
    metal_interrupt_disable(tmr_intr, tmr_id);

    // lock
    lock(&custom_lock);

    //printf("IRQ handler.\n");

    // transmitting message
    send_message(uart0, irq_msg, irq_msg_len);

    // unlock
    unlock(&custom_lock);

    // set timer
    metal_cpu_set_mtimecmp(cpu, metal_cpu_get_mtime(cpu) + RTC_FREQ);

    // Enable Timer interrupt
    metal_interrupt_enable(tmr_intr, tmr_id);
}


int main (void)
{
    int rc;

    // Lets go
    uart0_msg_len = strlen(uart0_msg);
    irq_msg_len = strlen(irq_msg);
    custom_lock = 0;

    // Lets get the CPU and and its interrupt
    cpu = metal_cpu_get(metal_cpu_get_current_hartid());
    if (cpu == NULL) {
        printf("CPU null.\n");
        return 2;
    }
    cpu_intr = metal_cpu_interrupt_controller(cpu);
    if (cpu_intr == NULL) {
        printf("CPU interrupt controller is null.\n");
        return 3;
    }
    metal_interrupt_init(cpu_intr);

    // display welcome banner
    display_banner();

    // Setup Timer and its interrupt
    tmr_intr = metal_cpu_timer_interrupt_controller(cpu);
    if (tmr_intr == NULL) {
        printf("TIMER interrupt controller is  null.\n");
        return 4;
    }
    metal_interrupt_init(tmr_intr);
    tmr_id = metal_cpu_timer_get_interrupt_id(cpu);
    rc = metal_interrupt_register_handler(tmr_intr, tmr_id, timer_isr, cpu);
    if (rc < 0) {
        printf("TIMER interrupt handler registration failed\n");
        return (rc * -1);
    }

    // Lastly CPU interrupt
    if (metal_interrupt_enable(cpu_intr, 0) == -1) {
        printf("CPU interrupt enable failed\n");
        return 6;
    }

    // Set timer
    metal_cpu_set_mtimecmp(cpu, metal_cpu_get_mtime(cpu) + RTC_FREQ);
    // Enable Timer interrupt
    metal_interrupt_enable(tmr_intr, tmr_id);

    // Setup UART 0
    uart0 = metal_uart_get_device(0);
    metal_uart_init(uart0, 115200);

    // do work here

    while (1) {
    	// lock
    	lock(&custom_lock);

    	//printf("Main cycle.\n");

        // transmitting message
        send_message(uart0, uart0_msg, uart0_msg_len);

    	// unlock
        unlock(&custom_lock);


    	// delay 3 cpu cycle - without delay frezes up
    	__asm__("wfi");
    	__asm__("wfi");
    	__asm__("wfi");

    }

    // return
    return 0;
}
