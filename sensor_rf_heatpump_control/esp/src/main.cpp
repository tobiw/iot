#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include "DHT.h"

//#define SSD1306_128_64
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "settings.h"

#define DHTPIN 12
#define DHTTYPE DHT22

#define OLED_RESET 13
Adafruit_SSD1306 display(OLED_RESET);

#define IRLED_PIN 14

DHT dht(DHTPIN, DHTTYPE);

#define HAVE_MQTT 1

#if HAVE_MQTT
#include <PubSubClient.h>
WiFiClient esp_client;
PubSubClient mqtt_client(esp_client);
#endif

#include <MitsubishiHeatpumpIR.h>
IRSenderBitBang irSender(14);
HeatpumpIR *heatpumpIR = new MitsubishiFDHeatpumpIR();

void heatpump_loop(HeatpumpIR *heatpump) {
  const char* buf;
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  display.print("Heatpump");
  display.setTextSize(1);

  buf = heatpump->model();
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

   display.display();

  // Send the IR command
  heatpump->send(irSender, POWER_ON, MODE_HEAT, FAN_AUTO, 20, VDIR_SWING, HDIR_AUTO);

  delay(5000);
}

void display_sensor(float t, float h) {
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
}

void publish_sensor(float t, float h) {
  char str_temp[6];
  dtostrf(t, 4, 1, str_temp);

  char msg[32];
  snprintf(msg, 32, "{ \"temp\": %s, \"rh\": %d }", str_temp, (int)h);
  
  mqtt_client.publish(TOPIC_SENSOR, msg);
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived: ");
  Serial.println(topic);

  char payload_str[8] = { 0 };
  for (int i = 0; i < length; i++) {
    payload_str[i] = (char)payload[i];
  }

  Serial.print("Assembled payload: ");
  Serial.println(payload_str);

  // Homie node will be: $name = Heatpump, $properties = temperature
  if (strcmp(topic, TOPIC_HEATPUMP) == 0) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println("Heatpump");

    // Payload should be an integer (temperature)
    uint8 payload_int = atoi(payload_str);
        
    if (payload_int == 0) {
      display.println("Turning");
      display.println("off");
      display.display();
      heatpumpIR->send(irSender, POWER_OFF, MODE_HEAT, FAN_AUTO, 18, VDIR_SWING, HDIR_AUTO);
    } else if (payload_int >= 18 && payload_int <= 24) {
      display.println("Turning on");
      display.print(payload_int);
      display.println(" C");
      display.display();
      heatpumpIR->send(irSender, POWER_ON, MODE_HEAT, FAN_AUTO, payload_int, VDIR_SWING, HDIR_AUTO);
    } else {
      display.println("Invalid payload");
      delay(1000);
    }
  }
}

#if HAVE_MQTT
void mqtt_reconnect() {
  if (!mqtt_client.connected()) { // WHILE
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect(MQTT_CLIENT_NAME)) {
      Serial.println("connected");
      
      mqtt_client.subscribe(TOPIC_HEATPUMP);
      
      display.println("MQTT connected");
      display.display();
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}
#endif

void setup() {
  pinMode(IRLED_PIN, OUTPUT);
  digitalWrite(IRLED_PIN, LOW);
  
  Wire.begin(5, 4);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  delay(500);
  
  Serial.begin(115200);
  delay(10);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Connecting");
  display.print(".");
  display.display();
    
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
  }

  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("WiFi connected");
  display.display();
  delay(2000);

  dht.begin();

#if HAVE_MQTT
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(mqtt_callback);
#endif
}

unsigned long last_sensor_readout = 0;

void loop() {
#if HAVE_MQTT
 if (!mqtt_client.connected()) {
    mqtt_reconnect();
  }
  mqtt_client.loop();
#endif

  unsigned long now = millis();
  if (last_sensor_readout == 0 || now - last_sensor_readout > 60000) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    while (isnan(t) || isnan(h)) {
      Serial.println("Failed to get valid t or h.");
      delay(1000);
      h = dht.readHumidity();
      t = dht.readTemperature();
    }
    last_sensor_readout = now;

    display_sensor(t, h);
    publish_sensor(t, h);
  }

  delay(100);
}
