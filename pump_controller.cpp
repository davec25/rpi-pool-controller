/*
 * Dave Cabot
 *
 */

#include <termios.h>
#include <unistd.h>

#include <pump_controller.hpp>


            
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


int PumpController::ProcessPacket(const unsigned char *pData, int len)
{
    if (len != 13) {
        std::cerr << "ProcessPacket: packet size unexpected, " << len << "\n";
        return(-1);
    }

    int watts = pData[7]; watts = watts << 8 + pData[8];
    printf("motor at %d%% and using %x%2.2x watts\n",
                                            pData[6], pData[7], pData[8]);
    return(0);
}


#ifdef PUMP_DEBUG
int main(int argc, char **argv)
{
//declare
    class PumpController pc;

    const unsigned char buf[] = { 0x10, 0x02, 0x0C, 0x01, 0x00, 0x99, 0x00, 0x00, 0x10, 0x03 } ;

    if (pc.SendPacket(buf, 10, 60)) {
        std::cerr << "SendPacket failed\n";
        exit(-1);
    }

    while(1) {
        sleep(60);
        std::cout << "motor speed: " << pc.GetPumpRPMs();
    }
}
#endif // PUMP_DEBUG

