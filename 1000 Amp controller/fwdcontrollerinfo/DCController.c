// FOR 0-5k THROTTLE!!  0 means zero throttle.  5K MEANS MAX THROTTLE.
#include "DCController.h"

/*****************Config bit settings****************/
_FOSC(0xFFFF & XT_PLL8);//XT_PLL4); // Use XT with external crystal from 4MHz to 10MHz.  FRM Pg. 178
// nominal clock is 128kHz.  The counter is 1 byte. 
//#define CONSTANT_PWM
//#ifdef DEBUG
	_FWDT(WDT_OFF);
//#else
//_FWDT(WDT_ON & WDTPSA_64 & WDTPSB_8); // See Pg. 709 in F.R.M.  Timeout in 1 second or so.  128000 / 64 / 8 / 256
//#endif

// PWMxL_ACT_LO is so the output of PWM1L will be LOW when off.  
_FBORPOR(0xFFFF & BORV_20 & PWRT_64 & MCLR_EN & PWMxH_ACT_HI); // Brown Out voltage set to 2v.  Power up time of 64 ms. MCLR is enabled. 
_FGS(CODE_PROT_OFF);  


#define MAX_DUTY_ARRAY_SIZE 401  // entries 0 to 400 inclusive.
#define MAX_RAMP_RATE 16384
#define Fcy 16000000L
#define SPREAD_SPECTRUM_SWITCHING
//#define DEBUG
//#define STANDARD_THROTTLE
#define AmpsToLemTicks(x) ((((long)x) << 7) / 300L);  	// maxMotorAmps * 128/300 = maxMotorCurrentLemTicks.

typedef struct {
	long K1;
	long K2;
	long errorNew;
	long errorOld;
	long pwm;
} piType;

SavedValuesStruct savedValuesDefault = {
	16,									// PI loop P gain (Joe's was 1)
	1280,								// PI loop I gain (Joe's was 20)
	150, // 95, //440, //413,								// throttle low voltage (pedal to metal)
	850, // 808, //700, //683,								// throttle high voltage (foot off pedal)
	11,								// throttle fault voltage (after 200mS)
	8,									// throttle pedal position gain
	0,									// throttle pwm (voltage) gain
	16384,								// current ramp rate.  0 to 16384.
	0,									// rtd (real time data) period
	0,									// motor overspeed threshold
	2000,								// motor overspeed fault time. 1 tick is 1ms
	0,									// battery amps limit
	50,									// precharge time in 0.1 second intervals.
	0,									// motor speed calc amps
	334,								// max motor amps.  Units in amperes.  400 max I think.
	0									// I compute the CRC below, so this is just garbage here.
};
SavedValuesStruct savedValues = {
	0,									// PI loop P gain (Joe's was 1)
	0,									// PI loop I gain (Joe's was 20)
	0,									// throttle low voltage (pedal to metal)
	0,									// throttle high voltage (foot off pedal)
	0,									// throttle fault voltage (after 200mS)
	0,									// throttle pedal position gain
	0,									// throttle pwm (voltage) gain
	0,									// current ramp rate.  0 to 16384.
	0,									// rtd (real time data) period
	0,									// motor overspeed threshold
	0,									// motor overspeed fault time. 1 tick is 1ms
	0,									// battery amps limit
	0,									// precharge time in tenths of a seconds.
	0,									// motor speed calc amps
	0,									// max motor amps (per IGBT).  Units in amperes.  400 max I think.
	0									// crc
};

extern unsigned int timeOfLastDatastreamTransmission;

// This is always a copy of the data that's in the EE PROM.
// To change EE Prom, modify this, and then copy it to EE Prom.

int EEDataInRamCopy1[] = {1,2,3,4,0,0,0,0,0,0,0,0,0,0,0,0};
_prog_addressT EE_addr_Copy1 = 0x7ffc00;

// 401.  No hint of high pitches.  Very white noise.
// 0 to 400 is 401 objects.  7-11kHz
const unsigned int maxDutyDiv2[] = {
 787, 723, 814, 889, 790, 778, 813, 797, 928, 938, 990, 930, 959, 973, 958,1035, 821, 893, 733, 984,1019, 908, 721,1051, 967, 761, 933, 753, 759, 708, 703,1040,
 899, 985, 871, 848, 822,1014, 846, 924, 726, 714, 782, 943, 783, 962, 746, 922, 982, 809,1100, 880,1043, 847, 952, 843, 722, 996, 913, 844, 805,1096, 757, 769,
 776,1034, 729, 854, 738,1076, 851,1059,1033,1016, 981, 953, 887, 870, 748, 804,1039, 744, 978, 991, 798,1028,1092,1094, 781,1050, 976,1038, 793,1063, 767, 859,
 897,1087, 820, 819, 816,1066, 988,1075, 784, 815, 999, 833,1056, 730,1071, 796, 874,1085, 716, 993,1005, 762, 862, 774, 705,1049, 929, 818, 747, 885,1095, 713,
 792, 987, 920, 895, 831, 828, 758,1047, 823,1023, 881,1064, 927, 812, 912,1025, 916, 811, 979,1080, 983, 742, 902,1061, 940, 950, 905, 997,1020, 755, 956, 840,
1070, 741, 842, 992, 704, 808, 711,1055, 873,1021, 998,1062,1083,1000,1002,1078,1099, 894, 955, 964, 736, 963,1068, 780, 970, 707, 702, 700,1084, 737, 771, 745,
1057, 802,1045, 853, 725, 861,1058, 969, 937, 903,1037, 773, 735, 806, 749,1030, 977, 719,1041, 865, 890, 766, 860, 883, 712, 960,1082, 866, 906, 867, 715, 765,
 939,1079, 968, 845, 835, 884, 760, 891,1003,1053,1042,1029, 717, 919,1046,1024, 971,1065, 986, 872, 732, 731, 877, 886, 936, 841, 944, 803, 799, 706,1001, 824,
1054, 918, 917, 832, 810, 878, 750, 975, 724, 739, 863,1074, 807, 868,1006,1098,1012, 949, 827, 882, 875, 829,1048,1088, 791, 915, 910,1089, 777, 838, 898, 740,
 795, 948, 932,1072,1081, 839,1067,1086,1015, 974, 800, 756, 830, 754,1027, 931,1077, 857, 951, 900,1022, 946, 751, 879,1091, 892, 876, 727, 770, 935, 907, 904,
 720, 836,1032, 945, 972, 743, 834, 763, 864,1052,1036, 849, 888, 961, 850, 801, 911, 925, 869, 785, 734, 709, 901,1093, 957, 728, 954,1090, 896,1011, 923, 786,
 914, 718, 775, 855, 817, 942,1097, 941,1009, 934, 965, 926,1007, 772, 921,1031,1060, 710, 837, 768,1073, 858,1013,1026, 909, 825,1044,1069,1017, 994,1018, 764,
 789,1004, 989, 980, 947, 856, 752, 701,1010, 995, 852, 794, 788, 779, 826, 966,1008,787
};

