#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>

int fd;

int
OpenMotorPort ()
{
    char *portname = "/dev/ttyUSB0";
    int speed = B19200;
    int parity = 0;

    //int fd = open (portname, O_RDWR );
    fd = open (portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
    {
        printf ("error %d opening %s: %s", errno, portname, strerror (errno));
        return -1;
    }

    struct termios tty;
    if (tcgetattr (fd, &tty) != 0)
    {
        printf ("error %d from tcgetattr", errno);
        return -1;
    }

    cfsetospeed (&tty, speed);
    cfsetispeed (&tty, speed);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
                                    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 20;            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag |= parity;
    //tty.c_cflag &= ~CSTOPB;
    tty.c_cflag |= CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr (fd, TCSANOW, &tty) != 0)
    {
        printf ("error %d from tcsetattr", errno);
        return -1;
    }
    return 0;
}

int getPacket(int fd, unsigned char buf[], int max)
{
    enum { PREAMBLE_10, PREAMBLE_02, POSTAMBLE_10, POSTAMBLE_03 } state;
    int cnt = 0;
    unsigned char databyte;

    state = PREAMBLE_10;
    while(max > cnt) {
        int len = read (fd, &databyte, 1);  
        if (len<1) 
            return(-1);

        switch (state) {
        case PREAMBLE_10:
            if (0x10 != databyte)
                continue;
            state = PREAMBLE_02;
            buf[cnt++] = databyte;
            continue;

        case PREAMBLE_02:
            if (0x02 == databyte) {
                state = POSTAMBLE_10;
                buf[cnt++] = databyte;
                continue;
            } else if (0x10 != databyte) {
                cnt = 0;
                state = PREAMBLE_10;
            } // intentionally leave state and cnt alone
            continue;

        case POSTAMBLE_10:
            if (0x10 == databyte) {
                state = POSTAMBLE_03;
            }
            buf[cnt++] = databyte;
            continue;

        case POSTAMBLE_03:
            if (0x03 == databyte) {
                buf[cnt++] = databyte;
                return (cnt);
            } else if (0x10 == databyte) {
                state = POSTAMBLE_10;
                continue;
            }
            continue;
        } // switch
    } // while
    return (-1);
}

            
int SetMotorPercent(int pcnt);

int SetMotorRPM(int rpm)
{
    int speed = 100 * 3450 / rpm;
    SetMotorPercent(speed);
    return (0);
}

int SetMotorPercent(int pcnt)
{
    int csc, len;

    if (0 == pcnt) {
        // cancel timer
        return (0);
    }
        
    // set timer for every 5 seconds

    unsigned char buf[] = { 0x10, 0x02, 0x0C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, 0x03 } ;
    buf[5] = pcnt & 0xFF;
    csc = 0x10 + 0x02 + 0x0C + 0x01 + buf[5];
    buf[6] = csc >> 8;
    buf[7] = csc & 0xFF;

    unsigned char pData[100];

    while(1) {
        write (fd, buf, sizeof buf);  
        printf("command sent: ");
        for (int i=0; i<10; i++)
            printf("%2.2X ", buf[i]);
        printf("\n\n");

        if ( (len = getPacket(fd, pData, sizeof pData)) > 0) {
            if (0x03 != pData[len-1] || 0x10 != pData[len-2])
                printf("partial packet received\n");

            csc = 0;
            for (int i=0; i<len; i++)
                if (i < (len-4)) csc+=pData[i];

//            if ( csc >> 8 != pData[len-4] || (csc & 0xFF) != pData[len-3]) {
//                printf("checksum does not match [%4.4X] [%2.2X%2.2X]\n", 
//					csc, pData[len-4], pData[len-3]);
                for (int i=0; i<len; i++)
                    printf("%2.2X ", pData[i]);
                printf("\n\n");
//            }

            int watts = pData[7]; watts = watts << 8 + pData[8];
            printf("motor at %d%% and using %x%2.2x watts\n", 
						pData[6], pData[7], pData[8]);
        }
        sleep(5);
    }
}

int main()
{
    OpenMotorPort();
    SetMotorPercent(75);
}
