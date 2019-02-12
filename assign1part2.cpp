/*
  Names:  Ang Li  Matthew Braun
  IDs:    1550746 1497171
  CMPUT 275, Winter 2019

  Assignment 1: Restaurant Finder Part 2

  Modified form the solution for weekly exercise 1
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <math.h>
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
#define JOY_SEL   23

#define JOY_CENTER   512
#define JOY_DEADZONE 64

#define CURSOR_SIZE 9

// smaller numbers yield faster cursor movement
#define JOY_SPEED 64

// the cursor position on the display, stored as the middle pixel of the cursor
// initial cursor position is the middle of the screen
int  cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
int  cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;

// previous state of the JOY_VERT
int prevJOY_VERT = JOY_CENTER;

// lower-left coordinates in the image of the map of Edmonton to be displayed
int mapOriginX = MAP_WIDTH/2 - (DISPLAY_WIDTH - 48)/2;
int mapOriginY = MAP_HEIGHT/2 - DISPLAY_HEIGHT/2;

// info of the restaurants
struct restaurant {
    int32_t lat;
    int32_t lon;
    uint8_t rating; // from 0 to 10
    char name[55];
};

// for quicker access when 2 consecutive restaurants are being called
uint32_t prevBlock = 100;
restaurant restBlockFast[8];

// info of the restaurants
struct RestDist {
    uint16_t index ; // index of restaurant from 0 to NUM_RESTAURANTS - 1
    uint16_t dist ; // Manhattan distance to cursor position
};

RestDist rest_dist[NUM_RESTAURANTS];

// minRating cycles from 0 to 4
// the actual rating threshold = minRating + 1
int minRating = 0;

// selectedSort cycles form 0 to 2 representing
// which sorting algorithm would be used in mode 1
// 0 = quick sort
// 1 = insertion sort
// 2 = both
int selectedSort = 0;

// qualifiedRests = the number of the restaurants that >= the minRating + 1
int qualifiedRests;

// forward declaration for drawing the cursor
void redrawCursor(int newX, int newY, int oldX, int oldY);

void setup() {
    init();

    Serial.begin(9600);
    Serial.flush();

    pinMode(JOY_SEL, INPUT_PULLUP);

    tft.begin();

    // SD card initialization for reading the map
    Serial.print("Initializing SD card...");
    if (!SD.begin(SD_CS)) {
        Serial.println("failed!");
        while (true) {}
    }
    Serial.println("OK!");

    // SD card initialization for raw reads
    Serial.print("Initializing SPI...");
    if (!card.init(SPI_HALF_SPEED, SD_CS)) {
        Serial.println("failed!");
        while (true) {}
    }
    else {
        Serial.println("OK!");
    }

    tft.setRotation(3);
}

// redraw the patch of Edmonton over the older cursor position
// given by "oldX, oldY"
// and draws the cursor at its new position given by "newX, newY"
void redrawCursor(int newX, int newY, int oldX, int oldY) {
    // draw the patch of Edmonton over the old cursor
    lcd_image_draw(&yegImage, &tft,
                    mapOriginX + oldX, mapOriginY + oldY,
                    oldX, oldY, CURSOR_SIZE, CURSOR_SIZE);

    // and now draw the cursor at the new position
    tft.fillRect(newX, newY, CURSOR_SIZE, CURSOR_SIZE, ILI9341_RED);
}

// shift the map over when the cursor is brought to the edge of the screen
void shiftMap() {
    // if the cursor moves beyond the upper boundary
    if (cursorX > MAP_DISP_WIDTH - CURSOR_SIZE && mapOriginX < MAP_WIDTH - MAP_DISP_WIDTH) {
        // shifts the mapOriginX up by MAP_DISP_WIDTH/2
        mapOriginX += MAP_DISP_WIDTH/2;

        // draw the updated part of the Edmonton map
        mapOriginX = constrain(mapOriginX, 0, MAP_WIDTH - MAP_DISP_WIDTH);
        lcd_image_draw(&yegImage, &tft, mapOriginX, mapOriginY,
                    0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);

        // set the cursor to the middle of the display
        cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
        cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
    }
    // if the cursor moves beyond the lower boundary
    else if (cursorX < 0 && mapOriginX > 0) {
        // shifts the mapOriginX down by MAP_DISP_WIDTH/2
        mapOriginX -= MAP_DISP_WIDTH/2;

        mapOriginX = constrain(mapOriginX, 0, MAP_WIDTH - MAP_DISP_WIDTH);
        lcd_image_draw(&yegImage, &tft, mapOriginX, mapOriginY,
                    0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);

        cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
        cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
    }

    // if the cursor moves beyond the right boundary
    if (cursorY > MAP_DISP_HEIGHT - CURSOR_SIZE && mapOriginY < MAP_HEIGHT - MAP_DISP_HEIGHT) {
        // shifts the mapOriginX right by MAP_DISP_WIDTH/2
        mapOriginY += MAP_DISP_HEIGHT/2;

        mapOriginY = constrain(mapOriginY, 0, MAP_HEIGHT - MAP_DISP_HEIGHT);
        lcd_image_draw(&yegImage, &tft, mapOriginX, mapOriginY,
                    0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);

        cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
        cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
    }
    // if the cursor moves beyond the left boundary
    else if (cursorY < 0 && mapOriginY > 0) {
        // shifts the mapOriginX left by MAP_DISP_WIDTH/2
        mapOriginY -= MAP_DISP_HEIGHT/2;

        mapOriginY = constrain(mapOriginY, 0, MAP_HEIGHT - MAP_DISP_HEIGHT);
        lcd_image_draw(&yegImage, &tft, mapOriginX, mapOriginY,
                    0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);

        cursorX = (DISPLAY_WIDTH - 48 - CURSOR_SIZE)/2;
        cursorY = (DISPLAY_HEIGHT - CURSOR_SIZE)/2;
    }
}

// process joystick controls for mode 0
void processJoystick0() {
    int xVal = analogRead(JOY_HORIZ);
    int yVal = analogRead(JOY_VERT);

    // copy the cursor position (to check later if it changed)
    int oldX = cursorX;
    int oldY = cursorY;

    // move the cursor, further pushes mean faster movement
    cursorX += (JOY_CENTER - xVal) / JOY_SPEED;
    cursorY += (yVal - JOY_CENTER) / JOY_SPEED;

    // shift the map over when the cursor is brought to the edge of the screen
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
            Serial.println("Read block failed.");
        }
        prevBlock = blockNum;
    }

    *restPtr = restBlockFast[restIndex % 8];
}

// These 4 functions convert between x / y map position and lat / lon
// (and vice versa.)
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

// draw the rating threshold
void drawMinRating() {
    tft.fillRect(273, 1, 47, 118, ILI9341_WHITE);
    tft.setTextColor(ILI9341_BLUE);
    tft.setCursor(290, 52);
    tft.println(minRating + 1);
}

// draw which sorting algorithm would be used in mode 1
void drawSortSelection() {
    tft.fillRect(273, 121, 47, 118, ILI9341_WHITE);
    switch (selectedSort) {
        // for quick sort
        case 0:
            tft.drawChar(290, 140, 'Q', ILI9341_BLUE, ILI9341_WHITE, 2);
            tft.drawChar(290, 156, 'S', ILI9341_BLUE, ILI9341_WHITE, 2);
            tft.drawChar(290, 172, 'O', ILI9341_BLUE, ILI9341_WHITE, 2);
            tft.drawChar(290, 188, 'R', ILI9341_BLUE, ILI9341_WHITE, 2);
            tft.drawChar(290, 204, 'T', ILI9341_BLUE, ILI9341_WHITE, 2);
            break;

        // for insert sort
        case 1:
            tft.drawChar(290, 140, 'I', ILI9341_BLUE, ILI9341_WHITE, 2);
            tft.drawChar(290, 156, 'S', ILI9341_BLUE, ILI9341_WHITE, 2);
            tft.drawChar(290, 172, 'O', ILI9341_BLUE, ILI9341_WHITE, 2);
            tft.drawChar(290, 188, 'R', ILI9341_BLUE, ILI9341_WHITE, 2);
            tft.drawChar(290, 204, 'T', ILI9341_BLUE, ILI9341_WHITE, 2);
            break;

        // for both
        case 2:
            tft.drawChar(290, 145, 'B', ILI9341_BLUE, ILI9341_WHITE, 2);
            tft.drawChar(290, 161, 'O', ILI9341_BLUE, ILI9341_WHITE, 2);
            tft.drawChar(290, 177, 'T', ILI9341_BLUE, ILI9341_WHITE, 2);
            tft.drawChar(290, 193, 'H', ILI9341_BLUE, ILI9341_WHITE, 2);
            break;
    }
}

// display blue dots represent the restaurants
// or change the rating threshold
// or change the sorting algorithm
// depends on which part of the screen is tapped
void tapScreen() {
    TSPoint touch = ts.getPoint();

    // map the values obtained from the touch screen
    // to the x, y coordinates in display
    int16_t screen_x = map(touch.y, TS_MINY, TS_MAXY, DISPLAY_WIDTH-1, 0);
    int16_t screen_y = map(touch.x, TS_MINX, TS_MAXX, 0, DISPLAY_HEIGHT-1);

    // display blue dots represent the restaurants
    if (screen_x <= 272) {
        restaurant rest;
        for (int i = 0; i < NUM_RESTAURANTS; i++) {
            getRestaurantFast(i, &rest);
            // if the restaurant is in part of the displayed map & meets the required rating
            int16_t restX = lon_to_x(rest.lon);
            if (restX > mapOriginX + 3 && restX < mapOriginX + MAP_DISP_WIDTH - 3 &&
                max(floor(((double) rest.rating + 1)/2), 1) >= minRating + 1) {
                int16_t restY = lat_to_y(rest.lat);
                if (restY > mapOriginY + 3 && restY < mapOriginY + MAP_DISP_HEIGHT - 3) {
                    // draw the blue dot which indicates its position
                    tft.fillCircle(restX - mapOriginX,
                    restY - mapOriginY, CURSOR_SIZE/2, ILI9341_BLUE);
                }
            }
        }
    }
    else {
        if (screen_y > 120) {
            // change the sorting algorithm
            selectedSort = (selectedSort + 1) % 3;
            drawSortSelection();
            delay(300);
        }
        else {
            // change the rating threshold
            minRating = (minRating + 1) % 5;
            drawMinRating();
            delay(300);
        }
    }
}

// process mode 0
void mode0() {
    tft.setTextSize(2);

    // draw the buttons which won't be further modified
    tft.drawFastHLine(272, 119, 48, ILI9341_BLACK);
    tft.drawRect(272, 0, 48, 119, ILI9341_RED);
    tft.drawRect(272, 120, 48, 119, ILI9341_GREEN);

    // draw the rating threshold
    drawMinRating();
    // draw the selected sorting algorithm
    drawSortSelection();

    // draws the center of the Edmonton map, leaving the rightmost 48 columns black
    lcd_image_draw(&yegImage, &tft, mapOriginX, mapOriginY,
        0, 0, MAP_DISP_WIDTH, MAP_DISP_HEIGHT);

    // draw the initial cursor
    redrawCursor(cursorX, cursorY, cursorX, cursorY);

    // while the joystick is not pressed
    while (digitalRead(JOY_SEL) != LOW) {
        processJoystick0();

        TSPoint touch = ts.getPoint();
        // if the screen is not being tapped
        if (touch.z < MINPRESSURE || touch.z > MAXPRESSURE) {
        continue; // just go to the top of the loop again
        }
        tapScreen();
    }
}

// process joystick controls for mode 1
// change the selectedRest according to the position of the joystick
// if the joystick is pushed(pull & release) downward, selectedRest -= 1
// if the joystick is pushed upward, selectedRest += 1
void processJoystick1(int* selectedRest) {
    // if the joystick is pushed downward
    if (analogRead(JOY_VERT) < JOY_CENTER - JOY_DEADZONE && prevJOY_VERT > JOY_CENTER - JOY_DEADZONE) {
        *selectedRest -= 1;
    }
    // if the joystick is pushed upward
    else if (analogRead(JOY_VERT) > JOY_CENTER + JOY_DEADZONE && prevJOY_VERT < JOY_CENTER + JOY_DEADZONE) {
        *selectedRest += 1;
    }

    // previous state of the JOY_VERT
    prevJOY_VERT = analogRead(JOY_VERT);
    delay(10);
}

// swap elements between 2 RestDist struct
void swap(RestDist* a, RestDist* b) {
    RestDist temp = *a;
    *a = *b;
    *b = temp;
}

// the following 2 quick sort algorithms is modified from the quicksort.cpp
// provided in CMPUT 274
int partition (RestDist arr[], int low, int high) 
{ 
    RestDist pivot = arr[high]; // pivot 
    int i = (low - 1); // Index of smaller element 

    for (int j = low; j <= high- 1; j++) 
    { 
        // If current element is smaller than or 
        // equal to pivot 
        if (arr[j].dist <= pivot.dist) 
        { 
            i++; // increment index of smaller element 
            swap(&arr[i], &arr[j]); 
        } 
    } 
    swap(&arr[i + 1], &arr[high]); 
    return (i + 1); 
} 

/* The main function that implements QuickSort 
arr[] --> Array to be sorted, 
low --> Starting index, 
high --> Ending index */
void qSort(RestDist arr[], int low, int high) 
{ 
    if (low < high) 
    { 
        /* pi is partitioning index, arr[p] is now 
        at right place */
        int pi = partition(arr, low, high); 

        // Separately sort elements before 
        // partition and after partition 
        qSort(arr, low, pi - 1); 
        qSort(arr, pi + 1, high); 
    } 
} 