// 101.  Fine.
/*
unsigned int maxDutyDiv2[] = {
1029,1016, 979, 985, 965,1038,1048,1040, 963, 993,1022, 959, 988,1034, 974,1011, 969,1009,1005, 954, 960,1002,1035, 958,1006, 970, 977,1003, 968,1031, 957,1001, 964, 961,1020, 984,1021, 976, 953, 991, 989,1024, 982, 971,1030,1027,1013, 955, 983,1042, 990,1050, 956,1000,1015,1032,1004, 992, 975, 999,1023, 950,1007, 986, 998,1018, 951,1017,1041, 994,1043,1026,1010,1036,1047,1019,1028,1045,1039, 995, 972, 962,1037, 980, 966, 996, 973,1008,1044,1014, 987,1025, 978,1012, 967, 952, 981,1049,1046, 997,1033,
};
*/
//151. Fine.
/*
unsigned int maxDutyDiv2[] = {
1004, 975, 959,1048,1012,1003,1014,1051, 951, 989,1037,1020,1044, 993, 979,1066,1023,1029, 977, 973, 949,1024,1018, 937,1075, 944,1040,1065,1015, 954,1000,1046, 955, 976,1026, 946,1001, 999,1045, 952, 960,1067,1034,1041, 991,1013, 941, 996, 994, 969,1027,1022, 972, 965,1073,1038, 939,1074, 953, 983,1058,1068,1050, 936, 974,1069,1070,1011, 926,1047,1005, 938, 981,1059,1028,1031,1057, 942, 963, 988, 995, 980, 964, 956,1016, 990, 985,1064,1002,1049, 961, 958, 998, 997, 970, 978, 992,1019, 987, 934, 
971,1009,1053,1063,1035, 928,1030,1072, 957, 962,1055,1036,1006,1032,1008,1062, 930,1010,1042, 927, 968, 966,1017,1056,1033,1052,1071,1025, 950,1039, 984, 947, 945, 982, 935, 986, 943, 929,1054, 933,1061,1060,1007, 931, 948, 967, 940, 932, 925,1021,1043,
};
*/
/*
// 199. Good.
unsigned int maxDutyDiv2[] = {
927,1029,1043, 969,1001, 948, 953,1025, 932, 909, 976, 967,1071, 930, 919,1063, 955, 979,1052,1085,1058, 996,1041, 922,1062,1011, 946, 974, 934,1030, 998, 938, 990,1034, 903, 987, 936,1048,1039, 941, 961,1057,1077,1095, 973, 923,1026, 916, 960, 937,1036, 942, 956,1073, 957,1019,1066,1055,1087,1097,1089, 954,1018,1031, 951, 997,1064, 939,1023, 947, 980,1021,1053, 970,1042,1014, 959,1075,1079,1013,1090,1047, 995,1072, 929,1081, 943,1082, 907,1006, 994, 945, 910,1076, 982,1024,1067, 913, 952,1059,1040, 925, 924,1065, 931, 915, 
958,1038,1093,1083,1060, 944, 978, 963, 921, 993,1080,1056, 950,1002, 926,1074,1054,1078,1009,1070,1094,1045, 975,1049,1099, 928, 964,1091, 949, 985,1003,1061, 992, 914,1051, 984,1035, 940,1012, 902, 968,1032, 986, 971,1037,1004, 962, 933, 991, 918, 911,1044, 904,1086, 972,1005,1017,1022,1010, 920, 906,1068,1015,1050, 966, 965, 981,1008, 908,1020,1084, 905,1000, 977,1028, 988, 983,1069, 912,1033,1096,1088, 935,1092, 917,1046,1007,1016,1098,1027, 989, 999, 1100
};
*/

realtime_data_type RTData;
unsigned int showDatastreamJustOnce = 0;
volatile unsigned long int motorOverspeedThresholdTimes1024 = 0;
volatile int motorCurrentReference_0_16384 = 0;  // **** MUST start at zero. ****
volatile int motorCurrentReference_0_512 = 0;
volatile int batteryCurrent_0_512 = 0;
volatile int maxBatteryAmps = 0;
volatile int maxBatteryCurrentLemTicks = 0;
volatile int maxBatteryCurrent_0_512 = 0;
volatile long int maxBatteryCurrent_0_512_Times4096 = 0;
volatile int maxMotorCurrentLemTicks = 0;
volatile int current1_0_512 = 0;
volatile unsigned int motorOverspeedDebounceTime = 13; // 1 tick is 1/128 sec.
volatile int minMotorCurrentForOverspeed_0_512;
volatile int pwmAverage_0_4096 = 0;
volatile unsigned int faultBits = HPL_FAULT;
volatile unsigned int vRef1 = 509, vRef2 = 509, vRef3 = 509;
volatile unsigned int counter_1sec = 0;
volatile int throttleAverage = 0;
volatile int temperatureAverage = 0;
volatile int throttle_0_16384 = 0; // in [0,16384].
volatile int battery_amps = 0;				// calculated battery current
unsigned int counter1ms = 0;
volatile int overCurrentHappened = 0;
volatile int underVoltageHappened = 0;
volatile int ADCurrent1 = 0, ADCurrent2 = 0, ADCurrent3 = 0, ADThrottle = 0, ADTemperature = 0;
piType pi;

extern int TransmitString(char* str);
extern void u16x_to_str(char *str, unsigned val, unsigned char digits);
extern void u16_to_str(char *str, unsigned val, unsigned char digits);
extern void ShowMenu(void);
extern void ProcessCommand(void);
void FetchRTData();
void InitTimers();
void InitIORegisters(void);
void Delay(unsigned int);
void DelayTenthsSecond(unsigned int time);
void DelaySeconds(unsigned int time);
void ReadADInputs();
void GrabADResults();
void InitADAndPWM();
void InitDiscreteADConversions();
void GetVRefs();
void DelaySeconds(unsigned int seconds);
void DecimalToString(int number, int length);
char IntToCharHex(unsigned int i);
void InitCNModule();
void InitPIStruct();
void ClearAllFaults();  // clear the flip flop fault and the desat fault.
void ClearDesatFault();
void ClearFlipFlop();

void __attribute__((__interrupt__, auto_psv)) _CNInterrupt(void);
void __attribute__((__interrupt__,auto_psv)) _ADCInterrupt(void);

int LemTicksTo_0_512(int lemTicks);
int AmpsTo_0_512(int amps);
void NormalizeAllConfigurationCurrentsTo_0_512(void);

void MoveDataFromEEPromToRAM();
void EESaveValues();
int ClampToMaxRadius(int x, int y);

char RTDataString[] = "TR=xxx CR=xxx CF=xxx PW=xxx HS=xxxx RT=xxxx FB=xx BA=xxx AH=xxx.x\r\n";

