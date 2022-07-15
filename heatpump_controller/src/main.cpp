#include <Arduino.h>

#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

#include "ir_sender.h"
#include "secrets.h"


byte mac[] = { 0xb2, 0xa1, 0xa0, 0x8a, 0x23, 0x45 };
IPAddress ip_addr(10, 1, 1, 173);  // TODO: DHCP
IPAddress ip_gw(10, 1, 0, 2);
IPAddress netmask(255, 255, 0, 0);
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;
#define TOPIC_IOT_STATUS "iot/ir-sender"
#define TOPIC "home/ir-sender"

EthernetClient client;
PubSubClient mqtt_client;

bool new_heatpump_setting = false;
uint8_t current_temperature = 0;

void mqtt_cb(char* topic, byte* payload, unsigned int length) {
  static char msgbuf[10];
  uint8_t i;

  for (i = 0; i < 8 && i < length; i++) {
      msgbuf[i] = payload[i];
  }
  msgbuf[i] = '\0';

  if (msgbuf[0] == 'T') {
      current_temperature = atoi(&msgbuf[1]);
      new_heatpump_setting = true;
      if (current_temperature < 16) current_temperature = 16;
      else if (current_temperature > 28) current_temperature = 28;
      Serial.print("Got temperature: ");
      Serial.println(current_temperature);
  }
  //mqtt_client.publish(TOPIC "/echo", msgbuf);
  delay(100);
}

void mqtt_connect() {
    byte retry = 0;
    int r;
    mqtt_client.setClient(client);
    mqtt_client.setServer("10.1.2.1", 1883);
    mqtt_client.setCallback(mqtt_cb);
    while (!(r = mqtt_client.connect("ir-sender", TOPIC_IOT_STATUS "/online", 1, true, "false")) && retry++ < 20);
    if (!mqtt_client.connected()) {
        Serial.println("ERROR: Failed to connect to MQTT server");
        while (1) { delay(1); }
    }
    mqtt_client.subscribe(TOPIC "/set");
    mqtt_client.publish(TOPIC_IOT_STATUS "/online", "true", true);
}

void setup() {
    Serial.begin(115200);
    delay(1);
    Serial.println("Setup start ...");

    Ethernet.begin(mac, ip_addr, ip_gw, ip_gw, netmask);
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("ERROR: no Ethernet hardware");
        while (1) { delay(1); }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("ERROR: link down");
        while (1) { delay(1); }
    }

    Serial.print("Ethernet link is up: ");
    Serial.println(Ethernet.localIP());
    delay(100);
    mqtt_connect();

    Serial.println("MQTT connected.");

    pinMode(13, OUTPUT);
    pinMode(PIN_IR, OUTPUT);
    digitalWrite(PIN_IR, LOW);

    digitalWrite(13, HIGH);
    delay(1000);
    digitalWrite(13, LOW);

    Serial.println("Setup done.");
}

void loop() {
    static int i = 0;

    mqtt_client.loop(); // process incoming MQTT messages

    if (new_heatpump_setting) {
        //                    0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15    16    17
        uint8_t data[18] = { 0x23, 0xCB, 0x26, 0x01, 0x00, 0x20, 0x48, 0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x10, 0x40, 0x00, 0x00 };
        ir_mitsubishi_set_mode(1, 0, data);
        ir_mitsubishi_set_fan(MITSUBISHI_AIRCON1_FAN2 | MITSUBISHI_AIRCON1_VS_MIDDLE, data);
        ir_mitsubishi_set_temperature(current_temperature, data);
        ir_mitsubishi_set_2flow(0, data);
        ir_mitsubishi_calculate_checksum(data);
        ir_send_message(data);

        new_heatpump_setting = false;
    }

    delay(100);
}
