/*
TV-B-Gone for Arduino version 1.2, Oct 23 2010
Ported to Arduino by Ken Shirriff=
http://www.arcfn.com/2009/12/tv-b-gone-for-arduino.html

The hardware for this project uses an Arduino:
 Connect an IR LED to pin 3 (RLED).
 Connect a visible LED to pin 13 (or use builtin LED in some Arduinos).
 Connect a pushbutton between pin 2 (TRIGGER) and ground.
 Pin 5 (REGIONSWITCH) is floating for North America, or wired to ground for Europe.

The original code is:
TV-B-Gone Firmware version 1.2
 for use with ATtiny85v and v1.2 hardware
 (c) Mitch Altman + Limor Fried 2009
 Last edits, August 16 2009


 I added universality for EU or NA,
 and Sleep mode to Ken's Arduino port
      -- Mitch Altman  18-Oct-2010
 Thanks to ka1kjz for the code for adding Sleep
      <http://www.ka1kjz.com/561/adding-sleep-to-tv-b-gone-code/>


 With some code from:
 Kevin Timmerman & Damien Good 7-Dec-07

 Distributed under Creative Commons 2.5 -- Attib & Share Alike

 */

#include "main.h"
#include <avr/sleep.h>
#include <SPI.h>

void xmitCodeElement(uint16_t ontime, uint16_t offtime, uint8_t PWM_code );
void delay_ten_us(uint16_t us);

uint8_t buf [256];

uint8_t bitsleft_r = 0;
uint8_t bits_r=0;
uint8_t bits_byte_count = 0;

volatile uint16_t bufSize = 0;
volatile byte pos = 0;
volatile boolean process_it = false;

#define putstring_nl(s) Serial.println(s)
#define putstring(s) Serial.print(s)
#define putnum_ud(n) Serial.print(n, DEC)
#define putnum_uh(n) Serial.print(n, HEX)

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

// This function quickly pulses the visible LED (connected to PB0, pin 5)
// This will indicate to the user that a code is being transmitted
void quickflashLED( void ) {
  digitalWrite(LED, HIGH);
  delay_ten_us(3000);   // 30 millisec delay
  digitalWrite(LED, LOW);
}

void slowflashLED( void ) {
  digitalWrite(LED, HIGH);
  delay_ten_us(30000);   // 300 millisec delay
  digitalWrite(LED, LOW);
}

// This function just flashes the visible LED a couple times, used to
// tell the user what region is selected
void quickflashLEDx( uint8_t x ) {
  quickflashLED();
  while(--x) {
    delay_ten_us(15000);     // 150 millisec delay between flahes
    quickflashLED();
  }
}
// This function just flashes the visible LED a couple times, used to
// tell the user what region is selected
void slowflashLEDx( uint8_t x ) {
  slowflashLED();
  while(--x) {
    delay_ten_us(50000);     // 150 millisec delay between flahes
    slowflashLED();
  }
}



/*
This project is a good example of how to use the AVR chip timers.
 */

/* This function is the 'workhorse' of transmitting IR codes.
 Given the on and off times, it turns on the PWM output on and off
 to generate one 'pair' from a long code. Each code has ~50 pairs! */
void xmitCodeElement(uint16_t ontime, uint16_t offtime, uint8_t PWM_code )
{
  TCNT2 = 0;
  if(PWM_code) {
    pinMode(IRLED, OUTPUT);
    // Fast PWM, setting top limit, divide by 8
    // Output to pin 3
    TCCR2A = _BV(COM2A0) | _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
    TCCR2B = _BV(WGM22) | _BV(CS21);
  }
  else {
    // However some codes dont use PWM in which case we just turn the IR
    // LED on for the period of time.
    digitalWrite(IRLED, HIGH);
  }

  // Now we wait, allowing the PWM hardware to pulse out the carrier
  // frequency for the specified 'on' time
  delay_ten_us(ontime);

  // Now we have to turn it off so disable the PWM output
  TCCR2A = 0;
  TCCR2B = 0;
  // And make sure that the IR LED is off too (since the PWM may have
  // been stopped while the LED is on!)
  digitalWrite(IRLED, LOW);

  // Now we wait for the specified 'off' time
  delay_ten_us(offtime);
}

