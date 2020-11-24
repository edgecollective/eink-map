// Marinus APRS Map Display
// Copyright 2012 Leigh L. Klotz, Jr.
// License: http://www.opensource.org/licenses/mit-license

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <SD.h>
#include <ArgentRadioShield.h>

/*****************************************************************
 * TFT Settings
 * If your TFT's plastic wrap has a Red Tab, use the following:
 * #define TAB_COLOR (INITR_REDTAB)
 * If your TFT's plastic wrap has a Green Tab, use the following:
 * #define TAB_COLOR(INITR_GREENTAB)
 *****************************************************************/

#define TAB_COLOR (INITR_REDTAB)

// TFT display and SD card share the hardware SPI interface.
#define SD_CS    4  // Chip select line for SD card
#define TFT_CS  10  // Chip select line for TFT display
#define TFT_DC 8
#define TFT_RST 0

// LCD Size
#define LCD_HEIGHT (160)
#define LCD_WIDTH (128)

// Colors
#define CALL_TEXT_COLOR ST7735_RED
#define ICON_COLOR ST7735_BLUE
#define LINE_COLOR ST7735_BLUE

// APRS Buffers
#define BUFLEN (260)
char packet[BUFLEN];
int buflen = 0;

// Maps and calls
int last_map_n;
char last_call[10];
int last_pix_down;
int last_pix_right;

// POI and Map info
// Location of center tile upper left corner
long POI_TL_LAT;
long POI_TL_LON;

int DEGREES_PER_PIXEL_LAT;
int DEGREES_PER_PIXEL_LON;

byte MAP_WIDTH_IN_TILES;
byte MAP_HEIGHT_IN_TILES;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
ArgentRadioShield argentRadioShield = ArgentRadioShield(&Serial);

#define FLOOR(a,b) (a < 0) ? (a/b)-1 : (a/b)

#define DEBUG_PRINT false

void setup(void) {
  Serial.begin(4800);
  tft.initR(TAB_COLOR);   // initialize a ST7735R chip, red or green tab
  tft.setTextWrap(true);
  tft.setCursor(0,0);
  tft.fillScreen(ST7735_BLUE);
  tft.setTextColor(ST7735_WHITE);
  tft.print(F("Marinus APRS Mapper\n\n(C) 2012 WA5ZNU\nMIT license\n\n"
	      "Data imagery and map\n"
	      "information provided\n"
	      "by MapQuest,\n"
	      "openstreetmap.org,\n"
	      "and contributors:\nCC-BY-SA-2.0\n\n"));

  if (!SD.begin(SD_CS)) {
    tft.setTextColor(ST7735_RED);
    tft.print(F("SD failed!"));
    return;
  }

  if (! readMapPOI()) {
    return;
  }

  delay(2000);

  // start with center map.  inits last_map_n as well.
  drawMap(0, 0);
}

void loop() {
  while (Serial.available()) {
    char ch = argentRadioShield.read();
    if (ch == '\n') {
      packet[buflen] = 0;
      show_packet();
      buflen = 0;
    } else if ((ch > 31 || ch == 0x1c || ch == 0x1d || ch == 0x27) && buflen < BUFLEN) {
      // Mic-E uses some non-printing characters
      packet[buflen++] = ch;
    }
  }
}

void show_packet() {
  char *call, *posit;
  char type;
  long lat, lon;
  if (DEBUG_PRINT) {
    Serial.print(F(" * "));
    Serial.println(packet);
  }
  if (argentRadioShield.decode_posit(packet, &call, &type, &posit, &lon, &lat)) {
    if (DEBUG_PRINT) {
      Serial.print(F(" call ")); Serial.print(call);
      Serial.print(F(" packet type ")); Serial.print(type);
      Serial.print(F(" posit ")); Serial.print(posit);
      Serial.print(F(" lat ")); Serial.print(lat, DEC);
      Serial.print(F(" lon ")); Serial.print(lon, DEC);
      Serial.println();
    }

    long dy = (POI_TL_LAT - lat) / DEGREES_PER_PIXEL_LAT;
    long dx = (lon - POI_TL_LON) / DEGREES_PER_PIXEL_LON;
    if (DEBUG_PRINT) {
      Serial.print(F(" down ")); Serial.print(dy, DEC);
      Serial.print(F(" right ")); Serial.println(dx, DEC);
    }

    int map_down = FLOOR(dy, LCD_HEIGHT);
    int map_right = FLOOR(dx, LCD_WIDTH);

    if (DEBUG_PRINT) {
      Serial.print(F(" map down ")); Serial.println(map_down, DEC);
      Serial.print(F(" map right ")); Serial.println(map_right, DEC);
    }

    int pix_down = dy % LCD_HEIGHT;
    int pix_right = dx % LCD_WIDTH;
    if (DEBUG_PRINT) {
      Serial.print(F(" in map down ")); Serial.print(pix_down, DEC);
      Serial.print(F(" in map right ")); Serial.println(pix_right, DEC);
    }

    display(call, posit, map_down, map_right, pix_down, pix_right);
  } else {
    if (DEBUG_PRINT) {
      Serial.println(F(" ! decode failed\n"));
    }
  }
}

