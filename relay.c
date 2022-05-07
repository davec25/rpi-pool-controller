/*
 * relay.c:
 *	Command-line interface to the Raspberry
 *	Pi's 8-Relay Industrial board.
 *	Copyright (c) 2016-2021 Sequent Microsystem
 *	<http://www.sequentmicrosystem.com>
 ***********************************************************************
 *	Author: Alexandru Burcea
 ***********************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "relay.h"
#include "comm.h"
#include "thread.h"

#define VERSION_BASE	(int)1
#define VERSION_MAJOR	(int)1
#define VERSION_MINOR	(int)0

#define UNUSED(X) (void)X      /* To avoid gcc/g++ warnings */
#define CMD_ARRAY_SIZE	7

const u8 relayMaskRemap[8] =
{
	0x01,
	0x04,
	0x40,
	0x10,
	0x20,
	0x80,
	0x08,
	0x02};
const int relayChRemap[8] =
{
	0,
	2,
	6,
	4,
	5,
	7,
	3,
	1};

int relayChSet(int dev, u8 channel, OutStateEnumType state);
int relayChGet(int dev, u8 channel, OutStateEnumType* state);
u8 relayToIO(u8 relay);
u8 IOToRelay(u8 io);

u8 relayToIO(u8 relay)
{
	u8 i;
	u8 val = 0;
	for (i = 0; i < 8; i++)
	{
		if ( (relay & (1 << i)) != 0)
			val += relayMaskRemap[i];
	}
	return val;
}

u8 IOToRelay(u8 io)
{
	u8 i;
	u8 val = 0;
	for (i = 0; i < 8; i++)
	{
		if ( (io & relayMaskRemap[i]) != 0)
		{
			val += 1 << i;
		}
	}
	return val;
}

int relayChSet(int dev, u8 channel, OutStateEnumType state)
{
	int resp;
	u8 buff[2];

	if ( (channel < CHANNEL_NR_MIN) || (channel > RELAY_CH_NR_MAX))
	{
		printf("Invalid relay nr!\n");
		return ERROR;
	}
	if (FAIL == i2cMem8Read(dev, RELAY8_INPORT_REG_ADD, buff, 1))
	{
		return FAIL;
	}

	switch (state)
	{
	case OFF:
		buff[0] &= ~ (1 << relayChRemap[channel - 1]);
		resp = i2cMem8Write(dev, RELAY8_OUTPORT_REG_ADD, buff, 1);
		break;
	case ON:
		buff[0] |= 1 << relayChRemap[channel - 1];
		resp = i2cMem8Write(dev, RELAY8_OUTPORT_REG_ADD, buff, 1);
		break;
	default:
		printf("Invalid relay state!\n");
		return ERROR;
		break;
	}
	return resp;
}

int relayChGet(int dev, u8 channel, OutStateEnumType* state)
{
	u8 buff[2];

	if (NULL == state)
	{
		return ERROR;
	}

	if ( (channel < CHANNEL_NR_MIN) || (channel > RELAY_CH_NR_MAX))
	{
		printf("Invalid relay nr!\n");
		return ERROR;
	}

	if (FAIL == i2cMem8Read(dev, RELAY8_INPORT_REG_ADD, buff, 1))
	{
		return ERROR;
	}

	if (buff[0] & (1 << relayChRemap[channel - 1]))
	{
		*state = ON;
	}
	else
	{
		*state = OFF;
	}
	return OK;
}

int relaySet(int dev, int val)
{
	u8 buff[2];

	buff[0] = relayToIO(0xff & val);

	return i2cMem8Write(dev, RELAY8_OUTPORT_REG_ADD, buff, 1);
}

int relayGet(int dev, int* val)
{
	u8 buff[2];

	if (NULL == val)
	{
		return ERROR;
	}
	if (FAIL == i2cMem8Read(dev, RELAY8_INPORT_REG_ADD, buff, 1))
	{
		return ERROR;
	}
	*val = IOToRelay(buff[0]);
	return OK;
}

int doBoardInit(int stack)
{
	int dev = 0;
	int add = 0;
	uint8_t buff[8];

	if ( (stack < 0) || (stack > 7))
	{
		printf("Invalid stack level [0..7]!");
		return ERROR;
	}
	add = (stack + RELAY8_HW_I2C_BASE_ADD) ^ 0x07;
	dev = i2cSetup(add);
	if (dev == -1)
	{
		printf("i2cSetup error\n");
		return ERROR;

	}
	if (ERROR == i2cMem8Read(dev, RELAY8_CFG_REG_ADD, buff, 1))
	{
		add = (stack + RELAY8_HW_I2C_ALTERNATE_BASE_ADD) ^ 0x07;
		dev = i2cSetup(add);
		if (dev == -1)
		{
		        printf("i2cSetup 2 error\n");
			return ERROR;
		}
		if (ERROR == i2cMem8Read(dev, RELAY8_CFG_REG_ADD, buff, 1))
		{
			printf("8relind board id %d not detected\n", stack);
			return ERROR;
		}
	}
	if (buff[0] != 0) //non initialized I/O Expander
	{
		// make all I/O pins output
		buff[0] = 0;
		if (0 > i2cMem8Write(dev, RELAY8_CFG_REG_ADD, buff, 1))
		{
		        printf("i2cMem9Write error\n");
			return ERROR;
		}
		// put all pins in 0-logic state
		buff[0] = 0;
		if (0 > i2cMem8Write(dev, RELAY8_OUTPORT_REG_ADD, buff, 1))
		{
		        printf("i2cMem9Write 2 error\n");
			return ERROR;
		}
	}

	return dev;
}

int boardCheck(int hwAdd)
{
	int dev = 0;
	uint8_t buff[8];

	hwAdd ^= 0x07;
	dev = i2cSetup(hwAdd);
	if (dev == -1)
	{
		return FAIL;
	}
	if (ERROR == i2cMem8Read(dev, RELAY8_CFG_REG_ADD, buff, 1))
	{
		return ERROR;
	}
	return OK;
}

/*
 * doRelayWrite:
 *	Write coresponding relay channel
 **************************************************************************************
 */
int doRelayWrite(int dev, int pin, int state)
{
//	OutStateEnumType state = STATE_COUNT;
	int val = 0;
//	int dev = 1;
	OutStateEnumType stateR = STATE_COUNT;
	int valR = 0;
	int retry = 0;

	retry = RETRY_TIMES;

	while ( (retry > 0) && (stateR != state))
	{
		if (OK != relayChSet(dev, pin, state))
		{
			printf("Fail to write relay\n");
			return(-1);
		}
		if (OK != relayChGet(dev, pin, &stateR))
		{
			printf("Fail to read relay\n");
			return(-1);
		}
		retry--;
	}
	if (retry == 0)
	{
		printf("Fail to write relay\n");
		return(-1);
	}
	return(0);
}

/*
 * doRelayRead:
 *	Read relay state
 ******************************************************************************************
 */
int doRelayRead(int dev, int pin) 
{
	int val = 0;
//	int dev = 1;
	OutStateEnumType state = STATE_COUNT;

	if ( (pin < CHANNEL_NR_MIN) || (pin > RELAY_CH_NR_MAX))
	{
		printf("Relay number value out of range!\n");
		return(-1);
	}

	if (OK != relayChGet(dev, pin, &state))
	{
		printf("Fail to read!\n");
		return(-1);
	}
	if (state != 0)
	{
		return(1);
	}
	else
	{
		return(0);
	}
}

