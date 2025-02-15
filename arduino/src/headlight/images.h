#include <cstddef>

#include <eyes.h>
// #include <eyes_movable.h>
#include <nyan.h>
#include <frogs.h>

struct Image {
  const uint8_t* data;
  size_t size;
  int height;
  int width;
};

const Image rightImages[] = {
  // { eyes_movable_bytes, sizeof(eyes_movable_bytes), 220, 220 },
  { eyes_right_bytes, sizeof(eyes_right_bytes), 720, 720 },
  { nyan_right_bytes, sizeof(nyan_right_bytes), 720, 720 },
  { frogs_right_bytes, sizeof(frogs_right_bytes), 720, 720 }
};

const Image eyeImages[] = {
    { eyes_right_bytes, sizeof(eyes_right_bytes), 720, 720 },
    { eyes_looking_left_bytes, sizeof(eyes_looking_left_bytes), 720, 720 },
    { eyes_looking_right_bytes, sizeof(eyes_looking_right_bytes), 720, 720 },
};

const Image* images = rightImages;

const int imageCount = 3;