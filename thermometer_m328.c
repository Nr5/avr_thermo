#define F_CPU 16000000
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdbool.h>
#include <avr/sfr_defs.h>
#include <stdio.h>

#define BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (BAUDRATE * 16UL))) - 1)

#define CS_PIN PB2 // CS pin connected to PB2 (digital pin 10)
#define THERMOMETER_PIN PB1 // CS pin connected to PB2 (digital pin 10)
#include "font.h"

// MAX7219 commands
#define MAX7219_NOOP        0x00

//...
//the values 1 to 8 are for drawing to the corresponding rows of the led matrix
//...
#define MAX7219_DECODEMODE  0x09
#define MAX7219_INTENSITY   0x0A
#define MAX7219_SCANLIMIT   0x0B
#define MAX7219_SHUTDOWN    0x0C
#define MAX7219_DISPLAYTEST 0x0F
volatile uint32_t brightness = 0x0f0e0d0c;

#define CS_PIN PB2 // CS pin connected to PB2 (digital pin 10)
#define NUM_MATRICES 4

volatile uint16_t pulse_start_time;
volatile uint16_t pulse_duration;
volatile uint64_t bitpattern;

void SPI_MasterInit(void) {
    DDRB |= (1 << DDB5) | (1 << DDB3) | (1 << DDB2); // Set MOSI, SCK, and SS pins as outputs
    DDRB &= ~(1 << DDB4); // Set MISO pin as input

    SPCR |= (1 << MSTR); // Set the ATmega328 as master
    SPCR |= (1 << SPR0) | (1 << SPR1); // Set SPI clock rate as fck/128
    SPCR |= (1 << SPE); // Enable SPI
}

void SPI_MasterTransmit(uint16_t data) {
    SPDR = data; // Start transmission by loading data into SPI data register
    while (!(SPSR & (1 << SPIF))); // Wait for transmission to complete
}

void LEDMatrix_Init(void) {
    DDRB |= (1 << CS_PIN); // Set CS pin as output
    PORTB |= (1 << CS_PIN); // Set CS pin high initially (not selected)
}

void MAX7219_SendCommand(uint8_t command, uint32_t data) {
    PORTB &= ~(1 << CS_PIN); // Select the MAX7219 chip

    // Send the command and data
    uint8_t i ;
    for (i = NUM_MATRICES; i ; i--) {
        //every matrix in the chain get the same command, but a different 8bit chunk from the 64bit input
        SPI_MasterTransmit(command);
        SPI_MasterTransmit( data>>((i-1)*8) & 0xFF ); 
    }

    PORTB |= (1 << CS_PIN); // Deselect the MAX7219 chip
}

void MAX7219_Init() {
    // Initialize SPI communication
    SPI_MasterInit();

    // Configure the MAX7219 chip
    MAX7219_SendCommand(MAX7219_SCANLIMIT, 0x07070707ul);   // Display all 8 digits
    MAX7219_SendCommand(MAX7219_DISPLAYTEST, 0); // Normal operation
    MAX7219_SendCommand(MAX7219_SHUTDOWN,  0x01010101ul);    // Wake up from shutdown mode
    MAX7219_SendCommand(MAX7219_DECODEMODE, 0);  // No BCD decoding
    MAX7219_SendCommand(MAX7219_INTENSITY, brightness);   // Set display intensity (brightness)
}

ISR(PCINT0_vect) {
    // Rising edge detection
    if ((PINB >> THERMOMETER_PIN) & 0x01) //only measure on rising edge so we always get the duration of one pulse plus the delimiter low signal
    {
        pulse_duration = (TCNT1 - pulse_start_time) ;
        pulse_start_time = TCNT1;
        bitpattern = (bitpattern << 1) | (pulse_duration > 1536); // if pulse duration is longer than 1.5k clockcycles append a 1 to bitpattern, else a 0
    }
}

