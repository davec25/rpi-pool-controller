/*
 * Dave Cabot
 *
 */

#include <errno.h>
//#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <time.h>

#include "hayward_controller.hpp"
#include "timer_thread.hpp"

#define FALSE 0
#define TRUE  1

HaywardController::HaywardController(const char *device, int baud)
{
    commState = FALSE;
    tries = 3;
    int parity = 0;
    packet_len = 0;
    packet = NULL;

    if (SetupPort(device, baud, parity) < 0) {
        // exception
    }
}

int HaywardController::SetupPort(const char *device, int baud, int parity = 0)
{
//    std::cout << "SetupPort " << device << "\n";

    pump_fd = open (device, O_RDWR | O_NOCTTY );
//    pump_fd = open (device, O_RDWR | O_NOCTTY | O_SYNC);
    if (pump_fd < 0)
    {
        std::cerr << "error " << errno << " opening " << device << ": " 
		<< strerror (errno);
        return (-1);
    }

    struct termios tty;
    if (tcgetattr (pump_fd, &tty) != 0)
    {
        std::cerr << "error " << errno << " from tcgetattr\n";
        return (-1);
    }

#ifdef NOT
    cfsetospeed (&tty, baud);
    cfsetispeed (&tty, baud);

    cfmakeraw(&tty);
    tty.c_cflag     |= (CLOCAL | CREAD | CS8);
    tty.c_cflag |= CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

    tty.c_cflag     |= (CLOCAL | CREAD | CS8);
    tty.c_lflag     &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag     &= ~OPOST;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 5;

    tty.c_cflag = baud | CS8 | CLOCAL | CREAD;
    tty.c_iflag = IGNPAR ;
    tty.c_oflag = 0;
    tty.c_lflag = 0; // ICANON;
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout
#endif // NOT

    cfsetospeed (&tty, baud);
    cfsetispeed (&tty, baud);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tty.c_cflag |= (CLOCAL | CREAD );// ignore modem controls,
                                    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    if (parity)
       tty.c_cflag |= PARENB ;
    if (parity < 0)
       tty.c_cflag |= PARODD;

    tty.c_cflag |= CSTOPB;
    tty.c_cflag &= ~CRTSCTS;


    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
//    tty.c_iflag |= IGNPAR ;

    tty.c_lflag = 0;                // no signaling chars, no echo,
                                    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 10;            // 5 seconds read timeout


    if (tcsetattr (pump_fd, TCSANOW, &tty) != 0)
    {
        std::cerr << "error " << errno << " from tcsetattr";
        return (-1);
    }
//    tcflush(pump_fd, TCIFLUSH);

    return(0);
}

int HaywardController::GetPacket(unsigned char buf[], int max)
{
//    std::cout << "GetPacket\n";

    enum { PREAMBLE_10, PREAMBLE_02, POSTAMBLE_10, POSTAMBLE_03 } state;
    int cnt = 0;
    unsigned char databyte;

    state = PREAMBLE_10;
    while(max > cnt) {
        int len = read (pump_fd, &databyte, 1);  
        if (len<1) {
            std::cerr << Name() << " read failed " << strerror(errno) << 
			", cnt:" << cnt << ", max:" << max << "\n";
            return(-1);
        }

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
                return (cnt); // successful return is here
            } else if (0x10 == databyte) {
                state = POSTAMBLE_10;
                continue;
            }
            continue;
        } // switch
    } // while
    return (-1);
}

            
int HaywardController::PacketPrechecks(const unsigned char *data, int len)
{
//    std::cout << "PacketPrechecks\n";
    if (len < 7) {
        std::cerr << "PacketPrechecks: packet too small\n";
        return (-1);
    }

    if (data[0] != 0x10 || data[1] != 0x02) {
        std::cerr << "PacketPrechecks: preamble wrong\n";
        return (-1);
    }

    if (data[len-2] != 0x10 || data[len-1] != 0x03) {
        std::cerr << "PacketPrechecks: postamble wrong\n";
        return (-1);
    }

    return(0);
}

int HaywardController::GenerateCSC(unsigned char *data, int len)
{
//    std::cout << "GenerateCSC\n";
    if (PacketPrechecks(data, len)) return (-1);

    int pos = len - 4;
    int csc = 0;
    do
        csc += data[pos];
    while (pos--);

    data[len-4] = csc >> 8;
    data[len-3] = csc & 0xFF;

    return(0);
}

int HaywardController::SendPacket(const unsigned char *data, int size, int interval_secs)
{
//    std::cout << "SendPacket: timer_secs = " << interval_secs << "\n";

    if (PacketPrechecks(data, size)) {
        std::cerr << "SendPacket: precheck failed\n";
        return (-1);
    }

    // mutex lock
    if (packet) 
        delete packet;

    packet = new unsigned char(size);
    memcpy((void *)packet, data, size);
    packet_len = size;
    // mutex free

    GenerateCSC((unsigned char*)packet, packet_len);

    ThreadProcess();
    InitTimerThread(interval_secs);
  
    return(0);
}

void HaywardController::ThreadProcess(void)
{
//    std::cout << "ThreadProcess\n";
    int csc, len;
    unsigned char pData[30];

    // mutex lock
    int ret = write (pump_fd, packet, packet_len);
    // mutex free

    if (ret < packet_len) {
        std::cerr << "ThreadProcess: write failed: " << strerror(errno) << "\n";
        return;
    }

#ifdef NOT
    std::cout << Name() << " command sent: ";
    for (int i=0; i<packet_len; i++)
        printf("%2.2X ", packet[i]);
    std::cout << "\n\n";
#endif // NOT

    if ( (len = GetPacket(pData, sizeof pData)) == -1) {
        std::cerr << Name() << " GetPacket returned fail\n";
        if (tries-- < 0) {
            std::cerr << Name() << " ThreadProcess: going to fault state\n";
            commState = FALSE;
        }
        return;
    }

//#ifdef NOT
    std::cout << Name() << " packet received: ";
    for (int i=0; i<len; i++)
        printf("%2.2X ", pData[i]);
    std::cout << "\n\n";
//#endif // NOT

    csc = 0;

    for (int i=0; i<len; i++)
        if (i < (len-4)) csc+=pData[i];

#ifdef NOT
    if ( csc >> 8 != pData[len-4] || (csc & 0xFF) != pData[len-3]) {
        printf("checksum does not match [%4.4X] [%2.2X%2.2X]\n", 
				csc, pData[len-4], pData[len-3]);
        for (int i=0; i<len; i++)
            printf("%2.2X ", pData[i]);
        std::cout << "\n\n";
    }
#endif // NOT

    if (tries != 3) {
        std::cerr << Name() << " ThreadProcess: connection restored\n";
    }

    commState = TRUE;
    tries = 3;
    if (ProcessPacket((const unsigned char *)pData, len)) {
        std::cerr << Name() << ": subclass unhappy with packet\n";
    }
//     std::cout << "packet processed\n";
    return;
}
