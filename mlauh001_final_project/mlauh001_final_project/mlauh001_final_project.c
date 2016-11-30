#include <stdint.h> 
#include <stdlib.h> 
#include <stdio.h> 
#include <stdbool.h> 
#include <string.h> 
#include <math.h> 
#include <avr/io.h> 
#include <avr/interrupt.h> 
#include <avr/eeprom.h> 
#include <avr/portpins.h> 
#include <avr/pgmspace.h> 
#include <util/delay.h>

#include "usart_ATmega1284.h"
 
//FreeRTOS include files 
#include "FreeRTOS.h" 
#include "task.h" 
#include "croutine.h" 

enum RotateState {Rotate_INIT, Rotate_Wait, go_to_drink} rotate_state;
enum DispenseState {Dispense_INIT, Dispense_Wait, Dispense_Up, Dispense_Hold, Dispense_Down} dispense_state;
enum PollUSARTState {PollUSART_INIT, PollUSART_Wait} poll_usart_state;
enum MakeDrinkState {MakeDrink_INIT, MakeDrink_Wait, MakeDrink, MakeDrink_Rotate, MakeDrink_Dispense} make_drink_state;
enum WritePORTAState {WritePORTA_INIT, WritePORTA_Wait} write_porta_state;
	
#define A 0x01
#define B 0x02
#define C 0x04
#define D 0x08

#define AB 0x03
#define BC 0x06
#define CD 0x0C
#define DA 0x09

unsigned char forward_steps[] = {AB, BC, CD, DA};
unsigned char backward_steps[] = {DA, CD, BC, AB};
unsigned char num_steps = (sizeof(forward_steps)/sizeof(forward_steps[0]));

unsigned char cnt = 0; //index in array of steps 0 through 3
unsigned short total_cnt = 0; //count from 0 to total number of steps to next drink
unsigned char next_drink = 110; //number of steps to adjacent bottle
unsigned char current_position = 0; // current bottle position 0 through 5
unsigned short steps_next_drink = 0; // total number of steps to next drink
unsigned char drink = 0; //selected drink
unsigned char drink_select = 0; //button selected drink on PORTB
unsigned char lcd_str; // first byte from usart
unsigned char received = 0; //second byte from usart

unsigned char rotate_stepper = 0, linear_stepper = 0;

unsigned short dispense_cnt = 0;
unsigned short dispense_totalcnt = 660;
unsigned char dispense_index = 0;
unsigned short dispense_hold = 125; //dispense time multplied by 8, 250 * 8 = 2000, 2 seconds
unsigned short dispense_hold_cnt = 0;

unsigned char received_message = 0;
unsigned char make_drink_flag = 0;
unsigned char drink_to_make = 0;

unsigned char drinks[6][6] = {{1,0,0,0,0,0},{1,1,0,0,1,0},{0,0,1,0,1,1},{1,0,0,0,1,0},{0,1,0,1,0,1},{1,0,0,1,0,1}};
unsigned char dispense_flag = 0;
unsigned char rotate_flag = 0;
unsigned char make_drink_cnt = 0;

/*
void USART_LCD_Receive(unsigned char usartNum){
	unsigned char count = 0;
	unsigned char rec = USART_Receive(usartNum);
	command[count++] = rec;	
	while (rec != 0xFF) {
		//if(USART_HasReceived(0)) {
			rec = USART_Receive(0);
			command[count++] = rec;	
		//}
	}
	PORTC = count;
	command[count] = '\0';
}
*/

