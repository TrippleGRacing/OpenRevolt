#include "UART4011.h"

void ShowMenu(void);
void ShowConfig(unsigned int mask);
void u16x_to_str(char *str, unsigned val, unsigned char digits);
void u16_to_str(char *str, unsigned val, unsigned char digits);
int TransmitString(char* str);
extern void InitPIStruct(void);
extern void NormalizeAllConfigurationCurrentsTo_0_512(void);
extern void EESaveValues(void);
UARTCommand myUARTCommand;
char commandText[32];
char tempStr[80];

unsigned int timeOfLastDatastreamTransmission = 0;
unsigned int commandNumber = 0;
unsigned int commandType = 0;	// 0 means no number argument.
extern unsigned int faultBits;
extern SavedValuesStruct savedValues;
extern unsigned long int motorOverspeedThresholdTimes1024;
extern unsigned int counter1ms;
extern unsigned int showDatastreamJustOnce;

char command0[] = "config";
char command1[] = "save";
char command2[] = "idle";
char command3[] = "restart";
char command4[] = "reset-ah";
char command5[] = "kp";
char command6[] = "ki";
char command7[] = "t-min-rc";
char command8[] = "t-max-rc";
char command9[] = "t-fault-rc";
char command10[] = "t-pos-gain";
char command11[] = "t-pwm-gain";
char command12[] = "c-rr";
char command13[] = "rtd-period";
char command14[] = "rtd";
char command15[] = "motor-os-th";
char command16[] = "motor-os-ft";
char command17[] = "motor-os-dt";
char command18[] = "pwm-deadzone";
char command19[] = "motor-sc-amps";
char command20[] = "bat-amps-lim";
char command21[] = "pc-time";
char command22[] = "mot-amps-lim";

char savedEEString[] = "configuration written to EE\r\n";
char menuString[] = "The Stinky Diaper firmware, ver. 1.0\r\n";

char showConfigKpKiString[] = "Kp=xxx Ki=xxx\r\n";
char showConfigThrottleRangeString[] = "throttle_min_raw_counts=xxxx throttle_max_raw_counts=xxxx\r\n";
char showConfigThrottleFaultString[] = "throttle_fault_raw_counts=xxxx\r\n";
char showConfigPosPwmGainString[] = "throttle_pos_gain=xxx throttle_pwm_gain=xxx\r\n";
char showConfigRampRateString[] = "current_ramp_rate=xxx\r\n";
char showConfigRTDPeriodString[] = "rtd_period=xxxxx\r\n";
//char showConfigPwmFilterString[] = "pwm_filter=x\r\n";
char showConfigMotorFaultTimeString[] = "motor_os_threshold=xxxx motor_os_ftime=xxxx\r\n";
char showConfigMotorDeadTimeString[] = "motor_os_dtime=xx pwm_deadzone=xx\r\n";
char showConfigMinAmpsForMotorOverspeedString[] = "motor_speed_calc_amps=xxx\r\n";
char showConfigMaxBatteryAmpsString[] = "battery_amps_limit=xxxx\r\n";
char showConfigPrechargeString[] = "precharge_time=xxx\r\n";
char showConfigMaxMotorAmpsString[] = "mot_amps_lim=xxxx\r\n";

void InitUART2() {
	// assuming 7.37MHz crystal.
	U2BRG = 47; // Pg. 506 on F.R.M.  Baud rate is 19.2kbps
	U2MODE = 0;  // initialize to 0.
	U2MODEbits.PDSEL = 0b00; // 8 N 
	U2MODEbits.STSEL = 0; // 1 stop bit.

	IEC1bits.U2RXIE = 1;  // enable receive interrupts.
	IPC6bits.U2RXIP = 2;	// INTERRUPT priority of 2.
//bit 7-6 URXISEL<1:0>: Receive Interrupt Mode Selection bit
//11 =Interrupt flag bit is set when Receive Buffer is full (i.e., has 4 data characters)
//10 =Interrupt flag bit is set when Receive Buffer is 3/4 full (i.e., has 3 data characters)
//0x =Interrupt flag bit is set when a character is received
	U2STAbits.URXISEL = 0b00;  // 0b11 later..

	U2MODEbits.UARTEN = 1; // enable the uart
	asm("nop");
	U2STAbits.UTXEN = 1; // Enable transmissions
}

