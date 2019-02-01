/*
	One possible solution for exercise 1. You may use it freely
  in assignment 1 (just mention you are using the provided solution
  for exercise 1).

  Your solution for assignment 1 does not need to exactly duplicate 
  this cursor movement speed. As long as it is natural and varies 
  with how far the joystick is pushed.
*/

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_ILI9341.h>
#include "lcd_image.h"

#define TFT_DC 9
#define TFT_CS 10
#define SD_CS 6

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// physical dimensions of the tft display (# of pixels)
#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240

// dimensions of the part allocated to the map display
#define MAP_DISP_WIDTH (DISPLAY_WIDTH - 48)
#define MAP_DISP_HEIGHT DISPLAY_HEIGHT

// width and height (in pixels) of the LCD image
#define LCD_WIDTH 2048
#define LCD_HEIGHT 2048
lcd_image_t yegImage = { "yeg-big.lcd", LCD_WIDTH, LCD_HEIGHT };

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
int mapCenterX = (LCD_WIDTH/2 - MAP_DISP_WIDTH/2);
int mapCenterY = (LCD_HEIGHT/2 - MAP_DISP_HEIGHT/2);

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
  if (cursorX > MAP_DISP_WIDTH - CURSOR_SIZE && mapCenterX < LCD_WIDTH - MAP_DISP_WIDTH) {
    mapCenterX += MAP_DISP_WIDTH;
    mapCenterX = constrain(mapCenterX, 0, LCD_WIDTH - MAP_DISP_WIDTH);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }
  else if (cursorX < 0 && mapCenterX > 0) {
    mapCenterX -= MAP_DISP_WIDTH;
    mapCenterX = constrain(mapCenterX, 0, LCD_WIDTH - MAP_DISP_WIDTH);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }

  if (cursorY > MAP_DISP_HEIGHT - CURSOR_SIZE && mapCenterY < LCD_HEIGHT - MAP_DISP_HEIGHT) {
    mapCenterY += MAP_DISP_HEIGHT;
    mapCenterY = constrain(mapCenterY, 0, LCD_HEIGHT - MAP_DISP_HEIGHT);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }
  else if (cursorY < 0 && mapCenterY > 0) {
    mapCenterY -= MAP_DISP_HEIGHT;
    mapCenterY = constrain(mapCenterY, 0, LCD_HEIGHT - MAP_DISP_HEIGHT);
    lcd_image_draw(&yegImage, &tft, mapCenterX, mapCenterY,
                 0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);
    cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
    cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
  }

}

void processJoystick() {
  int xVal = analogRead(JOY_HORIZ);
  int yVal = analogRead(JOY_VERT);
  int buttonVal = digitalRead(JOY_SEL);

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

int main() {
	setup();

  while (true) {
    processJoystick();
  }

	Serial.end();
	return 0;
}
