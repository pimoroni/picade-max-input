/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"
#include "pico/unique_id.h"
#include "custom_gamepad.h"

#ifndef USB_DEVICE_VERSION
// 1.0, Format 0xXXYZ (YY = major, Y = minor, Z = sub)
#define USB_DEVICE_VERSION (0x0100)
#endif

enum
{
  ITF_GAMEPAD_1,
  ITF_GAMEPAD_2,
  ITF_KEYBOARD,
  ITF_CDC_0,
  ITF_CDC_0_DATA,
  ITF_NUM_TOTAL
};

#define EPNUM_HID1   0x83
#define EPNUM_HID2   0x84
#define EPNUM_HID3   0x85

#define EPNUM_CDC_0_NOTIF   0x81
#define EPNUM_CDC_0_OUT     0x02
#define EPNUM_CDC_0_IN      0x82

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+

// Storage for 8-byte unique ID, needs 16 + 1 bytes for hex representation + '\0'.
char usb_serial[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];

tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0x2e8a,
    .idProduct          = 0x1098,
    .bcdDevice          = USB_DEVICE_VERSION,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

void usb_serial_init(void) {
  pico_get_unique_board_id_string(usb_serial, sizeof(usb_serial));
}

//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+

uint8_t const desc_hid_report_gamepad1[] =
{
  PICADE_HID_GAMEPAD()
};

uint8_t const desc_hid_report_gamepad2[] =
{
  PICADE_HID_GAMEPAD()
};

uint8_t const desc_hid_report_keyboard[] =
{
  TUD_HID_REPORT_DESC_KEYBOARD()
};

// Invoked when received GET HID REPORT DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf)
{
  if (itf == ITF_GAMEPAD_1)
  {
    return desc_hid_report_gamepad1;
  }
  else if (itf == ITF_GAMEPAD_2)
  {
    return desc_hid_report_gamepad2;
  }
  else if (itf == ITF_KEYBOARD)
  {
    return desc_hid_report_keyboard;
  }

  return NULL;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

#define  CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN + TUD_CDC_DESC_LEN)

uint8_t const desc_configuration[] =
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

  // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
  TUD_HID_DESCRIPTOR(ITF_GAMEPAD_1, 4, HID_ITF_PROTOCOL_NONE,     sizeof(desc_hid_report_gamepad1), EPNUM_HID1, CFG_TUD_HID_EP_BUFSIZE, 1),
  TUD_HID_DESCRIPTOR(ITF_GAMEPAD_2, 5, HID_ITF_PROTOCOL_NONE,     sizeof(desc_hid_report_gamepad2), EPNUM_HID2, CFG_TUD_HID_EP_BUFSIZE, 1),
  TUD_HID_DESCRIPTOR(ITF_KEYBOARD,  6, HID_ITF_PROTOCOL_KEYBOARD, sizeof(desc_hid_report_keyboard), EPNUM_HID3, CFG_TUD_HID_EP_BUFSIZE, 1),
  TUD_CDC_DESCRIPTOR(ITF_CDC_0,     7, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// array of pointer to string descriptors
char const* string_desc_arr [] =
{
  (const char[]) { 0x09, 0x04 },  // 0: is supported language is English (0x0409)
  "Pimoroni",                     // 1: Manufacturer
  "Picade Max",                   // 2: Product
  usb_serial,                     // 3: Serials, should use chip ID
  "GamePad 1",
  "GamePad 2",
  "Keyboard",
  "Plasma",
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = (uint8_t) strlen(str);
    if ( chr_count > 31 ) chr_count = 31;

    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8 ) | (2*chr_count + 2));

  return _desc_str;
}
