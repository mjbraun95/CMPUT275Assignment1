//////////////////////////////////////////////////////////
// CMPUT 275 Winter 2019
// Matthew Braun, 1497171
// Weekly Exercise 2: Reastaurant Finder
//////////////////////////////////////////////////////////

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SD.h>
#include <TouchScreen.h>

#define TFT_DC 9
#define TFT_CS 10
#define SD_CS 6

// physical dimensions of the tft display (# of pixels)
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240

// touch screen pins, obtained from the documentaion
#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM  5  // can be a digital pin
#define XP  4  // can be a digital pin

// dimensions of the part allocated to the map display
#define MAP_DISP_WIDTH (DISPLAY_WIDTH - 48)
#define MAP_DISP_HEIGHT DISPLAY_HEIGHT

#define REST_START_BLOCK 4000000
#define NUM_RESTAURANTS 1066

// calibration data for the touch screen, obtained from documentation
// the minimum/maximum possible readings from the touch point
#define TS_MINX 150
#define TS_MINY 120
#define TS_MAXX 920
#define TS_MAXY 940

// thresholds to determine if there was a touch
#define MINPRESSURE   10
#define MAXPRESSURE 1000

//Initializing global variables
int SSLOW = 0;
int nSLOW = 0;
int aSLOW = 0;
int SFAST = 0;
int nFAST = 0;
int aFAST = 0;

uint32_t lastrestBlock = 0;


Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);



// a multimeter reading says there are 300 ohms of resistance across the plate,
// so initialize with this to get more accurate readings
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// different than SD
Sd2Card card;

struct restaurant {
  int32_t lat;
  int32_t lon;
  uint8_t rating; // from 0 to 10
  char name[55];
};

// the implementation from class
void getRestaurant(int restIndex, restaurant* restPtr) {
  uint32_t blockNum = REST_START_BLOCK + restIndex/8;
  restaurant restBlock[8];

  while (!card.readBlock(blockNum, (uint8_t*) restBlock)) {
    Serial.println("Read block failed, trying again.");
  }

  *restPtr = restBlock[restIndex % 8];
}

//Only reads SD card when new block needs to be loaded
void getRestaurantFast(int restIndex, restaurant* restPtr) {
  uint32_t blockNum = REST_START_BLOCK + restIndex/8;
  restaurant restBlock[8];
  if (lastrestBlock != blockNum)
  {
      while (!card.readBlock(blockNum, (uint8_t*) restBlock)) {
        Serial.println("Read block failed, trying again.");
      }

      *restPtr = restBlock[restIndex % 8];
      lastrestBlock = blockNum;
  }
  else 
  {
    *restPtr = restBlock[restIndex % 8];
  }
}

void processTouchSCreen() 
{
    TSPoint touch = ts.getPoint();

    if (touch.z < MINPRESSURE || touch.z > MAXPRESSURE) 
    {
        return;
    }

    int screen_x = map(touch.y, TS_MINY, TS_MAXY, DISPLAY_WIDTH-1, 0);
    int screen_y = map(touch.x, TS_MINX, TS_MAXX, 0, DISPLAY_WIDTH-1);

    Serial.print(screen_x);
    Serial.print(' ');
    Serial.println(screen_y);
    delay(100);

    //Check for button presses
    if (screen_x > 272)
    {
        if (screen_y < DISPLAY_HEIGHT/2)
        {
          int time1 = millis();
          restaurant rest3;
          for (int i = 0; i < NUM_RESTAURANTS; ++i) 
          {
            getRestaurant(i, &rest3);
          }
          int time2 = millis();
          int dTime = time2 - time1; //Calculates time for slow restaurant reader to execute
          tft.fillRect(0,16,272,16, ILI9341_BLACK); //Erases old text
          tft.fillRect(0,64,272,16, ILI9341_BLACK);
          tft.setCursor(0,16);
          tft.print(dTime); //Displays recalculated times
          nSLOW++;
          SSLOW = SSLOW + dTime;
          aSLOW = SSLOW / nSLOW;
          tft.println(" ms");
          tft.setCursor(0,64);
          tft.print(aSLOW);
          tft.println(" ms");
        }
        else if (screen_y > DISPLAY_HEIGHT/2)
        {
          int time1 = millis();
          restaurant rest3;
          for (int i = 0; i < NUM_RESTAURANTS; ++i) 
          {
            getRestaurantFast(i, &rest3);
          }
          int time2 = millis();
          int dTime = time2 - time1; //Calculates time for fast restaurant reader to execute
          tft.fillRect(0,112,272,16, ILI9341_BLACK); //Erases old text
          tft.fillRect(0,160,272,16, ILI9341_BLACK);
          tft.setCursor(0,112);
          tft.print(dTime); //Displays recalculated times
          nFAST++;
          SFAST = SFAST + dTime;
          aFAST = SFAST / nFAST;
          tft.println(" ms");
          tft.setCursor(0,160);
          tft.print(aFAST);
          tft.println(" ms");
        }
    }
}

void setup() {
  init();
  Serial.begin(9600);

  // tft display initialization
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tft.setRotation(3);


  // Display the buttons
  tft.fillRect(MAP_DISP_WIDTH, 0,
               48, DISPLAY_HEIGHT, ILI9341_RED);
  tft.fillRect(MAP_DISP_WIDTH + 2, 2,
               44, (DISPLAY_HEIGHT/2)-4, ILI9341_BLACK);
  tft.fillRect(MAP_DISP_WIDTH + 2, (DISPLAY_HEIGHT/2)+2,
               44, (DISPLAY_HEIGHT/2)-4, ILI9341_BLACK);
  tft.setCursor(DISPLAY_WIDTH - 31, 10);
  tft.setTextColor(ILI9341_WHITE);  
  tft.setTextSize(3);
  tft.println('S');
  tft.setCursor(DISPLAY_WIDTH - 31, 35);
  tft.println('L');
  tft.setCursor(DISPLAY_WIDTH - 31, 60);
  tft.println('O');
  tft.setCursor(DISPLAY_WIDTH - 31, 85);
  tft.println('W');
  tft.setCursor(DISPLAY_WIDTH - 31, 130);
  tft.println('F');
  tft.setCursor(DISPLAY_WIDTH - 31, 155);
  tft.println('A');
  tft.setCursor(DISPLAY_WIDTH - 31, 180);
  tft.println('S');
  tft.setCursor(DISPLAY_WIDTH - 31, 205);
  tft.println('T');

  // Display initial text
  tft.setCursor(0,0);
  tft.setTextSize(2);
  tft.println("Recent SLOW run:");
  tft.println("Not run yet");
  tft.println(" ");
  tft.println("SLOW run avg:");
  tft.println("Not run yet");
  tft.println(" ");
  tft.println("Recent FAST run:");
  tft.println("Not run yet");
  tft.println(" ");
  tft.println("FAST run avg:");
  tft.println("Not run yet");
  tft.println(" ");


  // SD card initialization for raw reads
  Serial.print("Initializing SPI communication for raw reads...");
  if (!card.init(SPI_HALF_SPEED, SD_CS)) {
    Serial.println("failed! Is the card inserted properly?");
    while (true) {}
  }
  else {
    Serial.println("OK!");
  }
}



int main() {
  setup();

  //Waiting for touchscreen response
  while (true) 
  {
    processTouchSCreen();
  }

  Serial.end();
  return 0;
}