int TransmitReady() {
	if (U2STAbits.UTXBF == 1) // Pg. 502 in F.R.M.  Is transmit buffer full?
		return 0;
	return 1; 
}

void SendCharacter(char ch) {
	// Make sure to run TransmitReady() before this.
	U2TXREG = ch;
}
int ReceiveBufferHasData() {
	return U2STAbits.URXDA;  // returns 1 if true.  0 otherwise.
}

unsigned char GetCharacter() {
	return (unsigned char)U2RXREG;
}

void ClearReceiveBuffer() {
	U2STAbits.OERR = 0;  // clear the error.
}

void __attribute__((__interrupt__, auto_psv)) _U2RXInterrupt(void) {
	static unsigned char temp = 0;

	IFS1bits.U2RXIF = 0;  // clear the interrupt.
	temp = U2RXREG;		// get the character that caused the interrupt.
	Nop();
	Nop();
	Nop();
	Nop();
	Nop();


	if (myUARTCommand.complete == 1) {	// just ignore everything until the command is processed.
		return;
	}
	if (temp == 0x0d) {	
		myUARTCommand.complete = 1;
		myUARTCommand.string[myUARTCommand.i] = 0;

		Nop();
		Nop();
		Nop();
		Nop();

		return;
	}
	myUARTCommand.string[myUARTCommand.i] = temp; // save the character that caused the interrupt!
	myUARTCommand.i++;
	if (myUARTCommand.i >= MAX_COMMAND_LENGTH) {
		myUARTCommand.complete = 0;  // It can't make it here unless myUARTCommand.complete == 0 anyway.
		myUARTCommand.i = 0;	// just clear the array, and start over.
		myUARTCommand.string[0] = 0;
		return;
	}
}

