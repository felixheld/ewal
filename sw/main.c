#define F_CPU	8000000UL

#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "i2cmaster.h"

struct led_t	// LED
{
	uint8_t o : 1;
	uint8_t l : 1;
	uint8_t m : 1;
	uint8_t r : 1;
	uint8_t u : 1;
} leds;

struct led_i	// Led intensity for PWM purposes
{
	int8_t ou;
	int8_t lr;
} ledi;

//set the led output ports to the state in the global variable leds
void update_led(){
	uint8_t temp = 0;
	temp |= leds.m?0x01:0;
	temp |= leds.r?0x02:0;
	temp |= leds.o?0x04:0;
	PORTB = (PORTB & ~0x07) | temp;

	temp = 0;
	temp |= leds.u?0x40:0;
	temp |= leds.l?0x80:0;
	PORTD = (PORTD & ~0xc0) | temp;
}

#define THRESHOLD_VALUE	10
#define VALMUL 3

//this code only implements the case where the pcb is lies on a flat surface
void compute_led_state(int16_t x, int16_t y, int16_t z){


	int16_t x_abs = x<0?-x:x;
	int16_t y_abs = y<0?-y:y;
	int16_t z_abs = z<0?-z:z;
	
	if(x_abs > z_abs && x_abs > y_abs)	// Wall	
	{
		if(x>0)
		{
			x = -z;
			y = 0;	// Disable y-axis, as that would only measure the rotation of the device
		}
		else	// x < 0
		{
			x = z;
			y = 0;
		}
	}
	else if(y_abs > x_abs && y_abs > z_abs)	// Wall
	{
		if(y > 0)
		{
			y = -z;	// Disable x-axis, as that would only measure the rotation of the device
			x = 0;
		}
		else	// y < 0
		{
			y = z;
			x = 0;	
		}
	}


	leds.m = 1;
	if(x > THRESHOLD_VALUE){
		leds.m = 0;
	}
	if(x < -THRESHOLD_VALUE){
		leds.m = 0;
	}
	if(y > THRESHOLD_VALUE){
		leds.m = 0;
	}
	if(y < -THRESHOLD_VALUE){
		leds.m = 0;
	}
	
	
	if(-x > INT8_MAX)
	{
		ledi.ou = INT8_MAX;
	}
	else if (-x < INT8_MIN)
	{
		ledi.ou = -100;
	}
	else
	{
		ledi.ou = -x;
	}
	
	if(y > INT8_MAX)
	{
		ledi.lr = 100;
	}
	else if (y < INT8_MIN)
	{
		ledi.lr = -100;
	}
	else
	{
		ledi.lr = y;
	}

		
	//update_led();
}

#define BUFDEPTH 16	// Depth of the ringbuffer for averaging purposes
void filter(uint16_t x, uint16_t y, uint16_t z)
{
	static int16_t counter = 0;
	static int16_t xbuf[BUFDEPTH];
	static int16_t ybuf[BUFDEPTH];
	static int16_t zbuf[BUFDEPTH];
	int32_t xt = 0;
	int32_t yt = 0;
	int32_t zt = 0;

	xbuf[counter] = x;
	ybuf[counter] = y;
	zbuf[counter] = z;
	if(++counter >= BUFDEPTH)
	{
		counter = 0;
	}

	for(uint8_t i = 0; i < BUFDEPTH; i++)
	{
		xt += xbuf[i];
		yt += ybuf[i];
		zt += zbuf[i];
	}

	x = xt/BUFDEPTH;
	y = yt/BUFDEPTH;
	z = zt/BUFDEPTH;
	
	compute_led_state(x,y,z);
}

//read the 6 channel data registers and pass them to the compute_led_state function
void read_and_process_sensor_data(){
	uint8_t temp[6];

	i2c_start_wait(0x3a);
	i2c_write(0x01);
	i2c_rep_start(0x3b);
	int i;
	for(i = 0; i<6; i++){
		if(i == 5){
			temp[i] = i2c_readNak();
		}else{
			temp[i] = i2c_readAck();
		}
	}

	int16_t x, y, z;

	x = (temp[0] << 2) | (temp[1]>>6);
	x |= x&0x0200?0xFC00:0;	//sign extension

	y = (temp[2] << 2) | (temp[3]>>6);
	y |= y&0x0200?0xFC00:0;

	z = (temp[4] << 2) | (temp[5]>>6);
	z |= z&0x0200?0xFC00:0;
	
	filter(x,y,z);
}

int main(){
	//set led output ports
	DDRB |= 0x07;
	DDRD |= 0xC0;

	// Initiate timer0
	TCCR0A = (0 << CS02) | (1 << CS01) | (1 << CS00);
	TIMSK0 = (1 << OCIE0A) | (1 << OCIE0B) | (1 << TOIE0);

	sei();	// Enable global interrupts

	leds.o = 1;
	leds.l = 1;
	leds.m = 1;
	leds.r = 1;
	leds.u = 1;
	update_led();

	_delay_ms(5);

	i2c_init();


	leds.o = 0;
	leds.l = 0;
	leds.m = 0;
	leds.r = 0;
	leds.u = 0;
	update_led();

	//activate sensor
	i2c_start_wait(0x3A);
	i2c_write(0x2a);
	i2c_write(0x01);
	i2c_stop();

	uint8_t temp;

	while(1)
	{
		//get sensor status
		i2c_start_wait(0x3a);
		i2c_write(0x00);
		i2c_rep_start(0x3b);
		temp = i2c_readNak();
		i2c_stop();
		
		//only read and process sensor data, if all channels have new values
		if((temp & 0x07) == 0x07){
			read_and_process_sensor_data();
		}
	}

	return 0;
}


ISR(TIMER0_OVF_vect)
{
	leds.o = (ledi.ou > 0)?1:0;
	leds.l = (ledi.lr < 0)?1:0;
	leds.r = (ledi.lr > 0)?1:0;
	leds.u = (ledi.ou < 0)?1:0;
	update_led();

	OCR0A = ((ledi.ou > 0)?ledi.ou:-ledi.ou);
	OCR0B = ((ledi.lr > 0)?ledi.lr:-ledi.lr);
}

ISR(TIMER0_COMPA_vect)	// Takes control of l/r LEDs
{
	leds.o = 0;
	leds.u = 0;
	update_led();
}

ISR(TIMER0_COMPB_vect)	// Takes control of u/o LEDs
{
	leds.l = 0;
	leds.r = 0;
	update_led();
}
