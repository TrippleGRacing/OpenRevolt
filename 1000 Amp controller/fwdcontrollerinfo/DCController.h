#ifndef CONTROLLER_H
#define CONTROLLER_H
#include "p30F4011.h"
#include "UART4011.h"
#include <libpic30.h>

//#define CONSTANT_PWM
#define DC_CONTROL_DRIVER_27

#define I_TRIS_THROTTLE 		_TRISB0
#define I_TRIS_CURRENT1			_TRISB1
#define I_TRIS_CURRENT2			_TRISB2
#define I_TRIS_TEMPERATURE 		_TRISB7
#define I_TRIS_GLOBAL_FAULT		_TRISD2

#define I_TRIS_UNDERVOLTAGE_FAULT		_TRISC13
#define I_TRIS_DESAT_FAULT		_TRISC14

#define O_TRIS_PRECHARGE_RELAY  _TRISD3 // HIGH means close relay.
#define O_TRIS_CONTACTOR	   	_TRISD0 // HIGH means close contactor.
#define O_TRIS_CLEAR_FLIP_FLOP	_TRISE8
#define O_TRIS_CLEAR_DESAT		_TRISF6
#define O_TRIS_PWM   			_TRISE1  // pwm.  low act high. NOT low act low.


#define I_LAT_THROTTLE 		_LATB0
#define I_LAT_CURRENT1			_LATB1
#define I_LAT_CURRENT2			_LATB2
#define I_LAT_TEMPERATURE 		_LATB7
#define I_LAT_GLOBAL_FAULT		_LATD2
#define I_LAT_UNDERVOLTAGE_FAULT		_LATC13
#define I_LAT_DESAT_FAULT		_LATC14
#define O_LAT_PRECHARGE_RELAY  _LATD3 // HIGH means close relay.
#define O_LAT_CONTACTOR	   	_LATD0 // HIGH means close contactor.
#define O_LAT_CLEAR_FLIP_FLOP	_LATE8
#define O_LAT_CLEAR_DESAT		_LATF6
#define O_LAT_PWM   			_LATE1  // pwm.  low act high. NOT low act low.

#define I_PORT_THROTTLE 		_RB0
#define I_PORT_CURRENT1			_RB1
#define I_PORT_CURRENT2			_RB2
#define I_PORT_TEMPERATURE 		_RB7
#define I_PORT_GLOBAL_FAULT		_RD2
#define I_PORT_UNDERVOLTAGE_FAULT		_LATC13
#define I_PORT_DESAT_FAULT		_RC14
#define O_PORT_PRECHARGE_RELAY  _RD3 // HIGH means close relay.
#define O_PORT_CONTACTOR	   	_RD0 // HIGH means close contactor.
#define O_PORT_CLEAR_FLIP_FLOP	_RE8
#define O_PORT_CLEAR_DESAT		_RF6
#define O_PORT_PWM   			_RE1  // pwm.  low act high. NOT low act low.







#define OVERCURRENT_FAULT (1 << 4)

#define THROTTLE_FAULT (1 << 0)
#define VREF_FAULT (1 << 1)
#define UART_FAULT (1 << 2)
#define UNDERVOLTAGE_FAULT (1 << 3)
#define OC_FAULT (1 << 4)
//#define PRECHARGE_WAIT (1 << 5)
#define MOTOR_OVERSPEED_FAULT (1 << 6)
#define HPL_FAULT (1 << 7)
#define GLOBAL_FAULT (1 << 8)
#define DESAT_FAULT (1 << 9)
#define VREF_FAULT2 (1 << 10)


#define THROTTLE_FAULT_COUNTS 200		// after 200mS, flag a throttle fault.
#define MAX_AMPERES 2000
#define BAUD_RATE 19200
#define DELAY_200MS_SLOW_TIMER 12500
#define THERMAL_CUTBACK_START 670	// 75degC
#define THERMAL_CUTBACK_END 726		// 85degC
// max temperature is 726, which is 85degC.

typedef struct {
	unsigned Kp;								// PI loop proportional gain
	unsigned Ki;								// PI loop integreal gain
	unsigned throttleLowVoltage;		// throttle low voltage ()
	unsigned throttleHighVoltage;		// throttle high voltage ()
	unsigned throttleFaultVoltage;		// throttle fault voltage.  Too low of voltage (to protect from disconnected throttle.
	unsigned throttlePositionGain;		// gain for actual throttle position
	unsigned throttlePWMGain;			// gain for pwm (voltage)
	int currentRampRate;				// 0-8.  8 means zero seconds to max current. 7 means 1 second to max current ... 0 means 8 seconds to max current.
	unsigned datastreamPeriod;			// real time data period
	unsigned motorOverspeedThreshold;	// motor overspeed threshold
	unsigned motorOverspeedOffTime;		// motor overspeed fault time, in units of about 1/128 sec.
	unsigned maxBatteryAmperes;			// battery amps limit.  Unit is amperes. Must be <= maxMotorAmps 
	unsigned prechargeTime;				// precharge time in 0.1 second increments
	unsigned minAmperesForOverspeed;	// motor current must be > motor_sc_amps to calculate motor speed.  Units are amperes.
	unsigned maxMotorAmperes;			// motor amps limit.  Unit is amperes.  Must be >= maxBatteryAmps.
	unsigned crc;
} SavedValuesStruct;

typedef struct {
	int throttle_ref;
	int current_ref;
	int current_fb;
	unsigned raw_hs_temp;
	unsigned raw_throttle;
	unsigned battery_amps;
	unsigned long battery_ah;
	unsigned int pwmDuty;
} realtime_data_type;

#endif