// process the command, and reset UARTCommandPtr back to zero.
// myUARTCommand is of the form XXXXXXXXX YYYYY CR
void ProcessCommand(void) {
	static int i = 0;
	static int w = 35;
	if (myUARTCommand.complete == 0) {
		return;
	}
	commandNumber = 0;	// set number argument to zero.
	for (i = 0; myUARTCommand.string[i] != 0; i++) {
		if (myUARTCommand.string[i] == ' ') {
			commandText[i] = 0;  // null terminate the text portion.
			commandNumber = atoi(&myUARTCommand.string[i+1]);
			break;
		}
		commandText[i] = myUARTCommand.string[i];

	}
	commandText[i] = 0;  // NULL TERMINATE IT!!!

	if (myUARTCommand.i == 0) {	// The string was a carriage return.
		w = TransmitString(&menuString[0]);
		Nop();
		Nop();
		Nop();
		Nop();
		Nop();
	}
	else if (!strcmp(commandText, command0)) { // "config"
		ShowConfig(0x0FFFF);
	}
	else if (!strcmp(&commandText[0], command1)){ // "save"
//		write_config();
		TransmitString(savedEEString);  // 	"configuration written to EE"	
		EESaveValues();
	}
	else if (!strcmp(&commandText[0], command2)){ // "idle".  Probably won't use it.
//		strcpy_P(uart_str, PSTR("AVR xxx% idle\r\n"));
//		u16_to_str(&uart_str[4],
//			(unsigned)(wait_time(100) * (unsigned long)100 / idle_loopcount),
//			3);		
//		TransmitString();
	}
	else if (!strcmp(&commandText[0], command3)){ // "restart"
//		watchdog_enable();
		while(1);	
	}
	else if (!strcmp(&commandText[0], command4)){ // "reset-ah"
//		cli(); battery_ah = 0; sei();
//		strcpy_P(uart_str, PSTR("battery amp hours reset\r\n"));
//		TransmitString();
	}
	else if (!strcmp(&commandText[0], command5)){ // "kp"
		if (commandNumber <= 500) {
			savedValues.Kp = commandNumber; InitPIStruct();
			ShowConfig((unsigned)1 << 0);
		}
	}
	else if (!strcmp(&commandText[0], command6)){ // "ki"
		if ((unsigned)commandNumber <= 500) {
			savedValues.Ki = commandNumber; InitPIStruct();
			ShowConfig((unsigned)1 << 0);
		}
	}
	else if (!strcmp(&commandText[0], command7)){ // "t-min-rc"
		if ((unsigned)commandNumber <= 1023) {
			savedValues.throttleLowVoltage = commandNumber; 
			ShowConfig((unsigned)1 << 1);
		}
	}
	else if (!strcmp(&commandText[0], command8)){ // "t-max-rc"
		if ((unsigned)commandNumber <= 1023) {
			savedValues.throttleHighVoltage = commandNumber;
			ShowConfig((unsigned)1 << 1);
		}
	}
	else if (!strcmp(&commandText[0], command9)){ // "t-fault-rc"
		if ((unsigned)commandNumber <= 1023) {
			savedValues.throttleFaultVoltage = commandNumber;
			ShowConfig((unsigned)1 << 2);
		}
	}
	else if (!strcmp(&commandText[0], command10)){ // "t-pos-gain"
		if ((unsigned)commandNumber <= 128) {
			savedValues.throttlePositionGain = commandNumber;
			ShowConfig((unsigned)1 << 3);
		}
	}
	else if (!strcmp(&commandText[0], command11)){ // "t-pwm-gain"
		if ((unsigned)commandNumber <= 128) {
			savedValues.throttlePWMGain = commandNumber;
			ShowConfig((unsigned)1 << 3);
		}
	}
	// Really, 8 - c-rr = time to max current.
	else if (!strcmp(&commandText[0], command12)){ // "c-rr"
		if ((unsigned)commandNumber <= 20000 && (unsigned)commandNumber >= 1) {
			savedValues.currentRampRate = commandNumber;
			ShowConfig((unsigned)1 << 4);
		}
	}
	else if (!strcmp(&commandText[0], command13)){ // "rtd-period"
		if ((unsigned)commandNumber <= 32000) {
			savedValues.datastreamPeriod = commandNumber;
			showDatastreamJustOnce = 0;
			timeOfLastDatastreamTransmission = counter1ms;	
			ShowConfig((unsigned)1 << 5);
		}
	}
	else if (!strcmp(&commandText[0], command14)){ // "rtd"
		savedValues.datastreamPeriod = 1;
		showDatastreamJustOnce = 1;
		ShowConfig((unsigned)1 << 6);
	}
	else if (!strcmp(&commandText[0], command15)){ // "motor-os-th"
	// warning!! also initialize motorOverspeedThresholdTimes1024 when the eeprom gets read.
		if ((unsigned)commandNumber <= 9999) {
			savedValues.motorOverspeedThreshold = commandNumber;
			motorOverspeedThresholdTimes1024 = ((unsigned long)savedValues.motorOverspeedThreshold) << 10;
			ShowConfig((unsigned)1 << 7);
		}
	}
	else if (!strcmp(&commandText[0], command16)){ // "motor-os-ft"
		if ((unsigned)commandNumber <= 9999) {
			savedValues.motorOverspeedOffTime = commandNumber;
			ShowConfig((unsigned)1 << 7);
		}
	}
//	else if (!strcmp(&commandText[0], command17)){ // "motor-os-dt"
//		if ((unsigned)commandNumber <= 99) {
//			savedValues.motorOverspeedDebounceTime = commandNumber;
//			ShowConfig((unsigned)1 << 8);
//		}
//	}
//	else if (!strcmp(&commandText[0], command18)){ // "pwm-deadzone"
//		if ((unsigned)commandNumber <= 99) {
//			savedValues.pwmDeadzone = commandNumber;
//			ShowConfig((unsigned)1 << 8);
//		}
//	}
	else if (!strcmp(&commandText[0], command19)){ // "motor-sc-amps"
		if ((unsigned)commandNumber <= savedValues.maxMotorAmperes) {
			savedValues.minAmperesForOverspeed = commandNumber;
			NormalizeAllConfigurationCurrentsTo_0_512();			
			ShowConfig((unsigned)1 << 9);
		}
	}
	else if (!strcmp(&commandText[0], command20)){ // "bat-amps-lim"
		if ((unsigned)commandNumber <= MAX_AMPERES) {  // <= 1800.
			savedValues.maxBatteryAmperes = commandNumber;
			NormalizeAllConfigurationCurrentsTo_0_512();
			ShowConfig((unsigned)1 << 10);
		}
	}
	else if (!strcmp(&commandText[0], command21)){ // "pc-time"
		if ((unsigned)commandNumber <= 999) {
			savedValues.prechargeTime = commandNumber;
			ShowConfig((unsigned)1 << 11);
		}
	}
	else if (!strcmp(&commandText[0], command22)){ // "mot-amps-lim" 
		if ((unsigned)commandNumber <= MAX_AMPERES && (unsigned)commandNumber >= 1) {
			savedValues.maxMotorAmperes = commandNumber;
			NormalizeAllConfigurationCurrentsTo_0_512();
			ShowConfig((unsigned)1 << 10);
		}
	}
	myUARTCommand.string[0] = 0; 	// clear the string.
	myUARTCommand.i = 0;
	myUARTCommand.complete = 0;  // You processed that command.  Dump it!  Do this last.  The ISR will do nothing as long as the command is complete.
//	commandText[0] = 0;
	
}

