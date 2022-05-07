#include "DEV_Config.h"

int main(int argc, char **argv)
{

    class PEAR *ser1 = new class PEAR(2, 19200);

    unsigned char buf100percent[] = { 0x10, 0x02, 0x0C, 0x01, 0x00, 0x64, 0x00, 0x83, 0x10, 0x03 };
    unsigned char pData[100];
    int len;

    ser1->write(buf100percent, sizeof(buf100percent));
    printf("command sent\n");
    len = ser1->read(pData, 20);
    for (int i=0; i<len; i++)
        printf("%2.2x ", pData[i] & 0xFF);
    printf("\n\n");
}
