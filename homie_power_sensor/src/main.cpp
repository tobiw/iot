/*
 * Use https://github.com/jpmens/homie-ota to inventory and OTA update.
 * 
 * Upload data/config.json with
 * curl -X PUT http://192.168.../config --header "Content-Type: application/json" -d @config.json
 * or upload via platformio/esptool with
 * pio run -t uploadfs
 */

#include <Homie.h>
#include "TM1637Display.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define FW_NAME "house-power-esp"
#define FW_VERSION "1.2.2"
/* Magic sequence for Autodetectable Binary Upload */
const char *__FLAGGED_FW_NAME = "\xbf\x84\xe4\x13\x54" FW_NAME "\x93\x44\x6b\xa7\x75";
const char *__FLAGGED_FW_VERSION = "\x6a\x3f\x3e\x0e\xe1" FW_VERSION "\xb0\x30\x48\xd4\x1a";
/* End of magic sequence for Autodetectable Binary Upload */

#define LED_PIN 14
#define PULSE_LED_PIN 2 // Built-in LED on ESP12
#define PULSE_SENSOR_PIN 13
#define TEMP_SENSOR_PIN 12
#define DISPLAY_CLK_PIN 4
#define DISPLAY_DIO_PIN 5

TM1637Display display(DISPLAY_CLK_PIN, DISPLAY_DIO_PIN);

OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature temp_sensor(&oneWire);

unsigned long lastPowerSent = 0;
unsigned long lastTemperatureSent = 0;
int lastPulseReading = HIGH;

const unsigned long measurement_interval = 60000UL; // process pulses every minute
const float watthour_multiplier = 3600000UL / measurement_interval;
float pulsesPerKWh = 1.0; // configurable via MQTT
volatile unsigned long pulseCount = 0;
unsigned long totalPulses = 0;

typedef enum {
  LED_OFF = 0,
  LED_ON,
  LED_FLASH,
  LED_FAST_FLASH
} led_mode_t;

led_mode_t led_mode = LED_OFF;
unsigned long last_led_update = 0;
uint8_t last_led_status = LOW;
bool led_changed_to_off = false;

bool led_message_handler(const HomieRange& range, const String& value) {
  digitalWrite(LED_PIN, HIGH);
  if (value == "1") {
    led_mode = LED_ON;
  } else if (value == "2") {
    led_mode = LED_FLASH;
  } else if (value == "3") {
    led_mode = LED_FAST_FLASH;
  } else {
    led_mode = LED_OFF;
    led_changed_to_off = true;
  }
  Homie.getLogger() << "LED config done" << endl;
  return true;
}

bool pulses_per_kwh_message_handler(const HomieRange& range, const String& value) {
  float f = strtof(value.c_str(), NULL);
  if (f > 0.0 && f < 100.0) {
    pulsesPerKWh = f;
    Homie.getLogger() << "pulsesPerKWh set to " << f << endl;
  }
  return true;
}

bool reset_wh_message_handler(const HomieRange& range, const String& value) {
  totalPulses = 0;
  Homie.getLogger() << "totalPulses reset done" << endl;
  return true;
}

// id, name, type
HomieNode powerNode("house-power", "sensors", "unsigned long");
HomieNode ledNode("alarm", "led", "unsigned long"); // set:settable
//HomieNode sensorsNode("sensors", "sensors");

void setupHandler() {
  powerNode.advertise("watts");
  powerNode.advertise("watthours").settable(reset_wh_message_handler);
  //sensorsNode.advertise("temperature");
  ledNode.advertise("mode").settable(led_message_handler);
  ledNode.advertise("pulses-per-kwh").settable(pulses_per_kwh_message_handler);
  
  display.setBrightness(0x0a);
  display.showNumberDec(8888);

  temp_sensor.begin();

  Homie.getLogger() << "setupHandler done" << endl;
}

void loopHandler() {
  // Check for falling edge on LED sensor pin
  int pulse = digitalRead(PULSE_SENSOR_PIN);
  if (pulse == LOW && lastPulseReading == HIGH) { // falling edge
    pulseCount++;
    totalPulses++;
    lastPulseReading = LOW;
    delay(50);
  } else if (pulse == HIGH) {
    lastPulseReading = HIGH;
  }
  digitalWrite(PULSE_LED_PIN, lastPulseReading);

  const unsigned long now = millis();

  // Calculate power consumption and send
  if (now - lastPowerSent >= 60000 || lastPowerSent == 0) { // every minute
    float p = pulseCount / pulsesPerKWh;
    p *= watthour_multiplier; // convert to Watts
    display.showNumberDec((int)p, false);
    powerNode.setProperty("watts").send(String((int)p));

    p = totalPulses / pulsesPerKWh;
    powerNode.setProperty("watthours").send(String((int)p));
    
    lastPowerSent = now;
    pulseCount = 0;
  }

  // Measure and send temperature
  #if 0
  if (now - lastTemperatureSent >= 300000 || lastTemperatureSent == 0) { // every 5 minutes
    temp_sensor.requestTemperatures();
  }
  if (now - lastTemperatureSent >= 300500 || lastTemperatureSent == 0) { // every 5 minutes
    float t = temp_sensor.getTempCByIndex(0);
    //sensorsNode.setProperty("temperature").send(String(t));
    lastTemperatureSent = now;
  }
  #endif

  // Update alarm LED if enabled
  if (led_mode == LED_OFF && led_changed_to_off) {
    digitalWrite(LED_PIN, LOW);
    last_led_status = LOW;
    last_led_update = now;
    led_changed_to_off = false;
  } else if (led_mode == LED_ON && last_led_status == LOW) {
    digitalWrite(LED_PIN, HIGH);
    last_led_status = HIGH;
    last_led_update = now;
  } else if (led_mode == LED_FLASH || led_mode == LED_FAST_FLASH) {
    const unsigned int flashfreq = led_mode == LED_FLASH ? 1000 : 200;
    if (now - last_led_update >= flashfreq) {
      last_led_status = last_led_status == LOW ? HIGH : LOW;
      digitalWrite(LED_PIN, last_led_status);
      last_led_update = now;
    }
  }
}

void setup() {
  //attachInterrupt(digitalPinToInterrupt(PULSE_SENSOR_PIN), onPulse, FALLING);
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(PULSE_LED_PIN, OUTPUT);
  
  Homie_setFirmware(FW_NAME, FW_VERSION);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);
  //Homie.disableLedFeedback();
  //Homie.setLedPin(LED_PIN, HIGH);
  Homie.setup();
}

void loop() {
  Homie.loop();
}
