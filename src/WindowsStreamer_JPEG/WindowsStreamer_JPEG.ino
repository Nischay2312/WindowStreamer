//Libraries:
//For server communication
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include "index.h"
#include <string.h>

#include <JPEGDecoder.h>  // JPEG decoder library

// Return the minimum of two values a and b
#define minimum(a,b)     (((a) < (b)) ? (a) : (b))

TFT_eSPI tft = TFT_eSPI();
void TFTSetup();

//These are used when we setup Soft AP mode WiFi 
const char* ssidAP = "WindowsStreamer";
const char* passwordAP = "eightcharlong";
// These are used when we use Station mode WiFi 
const char* ssid = "Nj";
const char* password = "r18nmbr4";

//Display Stuff
#define DISPLAY_BUFFER_SIZE 160*128
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 128
#define StartX 0
#define StartY 0
#define FPSdesired 150                                   //Desired FPS for the display(max 30)
#define FPSDelayMS 1/(FPSdesired*1.0)*1000              //The time we have to wait for between each new frame
uint16_t* DisplayBuffer;
uint8_t* DisplayPreBuffer;
void fillDisplayBuffer(uint8_t *payload, uint16_t length);
void fillDisplayPreBuffer(uint8_t *payload, size_t length);
bool DisplayBufferReady = false;
uint16_t Current_BUFFER_Pos = 0;
uint16_t fpscounter = 0;
unsigned long timestart = 0;
const int FPSDelay = ceil(FPSDelayMS);                  //The Delay in Milliseconds between each new frame.

volatile uint32_t payload_length = 0;
void jpegInfo();
void DecodeImage(uint8_t *payload, uint32_t size);
void renderJPEG(int xpos, int ypos);

unsigned long SessionStart = 0;
void DisplayConnectionInfo();

//For the Webserver stuff
#define soft_AP 0 //if this is 1 then the ESP will be a soft AP otherwise it will be a station
String jsonString; //String to hold the JSON data
WebServer server(80); //Webserver object
WebSocketsServer webSocket = WebSocketsServer(81); //Websocket object


//This function is called when a client connects to the websocket
void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length){
  //Serial.printf("WebSocket %d received event: %d\n", num, type);
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("WebSocket %d disconnected\n", num);
      Serial.printf("Session Time: %d sec\n", (micros() - SessionStart)/1000000);
      SessionStart = micros();
      //display the connection info
      DisplayConnectionInfo();
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("WebSocket %d connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        SessionStart = micros();
      }
      break;
    case WStype_TEXT:
      //Serial.printf("WebSocket %d received text: %d\n", num, *payload);
      if(*payload == 49){
        //Serial.println("Start");
      }
      if(*payload == 48){
        //Serial.printf("Finish. %d\n",Current_BUFFER_Pos);
        DecodeImage(DisplayPreBuffer, Current_BUFFER_Pos + 1);
        Current_BUFFER_Pos = 0;
        //fillDisplayBuffer(DisplayPreBuffer, DISPLAY_BUFFER_SIZE*2);
        DisplayBufferReady = true;
      }
      break;
    case WStype_BIN:
      //Serial.printf("WebSocket %d received binary length: %u\n", num, length);
      //Fill the received data into our DisplayBuffer
      //but the data is 8bit, buffer is 16bit so we need to do some conversion
      fillDisplayPreBuffer(payload, length);
      break;
    case WStype_ERROR:
      Serial.printf("WebSocket %d error\n", num);
      break;
    case WStype_FRAGMENT:
      Serial.printf("WebSocket %d fragment\n", num);
      break;
    default:
      Serial.printf("WebSocket %d unknown type\n", num);
      break;
  }
}

//Setup function
void setup(void) {
  //start serial communication
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  DisplayPreBuffer = (uint8_t*)malloc(2*DISPLAY_BUFFER_SIZE * sizeof(uint8_t));
  //DisplayBuffer = (uint16_t*)malloc(DISPLAY_BUFFER_SIZE * sizeof(uint16_t));
  // if(DisplayPreBuffer == NULL || DisplayBuffer == NULL){
  //   while(1)
  //     Serial.println("Buffer Allocation Failed");
  // }

  //connect to WiFi, check which connection type is wanted
  if(soft_AP == 1){
    //Setup soft AP
    WiFi.softAP(ssidAP, passwordAP);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("IP address: ");
    Serial.println(myIP);
    delay(5000);
  }
  else{
    //Setup Station
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  delay(5000);
  }
  
  //Setup the TFT Screen
  TFTSetup();

  // Start the server, when new client connects, we send the index.html page.
  server.on("/", [](){
    server.send(200, "text/html", index_html);
  });

  server.begin(); //intialize the server
  webSocket.begin();  // intialze the websocket server
  webSocket.onEvent(webSocketEvent);  // on a WS event, call webSocketEvent

  //Display the connection info
  DisplayConnectionInfo();

  Serial.println("Setup Done");
  delay(2500);
  timestart = micros();
}

void loop() {

  // Handle all the server BS
  server.handleClient(); // Listen for incoming clients
  webSocket.loop(); // Listen for incoming websocket clients

  //If the DisplayBuffer is ready then draw the buffer to the screen
  if(DisplayBufferReady == true){
    //We have a new buffer to display so do it
    //tft.pushImage(StartX, StartY, DISPLAY_WIDTH, DISPLAY_HEIGHT, DisplayBuffer);
    renderJPEG(0,0);
    DisplayBufferReady = false;
    fpscounter++;
    if(fpscounter > 100){
      float timetaken = ((micros()*1.0 - timestart*1.0)/1000000.0);
      Serial.printf("Printed %d frames in %lf seconds, framerate: %lf\n", fpscounter, timetaken, (fpscounter*1.0/timetaken));
      timestart = micros();
      fpscounter = 0;
    }
  }
  //Serial.println("Waiting for image");
  //delay(FPSDelay);
}

