/*
TV-B-Gone Firmware version 1.2
for use with ATtiny85v and v1.2 hardware
(c) Mitch Altman + Limor Fried 2009
Last edits, August 16 2009

With some code from:
Kevin Timmerman & Damien Good 7-Dec-07

Distributed under Creative Commons 2.5 -- Attib & Share Alike

This is the 'universal' code designed for v1.2 - it will select EU or NA
depending on a pulldown resistor on pin B1 !
*/

#include <avr/io.h>             // this contains all the IO port definitions
#include <avr/eeprom.h>
#include <avr/sleep.h>          // definitions for power-down modes
#include <avr/pgmspace.h>       // definitions or keeping constants in program memory
#include <avr/wdt.h>      // definitions or keeping constants in program memory
#include <Arduino.h>
#include "ircntrl.h"
#include "util.h"

// TODO: maybe connect regulat transistor and then change high and low of leds

/*
This project transmits a bunch of TV POWER codes, one right after the other, 
with a pause in between each.  (To have a visible indication that it is 
transmitting, it also pulses a visible LED once each time a POWER code is 
transmitted.)  That is all TV-B-Gone does.  The tricky part of TV-B-Gone 
was collecting all of the POWER codes, and getting rid of the duplicates and 
near-duplicates (because if there is a duplicate, then one POWER code will 
turn a TV off, and the duplicate will turn it on again (which we certainly 
do not want).  I have compiled the most popular codes with the 
duplicates eliminated, both for North America (which is the same as Asia, as 
far as POWER codes are concerned -- even though much of Asia USES PAL video) 
and for Europe (which works for Australia, New Zealand, the Middle East, and 
other parts of the world that use PAL video).

Before creating a TV-B-Gone Kit, I originally started this project by hacking 
the MiniPOV kit.  This presents a limitation, based on the size of
the Atmel ATtiny2313 internal flash memory, which is 2KB.  With 2KB we can only 
fit about 7 POWER codes into the firmware's database of POWER codes.  However,
the more codes the better! Which is why we chose the ATtiny85 for the 
TV-B-Gone Kit.

This version of the firmware has the most popular 100+ POWER codes for 
North America and 100+ POWER codes for Europe. You can select which region 
to use by soldering a 10K pulldown resistor.
*/


/*
This project is a good example of how to use the AVR chip timers.
*/


/*
The hardware for this project is very simple:
     ATtiny85 has 8 pins:
       pin 1   RST + Button
       pin 2   one pin of ceramic resonator MUST be 8.0 mhz
       pin 3   other pin of ceramic resonator
       pin 4   ground
       pin 5   OC1A - IR emitters, through a '2907 PNP driver that connects 
               to 4 (or more!) PN2222A drivers, with 1000 ohm base resistor 
               and also connects to programming circuitry
       pin 6   Region selector. Float for US, 10K pulldown for EU,
               also connects to programming circuitry
       pin 7   PB0 - visible LED, and also connects to programming circuitry
       pin 8   +3-5v DC (such as 2-4 AA batteries!)
    See the schematic for more details.

    This firmware requires using an 8.0MHz ceramic resonator 
       (since the internal oscillator may not be accurate enough).

    IMPORTANT:  to use the ceramic resonator, you must perform the following:
                    make burn-fuse_cr
*/

void delay_ten_us(uint16_t us);


void ir_init() {


  TCCR1 = 0;		   // Turn off PWM/freq gen, should be off already
  TCCR0A = 0;
  TCCR0B = 0;

  // set the visible and IR LED pins to outputs
  pinMode(IRLED, OUTPUT);

  //  visible LED is off when pin is high
  // IR LED is off when pin is high
  digitalWrite(IRLED, HIGH);

}

/* This function is the 'workhorse' of transmitting IR codes.
   Given the on and off times, it turns on the PWM output on and off
   to generate one 'pair' from a long code. Each code has ~50 pairs! */
void xmitCodeElement(uint16_t ontime, uint16_t offtime, uint8_t PWM_code ) {
  // start Timer0 outputting the carrier frequency to IR emitters on and OC0A 
  // (PB1, pin 6)
  TCNT0 = 0; // reset the timers so they are aligned
  TIFR = 0;  // clean out the timer flags

  if(PWM_code) {
    // 99% of codes are PWM codes, they are pulses of a carrier frequecy
    // Usually the carrier is around 38KHz, and we generate that with PWM
    // timer 0
    TCCR0A =_BV(COM0B0) | _BV(WGM01);          // set up timer 0
    TCCR0B = _BV(CS00);
  } else {
    // However some codes dont use PWM in which case we just turn the IR
    // LED on for the period of time.
    PORTB &= ~_BV(IRLED);
  }

  // Now we wait, allowing the PWM hardware to pulse out the carrier 
  // frequency for the specified 'on' time
  delay_ten_us(ontime);
  
  // Now we have to turn it off so disable the PWM output
  TCCR0A = 0;
  TCCR0B = 0;
  // And make sure that the IR LED is off too (since the PWM may have 
  // been stopped while the LED is on!)
  PORTB |= _BV(IRLED);           // turn off IR LED

  // Now we wait for the specified 'off' time
  delay_ten_us(offtime);
}

/* This is kind of a strange but very useful helper function
   Because we are using compression, we index to the timer table
   not with a full 8-bit byte (which is wasteful) but 2 or 3 bits.
   Once code_ptr is set up to point to the right part of memory,
   this function will let us read 'count' bits at a time which
   it does by reading a byte into 'bits_r' and then buffering it. */

uint8_t bitsleft_r = 0;
uint8_t bits_r=0;
uint8_t bits_byte_count = 0;