const char * UART_Return_String(unsigned char usartNum) {
	unsigned char string[20], x, i = 0;

	//receive the characters until ENTER is pressed (ASCII for ENTER = 13)
	while((x = USART_Receive(usartNum)) != 0xFF) {
		//and store the received characters into the array string[] one-by-one
		string[i++] = x;
	}

	//insert NULL to terminate the string
	string[i] = '\0';

	//return the received string
	return string;
}
/*
unsigned char* USART_LCD_Receive(unsigned char usartNum){
	unsigned char arr[10], i = 0, j = 0, rec = 0;
	while(i < 3) {
		rec = USART_Receive(usartNum);
		if (rec == 0xFF) {
			i++;
		}
		else {
			arr[j] = rec;
			j++;
		}
	}
	char *str = (char *) malloc(sizeof(char) * j+1);
	for(unsigned char k = 0; k < j; k++){
		str[k] = arr[k];
	}
	str[j] = '\0';
	return str;
}
*/
unsigned char USART_Send_String(unsigned char* string, unsigned char usartNum){
	unsigned char str_size = strlen(string);
	for(unsigned char i = 0; i < str_size; i++){
		//while(USART_IsSendReady(usartNum)==0) {};
		USART_Send(string[i], usartNum);
	}
	for(unsigned char i = 0; i < 3; i++){
		//while(USART_IsSendReady(usartNum)==0) {};
		USART_Send(0xFF,usartNum);
	}
}

void Rotate_Init(){
	rotate_state = Rotate_INIT;
}

void Dispense_Init(){
	dispense_state = Dispense_INIT;
}

void WritePORTA_Init() {
	write_porta_state = WritePORTA_INIT;
}

void PollUSART_Init() {
	poll_usart_state = PollUSART_INIT;
}

void MakeDrink_Init() {
	make_drink_state = MakeDrink_INIT;
}

void Rotate_Tick(){
	//USART_Send_String("page page1",0);
	//Actions
	switch(rotate_state){
		case Rotate_INIT:
			//PORTA = 0;
			rotate_stepper = 0;
			current_position = 0;
			break;
		case Rotate_Wait:
			cnt = 0; //index in list of steps
			total_cnt = 0; //count of total number of steps
			//drink = 0; //drink selected
			drink_select = ~PINB & 0x3F;
			/*lcd_str = 0;
			received = USART_HasReceived(0);
			if(received) {
				lcd_str = USART_Receive(0);
				//USART_Send_String(lcd_str,1);
			}
			*/
			break;
		case go_to_drink:
			if (current_position < drink) {
					//forward	
					//PORTA = forward_steps[cnt++];
					rotate_stepper = forward_steps[cnt++];
					cnt %= num_steps;
					total_cnt++;
			}	
			else if (current_position > drink) {
					//backward
					//PORTA = backward_steps[cnt++];
					rotate_stepper = backward_steps[cnt++];
					cnt %= num_steps;
					total_cnt++;
			}
			break;
		default:
			//PORTA = 0;
			rotate_stepper = 0;
			break;
	}

	//Transitions
	//----------------------------
	switch(rotate_state){
		case Rotate_INIT:
			rotate_state = Rotate_Wait;
			break;
		case Rotate_Wait:
			if (drink_select) {
				if (drink_select == 0x01) { drink = 0;}
				else if (drink_select == 0x02) { drink = 1;}
				else if (drink_select == 0x04) { drink = 2;}
				else if (drink_select == 0x08) { drink = 3;}
				else if (drink_select == 0x10) { drink = 4;}
				else if (drink_select == 0x20) { drink = 5;}
				//PORTC = drink;
				steps_next_drink = abs(next_drink * (drink - current_position));
				rotate_state = go_to_drink;
			}
			else if (rotate_flag == 0x01) {
					steps_next_drink = abs(next_drink * (drink - current_position));
					rotate_state = go_to_drink;
					PORTC = drink;
			}
			else {
				rotate_state = Rotate_Wait;
			}
			break;
		case go_to_drink:
			if (total_cnt < steps_next_drink) {
				rotate_state = go_to_drink;
			}
			else {
				current_position = drink;
				rotate_flag = 0x00;
				rotate_state = Rotate_Wait;
			}
			break;
		
		default:
			rotate_state = Rotate_INIT;
			break;
	}
}

