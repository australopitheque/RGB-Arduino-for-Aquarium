#include "Nextion.h"
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DS3232RTC.h> // https://github.com/JChristensen/DS3232RTC
#include <math.h>
#include <HardwareSerial.h>
#include <SolarCalculator.h>

#define DS3231_ADDRESS 0x68 // adresse  physique DS3231
#define ONE_WIRE_BUS 5      // pin 5 for DS18B20/
// var DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// calcul soleil
const double latitude = 47.65;
const double longitude = -2.08;
const int time_zone = +1; // paris
double sunrisecal;        // Sunrise, en Heure (UTC)
double transitcal;        // Solar transit, en Heure  (UTC)
double sunsetcal;         // Sunset, en Heure  (UTC)
int hourscal;
int minutescal;
int Heurelever;
int minutelever;
int heurecoucher;
int minutecoucher;
byte oldday;
byte oldminute;

int dayLED[] = {255, 255, 255}; // kelvin dans la journée
int nightLED[] = {0, 3, 15};    // 15, 5, 107 nuit lunaire
int sunLED[] = {251, 77, 0};    //  coucher de soleil
// variable tempo
uint32_t roulementPrecedent1;
uint32_t roulementPrecedent2;
const uint16_t refreshSwitch = 500;

uint32_t kelvin; // initialisation kelvin

// gestion led

#include <Adafruit_NeoPixel.h> // Library for the WS2812 Neopixel pixels
#ifdef __AVR__
#include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif
#define NUM_LEDS 30
#define DATA_PIN 6 // Défini le pin de la bande pixels ws2812 DATA_PIN = 6
// Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LEDS, DATA_PIN, NEO_RGB + NEO_KHZ800);
Adafruit_NeoPixel pixels(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);
unsigned long tempoled = 0;
const long intervalled = 12000; // delai entre les changements couleur led
const long interval = 250;     // Change this value (ms)
int setWhitePointRed;
int setWhitePointGrn;
int setWhitePointBlu;
int newRed;
int newGrn;
int newBlu;

/*
 * Declare Nextion instance
 */
#ifdef ESP8266
// esp8266 / NodeMCU software serial ports
SoftwareSerial mySerial(D2, D1); // RX, TX
#else
// SoftwareSerial mySerial(15,14); // RX, TX
// HardwareSerial mySerial(Serial3)  ;
#endif
Nextion *next = Nextion::GetInstance(Serial3); // software serial

// RTC_DS3231 rtc;
DS3232RTC myRTC;
const char *tableauDesJours[] = {"Samedi", "Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi"};

//**** Variable temperature
float Celcius = 0;
float Fahrenheit = 0;

// variable moon
String nfm = ""; // days to next full moon

// Declare objects that we are going to read from the display. This includes buttons, sliders, text boxes, etc:
// Format: <type of object> <object name>(<nextion instance>,<page id>, <object id>, "<object name>");
/* ***** Types of objects:
 * NexButton - Button
 * NexDSButton - Dual-state Button
 * NexHotspot - Hotspot, that is like an invisible button
 * NexCheckbox - Checkbox
 * NexRadio - "Radio" checkbox, that it's exactly like the checkbox but with a rounded shape
 * NexSlider - Slider
 * NexGauge - Gauge
 * NexProgressBar - Progress Bar
 * NexText - Text box
 * NexScrolltext - Scroll text box
 * NexNumber - Number box
 * NexVariable - Variable inside the nextion display
 * NexPage - Page touch event
 * NexGpio - To use the Expansion Board add-on for Enhanced Nextion displays
 * NexRtc - To use the real time clock for Enhanced Nextion displays
 * *****
 */
// page Nextion
NexPage pmenu(next, 0, "menu");
NexPage p1(next, 1, "kelvin");
NexPage p2(next, 2, "settings");

// texte modifiable page 0
/*
 *  NexText(Nextion instance, page id:0, objet id, cobjet name);
 */
NexText tempnexion(next, 0, 5, "temp");
NexText date(next, 0, 2, "date");
NexNumber heuremenu(next, 0, 6, "h0");
NexNumber minutemenu(next, 0, 7, "m0");
NexNumber secondemenu(next, 0, 8, "s0");
NexNumber luxkelvin(next, 0, 13, "kelvin");
NexPicture lune(next, 0, 3, "p0");
NexNumber nexHlever(next, 0, 17, "Hlever");
NexNumber nexMlever(next, 0, 18, "Mlever");
NexNumber nexHcoucher(next, 0, 19, "Hcoucher");
NexNumber nexMcoucher(next, 0, 20, "Mcoucher");

// texte modifiable page 2 qui sera appeler quand le Nextion sera sur cette page
NexNumber joursetting(next, 2, 4, "day");
NexNumber moissetting(next, 2, 5, "month");
NexNumber anneesetting(next, 2, 6, "year");
NexNumber heuresetting(next, 2, 1, "heure");
NexNumber minutesetting(next, 2, 2, "minute");
NexNumber secondesetting(next, 2, 3, "seconde");
NexText jsemainesetting(next, 2, 21, "weektext");

