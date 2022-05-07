#ifndef __DEV_HARDWARE_UART_
#define __DEV_HARDWARE_UART_

#include <termios.h>
#include <unistd.h>
#include <stdint.h>


#define DEV_HARDWARE_UART_DEBUG 0
#if DEV_HARDWARE_UART_DEBUG
#define DEV_HARDWARE_UART_Debug(__info,...) printf("Debug: " __info,##__VA_ARGS__)
#else
#define DEV_HARDWARE_UART_Debug(__info,...)
#endif

class APPLE {

private:
#ifdef NOT
typedef struct UARTStruct {
    //GPIO
    uint16_t TXD_PIN;
    uint16_t RXD_PIN;
    
    int fd; //
    uint16_t addr; //
} HARDWARE_UART;
#endif

struct termios uart_set;
int uart_fd;

public:
    APPLE(const char *UART_device, int baud);
    ~APPLE(void);
    void setBaud(uint32_t Baudrate);

    uint8_t write(const char * buf, uint32_t len);
    uint8_t read(unsigned char* buf, uint32_t len);

    int UART_Set(int databits, int stopbits, int parity);
}; // class APPLE

#endif
