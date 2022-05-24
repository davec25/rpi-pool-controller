#ifndef PUMP_CONTROLLER_HPP
#define PUMP_CONTROLLER_HPP

/*
 * Dave Cabot
 *
 */
#include "hayward_controller.hpp"

class PumpController : public HaywardController
{

private:
    int percent_speed;

//struct t_eventData { int myData; };

//int PumpController::InitTimerThread(void);

//static void *PumpThreadHelper(void *context) 
//	{ return ((PumpController *)context)->PumpThread(void); }


public:
    PumpController(const char *device="/dev/ttyS0", int baud=B19200) : 
					HaywardController(device, baud) {};

    int SetPumpPercent(int);
    int SetPumpRPMs(int);
    int GetPumpPercent();
    int GetPumpRPMs();

    int ProcessPacket(const unsigned char *, int);

};
#endif // PUMP_CONTROLLER_HPP