// declaration des boutons
//  bouton ennregistrement page 0
// bouton ennregistrement page 1
NexButton menu(next, 1, 26, "bmenu");
NexButton reglageheure(next, 1, 1, "b1next");
NexDSButton modeonoff(next, 1, 79, "bt0");
// bouton ennregistrement page 2
NexButton retourparamkelvin(next, 2, 22, "b3back");
NexButton enrsetting(next, 2, 27, "enrsetting");

// liste de surveillance nextion
NexTouch *nex_listen_list[] = {
    &pmenu,
    &p2,
    &p1,
    &menu,
    &modeonoff,
    &reglageheure,
    &retourparamkelvin,
    &enrsetting,
    &modeonoff,
    NULL};

// Sous Programme**************************************
// structure R,G,B
const PROGMEM int RGBs[] = { // RGB 1356 valeur , valeur haute 6500 Kelvin
    255, 249, 253, 255, 248, 252, 255, 247, 247, 255, 246, 241, 255, 245, 235, 255, 244, 229, 255, 243, 223, 255, 242, 217, 255, 241, 211,
    255, 240, 205, 255, 239, 199, 255, 238, 193, 254, 237, 188, 254, 236, 182, 254, 235, 176, 254, 234, 170, 254, 233, 163, 254, 232, 159,
    254, 231, 153, 254, 230, 147, 254, 229, 141, 254, 228, 135, 254, 227, 129, 254, 226, 123, 254, 225, 118, 254, 224, 112, 254, 223, 103,
    254, 222, 100, 254, 225, 95, 254, 225, 94, 255, 223, 95, 255, 222, 93, 254, 223, 91, 254, 223, 89, 254, 222, 88, 254, 220, 87, 255, 219, 86,
    255, 219, 85, 255, 218, 84, 255, 218, 84, 255, 219, 83, 254, 218, 80, 254, 216, 75, 253, 215, 74, 253, 215, 74, 254, 214, 72, 254, 214, 71,
    254, 213, 71, 254, 212, 70, 254, 212, 68, 255, 211, 66, 255, 211, 65, 254, 209, 64, 253, 209, 63, 253, 208, 62, 252, 208, 62, 251, 206, 64,
    250, 207, 64, 251, 206, 63, 252, 205, 61, 251, 203, 59, 251, 202, 56, 252, 201, 54, 252, 200, 52, 252, 199, 50, 252, 198, 49, 251, 196, 47,
    252, 195, 46, 252, 194, 46, 252, 194, 44, 253, 194, 39, 252, 193, 36, 253, 192, 37, 252, 190, 33, 252, 189, 31, 252, 187, 31, 253, 185, 30,
    253, 186, 28, 253, 184, 27, 253, 182, 26, 254, 180, 23, 253, 179, 21, 253, 179, 21, 253, 179, 21, 252, 178, 19, 251, 177, 17, 250, 176, 17,
    251, 174, 16, 252, 173, 15, 251, 173, 14, 252, 172, 13, 252, 170, 12, 253, 168, 11, 253, 168, 10, 253, 166, 9, 254, 165, 7, 253, 165, 7,
    252, 164, 6, 251, 163, 5, 251, 162, 4, 252, 161, 4, 252, 161, 3, 251, 160, 2, 252, 158, 2, 253, 157, 1, 253, 156, 1, 253, 155, 0, 253, 154, 0,
    253, 151, 0, 253, 150, 0, 253, 150, 0, 253, 150, 0, 253, 150, 0, 253, 150, 0, 254, 150, 0, 254, 149, 0, 253, 147, 0, 253, 146, 0, 254, 146, 0,
    254, 145, 0, 253, 143, 1, 253, 142, 2, 254, 140, 2, 253, 140, 1, 253, 140, 2, 252, 138, 2, 253, 137, 0, 254, 136, 1, 254, 137, 2, 254, 136, 3,
    253, 134, 4, 253, 133, 5, 252, 132, 7, 252, 133, 9, 253, 131, 11, 253, 129, 11, 253, 128, 12, 252, 129, 13, 252, 128, 13, 252, 126, 15,
    252, 123, 19, 252, 122, 21, 252, 123, 21, 253, 121, 23, 252, 119, 24, 252, 119, 25, 252, 118, 27, 252, 118, 29, 253, 117, 31, 253, 115, 32,
    252, 114, 34, 252, 113, 35, 252, 113, 36, 253, 111, 38, 255, 110, 42, 255, 107, 44, 255, 107, 44, 255, 107, 45, 255, 106, 47, 255, 105, 50,
    254, 104, 53, 255, 102, 56, 255, 100, 59, 255, 99, 64, 255, 98, 70, 254, 97, 72, 254, 96, 73, 255, 95, 76, 255, 95, 82, 255, 95, 87, 255, 94, 88,
    255, 92, 88, 254, 91, 90, 254, 91, 93, 255, 90, 96, 255, 88, 100, 255, 87, 104, 255, 86, 105, 254, 84, 108, 254, 83, 109, 254, 84, 110, 254, 83, 113,
    253, 80, 121, 252, 79, 125, 252, 79, 125, 253, 79, 126, 252, 78, 128, 252, 78, 131, 252, 77, 132, 252, 76, 132, 252, 75, 135, 252, 75, 138, 250, 73, 140,
    252, 72, 139, 252, 73, 140, 250, 72, 142, 251, 72, 146, 251, 72, 147, 251, 72, 147, 250, 73, 149, 249, 71, 151, 249, 72, 153, 249, 71, 156, 249, 71, 158,
    249, 71, 159, 249, 71, 159, 248, 69, 161, 247, 69, 164, 246, 69, 164, 247, 69, 166, 249, 68, 167, 251, 67, 168, 251, 67, 168, 250, 67, 169, 249, 69, 171,
    248, 69, 173, 247, 67, 175, 248, 67, 176, 247, 67, 178, 247, 67, 179, 246, 67, 181, 247, 67, 182, 245, 67, 183, 244, 68, 183, 243, 68, 185, 242, 69, 186,
    242, 69, 185, 241, 68, 187, 240, 69, 188, 239, 69, 190, 238, 70, 191, 237, 70, 193, 236, 70, 194, 236, 70, 196, 235, 69, 199, 233, 71, 200, 233, 71, 201,
    232, 70, 200, 232, 68, 199, 233, 67, 199, 233, 68, 200, 231, 68, 200, 229, 67, 203, 229, 68, 204, 228, 69, 205, 227, 69, 208, 226, 70, 209, 226, 70, 209,
    224, 69, 211, 223, 69, 212, 222, 69, 213, 221, 69, 212, 221, 69, 210, 220, 68, 209, 219, 68, 210, 220, 67, 211, 220, 67, 213, 218, 67, 213, 217, 66, 213,
    217, 66, 214, 215, 64, 215, 215, 64, 214, 215, 63, 217, 214, 63, 218, 213, 63, 217, 212, 62, 217, 211, 62, 217, 212, 61, 217, 212, 61, 217, 211, 62, 217,
    209, 61, 217, 209, 61, 216, 208, 60, 216, 208, 60, 215, 206, 59, 215, 205, 59, 215, 204, 58, 215, 204, 57, 215, 203, 57, 215, 204, 56, 216, 204, 56, 218,
    205, 55, 219, 204, 54, 219, 203, 53, 219, 201, 53, 219, 200, 52, 218, 199, 51, 218, 199, 51, 219, 197, 50, 219, 196, 50, 219, 194, 49, 218, 195, 48, 218,
    195, 48, 219, 194, 47, 219, 193, 47, 217, 192, 46, 217, 192, 46, 217, 191, 44, 217, 191, 43, 216, 190, 43, 217, 189, 42, 217, 189, 41, 217, 188, 40, 216,
    187, 39, 215, 186, 38, 216, 186, 38, 217, 185, 38, 216, 185, 36, 215, 185, 34, 216, 185, 33, 217, 185, 34, 217, 184, 33, 217, 183, 31, 217, 182, 30, 217,
    181, 29, 215, 181, 28, 215, 180, 27, 215, 180, 27, 215, 179, 27, 215, 179, 27, 216, 178, 25, 216, 177, 26, 216, 174, 25, 216, 172, 25, 216, 172, 24, 216,
    172, 24, 215, 172, 23, 214, 171, 22, 214, 170, 21, 215, 170, 21, 215, 169, 20, 214, 169, 19, 213, 168, 18, 212, 167, 17, 213, 167, 17, 213, 166, 16, 211,
    164, 16, 212, 163, 15, 212, 163, 14, 211, 162, 13, 212, 162, 13, 213, 161, 13, 212, 160, 12, 211, 160, 12, 210, 159, 11, 210, 159, 10, 211, 158, 9, 210,
    157, 8, 210, 158, 8, 209, 156, 8, 209, 151, 8, 210, 149, 6, 209, 149, 6, 209, 149, 6, 209, 148, 6, 208, 148, 5, 208, 146, 4, 208, 145, 3, 208, 144, 2, 208,
    144, 3, 208, 142, 3, 207, 142, 2, 206, 142, 2, 206, 141, 3, 208, 141, 2, 211, 142, 1, 213, 142, 1, 212, 140, 2, 211, 140, 1, 211, 139, 1, 211, 139, 1, 209,
    138, 1, 210, 137, 1, 209, 136, 1, 209, 135, 0, 208, 134, 0, 207, 134, 0, 207, 134, 0, 207, 135, 0, 207, 134, 0, 207, 134, 0, 207, 133, 0, 206, 131, 0, 206,
    130, 0, 206, 128, 0, 205, 125, 0, 205, 124, 0, 204, 124, 0, 204, 122, 1, 204, 119, 1, 204, 119, 1, 204, 119, 0, 203, 119, 0, 203, 115, 0, 205, 115, 0, 205,
    111, 0, 206, 109, 0, 206, 106, 0, 207, 106, 0, 207, 103, 0, 208, 103, 0, 208, 100, 0, 209, 100, 0, 210, 96, 0, 211, 96, 0, 212, 90, 0, 213, 90, 0, 214,
    87, 0, 215, 86, 0, 216, 84, 0, 217, 82, 0, 218, 82, 0, 219, 80, 0, 220, 79, 0, 221, 77, 0, 222, 76, 0, 223, 75, 0, 224, 74, 0, 225, 72, 0, 226, 70, 0, 227,
    69, 0, 228, 66, 0, 229, 64, 0, 230, 62, 0, 231, 59, 0, 232, 58, 0, 233, 57, 0, 234, 56, 0, 235, 54, 0, 236, 52, 0, 237, 50, 0, 238, 49, 0, 239, 48, 0, 240,
    46, 0, 241, 44, 0, 241, 42, 0, 242, 39, 0, 242, 37, 0, 243, 34, 0, 243, 33, 0, 244, 31, 0, 244, 29, 0, 245, 26, 0, 245, 21, 0, 246, 18, 0, 246, 15, 0, 247,
    11, 0, 248, 9, 0, 249, 5, 0, 250, 2, 0, 252, 1, 0, 254, 0, 0, 255};
