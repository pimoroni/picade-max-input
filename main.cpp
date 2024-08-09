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

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <string_view>
#include <ctype.h>  // isupper / islower

#include "bsp/board_api.h"
#include "tusb.h"
#include "custom_gamepad.h"

#include "picade.hpp"
#include "plasma.hpp"
#include "rgbled.hpp"

#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/bootrom.h"
#include "hardware/structs/rosc.h"
#include "hardware/watchdog.h"
#include "pico/timeout_helper.h"

pimoroni::RGBLED led(17, 18, 19);

const size_t MAX_UART_PACKET = 64;

const size_t COMMAND_LEN = 4;
uint8_t command_buffer[COMMAND_LEN];
std::string_view command((const char *)command_buffer, COMMAND_LEN);


extern "C" {
void usb_serial_init(void);
}

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

// Interface index depends on the order in configuration descriptor
enum {
  ITF_GAMEPAD_1,
  ITF_GAMEPAD_2,
  ITF_KEYBOARD,
  ITF_SERIAL,
  ITF_SERIAL_DATA,
};

void hid_task(void);
uint cdc_task(uint8_t *buf, size_t buf_len);

uint cdc_task(uint8_t *buf, size_t buf_len) {

    if (tud_cdc_connected()) {
        if (tud_cdc_available()) {
            return tud_cdc_read(buf, buf_len);
        }
    }

    return 0;
}

bool cdc_wait_for(std::string_view data, uint timeout_ms=50) {
    timeout_state ts;
    absolute_time_t until = delayed_by_ms(get_absolute_time(), timeout_ms);
    check_timeout_fn check_timeout = init_single_timeout_until(&ts, until);

    for(auto expected_char : data) {
        char got_char;
        while(1){
            tud_task();
            if (cdc_task((uint8_t *)&got_char, 1) == 1) break;
            if(check_timeout(&ts, false)) return false;
        }
        if (got_char != expected_char) return false;
    }
    return true;
}

size_t cdc_get_bytes(const uint8_t *buffer, const size_t len, const uint timeout_ms=1000) {
    memset((void *)buffer, len, 0);

    uint8_t *p = (uint8_t *)buffer;

    timeout_state ts;
    absolute_time_t until = delayed_by_ms(get_absolute_time(), timeout_ms);
    check_timeout_fn check_timeout = init_single_timeout_until(&ts, until);

    size_t bytes_remaining = len;
    while (bytes_remaining && !check_timeout(&ts, false)) {
        tud_task(); // tinyusb device task
        size_t bytes_read = cdc_task(p, std::min(bytes_remaining, MAX_UART_PACKET));
        bytes_remaining -= bytes_read;
        p += bytes_read;
    }
    return len - bytes_remaining;
}