// Insertion Sort
// at the start of each iteration i, the array A[0]...A[i − 1] will be already sorted
// the inner loop takes the next element A[i] and swaps it
// back through the sorted array until it finds its place
// after this, the sub array A[0]...A[i] is sorted
// and we are ready to increment i and repeat
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

// get all the restaurants that are above the rating threshold
// calculate its Manhattan distance & store the info to rest_dist[]
// qualifiedRest = number of the restaurants that are above the rating threshold
void getQualifiedRest() {
    qualifiedRests = 0;

    for (uint16_t i = 0; i < NUM_RESTAURANTS; i++) {
        restaurant r;
        getRestaurantFast(i, &r);
        // if the rating of that restaurant is below the rating threshold
        // stop fetching its info
        if (max(floor(((double) r.rating + 1)/2), 1) >= minRating + 1) {
            rest_dist[qualifiedRests].index = i;
            // calculate its Manhattan distance
            rest_dist[qualifiedRests].dist = abs(lon_to_x(r.lon) - (cursorX + mapOriginX + CURSOR_SIZE/2))
            + abs(lat_to_y(r.lat) - (cursorY + mapOriginY + CURSOR_SIZE/2)); //assign index & distance to each RestDist element
            qualifiedRests += 1;
        }
    }
}

// sort the qualified restaurants by its distance from cursor
// using desired sorting algorithm
// print the running time to the serial-mon
void getNearestRest() {
    uint32_t startTime;
    // sort the restaurant by its Manhattan distance
    switch (selectedSort) {
        // quick sort
        case 0:
            getQualifiedRest();
            Serial.print("Quick Sort ");
            Serial.print(qualifiedRests);
            Serial.print(" restaurants: ");
            startTime = millis();
            qSort(&rest_dist[0], 0, qualifiedRests-1);
            Serial.print(millis() - startTime);
            Serial.print(" ms\n\r");
            Serial.flush();
            break;
        // insertion sort
        case 1:
            getQualifiedRest();
            Serial.print("Insertion Sort ");
            Serial.print(qualifiedRests);
            Serial.print(" restaurants: ");
            startTime = millis();
            iSort(&rest_dist[0], qualifiedRests);
            Serial.print(millis() - startTime);
            Serial.print(" ms\n\r");
            Serial.flush();
            break;
        // both
        case 2:
            getQualifiedRest();
            Serial.print("Quick Sort ");
            Serial.print(qualifiedRests);
            Serial.print(" restaurants: ");
            startTime = millis();
            qSort(&rest_dist[0], 0, qualifiedRests);
            Serial.print(millis() - startTime);
            Serial.print(" ms\n\r");
            Serial.flush();
            // Reloads the RestDist array so quick sort is not sorting the already sorted array
            getQualifiedRest();
            Serial.print("Insertion Sort ");
            Serial.print(qualifiedRests);
            Serial.print(" restaurants: ");
            startTime = millis();
            iSort(&rest_dist[0], qualifiedRests);
            Serial.print(millis() - startTime);
            Serial.print(" ms\n\r");
            Serial.flush();
            break;
    }
}

