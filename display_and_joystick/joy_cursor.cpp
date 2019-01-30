#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_ILI9341.h>
#include "lcd_image.h"
#include <math.h>

#define TFT_DC 9
#define TFT_CS 10
#define SD_CS 6

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define DISPLAY_WIDTH  320
#define DISPLAY_HEIGHT 240
#define YEG_SIZE 2048

lcd_image_t yegImage = { "yeg-big.lcd", YEG_SIZE, YEG_SIZE };

#define JOY_VERT  A1 // should connect A1 to pin VRx
#define JOY_HORIZ A0 // should connect A0 to pin VRy
#define JOY_SEL   2

#define JOY_CENTER   512
#define JOY_CENTRE   512
#define JOY_DEADZONE 250

#define CURSOR_SIZE 9

// the cursor position on the display
int cursorX, cursorY;

// forward declaration for redrawing the cursor
void redrawCursor(uint16_t colour);

// initializing values
int previouscursorX, previouscursorY;

void setup() {
  init();

  Serial.begin(9600);

	pinMode(JOY_SEL, INPUT_PULLUP);

	tft.begin();

  //Loads data from SD card
	Serial.print("Initializing SD card...");
	if (!SD.begin(SD_CS)) {
		Serial.println("failed! Is it inserted properly?");
		while (true) {}
	}
	Serial.println("OK!");

	tft.setRotation(0);

  tft.fillScreen(ILI9341_BLACK);

  // draws the centre of the Edmonton map, leaving the rightmost 48 columns black
	int yegMiddleX = YEG_SIZE/2 - (DISPLAY_WIDTH - 48)/2;
	int yegMiddleY = YEG_SIZE/2 - DISPLAY_HEIGHT/2;
	lcd_image_draw(&yegImage, &tft, yegMiddleX, yegMiddleY,
                 0, 0, DISPLAY_WIDTH - 80, DISPLAY_HEIGHT);

  // initial cursor position is the middle of the screen
  cursorX = (DISPLAY_WIDTH - 48)/2;
  cursorY = DISPLAY_HEIGHT/2;
  previouscursorX = cursorX;
  previouscursorY = cursorY;
  redrawCursor(ILI9341_RED);
}

//draws the next frame of the cursor onto the display 
void redrawCursor(uint16_t colour) 
{
  tft.fillRect(cursorX - CURSOR_SIZE/2, cursorY - CURSOR_SIZE/2,
               CURSOR_SIZE, CURSOR_SIZE, colour);
}

//Converts joystick input to moving cursor on display
void processJoystick1() 
{
  //Reads joystick input
  int xVal = analogRead(JOY_HORIZ);
  int yVal = analogRead(JOY_VERT);

  //Adjusts speed of cursor depending on how far the joystick is pushed
  int xSpeed = -1*((xVal/167) - 3);
  int ySpeed = ((yVal/167) - 3);
  int buttonVal = digitalRead(JOY_SEL);
  int moving = 0;

  //Calculates the next location of the cursor with border blocking
  if (xSpeed != 0) 
  {
    if ((xSpeed < 0) && (cursorX <= 8))
    {cursorX = 8;}
    else if ((xSpeed > 0) && (cursorX >= 232))
    {cursorX = 232;}
    else 
    {
    previouscursorX = cursorX;
    cursorX += xSpeed;
    moving = 1;
    }
  }
  if (ySpeed != 0) 
  {
    if ((ySpeed < 0) && (cursorY <= 8))
    {cursorY = 8;}
    else if ((ySpeed > 0) && (cursorY >= 232))
    {cursorY = 232;}
    else 
    {
    previouscursorY = cursorY;
    cursorY += ySpeed;
    moving = 1;
    }
  }

  
  int yegMiddleX;
  int yegMiddleY;
  //Checks if cursor needs to be redrawn
  if (moving == 1)
  {
    //Covers up the red pixels from the previous frame of the cursor with the Edmonton map, as well
	  yegMiddleX = YEG_SIZE / 2 - (DISPLAY_WIDTH - 48) / 2;
	  yegMiddleY = YEG_SIZE / 2 - DISPLAY_HEIGHT / 2;
	  lcd_image_draw(&yegImage, &tft,
		  // upper left corner in the image to draw
		  yegMiddleX + previouscursorX - 3 - CURSOR_SIZE/2, yegMiddleY + previouscursorY - 3 - CURSOR_SIZE/2,
		  // upper left corner of the screen to draw it in
		  previouscursorX - 3 - CURSOR_SIZE/2, previouscursorY - 3 - CURSOR_SIZE/2,
		  // width and height of the patch of the image to draw
		  CURSOR_SIZE + 6, CURSOR_SIZE + 6);
    //Draws cursor in new position
	  redrawCursor(ILI9341_RED);
	  // moving = 0;
  }
}

int main() {
	setup();
  int moving = 0;
  //Constantly reading for joystick input
  while (true) 
  {
    processJoystick1();
  }
	Serial.end();
	return 0;
}