int TransmitString(char* str) {  // For echoing onto the display
	static unsigned int i = 0;
	static unsigned int now = 0;
	
	now = TMR5;	// timer 4 runs at 62.5KHz.  Timer5 is the high word of the 32 bit timer.  So, it updates at around 1Hz.
	while (1) {
		if (str[i] == 0) {
			i = 0;
			break;
		}
		if (U2STAbits.UTXBF == 0) { // TransmitReady();
			U2TXREG = str[i]; 	// SendCharacter(str[i]);
			i++;
		}
//		if (TMR5 - now > 3) { 	// 2-3 seconds
//			faultBits |= UART_FAULT;
//			return 0;
//		}
		//		ClrWdt();
	}
	return 1;  
}

void ShowMenu(void)
{
	TransmitString(menuString);
}

// convert val to string (inside body of string) with specified number of digits
// do NOT terminate string
void u16_to_str(char *str, unsigned val, unsigned char digits)
{
	str = str + (digits - 1); // go from right to left.
	while (digits > 0) { // 
		*str = (unsigned char)(val % 10) + '0';
		val = val / 10;
		str--;
		digits--;
	}
}

// convert val to hex string (inside body of string) with specified number of digits
// do NOT terminate string
void u16x_to_str(char *str, unsigned val, unsigned char digits)
{
	unsigned char nibble;
	
	str = str + (digits - 1);
	while (digits > 0) {
		nibble = val & 0x000f;
		if (nibble >= 10) nibble = (nibble - 10) + 'A';
		else nibble = nibble + '0';
		*str = nibble;
		val = val >> 4;
		str--;
		digits--;
	}
}