void printtemp(uint32_t* pic, uint8_t temperature, uint8_t humidity){
    uint8_t i;
    uint8_t j;

    uint8_t temp_str[8];
    uint8_t humid_str[8];
    humidity -= (humidity == 100);

    sprintf(temp_str,"%02d", temperature); // extract temperature from bitpattern, convert to celsius and put in string to render

    sprintf(humid_str,"%02d",humidity);       // extract humidity from bitpattern, convert to percent and put in string to render

    if (temp_str[0]=='0')temp_str[0]='@';
    if (humid_str[0]=='0')humid_str[0]='@';
    // we want to pad with spaces, sprintf doesn't seem to work. 
    // '@' is the symbol for space because regular space is not in the ascii range of our font
    for (i = 0 ; i < 8; i++ ){
        pic[i] = 1ull << 14;   //draw seperating line to framebuffer
    }

    uint8_t degree_celsius[7]={
       0b11000,
       0b11000,
       0b00011,
       0b00100,
       0b00100,
       0b00011
    };
    uint8_t percentage[7]={
       0b11001,
       0b11010,
       0b00010,
       0b00100,
       0b01000,
       0b01011,
       0b10011
    };

    if (bitpattern & (0x1ull << 23) )pic[4] |= 0x3ull << (30 -(4*(temperature<10))); // if sign bit is set put draw minus sign on framebuffer
    for (j=0;temp_str[j];j++){
        for (i = 0 ; i < 5; i++ ){
            pic[i+2] |= ((uint32_t) ascii_chars[temp_str[j]*5+i]) << (26-j*4) ; //draw string on framebuffer
        }
    }
    for (i = 0 ; i < 7; i++ ){
        pic[i+1] |= ((uint32_t) degree_celsius[i]) << 16 ; //draw string on framebuffer
    }


    for (j=0;humid_str[j];j++){
        for (i = 0 ; i < 5; i++ ){
            pic[i+2] |= ((uint32_t) ascii_chars[humid_str[j]*5+i]) << (10-j*4) ; //draw string on framebuffer
        }
    }

    for (i = 0 ; i < 7; i++ ){
        pic[i+1] |= ((uint32_t) percentage[i]) ; //draw string on framebuffer
    }
}
void printstring(uint32_t * pic, char* str){
    uint8_t i, j;
    for (j=0;str[j];j++){
        while (str[j] < '0' || str[j] > 'Z'){
            str++;
        }
        for (i = 0 ; i < 5; i++ ){
           pic[i+2] |= ((uint32_t) ascii_chars[str[j]*5+i]) << (29-j*4) ; //draw string on framebuffer
        }

    }

}
int main(void)
{
    uint8_t i;
    uint8_t j;
    
    // Enable ATmega328 as a master
    DDRB |= (1 << PB2);  // Set PB2 as output
    // Send AT command to ESP-01
    
    
    MAX7219_Init();

    // Draw a pixel at row 3, column 4 (0-indexed)
    
    PCICR |= (1 << PCIE0);
    PCMSK0 |= (1 << PCINT1);
    // Enable global interrupts
    TCCR1B |= (1 << CS10);
    sei();
    
    

    while (1) {
        PORTB ^= 1;
        uint32_t pic[8]={0,0,0,0,0,0,0,0};

        DDRB  |= (1 << THERMOMETER_PIN);    // set pb1 as output
        PORTB &= ~(1 << THERMOMETER_PIN);   // set pb1 low
        _delay_ms(2);
        bitpattern=0;
        PORTB |= (1 << THERMOMETER_PIN);    // after 2ms delay set pb1 high again to initialize data transfer
        DDRB  &= ~(1 << THERMOMETER_PIN);

        PCICR |= (1 << PCIE0);
        PCMSK0 |= (1 << PCINT1); //setup pinchange interrupt on pb1
        
        _delay_ms(100);
        uint8_t temperature=((bitpattern >> 10)    & 0xff)*  2/ 5;
        uint8_t humidity   = (((bitpattern >> 26) & 0xff))* 10/25;
        
        printtemp(pic,temperature,humidity);
        uint8_t i;
        for (i = 0; i<8; i++){
            MAX7219_SendCommand(i+1, pic[i]); //draw framebuffer to display
        }
    }
}
