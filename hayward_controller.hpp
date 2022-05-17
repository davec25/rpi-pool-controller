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

struct t_eventData { int myData; };

int InitTimerThread(void);
int SetupPort(const char *device, int baud, int parity);
//static void *TimerThreadHelper(void *context) 
//	{ return ((HaywardController *)context)->TimerThread(); }
int getPacket(unsigned char buf[], int max);


public:
void TimerThread(void); 
HaywardController(const char *device, int baud);
//~HaywardController();

bool CommState();

};
#endif // HAYWARD_CONTROLLER_HPP
