#define F_CPU	8000000UL

#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "i2cmaster.h"

struct led_t {
		unsigned char o : 1;
		unsigned char l : 1;
		unsigned char m : 1;
		unsigned char r : 1;
		unsigned char u : 1;
} leds;

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

//this code only implements the case where the pcb is lies on a flast surface
void compute_led_state(int16_t x, int16_t y, int16_t z){

	leds.o = 0;
	leds.l = 0;
	leds.m = 0;
	leds.r = 0;
	leds.u = 0;

	int16_t x_abs = x<0?-x:x;
	int16_t y_abs = y<0?-y:y;
	int16_t z_abs = z<0?-z:z;

	if((z_abs > x_abs) && (z_abs > y_abs)){
		if(z > 0){
			leds.m = 1;
			if(x > THRESHOLD_VALUE){
				leds.u = 1;
				leds.m = 0;
			}
			if(x < -THRESHOLD_VALUE){
				leds.o = 1;
				leds.m = 0;
			}
			if(y > THRESHOLD_VALUE){
				leds.r = 1;
				leds.m = 0;
			}
			if(y < -THRESHOLD_VALUE){
				leds.l = 1;
				leds.m = 0;
			}
		}
	}

	update_led();
}

//read the 6 channel date registers and pass them to the compute_led_state function
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

	compute_led_state(x,y,z);
}

int main(){
	//set led output ports
	DDRB |= 0x07;
	DDRD |= 0xC0;

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

	while(1){
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
