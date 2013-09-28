#include <p18f13k22.h>

//LED location definitions
#define LED0 LATCbits.LATC0
#define LED1 LATCbits.LATC1
#define LED2 LATCbits.LATC2
#define LED3 LATCbits.LATC4
#define LED4 LATCbits.LATC5
#define LED5 LATCbits.LATC6
#define LED6 LATCbits.LATC7
#define LED7 LATBbits.LATB4
#define LED8 LATBbits.LATB5
#define LED9 LATBbits.LATB6
#define LEDTris1 TRISC
#define LEDTris2 TRISB

//Function prototypes
void ADC_init(void);
void ADC_process(void);
void ADC_switch(unsigned char);

void Comparator_init(void);
void Comparator_process(void);

void Timer_Process(void);

void LED_initPWM(void);
void LED_PWMProcess(void);
void LED_PWMFrameProcess(void); 
void LED_onPWM(char);
void LED_offPWM(char);

void LED_init(void);
void LED_allOff(void);
void LED_allOn(void);
void LED_on(char);
void LED_off(char);

//LED management variables
int brightness[10];
unsigned char status[10];
int currentBrightness;
int frameCount;

//ADC variables
unsigned char ADC_result;
unsigned char ADC_status;
int ADC_val_buffer[13];
int ADC_avg;
char ADC_count;

//Battery Monitoring
unsigned char battery_down;

//Timing out
unsigned char timeOff;

void main() {
	int i = 0;
	int j = 0;

	//Sort out the clock, 64Mhz
	OSCCON = 0b01110000;
	OSCTUNE = 0b11000000;

	LED_init();
	LED_initPWM();
	ADC_init();
	Comparator_init();
	timeOff = 0;

	for(i = 0; i < 10000; i++) {
		for(j = 0; j < 100; j++) {}
	}

	i = 0;

	for(;;) {
		LED_PWMProcess();
		ADC_process();
		Timer_Process();
		Comparator_process();
	
		if(battery_down == 1) {
			LED_allOff();
			Sleep();
			PIE2 = 0b01000000;
			INTCON = 0b01000000;
			PIR2 = 0b00000000;
		} else if(ADC_status == 2) {
			ADC_val_buffer[12] = ADC_val_buffer[11];
			ADC_val_buffer[11] = ADC_val_buffer[10];
			ADC_val_buffer[10] = ADC_val_buffer[9];
			ADC_val_buffer[9] = ADC_val_buffer[8];
			ADC_val_buffer[8] = ADC_val_buffer[7];
			ADC_val_buffer[7] = ADC_val_buffer[6];
			ADC_val_buffer[6] = ADC_val_buffer[5];
			ADC_val_buffer[5] = ADC_val_buffer[4];
			ADC_val_buffer[4] = ADC_val_buffer[3];
			ADC_val_buffer[3] = ADC_val_buffer[2];
			ADC_val_buffer[2] = ADC_val_buffer[1];
			ADC_val_buffer[1] = ADC_val_buffer[0];
			ADC_val_buffer[0] = ADC_result;

			ADC_avg =	ADC_val_buffer[12] + ADC_val_buffer[11] +
						ADC_val_buffer[10] + ADC_val_buffer[9] + ADC_val_buffer[8] + 
						ADC_val_buffer[7] +	ADC_val_buffer[6] + ADC_val_buffer[5] +
						ADC_val_buffer[4] + ADC_val_buffer[3] + ADC_val_buffer[2] +
						ADC_val_buffer[1] +	ADC_val_buffer[0];
			ADC_avg = ADC_avg / 13;

			Timer_Process();
			
			if(timeOff == 0) {				
				for(i = 0; ADC_avg > 132; i++) {
					ADC_avg = ADC_avg - 11;
					LED_onPWM(i);
	 			}
				for(i; i < 10; i++) {
					LED_offPWM(i);
				}
			} else {
				LED_allOff();
			}

			ADC_status = 0;
		}
	}
}

//OTHER FUNCTIONS

void ADC_init() {
	//Set up RA0 as an analog input,
	//all that other fun stuff
	int i = 0;

	ANSELbits.ANS0 = 1;
	TRISAbits.TRISA0 = 1;
	ADCON1 = 0b00000000;
	ADCON2 = 0b00010111;
	ADCON0 = 0b00000001;

	ADC_result = 0;
	ADC_status = 0; //No operation in progress

	for(i = 0; i < 13; i++) {
		ADC_val_buffer[i] = 0;
	}

	ADC_avg = 0;
	ADC_count = 0;
}

void ADC_process() {
	//If there isn't an operation in progress...
	if(ADC_status == 0) {
		ADC_count++;
		if(ADC_count == 5) {
			//Start an operation
			ADCON0bits.GO = 1;
			ADC_status = 1;
			ADC_count = 0;
		}
	} else if(ADC_status == 1) {
		//If the operation is over, take care of bidniz
		if(ADCON0bits.GO == 0) {
			ADC_status = 2;
			ADC_result = ADRESH;
		}
	}
}

