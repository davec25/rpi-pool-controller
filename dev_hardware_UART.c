#include "dev_hardware_UART.h"


#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

APPLE::APPLE(const char *UART_device, int baud = B9600)
{
    //device
    if((uart_fd = open(UART_device, O_RDWR | O_NOCTTY)) < 0)  { 
        perror("Failed to open UART device.\n");  
        exit(1); 
    } else {
        DEV_HARDWARE_UART_Debug("open : %s\r\n", UART_device);
    }
    setBaud(baud);
    UART_Set(8, 1, 'N');
}

APPLE::~APPLE(void)
{
    if (close(uart_fd) != 0){
        DEV_HARDWARE_UART_Debug("Failed to close UART device\r\n");
        perror("Failed to close UART device.\n");  
    }
}

/******************************************************************************
function:   Set the serial port baud rate
parameter:
    Baudrate : Serial port baud rate
               B50          50
               B75          75
               B110         110
               B134         134
               B150         150
               B200         200
               B300         300
               B600         600
               B1200        1200
               B1800        1800
               B2400        2400
               B4800        4800
               B9600        9600
               B19200       19200
               B38400       38400
               B57600       57600
               B115200      115200
               B230400      230400
Info:
******************************************************************************/
void APPLE::setBaud(uint32_t Baudrate)
{
    uint16_t err;
    uint32_t baud;
    tcflush(uart_fd, TCIOFLUSH);
    
    switch(Baudrate)        //Set the number of data bits
    {
        case B230400:
        case 230400:    baud = B230400; break;
        case B115200:   
        case 115200:    baud = B115200; break;
        case B57600:   
        case 57600:     baud = B57600; break;
        case B38400:   
        case 38400:     baud = B38400; break;
        case B19200:   
        case 19200:     baud = B19200; break;
        case B9600:   
        case 9600:  baud = B9600; break;
        case B4800:   
        case 4800:  baud = B4800; break;
        case B2400:   
        case 2400:  baud = B2400; break;
        case B1800:   
        case 1800:  baud = B1800; break;
        case B1200:   
        case 1200:  baud = B1200; break;
        case B600:   
        case 600:   baud = B600; break;
        case B300:   
        case 300:   baud = B300; break;
        case B200:   
        case 200:   baud = B200; break;
        case B150:   
        case 150:   baud = B150; break;
        case B134:   
        case 134:   baud = B134; break;
        case B110:   
        case 110:   baud = B110; break;
        case B75:   
        case 75:    baud = B75; break;
        case B50:   
        case 50:    baud = B50; break;
        default:    baud = B9600;break;

    }
    DEV_HARDWARE_UART_Debug("Baud rate setting\r\n");
 	if(cfsetispeed(&uart_set, baud) != 0){
        DEV_HARDWARE_UART_Debug("Baud rate setting failed 1\r\n");
    }
	if(cfsetospeed(&uart_set, baud) != 0){
        DEV_HARDWARE_UART_Debug("Baud rate setting failed 2\r\n");
        
    }
    err = tcsetattr(uart_fd, TCSANOW, &uart_set);
    if(err != 0){
        perror("tcsetattr fd");
        DEV_HARDWARE_UART_Debug("Setting the terminal baud rate failed\r\n");
    }
}

uint8_t APPLE::write(const char * buf, uint32_t len)
{
    int ret;
    ret = ::write(uart_fd, buf, len); 
    if (-1 == ret)
        perror("write");
    return 0;
}

uint8_t APPLE::read(unsigned char* buf, uint32_t len)
{
    int ret, cnt = 0;
    while (len--) {
        ret = ::read(uart_fd, buf, 1);
        if (-1 == ret) {
            perror("read");
            return cnt;
        }
        *buf++;
        cnt++;
    }
#ifdef NOT
    ret = ::read(uart_fd, buf, len);
    if (-1 == ret)
        perror("read");
#endif
    return cnt;
}

int APPLE::UART_Set(int databits, int stopbits, int parity)
{
    if(tcgetattr(uart_fd, &uart_set) != 0)
    {
        perror("tcgetattr fd");
        DEV_HARDWARE_UART_Debug("Failed to get terminal parameters\r\n");
        return 0;
    }
    uart_set.c_cflag |= (CLOCAL | CREAD);        //Generally set flag

    switch(databits)        //Set the number of data bits
    {
    case 5:
        uart_set.c_cflag &= ~CSIZE;
        uart_set.c_cflag |= CS5;
        break;
    case 6:
        uart_set.c_cflag &= ~CSIZE;
        uart_set.c_cflag |= CS6;
        break;
    case 7:
        uart_set.c_cflag &= ~CSIZE;
        uart_set.c_cflag |= CS7;
        break;
    case 8:
        uart_set.c_cflag &= ~CSIZE;
        uart_set.c_cflag |= CS8;
        break;
    default:
        fprintf(stderr, "Unsupported data size.\n");
        return 0;
    }

    switch(parity)            //Set check digit
    {
    case 'n':
    case 'N':
        uart_set.c_cflag &= ~PARENB;        //Clear check digit
        uart_set.c_iflag &= ~INPCK;         //enable parity checking
        break;
    case 'o':
    case 'O':
        uart_set.c_cflag |= PARENB;         //enable parity
        uart_set.c_cflag |= PARODD;         //Odd parity
        uart_set.c_iflag |= INPCK;          //disable parity checking
        break;
    case 'e':     
    case 'E':        
        uart_set.c_cflag |= PARENB;         //enable parity        
        uart_set.c_cflag &= ~PARODD;        //Even parity         
        uart_set.c_iflag |= INPCK;          //disable pairty checking        
        break;     
    case 's':    
    case 'S':         
        uart_set.c_cflag &= ~PARENB;        //Clear check digit         
        uart_set.c_cflag &= ~CSTOPB;
        uart_set.c_iflag |= INPCK;          //disable pairty checking        
        break;    
    default:         
        fprintf(stderr, "Unsupported parity.\n");        
        return 0;        
    }
    switch(stopbits)        //Set stop bit  1   2
    {     
        case 1:         
            uart_set.c_cflag &= ~CSTOPB;        
            break;     
        case 2:        
            uart_set.c_cflag |= CSTOPB;    
            break;   
        default:     
            fprintf(stderr, "Unsupported stopbits.\n");         
            return 0;     
    }    
    uart_set.c_cflag |= (CLOCAL | CREAD);
    uart_set.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    uart_set.c_oflag &= ~OPOST;
    uart_set.c_oflag &= ~(ONLCR | OCRNL);
    uart_set.c_iflag &= ~(ICRNL | INLCR);    
    uart_set.c_iflag &= ~(IXON | IXOFF | IXANY);
    tcflush(uart_fd, TCIFLUSH);
    uart_set.c_cc[VTIME] = 150;        //Set timeout
    uart_set.c_cc[VMIN] = 1;        //
    if(tcsetattr(uart_fd, TCSANOW, &uart_set) != 0)   //Update the UART_Set and do it now  
    {
        perror("tcsetattr fd");
        DEV_HARDWARE_UART_Debug("Setting terminal parameters failed\r\n");
        return 0;    
    }   
    DEV_HARDWARE_UART_Debug("Set terminal parameters successfully\r\n");
    return 1;
}

