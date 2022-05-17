/*
 * Dave Cabot
 *
 */

#include <unistd.h>
//#include <time.h>
#include <termios.h>
#include <pump_controller.hpp>

#define FALSE 0
#define TRUE  1

#ifdef NOT
PumpController::PumpController(const char *device="/dev/ttyAMA0", int baud=B19200)
{
    percent_speed = -1; // motor speed, not comm port speed
    commState = FALSE;
    tries = 3;
    int parity = 0;

    if (SetupPort(device, baud, parity) < 0) {
        // exception
    }

// REVERT timer?
    pthread_create(&pid_t, NULL, &PumpController::PumpThreadHelper, (void*)this);

}

int PumpController::InitTimerThread(void)
{
    int res = 0;
    timer_t timerId = 0;

    struct t_eventData eventData = { .myData = 0 };

    /*  sigevent specifies behaviour on expiration  */
    struct sigevent sev = { 0 };

    /* specify start delay and interval
     * it_value and it_interval must not be zero */

    struct itimerspec its = {   .it_value.tv_sec  = 30,
                                .it_value.tv_nsec = 0,
                                .it_interval.tv_sec  = 30,
                                .it_interval.tv_nsec = 0
                            };
    std::cout << "Simple Threading Timer - thread-id: " << pthread_self();

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = &poll_sensors;
    sev.sigev_value.sival_ptr = &eventData;

    /* create timer */
    res = timer_create(CLOCK_REALTIME, &sev, &timerId);

    if (res != 0){
        std::cerr << "Error timer_create: " << strerror(errno);
        return(-1);
    }

    /* start timer */
    res = timer_settime(timerId, 0, &its, NULL);
    if (res != 0){
        std::cerr << "Error timer_settime: " << strerror(errno);
        return(-1);
    }
    return(0);
}


int PumpController::SetupPort(const char *device, int baud, int parity = 0)
{
    pump_fd = open (device, O_RDWR | O_NOCTTY | O_SYNC);
    if (pump_fd < 0)
    {
        std::cerr << "error " << errno << " opening " << device << ": " 
		<< strerror (errno);
        return (-1);
    }

    struct termios tty;
    if (tcgetattr (pump_fd, &tty) != 0)
    {
        std::cerr << "error " << errno << " from tcgetattr";
        return (-1);
    }

    cfsetospeed (&tty, baud);
    cfsetispeed (&tty, baud);

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
    if (parity != 0)
       tty.c_cflag |= PARENB ;
    if (parity < 0)
       tty.c_cflag |= PARODD;
    //tty.c_cflag &= ~CSTOPB;
    tty.c_cflag |= CSTOPB;
    tty.c_cflag &= ~CRTSCTS;

    if (tcsetattr (pump_fd, TCSANOW, &tty) != 0)
    {
        std::cerr << "error " << errno << " from tcsetattr";
        return (-1);
    }

    return(0);
}

int PumpController::getPacket(unsigned char buf[], int max)
{
    enum { PREAMBLE_10, PREAMBLE_02, POSTAMBLE_10, POSTAMBLE_03 } state;
    int cnt = 0;
    unsigned char databyte;

    state = PREAMBLE_10;
    while(max > cnt) {
        int len = read (pump_fd, &databyte, 1);  
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

#endif // NOT
            
int PumpController::SetPumpRPMs(int rpm)
{
    int speed = 100 * 3450 / rpm;
    SetPumpPercent(speed);
    return (0);
}

int PumpController::GetPumpRPMs()
{
    int rpm = 3450 * percent_speed / 100 ;
    return (rpm);
}

int PumpController::SetPumpPercent(int pcnt)
{
    if (pcnt > -1 && pcnt < 101) {
        percent_speed = pcnt;
        return (0);
    }

    return(-1);
}

int PumpController::GetPumpPercent()
{
    return(percent_speed);
}

#ifdef NOT
void PumpController::PumpThread(void)
{
    int csc, len;

    unsigned char buf[] = { 0x10, 0x02, 0x0C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x10, 0x03 } ;
    if (0 == percent_speed)
        buf[5] = 0x99 & 0xFF;
    else
        buf[5] = percent_speed & 0xFF;
    csc = 0x10 + 0x02 + 0x0C + 0x01 + buf[5];
    buf[6] = csc >> 8;
    buf[7] = csc & 0xFF;

    unsigned char pData[100];

    write (pump_fd, buf, sizeof buf);  
    std::cout << "command sent: ";
    for (int i=0; i<10; i++)
        printf("%2.2X ", buf[i]);
    std::cout << "\n\n";

    if ( (len = getPacket(pData, sizeof pData)) > 0) {
        if (0x03 != pData[len-1] || 0x10 != pData[len-2]) {
            std::cerr << "partial packet received\n";
            if (tries-- < 0) {
                std::cerr << "going to fault state\n";
                commState = FALSE;
                percent_speed = -1;
                // change timer to 1 minute
                return;
            }
        }

        csc = 0;
        for (int i=0; i<len; i++)
            if (i < (len-4)) csc+=pData[i];

#ifdef PUMP_DEBUG
        if ( csc >> 8 != pData[len-4] || (csc & 0xFF) != pData[len-3]) {
            printf("checksum does not match [%4.4X] [%2.2X%2.2X]\n", 
				csc, pData[len-4], pData[len-3]);
#endif
            for (int i=0; i<len; i++)
                printf("%2.2X ", pData[i]);
            std::cout << "\n\n";
#ifdef PUMP_DEBUG
        }
#endif

        commState = TRUE;
        tries = 3;
        int watts = pData[7]; watts = watts << 8 + pData[8];
        printf("motor at %d%% and using %x%2.2x watts\n", 
						pData[6], pData[7], pData[8]);
    }
}

#endif // NOT

#ifdef PUMP_DEBUG
int main(int argc, char **argv)
{
//declare
    class PumpController pc;


    while(1) {
        sleep(60);
        std::cout << "motor speed: " << pc.GetPumpRPMs();
    }
}
#endif // PUMP_DEBUG

