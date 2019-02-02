/*
	One possible solution for exercise 1. You may use it freely
  in assignment 1 (just mention you are using the provided solution
  for exercise 1).

  Your solution for assignment 1 does not need to exactly duplicate 
  this cursor movement speed. As long as it is natural and varies 
  with how far the joystick is pushed.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <SD.h>
#include <TouchScreen.h>
#include "lcd_image.h"

#define TFT_DC 9
#define TFT_CS 10
#define SD_CS 6

// touch screen pins, obtained from the documentaion
#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM  5  // can be a digital pin
#define XP  4  // can be a digital pin

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

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// a multimeter reading says there are 300 ohms of resistance across the plate,
// so initialize with this to get more accurate readings
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// different than SD
Sd2Card card;

// physical dimensions of the tft display (# of pixels)
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240

// dimensions of the part allocated to the map display
#define MAP_DISP_WIDTH (DISPLAY_WIDTH - 48)
#define MAP_DISP_HEIGHT DISPLAY_HEIGHT

// These constants are for the 2048 by 2048 map .
# define MAP_WIDTH 2048
# define MAP_HEIGHT 2048
# define LAT_NORTH 5361858l
# define LAT_SOUTH 5340953l
# define LON_WEST -11368652l
# define LON_EAST -11333496l

lcd_image_t yegImage = { "yeg-big.lcd", MAP_WIDTH, MAP_HEIGHT };

#define JOY_VERT  A1 // should connect A1 to pin VRx
#define JOY_HORIZ A0 // should connect A0 to pin VRy
#define JOY_SEL   2

#define JOY_CENTER   512
#define JOY_DEADZONE 64

#define CURSOR_SIZE 9

// smaller numbers yield faster cursor movement
#define JOY_SPEED 64

// the cursor position on the display, stored as the middle pixel of the cursor
int cursorX, cursorY;

// upper-left coordinates in the image of the middle of the map of Edmonton
int mapCenterX = (MAP_WIDTH/2 - MAP_DISP_WIDTH/2);
int mapCenterY = (MAP_HEIGHT/2 - MAP_DISP_HEIGHT/2);

// store the info of restaurants
struct restaurant {
    int32_t lat;
    int32_t lon;
    uint8_t rating; // from 0 to 10
    char name[55];
};

uint32_t prevBlock = 100;

restaurant restBlockFast[8];

// forward declaration for drawing the cursor
void redrawCursor(int newX, int newY, int oldX, int oldY);

void setup() {
    init();

    Serial.begin(9600);

    // not actually used in this exercise, but it's ok to leave it
	pinMode(JOY_SEL, INPUT_PULLUP);

	tft.begin();

	Serial.print("Initializing SD card...");
	if (!SD.begin(SD_CS)) {
		Serial.println("failed! Is it inserted properly?");
		while (true) {}
	}
	Serial.println("OK!");

    // SD card initialization for raw reads
    Serial.print("Initializing SPI communication for raw reads...");
    if (!card.init(SPI_HALF_SPEED, SD_CS)) {
        Serial.println("failed! Is the card inserted properly?");
        while (true) {}
    }
    else {
        Serial.println("OK!");
    }

	tft.setRotation(3);

    tft.fillScreen(ILI9341_BLACK);

    // draws the centre of the Edmonton map, leaving the rightmost 48 columns black
	lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);

    // initial cursor position is the middle of the screen
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;

    // draw the initial cursor
    redrawCursor(cursorX, cursorY, cursorX, cursorY);
}

// redraws the patch of edmonton over the older cursor position
// given by "oldX, oldY" and draws the cursor at its new position
// given by "newX, newY"
void redrawCursor(int newX, int newY, int oldX, int oldY) {

  // draw the patch of edmonton over the old cursor
  lcd_image_draw(&yegImage, &tft,
                 mapCenterX + oldX, mapCenterY + oldY,
                 oldX, oldY, CURSOR_SIZE, CURSOR_SIZE);

  // and now draw the cursor at the new position
  tft.fillRect(newX, newY, CURSOR_SIZE, CURSOR_SIZE, ILI9341_RED);
}

void shiftMap() {
  if (cursorX > MAP_DISP_WIDTH - CURSOR_SIZE && mapCenterX < MAP_WIDTH - MAP_DISP_WIDTH) {
    mapCenterX += MAP_DISP_WIDTH;
    mapCenterX = constrain(mapCenterX, 0, MAP_WIDTH - MAP_DISP_WIDTH);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }
  else if (cursorX < 0 && mapCenterX > 0) {
    mapCenterX -= MAP_DISP_WIDTH;
    mapCenterX = constrain(mapCenterX, 0, MAP_WIDTH - MAP_DISP_WIDTH);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }

  if (cursorY > MAP_DISP_HEIGHT - CURSOR_SIZE && mapCenterY < MAP_HEIGHT - MAP_DISP_HEIGHT) {
    mapCenterY += MAP_DISP_HEIGHT;
    mapCenterY = constrain(mapCenterY, 0, MAP_HEIGHT - MAP_DISP_HEIGHT);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }
  else if (cursorY < 0 && mapCenterY > 0) {
    mapCenterY -= MAP_DISP_HEIGHT;
    mapCenterY = constrain(mapCenterY, 0, MAP_HEIGHT - MAP_DISP_HEIGHT);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }

}

void processJoystick() {
  int xVal = analogRead(JOY_HORIZ);
  int yVal = analogRead(JOY_VERT);
  // int buttonVal = digitalRead(JOY_SEL);

  // copy the cursor position (to check later if it changed)
  int oldX = cursorX;
  int oldY = cursorY;

  // move the cursor, further pushes mean faster movement
  cursorX += (JOY_CENTER - xVal) / JOY_SPEED;
  cursorY += (yVal - JOY_CENTER) / JOY_SPEED;

  shiftMap();

  // constrain so the cursor does not go off of the map display window
  cursorX = constrain(cursorX, 0, MAP_DISP_WIDTH - CURSOR_SIZE);
  cursorY = constrain(cursorY, 0, MAP_DISP_HEIGHT - CURSOR_SIZE);

  // redraw the cursor only if its position actually changed
  if (cursorX != oldX || cursorY != oldY) {
    redrawCursor(cursorX, cursorY, oldX, oldY);
  }

  delay(10);
}

// read the restaurant at position "restIndex" from the card
// and store at the location pointed to by restPtr
// if two consecutive calls request restaurants in the same block
// then the latter call should not have to re-read the block from the SD card
void getRestaurantFast(int restIndex, restaurant* restPtr) {
  // calculate the block containing this restaurant
  uint32_t blockNum = REST_START_BLOCK + restIndex/8;
  // if two consecutive calls request restaurants in the different block
  if (blockNum != prevBlock) {
    // fetch the block of restaurants containing the restaurant
    // with index "restIndex"
    // store the info in a global value for later use
    while (!card.readBlock(blockNum, (uint8_t*) restBlockFast)) {
        Serial.println("Read block failed, trying again.");
    }
    prevBlock = blockNum;
  }
  *restPtr = restBlockFast[restIndex % 8];
}

// These functions convert between x / y map position and lat / lon
// ( and vice versa .)
int32_t x_to_lon(int16_t x) {
    return map(x, 0, MAP_WIDTH, LON_WEST, LON_EAST);
}

int32_t y_to_lat(int16_t y) {
    return map(y, 0, MAP_HEIGHT, LAT_NORTH, LAT_SOUTH);
}

int16_t lon_to_x(int32_t lon) {
    return map(lon, LON_WEST, LON_EAST, 0, MAP_WIDTH);
}

int16_t lat_to_y(int32_t lat) {
    return map(lat, LAT_NORTH, LAT_SOUTH, 0, MAP_HEIGHT);
}

void tapScreen() {
    restaurant rest;
    for (int i = 0; i < NUM_RESTAURANTS - 1; i++) {
        getRestaurantFast(i, &rest);
        int16_t restX = lon_to_x(rest.lon);
        if (restX > mapCenterX + 3 && restX < mapCenterX + MAP_DISP_WIDTH - 3) {
            int16_t restY = lat_to_y(rest.lat);
            if (restY > mapCenterY + 3 && restY < mapCenterY + MAP_DISP_HEIGHT - 3) {
                tft.fillCircle(restX - mapCenterX - CURSOR_SIZE/2,
                    restY - mapCenterY - CURSOR_SIZE/2,
                    CURSOR_SIZE/2, ILI9341_BLUE);
            }
        }
    }
}

void mode0() {
   while (true) {
        processJoystick();
        TSPoint touch = ts.getPoint();
        // map the values obtained from the touch screen
        // to the x coordinates in display
        int16_t screen_x = map(touch.y, TS_MINY, TS_MAXY, DISPLAY_WIDTH-1, 0);
        // a certain amount of pressure is required 
        if (touch.z < MINPRESSURE || touch.z > MAXPRESSURE || screen_x > 272) {
            continue; // just go to the top of the loop again
        }
        tapScreen();
    }
}

int main() {
	setup();
    mode0();
	Serial.end();
	return 0;
}