void display(char *call, char *posit, int map_down,
	     int map_right, int pix_down, int pix_right) {
  // If the map is too many tiles away from the center, it's out of range so don't display.
  if ((abs(map_down) > MAP_HEIGHT_IN_TILES/2) || (abs(map_right) > MAP_WIDTH_IN_TILES/2)) 
    return;

  // Find the map number, and display it if it's not the current map.
  drawMap(map_down, map_right);

  // Find the spot within the map.  Adjust negative coordinates
  if (pix_right < 0) pix_right = LCD_WIDTH + pix_right;
  if (pix_down < 0) pix_down = LCD_HEIGHT + pix_down;

  // If it's the same callsign as last time, draw a line.
  if (strcmp(call, last_call) == 0) {
    if (last_pix_down != 0) {
      tft.drawLine(last_pix_right, last_pix_down, pix_right, pix_down, LINE_COLOR);
    }
  } else {
    // If not the same callsign, draw the callsign and start a new spot.
    bmdrawtext(call, CALL_TEXT_COLOR, pix_right, pix_down);
    // Remember the new callsign
    strlcpy(last_call, call, sizeof(last_call));
  }
  // Remember the last point we drew for this callsign.
  last_pix_down = pix_down;
  last_pix_right = pix_right;

  // Draw a simple icon at the position.
  tft.fillCircle(pix_right, pix_down, 2, ICON_COLOR);
}

// find the map file and load it if it's not the current map.
void drawMap(int map_down, int map_right) {
  char name[] = "map####.bmp";
  map_down += MAP_HEIGHT_IN_TILES/2;
  map_right += MAP_WIDTH_IN_TILES/2;
  int n = map_right * 100 + map_down;
  if (n != last_map_n) {
    char *p = name+3;
    *p++ = '0' + map_right / 10;
    *p++ = '0' + map_right % 10;
    *p++ = '0' + map_down / 10;
    *p++ = '0' + map_down % 10;
    bmpDraw(name, 0, 0);

    // Set the map number and reset callsign and last icon positions.
    last_map_n = n;
    last_call[0] = 0;
    last_pix_down = -1;
    last_pix_right = -1;
  }
}

void bmdrawtext(char *text, uint16_t color, byte x, byte y) {
  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}

boolean readMapPOI() {
  File poi;
  char *fn = "POI.CSV";
  if ((poi = SD.open(fn)) == NULL) {
    tft.print(fn);
    return false;
  }

  // skip first line of header text
  while (poi.read() != '\n') {
    ;
  }

  if (LCD_WIDTH != poi.parseInt()) return false;
  if (LCD_HEIGHT != poi.parseInt()) return false;

  {
    char qra[16];
    // skip comma after previous field
    poi.read();
    // read QRA string
    qra[poi.readBytesUntil(',', qra, sizeof(qra))] = 0;
    byte zoom =  poi.parseInt();
    tft.print(F("\n"));
    tft.print(qra);
    tft.print(F(" zoom "));
    tft.println(zoom, DEC);
  }

  POI_TL_LAT = poi.parseInt();
  POI_TL_LON = poi.parseInt();
  DEGREES_PER_PIXEL_LAT = poi.parseInt();
  DEGREES_PER_PIXEL_LON = poi.parseInt();
  MAP_WIDTH_IN_TILES = poi.parseInt();
  MAP_HEIGHT_IN_TILES = poi.parseInt();

  poi.close();
  return true;
}
