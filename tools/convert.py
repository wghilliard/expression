import sys
from io import BytesIO
from PIL import Image
import gzip

def rgb_to_rgb565(r, g, b):
    """Convert an RGB color to RGB565 format."""
    r = r >> 3      # 5 bits for red (0-31)
    g = g >> 2      # 6 bits for green (0-63)
    b = b >> 3      # 5 bits for blue (0-31)
    return (r << 11) | (g << 5) | b  # Combine into a 16-bit value

def convert_image_to_rgb565(image_path, width, height, should_compress):
    """Convert an image to a list of RGB565 values."""
    # Open the image and convert to RGB
    img = Image.open(image_path).convert('RGB')
    img = img.resize((width, height))  # Resize to match the desired dimensions

    image_bytes = []

    # Convert each pixel to RGB565 and add it to the list
    for y in range(height):
        for x in range(width):
            r, g, b = img.getpixel((x, y))
            rgb565 = rgb_to_rgb565(r, g, b)
            # print(rgb565)
            image_bytes.append(rgb565)
    if not should_compress:
        return image_bytes

    buffer = BytesIO()
    img.save(buffer, format="JPEG")
    return buffer.getvalue()

def save_as_cpp_array(image_bytes, name, filename):
    """Save the image bytes as a C++ array."""
    with open(filename, 'w') as f:
        f.write('const uint8_t ' + name + '_bytes[] PROGMEM = {\n')
        for i, byte in enumerate(image_bytes):
            if i % 8 == 0:
                f.write('\n')
            f.write(f'0x{byte:02X},')
        f.write('\n};\n')

def save_as_jpg(image_bytes, filename):
    """Save the image bytes as a JPG image."""
    with open(filename, 'wb') as f:
        f.write(image_bytes)

# Parameters
# args = argparse.ArgumentParser().parse_args()
image_path = sys.argv[1]  # Replace with the path to your PNG image
print(image_path)
name = image_path.split('.')[0]
width = int(sys.argv[2])  # Desired width of the image (in pixels)
height = int(sys.argv[3])  # Desired height of the image (in pixels)
should_compress = True

# Convert image to RGB565 and get the byte array
g_image_bytes_compressed = convert_image_to_rgb565(image_path, width, height, should_compress)
g_image_bytes = convert_image_to_rgb565(image_path, width, height, False)

# Save the array as a C++ file
save_as_cpp_array(g_image_bytes_compressed, name,  f'{name}_bytes_as_jpeg.cpp')
# save_as_cpp_array(g_image_bytes, name, 'image_bytes.cpp')
save_as_jpg(g_image_bytes_compressed, f'{name}.jpg')

print(f"Image converted and saved as '{name}_bytes_as_jpeg.cpp'.")
print(f"Image converted and saved as '{name}.jpeg'.")
