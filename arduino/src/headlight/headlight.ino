#include <Arduino_GFX_Library.h>
#include <Adafruit_XCA9554.h>

#include <pgmspace.h>
#include "driver/gpio.h"
#include "driver/twai.h"

// https://github.com/bitbank2/JPEGDEC
#include <JPEGDEC.h>

#include "images.h"
#include "can.h"


#define SCREEN_HEIGHT 720
#define SCREEN_WIDTH 720

Arduino_XCA9554SWSPI* expander = new Arduino_XCA9554SWSPI(PCA_TFT_RESET, PCA_TFT_CS, PCA_TFT_SCK, PCA_TFT_MOSI, &Wire, 0x3F);

Arduino_ESP32RGBPanel* rgbpanel = new Arduino_ESP32RGBPanel(
  TFT_DE, TFT_VSYNC, TFT_HSYNC, TFT_PCLK,
  TFT_R1, TFT_R2, TFT_R3, TFT_R4, TFT_R5,
  TFT_G0, TFT_G1, TFT_G2, TFT_G3, TFT_G4, TFT_G5,
  TFT_B1, TFT_B2, TFT_B3, TFT_B4, TFT_B5,
  1 /* hsync_polarity */, 44 /* hsync_front_porch */, 2 /* hsync_pulse_width */, 46 /* hsync_back_porch */,
  1 /* vsync_polarity */, 17 /* vsync_front_porch */, 2 /* vsync_pulse_width */, 19 /* vsync_back_porch */
  ,
  1, 11000000);

Arduino_RGB_Display* gfx = new Arduino_RGB_Display(720 /* width */, 720 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */, expander, GFX_NOT_DEFINED /* RST */, NULL, 0);

JPEGDEC jpeg;

bool displayOn = true;
int imageCursor = 0;

unsigned long lastMovedImageDrawnTimestamp = millis();
unsigned long currentMillis;
long messageCooldown = 500;
uint lastMessageValue = 0;

// int updateCount = 0;
// int renderedBytes = 0;
// int totalBytes = 720 * 720 * sizeof(uint16_t);

uint16_t** grid;
int gridSize = 0;
uint16_t* buffer;
int bufferSize = 0;


void loadImage(const uint8_t* compressedData, size_t size) {
  if (gridSize > 0) {
    for (int i = 0; i < gridSize; i++) {
      free(grid[i]);
    }
    free(grid);
    free(buffer);
  }

  gridSize = images[imageCursor].width;
  grid = (uint16_t**)ps_malloc(gridSize * sizeof(uint16_t*));
  bufferSize = images[imageCursor].width * images[imageCursor].height;
  buffer = (uint16_t*)ps_malloc(bufferSize * sizeof(uint16_t*));

  // Allocate an array of pointers
  // int** arr = malloc(rows * sizeof(int*));
  for (int i = 0; i < images[imageCursor].width; i++) {
    grid[i] = (uint16_t*)ps_malloc(images[imageCursor].height * sizeof(uint16_t));
  }

  jpeg.openFLASH((uint8_t*)compressedData, size, JPEGDraw);
  jpeg.decode(0, 0, 0);
  jpeg.close();


  int currentX = 0;
  int currentY = 0;
  for (int bufferIndex = 0; bufferIndex < bufferSize; bufferIndex++) {
    buffer[bufferIndex] = grid[currentX][currentY];
    currentX++;
    if (currentX >= gridSize) {
      currentX = 0;
      currentY++;
    }
  }
}

void drawImage(int xOffset, int yOffset) {
  // Serial.printf("xOffset=%d yOffset=%d\n", xOffset, yOffset);
  gfx->fillScreen(BLACK);

  // for (int xIndex = 0; xIndex < images[imageCursor].width; xIndex++) {
  //   for (int yIndex = 0; yIndex < images[imageCursor].height; yIndex++) {
  //     gfx->writePixel(xOffset + xIndex, yOffset + yIndex, grid[xIndex][yIndex]);
  //   }
  // }

  gfx->draw16bitRGBBitmap(xOffset, yOffset, buffer, images[imageCursor].width, images[imageCursor].height);
}

int JPEGDraw(JPEGDRAW* pDraw) {
  Serial.printf("pDraw->x=%d pDraw->y=%d\n", pDraw->x, pDraw->y);

  // renderedBytes += pDraw->iWidth * pDraw->iHeight * sizeof(uint16_t);
  // Serial.println(
  //   "updateCount=" + String(updateCount++)
  //   + " | writing x=" + String(pDraw->x) + " y=" + String(pDraw->y) + " w=" + String(pDraw->iWidth) + " h=" + String(pDraw->iHeight)
  //   // + " | RAM left: " + String(esp_get_free_heap_size())
  //   + " | task stack: " + String(uxTaskGetStackHighWaterMark(NULL))
  //   + " | Free PSRAM: " + String(ESP.getFreePsram()) + "/" + String(ESP.getPsramSize())
  //   + " | Free Heap RAM: " + String(ESP.getFreeHeap()) + "/" + String(ESP.getHeapSize())
  //   + " | Max Alloc Heap RAM: " + String(ESP.getMaxAllocHeap())
  //   + " | Max Alloc PSRAM: " + String(ESP.getMaxAllocPsram())
  //   + " | Min Free PSRAM: " + String(ESP.getMinFreePsram())
  //   + " | renderedBytes= " + String(renderedBytes) + "/" + String(totalBytes));
  // gfx->draw16bitRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  int currentX = 0;
  int currentY = 0;
  int xToDraw = 0;
  int yToDraw = 0;
  int range = pDraw->iWidth * pDraw->iHeight;
  // Serial.printf("range=%d\n", range);
  for (int index = 0; index < range; index++) {
    xToDraw = pDraw->x + currentX;
    yToDraw = pDraw->y + currentY;
    // Serial.printf("xToDraw=%d yToDraw=%d\n", xToDraw, yToDraw);
    grid[xToDraw][yToDraw] = pDraw->pPixels[index];
    currentX++;
    if (currentX >= pDraw->iWidth) {
      currentX = 0;
      currentY++;
    }
  }

  // delay(10);
  return 1;
}