unsigned int idxR, idxG, idxB, Nbpixel;
const int intensity = 2; // division intensité eclairement lune/sunrise/sunset
unsigned int ndiomoon;
byte illuminamoon;
int isunrise = 1356;
int isunset = 0;
String effect = "jour";
boolean ModeAuto = 1; // initialisation mode automatique on par defaut
int NumPhase = 0;
boolean fineffect = false;
uint32_t execol;
double toLocal(double utc)
{
  time_t t = myRTC.get();
  byte phete;
  byte phhiver;
  byte decalageetehiver;
  phete = 25 + (7002 - (year(t)) - int((year(t)) / 4)) % 7;   // dernier jour été Mars
  phhiver = 25 + (7005 - (year(t)) - int((year(t)) / 4)) % 7; // dernier jour hiver octobre
  if ((month(t) >= 4) || ((month(t) == 3) && (day(t) == 31 - phete)))
  {
    decalageetehiver = 1;
  }
  if ((month(t) >= 11) || ((month(t) == 10) && (day(t) == 31 - phhiver)))
  {
    decalageetehiver = 0;
  }
  return utc + time_zone + decalageetehiver;
}
void SunTime24h(double h)
{
  int m = int(round(h * 60));
  hourscal = (m / 60) % 24;
  minutescal = m % 60;
}
uint32_t oldcol;
void colorWipe(uint32_t nPix, uint32_t col)
{
  Serial.print("col :");
  Serial.println(col);
  Serial.print("execol ");
  Serial.println(execol);
  if (execol == 0)
  {
    if (col != oldcol)
    {
      for (uint32_t i = 0; i < pixels.numPixels(); i += (pixels.numPixels() / nPix))
      {
        pixels.setPixelColor(i, col);
      }
      pixels.show();
      execol = 1;
      oldcol = col;
    }
  }
}
void nuitlune()
{
  byte illune;
  Serial.println(" VOL- - Clair de lune  ");
  ndiomoon = 7;
  colorWipe(pixels.numPixels(), pixels.Color(0, 0, 0));
  execol = 0;
  illune = map(illuminamoon, 0, 100, 0, 255);
  colorWipe(pixels.numPixels() / ndiomoon, pixels.Color(0, 0, illune)); // si besoin de diminuer la luminosité lune rajouter apres ilune: / intensity
  fineffect = true;
}
void journee()
{
  Nbpixel = 1;
  Serial.println(" VOL+ - Journée  ");
  colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(255, 249, 253)); // 6500K
  fineffect = true;
}
void ledoninterval(uint32_t i)
{
  if ((i >= 500) && (i < 600))
  {
    Nbpixel = 2; // 2 led éteint en interval
  }
  else if ((i >= 600) && (i < 700))
  {
    Nbpixel = 3;
  }
  else if ((i >= 700) && (i < 850))
  {
    Nbpixel = 4;
  }
  else if ((i >= 850) && (i < 1050))
  {
    Nbpixel = 5;
  }
  else if ((i >= 1050) && (i < 1200))
  {
    Nbpixel = 6;
  }
  else if (i >= 1200)
  {
    Nbpixel = 7;
  }
  else
  {
    Nbpixel = 1;
  }
}
void couchersoleil()
{
  Serial.println(" CH- - Sunset ");
  
  if (isunset <= 1353)
  {
    Nbpixel = 1;
        // colorWipe(pixels.numPixels(), pixels.Color(0, 0, 0));
    Serial.print(isunset);
    Serial.print("/");
    idxR = pgm_read_word_near(RGBs + isunset);
    idxG = pgm_read_word_near(RGBs + isunset + 1);
    idxB = pgm_read_word_near(RGBs + isunset + 2);

    if (isunset == 1200)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    else if (isunset == 1050)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    else if (isunset == 852)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    else if (isunset == 702)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    else if (isunset == 600)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    else if (isunset == 501)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    ledoninterval(isunset);
    execol = 0;
    colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(idxR / intensity, idxG / intensity, idxB / intensity));
    isunset += 3;
  }
  fineffect = true;
}
void leversoleil()
{
  Serial.println(" CH+ - Sunrise ");
  
  if (isunrise >= 0)
  {

    Serial.print(isunrise);
    Serial.print("/");
    idxR = pgm_read_word_near(RGBs + isunrise);
    idxG = pgm_read_word_near(RGBs + isunrise + 1);
    idxB = pgm_read_word_near(RGBs + isunrise + 2);
    if (isunrise == 1197)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    else if (isunrise == 1047)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    else if (isunrise == 849)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    else if (isunrise == 699)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    else if (isunrise == 597)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    else if (isunrise == 498)
    {
      execol = 0;
      colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(0, 0, 0));
    }
    ledoninterval(isunrise);
    execol = 0;
    colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(idxR / intensity, idxG / intensity, idxB / intensity));
    isunrise -= 3;
  }
  fineffect = true;
}
// Envoyer la couleur à la bande LED RGB mode manuel
void led() // color led manuel
{

  Serial.println("traitement Led manuel");
  colorWipe(pixels.numPixels() / Nbpixel, pixels.Color(newRed, newGrn, newBlu));
}
uint32_t oldkelvin;
void calrgb()
{
  luxkelvin.getValue(&kelvin);
  if (kelvin != oldkelvin)
  {
    execol = 0; // changement couleur led autoriser
    // Calibrage des leds
    // Utilisez ceci pour calibrer vos leds, si vous n'êtes pas satisfait de la couleur blanche ou pour diminuer la luminosité maximale.
    // Utilisez des valeurs comprises entre 0 et 255. Toutes les valeurs de l'échelle de kelvins seront recalculées avec ces valeurs. (Pas les valeurs RGB)
    // Définissez chaque valeur à 255 pour ne pas affecter les valeurs ci-dessous. (le nextion luminosité au max )
    setWhitePointRed = 255; // descendre si rendu trop rouge
    setWhitePointGrn = 255; // descendre si rendu trop verte
    setWhitePointBlu = 255; // descendre si rendu trop bleu

    // Si nous avons une valeur Kelvin> 1000 K et une luminosité, calcule la valeur RGB de celle-ci.
    if (kelvin > 1000 && kelvin < 40000)
    {
      Nbpixel = 1;
      if (kelvin <= 6600)
      {
        newRed = 255;
        newGrn = round((99.4708025861 * log(kelvin / 100) - 161.1195681661));
        if (kelvin <= 1900)
        {
          newBlu = 0;
        }
        else
        {
          newBlu = round(138.5177312231 * log((kelvin - 1000) / 100) - 305.0447927307);
        }
      }
      else
      {
        newRed = round(329.698727446 * pow((kelvin - 6000) / 100, -0.1332047592));
        newGrn = round(288.1221695283 * pow((kelvin - 6000) / 100, -0.0755148492));
        newBlu = 255;
      }
      newRed = max(min(newRed, 255), 0);
      newGrn = max(min(newGrn, 255), 0);
      newBlu = max(min(newBlu, 255), 0);

      // Calibration
      newRed = map(newRed, 0, 255, 0, setWhitePointRed);
      newGrn = map(newGrn, 0, 255, 0, setWhitePointGrn);
      newBlu = map(newBlu, 0, 255, 0, setWhitePointBlu);
    }
    if (kelvin < 1000 && kelvin > 400)
    {
      // mode nuit ou lever/coucher du soleil
      byte Heure;
      byte Minute;
      time_t t = myRTC.get();
      Heure = hour(t);
      Minute = minute(t);
      Nbpixel = 1;
      /*
      dayLED[] = {255, 255, 255}; //kelvin dans la journée
      nightLED[] = {0,3,15};//15, 5, 107 nuit lunaire
      sunLED[] = {251,77,0};//  coucher/lever de soleil
      */
      if (Heure == 7 && Minute <= 30)
      {
        // lever soleil avant 8 H si kelvin <1000 et > 400
        newRed = map(30 + Heure, 0, 100, 0, sunLED[0]);
        newGrn = map(30 + Heure, 0, 100, 0, sunLED[1]);
        newBlu = map(30 + Heure, 0, 100, 0, sunLED[2]);
        Nbpixel = 3;
      }
      if (Heure == 7 && Minute >= 30)
      {
        newRed = map(40 + Heure + Minute, 0, 100, 0, dayLED[0]);
        newGrn = map(40 + Heure + Minute, 0, 100, 0, dayLED[1]);
        newBlu = map(40 + Heure + Minute, 0, 100, 0, dayLED[2]);
        Nbpixel = 2;
      }
      if (Heure >= 8 && Heure <= 18)
      {
        Nbpixel = 2; // nombre de pixels a eteindre
        newRed = sunLED[0];
        newGrn = sunLED[1]; // nuit noire
        newBlu = sunLED[2];
      }
      if (Heure == 18 && Minute <= 30)
      {
        // coucher soleil apres 18 H
        newRed = map(88 - (Heure + Minute), 0, 100, 0, sunLED[0]);
        newGrn = map(88 - (Heure + Minute), 0, 100, 0, sunLED[1]);
        newBlu = map(88 - (Heure + Minute), 0, 100, 0, sunLED[2]);
        Nbpixel = 2;
      } // couchersoleil apres 18h30
      if (Heure == 18 && Minute >= 30)
      {
        newRed = map(88 - (Heure + Minute), 0, 100, 0, sunLED[0]);
        newGrn = map(88 - (Heure + Minute), 0, 100, 0, sunLED[1]);
        newBlu = map(88 - (Heure + Minute), 0, 100, 0, sunLED[2]);
        Nbpixel = 3;
      }
      else
      {
        newRed = 0;
        newGrn = 0;
        newBlu = 0;
      }
      Serial.print("Minute :");
      Serial.println(Minute);
      Serial.print("Heure :");
      Serial.println(Heure);
      Serial.print("Nbpixel :");
      Serial.println(Nbpixel);
    }
    else if (kelvin < 400)
    {
      Nbpixel = 2; // nombre pas de pixels a eteindre
      int NumPhaseRed;
      int NumPhaseGrn;
      int NumPhaseBlu;
      if (NumPhase <= 3)
      {
        NumPhaseRed = map(NumPhase, 0, 3, 0, 0);
        NumPhaseGrn = map(NumPhase, 0, 3, -3, 3);
        NumPhaseBlu = map(NumPhase, 0, 3, -15, 15);
        newRed = nightLED[0] - NumPhaseRed;
        newGrn = nightLED[1] - NumPhaseGrn;
        newBlu = nightLED[2] - NumPhaseBlu;
      }
      else if (NumPhase >= 3 && NumPhase <= 7)
      {
        NumPhaseRed = map(NumPhase, 3, 7, 0, 0);
        NumPhaseGrn = map(NumPhase, 3, 7, -3, 3);
        NumPhaseBlu = map(NumPhase, 3, 7, -15, 15);
        newRed = nightLED[0] + NumPhaseRed;
        newGrn = nightLED[1] + NumPhaseGrn;
        newBlu = nightLED[2] + NumPhaseBlu;
      }
    }
    oldkelvin = kelvin;
  }
  Serial.print("Calcul RGB : ");
  Serial.println(kelvin);
  Serial.println(newRed);
  Serial.println(newGrn);
  Serial.println(newBlu);
}
double julianDate(int y, int m, int d)
{
  // convertir une date en date julienne
  int mm, yy;
  double k1, k2, k3;
  double j;
  yy = y - int((12 - m) / 10);
  mm = m + 9;
  if (mm >= 12)
  {
    mm = mm - 12;
  }
  k1 = 365.25 * (yy + 4172);
  k2 = int((30.6001 * mm) + 0.5);
  k3 = int((((yy / 100) + 4) * 0.75) - 38);
  j = k1 + k2 + d + 59;
  j = j - k3; // j est la date julienne à 12h TU (temps universel)
  return j;
}
// gestion des evenements
void sondetemperature()
{
  if (millis() - roulementPrecedent2 > refreshSwitch)
  {                                 // si compteur atteind 1 sec
    roulementPrecedent2 = millis(); // reinitialise le compteur
                                    // init temp DS18B20
    sensors.begin();
    // ds18b20 temp request
    sensors.requestTemperatures();
    delay(500); // delai request temperature
    Celcius = sensors.getTempCByIndex(0);
    Fahrenheit = sensors.toFahrenheit(Celcius);
    float t = Celcius;
    static char temperatureCTemp[6];
    dtostrf(t, 6, 2, temperatureCTemp);
    tempnexion.setText(temperatureCTemp);
  }
}

