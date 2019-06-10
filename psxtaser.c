#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#define USE_TIMER 1
#define USE_USART 1
#define BIAS_COMPENSATION 1
#define TIMER_USECS 10
#define TIMER_FIRST_USECS 10

uint16_t last_sector = 0;

uint8_t spi_buf;
#define SPI_WRITE(data) spi_buf = (data)
#define SPI_READ() spi_buf;

const uint8_t hextab[16+1] = "0123456789abcdef";

static uint16_t button_state = 0xFFFF;
struct buttonseq_state {
	uint16_t value;
	uint8_t wait;
} __attribute__((__packed__));
const PROGMEM struct buttonseq_state buttonseq_start2[] = {
	{0xFFF7, 0x02},
	{0xFFFF, 0x00},
};
const PROGMEM struct buttonseq_state buttonseq_start1[] = {
	{0xFFF7, 0x01},
	{0xFFFF, 0x00},
};
const PROGMEM struct buttonseq_state buttonseq_x1[] = {
	{0xBFFF, 0x01},
	{0xFFFF, 0x00},
};
const PROGMEM struct buttonseq_state buttonseq_main_menu[] = {
	{0xBFFF, 0x01},
	{0xFFEF, 0x01},
	{0xBFFF, 0x01},
	{0xFFFF, 0x00},
};
const struct buttonseq_state *const buttonseq_list[] = {
	buttonseq_start2,
	buttonseq_start1,
	buttonseq_x1,
	buttonseq_main_menu,
	NULL,
};
uint8_t buttonseq_list_idx = 0;
const struct buttonseq_state *buttonseq_list_ptr = NULL;
uint8_t buttonseq_wait = 0;
bool buttonseq_button_last = false;
uint8_t buttonseq_button_wait = 0;

int main(int argc, char *argv[]);
void controller_attempt(void);
void handle_controller(void);
static inline int16_t spi_read(void);
bool spi_pump_bus(void);

//uint8_t memcard_flag = 0|(1<<3);
uint8_t memcard_flag = 0|(0<<3);

uint8_t byte_count = 0;

static volatile uint8_t sink8;

uint16_t this_delay = 0;
uint16_t last_delay = 0;

#define PUMP_IN   0x80
#define PUMP_OUT  0x10
#define MCR_CLK   0x20
#define MCR_MISO  0x10
#define MCR_MOSI  0x08
#define MCR_SS    0x04
#define MCR_ACK   0x01

#define NEXT_BUTTON 0x04

int main(int argc, char *argv[])
{
	// Reset all port directions to input and float them
	DDRB  = 0x00;
	DDRC  = 0x00;
	DDRD  = 0x00;
	PORTB = 0x00;
	PORTC = 0x00;
	PORTD = 0x00;

	MCUCR &= (1<<4); // Ensure pullups are enabled

	// Set intended port directions
	PORTB = 0|MCR_SS|MCR_CLK;
	DDRB  = 0;
	PORTD = 0xFF&~PUMP_IN;
	DDRD  = 0xFF&~PUMP_IN;

	// Set up USART0
#if USE_USART
	UBRR0H = 0; UBRR0L = 0; // 1,000,000 baud
	UCSR0A = 0x40;
	UCSR0C = 0x06;
	UCSR0B = 0x18;
#endif

#if USE_TIMER
	// Set up TC0
	TCCR0A = 0x00;
	TCCR0B = 0x02; // fclk/8, so 2MHz, so 2 per usec, so 128 usecs before we have problems
	TIMSK0 = 0x00;
#endif

	// Set up TC1
	// Timings:
	// Ideally about 1042 ticks per NTSC frame
	// Ideally exactly 1250 ticks per PAL frame
	// psx-spx suggests about 1054 ticks per NTSC frame
	// psx-spx suggests about 1256 ticks per PAL frame
	// psx-spx might suggest about 1251 ticks per PAL frame if we assume 313 lines per frame
	// NTSC on PAL console: roughly 1053.5 ticks per NTSC frame
	// PAL on PAL console: roughly 1249 ticks per PAL frame
	TCCR1A = 0x00;
	TCCR1B = 0x04; // fclk/256, ticking roughly every 16us
	TCCR1C = 0x00;
	TIMSK1 = 0x00;
	TCNT1H = 0x00;
	TCNT1L = 0x00;

	GTCCR = 0x00;

	for(;;) {
		controller_attempt();
	}
}

void controller_attempt(void)
{
#if USE_USART
	static bool do_newline = false;
#endif
	int16_t inv;

	// Float some outputs
	DDRB &= ~MCR_ACK;
	DDRB &= ~MCR_MISO;

	// Send a newline while we wait
#if USE_USART
	if(do_newline) {
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(byte_count>>4)&0xF];
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(byte_count>>0)&0xF];
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = '\n';

		uint8_t delayl = TCNT1L;
		uint8_t delayh = TCNT1H;
		this_delay = (((uint16_t)(delayh))<<8)|(uint16_t)delayl;
		uint16_t calc_delay = (uint16_t)(this_delay - last_delay);
		last_delay = this_delay;
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(calc_delay>>12)&0xF];
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(calc_delay>>8)&0xF];
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(calc_delay>>4)&0xF];
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(calc_delay>>0)&0xF];
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = ',';
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(buttonseq_list_idx>>4)&0xF];
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(buttonseq_list_idx>>0)&0xF];
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = ',';

		do_newline = false;
	}