void Dispense_Tick(){
	switch(dispense_state){ //actions
		case Dispense_INIT:
			break;
		case Dispense_Wait:
			dispense_cnt = 0;
			dispense_index = 0;
			dispense_hold_cnt = 0;
			break;
		case Dispense_Up:
			linear_stepper = backward_steps[dispense_index++];
			dispense_index %= num_steps; 
			dispense_cnt++;
			break;
		case Dispense_Hold:
			dispense_hold_cnt++;
			break;
		case Dispense_Down:
			linear_stepper = forward_steps[dispense_index++];
			dispense_index %= num_steps;
			dispense_cnt++;
			break;
		default:
			break;
	}
	switch(dispense_state){ //transition
		case Dispense_INIT:
			dispense_state = Dispense_Wait;
			break;
		case Dispense_Wait:
			if ((~PINB & 0x40)==0x40) { // if PINB6 is set to low, dispense
				//PORTC = 0xFF;
				dispense_state = Dispense_Up;	
				dispense_cnt = 0;
			}
			else if (dispense_flag == 0x01) {
				dispense_state = Dispense_Up;
				dispense_cnt = 0;
			}
			else {
				dispense_state = Dispense_Wait;
			}
			break;
		case Dispense_Up:
			if (dispense_cnt >= dispense_totalcnt) {
				dispense_state = Dispense_Hold;
				dispense_cnt = 0;
			}
			else {
				dispense_state = Dispense_Up;
			}
			break;
		case Dispense_Hold:
			if (dispense_hold_cnt >= dispense_hold) {
				dispense_state = Dispense_Down;
				dispense_hold_cnt = 0;
				dispense_cnt = 0;
			}
			else {
				dispense_state = Dispense_Hold;
			}
			break;
		case Dispense_Down:
			if (dispense_cnt >= dispense_totalcnt) {
				dispense_state = Dispense_Wait;
				dispense_hold_cnt = 0;
				dispense_cnt = 0;
				dispense_flag = 0;
			}
			else {
				dispense_state = Dispense_Down;
			}
			break;
		default:
			dispense_state = Dispense_Wait;
			break;	
	}
}

void WritePORTA_Tick() {
	switch (write_porta_state){ //actions
		case WritePORTA_INIT:
			break;
		case WritePORTA_Wait:
			break;
		default:
			break;
	}
	switch (write_porta_state){ //transitions
		case WritePORTA_INIT:
			PORTA = 0;
			write_porta_state = WritePORTA_Wait;
			break;
		case WritePORTA_Wait:
			PORTA = rotate_stepper | (linear_stepper << 4);
			write_porta_state = WritePORTA_Wait;
			break;
		default:
			write_porta_state = WritePORTA_Wait;
			PORTA = 0;
			break;	
	}
}

void PollUSART_Tick() {
	switch (poll_usart_state) { //actions
		case PollUSART_INIT:
			break;
		case PollUSART_Wait:
			if (USART_HasReceived(0)) {
				received_message = USART_Receive(0);	
				if (received_message == 0xAA) {
					make_drink_flag = 0x01;
					drink_to_make = USART_Receive(0);
					USART_Flush(0);
				}
				else {
					make_drink_flag = 0x00;
				}
			}
			break;
		default:
			break;
	}
	switch (poll_usart_state) { //transitions
		case PollUSART_INIT:
			poll_usart_state = PollUSART_Wait;
			received_message = 0;
			break;
		case PollUSART_Wait:
			poll_usart_state = PollUSART_Wait;
			break;
		default:
			poll_usart_state = PollUSART_Wait;
			break;
	}
}