int main() {
	int c = 0;
	InitIORegisters();

	MoveDataFromEEPromToRAM();
	EESaveValues();

	InitTimers();  // Now timer1 is running at 62.5KHz
	Delay(DELAY_200MS_SLOW_TIMER);  // 200mS, just to let voltages settle.	

	InitCNModule();
	//InitDiscreteADConversions();
	//GetVRefs();
	
	
	InitUART2();  // Now the UART is running.
	InitPIStruct();
	NormalizeAllConfigurationCurrentsTo_0_512();	

	// High pedal lockout. Wait until they aren't touching the throttle before starting the precharge sequence.
//#ifdef STANDARD_THROTTLE
//	do {
//		ReadADInputs();
//	} while (ADThrottle < savedValues.throttleHighVoltage);  // For the raw throttle, the higher the voltage, the closer to zero throttle.
//#else
//	do {
//		ReadADInputs();
//	} while (ADThrottle > savedValues.throttleLowVoltage);  // For the raw INVERTED throttle, the lower the voltage, the closer to zero throttle.
//#endif

	O_LAT_PRECHARGE_RELAY = 1;  // 1 means close the relay.  Now the cap is filling up.
	DelayTenthsSecond(savedValues.prechargeTime);
	// High pedal lockout. Wait until they aren't touching the throttle before closing the main contactor.

//#ifdef STANDARD_THROTTLE
//	do {
//		ReadADInputs();
//	} while (ADThrottle < savedValues.throttleHighVoltage);  // For the raw throttle, the higher the voltage, the closer to zero throttle.
//#else
//	do {
//		ReadADInputs();
//	} while (ADThrottle > savedValues.throttleLowVoltage);  // For the raw INVERTED throttle, the lower the voltage, the closer to zero throttle.
//#endif

	O_LAT_CONTACTOR = 1;	// close main contactor.
	DelayTenthsSecond(2);   // delay 0.2 seconds, to give the contactor a chance to close.  Then, there will be no current going through the precharge relay.
	O_LAT_PRECHARGE_RELAY = 0;  // open precharge relay once main contactor is closed.

	InitADAndPWM();		// Now the A/D is triggered by the pwm period, and the PWM interrupt is enabled.
	ClearAllFaults();
	ClearReceiveBuffer();
	ShowMenu(); 	// serial show menu.

	while(1) {
		if (I_PORT_GLOBAL_FAULT == 0) {
			faultBits |= GLOBAL_FAULT;
		}
		ProcessCommand();  // If there's a command to be processed, this does it.  haha.
		if (TMR2 > 16000) {  // TMR3:TMR2 is a 32 bit timer, running at 16MHz.
			TMR2 = 0;
			counter1ms++;
		}
		// add non time-critical code below 
		// fetch real time data
		FetchRTData();
		// if datastreamPeriod not zero display rt data at specified interval
		if (savedValues.datastreamPeriod) {
			if ((counter1ms - timeOfLastDatastreamTransmission) >= savedValues.datastreamPeriod) {
				if (showDatastreamJustOnce) {
					savedValues.datastreamPeriod = 0;  // You showed it once, now stop the stream.
				}
				// savedValues.datastreamPeriod mS passed since last time, adjust timeOfLastDatastreamTransmission to trigger again
				timeOfLastDatastreamTransmission += savedValues.datastreamPeriod;
				u16_to_str(&RTDataString[3], RTData.throttle_ref, 3);
				u16_to_str(&RTDataString[10], RTData.current_ref, 3);
				u16_to_str(&RTDataString[17], RTData.current_fb, 3);
				u16_to_str(&RTDataString[24], RTData.pwmDuty, 3);
				u16_to_str(&RTDataString[31], RTData.raw_hs_temp, 4);
				u16_to_str(&RTDataString[39], RTData.raw_throttle, 4);
				u16x_to_str(&RTDataString[47], faultBits, 2);
				u16_to_str(&RTDataString[53], RTData.battery_amps, 3);
				//x = RTData.battery_ah / (unsigned long)351562;
				//if (x > 9999) x = 9999;
				//u16_to_str(&RTDataString[60], x / 10, 3);
				//u16_to_str(&RTDataString[64], x % 10, 1);
				TransmitString(RTDataString);
			}
		}
		// let the interrupt take care of the rest...
		ClrWdt();
	}
	return 0;
}

void InitIORegisters() {
	O_TRIS_PWM = 0;  			// 0 means configure as output.
	O_LAT_PWM = 0;  			// 0 means OFF.

	ADPCFG = 0b1111111110110000;  // Set the lowest 5 bits to 0 which is analog, and make all others 1, meaning that they are digital.

	// Declare which are inputs and which are outputs:
	I_TRIS_THROTTLE = 1; 	// 1 means configure as input.
	I_TRIS_CURRENT1 = 1; 	// 1 means configure as input.
	I_TRIS_CURRENT2	= 1; 	// 1 means configure as input.
	I_TRIS_TEMPERATURE = 1; // 1 means configure as input.
	I_TRIS_GLOBAL_FAULT	= 1; // 1 means configure as input.
	I_TRIS_DESAT_FAULT = 1; // 1 means configure as input.

	O_TRIS_PRECHARGE_RELAY = 0; // 0 means configure as output.
	O_LAT_PRECHARGE_RELAY = 0; 	// 0 means turn OFF relay.  Let's not precharge yet.
	O_TRIS_CONTACTOR = 0;	   	// 0 means configure as output.
	O_LAT_CONTACTOR = 0;		// 0 means turn OFF contactor.  Precharge hasn't happened yet!
	O_TRIS_CLEAR_FLIP_FLOP = 0; // 0 means configure as output.
	O_LAT_CLEAR_FLIP_FLOP = 1; 	// You must pulse this low, then high to clear the flip flop.
	O_TRIS_CLEAR_DESAT = 0;		// 0 means configure as output.
	O_LAT_CLEAR_DESAT = 1;		// 0 means clear the desat fault.
}

void InitADAndPWM() {
    ADCON1bits.ADON = 0; // Pg. 416 in F.R.M.  Under "Enabling the Module".  Turn it off for a moment.

	// PWM Initialization

	PTPER = 999;	// 8KHz assuming 16MHz clock.
	PDC1 = 0;

	PWMCON1 = 0b0000000000000000; 			// Pg. 339 in FRM.
	PWMCON1bits.PEN1H = 1; // enabled.
	PWMCON1bits.PEN1L = 1; // enabled.  It doesn't ever seem to work if I have 1H disabled, but enable 1L.  1H is unused, so whatever.  I'll enable it.

//	PWMCON1bits.PMOD1 = 1; // independent output.

	// PEN3H = PEN2H = PEN1H = 0;  See Pg. 339 in Family Reference Manual. 
	// PEN3L = 0   PEN2L = 0   PEN1L = 1;  1 means enable that pwm channel.
	// PMOD1 = 1 means independent output mode for pwm1 pair.
	
	PWMCON2 = 0;

    DTCON1 = 0;     		// Pg. 341 in Family Reference Manual.  No dead time needed.  Just one pwm output for the charger.

//    FLTACON = 0b0000000010000001; // Pg. 343 in Family Reference Manual. The Fault A input pin functions in the cycle-by-cycle mode.

	PTCON = 0x8002;         // Pg. 337 in FRM.  Enable PWM for center aligned operation.
							// PTCON = 0b1000000000000010;  Pg. 337 in Family Reference Manual.
							// PTEN = 1; PWM time base is ON
							// PTSIDL = 0; PWM time base runs in CPU Idle mode
							// PTOPS = 0000; 1:1 Postscale
							// PTCKPS = 00; PWM time base input clock period is TCY (1:1 prescale)
							// PTMOD = 10; PWM time base operates in a continuous up/down counting mode

	// SEVTCMP: Special Event Compare Count Register 
    // Phase of ADC capture set relative to PWM cycle: 0 offset and counting up
    SEVTCMP = 2;        // Cannot be 0 -> turns off trigger (Missing from doc)
						// SEVTCMP = 0b0000000000000010;  Pg. 339 in Family Reference Manual.
						// SEVTDIR = 0; A special event trigger will occur when the PWM time base is counting upwards and...
						// SEVTCMP = 000000000000010; If SEVTCMP == PTMR<14:0>, then a special event trigger happens.


	// ============= ADC - Measure 
	// ADC setup for simultanous sampling
	// AN0 = CH1 = ADThrottle;
	// AN1 = CH2 = ADCurrent1;
	// AN2 = CH3 = ADCurrent2;
	// AN3 = CH0 = ADCurrent3;		// trade between these 2
	// AN6 = CH0 = ADTemperature;	// trade between these 2.

	ADCON1 = 0;  // Starts this way anyway.  But just to be sure.   

    ADCON1bits.FORM = 0;  // unsigned integer in the range 0-1023
    ADCON1bits.SSRC = 0b11;  // Motor Control PWM interval ends sampling and starts conversion

    // Simultaneous Sample Select bit (only applicable when CHPS = 01 or 1x)
    // Samples CH0, CH1, CH2, CH3 simultaneously (when CHPS = 1x)
    // Samples CH0 and CH1 simultaneously (when CHPS = 01)
    ADCON1bits.SIMSAM = 1; 
 
    // Sampling begins immediately after last conversion completes. 
    // SAMP bit is auto set.
    ADCON1bits.ASAM = 1;  

	ADCON2 = 0; // Pg. 407 in F.R.M.
    // Pg. 407 in F.R.M.
    // Samples CH0, CH1, CH2, CH3 simultaneously when CHPS = 1x
    ADCON2bits.CHPS = 0b10; // VCFG = 000; This selects the A/D High voltage as AVdd, and A/D Low voltage as AVss.
						 // SMPI = 0000; This makes an interrupt happen every time the A/D conversion process is done (for all 4 I guess, since they happen at the same time.)
						 // ALTS = 0; Always use MUX A input multiplexer settings
						 // BUFM = 0; Buffer configured as one 16-word buffer ADCBUF(15...0)


 	ADCON3 = 0; // Pg. 408 in F.R.M.
    // Pg. 408 in F.R.M.
    // A/D Conversion Clock Select bits = 4 * Tcy.  (7+1) * Tcy/2 = 4*Tcy.
	// The A/D conversion of 4 simultaneous conversions takes 4*12*A/D clock cycles.  The A/D clock is selected to be 16*Tcy.
    // So, it takes about 48us to complete if ADCS = 15??  The pwm period is 125uS, since it's 8kHz nominal.
	ADCON3bits.ADCS = 15;  // 


    // ADCHS: ADC Input Channel Select Register 
    ADCHS = 0; // Pg. 409 in F.R.M.

    // ADCHS: ADC Input Channel Select Register 
    // Pg. 409 in F.R.M.
    // CH0 positive input is AN6, temperature.
    ADCHSbits.CH0SA = 6;
    	
	// CH1 positive input is AN0, CH2 positive input is AN1, CH3 positive input is AN2.
	ADCHSbits.CH123SA = 0;

	// CH0 negative input is Vref-.
	ADCHSbits.CH0NA = 0;

	// CH1, CH2, CH3 negative inputs are Vref-, which is AVss, which is Vss.  haha.
    ADCHSbits.CH123NA = 0;

    // ADCSSL: ADC Input Scan Select Register 
    ADCSSL = 0; // Pg. 410 F.R.M.
				// I think it sets the order that the A/D inputs are done.  But I'm doing 4 all at the same time, so set it to 0?


    // Turn on A/D module
    ADCON1bits.ADON = 1; // Pg. 416 in F.R.M.  Under "Enabling the Module"
						 // ** It's important to set all the bits above before turning on the A/D module. **
						 // Now the A/D conversions start happening once ADON == 1.
	_ADIP = 4;			 // A/D interrupt priority set to 4.  Default is 4.
	IEC0bits.ADIE = 1;	 // Enable interrupts to happen when a A/D conversion is complete. Pg. 148 of F.R.M.  	
	PDC1 = 0;
}

