#define BIAS_COMPENSATION 1

#define IO_PINB 0x03
#define IO_DDRB 0x04
#define IO_PORTB 0x05
#define IO_PIND 0x09
#define IO_PORTD 0x0B

	.extern spi_buf
	.global spi_pump_bus
spi_pump_bus:
	lds r25, spi_buf // ring buffer
	ldi r26, 8 // count

	//sbi IO_PORTB, 4 // MISO

	// If the clock is already 0, we have failed.
	sbis IO_PIND, 7
	rjmp __spi_pump_bus_fail

__spi_pump_bus_clk1:
	// Set bit
	lsr r25
	brcs __spi_pump_bus_clk1_write_1

__spi_pump_bus_clk1_write_0:
	// Wait while clock is 1
	sbic IO_PINB, 2 // ~SS
	rjmp __spi_pump_bus_fail
	sbic IO_PIND, 7 // alt SCK
	rjmp __spi_pump_bus_clk1_write_0
#if BIAS_COMPENSATION
	cbi IO_PORTB, 4
#endif

__spi_pump_bus_clk0_wrote_0:
	// Wait while clock is 0
	sbic IO_PINB, 2 // ~SS
	rjmp __spi_pump_bus_fail
	sbis IO_PIND, 7 // alt SCK
	rjmp __spi_pump_bus_clk0_wrote_0

	// Read bit
	sbic IO_PINB, 3 // MOSI
	ori r25, 0x80
#if BIAS_COMPENSATION
	sbi IO_PORTB, 4 // MISO
#endif

	// Decrement and loop if nonzero
	subi r26, 1
	brne __spi_pump_bus_clk1
	rjmp __spi_pump_bus_pass

__spi_pump_bus_clk1_write_1:
	// Wait while clock is 1
	sbic IO_PINB, 2 // ~SS
	rjmp __spi_pump_bus_fail
	sbic IO_PIND, 7 // alt SCK
	rjmp __spi_pump_bus_clk1_write_1
#if BIAS_COMPENSATION
	sbi IO_PORTB, 4
#endif

__spi_pump_bus_clk0_wrote_1:
	// Wait while clock is 0
	sbic IO_PINB, 2 // ~SS
	rjmp __spi_pump_bus_fail
	sbis IO_PIND, 7 // alt SCK
	rjmp __spi_pump_bus_clk0_wrote_1
	cbi IO_PORTB, 4

	// Read bit
	sbic IO_PINB, 3 // MOSI
	ori r25, 0x80

	// Decrement and loop if nonzero
	subi r26, 1
	brne __spi_pump_bus_clk1

__spi_pump_bus_pass:
	//cbi IO_DDRB, 4 // MISO
	sbi IO_PORTB, 4
	sts spi_buf, r25
	ldi r24, 0x01
	ret
__spi_pump_bus_fail:
	//cbi IO_DDRB, 4 // MISO
	sbi IO_PORTB, 4
	sts spi_buf, r25
	ldi r24, 0x00
	ret