/* This is kind of a strange but very useful helper function
 Because we are using compression, we index to the timer table
 not with a full 8-bit byte (which is wasteful) but 2 or 3 bits.
 Once code_ptr is set up to point to the right part of memory,
 this function will let us read 'count' bits at a time which
 it does by reading a byte into 'bits_r' and then buffering it. */


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

uint16_t ontime, offtime;
#define FALSE 0
#define TRUE 1


void setup()   {

  Serial.begin(9600);

  TCCR2A = 0;
  TCCR2B = 0;

  digitalWrite(IRLED, LOW);
  digitalWrite(DBG, LOW);     // debug
  pinMode(IRLED, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(DBG, OUTPUT);       // debug

  delay_ten_us(5000);            // Let everything settle for a bit
  
  setupSpiSlave();
  
  // Indicate how big our database is
  DEBUGP(putstring("Ready!\n  "););
}

void setupSpiSlave (void)
{
  // https://gist.github.com/chrismeyersfsu/3317769
 
  // have to send on master in, *slave out*
  pinMode(MISO, OUTPUT);
  
  // turn on SPI in slave mode
  SPCR |= _BV(SPE);
  
  // get ready for an interrupt 
  bits_byte_count = 0;   // buffer empty
  process_it = false;
 
  // now turn on interrupts
  SPI.attachInterrupt();
 
}  // end of setup
 
 
// SPI interrupt routine
ISR (SPI_STC_vect)
{
  byte c = SPDR;  // grab byte from SPI Data Register

  if (pos == 0) {
    bufSize = c;
    pos++;
    return;
  }
  if (pos == 1) {
    bufSize = bufSize | (c << 8);    
    pos++;
    return;
  }
  
  // add to buffer if room
  if (pos < sizeof buf) {
    buf [pos - 2] = c;
    pos++;
    // example: newline means time to process buffer
    if ((pos - 2) == bufSize)
      process_it = true;
      
   }  // end of room available
}  // end of interrupt routine SPI_STC_vect
 
// main loop - wait for flag set in interrupt routine
void loop()
{
  if (process_it) {
    pos = 0;
    process_it = false;
    IrCode code = *(IrCode*)buf;
    
#if DEBUG

    uint16_t calcbufSize = sizeof(IrCode) + code.size_times*sizeof(uint16_t) + code.size_pairs*sizeof(uint8_t);
    
    putstring("\nGot new buffer. sizes: ");
    putnum_ud(calcbufSize);
    putstring(" ");
    putnum_ud(bufSize);
    putstring("\n");
    for (int i = 0; i < bufSize; ++i) {
      putnum_uh(0xff & buf[i]);
      putstring(" ");
    }
    putstring("\n");
#endif

    const uint16_t* times = reinterpret_cast<uint16_t*>(buf + sizeof(IrCode));
    const uint8_t* codes = reinterpret_cast<uint8_t*>(buf + sizeof(IrCode) + code.size_times*sizeof(uint16_t));
    sendCode(code, times, codes);
    quickflashLEDx(3);
 
    
#if DEBUG
    uint16_t  checksum = 0;
    for (int i = 0; i < bufSize; ++i) {
        checksum += buf[i];
    }    


    putstring("\nchecksum: ");
    putnum_ud(checksum);
    putstring("\n");
#endif
 
    } else {
     delay(1000);
    } // end of flag set
    
}  // end of loop
void sendCode(IrCode code, const uint16_t* times, const uint8_t* codes) {


    // Read the carrier frequency from the first byte of code structure
    const uint8_t freq = (code.freq == 0) ? 0 : freq_to_timerval(code.freq);
    // set OCR for Timer1 to output this POWER code's carrier frequency
    OCR2A = freq;
    OCR2B = freq / 3; // 33% duty cycle


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
    for (uint8_t k=0; k<numpairs; k++) {
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


