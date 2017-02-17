/* Project Fishlight by Oleg Żero 2014. */

#include <asf.h>

/* ATmega88 pin out. */
#define UP 		PB0
#define DOWN 	PB3
#define COLOR 	PB4
#define MODE 	PB5
#define BATTERY PD0

/*Three modes of operation: */
enum OperationMode
{
	AUTO_MODE,
	MANUAL_MODE,
	BATTERY_MODE,
};
uint8_t Mode = 0x00; 		// contains the Operation Mode setting

/*Four output channels*/
enum ColorSelections
{
	RED, GREEN, BLUE, LAMP
};
uint8_t ColorSel = 0x00; 	// contains the selected channel

struct time 				// stores the time (hour, minute, second)
{
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
} daytime;

uint8_t RGB_lvl_auto[4]; 	// stores the channels' levels in auto mode
uint8_t RGB_lvl_manual[4];	// stores the channels' levels in the manual mode

void PortInit(void)
{
	DDRB = 0x06; 			// [tosc2, tosc1, Mode, Color, down, LAMP, RED, up]
	DDRD |= 0x60; 			// [ --- , GREEN, BLUE, ---- , ----, ----, ---, --]
}

/* Configure PCINT[5,4,3,0] for reading the keys. */
/* pins PB: 5,4,3,0 can generate interrupts */
void InterruptInit(void) 
{
	PCICR |= (1 << PCIE0); 	//pins 0-7 allowed to generate interrupts
	PCMSK0 |= (1 << PCINT5) | (1 << PCINT4) | (1 << PCINT3) | (1 << PCINT0);
	sei(); 					//global interrupt enable
}

void TC2Init(void)
{
	ASSR |= (1 << AS2);		 //enable asynch. mode for TC2
	TCCR2B |= (1 << CS22) | (1 << CS20); //div 128, (1 Hz)
	TIMSK2 |= (1 << TOIE2);	//interrupt generated on overflow
	TCNT2 = 0x00; 			//reset the register
	TCCR2A = 0x00;			//normal mode of operation
}

void TC0Init(void)
{
	TCCR0A |= (1 << COM0A1) | (1 << COM0A0); //inverting PWM mode for OC0A
	TCCR0A |= (1 << COM0B1) | (1 << COM0B0); //inverting PWM mode for OC0B
	TCCR0A |= (1 << WGM01)  | (1 << WGM00);  //PWM fast mode enabled
	TCCR0B |= (1 << CS00);					 //no prescaler (f_TC0 = f_I/O / 256)
	PRR &= ~(1 << PRTIM0); 					 //enable TC0 module
}

void TC1Init(void)
{
	TCCR1A |= (1 << COM1A1) | (1 << COM1A0); //inverting PWM mode for OC1A
	TCCR1A |= (1 << COM1B1) | (1 << COM1B0); //inverting PWM mode for OC1B
	TCCR1A |= (1 << WGM10);					 //PWM fast mode enabled (8-bit)
	TCCR1B |= (1 << WGM12); 				 //PWM fast mode enabled (8-bit)
	TCCR1B |= (1 << CS10); 					 //no prescaler (f_TC0 = f_I/O / 256)
	OCR1AH = 0x00; 							 //this part will not be used
	PRR &= ~(1 << PRTIM1);					 //enable TC1 module
}

/* Tracks the time flow (h,m,s) */
void TimeTrack(struct time *t)
{
	if (t->second == 60)
	{
		t->second = 0x00;
		t->minute++;
	}
	if (t->minute == 60)
	{
		t->minute = 0x00;
		t->hour++;
	}
	if (t->hour == 24)
	{
		t->hour = 0x00;
	}
}

void AutomaticModeInit(void)
{
	DDRB |= 0x06; 							 // enable all light channels: power, RED
	DDRD |= 0x60; 							 // enable all light channels: GREEN, BLUE
	PCMSK0 &= ~((1 << PCINT4) | (1 << PCINT3) | (1 << PCINT0)); // disable keys: color, down, up
	PCMSK0 |= (1 << PCINT5); 				 // enable keys: mode
	PRR &= ~((1 << PRTIM1) | (1 << PRTIM0)); // enable TC0 and TC1
	Mode = AUTO_MODE; 						 // set to AUTOMATIC MODE
}
void ManualModeInit(void)
{
	DDRB |= 0x06; 							 // enable all light channels: power, RED
	DDRD |= 0x60; 							 // enable all light channels: GREEN, BLUE
	PCMSK0 |= (1 << PCINT5) | (1 << PCINT4) | (1 << PCINT3) | (1 << PCINT0);
	PRR &= ~((1 << PRTIM1) | (1 << PRTIM0)); // enable TC0 and TC1
	Mode = MANUAL_MODE;						 // set to MANUAL MODE
}
void BatteryModeInit(void)
{
	DDRB &= ~(0x06); 						 // disable all light channels: power, RED
	DDRD &= ~(0x60); 						 // disable all light channels: GREEN, BLUE
	DDRD &= ~(1 << BATTERY); 				 // make sure the BATTERY pin is sensing input
	// disable all keys
	PCMSK0 &= ~((1 << PCINT5) | (1 << PCINT4) | (1 << PCINT3) | (1 << PCINT0));
	PRR |= (1 << PRTIM1) | (1 << PRTIM0); 	 // disable TC0 and TC1
	Mode = BATTERY_MODE; 					 // set to BATTERY OPERATIED MODE
}

