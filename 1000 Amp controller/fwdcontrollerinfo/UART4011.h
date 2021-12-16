#ifndef UART4011_H
#define UART4011_H
#include "p30F4011.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "DCController.h"


#define MAX_COMMAND_LENGTH 64

typedef struct UARTCommand_typ {
	int i;
	int complete;
	char string[100];
} UARTCommand;

void InitUART2();
int TransmitReady();
int TransmitReadyAlt();
void SendCharacter(char ch);
int ReceiveBufferHasData();
unsigned char GetCharacter();
void ClearReceiveBuffer();
void __attribute__((__interrupt__, auto_psv)) _U2RXInterrupt(void);

#endif
