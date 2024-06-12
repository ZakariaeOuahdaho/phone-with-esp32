#include "FS.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SoftwareSerial.h>

TFT_eSPI tft = TFT_eSPI();
SoftwareSerial sim800l(16, 17); // RX, TX pour SIM800L

#define CALIBRATION_FILE "/TouchCalData1"
#define REPEAT_CAL false

#define KEY_X 40
#define KEY_Y 96
#define KEY_W 62
#define KEY_H 30
#define KEY_SPACING_X 18
#define KEY_SPACING_Y 20
#define KEY_TEXTSIZE 1

#define LABEL1_FONT &FreeSansOblique12pt7b
#define LABEL2_FONT &FreeSansBold12pt7b

#define DISP_X 1
#define DISP_Y 10
#define DISP_W 238
#define DISP_H 50
#define DISP_TSIZE 3
#define DISP_TCOLOR TFT_CYAN

#define NUM_LEN 12
char numberBuffer[NUM_LEN + 1] = "";
uint8_t numberIndex = 0;

#define STATUS_X 120
#define STATUS_Y 65

char keyLabel[15][5] = {"New", "Del", "Send", "1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "#" };
uint16_t keyColor[15] = {TFT_RED, TFT_DARKGREY, TFT_DARKGREEN,
                         TFT_BLUE, TFT_BLUE, TFT_BLUE,
                         TFT_BLUE, TFT_BLUE, TFT_BLUE,
                         TFT_BLUE, TFT_BLUE, TFT_BLUE,
                         TFT_BLUE, TFT_BLUE, TFT_BLUE
                        };

TFT_eSPI_Button key[15];

void touch_calibrate();
void sendCommand(const String& command);
void readResponse();

void setup() {
  Serial.begin(9600);
  sim800l.begin(9600);

  tft.init();
  tft.setRotation(0);
  touch_calibrate();

  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 240, 320, TFT_DARKGREY);
  tft.fillRect(DISP_X, DISP_Y, DISP_W, DISP_H, TFT_BLACK);
  tft.drawRect(DISP_X, DISP_Y, DISP_W, DISP_H, TFT_WHITE);

  drawKeypad();

  // appèle du module  SIM800L
  delay(1000);
  sendCommand("AT");
  delay(1000);
  sendCommand("AT+CMGF=1");  // mise en mode sms
  delay(1000);
  sendCommand("AT+CSCS=\"GSM\"");  // convertion du module sim en mode lettre
}

void loop(void) {
  uint16_t t_x = 0, t_y = 0;
  bool pressed = tft.getTouch(&t_x, &t_y);

  for (uint8_t b = 0; b < 15; b++) {
    if (pressed && key[b].contains(t_x, t_y)) {
      key[b].press(true);
    } else {
      key[b].press(false);
    }
  }

  for (uint8_t b = 0; b < 15; b++) {
    if (b < 3) tft.setFreeFont(LABEL1_FONT);
    else tft.setFreeFont(LABEL2_FONT);

    if (key[b].justReleased()) key[b].drawButton();

    if (key[b].justPressed()) {
      key[b].drawButton(true);

      if (b >= 3) {
        if (numberIndex < NUM_LEN) {
          numberBuffer[numberIndex] = keyLabel[b][0];
          numberIndex++;
          numberBuffer[numberIndex] = 0;
        }
        status("");
      }

      if (b == 1) {
        numberBuffer[numberIndex] = 0;
        if (numberIndex > 0) {
          numberIndex--;
          numberBuffer[numberIndex] = 0;
        }
        status("");
      }

      if (b == 2) {
        status("Appel en cours...");
        String command = "ATD+212" + String(numberBuffer) + ";";
        sendCommand(command);
        Serial.println("Calling: " + String(numberBuffer));
      }

      if (b == 0) {
        status("Valeur effacée");
        numberIndex = 0;
        numberBuffer[numberIndex] = 0;
      }

      tft.setTextDatum(TL_DATUM);
      tft.setFreeFont(&FreeSans18pt7b);
      tft.setTextColor(DISP_TCOLOR);
      int xwidth = tft.drawString(numberBuffer, DISP_X + 4, DISP_Y + 12);
      tft.fillRect(DISP_X + 4 + xwidth, DISP_Y + 1, DISP_W - xwidth - 5, DISP_H - 2, TFT_BLACK);
      delay(10);
    }
  }

  readResponse();
}

void drawKeypad() {
  for (uint8_t row = 0; row < 5; row++) {
    for (uint8_t col = 0; col < 3; col++) {
      uint8_t b = col + row * 3;
      if (b < 3) tft.setFreeFont(LABEL1_FONT);
      else tft.setFreeFont(LABEL2_FONT);

      key[b].initButton(&tft, KEY_X + col * (KEY_W + KEY_SPACING_X),
                        KEY_Y + row * (KEY_H + KEY_SPACING_Y),
                        KEY_W, KEY_H, TFT_WHITE, keyColor[b], TFT_WHITE,
                        keyLabel[b], KEY_TEXTSIZE);
      key[b].drawButton();
    }
  }
}

void touch_calibrate() {
  uint16_t calData[5];

  bool calDataOK = 0;

  if (!SPIFFS.begin()) {
    Serial.println("formatting file system");
    SPIFFS.format();
    SPIFFS.begin();
  }

  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL) {
      SPIFFS.remove(CALIBRATION_FILE);
    } else {
      File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    tft.setTouch(calData);
  } else {
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }

    tft.setTouch(calData);
  }
}

void sendCommand(const String& command) {
  Serial.println("Sending command: " + command);
  sim800l.println(command);
  delay(100);  // Ajouter un délai pour permettre au module de répondre
  Serial.println("Command sent: " + command);
}

void readResponse() {
  while (sim800l.available()) {
    String response = sim800l.readString();
    Serial.println("Response: " + response);
    status(response.c_str());
  }
}

void status(const char *msg) {
  tft.setTextPadding(240);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextFont(0);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.drawString(msg, STATUS_X, STATUS_Y);
}