void ShowConfig(unsigned int mask) {
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// Kp=xxx Ki=xxx
	if (mask & ((unsigned)1 << 0)) {
		u16_to_str(&showConfigKpKiString[3], savedValues.Kp, 3);	
		u16_to_str(&showConfigKpKiString[10], savedValues.Ki, 3);
		TransmitString(showConfigKpKiString);
	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// throttle_min_raw_counts=xxxx throttle_max_raw_counts=xxxx
	if (mask & ((unsigned)1 << 1)) {
		u16_to_str(&showConfigThrottleRangeString[24], savedValues.throttleLowVoltage, 4);
		u16_to_str(&showConfigThrottleRangeString[53], savedValues.throttleHighVoltage, 4);
		TransmitString(showConfigThrottleRangeString);
	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// throttle_fault_raw_counts=xxxx
	if (mask & ((unsigned)1 << 2)) {
		u16_to_str(&showConfigThrottleFaultString[26], savedValues.throttleFaultVoltage, 4);
		TransmitString(showConfigThrottleFaultString);
	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// throttle_pos_gain=xxx throttle_pwm_gain=xxx
	if (mask & ((unsigned)1 << 3)) {
		u16_to_str(&showConfigPosPwmGainString[18], savedValues.throttlePositionGain, 3);
		u16_to_str(&showConfigPosPwmGainString[40], savedValues.throttlePWMGain, 3);
		TransmitString(showConfigPosPwmGainString);
	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// current_ramp_rate=xxx
	if (mask & ((unsigned)1 << 4)) {
		u16_to_str(&showConfigRampRateString[18], savedValues.currentRampRate, 3);
		TransmitString(showConfigRampRateString);
	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// rtd_period=xxxxx
	if (mask & ((unsigned)1 << 5)) {
		u16_to_str(&showConfigRTDPeriodString[11], savedValues.datastreamPeriod, 5);
		TransmitString(showConfigRTDPeriodString);
	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// pwm_filter=x
//	if (mask & ((unsigned)1 << 6)) {
//	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// motor_os_threshold=xxxx motor_os_ftime=xxxx
	if (mask & ((unsigned)1 << 7)) {
		u16_to_str(&showConfigMotorFaultTimeString[19], savedValues.motorOverspeedThreshold, 4);
		u16_to_str(&showConfigMotorFaultTimeString[39], savedValues.motorOverspeedOffTime, 4);
		TransmitString(showConfigMotorFaultTimeString);
	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// motor_os_dtime=xx pwm_deadzone=xx
//	if (mask & ((unsigned)1 << 8)) {
//		u16_to_str(&showConfigMotorDeadTimeString[15], savedValues.motorOverspeedDebounceTime, 2);
//		u16_to_str(&showConfigMotorDeadTimeString[31], savedValues.pwmDeadzone, 2);
//		TransmitString(showConfigMotorDeadTimeString);
//	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// motor_speed_calc_amps=xxx
	if (mask & ((unsigned)1 << 9)) {
		u16_to_str(&showConfigMinAmpsForMotorOverspeedString[22], savedValues.minAmperesForOverspeed, 3);
		TransmitString(showConfigMinAmpsForMotorOverspeedString);
	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// battery_amps_limit=xxxx
	if (mask & ((unsigned)1 << 10)) {
		u16_to_str(&showConfigMaxBatteryAmpsString[19], savedValues.maxBatteryAmperes, 4);
		TransmitString(showConfigMaxBatteryAmpsString);
	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// precharge_time=xxx
	if (mask & ((unsigned)1 << 11)) {
		u16_to_str(&showConfigPrechargeString[15], savedValues.prechargeTime, 3);
		TransmitString(showConfigPrechargeString);
	}
	// 0         1         2         3         4         5
	// 012345678901234567890123456789012345678901234567890123456789
	// mot_amps_lim=xxxx
	if (mask & ((unsigned)1 << 12)) {
		u16_to_str(&showConfigMaxMotorAmpsString[13], savedValues.maxMotorAmperes, 4);
		TransmitString(showConfigMaxMotorAmpsString);
	}	
}
