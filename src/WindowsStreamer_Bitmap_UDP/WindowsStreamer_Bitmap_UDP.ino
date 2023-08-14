//For server communication
#include <WiFi.h>
#include <WiFiUdp.h>
#include "AsyncUDP.h"
#include <TFT_eSPI.h>  // Graphics and font library for ST7735 driver chip
#include <SPI.h>
//Hmm not sure why this is needed
#include <string.h>
#include <esp_task_wdt.h>


TFT_eSPI tft = TFT_eSPI();
void TFTSetup();

//Wifi credentials : Aim is to make a WiFiStation on ESP.
#define soft_AP 0                                   //if this is 1 then the ESP will be a soft AP otherwise it will be a station
//These are used when we setup Soft AP mode WiFi
const char* ssidAP = "WindowsStreamer";
const char* passwordAP = "eightcharlong";
// These are used when we use Station mode WiFi
const char* ssid = "Nj";
const char* password = "r18nmbr4";

//Display Stuff
#define DISPLAY_BUFFER_SIZE 160 * 128
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 128
#define StartX 0
#define StartY 0
#define FPSdesired 100                             //Desired FPS for the display(max 30)
#define FPSDelayMS 1 / (FPSdesired * 1.0) * 1000  //The time we have to wait for between each new frame
uint16_t* DisplayBuffer;
uint8_t* DisplayPreBuffer;
void fillDisplayBuffer(uint8_t* payload, uint16_t length);
void fillDisplayPreBuffer(uint8_t* payload, size_t length);
bool DisplayBufferReady = false;
uint16_t Current_BUFFER_Pos = 0;
uint16_t fpscounter = 0;
unsigned long timestart = 0;
const int FPSDelay = ceil(FPSDelayMS);  //The Delay in Milliseconds between each new frame.

//UDP
bool GotHeader = false;
const int udpPort = 1234;
WiFiUDP udp;
#define head_tail_size 1
uint8_t packetHeader[head_tail_size] = {0x00};
uint8_t packetFooter[head_tail_size] = {0xFF};

void UDPGetData();

AsyncUDP Audp;
void AsyncUDPGetData();

unsigned long SessionStart = 0;
void DisplayConnectionInfo();


//Setup function
void setup(void) {
  //start serial communication
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  //Allocate the display Buffers
  DisplayPreBuffer = (uint8_t*)malloc(2 * DISPLAY_BUFFER_SIZE * sizeof(uint8_t));
  DisplayBuffer = (uint16_t*)malloc(DISPLAY_BUFFER_SIZE * sizeof(uint16_t));
  if (DisplayPreBuffer == NULL || DisplayBuffer == NULL) {
    while (1)
      Serial.println("Buffer Allocation Failed");
  }

  //connect to WiFi, check which connection type is wanted
  if (soft_AP == 1) {
    //Setup soft AP
    WiFi.softAP(ssidAP, passwordAP);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("IP address: ");
    Serial.println(myIP);
    delay(5000);
  } else {
    //Setup Station
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
    //initializes the UDP state
    //This initializes the transfer buffer
    //udp.begin(WiFi.localIP(),udpPort);
    //delay(5000);
  }

  //Setup the TFT Screen
  TFTSetup();

  //Display the connection info
  DisplayConnectionInfo();

  AsyncUDPGetData();

  Serial.println("Setup Done");
  timestart = micros();
}

void loop() {

  //If the DisplayBuffer is ready then draw the buffer to the screen
  if (DisplayBufferReady == true) {
    //We have a new buffer to display so do it
    tft.pushImage(StartX, StartY, DISPLAY_WIDTH, DISPLAY_HEIGHT, DisplayBuffer);
    DisplayBufferReady = false;
    fpscounter++;
    if (fpscounter > 100) {
      float timetaken = ((micros() * 1.0 - timestart * 1.0) / 1000000.0);
      Serial.printf("Printed %d frames in %lf seconds, framerate: %lf\n", fpscounter, timetaken, (fpscounter * 1.0 / timetaken));
      timestart = micros();
      fpscounter = 0;
    }
  }
  //UDPGetData();
  delay(1);
}

void UDPGetData(){
  //Contiously Check for incoming packets
  int packetSize = udp.parsePacket();
  if(packetSize){
    //Serial.printf("Received packet from %s, size: %d\n", udp.remoteIP().toString().c_str(), packetSize);
    uint8_t packetIncoming[packetSize];
    udp.read(packetIncoming, packetSize);
    //if the packet size match the head or tail size we check for the that
    if(packetSize == head_tail_size){
      //if packet is head then do nothing. but if tail, then fill display buffer and send it for displaying.
      if(packetIncoming[0] == packetFooter[0]){
        //Serial.printf("Received Footer, generating display buffer\n");
        fillDisplayBuffer(DisplayPreBuffer, DISPLAY_BUFFER_SIZE*2);
        DisplayBufferReady = true;
        Current_BUFFER_Pos = 0;
        //Serial.printf("display buffer done\n");
      }
      else if(packetIncoming[0] == packetHeader[0]){
        GotHeader = true;
      }
    }
    else{
      //fill the prebuffer
      if(GotHeader){
      fillDisplayPreBuffer(packetIncoming, packetSize);
      }
    }
  }
}

void AsyncUDPGetData(){
  if(Audp.listen(udpPort)) {
        Audp.onPacket([](AsyncUDPPacket packet) {
            if(packet.length() == head_tail_size){
              if(*(packet.data()) == packetFooter[0]){
                //Serial.printf("Received Footer, generating display buffer\n");
                fillDisplayBuffer(DisplayPreBuffer, DISPLAY_BUFFER_SIZE*2);
                DisplayBufferReady = true;
                Current_BUFFER_Pos = 0;
                //Serial.printf("display buffer done\n");
                GotHeader = false;
              }
              else if(*(packet.data()) == packetHeader[0]){
                GotHeader = true;
              }
            }
            else{

              //fill the prebuffer
              if(GotHeader){
                fillDisplayPreBuffer(packet.data(), packet.length());
              }
            }
          });
      }
}

void TFTSetup() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
}

void fillDisplayPreBuffer(uint8_t* payload, size_t length) {
  int j = Current_BUFFER_Pos;
  //Serial.printf("BufferPosition at Start of filling: %d\n", j);
  for (int i = 0; i < length; i++) {
    DisplayPreBuffer[j] = payload[i];
    j++;
  }
  Current_BUFFER_Pos = j;
  //Serial.printf("BufferPosition at End of filling: %d\n", j);
}

void fillDisplayBuffer(uint8_t* payload, uint16_t length) {
  int j = 0;
  for (int i = 0; i < length - 1; i += 2) {
    DisplayBuffer[j] = ((payload[i] << 8) | (payload[i + 1] & 0xff));
    j++;
  }
}

void DisplayConnectionInfo() {
  //pirint the basic connection info like SSID, IP address and PAssword on the TFT
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.println("Windows Screen Mirror\n");
  if (soft_AP == 1) {
    tft.println("Soft AP Mode\n");
    tft.println("SSID: " + String(ssidAP));
    tft.println("IP: " + String(WiFi.softAPIP().toString()));
    tft.println("Password: " + String(passwordAP));
  } else {
    tft.println("Station Mode\n");
    tft.println("SSID: " + String(ssid));
    tft.println("IP: " + String(WiFi.localIP().toString()));
    tft.println("Password: " + String(password));
  }
  tft.println("\n\n");
  tft.println("Connect to the WiFi, and ");
  tft.println("set the IP address in");
  tft.println("python program.");
}