void luneimage()
{
  // lune calcul
  // calcule l'âge de la phase de la lune(0 à 7)
  // il y a huit phases, 0 est la pleine lune et 4 est la nouvelle lune
  double jd = 0; // Julian Date
  double ed = 0; // jours écoulés depuis le début de la pleine lune
  time_t t = myRTC.get();
  calcSunriseSunset(year(t), month(t), day(t), latitude, longitude, transitcal, sunrisecal, sunsetcal); // calcul lever/coucher soleil
  SunTime24h(toLocal(sunrisecal));                                                                      // conversion en h et m locale
  Heurelever = hourscal;                                                                                // recupere l'heure convertie
  minutelever = minutescal;                                                                             // recupere minutes convertie
  Serial.print("Sunset cal:  ");
  SunTime24h(toLocal(sunsetcal));
  heurecoucher = hourscal;
  minutecoucher = minutescal;
  if (day(t) != oldday)
  {
    jd = julianDate(year(t), month(t), day(t));
    // jd = julianDate(1972,1,1); // utilisé pour déboguer ceci est une nouvelle lune
    jd = int(jd - 2244116.75); // start at Jan 1 1972
    jd /= 29.53;               // diviser par le cycle lunaire
    NumPhase = jd;
    jd -= NumPhase;                  // laisse la partie fractionnaire de jd
    ed = jd * 29.53;                 // jours écoulés ce mois-ci
    nfm = String((int(29.53 - ed))); // jours jusqu'à la prochaine pleine lune
    NumPhase = jd * 8 + 0.5;
    NumPhase = NumPhase & 7;
    oldday = day(t);
  }
  // affichage phase de la lune
  if (NumPhase >= 0 && NumPhase <= 7)
  {
    if (NumPhase == 0)
    {
      lune.setPic(5);
      illuminamoon = 100;
    }
    else if (NumPhase == 1)
    {
      lune.setPic(6);
      illuminamoon = 76;
    }
    else if (NumPhase == 2)
    {
      lune.setPic(7);
      illuminamoon = 41;
    }
    else if (NumPhase == 3)
    {
      lune.setPic(8);
      illuminamoon = 12;
    }
    else if (NumPhase == 4)
    {
      lune.setPic(9);
      illuminamoon = 0;
    }
    else if (NumPhase == 5)
    {
      lune.setPic(10);
      illuminamoon = 18;
    }
    else if (NumPhase == 6)
    {
      lune.setPic(11);
      illuminamoon = 55;
    }
    else if (NumPhase == 7)
    {
      lune.setPic(12);
      illuminamoon = 90;
    }
  }
  Serial.println("lunephase :");
}
// display time, date
void display() // heure page 0
{
  byte Hour;
  byte Minute;
  byte Second;
  Wire.beginTransmission(DS3231_ADDRESS); // cherche si horloge presente a  son adresse
  byte erreur = Wire.endTransmission();   // transmission ok ou pas
  if (erreur == 0)
  { // si transmission ok
    // partie affichage date
    char buf[12];
    time_t t = myRTC.get();
    sprintf(buf, "%.2d/%s/%d",
            day(t), monthShortStr(month(t)), year(t));
    date.setText(buf);
    // partie affichage horaire
    Hour = hour(t);
    Minute = minute(t);
    Second = second(t);
    heuremenu.setValue(Hour);
    minutemenu.setValue(Minute);
    secondemenu.setValue(Second);
    nexHlever.setValue(Heurelever);
    nexMlever.setValue(minutelever);
    nexHcoucher.setValue(heurecoucher);
    nexMcoucher.setValue(minutecoucher);
  }
  else
  {
    date.setText("pas DS3231");
  }
  Serial.println("Affichage Heure");
}
// traitement des pages/boutons surveillés
/*
 */