void MakeDrink_Tick() {
	switch (make_drink_state) { //actions
		case MakeDrink_INIT:
			break;
		case MakeDrink_Wait:
			make_drink_cnt = 0;
			break;
		case MakeDrink:
			break;
		case MakeDrink_Rotate:
			break;
		case MakeDrink_Dispense:
			break;
		default:
			break;
	}
	switch (make_drink_state) { //transitions
		case MakeDrink_INIT:
			make_drink_state = MakeDrink_Wait;
			make_drink_cnt = 0;
			rotate_flag = 0;
			dispense_flag = 0;
			break;
		case MakeDrink_Wait:
			if (make_drink_flag == 0x01 && rotate_flag == 0 && dispense_flag == 0) {
				USART_Send_String("page page2",0);
				make_drink_state = MakeDrink;
			}
			else {
				make_drink_state = MakeDrink_Wait;
			}
			break;
		case MakeDrink:
			if (drinks[drink_to_make][make_drink_cnt]==0x01 && make_drink_cnt < 6) {
				make_drink_state = MakeDrink_Rotate;
				rotate_flag = 0x01;
				drink = make_drink_cnt;
			}
			else if (make_drink_cnt >= 6) {
				make_drink_state = MakeDrink_Wait;
				USART_Send_String("page page0",0);
				make_drink_flag = 0x00;
				rotate_flag = 0x01;
				drink = 0;
			}
			else {
				make_drink_cnt++;
				make_drink_state = MakeDrink;
			}
			break;
		case MakeDrink_Rotate:
			if (rotate_flag == 0x00) {
				dispense_flag = 0x01;
				make_drink_state = MakeDrink_Dispense;
			}
			else {
				make_drink_state = MakeDrink_Rotate;
			}
			break;
		case MakeDrink_Dispense:
			if (dispense_flag == 0x00) {// && make_drink_cnt < 6) {
				make_drink_cnt++;
				make_drink_state = MakeDrink;
			}
			/*
			else if (dispense_flag == 0x00 && make_drink_cnt >= 6) {
				make_drink_state = MakeDrink_Wait;
				USART_Send_String("page page0",0);
				make_drink_flag = 0x00;
				rotate_flag = 0x01;
				drink = 0;
			}
			*/
			else {
				make_drink_state = MakeDrink_Dispense;
			}
			break;
		default:
			make_drink_state = MakeDrink_Wait;
			break;
	}
}

void RotateSecTask() {
	Rotate_Init();
	for(;;) { 	
		Rotate_Tick();
		vTaskDelay(6); 
	} 
}

void DispenseSecTask() {
	Dispense_Init();
	for(;;) { 	
		Dispense_Tick();
		vTaskDelay(8); 
	} 
}

void WritePORTASecTask() {
	WritePORTA_Init();
	for(;;) { 	
		WritePORTA_Tick();
		vTaskDelay(1); 
	}
}

void PollUSARTSecTask() {
	PollUSART_Init();
	for(;;) { 	
		PollUSART_Tick();
		vTaskDelay(1); 
	}
}

void MakeDrinkSecTask() {
	MakeDrink_Init();
	for(;;) { 	
		MakeDrink_Tick();
		vTaskDelay(1); 
	}
}

void StartSecPulse(unsigned portBASE_TYPE Priority) {
	xTaskCreate(RotateSecTask, (signed portCHAR *)"RotateSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(DispenseSecTask, (signed portCHAR *)"DispenseSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(WritePORTASecTask, (signed portCHAR *)"WritePORTASecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(PollUSARTSecTask, (signed portCHAR *)"PollUSARTSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
	xTaskCreate(MakeDrinkSecTask, (signed portCHAR *)"MakeDrinkSecTask", configMINIMAL_STACK_SIZE, NULL, Priority, NULL );
}

int main(void) { 
	DDRA = 0xFF; PORTA = 0x00;//Set PORTA as output
	DDRB = 0x00; PORTB = 0xFF;//Set PORTB as input
	DDRC = 0xFF; PORTC = 0x00;//Set PORTA as output
	
	initUSART(0);
	initUSART(1);
	//Start Tasks  
	StartSecPulse(1);
	//RunSchedular 
	vTaskStartScheduler(); 
 
	return 0; 
}