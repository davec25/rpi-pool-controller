#ifndef HAYWARD_CONTROLLER_HPP
#define HAYWARD_CONTROLLER_HPP

/*
 * Dave Cabot
 *
 */
#include <iostream>
#include <pthread.h>
#include <string.h>
#include <time.h>

extern "C" void TimerThreadHelper(union sigval timer_data);

class HaywardController
{

private:
    pthread_t pid_t;
    bool commState;
    int tries;
    int pump_fd;
    const unsigned char *packet;
    int len;
    int timer_secs;

    int InitTimerThread(void);
    int PacketPrechecks(const unsigned char *data, int size);
    int SetupPort(const char *device, int baud, int parity);
    int GetPacket(unsigned char buf[], int max);


public:
    void TimerThread(void); 
    HaywardController(const char *device, int baud);

    int SendPacket(const unsigned char *packet, int len, int interval_secs);
    int GenerateCSC(unsigned char *packet, int len);
    virtual int ProcessPacket(const unsigned char *packet, int len) = 0;

    bool CommState();
};
#endif // HAYWARD_CONTROLLER_HPP