void toggleScreen() {
  displayOn = !displayOn;
  expander->digitalWrite(PCA_TFT_BACKLIGHT, displayOn);
}

void toggleScreenOn() {
  displayOn = true;
  expander->digitalWrite(PCA_TFT_BACKLIGHT, true);
}

void toggleScreenOff() {
  displayOn = false;
  expander->digitalWrite(PCA_TFT_BACKLIGHT, false);
}

void nextImage() {
  imageCursor++;
  if (imageCursor > imageCount - 1) {
    imageCursor = 0;
  }

  const Image& currentImage = images[imageCursor];
  loadImage(currentImage.data, currentImage.size);
}

void setup(void) {
  Serial.begin(115200);
  // while (!Serial) delay(100);

#ifdef GFX_EXTRA_PRE_INIT
  GFX_EXTRA_PRE_INIT();
#endif
  Serial.println("hi");
  expander->pinMode(PCA_TFT_BACKLIGHT, OUTPUT);
  expander->digitalWrite(PCA_TFT_BACKLIGHT, HIGH);

  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }

  gfx->fillScreen(BLACK);

  const Image& currentImage = images[imageCursor];

  int imageLeftX, imageTopY;
  if (currentImage.width != SCREEN_WIDTH) {
    imageLeftX = (SCREEN_WIDTH / 2) - (currentImage.width / 2);
  }

  if (currentImage.height != SCREEN_HEIGHT) {
    imageTopY = (SCREEN_HEIGHT / 2) - (currentImage.height / 2);
  }

  Serial.printf("setting imageLeftX=%d imageTopY=%d\n", imageLeftX, imageTopY);
  loadImage(currentImage.data, currentImage.size);
  // Serial.println("loadedImage");
  drawImage(imageLeftX, imageTopY);
  // Serial.println("drewImage");

  // https://learn.adafruit.com/adafruit-qualia-esp32-s3-for-rgb666-displays/pinouts
  // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/twai.html#examples
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_6, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  // Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.printf("Driver installed\n");
    // Start TWAI driver
    if (twai_start() == ESP_OK) {
      Serial.printf("Driver started\n");
    } else {
      Serial.printf("Failed to start driver\n");
    }
  } else {
    Serial.printf("Failed to install driver\n");
  }
}


void loop() {
  // Serial.print("Free memory: ");
  // Serial.println(ESP.getFreeHeap());
  // Serial.print("Free PSRAM: ");
  // Serial.println(ESP.getFreePsram());
  // delay(500);

  if (!expander->digitalRead(PCA_BUTTON_DOWN)) {
    toggleScreen();
    delay(500);
  }

  if (!expander->digitalRead(PCA_BUTTON_UP)) {
    nextImage();
    delay(500);
  }

  twai_message_t message;
  if (!twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
    return;
  }

  if (!(message.rtr)) {
    // for (int i = 0; i < message.data_length_code; i++) {
    //   Serial.printf("Data byte %d = %d\n", i, message.data[i]);
    // }
    // Serial.printf("message.identifier=%04X\n", message.identifier);
    if (message.identifier == SCREEN_ON_PACKET_ID) {
      toggleScreenOn();
    } else if (message.identifier == SCREEN_OFF_PACKET_ID) {
      toggleScreenOff();
    } else if (message.identifier == MOVE_IMAGE_PACKET_ID) {
      // if (images[imageCursor].width == SCREEN_WIDTH) {
      //   return;
      // }
      // [0, 100]
      uint percentOffset = message.data[0];

      if (percentOffset == lastMessageValue) {
        return;
      } else {
        lastMessageValue = percentOffset;
      }

      // screen origin is 0, 0 at top left
      int imageCenterX = (SCREEN_WIDTH * percentOffset) / 100;
      int imageCenterY = SCREEN_HEIGHT / 2;

      int imageLeftX = imageCenterX - (images[imageCursor].width / 2);
      int imageTopY = imageCenterY - (images[imageCursor].height / 2);

      imageLeftX = max(imageLeftX, 0);
      imageLeftX = min(imageLeftX, SCREEN_WIDTH - images[imageCursor].width);

      currentMillis = millis();
      if (currentMillis - lastMovedImageDrawnTimestamp > messageCooldown) {
        // Serial.printf("imageCenterX=%d, imageLeftX=%d \n", imageCenterX, imageLeftX);
        loadImage(images[imageCursor].data, images[imageCursor].size);
        drawImage(imageLeftX, imageTopY);
      }

    } else if (message.identifier == SET_IMAGE_PACKET_ID) {
      imageCursor = message.data[0];
      if (imageCursor > imageCount - 1) {
        imageCursor = 0;
      }
    }
  }
}
