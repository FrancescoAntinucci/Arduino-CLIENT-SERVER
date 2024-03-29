#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>

#include "logger.h"

// ==============================================================
// ========================= STRUCT =============================
// ==============================================================

DistSensor* dist_sensor;
WaterSensor* water_sensor;
TempSensor* temp_sensor;
LogStruct* log_struct;
Buffer* buffer;

void struct_init(DistSensor* dist_sensor, WaterSensor* water_sensor, TempSensor* temp_sensor, LogStruct* log_struct, Buffer* buffer) {
	dist_sensor->desc  = (char*) malloc(16);
	water_sensor->desc = (char*) malloc(16);
	temp_sensor->temp_desc = (char*) malloc(16);
	temp_sensor->hum_desc  = (char*) malloc(16);
	
	dist_sensor->desc = "Distanza";
	water_sensor->desc = "Profondità";
	temp_sensor->temp_desc = "Temperatura";
	temp_sensor->hum_desc = "Umidità";
	
	dist_sensor->data = 0.0;
	water_sensor->to = 0.0;
	temp_sensor->temp_data = 0.0;
	temp_sensor->hum_data = 0.0;
	
	dist_sensor->echo_bit = PORTB4;
	dist_sensor->trig_bit = PORTB3;
	water_sensor->analog_bit = PORTC0;
	temp_sensor->digital_bit = PORTD6;
	
	log_struct->ds = dist_sensor;
	log_struct->ws = water_sensor;
	log_struct->ts = temp_sensor;
	
	log_struct->read_count = 0;
	log_struct->write_count = 0;
	log_struct->type = GetAll;
	
	buffer->data = malloc(4);
	buffer->size = 4;
	buffer->next = 0;
}

void reset_buffer(Buffer* b) {
	memset(((char*) b->data), 0, strlen(b->data) + 1);
	buffer->next = 0;
}

void reserve_space(Buffer* buffer, int bytes) {
    if((buffer->next + bytes) > buffer->size) {
        buffer->data = realloc(buffer->data, buffer->size * 2);
        buffer->size *= 2;
    }
}

void serialize_string(char* str, Buffer* b) {
	int len = strlen(str);
	reserve_space(b, len+2);
	memcpy(((char*) b->data) + b->next, str, len);
	memcpy(((char*) b->data) + b->next + len, "  ", 2);
	b->next += len+2;
}

void serialize_float(float x, FloatType float_type, Buffer* b) {
	char str[8];
	dtostrf(x, 4, 2, str);
	
	int len = strlen(str);
	reserve_space(b, len+5);
	memcpy(((char*) b->data) + b->next, str, len);
    
	switch(float_type) {
		case DistFloat:
		case WaterTo:
			memcpy(((char*) b->data) + b->next + len, " cm\n", 4);
			b->next += len+4;
			break;
		
		case TempFloat:
			memcpy(((char*) b->data) + b->next + len, " °C\n", 5);
			b->next += len+5;
			break;
			
		case HumFloat:
			memcpy(((char*) b->data) + b->next + len, " %\n", 3);
			b->next += len+3;
			break;
		
		default:
			memcpy(((char*) b->data) + b->next + len, "\n", 1);
			b->next += len+1;
			break;
	}
}

void packet_serialize(LogStruct* ls, Buffer* b) {
	switch(ls->type) {
		case GetDist:
			serialize_string(ls->ds->desc, b);
			serialize_float(ls->ds->data, DistFloat, b);
			break;
		
		case GetWater:
			serialize_string(ls->ws->desc, b);
			serialize_float(ls->ws->to, WaterTo, b);
			break;
		
		case GetTemp:
			serialize_string(ls->ts->temp_desc, b);
			serialize_float(ls->ts->temp_data, TempFloat, b);
			serialize_string(ls->ts->hum_desc, b);
			serialize_float(ls->ts->hum_data, HumFloat, b);
			break;
		
		case GetAll:
			
			ls->type = GetDist;
			packet_serialize(ls, b);
			ls->type = GetWater;
			packet_serialize(ls, b);
			ls->type = GetTemp;
			packet_serialize(ls, b);
			
			break;
		
		default:
			printf("Operazione non valida!\n");
			break;			
	}
}

// ==============================================================
// ========================== USART ==============================
// ==============================================================

void USART_init() {
	UBRR0H = (UBRR0_SET >> 8);
	UBRR0L = UBRR0_SET;
	
	UCSR0A |= bit_value(U2X0);
	UCSR0B |= bit_value(RXCIE0);
	UCSR0B |= bit_value(RXEN0)  | bit_value(TXEN0);
	UCSR0C |= bit_value(UCSZ00) | bit_value(UCSZ01) | bit_value(USBS0);
}

static int USART_receive(FILE *f) {
	loop_until_bit_is_set(UCSR0A, RXC0);
	while(!(UCSR0A & bit_value(UDRE0)));
	return UDR0;
}

static int USART_send(char c, FILE *f) {
	if(c == '\n') USART_send('\r', f);
	while(!(UCSR0A & bit_value(UDRE0)));
	UDR0 = c;
	return 0;
}

static FILE OUTFP = FDEV_SETUP_STREAM(USART_send, NULL, _FDEV_SETUP_WRITE);
static FILE INFP  = FDEV_SETUP_STREAM(NULL, USART_receive, _FDEV_SETUP_READ);


// ==============================================================
// ======================= DIST SENSOR ==========================
// ==============================================================

void dist_sensor_init() {
	DDRB |= bit_value(dist_sensor->trig_bit);
	DDRB &= ~bit_value(dist_sensor->echo_bit);
}

void begin_trig() {
	PORTB &= ~bit_value(dist_sensor->trig_bit);
	_delay_us(5);
	PORTB |= bit_value(dist_sensor->trig_bit);
	_delay_us(10);
	PORTB &= ~bit_value(dist_sensor->trig_bit);
}

