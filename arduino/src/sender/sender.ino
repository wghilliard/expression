#include <Arduino.h>
#include <CANSAME5x.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_NeoMatrix.h>

#include "can.h"
#include "eye_math.h"

#define LED_PIN 5
#define LED_WIDTH 8
#define LED_HEIGHT 4

#define X_AXIS_PIN 14
#define Y_AXIS_PIN 15

#define STATIC_IMAGE_MODE 0
#define DYNAMIC_IMAGE_MODE 1
#define NO_IMAGE_MODE 2

const int displayRow = 3;


CANSAME5x CAN;
int mode = 1;
auto matrix = Adafruit_NeoMatrix(LED_WIDTH,
                                 LED_HEIGHT,
                                 LED_PIN,
                                 NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE,
                                 NEO_GRB);

int yAccel = 0;

// int yMin = 1000;
// int yMax = 0;
int yAvg = 0;
int ySum = 0;
int val = 0;
int sample_count = 0;
int cooldown = 0;
int grid[LED_WIDTH][LED_HEIGHT] = { 0 };

unsigned long lastSendMoveTimestamp = millis();
unsigned long currentMillis;
long messageCooldown = 100;

int currentImageNumber = 0;

const int circularBufferSize = 40;
int circularBuffer[circularBufferSize];
int circularBufferIndex = 0;

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
// canValue [0, 100]
void sendMove(int canValue) {
  currentMillis = millis();

  if (currentMillis - lastSendMoveTimestamp < messageCooldown) {
    return;
  }

  CAN.beginPacket(MOVE_IMAGE_PACKET_ID);
  CAN.write(canValue);
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

// [0, 100]
// [0, 0, 0, 0, 0, 0, 0, 0]

// [40, 60]
// [0, 7]
void updateDisplay(int canValue) {
  // if (cooldown == 0) {

  // // we moving back
  // if (yAcceleration > yAvg + senstivity) {
  //   grid[3][3] = 255;
  //   grid[4][3] = 255;
  // }
  // // we moving forward
  // else if (yAcceleration < yAvg - senstivity) {
  //   grid[3][0] = 255;
  //   grid[4][0] = 255;
  // }
  // } else if (cooldown < 5) {
  //   cooldown++;
  // } else {
  //   cooldown = 0;
  // }

  int index = ((canValue - 40) / 3);
  // Serial.printf("index=%d\n", index);
  index = max(index, 0);
  index = min(index, 7);
  grid[index][displayRow] = 255;

  for (short x = 0; x < LED_WIDTH; x++) {
    val = grid[x][displayRow];
    matrix.drawPixel(x, displayRow, Adafruit_NeoMatrix::Color(0, 0, val));

    if (val > 0) {
      grid[x][displayRow] -= 5;
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
    }

    CAN.write(currentImageNumber);
    CAN.endPacket();

    // update display
  }
  // xAccel = analogRead(X_AXIS_PIN);
  yAccel = analogRead(X_AXIS_PIN);
  // yMin = min(yAccel, yMin);
  // yMax = max(yAccel, yMax);
  // Serial.printf("yAccel=%d\n", yAccel);
  circularBuffer[circularBufferIndex] = yAccel;
  circularBufferIndex++;

  if (circularBufferIndex == circularBufferSize) {
    circularBufferIndex = 0;
  }

  ySum = 0;
  for (int i = 0; i < circularBufferSize; i++) {
    ySum += circularBuffer[i];
  }

  yAvg = ySum / circularBufferSize;

  if (sample_count < circularBufferSize) {
    sample_count++;
  }
  if (sample_count < circularBufferSize) {
    return;
  }

  // yAvg [0, 1000]
  // Serial.printf("yAvg=%d\n", yAvg);
  double yAccelMetersPerSecondSquared = (yAvg - 500.0) / 500 * 29.4;

  // tmp [-1, 1]
  double pupilHorizonalOffsetRatio = pupilHorizonalOffset(27.0, 20.0, yAccelMetersPerSecondSquared);

  // [-1, 1] -> [0, 100]
  int canValue = round((pupilHorizonalOffsetRatio + 1) * 50);
  // Serial.printf("yAccelMetersPerSecondSquared=%f pupilHorizonalOffsetRatio=%f canValue=%d at %d\n", yAccelMetersPerSecondSquared, pupilHorizonalOffsetRatio, canValue, currentMillis);

  canValue = min(canValue, 100);
  canValue = max(canValue, 0);
  if (mode == DYNAMIC_IMAGE_MODE) {
    // Serial.printf("canValue=%d\n", canValue);
    sendMove(canValue);
    updateDisplay(canValue);
  }

  delay(10);
}