void commandeffect()
{

  time_t t = myRTC.get();
  if ((hour(t) * 100 + minute(t) >= Heurelever * 100 + minutelever) && (hour(t) * 100 + minute(t) < Heurelever * 100 + minutelever + 200))
  { // test si heure lever et si 2h apres lever
    effect = "leversoleil";
    fineffect = false;
  }
  else if ((hour(t) * 100 + minute(t) >= heurecoucher * 100 + minutecoucher - 200) && (hour(t) * 100 + minute(t) < heurecoucher * 100 + minutecoucher ))
  {//si 2h avant heure coucher et si heure coucher
    effect = "couchersoleil";
    fineffect = false;
  }
  else if ((hour(t) * 100 + minute(t) > Heurelever * 100 + minutelever + 200) && (hour(t) * 100 + minute(t) < heurecoucher * 100 + minutecoucher - 200))
  {//si 2h avant heure coucher et si 2h apres heure lever
    effect = "jours";
    fineffect = false;
  }
  else if ((hour(t) * 100 + minute(t) <= Heurelever * 100 + minutelever) || (hour(t) * 100 + minute(t) > heurecoucher * 100 + minutecoucher))
  { //si avant heure lever ou si apres heure coucher
    effect = "lune";
    fineffect = false;
  }
  if ((ModeAuto == 1) && (fineffect == false))
  {
    Serial.println("Mode Auto");
    if (effect == "StandBye")
    {
      fineffect = true;
    }
    if (effect == "jours")
    {
      execol = 0;
      journee();
      if (fineffect == true)
      {
        effect = "StandBye";
      }
    }
    if (effect == "lune")
    {
      if (millis() - tempoled > intervalled)
      {                      // si compteur atteind 1 sec
        tempoled = millis(); // reinitialise le compteur
        execol = 0;
        nuitlune();
        if (fineffect == true)
        {
          effect = "StandBye";
          fineffect = false;
        }
      }
    }
    if (effect == "leversoleil")
    {
      if (millis() - tempoled > intervalled)
      {                      // si compteur atteind 1 sec
        tempoled = millis(); // reinitialise le compteur
        execol = 0;
        Nbpixel = 1;
        leversoleil();
        if (fineffect == true)
        {
          effect = "jours";
          fineffect = false;
        }
      }
    }
    if (effect == "couchersoleil")
    {
      if (millis() - tempoled > intervalled)
      {                      // si compteur atteind 1 sec
        tempoled = millis(); // reinitialise le compteur
        execol = 0;
        couchersoleil();
        if (fineffect == true)
        {
          fineffect = false;
          effect = "lune";
        }
      }
    }
  }
}
void pmenupush(void *ptr) // traitement page 0
{
  if (millis() - roulementPrecedent1 > refreshSwitch)
  {                                 // si compteur atteind 1/2 sec
    roulementPrecedent1 = millis(); // reinitialise le compteur
    sondetemperature();
    display();
    calrgb();
    luneimage();
    if (ModeAuto == true)
    {
      commandeffect();
    }
    else
    {
      led();
    }

    Serial.println("Affichage Menu");
  }
}
void reglageheurepop(void *ptr)
{
}
void retourparamkelvinpush(void *ptr) {}
void boutonmodeonoff(void *ptr)
{
  uint32_t dual_state;
  modeonoff.getValue(&dual_state);
  if (dual_state)
  {
    ModeAuto = true;
    Serial.println("true");
  }
  else
  {
    ModeAuto = false;
    Serial.println("false");
  }
}
void p1PopCallback(void *ptr) // traitement page 1
{
  oldday = 0;
}
void p2PopCallback(void *ptr) // traitement page 2
{
  // affichage de l'heure et date du DS3231 sur la page 2
  byte Jours;
  Jours = RTC.readRTC(3);
  time_t t = myRTC.get();
  jsemainesetting.setText(tableauDesJours[Jours]);
  heuresetting.setValue(hour(t));
  minutesetting.setValue(minute(t));
  secondesetting.setValue(second(t));
  joursetting.setValue(day(t));
  moissetting.setValue(month(t));
  anneesetting.setValue(year(t));
}
void enrsettingPopCallback(void *ptr) // traitement bouton enr de la page 2
{
  time_t t = myRTC.get();
  // recuperation de l'heure et date de la page setting pour ds3231
  tmElements_t tm;
  uint32_t Annee;
  uint32_t Mois;
  uint32_t Jours;
  uint32_t Heure;
  uint32_t Minute;
  uint32_t Seconde;
  heuresetting.getValue(&Heure);
  minutesetting.getValue(&Minute);
  secondesetting.getValue(&Seconde);
  joursetting.getValue(&Jours);
  moissetting.getValue(&Mois);
  anneesetting.getValue(&Annee);
  if (Annee >= 100 && Annee < 1000) // verifie si dizaine d'année et superieur a 1000 ans
    Serial.println(F("Erreur !"));  // debug
  else
  {
    // Serial.println(F("Début traitement Date")); // debug
    if (Annee >= 1000)
      tm.Year = CalendarYrToTm(Annee); // si annee superieur a 1000 deduit 1970 (annee de defaut ds331)
    else                               // (y < 100)
      tm.Year = y2kYearToTm(Annee);    // si annee inferieur a 100 deduit 30 pour calcul par rapport a 70 (1970)
    tm.Month = Mois;
    tm.Day = Jours;
    tm.Hour = Heure;
    tm.Minute = Minute;
    tm.Second = Seconde;
    t = makeTime(tm);
    RTC.set(t);        // utiliser la valeur time_t pour s'assurer que le jour de la semaine correct est défini.
    Serial.println(t); // debug
    setTime(t);
    byte Joursemaine = RTC.readRTC(3);
    jsemainesetting.setText(tableauDesJours[Joursemaine]);
  }
}
void retourversmenu(void *ptr)
{
}