//---------------------------------------------------------------------
// The ADC sample and conversion is triggered by the PWM period.
//---------------------------------------------------------------------
// This runs between 6.667kHz and 10kHz, randomly.  So, 8KHz on average. 
// ADCounter1 divides the computing load into 2 parts, 1 and 2.  Part 1 takes care of current feedback, and the PI loop.
// Part 2 does throttle, temperature, motor overspeed, battery amp limit.  Only one of those is done at a time, by use of ADCounter2.
// So, here's what happens when you enter the interrupt:
//
// 1  Normalize current to [0,512], and run PI loop.  4KHz +/- due to spread spectrum switching variation.
// 2a Normalize throttle to [0,16384]. 1KHz +/- due to spread spectrum switching variation.
// 1  Normalize current to [0,512], and run PI loop. 4KHz +/- due to spread spectrum switching variation.
// 2b Limit current ramp rate.  currentCommand chases throttle position.  Run battery amp limit logic. 1KHz +/- due to spread spectrum switching variation.
// 1  Normalize current to [0,512], and run PI loop. 4KHz +/- due to spread spectrum switching variation.
// 2c Thermal cutback logic. 1KHz +/- due to spread spectrum switching variation.
// 1  Normalize current to [0,512], and run PI loop. 4KHz +/- due to spread spectrum switching variation.
// 2d Run sensorless over-rev protection logic. 1KHz +/- due to spread spectrum switching variation.
// REPEAT...

unsigned long int xor128(void) {
  static unsigned long x = 123456789;
  static unsigned long y = 362436069;
  static unsigned long z = 521288629;
  static unsigned long w = 88675123;
  unsigned long t;
  t = x ^ (x << 11);   
  x = y; y = z; z = w;   
  return w = w ^ (w >> 19) ^ (t ^ (t >> 8));
}

