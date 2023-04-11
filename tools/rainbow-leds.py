import time
import serial
from colorsys import hsv_to_rgb

port = serial.Serial('/dev/ttyACM0', 115200)

NUM_LEDS = 16 * 4

buf = bytearray(NUM_LEDS * 4)
buf[::4] = [0b11100000 | 31] * NUM_LEDS

t_start = time.time()
count = 0

def set_pixel(x, r, g, b):
    buf[x * 4 + 1] = b
    buf[x * 4 + 2] = g
    buf[x * 4 + 3] = r


while True:
    h = (time.time() / 2.0) % 1.0

    for x in range(NUM_LEDS):
        o = float(x) / NUM_LEDS
        r, g, b = [int(c * 255) for c in hsv_to_rgb(h + o, 1.0, 1.0)]
        set_pixel(x, r, g, b)
    #buf[5:8] = (r, g, b)
    #buf[1::4] = [b] * NUM_LEDS
    #buf[2::4] = [g] * NUM_LEDS
    #buf[3::4] = [r] * NUM_LEDS

    port.write(buf)
    #port.flush()
    count += 1
    #time.sleep(1.0 / 60)
    if time.time() - t_start >= 1.0:
        print(f"FPS: {count} {len(buf)}")
        count = 0
        t_start = time.time()

port.close()