/*------------- MAIN -------------*/
int main(void)
{
  // Apply a modest overvolt, default is 1.10v.
  // this is required for a stable 250MHz on some RP2040s
  //vreg_set_voltage(VREG_VOLTAGE_1_20);
  //sleep_ms(10);
  //set_sys_clock_khz(250000, true);

  led.set_rgb(255, 0, 0);
  board_init();
  // Fetch the Pico serial (actually the flash chip ID) into `usb_serial`
  usb_serial_init();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  led.set_rgb(0, 0, 255);

  picade_init();
  plasma_init();

  led.set_rgb(0, 255, 0);

  while (1)
  {
    tud_task();
    hid_task();
    //cdc_task();

    if (tud_cdc_connected()) {
      if (tud_cdc_available()) {
        if(!cdc_wait_for("multiverse:")) {
            continue; // Couldn't get 16 bytes of command
        }

        if(cdc_get_bytes(command_buffer, COMMAND_LEN) != COMMAND_LEN) {
            //display::info("cto");
            continue;
        }

        if(command == "data") {
            if (cdc_get_bytes(led_front_buffer, sizeof(led_front_buffer)) == sizeof(led_front_buffer)) {
              plasma_flip();
            }
            continue;
        }

        if(command == "_rst") {
            sleep_ms(500);
            save_and_disable_interrupts();
            rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB;
            watchdog_reboot(0, 0, 0);
            continue;
        }

        if(command == "_usb") {
            sleep_ms(500);
            save_and_disable_interrupts();
            rosc_hw->ctrl = ROSC_CTRL_ENABLE_VALUE_ENABLE << ROSC_CTRL_ENABLE_LSB;
            reset_usb_boot(0, 0);
            continue;
        }
      }
    }
  }

  return 0;
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

void hid_task(void)
{
  // Poll every 10ms
  const uint32_t interval_ms = 1;
  static uint32_t start_ms = 0;
  static bool state = false;

  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  input_t in = picade_get_input();

  if(in.changed) {
    state = !state;
    led.set_rgb(255 * state, 0, 0);
  }

  // Remote wakeup
  if ( tud_suspended() && (in.changed) )
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }

  /*------------- Keyboard -------------*/
  if ( tud_hid_n_ready(ITF_KEYBOARD) )
  {
    /*
    // use to avoid send multiple consecutive zero report for keyboard
    static uint8_t last_util = 0;

    if ( in.changed && in.util != last_util )
    {
      uint8_t keycode[6] = {
        (uint8_t)((in.util & (UTIL_P1_HOTKEY | UTIL_P2_HOTKEY)) ? HID_KEY_ESCAPE : 0u),
      };
  
      tud_hid_n_keyboard_report(ITF_KEYBOARD, 0, 0, keycode);

      last_util = in.util;
    }
    */
  }

  if ( tud_hid_n_ready(ITF_GAMEPAD_1) )
  {
    //tud_hid_n_gamepad_report(ITF_GAMEPAD_1, 0, in.p1_x, in.p1_y, 0, 0, 0, 0, 0, in.p1 & BUTTON_MASK);
    uint16_t hotkey = (in.util & UTIL_P1_HOTKEY) ? (1 << 12) : 0;
    picade_gamepad_report(ITF_GAMEPAD_1, in.p1_x, in.p1_y, (in.p1 & BUTTON_MASK) | hotkey);
  }

  if ( tud_hid_n_ready(ITF_GAMEPAD_2) )
  {
    //tud_hid_n_gamepad_report(ITF_GAMEPAD_2, 0, in.p2_x, in.p2_y, 0, 0, 0, 0, 0, in.p2 & BUTTON_MASK);
    uint16_t hotkey = (in.util & UTIL_P2_HOTKEY) ? (1 << 12) : 0;
    picade_gamepad_report(ITF_GAMEPAD_2, in.p2_x, in.p2_y, (in.p2 & BUTTON_MASK) | hotkey);
  }
}


// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  // TODO set LED based on CAPLOCK, NUMLOCK etc...
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+

/*
void cdc_task(void)
{
  const uint32_t interval_ms = 500;
  static uint32_t start_ms = 0;
  static uint32_t ptr = 0;
  //uint8_t buf[64] = {0};

  if( tud_cdc_available() ) {
    //ptr = tud_cdc_read(led_buffer + ptr, sizeof(led_buffer));
    //if(ptr >= sizeof(led_buffer)){
    //  ptr = 0;
    //}
  }

  if(tud_cdc_connected()) {
#ifdef INPUT_DEBUG
    if ( board_millis() - start_ms < interval_ms) return; // not enough time
    start_ms += interval_ms;
    tud_cdc_write_str("input: ");
    for (auto i = 0u; i < 8; i++) {
      uint8_t in = input_debug[i];
      for (auto j = 0u; j < 8; j++) {
        if (in & (0b10000000 >>  j)) {
          tud_cdc_write_str("1");
        } else {
          tud_cdc_write_str("0");
        }
      }
      tud_cdc_write_str(" ");
    }
    tud_cdc_write_str("\r\n");
    tud_cdc_write_flush();
#endif
  }
}
*/