int pulseIn(uint8_t state, int timeout) {
	uint8_t bit = bit_value(dist_sensor->echo_bit);
	uint8_t cmp = (state ? bit : 0);
	
	int res = 0;
	int cnt = 0;

	while ((PINB & bit) == cmp)
		if (cnt++ == timeout)
			return 0;
	
	while ((PINB & bit) != cmp)
		if (cnt++ == timeout)
			return 0;

	while ((PINB & bit) == cmp) {
		if (cnt++ == timeout)
			return 0;
		res++;
	}

	return res + 1;
}

void dist_sensor_active() {
	begin_trig();
	
	int duration = pulseIn(HIGH, 10000);
	float cm = (duration/2) / 29.1;
	
	dist_sensor->data = cm;
	
	_delay_ms(500);
}

// ==============================================================
// ======================= ANALOG SENSOR ========================
// ==============================================================

void analog_init() {
	ADMUX  = 0x00;
	ADCSRA = 0x00;
	ADCSRB = 0x00;
	
	ADMUX  |= bit_value(REFS0) | bit_value(MUX3)  | bit_value(MUX2)  | bit_value(MUX1)  | bit_value(MUX0);
	ADCSRA |= bit_value(ADEN)  | bit_value(ADPS2) | bit_value(ADPS1) | bit_value(ADPS0) | bit_value(ADSC);
	loop_until_bit_is_clear(ADCSRA, ADSC);
}

void analog_sensor_init() {
	DDRC  &= ~bit_value(water_sensor->analog_bit);
	PORTC &= ~bit_value(water_sensor->analog_bit);
	DIDR0 |= bit_value(water_sensor->analog_bit);
}

uint16_t analog_read() {
	ADMUX = (ADMUX & 0xf0) | (water_sensor->analog_bit & 0x0f);

	ADCSRA |= bit_value(ADSC);
	loop_until_bit_is_clear(ADCSRA, ADSC);

	uint8_t lb = ADCL;
	uint8_t hb = ADCH;

	return (((uint16_t) hb) << 8) | lb;
}

void analog_sensor_active() {
	uint16_t val = analog_read();
	if(0 <= val && val < 25) {
		water_sensor->to = 0.0;
	}
	if(25 <= val && val < 130) {
		water_sensor->to = 0.25;
	}
	if(130 <= val && val < 260)	{
		water_sensor->to = 0.75;
	}
	if(260 <= val && val < 320)	{
		water_sensor->to = 1.5;
	}
	if(320 <= val && val < 390)	{
		water_sensor->to = 2.5;
	}
	if(390 <= val && val < 460)	{
		water_sensor->to = 3.5;
	}
	if(val >= 460) {
		water_sensor->to = 4.5;
	}

	_delay_ms(500);
}

// ==============================================================
// ======================= TEMP SENSOR ==========================
// ==============================================================

void temp_request() {
	DDRD |= bit_value(temp_sensor->digital_bit);
	
	PORTD &= ~bit_value(temp_sensor->digital_bit);
	_delay_ms(20);
	
	PORTD |= bit_value(temp_sensor->digital_bit);
}

void temp_response() {
	uint8_t bit = bit_value(temp_sensor->digital_bit);
	DDRD &= ~bit;
	
	while(PIND & bit);
	while(!(PIND & bit));
	while(PIND & bit);
}

float temp_data() {
	uint8_t bit = bit_value(temp_sensor->digital_bit);
	uint8_t c = 0;
	
	for(int i = 0; i < 8; i++) {
		while(!(PIND & bit));
		_delay_us(30);
		
		if(PIND & bit) c = (c << 1) | HIGH;
		else c = (c << 1);
		
		while(PIND & bit);
	}
	
	return c;
}

void temp_sensor_active() {
	float hum, temp, checksum;
	
	temp_request();
	temp_response();
	
	hum = temp_data() + temp_data() / 100;
	temp = temp_data() + temp_data() / 100;
	checksum = temp_data();
	
	temp_sensor->temp_data = temp;
	temp_sensor->hum_data = hum;
	
	_delay_ms(500);
}

// ==============================================================
// ========================== MAIN ==============================
// ==============================================================

void sensor_init() {
	dist_sensor_init();
	analog_init();
	analog_sensor_init();
}

void delay_ms(int ms) {
	for(int i = 0; i < ms; i++)
		_delay_ms(1);
}

void stampa(){

	cli(); //abilita interrupt
	reset_buffer(buffer);
	
	int data;
			
	log_struct->type = GetAll;
	packet_serialize(log_struct, buffer);
	
	printf("%s", (char*) buffer->data);
		
	sei(); //disabilita interrupt
	
}

	
int main() {
	dist_sensor = (DistSensor*) malloc(sizeof(DistSensor));
	water_sensor = (WaterSensor*) malloc(sizeof(WaterSensor));
	temp_sensor = (TempSensor*) malloc(sizeof(TempSensor));
	log_struct = (LogStruct*) malloc(sizeof(LogStruct));
	buffer = (Buffer*) malloc(sizeof(Buffer));
	
	USART_init();
	stdout = &OUTFP;
	stdin  = &INFP;

	sensor_init();
	
	struct_init(dist_sensor, water_sensor, temp_sensor, log_struct, buffer);
	
	
	printf("Inserire il periodo di acquisizione dei dati: \n");
	char c;
	scanf("%c", &c);
	int periodo=(int)c;
	periodo=periodo*1000;
	
	sei(); 
	
	while(1) {
		printf("[");
		dist_sensor_active();
		analog_sensor_active();
		temp_sensor_active();
		stampa();
		printf("]");
		delay_ms(periodo);
		
	}
}