void __attribute__ ((__interrupt__,auto_psv)) _ADCInterrupt(void) {
	static int ADCounter1 = 0;
	static int ADCounter2 = 0;
	static int i = 0;
	static int x = 0;
	static int pwm_0_4096 = 0; // [0,4096]
	static long int pwmAverage_0_4096Times65536 = 0;
	static int tempThrottle = 0;
	static long int throttleAverageTimes65536 = 0;
	static long int temperatureAverageTimes65536 = 0;
	static int temperatureMultiplier = 8;
	static int maxMotorCurrentAllowedGivenBatteryCurrentRestriction = 0;
	static int motorOverspeedDebounceCounter = 0;
	static int motorOverspeedFaultDurationTimer = 0;
	static unsigned long int motorSpeedLong = 0;
	static long pwmAverage_0_4096DeadzoneSubtractedTimes65536 = 0;
	static int throttleFaultCounter = 0;
	static int rampRate = 0;
	static int primeArrayPosition = 1;
    IFS0bits.ADIF = 0;  	// Interrupt Flag Status Register. Pg. 142 in F.R.M.
							// ADIF = A/D Conversion Complete Interrupt Flag Status bit.  
							// ADIF = 0 means we are resetting it so that an interrupt request has not occurred.
	
	// CH0 corresponds to ADCBUF0. etc...
	// CH0=AN6, CH1=AN0, CH2=AN1, CH3=AN2. 
	// AN0 = CH1 = ADThrottle
	// AN1 = CH2 = ADCurrent1
	// AN2 = CH3 = ADCurrent2
	// AN6 = CH0 = ADTemperature

	ADTemperature = ADCBUF0;
	ADThrottle = ADCBUF1;
	ADCurrent1 = ADCBUF2;	// CH2 = ADCurrent1
	ADCurrent2 = ADCBUF3;	// CH2 = ADCurrent1
	if ((ADCounter1 & 3) == 0x00) { // 2.5kHz.
		 //continuous Vref fault checking - if below 0.5v or so, set fault. 
		if (ADCurrent1 < 100 || ADCurrent1 > 900) { // || ADCurrent2 < 100 || ADCurrent2 > 900) { // || ADCurrent2 < 100) { 		// Really, this one should never be much below 512..
			faultBits |= VREF_FAULT;
		}
		if (ADCurrent2 < 100 || ADCurrent2 > 900) {
			faultBits |= VREF_FAULT2;
		}

		// I'm using the LEM Hass 300-s.  So, current starts in [512, 512 + maxMotorCurrentLemTicks].  maxMotorCurrentLemTicks is a function of maxMotorAmperes

		// Let's deal with ADCurrent1.  Scale to [0, 512]
		if (ADCurrent1 < vRef1) {		// Assume this is just noise.
			ADCurrent1 = 0;
		}
		else {
			ADCurrent1 -= vRef1;
		}
		// Now, ADCurrent1 is in [0, maxMotorCurrentLemTicks].  Scale it to [0,512].
		current1_0_512 = (((long)ADCurrent1) << 9)/((long)maxMotorCurrentLemTicks);
		// Now, current1_0_512 is in [0, 512].  Possibly more, but close to that.	
		if (faultBits) {
			motorCurrentReference_0_512 = 0;
		}

		// PI loop:
		pi.errorNew = motorCurrentReference_0_512 - current1_0_512;
		// execute PI loop
		// first, K1 = Kp << 10;  
		// second, K2 = Ki - K1;
		if (motorCurrentReference_0_512 == 0) {
			pi.pwm = 0;
			//pi.Kp = 0;
			//pi.Ki = 0;

			// if Kp and Ki = 0, then K1 and K2 = 0;
			// if K1 and K2 = 0, then pwm = pwm + 0, and 0 + 0 = 0
			// so we don't need to run the PI loop, just set errorOld to errorNew
		}
		else {
			pi.pwm += (pi.K1 * pi.errorNew) + (pi.K2 * pi.errorOld);
		}
		pi.errorOld = pi.errorNew;
		if (pi.pwm > (4096L << 16))  {
			pi.pwm = (4096L << 16);		
		}
		else if (pi.pwm < 0L) {
			pi.pwm = 0L;
		}
		pwm_0_4096 = (pi.pwm >> 16);
		// Now, pwm_0_4096 is in [0, 4096].

		// 1.  pwmAverageTimes65536 = (31*pwmAverageTimes65536)/65536 + 1*pwm_0_4096) / 32 * 65536;
		// 2.  pwmAverageTimes65536 = 32*(pwmAverageTimes65536/65536) - 1*pwmAverage + 1*pwm_0_4096) / 32 * 65536
		pwmAverage_0_4096Times65536 = ((pwmAverage_0_4096Times65536 >> 11) - ((long)pwmAverage_0_4096) + ((long)pwm_0_4096)) << 11;
		pwmAverage_0_4096 = (pwmAverage_0_4096Times65536 >> 16);  // pwmAverage is now in [0, 4096]
	}
	else if ((ADCounter1 & 3) == 0x01) { // 2.5kHz, due to spread spectrum switching.  Throttle, temperature, etc...
		// AN0 = CH1 = ADThrottle
		// AN1 = CH2 = ADCurrent1
		// AN2 = CH3 = ADCurrent2
		// AN6 = CH0 = ADTemperature
		throttleAverageTimes65536   	//= (15*throttleAverageTimes65536)/65536 + 1*ADThrottle) / 16 * 65536;
								 		//= 16*(throttleAverageTimes65536/65536) - 1*throttleAverage + 1*ADThrottle) / 16 * 65536
								= ((throttleAverageTimes65536 >> 12) - ((long)throttleAverage) + ((long)ADThrottle)) << 12;

		temperatureAverageTimes65536	//= (127*temperatureAverageTimes65536)/65536 + 1*ADTemperature) / 128 * 65536;
								 		//= 128*temperatureAverageTimes65536/65536 - 1*(temperatureAverageTimes65536/65536) + 1*ADTemperature) / 128 * 65536
								= ((temperatureAverageTimes65536 >> 9) - ((long)temperatureAverage) + ((long)ADTemperature)) << 9;

		throttleAverage = throttleAverageTimes65536 >> 16; 
		temperatureAverage = temperatureAverageTimes65536 >> 16;

		if ((ADCounter2 & 3) == 0) { // only happens every 4 times.  Around 625 Hz.
			tempThrottle = throttleAverage; // you are going to do a bunch of transformations on tempThrottle. Don't mess with throttleAverage.  It's updated in the ISR.
			// At this point, tempThrottle nominally is in [savedValues.throttleLowVoltage, savedValues.throttleHighVoltage-1].  -1 because the averaging scheme above never rounds up.
//			if (tempThrottle > savedValues.throttleFaultVoltage) {  // Everything is fine.  Throttle not disconnected.
//				if (throttleFaultCounter > 0) // Inch it closer back to zero, which is where everything is wonderful.  haha.
//					throttleFaultCounter--;
//			}
//			else {  // it's in fault territory.  Possibly disconnected throttle.
//				throttleFaultCounter++;		// Don't worry about clamping throttleFaultCounter.  If the throttle fault bit gets set, there's no undoing it.
//				if (throttleFaultCounter >= THROTTLE_FAULT_COUNTS) 
//					faultBits |= THROTTLE_FAULT;
//			}
			
			if (tempThrottle > savedValues.throttleHighVoltage) {
				tempThrottle = savedValues.throttleHighVoltage;
			}
			else if (tempThrottle < savedValues.throttleLowVoltage) {
				tempThrottle = savedValues.throttleLowVoltage;
			}
		
			tempThrottle -= savedValues.throttleLowVoltage;
			// now tempThrottle is in [0, (throttleHighVoltage - throttleLowVoltage)].  In the 5k to 0Ohm throttle case, you don't have to do the next line...
#ifdef STANDARD_THROTTLE
			// Invert throttle so that 0Ohm is 0 throttle and 5kOhm is max throttle.
			tempThrottle = (savedValues.throttleHighVoltage - savedValues.throttleLowVoltage) - tempThrottle;
#endif
			// now, 0 throttle is 0 (5kOhm), and max throttle is (throttleHighVoltage - throttleLowVoltage) (which is zero Ohms)
			throttle_0_16384 = __builtin_divsd(((long)tempThrottle) << 14,(int)(savedValues.throttleHighVoltage - savedValues.throttleLowVoltage));
			// Just transformed from [0,throttleHighVoltage - throttleLowVoltage] to [0,16384].  I normalize the interval to make it comparable to the current,
			//   which gets normalized to [0,512].  That makes doing the PI loop easier, since you want the current feedback to match throttle.  For example, 
			//   50% throttle means you want 50% of maximum motor current.

			// Now, correct for temperature.  If it's too hot, basically limit the access to the full throttle range.
				throttle_0_16384 = temperatureMultiplier * (throttle_0_16384 >> 3);  // So, >> 5 instead of >> 3 (above).  divide by 32 first.  It loses resolution, but is still in 0,512. good enough for government work.
			// now throttle_0_16384 is in [0 to 16384].  But this happened too fast! Oh no...
			// Let motorCurrentReference_0_16384 follow throttle_0_16384, with a limited ramp rate.
				if (throttle_0_16384 == 0) {
					faultBits &= ~HPL_FAULT;
				}
		}
		else if ((ADCounter2 & 3) == 1) { // happens at around 625 Hz.
			// motorCurrentReference_0_16384 must now chase throttle_0_16384. Limit the ramp rate.
			
			rampRate = savedValues.currentRampRate;  // 

			i = throttle_0_16384 - motorCurrentReference_0_16384;
			if (i > rampRate) {
				i = rampRate;
			}
			else if (i < -MAX_RAMP_RATE) {
				i = -MAX_RAMP_RATE;
			}
			motorCurrentReference_0_16384 += i;
			motorCurrentReference_0_512 = (motorCurrentReference_0_16384 >> 5);

			// to limit battery amps, limit motor amps based on PWM (with low pass filter)
			if (savedValues.maxBatteryAmperes > 0) {
				if (pwmAverage_0_4096 > 0) {  // This finds the maximum motor amps allowed, given the pwm duty. pwmAverage is in [0,4096].
					maxMotorCurrentAllowedGivenBatteryCurrentRestriction = maxBatteryCurrent_0_512_Times4096 / (unsigned long)pwmAverage_0_4096;  // this is the largest allowable motor amps for the given duty.
					if (motorCurrentReference_0_512 > maxMotorCurrentAllowedGivenBatteryCurrentRestriction) {  // If the motor amps reference is too big, clamp it.
						motorCurrentReference_0_512 = maxMotorCurrentAllowedGivenBatteryCurrentRestriction;
					}
				}
			}
		}

		else if ((ADCounter2 & 3) == 2) { //  625 Hz.
			// not much going on in here.  May as well do this multiply:
			batteryCurrent_0_512 = ((unsigned long)current1_0_512 * (unsigned long)pwmAverage_0_4096) >> 12;  // pwmAverage is in [0, 4096].
			//////////////////////////////////////////////////////////////////////////////
			if (temperatureAverage > THERMAL_CUTBACK_START) {  // Force the throttle to cut back.
				temperatureMultiplier = (temperatureAverage - THERMAL_CUTBACK_START) >> 3;  // 0 THROUGH 7.
				if (temperatureMultiplier >= 7)
					temperatureMultiplier = 0;
				else {
					// temperatureMultiplier is now 6 to 0 (for 1/8 to 7/8 current)
					temperatureMultiplier = 7 - temperatureMultiplier;
					// temperatureMultiplier is now 1 for 1/8, 2 for 2/8, ..., 7 for 7/8, etc.
				}
			}
			else {
				temperatureMultiplier = 8;	// Allow full throttle.
			}
		}
/*
		else if ((ADCounter2 & 3) == 3) { // 1KHz
			// run motor overspeed logic
			if (savedValues.motorOverspeedThreshold > 0) {
				// motor overspeed detection logic enabled
				if (faultBits & MOTOR_OVERSPEED_FAULT) {
					// we have a motor overspeed fault
					if (motorOverspeedFaultDurationTimer) {		// Keep the motor off for this number of milliseconds.
						motorOverspeedFaultDurationTimer--;
						if (motorOverspeedFaultDurationTimer == 0) {
							// motor overspeed fault expired, reset fault
							faultBits &= ~MOTOR_OVERSPEED_FAULT;
						}
					}
				}
				else {
					// use pwmAverage for the pwm value.
					// no motor overspeed fault, so check for overspeed
					if (pwmAverage_0_4096 > pwmDeadzone) {
						pwmAverage_0_4096DeadzoneSubtractedTimes65536 = (unsigned long)(pwmAverage_0_4096 - pwmDeadzone) << 16;  // I'm not sure of the reason for the deadzone here.  Fran did it.  When in Rome!  haha.
					}																											// I'm not convinced this is necessary.  I will test and then dump if possible.
					else {
						pwmAverage_0_4096DeadzoneSubtractedTimes65536 = 0;
					}
					// logic changed slightly in v1.11b
					// if current feedback > motor_speed_calc_amps, then rpm = k * V / current_feedback
					// else rpm = k * V / motor_speed_calc_amps
					if (current1_0_512 > minMotorCurrentForOverspeed_0_512) {
						motorSpeedLong = pwmAverage_0_4096DeadzoneSubtractedTimes65536 / (unsigned long)current1_0_512;
					}
					else {
						motorSpeedLong = pwmAverage_0_4096DeadzoneSubtractedTimes65536 / (unsigned long)(minMotorCurrentForOverspeed_0_512 + 1);
					}

					if (motorSpeedLong > motorOverspeedThresholdTimes1024) {  // I'm not sure why this is scaled by 1024. I guess it just worked this way.
						if (motorOverspeedDebounceCounter < motorOverspeedDebounceTime) {  // We were fishing around, trying to find what would cause the motor overspeed to trip.
							motorOverspeedDebounceCounter++;
						}
						else {	// we have motor overspeed
							faultBits |= MOTOR_OVERSPEED_FAULT;
							motorOverspeedDebounceCounter = 0;
							motorOverspeedFaultDurationTimer = savedValues.motorOverspeedOffTime;
						}
					}
					else {
						motorOverspeedDebounceCounter = 0;
					}
				}
			}
		}
		*/
		ADCounter2++;
	}
	else if ((ADCounter1 & 3) == 0x02) {
		#ifdef SPREAD_SPECTRUM_SWITCHING 
			x += primeArrayPosition;
			if (x >= MAX_DUTY_ARRAY_SIZE) {  // modulo arithmetic
				x -= MAX_DUTY_ARRAY_SIZE;
				if (x == 0) {  // this happens after MAX_DUTY_ARRAY_SIZE iterations, since MAX_DUTY_ARRAY_SIZE is prime.
					if (primeArrayPosition >= MAX_DUTY_ARRAY_SIZE - 1) {
						primeArrayPosition = 1;
					}
					else {
						primeArrayPosition++;
					}
				}
			}
			#ifndef CONSTANT_PWM
			PTPER = maxDutyDiv2[x];		// set the new PWM frequency. That will take effect at the start of the next cycle.
			if (pwm_0_4096 > 4095) pwm_0_4096 = 4095; 
			PDC1 = __builtin_mulsu(pwm_0_4096,maxDutyDiv2[x]) >> 11; // >> 11 instead of >> 12 because it's maxDutyDiv2 array, NOT maxDuty array!! So, PDC1 needs an extra *2.
			#else
			PTPER = 999;
			PDC1 = 999;
			#endif
			// Now, PDC1 is in [0, maxDuty]
			// UPDATE These at the same time!!!  so they both take effect on the start of the next cycle.
		#else 
			PTPER = 999;  // You don't need to set this every time.  Just for clarity.
			PDC1 = (((long)pwm_0_4096) * 1999L) >> 12;
		#endif
	}
	ADCounter1++; 
}