void FishLights(void)
{
	if (Mode == AUTO_MODE)
	{
		/* Returns colour temperature index given time */
		uint8_t indexf;
		/* Calculates the time (dec) and shift to start at 0:00 pm not 7:00 am */
		float timedec = (float)(daytime.hour) + (float)(daytime.minute)/60 + (float)(daytime.second)/3600;
		uint32_t timedec2 = timedec*10000;
		/* morning */
		if ((timedec2 >= 0) & (timedec2 <= 35000))
		{
			indexf = timedec2/137;
		}
		/* day */
		if ((timedec2 > 35000) & (timedec2 < 100000))
		{
			indexf = 255;
		}
		/* evening */
		if ((timedec2 >= 100000) & (timedec2 <= 135000))
		{
			indexf = (135000 - timedec2)/137;
		}
		/* night */
			if (timedec2 > 135000)
		{
			indexf = 0;
		}
		/* Loads the channels' values into the buffer */
		RGB_lvl_auto[RED]   = color_temperature[indexf][RED];
		RGB_lvl_auto[GREEN] = color_temperature[indexf][GREEN];
		RGB_lvl_auto[BLUE]  = color_temperature[indexf][BLUE];
		RGB_lvl_auto[LAMP]  = ~indexf;
		/* Adjucts the PWM prescalers for each pin */
		OCR1A  = RGB_lvl_auto[RED];
		OCR0A  = RGB_lvl_auto[GREEN];
		OCR0B  = RGB_lvl_auto[BLUE];
		OCR1BL = RGB_lvl_auto[LAMP];
		/* In case power is down */
		if((PIND & (1 << BATTERY)) == 0)
		{
			BatteryModeInit();
		}
	}
	if (Mode == MANUAL_MODE)
	{
		OCR1AL = ~RGB_lvl_manual[RED];
		OCR0A = ~RGB_lvl_manual[GREEN];
		OCR0B = ~RGB_lvl_manual[BLUE];
		OCR1BL = ~RGB_lvl_manual[LAMP];
		/* In case power is down */
		if((PIND & (1 << BATTERY)) == 0)
		{
			BatteryModeInit();
		}
	}
	if (Mode == BATTERY_MODE)
	{
		/* Check if the power is restored*/
		if((PIND & (1 << BATTERY)) != 0)
		{
			AutomaticModeInit();
		}
	}
}

int main (void)
{
	board_init();
	PortInit();
	InterruptInit();
	TC2Init();
	TC0Init();
	TC1Init();
	AutomaticModeInit(); 	 // starts with automatic settings by default
	while (1)
	{
		TimeTrack(&daytime); // Update on the time
		FishLights(); 		 // Get the correct channel values based on the mode
	}
}

/* Interrupt dedicated to respond to the keys */
ISR(PCINT0_vect)
{
	/* If in the AUTOMATIC MODE */
	if(Mode == AUTO_MODE)
	{
		/* If Mode key is pressed */
		if((PINB & (1 << MODE)) == 0)
		{
			ManualModeInit();
			return;
		}
	}

	/* If in the MANUAL MODE */
	if(Mode == MANUAL_MODE)
	{
		if((PINB & (1 << MODE)) == 0) // Return to the automatic mode
		{
			AutomaticModeInit();
			return;
		}
		if((PINB & (1 << COLOR)) == 0) // Select the colour to alter
		{
			ColorSel = (ColorSel + 1) % 4;
			return;
		}
		if((PINB & (1 << DOWN)) == 0) // Decrease the intensity of the colour
		{
			RGB_lvl_manual[ColorSel] -= 0x01;
			return;
		}
		if((PINB & (1 << UP)) == 0) // Increase the intensity of the colour
		{
			RGB_lvl_manual[ColorSel] += 0x01;
			return;
		}
	}
}

/* Interrupt dedicated to update the time */
ISR(TIMER2_OVF_vect)
{
	daytime.second++;
}
