/*
 * Dave Cabot
 *
 */

#include <termios.h>
#include <unistd.h>

#include "chgen_controller.hpp"

            
int ChGenController::ProcessPacket(const unsigned char *pData, int len)
{
    switch (len) {
       case 9:
           printf("PPM: %d Status: %2.2x\n", pData[4]*50, pData[5]);
           break;

       case 24:
           char model[17];
           memcpy(model, &pData[5], 16);
           model[16] = 0;
           printf("model: %s\n", model);
           break;

       default:
        std::cerr << "ProcessPacket: packet size unexpected, " << len << "\n";
        return(-1);
    }

    return(0);
}


#ifdef CHGEN_DEBUG
int main(int argc, char **argv)
{
//declare
    class ChGenController gc;
// set chlorine to 0%
//    const unsigned char buf[] = { 0x10, 0x02, 0x50, 0x11, 0x0, 0x00, 0x00, 0x10, 0x03 } ;
// set chlorine to 20%
//    const unsigned char buf[] = { 0x10, 0x02, 0x50, 0x11, 0x14, 0x00, 0x00, 0x10, 0x03 } ;
// get model
    const unsigned char buf[] = { 0x10, 0x02, 0x50, 0x14, 0x00, 0x00, 0x10, 0x03 } ;
// disable external
//    const unsigned char buf[] = { 0x10, 0x02, 0x50, 0x00, 0x00, 0x00, 0x10, 0x03 } ;

    if (gc.SendPacket(buf, sizeof buf, 5)) {
        std::cerr << "SendPacket failed\n";
        exit(-1);
    }

//    std::cout << "class initiated\n";

    while(1) {
        sleep(60);
        std::cout << "Gen: " << gc.GetGenPercent() << "%\n" ;
    }
}
#endif // CHGEN_DEBUG

