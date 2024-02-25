#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include "DS3231.h"


#define CLK_PIN   PB7
#define DATA_PIN  PB5
#define CS_PIN    PB2
#define SEGMENT_COUNT 4

#define PIR_PIN   PD0

const uint8_t charPatterns[][8] = {
	{0b01000001, 0b01000001, 0b01000001, 0b01000001, 0b00100010, 0b00100010, 0b00010100, 0b00001000}, // 'V'
	{0b00000001, 0b00000010, 0b00000010, 0b01110010, 0b10001010, 0b11111010, 0b10000010, 0b01110001}, // 'e'
	{0b11000111, 0b00101000, 0b00001000, 0b00001000, 0b00001111, 0b00001000, 0b00101000, 0b11001000}, // 'C'
	{0b00111100, 0b10100010, 0b10100010, 0b10100010, 0b10100010, 0b10100010, 0b10100010, 0b10111100}, // 'A'
	
		
};

void init7Seg() {
	DDRB = 0xFF; // Set PORTB as output for latches
	DDRC = 0xFF; // Assuming 7-segment display control pins are connected to PORTC
	DDRA |= (1 << PA3) | (1 << PA4) | (1 << PA5) | (1 << PA6) | (1 << PA7); // Set PA3, PA4, PA5, PA6 as output for clock pulses
	PORTA &= ~((1 << PA3) | (1 << PA4) | (1 << PA5) | (1 << PA6)); // Set PA3, PA4, PA5, PA6 to logic lo
	DDRA |= (1 << PA7);
}

void displayTime(uint8_t hours, uint8_t minutes, uint8_t seconds) {
	// 7-segment patterns for numbers 0-9
	const uint8_t segments[10] = {
		0b00111111, // 0
		0b00000110, // 1
		0b01011011, // 2
		0b01001111, // 3
		0b01100110, // 4
		0b01101101, // 5
		0b01111101, // 6
		0b00000111, // 7
		0b01111111, // 8
		0b01101111  // 9
	};

	// Extract individual digits from hours, minutes, and seconds
	uint8_t hourDigit0 = hours / 10;
	uint8_t hourDigit1 = hours % 10;
	uint8_t minuteDigit0 = minutes / 10;
	uint8_t minuteDigit1 = minutes % 10;

	// Display hours on displays D and C
	PORTB = segments[hourDigit1];
	PORTA |= (1 << PA4); // Clock pulse for display C
	PORTA &= ~(1 << PA4);
	PORTB = segments[hourDigit0];
	PORTA |= (1 << PA3); // Clock pulse for display D
	PORTA &= ~(1 << PA3);

	// Display minutes on displays B and A
	PORTB = segments[minuteDigit1];
	PORTA |= (1 << PA6); // Clock pulse for display A
	PORTA &= ~(1 << PA6);

	PORTB = segments[minuteDigit0];
	PORTA |= (1 << PA5); // Clock pulse for display B
	PORTA &= ~(1 << PA5);
}

void send(uint8_t matrix, uint8_t registerIndex, uint8_t value) {
	PORTB &= ~(1 << CS_PIN);  // CS_PIN low

	// Propagate the data through the series connection
	for (uint8_t i = 0; i < matrix; i++) {
		writeWord(0, 0);  // Send a dummy byte to propagate the data
	}

	writeWord(registerIndex, value);

	// Clear any remaining propagation
	for (uint8_t i = matrix; i < SEGMENT_COUNT - 1; i++) {
		writeWord(0, 0);  // Send a dummy byte to clear any remaining propagation
	}

	PORTB |= (1 << CS_PIN);  // CS_PIN high
}

void clearDisplays() {
	for (uint8_t i = 0; i < SEGMENT_COUNT; i++) {
		for (uint8_t row = 1; row <= 8; row++) {
			send(i, row, 0);
		}
	}
}

void initSPI(void) {
	DDRB |= (1 << CLK_PIN) | (1 << DATA_PIN) | (1 << CS_PIN);
	PORTB |= (1 << CS_PIN);  // CS_PIN high initially
	SPCR |= (1 << MSTR) | (1 << SPE);  // Enable SPI as master
	
	// Set display brightness
	for (uint8_t i = 0; i < SEGMENT_COUNT; i++) {
		send(i, 0x0A, 0x07);  // Set brightness (adjust as needed, 0x00 to 0x0F)
	}
}

void writeByte(uint8_t byte) {
	SPDR = byte;
	while (!(SPSR & (1 << SPIF)));  // Wait for transmission complete
}

void writeWord(uint8_t address, uint8_t data) {
	writeByte(address);
	writeByte(data);
}

void handleDotMatrixDisplay() {
	if (PIND & (1 << PIR_PIN)) {
		// Motion detected
		for (uint8_t i = 0; i < SEGMENT_COUNT; i++) {
			for (uint8_t j = 0; j < 8; j++) {
				send(i, j + 1, charPatterns[i][j]);
			}
			
		}
		_delay_ms(5000);
		} else {
		// No motion, clear the display
		clearDisplays();
	}
}

void disable_SPI()
{
	SPCR &= ~(1 << SPE);
	DDRB = 0xFF;
}

int main(void) {
	initSPI();
	twi_INIT(); 
	init7Seg();
	//DS3231_setTime(00, 19, 10, NULL, NULL, NULL, NULL);  // 12:10:00 Monday 01/01/2021
	for (uint8_t i = 0; i < SEGMENT_COUNT; i++) {
		send(i, 0xF, 0); // Disable test mode
		send(i, 0xB, 7); // Set scanlines to 8
		send(i, 0xC, 1); // Enable display
	}
	clearDisplays();

	while (1) {
		uint8_t hours, minutes, seconds;
		
		disable_SPI();

		// Get current time from DS3231
		DS3231_getTime(&seconds, &minutes, &hours, NULL, NULL, NULL, NULL);

		// Display time on 7-segment displays
		displayTime(hours, minutes, seconds);
		
		initSPI();
		handleDotMatrixDisplay();
		_delay_ms(100); // Short delay to avoid false triggers
	}

	return 0;
}
