/***************************************************************************************************************
 *  IoT_Agricultura v1.0 usando NodeMCU ESP-12  
 *  DHT22 ==> pin D3 (Temperatura Ambiente y Humedad Relativa)
 *  Sensor de Suelo ==> pin A0
 *  OLED Display SCL==> pin D1 ; SDA==>pin D2
 *  OLED inicia con parámetros automáticos
 *  Control Manual 3 botones 
 *    1 ON/OFF Lampara
 *    2 ON/OFF BOMBA
 *    3 ON/OFF Modo Automático/Actualizacion de Sensores
 *  Control Automatico dependiente de variables                  
 *  Lectura de datos de los sensores para Blynk app
 *  Sistema de Control vía Blynk app
 ********************************************************************************************************************************/
#define SW_VERSION "IoT-Agro 1.0_dm"
#include "variables.h" // esp pines
#include "credenciales.h" // internet ssid, pass, y Blynk auth

/*ESP & Blynk*/
#define BLYNK_PRINT Serial 
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
WidgetLED BOMBAs(V0); //Bomba sensor para Blynk App
WidgetLED BOMBAa(V5); //Bomba actuador para Blynk App
WidgetLED LAMPs(V1); //Lampara sensor para Blynk App
WidgetLED LAMPa(V6); //Lampara actuador para Blynk App

/*TIMER*/
#include <SimpleTimer.h>
SimpleTimer timer;

/*OLED*/
#include <ACROBOTIC_SSD1306.h>
#include <SPI.h>
#include <Wire.h>

/*DHT22*/
#include "DHT.h"
DHT dht(DHTPIN, DHTTYPE);

/*DS18B20 Sensor de Temperatura*/
#include <OneWire.h>
#include <DallasTemperature.h>
OneWire oneWire (ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
/*****************************************************************/
/*****************************************************************/
void setup() {

  Serial.begin(115200);
  delay(10);
  Serial.println("IoT-Agricultura");
  Serial.println("... Inicializando");
  Serial.println(" ");
  
  pinMode(BOMBA_PIN, OUTPUT);
  pinMode(LAMP_PIN, OUTPUT);
  pinMode(BOMBA_ON_BOTON, INPUT_PULLUP);
  pinMode(LAMP_ON_BOTON, INPUT_PULLUP);
  pinMode(SENSORES_LEER_BOTON, INPUT_PULLUP);
  pinMode(soilMoisterVcc, OUTPUT);

  //Inicio de Blynk
  Blynk.begin(auth, ssid, pass, "blynk.iot-cm.com", 8080);
  oledInicio();
  dht.begin();
  DS18B20.begin();

  BOMBAs.off();
  LAMPs.off();
  BOMBAa.off();
  LAMPa.off();
  digitalWrite(BOMBA_PIN, HIGH); //Usar modulo rele con logica invertida, normalmente HIGH
  digitalWrite(LAMP_PIN, HIGH); //Usar modulo rele con logica invertida, normalmente HIGH
  digitalWrite(soilMoisterVcc, LOW);

  esperarBotonPush(MOSTRAR); //Esperar presion del Boton
  oled.clearDisplay();
  empezarTimer();
}

void loop() {
  timer.run(); //Inicializar Simple Timer
  Blynk.run();
}
/***************************************************
* Leer comandos remotos
****************************************************/
BLYNK_WRITE(3) //Control Remoto Bomba
{
  int i = param.asInt();
  if(i == 1)
  {
    bombaStatus = !bombaStatus;
    aplyCmd();
  }
}
BLYNK_WRITE(4) //Control Remoto Lampara
{
  int i = param.asInt();
  if(i == 1)
  {
    lampStatus = !lampStatus;
    aplyCmd();
  }
}
/***************************************************
* Leer comandos locales (bomba y lampara son normalmente "HIGH")
****************************************************/
void leerLocalCmd()
{
  boolean digiValor = debounce(BOMBA_ON_BOTON);
  if(!digiValor)
  {
    bombaStatus = !bombaStatus;
    aplyCmd();
  }
  digiValor = debounce(LAMP_ON_BOTON);
  if(!digiValor)
  {
    lampStatus = !lampStatus;
    aplyCmd();
  }
  digiValor = debounce(SENSORES_LEER_BOTON);
  if(!digiValor)
  {
    apagarOLED = !apagarOLED;
    if(!apagarOLED)
    {
      oled.setTextXY(0,0); oled.putString("Act..SENSORES");
      obtenerDHTdata();
      obtenerSueloMoisterData();
      obtenerSueloTempData();
      oledInicio();
      mostrarDatos();
    }else oled.clearDisplay();
  }
 }
/***************************************************
* Recibir comandos y actuar
****************************************************/
void aplyCmd()
{
  if(bombaStatus == 1)
  {
    Blynk.notify("IoT-Agro: Aviso ==> Bomba ON");
    digitalWrite(BOMBA_PIN, HIGH);
    if(!apagarOLED) mostrarDatos();
    BOMBAs.on();
    BOMBAa.on();
  }
  else
  {
    digitalWrite(BOMBA_PIN, LOW);
    if(!apagarOLED) mostrarDatos();
    BOMBAs.off();
    BOMBAa.off();
  }
  if(lampStatus == 1)
  {
    Blynk.notify("IoT-Agro: Aviso ==> Lampara ON");
    digitalWrite(LAMP_PIN, HIGH);
    if(!apagarOLED) mostrarDatos();
    LAMPs.on();
    LAMPa.on();
  }
  else
  {
    digitalWrite(LAMP_PIN, LOW);
    if(!apagarOLED) mostrarDatos();
    LAMPs.off();
    LAMPa.off();
  }
}
/***************************************************
* Control Automatico basado en la lectura de los sensores
****************************************************/
void autoControlAgro(void)
{
  if(sueloMoister < SUELO_SECO)
  {
    turnONBomba();
  }
  if(aireTemp < TEMP_FRIA)
  {
    turnONLamp();
  }
}
/***************************************************
* Funcion encender BOMBA
****************************************************/
void turnONBomba()
{
  bombaStatus = 1;
  aplyCmd();
  delay(TIME_BOMBA_ON * 1000);
  bombaStatus = 0;
  aplyCmd();
}
/***************************************************
* Funcion encender LAMPARA
****************************************************/
void turnONLamp()
{
  lampStatus = 1;
  aplyCmd();
  delay(TIME_LAMP_ON * 1000);
  lampStatus = 0;
  aplyCmd();
}
/***************************************************
* Enviar informacion a Blynk
****************************************************/
void enviarDatos()
{
  Blynk.virtualWrite(10, aireTemp);
  Blynk.virtualWrite(11, aireHum);
  Blynk.virtualWrite(12, sueloMoister);
  Blynk.virtualWrite(13, sueloTemp);
}
























 
