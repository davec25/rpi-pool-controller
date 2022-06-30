#ifndef HAYWARD_CONTROLLER_HPP
#define HAYWARD_CONTROLLER_HPP

/*
 * Dave Cabot
 *
 */
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <string.h>
#include <time.h>

#include "timer_thread.hpp"


class HaywardController : public TimerThread
{

private:
    bool commState;
    int tries;
    int pump_fd;
    const unsigned char *packet;
    int packet_len;

    int PacketPrechecks(const unsigned char *data, int size);
    int SetupPort(const char *device, int baud, int parity);
    int GetPacket(unsigned char buf[], int max);

public:
    HaywardController(const char *device, int baud);

    void ThreadProcess(void);
    int SendPacket(const unsigned char *packet, int len, int interval_secs);
    int GenerateCSC(unsigned char *packet, int len);
    virtual int ProcessPacket(const unsigned char *packet, int len) = 0;
    virtual const char *Name(void) = 0;

    bool CommState() {return commState;};
};

#endif // HAYWARD_CONTROLLER_HPP
