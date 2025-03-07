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

#define BUTTON_PIN 23

// #define STATIC_IMAGE_MODE 0
#define DYNAMIC_IMAGE_MODE 0
#define NO_IMAGE_MODE 1

#define NEXT_IMAGE_BUTTON_PRESS_DURATION 20
#define TOGGLE_SCREEN_BUTTON_PRESS_DURATION 1000

#define DISPLAY_ROW 3


CANSAME5x CAN;
int mode = 0;
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
unsigned long lastSendImageIndexTimestamp = millis();

unsigned long currentMillis;
long messageCooldown = 100;
long imageIndexMessageCooldown = 5000;

int buttonIsPressed = 0;
int currentImageNumber = 0;

const int circularBufferSize = 40;
int circularBuffer[circularBufferSize];
int circularBufferIndex = 0;

void changeModeAndSend() {
  mode++;
  if (mode > 1) {
    mode = 0;
  }

  if (mode == DYNAMIC_IMAGE_MODE) {
    CAN.beginPacket(SCREEN_ON_PACKET_ID);
  } else if (mode == NO_IMAGE_MODE) {
    CAN.beginPacket(SCREEN_OFF_PACKET_ID);
  }
  CAN.endPacket();
}

void changeImageAndSend() {
  CAN.beginPacket(SET_IMAGE_PACKET_ID);
  currentImageNumber++;

  if (currentImageNumber > 3) {
    currentImageNumber = 0;
  }

  CAN.write(currentImageNumber);
  CAN.endPacket();
}

// lateralAcceleration [0, 100]
// this method will send the current mode (just in case it was missed when it was initially set) and send the current acceraltion value
void sendMessages(int lateralAcceleration) {
  currentMillis = millis();

  // if (currentMillis - lastSendImageIndexTimestamp < imageIndexMessageCooldown) {
  //   CAN.beginPacket(SET_IMAGE_PACKET_ID);
  //   CAN.write(currentImageNumber);
  //   CAN.endPacket();
  //   lastSendImageIndexTimestamp = currentMillis;
  // }

  if (currentMillis - lastSendMoveTimestamp < messageCooldown) {
    CAN.beginPacket(MOVE_IMAGE_PACKET_ID);
    CAN.write(lateralAcceleration);
    CAN.endPacket();
    lastSendMoveTimestamp = currentMillis;
  }
}

void showModeAndImage() {
  if (mode == DYNAMIC_IMAGE_MODE) {
    matrix.drawPixel(0, 0, Adafruit_NeoMatrix::Color(0, 50, 50));
  }
  if (mode == NO_IMAGE_MODE) {
    matrix.drawPixel(0, 0, Adafruit_NeoMatrix::Color(50, 50, 0));
  }

  matrix.drawPixel(1, 0, Adafruit_NeoMatrix::Color(0, 0, (currentImageNumber + 1) * 50));
  matrix.show();
}

// [0, 100]
// [0, 0, 0, 0, 0, 0, 0, 0]

// [40, 60]
// [0, 7]
void updateAccelDisplay(int canValue) {
  int index = ((canValue - 40) / 3);
  // Serial.printf("index=%d\n", index);
  index = max(index, 0);
  index = min(index, 7);
  grid[index][DISPLAY_ROW] = 255;

  for (short x = 0; x < LED_WIDTH; x++) {
    val = grid[x][DISPLAY_ROW];
    matrix.drawPixel(x, DISPLAY_ROW, Adafruit_NeoMatrix::Color(0, 0, val));

    if (val > 0) {
      grid[x][DISPLAY_ROW] -= 5;
    }
  }
  matrix.show();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  pinMode(PIN_CAN_STANDBY, OUTPUT);
  digitalWrite(PIN_CAN_STANDBY, false);  // turn off STANDBY
  pinMode(PIN_CAN_BOOSTEN, OUTPUT);
  digitalWrite(PIN_CAN_BOOSTEN, true);  // turn on booster
  pinMode(BUTTON_PIN, INPUT);
  // start the CAN bus at 250 kbps
  if (!CAN.begin(250000)) {
    Serial.println("Starting CAN failed!");
  }

  matrix.begin();
  matrix.fillScreen(Adafruit_NeoMatrix::Color(0, 0, 0));
  matrix.show();
  showModeAndImage();
}

int loopDelay = 10;
int buttonPressDuration = 0;
bool buttonPressComplete = false;


void loop() {
  buttonIsPressed = digitalRead(BUTTON_PIN);
  // Serial.printf("buttonIsPressed=%d\n", buttonIsPressed);
  if (buttonIsPressed == HIGH) {
    if (buttonPressDuration == 0) {
      matrix.drawPixel(3, 0, Adafruit_NeoMatrix::Color(0, 0, 50));
      matrix.show();
    }
    buttonPressDuration += loopDelay;
  } else if (buttonPressDuration > 0) {
    buttonPressComplete = true;
    // Serial.printf("buttonPressComplete buttonPressDuration=%d\n", buttonPressDuration);
    matrix.drawPixel(3, 0, Adafruit_NeoMatrix::Color(0, 0, 0));
    matrix.show();
  }

  if (buttonPressComplete) {
    if (buttonPressDuration >= TOGGLE_SCREEN_BUTTON_PRESS_DURATION) {
      // Serial.printf("changing mode=%d\n", mode);
      changeModeAndSend();
    } else if (buttonPressDuration >= NEXT_IMAGE_BUTTON_PRESS_DURATION) {
      // Serial.printf("changing image=%d\n", currentImageNumber);
      changeImageAndSend();
    }

    showModeAndImage();

    buttonPressComplete = false;
    buttonPressDuration = 0;
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
  // Serial.printf("mode=%d\n", mode);
  if (mode == DYNAMIC_IMAGE_MODE) {
    // Serial.printf("canValue=%d\n", canValue);
    sendMessages(canValue);
    updateAccelDisplay(canValue);
  }

  delay(loopDelay);
}
