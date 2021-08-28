#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "led_strip_settings.h"

//#define DEBUG_SERIAL 1

int lights_on_status = -1, prev_lights_on_status = -1; // current status from ATtiny85
int brightness = -1, prev_brightness = -1;
int effect = -1, prev_effect = -1;

/* Setting the brightness slider will result in a brightness message followed by a lights_on message.
 * Do not send lights_on if brightness has been sent.
 */
unsigned long last_brightness_msg = 0;

/* MQTT callback when message is received
 * Translate MQTT message into message to ATtiny
 * payload for each topic can be:
 * - "/set": 0-1: off or on (use last brightness)
 * - "/brightness/set": 0-50
 * - "/effect/set": 0-2 (0 = constant, 1 = timer, 2 = fade)
 */
void mqtt_cb(char* topic, byte* payload, unsigned int length) {
  static char msgbuf[6];
  uint8_t i = 0;
  if (strcmp(topic, TOPIC "/set") == 0) {
    if (millis() - last_brightness_msg < 1000) return; // skip sending message
    msgbuf[0] = 'L';
    msgbuf[1] = payload[0] == '1' ? '1' : '0';
    i = 2;
  } else if (strcmp(topic, TOPIC "/brightness/set") == 0) {
    msgbuf[0] = 'B';
    msgbuf[1] = payload[0];
    msgbuf[2] = payload[1];
    i = 3;
    last_brightness_msg = millis();
  } else if (strcmp(topic, TOPIC "/effect/set") == 0) {
    msgbuf[0] = 'E';
    // Translate effect word to integer (0 = constant, 1 = timer, 2 = fade)
    if (payload[0] == 't') { // "timer"
      msgbuf[1] = '1';
    } else if (payload[0] == 't') { // "fade"
      msgbuf[1] = '2';
    } else { // "constant"
      msgbuf[1] = '0';
    }
    i = 2;
  }
  
  msgbuf[i] = '\r';
  msgbuf[i+1] = '\n';
  msgbuf[i+2] = '\0';
  Serial.write(msgbuf);
  delay(100);
}

WiFiClient client;
PubSubClient mqtt_client(mqtt_server, 1883, mqtt_cb, client);

void publish_status() {
  // Construct message
  char buf[64], buf_ip[16];
  unsigned long ip_int = (unsigned long)WiFi.localIP();
  sprintf(buf_ip, "%lu.%lu.%lu.%lu", (ip_int & 0xf000) >> 24, (ip_int & 0xf00) >> 16, (ip_int & 0xf0) >> 8, (ip_int & 0xf));
  sprintf(buf, "{ \"running\": 1, \"ip\": %s, \"signal\": %d }", buf_ip, WiFi.RSSI());
  
#ifdef TOPIC_IOT_STATUS
  mqtt_client.publish(TOPIC_IOT_STATUS, buf);
#endif
  mqtt_client.publish(TOPIC, lights_on_status == 1 ? "1" : "0");
}

void setup() {
  Serial.begin(9600); // Serial comms to ATtiny85
  delay(500);
  Serial.println("Hello");
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  byte retry = 0;
  int r;
  mqtt_client.setCallback(mqtt_cb);
  while (!(r = mqtt_client.connect("led-strip", mqtt_user, mqtt_password)) && retry++ < 2);
  
  mqtt_client.subscribe(TOPIC "/set");
  mqtt_client.subscribe(TOPIC "/brightness/set");
  mqtt_client.subscribe(TOPIC "/effect/set");
  publish_status();
}

unsigned long last_status_sent = 0;

void loop() {
  mqtt_client.loop(); // process incoming MQTT messages

  /* ATtiny85 will send current lights status when it changes:
   *  L(0|1)B(0-50)E(0|1|2)
   *  L: lights off or on
   *  B: brightness value (always 2 chars)
   *  E: current effect (0 = constant light)
   *  to be passed on to home-assistant
   */
  uint8_t buf_i = 0;
  char buf[16];
  while (Serial.available() > 0) {
    buf[buf_i] = Serial.read();
    if (buf_i < 15) buf_i++;
    delay(5);
  }
  buf[buf_i] = '\0';

#ifdef DEBUG_SERIAL
  if (buf_i > 1) {
    Serial.print("Message received: ");
    Serial.println(buf);
  }
#endif

  const uint8_t msg_len = buf_i;
  
  for (buf_i = 0; buf_i < msg_len - 1; buf_i++) {
    if (buf[buf_i] == 'L') { // read lights status in next char
      lights_on_status = buf[buf_i + 1] == '1' ? 1 : 0;
#ifdef DEBUG_SERIAL
        Serial.print("MQTT publish lights: ");
        Serial.print(lights_on_status);
        Serial.print(" (prev: ");
        Serial.print(prev_lights_on_status);
        Serial.println(")");
#endif
      if (lights_on_status != prev_lights_on_status) {
        mqtt_client.publish(TOPIC, lights_on_status == 1 ? "1" : "0");
        prev_lights_on_status = lights_on_status;
      }
    } else if (buf[buf_i] == 'B') {
      char digits_buf[3] = { buf[buf_i + 1], buf[buf_i + 2], '\0' };
      brightness = atoi(digits_buf);
#ifdef DEBUG_SERIAL
        Serial.print("MQTT publish brightness: ");
        Serial.print(brightness);
        Serial.print(" (prev: ");
        Serial.print(prev_brightness);
        Serial.println(")");
#endif
      if (brightness != prev_brightness) {
        mqtt_client.publish(TOPIC "/brightness", digits_buf);
        prev_brightness = brightness;
      }
    } else if (buf[buf_i] == 'E') {
      char digits_buf[2] = { buf[buf_i + 1], '\0' };
      effect = atoi(digits_buf);
#ifdef DEBUG_SERIAL
        Serial.print("MQTT publish effect: ");
        Serial.print(effect);
        Serial.print(" (prev: ");
        Serial.print(prev_effect);
        Serial.println(")");
#endif
      if (effect != prev_effect) {
        mqtt_client.publish(TOPIC "/effect", digits_buf);
        prev_effect = effect;
      }

      break;  // done parsing status message
    }
  }

  const unsigned long now = millis();

  if (now - last_status_sent > 60000) {
    publish_status();
    last_status_sent = now;
  }

  delay(100);
}
