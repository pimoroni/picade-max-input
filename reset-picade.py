import multiverse
import glob

picades = glob.glob("/dev/serial/by-id/usb-Pimoroni_Picade_Max_*")

d = multiverse.Display(picades[0], 32, 32, 0, 0)
d.setup()
d.bootloader()