// Make sure this is only called after you know the previous conversion is done.  Right now, the conversion is taking ?? uS, so call this
// when ?? us has gone by after starting "StartADConversion()".
// Run this just before starting the next ADConversion.  It will be a lag, but I'll know what's what.  haha.
void GrabADResults() {
	 ADTemperature = ADCBUF0;
	 ADThrottle = ADCBUF1;
	 ADCurrent1 = ADCBUF2;
	 ADCurrent2 = ADCBUF3;
	// AN0 = CH1 = ADThrottle
	// AN1 = CH2 = ADCurrent1
	// AN2 = CH3 = ADCurrent2
	// AN6 = CH0 = ADTemperature
}
// This is the slow one, when you just want to start a conversion, and wait for the results.
void ReadADInputs() {
	ADCON1bits.SAMP = 1; // start sampling ...
	Delay(256); // for 4mS
	ADCON1bits.SAMP = 0; // Stop sampling, and start converting.
	while (!ADCON1bits.DONE) {
		ClrWdt(); // conversion done?
	}
	GrabADResults();
}
/*
void GetVRefs() {
	int i, sum1 = 0, sum2 = 0; //, sum3 = 0;

	for (i = 0; i < 32; i++) {
		ReadADInputs();
		sum1 += ADCurrent1;
		sum2 += ADCurrent2;
//		sum2 += ADCurrent3;
	}
	vRef1 = (sum1 >> 5);
	vRef2 = (sum2 >> 5);
//	vRef3 = (sum3 >> 5);

	if (vRef1 < 512 - 50 || vRef1 > 512 + 50 || 
		vRef2 < 512 - 50 || vRef2 > 512 + 50) {
//		vRef3 < 512 - 50 || vRef3 > 512 + 50) {
		faultBits |= VREF_FAULT;
	}
}
*/
void InitTimers() {
	T1CON = 0;  // Make sure it starts out as 0.
	T1CONbits.TCKPS = 0b11;  // prescale of 256.  So, timer1 will run at 62.5KHz if Fcy is 16.000MHz.
	PR1 = 0xFFFF;  // 
	T1CONbits.TON = 1; // Start the timer.

	T2CONbits.T32 = 1;  // 32 bit mode.
	T2CONbits.TCKPS = 0b00;  // 1:1 prescaler.
	T2CONbits.TCS = 0;  	// use internal 16MHz Fcy for the clock.
	PR3 = 0x0FFFF;		// HIGH 16 BITS.
	PR2 = 0x0FFFF;		// low 16 bits.
	// Now, TMR3:TMR2 makes up the 32 bit timer, running at 16MHz.

	T4CONbits.T32 = 1;  // 32 bit mode.
	T4CONbits.TCKPS = 0b11;  // 1:1 prescaler.
	T4CONbits.TCS = 0;  	// use internal 16MHz Fcy for the clock.
	PR5 = 0x0FFFF;		// HIGH 16 BITS.
	PR4 = 0x0FFFF;		// low 16 bits.

	T2CONbits.TON = 1;	// Start the timer.
	T4CONbits.TON = 1;	// Start the timer.

	TMR3 = 0;  	// Timer3:Timer2 high word
	TMR5 = 0;	// Timer5:Timer4 high word

	TMR2 = 0;  	// Timer3:Timer2 low word
	TMR4 = 0;	// Timer5:Timer4 low word
}	

