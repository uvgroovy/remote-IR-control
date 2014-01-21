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

#include "ircntrl.h"
#include <avr/sleep.h>

uint8_t buf [256];

volatile uint16_t bufSize = 0;
volatile byte pos = 0;
volatile boolean process_it = false;

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


#define FALSE 0
#define TRUE 1


void setup()   {

 ir_init();
  
  
}

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