void TFTSetup(){
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);
}

void fillDisplayPreBuffer(uint8_t *payload, size_t length){
  //DisplayPreBuffer = (uint8_t*)realloc(DisplayPreBuffer, (length) * sizeof(uint8_t));;
  int j = Current_BUFFER_Pos;
  //Serial.printf("BufferPosition at Start of filling: %d\n", j); 
  for(int i = 0; i < length; i++){
    DisplayPreBuffer[j] = payload[i];
    j++;
  }
  Current_BUFFER_Pos = j;
  //Serial.printf("BufferPosition at End of filling: %d\n", j); 
  payload_length = (volatile uint32_t)length;
  //Serial.printf("Payload Length: %d\n", payload_length);
}

void fillDisplayBuffer(uint8_t *payload, uint16_t length){
  int j = 0;
  for(int i = 0; i < length-1; i+=2){
    DisplayBuffer[j] = ((payload[i] << 8) | (payload[i+1] & 0xff));
    j++;
  }
}


void DecodeImage(uint8_t *payload, uint32_t size) {
  if (JpegDec.decodeArray(payload, size)) {
    //jpegInfo();

  } else {
    // Handle decoding failure, e.g., by printing an error message
    Serial.println("JPEG decoding failed");
  }
}

//####################################################################################################
// Draw a JPEG on the TFT, images will be cropped on the right/bottom sides if they do not fit
//####################################################################################################
// This function assumes xpos,ypos is a valid screen coordinate. For convenience images that do not
// fit totally on the screen are cropped to the nearest MCU size and may leave right/bottom borders.
void renderJPEG(int xpos, int ypos) {

  // retrieve infomration about the image
  uint16_t *pImg;
  uint16_t mcu_w = JpegDec.MCUWidth;
  uint16_t mcu_h = JpegDec.MCUHeight;
  uint32_t max_x = JpegDec.width;
  uint32_t max_y = JpegDec.height;

  // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
  // Typically these MCUs are 16x16 pixel blocks
  // Determine the width and height of the right and bottom edge image blocks
  uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
  uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

  // save the current image block size
  uint32_t win_w = mcu_w;
  uint32_t win_h = mcu_h;

  // record the current time so we can measure how long it takes to draw an image
  //uint32_t drawTime = millis();

  // save the coordinate of the right and bottom edges to assist image cropping
  // to the screen size
  max_x += xpos;
  max_y += ypos;

  // read each MCU block until there are no more
  while (JpegDec.readSwappedBytes()) {
	  
    // save a pointer to the image block
    pImg = JpegDec.pImage ;

    // calculate where the image block should be drawn on the screen
    int mcu_x = JpegDec.MCUx * mcu_w + xpos;  // Calculate coordinates of top left corner of current MCU
    int mcu_y = JpegDec.MCUy * mcu_h + ypos;

    // check if the image block size needs to be changed for the right edge
    if (mcu_x + mcu_w <= max_x) win_w = mcu_w;
    else win_w = min_w;

    // check if the image block size needs to be changed for the bottom edge
    if (mcu_y + mcu_h <= max_y) win_h = mcu_h;
    else win_h = min_h;

    // copy pixels into a contiguous block
    if (win_w != mcu_w)
    {
      uint16_t *cImg;
      int p = 0;
      cImg = pImg + win_w;
      for (int h = 1; h < win_h; h++)
      {
        p += mcu_w;
        for (int w = 0; w < win_w; w++)
        {
          *cImg = *(pImg + w + p);
          cImg++;
        }
      }
    }

    // draw image MCU block only if it will fit on the screen
    if (( mcu_x + win_w ) <= tft.width() && ( mcu_y + win_h ) <= tft.height())
    {
      tft.pushRect(mcu_x, mcu_y, win_w, win_h, pImg);
    }
    else if ( (mcu_y + win_h) >= tft.height()) JpegDec.abort(); // Image has run off bottom of screen so abort decoding
  }

  // calculate how long it took to draw the image
  //drawTime = millis() - drawTime;

  // print the results to the serial port
  //Serial.print(F(  "Total render time was    : ")); Serial.print(drawTime); Serial.println(F(" ms"));
  //Serial.println(F(""));
}

void jpegInfo() {
  Serial.println(F("==============="));
  Serial.println(F("JPEG image info"));
  Serial.println(F("==============="));
  Serial.print(F(  "Width      :")); Serial.println(JpegDec.width);
  Serial.print(F(  "Height     :")); Serial.println(JpegDec.height);
  Serial.print(F(  "Components :")); Serial.println(JpegDec.comps);
  Serial.print(F(  "MCU / row  :")); Serial.println(JpegDec.MCUSPerRow);
  Serial.print(F(  "MCU / col  :")); Serial.println(JpegDec.MCUSPerCol);
  Serial.print(F(  "Scan type  :")); Serial.println(JpegDec.scanType);
  Serial.print(F(  "MCU width  :")); Serial.println(JpegDec.MCUWidth);
  Serial.print(F(  "MCU height :")); Serial.println(JpegDec.MCUHeight);
  Serial.println(F("==============="));
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