#endif
	byte_count = 0;

	// Wait for SS to go high
	while((PINB & MCR_SS) == 0) {
		if((TCNT0 & 0x01) != 0) {
			PORTB |= MCR_MISO;
		} else {
			PORTB &= ~MCR_MISO;
		}
	}

	// TEST: Set output to output
	//DDRB |=  MCR_MISO;

	// Wait for SS to go low
	while((PINB & MCR_SS) != 0) {
#if 0
		if((TCNT0 & 0x01) != 0) {
			PORTB |= MCR_MISO;
		} else {
			PORTB &= ~MCR_MISO;
		}
#endif
	}

	//UDR0 = 'L'; while((UCSR0A & (1<<5)) == 0) { }

	// Read command byte
	// Wait for SPIF in SPSR to be set
	SPI_WRITE(0xFF);
	if(!spi_pump_bus()) { return; }
	uint8_t inv8 = 0;
	inv8 = SPI_READ();

	// Ensure this is reading the controller
	if(inv8 != 0x01) {
		DDRB &= ~MCR_MISO;
		return;
	}
#if USE_USART
	do_newline = true;
#endif

#if USE_TIMER
	// Fire timer
	OCR0A = TIMER_FIRST_USECS*2; // usecs*2
	TCNT0 = 0;
	TIFR0 = 0x02;
#endif

	// Set output to output
	PORTB |=  MCR_MISO;
	DDRB  |=  MCR_MISO;
	PORTB |=  MCR_ACK;
	//DDRB  |=  MCR_ACK;

	handle_controller();
	DDRB &= ~MCR_MISO;
}

static inline int16_t spi_read(void)
{
#if 0
	while((UCSR0A & (1<<5)) == 0) { }
	UDR0 = hextab[(spi_buf>>4)&0xF];
	while((UCSR0A & (1<<5)) == 0) { }
	UDR0 = hextab[(spi_buf>>0)&0xF];
#endif

	// Strobe ACK pin
#if USE_TIMER
	while((TIFR0 & 0x02) == 0) {
#if BIAS_COMPENSATION
		if((TCNT0 & 0x01) != 0) {
			PORTB |= MCR_MISO;
		} else {
			PORTB &= ~MCR_MISO;
		}
#else
		//DDRB  &= ~MCR_MISO;
#endif
	}
#endif

	DDRB  |=  MCR_ACK;
	_delay_us(2);
	PORTB &= ~MCR_ACK;
	_delay_us(2);
	PORTB |=  MCR_ACK;
	_delay_us(2);
	DDRB  &= ~MCR_ACK;

	// Pump bus
	if(!spi_pump_bus()) { return -1; }

	int16_t data = (int16_t)(uint16_t)(uint8_t)SPI_READ();

#if 0
	while((UCSR0A & (1<<5)) == 0) { }
	UDR0 = hextab[(data>>4)&0xF];
	while((UCSR0A & (1<<5)) == 0) { }
	UDR0 = hextab[(data>>0)&0xF];
	while((UCSR0A & (1<<5)) == 0) { }
	UDR0 = ']';
#endif

	byte_count++;

#if USE_TIMER
	// Fire timer
	OCR0A = TIMER_USECS*2; // usecs*2
	TCNT0 = 0;
	TIFR0 = 0x02;
#endif

	// Return SPI data
	return data;
}

void handle_controller(void)
{
	int16_t inv;

	// Set PB4 to output
	//DDRB |=  MCR_MISO;

	#if 1
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(button_state>>12)&0xF];
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(button_state>>8)&0xF];
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(button_state>>4)&0xF];
		while((UCSR0A & (1<<5)) == 0) { }
		UDR0 = hextab[(button_state>>0)&0xF];
	#endif

	// Send first controller ID byte
	SPI_WRITE(0x41);
	inv = spi_read(); if(inv < 0) { return; }

	// Send second controller ID byte
	SPI_WRITE(0x5A);
	inv = spi_read(); if(inv < 0) { return; }

	// Send buttons LSB
	SPI_WRITE((button_state)&0xFF);
	inv = spi_read(); if(inv < 0) { return; }

	// Send buttons MSB
	SPI_WRITE((button_state>>8)&0xFF);
	inv = spi_read(); if(inv < 0) { return; }

	// Get next button if available
	if(!buttonseq_button_last) {
		buttonseq_button_last = ((PIND & NEXT_BUTTON) == 0);
		if(buttonseq_button_wait == 0 && buttonseq_button_last) {
			buttonseq_list_ptr = buttonseq_list[buttonseq_list_idx];
			buttonseq_list_idx++;
			if(buttonseq_list_ptr == NULL) {
				buttonseq_list_idx = 0;
				buttonseq_list_ptr = buttonseq_list[buttonseq_list_idx];
				buttonseq_list_idx++;
			}
			if(buttonseq_list[buttonseq_list_idx] == NULL) {
				buttonseq_list_idx = 0;
			}
			buttonseq_button_wait = 10;
		} else if(buttonseq_button_wait > 0) {
			buttonseq_button_wait--;
		}
	} else {
		buttonseq_button_last = ((PIND & NEXT_BUTTON) == 0);
		if(buttonseq_button_last) {
			buttonseq_button_wait = 10;
		}
	}

	if(buttonseq_wait > 0) {
		buttonseq_wait--;
	}
	if(buttonseq_wait == 0) {
		if(buttonseq_list_ptr != NULL) {
			button_state = pgm_read_word(&(buttonseq_list_ptr->value));
			buttonseq_wait = pgm_read_byte(&(buttonseq_list_ptr->wait));
			buttonseq_list_ptr++;
			if(buttonseq_wait == 0) {
				buttonseq_list_ptr = NULL;
			}
		}
	}
}


