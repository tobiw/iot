#include <Arduino.h>
#include <SoftwareSerial.h>
#include <avr/io.h>
#include <avr/wdt.h>

#define PIN_BUTTON 2

SoftwareSerial ss(4, 3); // RX, TX

/* Light parameters: once PWM is supported (requires IRFZ44N + NPN + PNP transistors),
 * set pwm (1-254) instead of binary on/off. */
const uint8_t DEFAULT_PWM = 25, MAX_PWM = 50;
bool lights_on = false;
bool lights_on_last = false;
uint8_t pwm = DEFAULT_PWM;
uint8_t prev_pwm = DEFAULT_PWM;
uint8_t current_effect = 0; // 0 = constant, 1 = timer, 2 = fade

inline void process_msg_L(char *buf)
{
  lights_on = buf[1] == '1';
  pwm = lights_on ? DEFAULT_PWM : 0;
}

inline void process_msg_B(char *buf)
{
  char buf2[3] = { buf[1], buf[2], '\0' };
  pwm = atoi(buf2);
  lights_on = pwm == 0 ? false : true;
}

inline void process_msg_E(char *buf)
{
  char buf2[2] = { buf[1], '\0' };
  current_effect = max(atoi(buf2), 2);
}

inline void send_status()
{
  static char serbuf[10];
  sprintf(serbuf, "L%uB%02uE%u\r\n", lights_on ? 1 : 0, pwm, current_effect);
  ss.write(serbuf);
  delay(100);
}

unsigned long last_button_press = 0;
bool last_button_state = HIGH;
bool button_pressed = false;
unsigned long turn_lights_off_at = 0;

void setup() {
  ss.begin(9600); // Serial comms to ESP-01

  // Setup PWM output
  DDRB |= 1<<DDB1;
  PORTB |= 1<<PB1;
  TCCR1 = 1<<PWM1A | 2<<COM1A0 | 1<<CS12 | 1<<10;
  OCR1A = 0;

  wdt_enable(WDTO_8S);
  wdt_reset();
  
  pinMode(PIN_BUTTON, INPUT_PULLUP); // button
}

void loop() {
  wdt_reset();

  // Receive serial data
  int buf_i = 0;
  char buf[8];
  while (ss.available() > 0) {
    buf[buf_i] = ss.read();
    if (buf_i < 7) buf_i++;
    delay(5);
  }

  /*
   * Translate received message to variables and commands
   * - L(0|1)
   * - B[0-9]]{2}
   * - E[0-2]
   */
  if (buf_i >= 2) {
    if (buf[0] == 'L') { // on or off message
      process_msg_L(buf);
    } else if (buf[0] == 'B') { // brightness setting message
      process_msg_B(buf);
    } else if (buf[0] == 'E') { // effect message
      process_msg_E(buf);
      send_status(); // send status here because not handled at bottom of loop
    }
  }

  // Check button
  const int btn = digitalRead(PIN_BUTTON);
  const unsigned long now = millis();
  if (btn != last_button_state) {
    if (btn == LOW && now - last_button_press > 200) { // 200 ms debounce
      button_pressed = true; // mark button as pressed, see short or long press below
      last_button_press = now;
    } else if (btn == HIGH && now - last_button_press < 1000) { // short press
      lights_on = !lights_on; // toggle lights

#ifdef DEBUG_SERIAL
      ss.print("Button press: toggle lights_on to ");
      ss.println(lights_on);
#endif
      
      pwm = lights_on ? DEFAULT_PWM : 0; // set pwm depending on lights_on state
      turn_lights_off_at = 0; // lights turned on or off normally: disable timer
      current_effect = 0;
    } else if (btn == HIGH && now - last_button_press >= 1000) { // long press
#ifdef DEBUG_SERIAL
      ss.println("Button long press");
#endif

      lights_on = true; // turn lights on now
      pwm = DEFAULT_PWM;
      current_effect = 1;
      turn_lights_off_at = now + 30000; // enable 30 second timer to turn lights off
    }
    last_button_state = btn;
  }

  // Turn lights off automatically if timer has been set
  if (turn_lights_off_at != 0 && now >= turn_lights_off_at) {
    lights_on = false;
    //pwm = 0;
    turn_lights_off_at = 0;
    current_effect = 0;
  }
  
  if (lights_on_last != lights_on || prev_pwm != pwm) {
    if (!lights_on) pwm = 0;

#ifdef DEBUG_SERIAL
    ss.print("new lights_on or pwm -> ");
#endif

    if (pwm == 0) {
      OCR1A = lights_on ? DEFAULT_PWM : 0; // set MOSFET output (never turn on 100%)
    } else {
      if (pwm > MAX_PWM) pwm = MAX_PWM;
      OCR1A = pwm;
    }

#ifdef DEBUG_SERIAL
    ss.print("OCR1A set to ");
    ss.println(pwm);
#endif

    send_status();

#ifdef DEBUG_SERIAL
    ss.println("");
#endif
    
    lights_on_last = lights_on;
    prev_pwm = pwm;
  }
}
