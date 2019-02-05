/*
  Name: Ang Li
  ID: 1550746
  CMPUT 275, Winter  2019

  Assignment 1: Restaurant Finder Part 1

  Varied form the solution for weekly exercise 1
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <SD.h>
#include <stdlib.h>
#include <TouchScreen.h>
#include "lcd_image.h"

#define TFT_DC 9
#define TFT_CS 10
#define SD_CS 6

// touch screen pins, obtained from the documentation
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
// initial cursor position is the middle of the screen
int  cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
int  cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;

// store previous state of the JOY_VERT
int prevJOY_VERT = JOY_CENTER;

// upper-left coordinates in the image of the middle of the map of Edmonton
int mapCenterX = (MAP_WIDTH/2 - MAP_DISP_WIDTH/2);
int mapCenterY = (MAP_HEIGHT/2 - MAP_DISP_HEIGHT/2);

// store info of restaurants
struct restaurant {
  int32_t lat;
  int32_t lon;
  uint8_t rating; // from 0 to 10
  char name[55];
};

// store previous block number
uint32_t prevBlock = 100;

// store an array of restaurant struct
// for getRestaurantFast()
restaurant restBlockFast[8];

// store info of restaurants
// smaller version of struct restaurant
struct RestDist {
  uint16_t index ; // index of restaurant from 0 to NUM_RESTAURANTS -1
  uint16_t dist ; // Manhatten distance to cursor position
};

// store an array of rest_dist struct
RestDist rest_dist[NUM_RESTAURANTS];

// forward declaration for drawing the cursor
void redrawCursor(int newX, int newY, int oldX, int oldY);

void setup() {
  init();

  Serial.begin(9600);

  // not actually used in this exercise, but it's ok to leave it
  pinMode(JOY_SEL, INPUT_PULLUP);

  tft.begin();

  // SD card initialization for reading the map
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

// 
void shiftMap() {
  if (cursorX > MAP_DISP_WIDTH - CURSOR_SIZE && mapCenterX < MAP_WIDTH - MAP_DISP_WIDTH) {
    mapCenterX += MAP_DISP_WIDTH/2;
    mapCenterX = constrain(mapCenterX, 0, MAP_WIDTH - MAP_DISP_WIDTH);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }
  else if (cursorX < 0 && mapCenterX > 0) {
    mapCenterX -= MAP_DISP_WIDTH/2;
    mapCenterX = constrain(mapCenterX, 0, MAP_WIDTH - MAP_DISP_WIDTH);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }

  if (cursorY > MAP_DISP_HEIGHT - CURSOR_SIZE && mapCenterY < MAP_HEIGHT - MAP_DISP_HEIGHT) {
    mapCenterY += MAP_DISP_HEIGHT/2;
    mapCenterY = constrain(mapCenterY, 0, MAP_HEIGHT - MAP_DISP_HEIGHT);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }
  else if (cursorY < 0 && mapCenterY > 0) {
    mapCenterY -= MAP_DISP_HEIGHT/2;
    mapCenterY = constrain(mapCenterY, 0, MAP_HEIGHT - MAP_DISP_HEIGHT);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }

}

void processJoystick0() {
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
  for (int i = 0; i < NUM_RESTAURANTS; i++) {
    getRestaurantFast(i, &rest);
    int16_t restX = lon_to_x(rest.lon);
    if (restX > mapCenterX + 3 && restX < mapCenterX + MAP_DISP_WIDTH - 3) {
      int16_t restY = lat_to_y(rest.lat);
      if (restY > mapCenterY + 3 && restY < mapCenterY + MAP_DISP_HEIGHT - 3) {
        tft.fillCircle(restX - mapCenterX,
        restY - mapCenterY, CURSOR_SIZE/2, ILI9341_BLUE);
      }
    }
  }
}

void mode0() {
  tft.fillScreen(ILI9341_BLACK);

  // draws the centre of the Edmonton map, leaving the rightmost 48 columns black
  lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
    0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);

  // draw the initial cursor
  redrawCursor(cursorX, cursorY, cursorX, cursorY);

  while (digitalRead(JOY_SEL) != LOW) {
    processJoystick0();
    TSPoint touch = ts.getPoint();
    // map the values obtained from the touch screen
    // to the x coordinates in display
    int16_t screen_x = map(touch.y, TS_MINY, TS_MAXY, DISPLAY_WIDTH - 1, 0);
    // a certain amount of pressure is required 
    if (touch.z < MINPRESSURE || touch.z > MAXPRESSURE || screen_x > 272) {
      continue; // just go to the top of the loop again
    }
    tapScreen();
  }
}

void processJoystick1(int* selectedRest) {
  if (analogRead(JOY_VERT) < JOY_CENTER - JOY_DEADZONE && prevJOY_VERT > JOY_CENTER - JOY_DEADZONE) {
    *selectedRest -= 1;
  }
  else if (analogRead(JOY_VERT) > JOY_CENTER + JOY_DEADZONE && prevJOY_VERT < JOY_CENTER + JOY_DEADZONE) {
    *selectedRest += 1;
  }
  *selectedRest = constrain(*selectedRest, 0, 29);
  prevJOY_VERT = analogRead(JOY_VERT);
  delay(10);
}

void swap(RestDist* a, RestDist* b) {
  RestDist temp = *a;
  *a = *b;
  *b = temp;
}

void iSort(RestDist A[], int len) {
  int i = 1;
  while (i < len) {
    int j = i;
    while (j > 0 && A[j - 1].dist > A[j].dist) {
      swap(&A[j - 1], &A[j]);
      j -= 1;
    }
    i += 1;
  }
}

void getNearestRest() {
  for (uint16_t i = 0; i < NUM_RESTAURANTS; i++) {
    restaurant r;
    getRestaurantFast(i, &r);
    rest_dist[i].index = i;
    rest_dist[i].dist = abs(lon_to_x(r.lon) - (cursorX + mapCenterX + CURSOR_SIZE/2))
     + abs(lat_to_y(r.lat) - (cursorY + mapCenterY + CURSOR_SIZE/2)); //Assigns index and distance to each RestDist element
  }
  iSort(&rest_dist[0], NUM_RESTAURANTS);
}

void mode1() {
  getNearestRest();
  tft.fillScreen(0);
  tft.setCursor(0, 0); // where the characters will be displayed
  tft.setTextWrap(false);
  int selectedRest = 0; // which restaurant is selected ?
  restaurant r;
  for (int16_t i = 0; i < 30; i ++) {
    getRestaurantFast(rest_dist[i].index, &r);
    if (i != selectedRest) { // not highlighted
      // white characters on black background
      tft.setTextColor(0xFFFF, 0x0000);
    } 
    else { // highlighted
      // black characters on white background
      tft.setTextColor(0x0000, 0xFFFF);
    }
    tft.print(r.name);
    tft.print("\n");
  }
  tft.print("\n");
  getRestaurantFast(rest_dist[0].index, &r);

  while (digitalRead(JOY_SEL) != LOW) {
    int prevSelectedRest = selectedRest;
    processJoystick1(&selectedRest);
    if (selectedRest != prevSelectedRest) {
      getRestaurantFast(rest_dist[prevSelectedRest].index, &r);
      tft.setCursor(0, prevSelectedRest * 8); // where the characters will be displayed
      // white characters on black background
      tft.setTextColor(0xFFFF, 0x0000);
      tft.print(r.name);
      tft.print("\n");
      getRestaurantFast(rest_dist[selectedRest].index, &r);
      tft.setCursor(0, selectedRest * 8); // where the characters will be displayed
      // black characters on white background
      tft.setTextColor(0x0000, 0xFFFF);
      tft.print(r.name);
      tft.print("\n");
    }
  }
  mapCenterX = lon_to_x(r.lon) - MAP_DISP_WIDTH/2;
  mapCenterY = lat_to_y(r.lat) - MAP_DISP_HEIGHT/2;

  mapCenterX = constrain(mapCenterX, 0, MAP_WIDTH - MAP_DISP_WIDTH);
  mapCenterY = constrain(mapCenterY, 0, MAP_HEIGHT - MAP_DISP_HEIGHT);

  cursorX = lon_to_x(r.lon) - mapCenterX - CURSOR_SIZE/2;
  cursorY = lat_to_y(r.lat) - mapCenterY - CURSOR_SIZE/2;

  // constrain so the cursor does not go off of the map display window
  cursorX = constrain(cursorX, 0, MAP_DISP_WIDTH - CURSOR_SIZE);
  cursorY = constrain(cursorY, 0, MAP_DISP_HEIGHT - CURSOR_SIZE);
}

int main() {
  setup();
  while(true) {
    mode0();
    mode1();
  }
  Serial.end();
  return 0;
}