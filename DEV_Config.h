#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include "sysfs_gpio.h"
#include "dev_hardware_UART.h"

#include <stdint.h>
#include "Debug.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <exception>
using namespace std;

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

class PEAR {

private:
    int IRQ_PIN;//25   
    int TXDEN; //27 or 22
    APPLE *hw_uart;
    void DEV_GPIO_Mode(int pin, UWORD Mode);
    bool writeMode;

public:

    PEAR(int channel, uint32_t Baudrate);
    ~PEAR();

    void setBaud(uint32_t Baudrate);
    int write(uint8_t *pData, uint32_t Lan);
    int read(uint8_t *pData, uint32_t Lan);

    struct BadChannelNbr : public exception {
        const char * what () const throw () {
            return "Bad Channel Number - only 1 or 2";
        }
    };


}; // class PEAR

#endif