// Assuming a 16.000MHz clock, one tick is 1/62500 seconds.
void Delay(unsigned int time) {
	static unsigned int temp;
	temp = TMR1;	
	while (TMR1 - temp < time) {
		ClrWdt();
	}
}
void DelaySeconds(unsigned int time) {
	int i;
	for (i = 0; i < time; i++) { 
		Delay(62500);  
	}
}
void DelayTenthsSecond(unsigned int time) {
	int i;
	for (i = 0; i < time; i++) { 
		Delay(6250);  
	}
}
// Input is an integer from 0 to 15.  Output is a character in '0', '1', '2', ..., '9', 'a','b','c','d','e','f'
char IntToCharHex(unsigned int i) {
	if (i <= 9) {
		return ((unsigned char)(i + 48));
	}
	else {
		return ((unsigned char)(i + 55));
	}
}

int LemTicksTo_0_512(int lemTicks) {
	return (((long)lemTicks)* 512L) / (long)maxMotorCurrentLemTicks; // [0,maxMotorCurrentLemTicks] --> [0,512].
}

int AmpsTo_0_512(int amps) {
	int x;
	x = AmpsToLemTicks(amps);
	return LemTicksTo_0_512(x);
}

void NormalizeAllConfigurationCurrentsTo_0_512() {
	maxMotorCurrentLemTicks = AmpsToLemTicks(savedValues.maxMotorAmperes);
//	maxMotorCurrentLemTicks /= 3; 
	maxBatteryCurrent_0_512 = AmpsTo_0_512(savedValues.maxBatteryAmperes);
	maxBatteryCurrent_0_512_Times4096 = ((long)maxBatteryCurrent_0_512) << 12;
	minMotorCurrentForOverspeed_0_512 = AmpsTo_0_512(savedValues.minAmperesForOverspeed);
	motorCurrentReference_0_16384 = 0;	// this chases throttle position, limited by a ramp rate.
}

void InitCNModule() {
	CNEN1bits.CN0IE = 1;
	CNEN1bits.CN1IE = 1;

	CNPU1bits.CN0PUE = 0; // Make sure internal pull-up is turned OFF on CN6.
	CNPU1bits.CN1PUE = 0; // Make sure internal pull-up is turned OFF on CN7.

	_CNIF = 0; // Clear change notification interrupt flag just to make sure it starts cleared.
	_CNIP = 3;  // Set the priority level for interrupts to 3.
	_CNIE = 1; // Make sure interrupts are enabled.
}

void __attribute__((__interrupt__, auto_psv)) _CNInterrupt(void) {
	IFS0bits.CNIF = 0;  // clear the interrupt flag.

	if (I_PORT_DESAT_FAULT == 0) {  // It just became 0 like 2Tcy's ago.
		faultBits |= DESAT_FAULT;
	}
	if (I_PORT_UNDERVOLTAGE_FAULT == 0) {
		faultBits |= UNDERVOLTAGE_FAULT;
	}
}

void InitPIStruct() {
	pi.K1 = (((long)savedValues.Kp) << 10);
	pi.K2 = ((long)savedValues.Ki) - pi.K1;
	pi.errorOld = 0;
	pi.errorNew = 0;
	pi.pwm = 0;
}

void FetchRTData(void)
{
	RTData.throttle_ref = throttle_0_16384;	
	RTData.throttle_ref >>= 5;  	// Now it's normalized to 0-512.
	RTData.current_ref = motorCurrentReference_0_512;	// pick the current ref that's normalized to 0-512.
	RTData.current_fb = current1_0_512;		// pick the current fb that's normalized to 0-512.
	RTData.raw_hs_temp = temperatureAverage; // same old same old.
	RTData.raw_throttle = throttleAverage; // hmm...  How about throttleAverage?
	RTData.battery_amps = batteryCurrent_0_512; // pick the one that's normalized to 0-512.
	RTData.battery_ah = 0; //battery_ah; // I skipped this part.  haha.
	RTData.pwmDuty = pwmAverage_0_4096;
	RTData.pwmDuty >>= 3; // pwmAverage_0_4096 is in [0, 4096].  pwmDuty should be in [0, 512] to stay compliant with RTD Explorer.
}

void ClearAllFaults() {
	int i = 0;

	O_LAT_CLEAR_DESAT = 0; 	// FOD8316 Datasheet says low for at least 1.2uS.  But then the stupid fault signal may not be cleared for 20 whole uS!
	for (i = 0; i < 6; i++) {
		Nop(); Nop(); Nop(); Nop();	Nop(); Nop(); Nop(); Nop();	// 0.5uS
	} // 3uS better be long enough!
	O_LAT_CLEAR_DESAT = 1;

	for (i = 0; i < 60; i++) {  // Now, let's waste more than 30uS waiting around for the desat fault signal to be cleared.
		Nop(); Nop(); Nop(); Nop();	Nop(); Nop(); Nop(); Nop();	// 0.5uS	// That means I have to burn 30uS in here.  
	} // 30uS better be long enough!
	
	O_LAT_CLEAR_FLIP_FLOP = 0;  // Really like 100nS would be enough.
	Nop(); Nop(); Nop(); Nop();	Nop(); Nop(); Nop(); Nop();	// 0.5uS
	Nop(); Nop(); Nop(); Nop();	Nop(); Nop(); Nop(); Nop(); // 0.5uS
	O_LAT_CLEAR_FLIP_FLOP = 1;
}

void ClearFlipFlop() {
	O_LAT_CLEAR_FLIP_FLOP = 0;  // Really like 100nS would be enough.
	Nop(); Nop(); Nop(); Nop();	Nop(); Nop(); Nop(); Nop();	// 0.5uS
	Nop(); Nop(); Nop(); Nop();	Nop(); Nop(); Nop(); Nop(); // 0.5uS
	O_LAT_CLEAR_FLIP_FLOP = 1;	
}

void ClearDesatFault() {	// reset must be pulled low for at least 1.2uS.
	int i = 0;
	O_LAT_CLEAR_DESAT = 0; 	// FOD8316 Datasheet says low for at least 1.2uS.  But then the stupid fault signal may not be cleared for 20 whole uS!@@@@@
	for (i = 0; i < 6; i++) {
		Nop(); Nop(); Nop(); Nop();	Nop(); Nop(); Nop(); Nop();	// 0.5uS	// That means I have to burn 20uS in here.  
	} // 3uS better be long enough!
	O_LAT_CLEAR_DESAT = 1;

	for (i = 0; i < 60; i++) {  // Now, let's waste more than 30uS waiting around for the desat fault signal to be cleared.
		Nop(); Nop(); Nop(); Nop();	Nop(); Nop(); Nop(); Nop();	// 0.5uS	// That means I have to burn 30uS in here.  
	} // 30uS better be long enough!
}

