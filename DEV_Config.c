#include "DEV_Config.h"

#include <termios.h>
#include <unistd.h>

#include <unistd.h>
#include <fcntl.h>


void PEAR::DEV_GPIO_Mode(int pin, uint16_t Mode)
{
    /*
        0:  input mode   
        1:  output mode
    */
    SYSFS_GPIO_Export(pin);
    if(Mode == 0 || Mode == SYSFS_GPIO_IN){
        SYSFS_GPIO_Direction(pin, SYSFS_GPIO_IN);
    }else{
        SYSFS_GPIO_Direction(pin, SYSFS_GPIO_OUT);
    }
}

PEAR::PEAR(int channel, uint32_t baud=19200)
{
    IRQ_PIN=25;
    if (1 == channel) {
        hw_uart = new APPLE("/dev/ttySC0", baud);
        TXDEN = 27;
    } else if (2 == channel) {
        hw_uart = new APPLE("/dev/ttySC1", baud);
        TXDEN = 22;
    } else { throw BadChannelNbr(); }

    DEV_GPIO_Mode(TXDEN, 1);
    SYSFS_GPIO_Write(TXDEN, 1);
    writeMode = false;
    DEV_GPIO_Mode(IRQ_PIN, 0);
}

PEAR::~PEAR()
{
    delete hw_uart;
    hw_uart = NULL;
}

int PEAR::read(uint8_t *pData, uint32_t len)
{
    if (writeMode) {
       SYSFS_GPIO_Write(TXDEN, 1);
       writeMode = false;
       printf("changed write mode to false\n");
    }
    return hw_uart->read((unsigned char *)pData, len);
}

void PEAR::setBaud(uint32_t Baudrate)
{
    hw_uart->setBaud(Baudrate);
}

int PEAR::write(uint8_t *pData, uint32_t len)
{
    if (!writeMode) {
       SYSFS_GPIO_Write(TXDEN, 0);
       writeMode = true;
       printf("changed write mode to true\n");
    }
    return hw_uart->write((const char *)pData, len);
}