// we cant read more than 8 bits at a time so dont try!
uint8_t read_bits(const uint8_t* codes, uint8_t count)
{
  uint8_t i;
  uint8_t tmp=0;

  // we need to read back count bytes
  for (i=0; i<count; i++) {
    // check if the 8-bit buffer we have has run out
    if (bitsleft_r == 0) {
      // in which case we read a new byte in
      bits_r = codes[bits_byte_count++];
      // and reset the buffer size (8 bites in a byte)
      bitsleft_r = 8;
    }
    // remove one bit
    bitsleft_r--;
    // and shift it off of the end of 'bits_r'
    tmp |= (((bits_r >> (bitsleft_r)) & 1) << (count-1-i));
  }
  // return the selected bits in the LSB part of tmp
  return tmp;
}


/*
The C compiler creates code that will transfer all constants into RAM when 
the microcontroller resets.  Since this firmware has a table (powerCodes) 
that is too large to transfer into RAM, the C compiler needs to be told to 
keep it in program memory space.  This is accomplished by the macro PROGMEM 
(this is used in the definition for powerCodes).  Since the C compiler assumes 
that constants are in RAM, rather than in program memory, when accessing
powerCodes, we need to use the pgm_read_word() and pgm_read_byte macros, and 
we need to use powerCodes as an address.  This is done with PGM_P, defined 
below.  
For example, when we start a new powerCode, we first point to it with the 
following statement:
    PGM_P thecode_p = pgm_read_word(powerCodes+i);
The next read from the powerCode is a byte that indicates the carrier 
frequency, read as follows:
      const uint8_t freq = pgm_read_byte(code_ptr++);
After that is a byte that tells us how many 'onTime/offTime' pairs we have:
      const uint8_t numpairs = pgm_read_byte(code_ptr++);
The next byte tells us the compression method. Since we are going to use a
timing table to keep track of how to pulse the LED, and the tables are 
pretty short (usually only 4-8 entries), we can index into the table with only
2 to 4 bits. Once we know the bit-packing-size we can decode the pairs
      const uint8_t bitcompression = pgm_read_byte(code_ptr++);
Subsequent reads from the powerCode are n bits (same as the packing size) 
that index into another table in ROM that actually stores the on/off times
      const PGM_P time_ptr = (PGM_P)pgm_read_word(code_ptr);
*/

void sendCode(struct IrCode code, const uint16_t* times, const uint8_t* codes) {

    uint16_t ontime, offtime;

    // Read the carrier frequency from the first byte of code structure
    const uint8_t freq = (code.freq == 0) ? 0 : freq_to_timerval(code.freq);
    // set OCR for Timer1 to output this POWER code's carrier frequency
    OCR0A = freq;
    // OCR0B = freq / 3; // 33% duty cycle


    // Get the number of pairs, the second byte from the code struct
    const uint8_t numpairs = code.numpairs;
    
    // Get the number of bits we use to index into the timer table
    // This is the third byte of the structure
    const uint8_t bitcompression = code.bitcompression;
    
    // Print out the frequency of the carrier and the PWM settings
#if DEBUG
    DEBUGP(putstring("\n\rOCR1: ");
    putnum_ud(freq);
    );
    DEBUGP(uint16_t x = (freq+1) * 8;
    putstring("\n\rFreq: ");  
    putnum_ud(F_CPU/x);
    );
    DEBUGP(putstring("\n\rOn/off pairs: ");
    putnum_ud(numpairs));

    DEBUGP(putstring("\n\rCompression: ");
    putnum_ud(bitcompression);
    putstring("\n\r"));

#endif

    // Transmit all codeElements for this POWER code
    // (a codeElement is an onTime and an offTime)
    // transmitting onTime means pulsing the IR emitters at the carrier
    // frequency for the length of time specified in onTime
    // transmitting offTime means no output from the IR emitters for the
    // length of time specified in offTime

    
    bitsleft_r = bits_r = bits_byte_count = 0;
    // For EACH pair in this code....
    cli();
    uint8_t k;
    for (k=0; k<numpairs; k++) {
      uint16_t ti;

      // Read the next 'n' bits as indicated by the compression variable
      // The multiply by 4 because there are 2 timing numbers per pair
      ti = read_bits(codes, bitcompression) << 1;

      // read the onTime and offTime from the program memory
      ontime = times[ti];  // read word 1 - ontime
      offtime = times[ti+1];  // read word 2 - offtime
      // transmit this codeElement (ontime and offtime)
      xmitCodeElement(ontime, offtime, (freq!=0));
    }
    sei();

#if DEBUG

    bitsleft_r = bits_r = bits_byte_count = 0;

    // print out all of the pulse pairs
    for (uint8_t k=0; k<numpairs; k++) {
      uint8_t ti;
      ti = (read_bits(codes, bitcompression)) << 1;
      // read the onTime and offTime from the program memory
      ontime = times[ti];
      offtime = times[ti+1];
      DEBUGP(putstring("\n\rti = ");
      putnum_ud(ti>>1);
      putstring("\tPair = ");
      putnum_ud(ontime));
      DEBUGP(putstring("\t");
      putnum_ud(offtime));
    }
#endif
}

/****************************** LED AND DELAY FUNCTIONS ********/


// This function delays the specified number of 10 microseconds
// it is 'hardcoded' and is calibrated by adjusting DELAY_CNT 
// in main.h Unless you are changing the crystal from 8mhz, dont
// mess with this.
void delay_ten_us(uint16_t us) {
  uint8_t timer;
  while (us != 0) {
    // for 8MHz we want to delay 80 cycles per 10 microseconds
    // this code is tweaked to give about that amount.
    for (timer=0; timer <= DELAY_CNT; timer++) {
      NOP;
      NOP;
    }
    NOP;
    us--;
  }
}