// initialisation executer une seule fois
void setup(void)
{
  Serial.begin(250000);
  // These lines are specifically to support the Adafruit Trinket 5V 16 MHz.
  // Any other board, you can remove this part (but no harm leaving it):
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif
  // END of Trinket-specific code.
  pixels.begin();
  pixels.setBrightness(100); // Réglez BRIGHTNESS à environ 1/5 (max = 255)
  // Start the I2C interface
  Wire.begin();
  // Initialize Nextion connection wiht selected baud in this case 19200
  if (!next->nexInit(115200))
  {
    Serial.println("nextion init fails");
  }
  delay(100); // delai initiaisation nextion
              // surveillance des boutons
  // paramkelvin.attachPop(kelvinpush, &paramkelvin);
  pmenu.attachPop(pmenupush, &pmenu);
  reglageheure.attachPop(reglageheurepop, &reglageheure);
  retourparamkelvin.attachPop(retourparamkelvinpush, &retourparamkelvin);
  p2.attachPop(p2PopCallback, &p2);
  p1.attachPop(p1PopCallback, &p1);
  modeonoff.attachPop(boutonmodeonoff, &modeonoff);
  enrsetting.attachPop(enrsettingPopCallback, &enrsetting);
  menu.attachPush(retourversmenu, &menu);
}

/* Regarde si un bouton a été actionné, a mettre dans le loop */
void lectureBoutons()
{
  /*
   * Quand un événement pop ou push se produit à chaque fois,
   * le composant correspondant [id de la page de droite et id du composant] dans la liste des événements tactiles sera demandé.
      duré minmal de gestion evenement nextion unsigned long duration= 450 ;
   */
  delay(100);
  next->nexLoop(nex_listen_list);
}
// boucle continue de l'arduino
void loop(void)
{
  lectureBoutons();
  commandeffect();
}
