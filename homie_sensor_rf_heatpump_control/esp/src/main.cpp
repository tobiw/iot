#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
//#include <Wire.h>
#include "DHT.h"
#include <Homie.h>
#include <SoftwareSerial.h>

SoftwareSerial ss(12, 14); // RX, TX

HomieNode sensorsNode("sensors", "sensors");
HomieNode heatpumpNode("heatpump", "switch");
HomieNode rfremoteNode("rfremote", "switch");

/*#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define TWI_SDA_PIN 5
#define TWI_SCL_PIN 4
*/

#define DHTPIN 13
#define DHTTYPE DHT22

//#define IRLED_PIN 14

/*#define OLED_RESET 12
Adafruit_SSD1306 display(OLED_RESET);*/

DHT dht(DHTPIN, DHTTYPE);

//#include <MitsubishiHeatpumpIR.h>
//IRSenderBitBang irSender(IRLED_PIN);
//HeatpumpIR *heatpumpIR = new MitsubishiFDHeatpumpIR();

#if 0
void heatpump_command(int temperature) {
  /*const char* buf;
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.print("Heatpump");
  display.setTextSize(1);*/

  /*buf = heatpump->model();
  while (char modelChar = pgm_read_byte(buf++))
  {
    display.print(modelChar);
  }
  
  display.println("");

  buf = heatpump->info();
  while (char infoChar = pgm_read_byte(buf++))
  {
    display.print(infoChar);
  }

   display.display();*/

  // Send the IR command
  if (temperature == 0) {
    heatpumpIR->send(irSender, POWER_OFF, MODE_HEAT, FAN_AUTO, 18, VDIR_SWING, HDIR_AUTO);
  } else {
    heatpumpIR->send(irSender, POWER_ON, MODE_HEAT, FAN_AUTO, temperature, VDIR_SWING, HDIR_AUTO);
  }
}
#endif

bool heatpump_message_handler(const HomieRange& range, const String& value) {
  if (value == "true" || value == "1") {
    //heatpump_command(18);
    ss.println("ac:18");
  } else if (value == "false" || value == "0") {
    //heatpump_command(0);
    ss.println("ac:0");
  } else {
    //heatpump_command(atoi(value.c_str()));
    Homie.getLogger() << "Received temperature: " << value << endl;
    char buf[8];
    sprintf(buf, "ac:%s", value.c_str());
    ss.println(buf);
  }
  return true;
}

bool rfremote1_message_handler(const HomieRange& range, const String& value) {
  if (value == "true" || value == "1") {
    ss.println("rf1:1");
  } else if (value == "false" || value == "0") {
    ss.println("rf1:0");
  }
}

bool rfremote2_message_handler(const HomieRange& range, const String& value) {
  if (value == "true" || value == "1") {
    ss.println("rf2:1");
  } else if (value == "false" || value == "0") {
    ss.println("rf2:0");
  }
}

bool rfremote3_message_handler(const HomieRange& range, const String& value) {
  if (value == "true" || value == "1") {
    ss.println("rf3:1");
  } else if (value == "false" || value == "0") {
    ss.println("rf3:0");
  }
}

/*void display_sensor(float t, float h) {
  char str_temp[6];
  dtostrf(t, 4, 1, str_temp);
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println("Sensors");
  display.print("Temp: ");
  display.println(str_temp);
  display.print("Hum: ");
  display.print((int)h);
  display.println("%");
  display.display();
}*/

void setup_handler() {
  //pinMode(IRLED_PIN, OUTPUT);
  //digitalWrite(IRLED_PIN, LOW);

  /*Wire.begin(TWI_SDA_PIN, TWI_SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.println("Setup");
  display.display();*/
  
  dht.begin();
}

unsigned long last_sensor_readout = 0;

void loop_handler() {
  //sendSensors();

  unsigned long now = millis();
  if (last_sensor_readout == 0 || now - last_sensor_readout > 300000) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    int retries = 10;
    while ((isnan(t) || isnan(h)) && retries-- > 0) {
      Homie.getLogger() << "Failed to get valid t or h." << endl;
      delay(50);
      h = dht.readHumidity();
      t = dht.readTemperature();
    }

    if (retries == 0) {
      t = h = 0;
    }

    //display_sensor(t, h);

    if (!isnan(t) && !isnan(h)) {
      sensorsNode.setProperty("temperature").send(String(t));
      sensorsNode.setProperty("humidity").send(String(h));
    }

    last_sensor_readout = now;
  }
}

void setup() {
  Homie.disableLedFeedback();
  Serial.begin(9600);
  ss.begin(9600);
  
  Homie_setFirmware("livingroom", "1.1.1");

  Homie.setSetupFunction(setup_handler).setLoopFunction(loop_handler);

  sensorsNode.advertise("temperature");
  sensorsNode.advertise("humidity");
  heatpumpNode.advertise("temperature").settable(heatpump_message_handler);
  rfremoteNode.advertise("remote1").settable(rfremote1_message_handler);
  rfremoteNode.advertise("remote2").settable(rfremote2_message_handler);
  rfremoteNode.advertise("cornerlight").settable(rfremote3_message_handler);

  Homie.setup();
}

void loop() {
  Homie.loop();
}


void sendSensors() {
  char t_buf[8], h_buf[4];
  int i = 0;
  sprintf(t_buf, "0.0");
  sprintf(h_buf, "0");
  while (ss.available() > 0) {
    delay(50);
    char c = ss.read();
    Homie.getLogger() << "Received character" << c << " i=" << i << endl;
    if (i < 5) {
      t_buf[i++] = c;
    } else if (i > 5) {
      h_buf[i++ - 6] = c;
    } else {
      i++;
    }
  }

  if (i > 7) {
    t_buf[5] = h_buf[3] = '\0';
    Homie.getLogger() << "Assembled msg: " << t_buf << " / " << h_buf << endl;
    sensorsNode.setProperty("temperature").send(String(t_buf));
    sensorsNode.setProperty("humidity").send(String(h_buf));
  }
}
