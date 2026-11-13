/* kernel_main.c: Enclave OS for Raspberry Pi 1 B+ */
#include "led.h"
#include "uart.h"
#include "interrupt.h"

// Global variables for exception handling
volatile unsigned int exc_probe = 0;
volatile unsigned int exc_caught = 0;
volatile unsigned int exc_pc = 0;

void panic(const char* msg, void* addr) {
    while(1) {
        // Infinite loop - should never reach here  
    }
}

void kernel_main(unsigned int r0, unsigned int r1, void* r2) {
    // Initialize peripherals
    
    // Initialize UART
    uart_init();
    
    // Test UART output first
    uart_puts("Enclave OS: Starting...\n");
    
    // Initialize LED  
    led_init_gpio();
    
    // Enable UART pinmuxing
    // GPIO14 (TX) and GPIO15 (RX) in Alt0
    // This configuration is done in boot.S in a real implementation
    
    uart_puts("Enclave OS: Hello world!\n");

    // Test LED functionality  
    for(int i = 0; i < 10; i++) {
        led_toggle();
        uart_puts("LED toggled\n");
    }

    // Main kernel loop
    while(1) {
        // Loop forever
    }
}