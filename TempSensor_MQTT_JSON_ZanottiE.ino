#include <MQTT.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <stdio.h>
#include <TimeLib.h>
#include <DallasTemperature.h>
#include <OneWire.h>




/******Wi-Fi********/
const char* ssid     = "iPhone di Riccardo";         // The SSID (name) of the Wi-Fi network you want to connect to
const char* password = "12345678";     // The password of the Wi-Fi network
/******sezione define********/
#define ONE_WIRE_BUS 0
////NTP////
#define TZ              1       // (utc+) TZ in hours
#define DST_MN          0      // use 60mn for summer time in some countries

#define NTP0_OR_LOCAL1  0       // 0:use NTP  1:fake external RTC

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
#define ONE_WIRE_BUS 0
#define TOPIC           "tempsens"



OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

/******* Variabili globali *******/

float temp_C = 0.0;

  int ore=0;
  int mins=0;
  int sec=0;

  int giorno=0;
  int mese=0;
  int anno=0;

  
char jsnMsg[351];
char sensorAddress[17];
char  tempo[8];
timeval tv;


WiFiClient  client;
MQTTClient mqttClient(351);

StaticJsonBuffer<350> jsonBuffer;
//JsonObject& messaggioJson = jsonBuffer.createObject();
  

void creaJson();
void locatingDevice(DeviceAddress deviceAddress);
void LetturaTemperatura(DeviceAddress deviceAddress);
//void ConversionFrom_C_To_F ();
char strObj[350];
 
void setup(void)
{
  /***starting SERIAL COMMUNICATION***/
  Serial.begin(115200);         
  delay(10);
  Serial.println('\n');

 /******Connessione a Wi-Fi********/
  WiFi.disconnect();
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);             // Connect to the network
  Serial.print("Connessione alla rete in corso... ");
  Serial.print(ssid); Serial.print(" ...");

  do
  { // Wait for the Wi-Fi to connect
    delay(500);
    Serial.print('.');
  }while (WiFi.status() != WL_CONNECTED);

  Serial.println('\n');
  Serial.println("Connessione avvenuta correttamente!");  
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer

 delay(5000);
 configTime(TZ_SEC, DST_SEC, "ntp1.inrim.it");
  

  // starting serial port
  Serial.begin(115200);
 
/***************************** Inizializzazione DS18B20 ****************************/
  // locate devices on the bus
  Serial.print("Locating devices...");
  sensors.begin();
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");
  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensors.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");

  // Method 1:
  // Search for devices on the bus and assign based on an index. Ideally,
  // you would do this to initially discover addresses on the bus and then 
  // use those addresses and manually assign them (see above) once you know 
  // the devices on your bus (and assuming they don't change).
  
  if (!sensors.getAddress(insideThermometer, 0)) 
  {
      Serial.println("Unable to find address for Device 0"); 
  }
  else
  {   
   Serial.print("Device 0 address: ");
   locatingDevice(insideThermometer);
   Serial.println();
  }
  
  sensors.setResolution(insideThermometer, 12);
  Serial.print("Device 0 resolution: ");
  Serial.print(sensors.getResolution(insideThermometer), DEC);
  Serial.println();
  
  /* Fine Inizializzazione DS18B20 */

  /*connecting to the MQTT broker......*/
  Serial.print("Connessiona ad MQTT in corso...");
  mqttClient.begin("iot.ittsrimini.edu.it", client);
  mqttConnect();


}

void loop()
{
 
  /***********pubblicazione feed MQTT*********/
  
  Serial.println("Lettura data e ora");
  realTime();
  
  Serial.println("Lettura sensore temperatura");
  LetturaTemperatura(insideThermometer);
  Serial.println("Crea msg JSON");
  creaJson();
  Serial.print("Dimensione messaggio: ");
  //Serial.println(sizeof(strObj));
  Serial.println("Invio a broker MQTT");
  //messaggioJson.prettyPrintTo(strObj, sizeof(strObj));
  Serial.println(jsnMsg);
  mqttSendData();

}

  
/********* DALLAS TEMPERATURE - DS18B20 ***********/


void locatingDevice(DeviceAddress deviceAddress)
{
   
 for(uint8_t i = 0; i<8; i++)
 {
  sprintf(sensorAddress + (i*2), "%02X", deviceAddress[i]);
    if (deviceAddress[i] < 16)
    {
      Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
    }
    else
    {
      Serial.print(deviceAddress[i], HEX); 
    }
  }
}

  void LetturaTemperatura(DeviceAddress deviceAddress)
  {
      sensors.requestTemperatures();
      delay(1000);
      temp_C=sensors.getTempC(deviceAddress);
  }
//
//  void ConversionFrom_C_To_F ();
//  {
//      temp_F = DallasTemperature::toFahrenheit(tempC;
//  }
//  //printTemperature(insideThermometer);


/******richiesta ed elaborazione dati server NTP*******/

 void realTime()
{
  
  gettimeofday(&tv, nullptr);
  static time_t lastv = 0;
  if (lastv != tv.tv_sec)
  {
     lastv = tv.tv_sec;
     giorno = day(lastv);
     mese = month(lastv);
     anno = year(lastv);
     ore = hour(lastv) + 1;
     mins = minute(lastv);
     sec = second(lastv);
    
//    sprintf(tempo, "%02d:%02d:%02d", ore, mins, sec);
//    Serial.println(tempo);

  }
 }

 /*****connessione ed invio dati ad MQTT*******/
 
 void mqttConnect()
 {
 Serial.print("checking wifi...");
 while (WiFi.status() != WL_CONNECTED) {
   Serial.print(".");
   delay(1000);
 }

 Serial.print("\nconnecting...");
 delay(2000);
 while (!mqttClient.connect("WeMos_temperature", "flr_iot", "Alohomora"))
 {
   Serial.print(".");
   delay(1000);
 }

  Serial.println("\nMQTT connesso!");
delay(2000);
}


void mqttSendData(void)
{
 mqttClient.loop();
 delay(10);

 if (!mqttClient.connected())
 {
     mqttConnect();
 }

 mqttClient.publish(TOPIC, jsnMsg);
 delay(2000);
}

/****creazione pachetto dati JSON da inviare*****/

 void creaJson()
 {
  int jsonOre  = ore;
  int jsonMins = mins;
  int jsonSec  = sec;
  
  int jsonGiorno= giorno;
  int jsonMese = mese;
  int jsonAnno = anno;
  
  //StaticJsonBuffer<300> jsonBuffer;
  JsonObject& messaggioJson = jsonBuffer.createObject();
  messaggioJson["sensore"] = sensorAddress;
  messaggioJson["temperatura_C"] = temp_C;
  //messaggioJson["temperatura_F"]
    
  
  JsonObject& ora_lettura = messaggioJson.createNestedObject("ora_lettura");
  ora_lettura["ora"] = jsonOre;
  ora_lettura["minuto"] = jsonMins;
  ora_lettura["secondo"]= jsonSec;

  JsonObject& data_lettura = messaggioJson.createNestedObject("data_lettura");
  data_lettura["num_giorno"] = jsonGiorno;
  data_lettura["mese"] = jsonMese;
  data_lettura["anno"] = jsonAnno;

  messaggioJson.prettyPrintTo(jsnMsg, sizeof(jsnMsg));
  delay(2000);

 }