// list one page of sorted restaurants
// startRest = index of the first restaurant in the list
// highlightedRest = index of the highlighted restaurant
void drawList(int startRest, int highlightedRest) {
    tft.fillScreen(0);
    tft.setCursor(0, 0); // where the characters will be displayed
    tft.setTextWrap(false);

    restaurant r;

    // list 30 restaurants
    // unless there are less than 30 restaurants in the last page
    for (int16_t i = startRest; i < constrain(startRest + 30, 0, qualifiedRests); i ++) {
        getRestaurantFast(rest_dist[i].index, &r);
        if (i != highlightedRest) { // not highlighted
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
}

// process mode 1
void mode1() {
    tft.setTextSize(1);

    // sort the qualified restaurants by its distance from cursor
    getNearestRest();

    // list the fist page of sorted restaurants
    int startRest = 0;
    drawList(startRest, 0);

    int selectedRest = 0; // which restaurant is selected
    restaurant r;
    getRestaurantFast(rest_dist[0].index, &r);

    // if the joystick is not being pushed
    while (digitalRead(JOY_SEL) != LOW) {
        // only need to redraw 2 restaurants
        int prevSelectedRest = selectedRest;

        processJoystick1(&selectedRest);

        // if the cursor goes beyond the lower boundary
        // & not in the last page
        // go to the next page
        if (startRest + 30 < qualifiedRests && selectedRest > 29) {
            startRest += 30;
            selectedRest = 0;
            prevSelectedRest = 0;
            getRestaurantFast(rest_dist[startRest].index, &r);
            drawList(startRest, startRest);
        }
        // if the cursor goes beyond the lower boundary
        // & in the last page
        // go to the first page
        else if (startRest + 30 >= qualifiedRests && selectedRest + startRest >= qualifiedRests) {
            startRest = 0;
            selectedRest = 0;
            prevSelectedRest = 0;
            getRestaurantFast(rest_dist[startRest].index, &r);
            // the highlightedRest = startRest + selectedRest
            drawList(startRest, startRest);
        }

        // if the cursor goes beyond the upper boundary
        // & not in the first page
        // go to previous page
        if (startRest - 30 >= 0 && selectedRest < 0) {
            startRest -= 30;
            selectedRest = 29;
            prevSelectedRest = 29;
            getRestaurantFast(rest_dist[startRest + 29].index, &r);
            drawList(startRest, startRest + 29);
        }
        // if the cursor goes beyond the upper boundary
        // & in the first page
        // go to last page
        else if (startRest - 30 < 0 && selectedRest < 0) {
            startRest = qualifiedRests - (qualifiedRests % 30);
            selectedRest = (qualifiedRests % 30) - 1;
            prevSelectedRest = (qualifiedRests % 30) - 1;
            getRestaurantFast(rest_dist[qualifiedRests - 1].index, &r);
            // the highlightedRest = startRest + selectedRest
            drawList(startRest, qualifiedRests - 1);
        }

        // change the highlighted restaurant
        // if cursor is moved
        if (selectedRest != prevSelectedRest) {
            getRestaurantFast(rest_dist[prevSelectedRest + startRest].index, &r);

            tft.setCursor(0, prevSelectedRest * 8); // where the characters will be displayed
            // white characters on black background
            tft.setTextColor(0xFFFF, 0x0000);
            tft.print(r.name);
            tft.print("\n");
            getRestaurantFast(rest_dist[selectedRest + startRest].index, &r);

            tft.setCursor(0, selectedRest * 8); // where the characters will be displayed
            // black characters on white background
            tft.setTextColor(0x0000, 0xFFFF);
            tft.print(r.name);
            tft.print("\n");
        }
    }

    // reposition mapOrigin to selected restaurant location
    mapOriginX = lon_to_x(r.lon) - MAP_DISP_WIDTH/2;
    mapOriginY = lat_to_y(r.lat) - MAP_DISP_HEIGHT/2;

    mapOriginX = constrain(mapOriginX, 0, MAP_WIDTH - MAP_DISP_WIDTH);
    mapOriginY = constrain(mapOriginY, 0, MAP_HEIGHT - MAP_DISP_HEIGHT);

    // reposition cursor to selected restaurant location
    cursorX = lon_to_x(r.lon) - mapOriginX - CURSOR_SIZE/2;
    cursorY = lat_to_y(r.lat) - mapOriginY - CURSOR_SIZE/2;

    // constrain so the cursor does not go off of the map display window
    cursorX = constrain(cursorX, 0, MAP_DISP_WIDTH - CURSOR_SIZE);
    cursorY = constrain(cursorY, 0, MAP_DISP_HEIGHT - CURSOR_SIZE);
}

int main() {
    setup();
    while(true) {
        // process mode 0
        mode0();
        // process mode 1
        mode1();
    }
    Serial.end();
    return 0;
}