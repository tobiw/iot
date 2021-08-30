#include <Arduino.h>
#include <SPI.h>
//#include <Wire.h>
#include "DHT.h"
#include <SoftwareSerial.h>
#include <RCSwitch.h>

#define PIN_RF_TX 7
RCSwitch rcswitch = RCSwitch();

void rf_setup() {  
  // Transmitter is connected to Arduino Pin #10  
  rcswitch.enableTransmit(PIN_RF_TX);

  // Optional set pulse length.
  rcswitch.setPulseLength(180);
  
  // Optional set protocol (default is 1, will work for most outlets)
  // rcswitch.setProtocol(2);
  
  // Optional set number of transmission repetitions.
  rcswitch.setRepeatTransmit(8);
}

#define CODE_ON_1  "101110001001111000001100"
#define CODE_OFF_1 "101110001001111000000100"
#define CODE_ON_2  "101110001001111000001010"
#define CODE_OFF_2 "101110001001111000000010"
#define CODE_ON_3  "101110001001111000001001"
#define CODE_OFF_3 "101110001001111000000001"

const char *relay_codes[3][2] = {
  { CODE_OFF_1, CODE_ON_1 },
  { CODE_OFF_2, CODE_ON_2 },
  { CODE_OFF_3, CODE_ON_3 },
};

SoftwareSerial ss(4, 5); // RX, TX

/*#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define TWI_SDA_PIN 5
#define TWI_SCL_PIN 4
*/

#define DHTPIN 2
#define DHTTYPE DHT22

#define IRLED_PIN 3

/*#define OLED_RESET 12
Adafruit_SSD1306 display(OLED_RESET);*/

DHT dht(DHTPIN, DHTTYPE);

#include <MitsubishiHeatpumpIR.h>
IRSenderPWM irSender(IRLED_PIN);
HeatpumpIR *heatpumpIR = new MitsubishiFDHeatpumpIR();

void setup() {
  Serial.begin(9600);
  ss.begin(9600);
  delay(500);
  Serial.println("Starting...");

  pinMode(IRLED_PIN, OUTPUT);
  dht.begin();
  rf_setup();
}

unsigned long last_sensor_send = 0;

#define SYS_HEATPUMP 0
#define SYS_RFREMOTE1 1
#define SYS_RFREMOTE2 2
#define SYS_RFREMOTE3 3

void sendSensors() {
  char t_buf[6];
  float t = dht.readTemperature();
  int h = (int)dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    dtostrf(t, 4, 2, t_buf);
    char msg[10];
    sprintf(msg, "%s,%d", t_buf, h);
    ss.println(msg);
  }
}

int getSerialCommand(int *sys, int *cmd) {
  char buf[32];
  int i = 0;
  while (ss.available() > 0) {
    char c = ss.read();
    buf[i++] = c;
    delay(50);
  }
  buf[i] = '\0';

  if (i > 3 && buf[0] == 'a' && buf[1] == 'c' && buf[2] == ':') {
    *sys = SYS_HEATPUMP;
    *cmd = atoi(&buf[3]);
    return 0;
  } else if (i > 4 && buf[0] == 'r' && buf[1] == 'f' && buf[2] == '1' && buf[3] == ':') {
    *sys = SYS_RFREMOTE1;
    *cmd = buf[4] == '0' ? 0 : 1;
    return 0;
  } else if (i > 4 && buf[0] == 'r' && buf[1] == 'f' && buf[2] == '2' && buf[3] == ':') {
    *sys = SYS_RFREMOTE2;
    *cmd = buf[4] == '0' ? 0 : 1;
    return 0;
  } else if (i > 4 && buf[0] == 'r' && buf[1] == 'f' && buf[2] == '3' && buf[3] == ':') {
    *sys = SYS_RFREMOTE3;
    *cmd = buf[4] == '0' ? 0 : 1;
    return 0;
  }

  return -1;
}

void loop() {
  if (millis() - last_sensor_send > 300000 || last_sensor_send == 0) {
    sendSensors();
    last_sensor_send = millis();
  }

  int r, sys, cmd;
  r = getSerialCommand(&sys, &cmd);
  if (r == 0) {
    Serial.print("Got cmd: "); Serial.print(sys); Serial.print(", "); Serial.println(cmd);
    if (sys == SYS_HEATPUMP) {
      if (cmd == 0) {
        heatpumpIR->send(irSender, POWER_OFF, MODE_HEAT, FAN_AUTO, 18, VDIR_SWING, HDIR_AUTO);
      } else if (cmd >= 16 && cmd <= 26) {
        heatpumpIR->send(irSender, POWER_ON, MODE_HEAT, FAN_AUTO, cmd, VDIR_SWING, HDIR_AUTO);
      }
    } else if (sys >= SYS_RFREMOTE1 && sys <= SYS_RFREMOTE3) {
      rcswitch.send(relay_codes[sys - SYS_RFREMOTE1][cmd]);
    }
  }
}
