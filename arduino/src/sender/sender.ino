#include <Arduino.h>
#include <CANSAME5x.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_NeoMatrix.h>

#include "can.h"

#define LED_PIN 5
#define LED_WIDTH 8
#define LED_HEIGHT 4

#define X_AXIS_PIN 14
#define Y_AXIS_PIN 15

#define STATIC_IMAGE_MODE 0
#define DYNAMIC_IMAGE_MODE 1
#define NO_IMAGE_MODE 2

CANSAME5x CAN;
int mode = 1;
auto matrix = Adafruit_NeoMatrix(LED_WIDTH,
                                 LED_HEIGHT,
                                 LED_PIN,
                                 NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE,
                                 NEO_GRB);

int xAccel = 0;
int yAccel = 0;
float scaledY = 0.0;
int scaledYInt = 0;

const int senstivity = 100;
int yMin = 1000;
int yMax = 0;
int yAvg = 0;
int val = 0;
int sample_count = 0;
int cooldown = 0;
int grid[LED_WIDTH][LED_HEIGHT] = { 0 };

unsigned long lastSendMoveTimestamp = millis();
unsigned long currentMillis;
long messageCooldown = 500;

int currentImageNumber = 0;

void changeModeAndSend() {
  mode++;
  if (mode > 2) {
    mode = 0;
  }

  if (mode == STATIC_IMAGE_MODE || mode == DYNAMIC_IMAGE_MODE) {
    CAN.beginPacket(SCREEN_ON_PACKET_ID);
  } else if (mode == NO_IMAGE_MODE) {
    CAN.beginPacket(SCREEN_OFF_PACKET_ID);
  }
  CAN.endPacket();
}

// TODO damper w/ integral?
void sendMove(int yAcceleration) {
  currentMillis = millis();

  if (currentMillis - lastSendMoveTimestamp < messageCooldown) {
    return;
  }

  scaledY = (yAccel / 1000.0) * 100.0;
  scaledYInt = static_cast<int>(round(scaledY));
  Serial.printf("scaledY value=%d at %d\n", scaledYInt, currentMillis);
  CAN.beginPacket(MOVE_IMAGE_PACKET_ID);
  CAN.write(scaledYInt);
  CAN.endPacket();
  lastSendMoveTimestamp = currentMillis;
}

uint16_t color;
void showMode() {
  if (mode == STATIC_IMAGE_MODE) {
    color = Adafruit_NeoMatrix::Color(5, 0, 0);
  } else if (mode == DYNAMIC_IMAGE_MODE) {
    color = Adafruit_NeoMatrix::Color(0, 5, 0);
  } else if (mode == NO_IMAGE_MODE) {
    color = Adafruit_NeoMatrix::Color(0, 0, 5);
  }
  matrix.drawPixel(0, 0, color);
}

void updateDisplay(int yAcceleration) {
  if (cooldown == 0) {
    // we moving back
    if (yAcceleration > yAvg + senstivity) {
      grid[3][3] = 255;
      grid[4][3] = 255;
    }
    // we moving forward
    else if (yAcceleration < yAvg - senstivity) {
      grid[3][0] = 255;
      grid[4][0] = 255;
    }
  } else if (cooldown < 5) {
    cooldown++;
  } else {
    cooldown = 0;
  }

  for (short x = 0; x < LED_WIDTH; x++) {
    for (short y = 0; y < LED_HEIGHT; y++) {
      val = grid[x][y];
      matrix.drawPixel(x, y, Adafruit_NeoMatrix::Color(0, val, 0));

      if (val > 0) {
        grid[x][y] -= 5;
      }
    }
  }
  matrix.show();
}

void setup() {
  Serial.begin(115200);
  // while (!Serial) delay(10);

  pinMode(PIN_CAN_STANDBY, OUTPUT);
  digitalWrite(PIN_CAN_STANDBY, false);  // turn off STANDBY
  pinMode(PIN_CAN_BOOSTEN, OUTPUT);
  digitalWrite(PIN_CAN_BOOSTEN, true);  // turn on booster

  // start the CAN bus at 250 kbps
  if (!CAN.begin(250000)) {
    Serial.println("Starting CAN failed!");
  }

  matrix.begin();
  matrix.fillScreen(Adafruit_NeoMatrix::Color(0, 0, 0));
  matrix.show();
}

void loop() {
  // TODO - check for button hold
  if (false) {
    changeModeAndSend();
  }

  // TODO - check for button press
  if (false) {
    CAN.beginPacket(SET_IMAGE_PACKET_ID);
    currentImageNumber++;

    if (currentImageNumber > 3) {
      currentImageNumber = 0;
    // }

    CAN.write(currentImageNumber);
    CAN.endPacket();

    // update display
  }

  xAccel = analogRead(X_AXIS_PIN);
  yAccel = analogRead(Y_AXIS_PIN);
  yMin = min(yAccel, yMin);
  yMax = max(yAccel, yMax);
  if (sample_count < 5) {
    sample_count++;
    yAvg = (yAvg + yAccel) / 2;
  }
  if (sample_count < 5) {
    return;
  }

  if (mode == DYNAMIC_IMAGE_MODE) {
    sendMove(yAccel);
    updateDisplay(yAccel);
  }

  delay(5);
}