void EESaveValues() {  // save the new stuff.

	EEDataInRamCopy1[0] = savedValues.Kp;		// PI loop proportional gain
	EEDataInRamCopy1[1] = savedValues.Ki;								// PI loop integreal gain
	EEDataInRamCopy1[2] = savedValues.throttleLowVoltage;		// throttle low voltage (full throttle)
	EEDataInRamCopy1[3] = savedValues.throttleHighVoltage;		// throttle high voltage (zero throttle)
	EEDataInRamCopy1[4] = savedValues.throttleFaultVoltage;		// throttle fault voltage.  Too low of voltage (to protect from disconnected throttle.
	EEDataInRamCopy1[5] = savedValues.throttlePositionGain;		// gain for actual throttle position
	EEDataInRamCopy1[6] = savedValues.throttlePWMGain;			// gain for pwm (voltage)
	EEDataInRamCopy1[7] = savedValues.currentRampRate;				// 0-8.  8 means zero seconds to max current. 7 means 1 second to max current ... 0 means 8 seconds to max current.
	EEDataInRamCopy1[8] = savedValues.datastreamPeriod;			// real time data period
	EEDataInRamCopy1[9] = savedValues.motorOverspeedThreshold;	// motor overspeed threshold
	EEDataInRamCopy1[10] = savedValues.motorOverspeedOffTime;	// motor overspeed fault time, in units of about 1/128 sec.
	EEDataInRamCopy1[11] = savedValues.maxBatteryAmperes;		// battery amps limit.  Unit is amperes. Must be <= maxMotorAmps 
	EEDataInRamCopy1[12] = savedValues.prechargeTime;			// precharge time in 0.1 second increments
	EEDataInRamCopy1[13] = savedValues.minAmperesForOverspeed;	// motor current must be > motor_sc_amps to calculate motor speed.  Units are amperes.
	EEDataInRamCopy1[14] = savedValues.maxMotorAmperes;		// motor amps limit.  Unit is amperes.  Must be >= maxBatteryAmps.
	EEDataInRamCopy1[15] =  savedValues.Kp + savedValues.Ki + savedValues.throttleLowVoltage + savedValues.throttleHighVoltage + 
					  		savedValues.throttleFaultVoltage + savedValues.throttlePositionGain + savedValues.throttlePWMGain + 
					  		savedValues.currentRampRate + savedValues.datastreamPeriod + savedValues.motorOverspeedThreshold + savedValues.motorOverspeedOffTime +
					  		savedValues.maxBatteryAmperes + savedValues.prechargeTime + savedValues.minAmperesForOverspeed + savedValues.maxMotorAmperes;

    _erase_eedata(EE_addr_Copy1, _EE_ROW);
    _wait_eedata();  // #define _wait_eedata() { while (NVMCONbits.WR); }
	ClrWdt();
    // Write a row to EEPROM from array "EEDataInRamCopy1"
    _write_eedata_row(EE_addr_Copy1, EEDataInRamCopy1);
    _wait_eedata();  // Hopefully this takes less than 3 seconds.  haha.
	ClrWdt();
}

void MoveDataFromEEPromToRAM() {
	int i = 0;
	unsigned int CRC1 = 0, CRC2 = 0, CRC3 = 0, CRC4 = 0;

	_memcpy_p2d16(EEDataInRamCopy1, EE_addr_Copy1, _EE_ROW);
	for (i = 0; i < 15; i++) { //for (i = 0; i < (sizeof(SavedValuesStruct) >> 1) - 1; i++) {  // i = 0; i < 15; i++.  Skip the last one, which is CRC.
		CRC1 += EEDataInRamCopy1[i];
	}
	if (EEDataInRamCopy1[15] == CRC1) {  // crc from EEProm is OK for copy 1.  There has been a previously saved configuration.  
										// No need to load the default configuration. 
		savedValues.Kp = EEDataInRamCopy1[0];		// PI loop proportional gain
		savedValues.Ki = EEDataInRamCopy1[1];						// PI loop integral gain
		savedValues.throttleLowVoltage = EEDataInRamCopy1[2];		// throttle low voltage (pedal to metal)
		savedValues.throttleHighVoltage = EEDataInRamCopy1[3];		// throttle high voltage (foot off pedal)
		savedValues.throttleFaultVoltage = EEDataInRamCopy1[4];		// throttle fault voltage.  Too low of voltage (to protect from disconnected throttle.
		savedValues.throttlePositionGain = EEDataInRamCopy1[5];		// gain for actual throttle position
		savedValues.throttlePWMGain = EEDataInRamCopy1[6];			// gain for pwm (voltage)
		savedValues.currentRampRate = EEDataInRamCopy1[7];				// 0-8.  8 means zero seconds to max current. 7 means 1 second to max current ... 0 means 8 seconds to max current.
		savedValues.datastreamPeriod = EEDataInRamCopy1[8];			// real time data period
		savedValues.motorOverspeedThreshold = EEDataInRamCopy1[9];	// motor overspeed threshold
		savedValues.motorOverspeedOffTime = EEDataInRamCopy1[10];	// motor overspeed fault time, in units of about 1/128 sec.
		savedValues.maxBatteryAmperes = EEDataInRamCopy1[11];		// battery amps limit.  Unit is amperes. Must be <= maxMotorAmps 
		savedValues.prechargeTime = EEDataInRamCopy1[12];			// precharge time in 0.1 second increments
		savedValues.minAmperesForOverspeed = EEDataInRamCopy1[13];	// motor current must be > motor_sc_amps to calculate motor speed.  Units are amperes.
		savedValues.maxMotorAmperes = EEDataInRamCopy1[14];		// motor amps limit.  Unit is amperes.  Must be >= maxBatteryAmps.
		savedValues.crc = CRC1;
	}
	else {	// There wasn't a single good copy.  Load the default configuration.
		savedValues.Kp = savedValuesDefault.Kp;		// PI loop proportional gain
		savedValues.Ki = savedValuesDefault.Ki;								// PI loop integreal gain
		savedValues.throttleLowVoltage = savedValuesDefault.throttleLowVoltage;		// throttle low voltage (pedal to metal)
		savedValues.throttleHighVoltage = savedValuesDefault.throttleHighVoltage;		// throttle high voltage (foot off pedal)
		savedValues.throttleFaultVoltage = savedValuesDefault.throttleFaultVoltage;		// throttle fault voltage.  Too low of voltage (to protect from disconnected throttle.
		savedValues.throttlePositionGain = savedValuesDefault.throttlePositionGain;		// gain for actual throttle position
		savedValues.throttlePWMGain = savedValuesDefault.throttlePWMGain;			// gain for pwm (voltage)
		savedValues.currentRampRate = savedValuesDefault.currentRampRate;				// 0-8.  8 means zero seconds to max current. 7 means 1 second to max current ... 0 means 8 seconds to max current.
		savedValues.datastreamPeriod = savedValuesDefault.datastreamPeriod;			// real time data period
		savedValues.motorOverspeedThreshold = savedValuesDefault.motorOverspeedThreshold;	// motor overspeed threshold
		savedValues.motorOverspeedOffTime = savedValuesDefault.motorOverspeedOffTime;	// motor overspeed fault time, in units of about 1/128 sec.
		savedValues.maxBatteryAmperes = savedValuesDefault.maxBatteryAmperes;		// battery amps limit.  Unit is amperes. Must be <= maxMotorAmps 
		savedValues.prechargeTime = savedValuesDefault.prechargeTime;			// precharge time in 0.1 second increments
		savedValues.minAmperesForOverspeed = savedValuesDefault.minAmperesForOverspeed;	// motor current must be > motor_sc_amps to calculate motor speed.  Units are amperes.
		savedValues.maxMotorAmperes = savedValuesDefault.maxMotorAmperes;		// motor amps limit.  Unit is amperes.  Must be >= maxBatteryAmps.
		savedValues.crc = savedValuesDefault.Kp + savedValuesDefault.Ki + savedValuesDefault.throttleLowVoltage + savedValuesDefault.throttleHighVoltage + 
						  savedValuesDefault.throttleFaultVoltage + savedValuesDefault.throttlePositionGain + savedValuesDefault.throttlePWMGain + 
						  savedValuesDefault.currentRampRate + savedValuesDefault.datastreamPeriod + savedValuesDefault.motorOverspeedThreshold + savedValuesDefault.motorOverspeedOffTime +
						  savedValuesDefault.maxBatteryAmperes + savedValuesDefault.prechargeTime + savedValuesDefault.minAmperesForOverspeed + savedValuesDefault.maxMotorAmperes;
	}
}
