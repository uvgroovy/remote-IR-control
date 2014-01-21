
#include <stddef.h>
#include <stdint.h>


// What pins do what
// #define DBG 12 // used by SPOI
#define DBG 9
// #define LED 13 Don't want to mess with the led, as SPI uses pin 13 too
#define IRLED 3

// Lets us calculate the size of the NA/EU databases
#define NUM_ELEM(x) (sizeof (x) / sizeof (*(x)));

// set define to 0 to turn off debug output
#define DEBUG 0
#define DEBUGP(x) if (DEBUG == 1) { x ; }

// Shortcut to insert single, non-optimized-out nop
#define NOP __asm__ __volatile__ ("nop")

// Tweak this if neccessary to change timing
#define DELAY_CNT 11

// Makes the codes more readable. the OCRA is actually
// programmed in terms of 'periods' not 'freqs' - that
// is, the inverse!
#define freq_to_timerval(x) ((F_CPU / x - 1)/ 2)

// The structure of compressed code entries
struct IrCode {
  uint16_t freq;
  uint8_t numpairs;
  uint8_t bitcompression;
  uint8_t size_times;
  uint8_t size_pairs;
};

void ir_init();
void sendCode(struct IrCode code, const uint16_t* times, const uint8_t* codes);