void Comparator_init() {
	TRISAbits.TRISA1 = 1;

	//Set up VREF
	VREFCON0 = 0b10110000;
	VREFCON1 = 0b11001000;
	VREFCON2 = 0b00011100;

	//Set up the comparator
	CM1CON0 = 0b10010100;

	//Set up interrupts
	PIE2 = 0b01000000;
	INTCON = 0b01000000;

	battery_down = 0;
}

void Comparator_process() {
	int i = 0;
	if(battery_down == 0) {
		if((CM1CON0 & 0b01000000) == 0b00000000) {
			battery_down = 1;
			VREFCON0 = 0b00110000;
			VREFCON1 = 0b01001000;
			CM1CON0 = 0b00010100;

			for(i = 0; i < 100; i++) {}

			VREFCON0 = 0b10110000;
			VREFCON1 = 0b11001000;
			VREFCON2 = 0b00011111; //has to go back above 31
			CM1CON0 = 0b10010100;
		}
	} else {
		if((CM1CON0 & 0b01000000) == 0b01000000) {
			battery_down = 0;
			VREFCON0 = 0b00110000;
			VREFCON1 = 0b01001000;
			CM1CON0 = 0b00010100;

			for(i = 0; i < 100; i++) {}

			VREFCON0 = 0b10110000;
			VREFCON1 = 0b11001000;
			VREFCON2 = 0b00011100; //has to go back below 28
			CM1CON0 = 0b10010100;
		}
	}
}

void Timer_Process() {
	if(ADC_result < ADC_val_buffer[2]) {
		if(ADC_avg < 145) {
			if(frameCount > 1000) {
				timeOff = 1;
			}
		}	
	} else {
		if(ADC_avg > 175) {
			timeOff = 0;
			frameCount = 0;
		}
	}
}

void LED_initPWM() {
	int i = 0;
	currentBrightness = 0;
	frameCount = 0;

	for(i = 0; i < 10; i++) {
		brightness[i] = 0;
		status[i] = 0;
	}
}

void LED_PWMProcess() {
	int i = 0;

	if(timeOff == 1) {
		return;
	}

	for(i = 0; i < 10; i++) {
		if((brightness[i] >= currentBrightness) && (brightness[i] != 0)) {
			LED_on(i);
		} else {
			LED_off(i);
		}
	}

	currentBrightness = currentBrightness + 1;
	if(currentBrightness == 255) {
		currentBrightness = 0;
		frameCount++;
		LED_PWMFrameProcess();
	}

	return;
}

void LED_PWMFrameProcess() {
	int i = 0;

	for(i = 0; i < 10; i++) {
		switch(status[i]) {
			case 0: //dead
				LED_off(i);
				break;
			case 1: //alive
				LED_on(i);
				break;
			case 2: //dying
				if(brightness[i] == 0) {
					status[i] = 0;
				}
				brightness[i]--;
				break;
			case 3: //born
				if(brightness[i] == 255) {
					status[i] = 1;
				}
				brightness[i]++;
				break;
			default: break;
		}
	}
}

void LED_onPWM(char ledNum) {
	if(status[ledNum] != 1) {
		status[ledNum] = 3;
	}
}

void LED_offPWM(char ledNum) {
	if(status[ledNum] != 0) {
		status[ledNum] = 2;
	}
}

void LED_allOff() {
	LED0 = 0;
	LED1 = 0;
	LED2 = 0;
	LED3 = 0;
	LED4 = 0;
	LED5 = 0;
	LED6 = 0;
	LED7 = 0;
	LED8 = 0;
	LED9 = 0;
}

void LED_allOn() {
	LED0 = 1;
	LED1 = 1;
	LED2 = 1;
	LED3 = 1;
	LED4 = 1;
	LED5 = 1;
	LED6 = 1;
	LED7 = 1;
	LED8 = 1;
	LED9 = 1;
}

void LED_on(char ledNum) {
	switch(ledNum) {
		case 0: LED0 = 1; break;
		case 1: LED1 = 1; break;
		case 2: LED2 = 1; break;
		case 3: LED3 = 1; break;
		case 4: LED4 = 1; break;
		case 5: LED5 = 1; break;
		case 6: LED6 = 1; break;
		case 7: LED7 = 1; break;
		case 8: LED8 = 1; break;
		case 9: LED9 = 1; break;
		default: break;
	}
}

void LED_off(char ledNum) {
	switch(ledNum) {
		case 0: LED0 = 0; break;
		case 1: LED1 = 0; break;
		case 2: LED2 = 0; break;
		case 3: LED3 = 0; break;
		case 4: LED4 = 0; break;
		case 5: LED5 = 0; break;
		case 6: LED6 = 0; break;
		case 7: LED7 = 0; break;
		case 8: LED8 = 0; break;
		case 9: LED9 = 0; break;
		default: break;
	}
}

void LED_init() {
	LEDTris1 = 0x00;
	LEDTris2 = 0x00;

	LED_allOff();
}