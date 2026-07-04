#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>   
#include <ScioSense_ENS160.h> 
#include <WiFi.h>
#include <HTTPClient.h> 

const char* ssid     = "Caius";      
const char* password = "Lambo2000";    

const char* serverUrl = "http://172.20.10.11:3000/api/data";


#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4
#define FAN_PWM   13  

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_AHTX0 aht;
ScioSense_ENS160 ens160(ENS160_I2CADDR_0); 

float temp = 22.0, hum = 50.0;
int aqi_logic = 1; 
int tvoc = 0, eco2 = 400;
int currentRawPower = 5; 
unsigned long lastUpdate = 0;
bool dotState = false;

String regimFan = "auto"; 

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  tft.init(240, 320);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 50);
  tft.println("Conectare WiFi...");

  WiFi.begin(ssid, password);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 15) {
    delay(500);
    tft.print(".");
    retry++;
  }

  tft.fillScreen(ST77XX_BLACK);

  if (!aht.begin()) Serial.println("AHT21 negasit");
  if (!ens160.begin()) {
    ens160 = ScioSense_ENS160(ENS160_I2CADDR_1);
    if (!ens160.begin()) {
      tft.setCursor(20, 100); tft.setTextColor(ST77XX_RED);
      tft.println("EROARE SENZOR AER!"); while(1);
    }
  }
  ens160.setMode(ENS160_OPMODE_STD);

  ledcAttach(FAN_PWM, 25000, 8); 

  tft.drawRect(5, 5, 310, 230, ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 25); tft.setTextColor(ST77XX_CYAN); tft.print("CALITATE AER: ");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(20, 70);  tft.print("CO2: "); 
  tft.setCursor(20, 105); tft.print("TVOC: ");
  tft.setCursor(20, 140); tft.print("Temp: ");
  tft.setCursor(20, 175); tft.print("Umid: ");
  tft.setCursor(20, 210); tft.setTextColor(ST77XX_YELLOW); tft.print("VITEZA FAN: ");
}

void loop() {
  if (millis() - lastUpdate > 2000) {
    
    sensors_event_t humidity, temp_event;
    aht.getEvent(&humidity, &temp_event);
    temp = temp_event.temperature;
    hum = humidity.relative_humidity;

    ens160.set_envdata(temp, hum);
    if (ens160.available()) {
      ens160.measure(true); 
      tvoc = ens160.getTVOC();
      eco2 = ens160.geteCO2();
    }

    if(WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");

      String json = "{\"temp\":" + String(temp) + 
                    ",\"hum\":" + String(hum) + 
                    ",\"co2\":" + String(eco2) + 
                    ",\"tvoc\":" + String(tvoc) + 
                    ",\"fan\":" + String(map(currentRawPower, 0, 255, 0, 100)) + "}";
      
      int httpResponseCode = http.POST(json);
      
      if(httpResponseCode == 200) {
        String response = http.getString();
        Serial.print("Raspuns de la server: "); Serial.println(response);
        
        if (response.indexOf("\"cmd\":\"turbo\"") != -1) {
          regimFan = "turbo";
        } else if (response.indexOf("\"cmd\":\"stop\"") != -1) {
          regimFan = "stop";
        } else {
          regimFan = "auto";
        }
        Serial.print("Regim Fan activat: "); Serial.println(regimFan);
      } else {
        Serial.print("HTTP Error: "); Serial.println(httpResponseCode);
      }
      http.end();
    }

    if (regimFan == "turbo") {
      currentRawPower = 255;
      aqi_logic = 5;
    } 
    else if (regimFan == "stop") {
      currentRawPower = 0;  
      aqi_logic = 1;
    } 
    else { 
      int pCO2 = map(constrain(eco2, 450, 1200), 450, 1200, 5, 255);
      int pTVOC = map(constrain(tvoc, 50, 400), 50, 400, 5, 255);
      currentRawPower = max(pCO2, pTVOC);

      if (currentRawPower <= 10) aqi_logic = 1;
      else if (currentRawPower < 80) aqi_logic = 2;
      else if (currentRawPower < 150) aqi_logic = 3;
      else if (currentRawPower < 220) aqi_logic = 4;
      else aqi_logic = 5;
    }

    
    ledcWrite(FAN_PWM, 255 - currentRawPower);

    updateDisplay();
    
    dotState = !dotState;
    tft.fillCircle(300, 15, 3, dotState ? ST77XX_GREEN : ST77XX_BLACK);

    lastUpdate = millis();
  }
}

void updateDisplay() {
  uint16_t statusColor = ST77XX_GREEN;
  String statusTxt = "";
  
  if (regimFan == "turbo") {
    statusColor = ST77XX_RED;
    statusTxt = "TURBO FORAT";
  } else if (regimFan == "stop") {
    statusColor = ST77XX_WHITE;
    statusTxt = "OPRIT MAN.";
  } else {
    switch(aqi_logic) {
      case 1: statusColor = ST77XX_GREEN;  statusTxt = "EXCELENTA "; break;
      case 2: statusColor = 0xAFE5;        statusTxt = "BUNA      "; break;
      case 3: statusColor = ST77XX_YELLOW; statusTxt = "MODERATA  "; break;
      case 4: statusColor = ST77XX_ORANGE; statusTxt = "POLUATA   "; break;
      case 5: statusColor = ST77XX_RED;    statusTxt = "CRITICA   "; break;
    }
  }
  
  tft.setCursor(180, 25); tft.setTextColor(statusColor, ST77XX_BLACK); tft.print(statusTxt);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(80, 70);   tft.print(eco2); tft.print(" ppm   ");
  tft.setCursor(90, 105);  tft.print(tvoc); tft.print(" ppb   ");
  tft.setCursor(90, 140);  tft.print(temp, 1); tft.print(" C  ");
  tft.setCursor(90, 175);  tft.print(hum, 1); tft.print(" %  ");
  
  tft.setCursor(160, 210);
  if (currentRawPower > 10) {
    tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
    tft.print(map(currentRawPower, 0, 255, 0, 100)); tft.print("%       "); 
  } else {
    tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
    tft.print("STAND-BY ");
  }
}
