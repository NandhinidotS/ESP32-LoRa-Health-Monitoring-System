#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include "MAX30100_PulseOximeter.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// DHT11
#define DHT_PIN 4
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

// MAX30100
#define MAX_SDA 21
#define MAX_SCL 22

PulseOximeter pox;

// LoRa
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 26

unsigned long lastSend = 0;

void setup()
{
  Serial.begin(115200);

  Wire.begin(MAX_SDA, MAX_SCL);

  dht.begin();

  Serial.println("Initializing MAX30100...");

  if (!pox.begin())
  {
    Serial.println("MAX30100 FAILED!");
    while (1);
  }

  pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);

  Serial.println("MAX30100 READY");

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6))
  {
    Serial.println("LoRa initialization failed!");
    while (1);
  }

  Serial.println("LoRa READY");
}

void loop()
{
  // MUST run continuously
  pox.update();

  // Send data every 2 seconds
  if (millis() - lastSend >= 2000)
  {
    lastSend = millis();

    float heartRate = pox.getHeartRate();
    uint8_t spo2 = pox.getSpO2();

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    Serial.print("Heart Rate: ");
    Serial.print(heartRate);
    Serial.println(" BPM");

    Serial.print("SpO2: ");
    Serial.print(spo2);
    Serial.println(" %");

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");

    String data = "HR:" + String(heartRate, 2) +
                  ",SPO2:" + String(spo2) +
                  ",TEMP:" + String(temperature, 1) +
                  ",HUM:" + String(humidity, 1);

    Serial.print("LoRa TX: ");
    Serial.println(data);

    LoRa.beginPacket();
    LoRa.print(data);
    LoRa.endPacket();

    Serial.println("Transmission: SUCCESS");
    Serial.println("--------------------");
  }
}
