#include <Arduino.h>

#define IRpin_PIN PIND
#define IRpin 2

#define MARK_THRESHOLD_BIT_HEADER 2000
#define SPACE_THRESHOLD_ZERO_ONE 800
#define SPACE_THRESHOLD_ONE_HEADER 1500
#define SPACE_THRESHOLD_HEADER_PAUSE 8000

#define MAXPULSE 65000
#define RESOLUTION 20

static char symbols[512];
static uint16_t currentpulse = 0;
static byte byteCount = 0;
static byte bytes[64];

static void receivePulses();
static void decodeSymbols();

#ifdef IR_DECODER_DEBUG
static void printDecodedBytes();
#endif

int decoder_loop(byte **ret_bytes) {
    memset(symbols, 0, sizeof(symbols));
    memset(bytes, 0, sizeof(bytes));

    currentpulse=0;
    byteCount=0;

    receivePulses();
    decodeSymbols();

#ifdef IR_DECODER_DEBUG
    Serial.print(F("\nNumber of symbols: "));
    Serial.println(currentpulse);
    Serial.println(F("Symbols:"));
    Serial.println(symbols+1); // Print the symbols (0, 1, H, h, W)
    Serial.println(F("Bytes:"));
    printDecodedBytes();
#endif

    if (ret_bytes != NULL && byteCount > 0) {
        *ret_bytes = bytes;
    }
    return byteCount;
}

static void receivePulses() {
    uint16_t highpulse, lowpulse;  // temporary storage timing

    while (currentpulse < sizeof(symbols))
    {
        highpulse = 0;
        while (IRpin_PIN & (1 << IRpin)) {
            // pin is still HIGH

            // count off another few microseconds
            highpulse++;
            delayMicroseconds(RESOLUTION);

            // timeout
            if ((highpulse >= MAXPULSE) && (currentpulse != 0)) {
                return;
            }
        }

        highpulse = highpulse * RESOLUTION;

        if (currentpulse > 0)
        {
            // this is a SPACE
            if ( highpulse > SPACE_THRESHOLD_HEADER_PAUSE ) {
                symbols[currentpulse] = 'W';
                // Cumulative moving average, see http://en.wikipedia.org/wiki/Moving_average#Cumulative_moving_average
            } else if ( (currentpulse > 0 && symbols[currentpulse-1] == 'H') || highpulse > SPACE_THRESHOLD_ONE_HEADER ) {
                symbols[currentpulse] = 'h';
                // Cumulative moving average, see http://en.wikipedia.org/wiki/Moving_average#Cumulative_moving_average
            } else if ( highpulse > SPACE_THRESHOLD_ZERO_ONE ) {
                symbols[currentpulse] = '1';
            } else {
                symbols[currentpulse] = '0';
            }
        }
        currentpulse++;

        // same as above
        lowpulse = 0;
        while (! (IRpin_PIN & _BV(IRpin))) {
            // pin is still LOW
            lowpulse++;
            delayMicroseconds(RESOLUTION);
            if ((lowpulse >= MAXPULSE)  && (currentpulse != 0)) {
                return;
            }
        }

        // this is a MARK
        lowpulse = lowpulse * RESOLUTION;

        if ( lowpulse > MARK_THRESHOLD_BIT_HEADER ) {
            symbols[currentpulse] = 'H';
            currentpulse++;
        }

        // we read one high-low pulse successfully, continue!
    }
}

static void decodeSymbols() {
  int bitCount = 0;
  byte currentByte = 0;

  // Decode the string of bits to a byte array
  for (uint16_t i = 0; i < currentpulse; i++) {
    if (symbols[i] == '0' || symbols[i] == '1') {
      currentByte >>= 1;
      bitCount++;

      if (symbols[i] == '1') {
        currentByte |= 0x80;
      }

      if (bitCount == 8) {
        bytes[byteCount++] = currentByte;
        bitCount = 0;
      }
    } else { // Ignore bits which do not form octets
      bitCount = 0;
      currentByte = 0;
    }
  }
}

#if IR_DECODER_DEBUG
static void printDecodedBytes() {
  for (int i = 0; i < byteCount; i++) {
    if (bytes[i] < 0x10) {
      Serial.print(F("0"));
    }
    Serial.print(bytes[i],HEX);
    if ( i < byteCount - 1 ) {
      Serial.print(F(","));
    }
  }
  Serial.println();
}
#endif
