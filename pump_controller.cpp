/*
 * Dave Cabot
 *
 */

#include "pump_controller.hpp"

PumpController::PumpController(const char *device, int baud) :
                                        	HaywardController(device, baud)
{
    const unsigned char buf[] = { 0x10, 0x02, 0x0C, 0x01, 0x00, 0x99, 0x00, 0x00, 0x10, 0x03 } ;

    if (SendPacket(buf, sizeof buf, 5)) {
        std::cerr << "SendPacket failed\n";
        exit(-1);
    }
}
            
int PumpController::SetPumpRPMs(int rpm)
{
    int percent = 100 * 3450 / rpm;
    SetPumpPercent(percent);
    return (0);
}

int PumpController::GetPumpRPMs()
{
    int rpm = 3450 * percent_speed / 100 ;
    return (rpm);
}

int PumpController::SetPumpPercent(int percent)
{
    if (percent < 0 || percent > 100) {
        return (-1);
    }

    unsigned char buf[] = { 0x10, 0x02, 0x0C, 0x01, 0x00, 0x99, 0x00, 0x00, 0x10, 0x03 } ;
    percent_speed = percent;
    buf[5] = percent & 0xFF;

    if (SendPacket(buf, sizeof buf, 5)) {
        std::cerr << "SendPacket failed\n";
        return(-1);
    }

    return(0);
}

int PumpController::GetPumpPercent()
{
    return(percent_speed);
}


int PumpController::GetPumpWatts()
{
    return (watts);
}

int PumpController::ProcessPacket(const unsigned char *pData, int len)
{
    if (len != 13) {
        std::cerr << "ProcessPacket: packet size unexpected, " << len << "\n";
        return(-1);
    }

    watts  = (pData[7] >> 4) * 1000;
    watts += (pData[7] & 0xF) * 100;
    watts += (pData[8] >> 4) * 10;
    watts +=  pData[8] & 0xF;

    percent_speed = pData[6];
//    printf("motor at %d%% (%d rpms) and using %x%2.2x  %d watts\n",
//                   GetPumpPercent(), GetPumpRPMs(), pData[7], pData[8], watts);
    return(0);
}


#ifdef PUMP_DEBUG
int main(int argc, char **argv)
{
//declare
    class PumpController pc;

//        buf[5] = percent_speed & 0xFF;

    const unsigned char buf[] = { 0x10, 0x02, 0x0C, 0x01, 0x00, 0x99, 0x00, 0x00, 0x10, 0x03 } ;
//    const unsigned char buf[] = { 0x10, 0x02, 0x0C, 0x01, 0x14, 0x00, 0x00, 0x10, 0x03 } ;

    if (pc.SendPacket(buf, sizeof buf, 5)) {
        std::cerr << "SendPacket failed\n";
        exit(-1);
    }

    std::cout << "class initiated\n";

    while(1) {
        sleep(60);
        std::cout << "motor speed: " << pc.GetPumpRPMs();
    }
}
#endif // PUMP_DEBUG

