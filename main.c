/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org), 2025 Mitsumine Suzu (@verylowfreq)
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
// #include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"


#include <ch32v20x_flash.h>

#include <core_riscv.h>

// #define USE_PRINTS

#ifdef USE_PRINTS
#define _PUTS(s) puts(s)
#define _PUTCHAR(c) putchar(c)
#else
#define _PUTS(s) (void)0
#define _PUTCHAR(c) (void)0
#endif

/*
[command], [param 1 LSB], [param 1 MSB], [param 2 LSB], [param 2 MSB], [data length], [ data... (max 58bytes) ]

[response], [data length], [ data... (max 58bytes) ]

*/

typedef struct __attribute__((__packed__)) {
  uint8_t command;
  uint32_t param1;
  uint32_t param2;
  uint8_t data[55];
} CommandPacket_t;

typedef enum {
  CMD_NOP,
  CMD_IDENT,
  CMD_ERASE,
  CMD_PROGRAM_START,
  CMD_PROGRAM_APPEND,
  CMD_FLUSH,
  CMD_READ,
  CMD_RESET,
  CMD_CRC
} Command_t;

typedef enum {
  RESP_INVALID,
  RESP_OK,
  RESP_NG
} Response_t;

uint16_t crc16_ccitt(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;        // 初期値
    const uint16_t poly = 0x1021; // 多項式

    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ poly;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}


uint32_t program_address;
uint32_t program_length;
uint8_t program_buffer[4096];


#define BOOT_MAGIC_APP_IMMEDIATELY      0x4170
#define BOOT_MAGIC_BOOTLOADER           0x624c


static void jump_to_addr(uint32_t addr) {
  __asm__ volatile(
    "jalr zero, %0, 0\n"
    :: "r"(addr)
  );
}

void put_hex(uint8_t val) {
#ifdef USE_PRINTS
  const char* chs = "0123456789abcdef";
  _PUTCHAR(chs[val >> 4]);
  _PUTCHAR(chs[val & 0x0f]);
#else
  (void)val;
#endif
}

#ifdef USE_PRINTS
void dump_rom(uint32_t addr, uint32_t size) {
    _PUTS("Memory dump:");
    const uint8_t* ptr = (const uint8_t*)addr;
    for (uint32_t i = 0; i < size; i++) {
      if (i % 16 == 0) {
        if (i != 0) {
          _PUTS("");
        }
        uint32_t _addr = addr + i;
        put_hex(_addr >> 24);
        put_hex(_addr >> 16);
        put_hex(_addr >> 8);
        put_hex(_addr & 0xff);
        _PUTCHAR(':');
        _PUTCHAR(' ');
      }
      put_hex(ptr[i]);
      _PUTCHAR(' ');
    }
    _PUTS("");
}
#else
void dump_rom(uint32_t addr, uint32_t size) {
  // NOP
}
#endif

bool enter_application(void) {

  const uint32_t* ptr = (const uint32_t*)0x00004000;
  if (*ptr == 0xe339e339UL) {
    // It seems that the flash is not programed.
    // _PUTS("Not programmed.");
    return false;
  }

    BKP_WriteBackupRegister(BKP_DR10, BOOT_MAGIC_APP_IMMEDIATELY);
  NVIC_SystemReset();

  while (1) {}

  {
    const uint32_t* ptr = (const uint32_t*)0x00004000;

    if (*ptr == 0xe339e339UL) {
      // It seems that the flash is not programed.
      // _PUTS("Not programmed.");
      return false;
    }
  }

  /* HSI ON */
  RCC->CTLR |= RCC_HSION;
  while ((RCC->CTLR & RCC_HSIRDY) == 0);

  /* SYSCLK = HSI */
  RCC->CFGR0 &= ~RCC_SW;
  while ((RCC->CFGR0 & RCC_SWS) != RCC_SWS_HSI);

  /* PLL OFF */
  RCC->CTLR &= ~RCC_PLLON;
  while (RCC->CTLR & RCC_PLLRDY);

  /* HSE OFF */
  RCC->CTLR &= ~RCC_HSEON;

  /* 分周を既定値に */
  RCC->CFGR0 &= ~(RCC_HPRE | RCC_PPRE1 | RCC_PPRE2);


  BKP_WriteBackupRegister(BKP_DR10, 0x0000);

  // _PUTS("Disable peripherals");
  PWR_BackupAccessCmd(DISABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, DISABLE);
  NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
  NVIC_DisableIRQ(USB_HP_CAN1_TX_IRQn);
  NVIC_DisableIRQ(USBFS_IRQn);
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, DISABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USB, DISABLE);

  SysTick->CTLR = 0x00;
  SysTick->SR = 0;
  SysTick->CMP = 0;
  SysTick->CNT = 0;


  // _PUTS("Disable IRQ");
  __disable_irq();

  // WORKAROUND: Crash on accessing mstatus
  // uint32_t mstatus = __get_MSTATUS();
  // mstatus &= ~0x08;
  // __set_MSTATUS(mstatus);

  PFIC->IENR[0] = 0;
  PFIC->IENR[1] = 0;
  PFIC->IENR[2] = 0;
  PFIC->IENR[3] = 0;
  PFIC->IPRR[0] = 0xffffffff;
  PFIC->IPRR[1] = 0xffffffff;
  PFIC->IPRR[2] = 0xffffffff;
  PFIC->IPRR[3] = 0xffffffff;
  PFIC->IRER[0] = 0xffffffff;
  PFIC->IRER[1] = 0xffffffff;
  PFIC->IRER[2] = 0xffffffff;
  PFIC->IRER[3] = 0xffffffff;

  GPIO_WriteBit(GPIOA, GPIO_Pin_5, Bit_RESET);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, DISABLE);

  jump_to_addr(0x00004000);

  while (1) { }
}

void led_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  if (!(RCC->RSTSCKR & RCC_PINRSTF)) {
    enter_application();
  }

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
  PWR_BackupAccessCmd(ENABLE);
  uint16_t magic = BKP_ReadBackupRegister(BKP_DR10);
  if (magic == BOOT_MAGIC_APP_IMMEDIATELY) {
    _PUTS("BOOT_MAGIC_APP");
    enter_application();
  }

  board_init();

  _PUTS("Start bootloader");

  {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef init = {
      .GPIO_Pin = GPIO_Pin_5,
      .GPIO_Mode = GPIO_Mode_Out_PP,
      .GPIO_Speed = GPIO_Speed_50MHz
    };
    GPIO_Init(GPIOA, &init);
  }
  GPIO_WriteBit(GPIOA, GPIO_Pin_5, Bit_SET);


  dump_rom(0x00004000, 1024);


  if (magic == BOOT_MAGIC_APP_IMMEDIATELY) {
    _PUTS("BOOT_MAGIC_APP");
    enter_application();

  } else if (magic == BOOT_MAGIC_BOOTLOADER) {
    // Continue bootloader
    _PUTS("BOOT_MAGIC_BOOTLOADER");

  } else {
    // First reset
    _PUTS("Write a magic for bootloader");

    BKP_WriteBackupRegister(BKP_DR10, BOOT_MAGIC_BOOTLOADER);
    _PUTS("Success.");
    SysTick->CTLR = 1 << 2;  // Clock source is HCLK.
    SysTick->CNT = 0;
    SysTick->CMP = SystemCoreClock / 1000 * 500;
    SysTick->SR = 0;
    SysTick->CTLR |= 0x01;
    _PUTS("Wait...");
    while (!(SysTick->SR & 0x01)) { }

    // If we reached here, it's timed out.
    _PUTS("Timeout. Enter application");
    enter_application();
  }

  // Clear magic code for next reset
  BKP_WriteBackupRegister(BKP_DR10, 0x0000);

  _PUTS("Continue bootlaoder");

  PWR_BackupAccessCmd(DISABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, DISABLE);

  // For Flash operation, EnhanceRead mode should be disabled.
  FLASH_Enhance_Mode(DISABLE);

  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {
    .role = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_AUTO
  };
  tusb_init(BOARD_TUD_RHPORT, &dev_init);

  board_init_after_tusb();

  while (1)
  {
    led_task();

    tud_task(); // tinyusb device task
  }
}

void led_task(void) {
  static uint32_t timer = 0;
  static bool led_on = false;
  const uint32_t interval = 500;
  uint32_t now = board_millis();
  if (now - timer >= interval) {
    timer = now;
    led_on = !led_on;
    GPIO_WriteBit(GPIOA, GPIO_Pin_5, led_on ? Bit_SET : Bit_RESET);
  }
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


static uint8_t response_buffer[64];

/// @brief Execute incoming command
/// @param buffer 64-bytes buffer
/// @return pointer to 64-bytes buffer (Need to be sent) or NULL (Nothing to send)
const uint8_t* execute_command(const uint8_t* buffer) {

  CommandPacket_t const* packet = (CommandPacket_t const*)buffer;
  memset(response_buffer, 0x00, sizeof(response_buffer));

  if (packet->command == CMD_NOP) {
    response_buffer[0] = RESP_OK;
    return response_buffer;

  } else if (packet->command == CMD_IDENT) {
    response_buffer[0] = RESP_OK;
    const char* text = "WCH CH32V203C8T6";
    response_buffer[1] = strlen(text);
    memcpy(&response_buffer[2], text, strlen(text));
    return response_buffer;

  } else if (packet->command == CMD_ERASE) {
    const uint32_t PAGE_SIZE = 256;
    uint32_t const address = packet->param1;
    uint32_t const size = packet->param2;

    if (address % PAGE_SIZE == 0 && size % PAGE_SIZE == 0) {
      uint32_t const page_count = size / PAGE_SIZE;

      FLASH_Unlock_Fast();
      FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP |FLASH_FLAG_WRPRTERR);
      for (uint32_t i = 0; i < page_count; i++) {
        uint32_t const start_address = address + i * PAGE_SIZE;
        FLASH_ErasePage_Fast(start_address);
      }
      FLASH_Lock_Fast();
    }

    response_buffer[0] = RESP_OK;
    return response_buffer;

  } else if (packet->command == CMD_PROGRAM_START || packet->command == CMD_PROGRAM_APPEND) {
    const uint32_t address = packet->param1;
    const uint32_t datalen = packet->param2;

    if (packet->command == CMD_PROGRAM_START) {
      program_address = address;
      program_length = 0;
      memset(program_buffer, 0xff, sizeof(program_buffer));
    }

    const uint32_t offset = address - program_address;

    if (offset + datalen <= sizeof(program_buffer)) {
      memcpy(&program_buffer[offset], packet->data, datalen);
    }

    program_length = offset + datalen;

    return NULL;

  } else if (packet->command == CMD_FLUSH) {
    FLASH_Unlock_Fast();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP |FLASH_FLAG_WRPRTERR);

    uint32_t addr = program_address;
    uint32_t* data = (uint32_t*)(void*)program_buffer;
    for (uint32_t i = 0; i < program_length;) {
      FLASH_ProgramPage_Fast(addr + i, (uint32_t*)data);
      data += 256 / 4;
      i += 256;
    }
    FLASH_Lock_Fast();

    program_address = 0;
    program_length = 0;

    response_buffer[0] = RESP_OK;
    return response_buffer;

  } else if (packet->command == CMD_READ) {
    uint32_t address = packet->param1;
    uint32_t size = packet->param2;
    if (size > 62) { size = 62; }
    response_buffer[0] = RESP_OK;
    response_buffer[1] = size;
    for (uint32_t i = 0; i < size; i++) {
      uint32_t addr = address + i;
      response_buffer[2 + i] = *((uint8_t*)addr);
    }

    return response_buffer;

  } else if (packet->command == CMD_RESET) {
    NVIC_SystemReset();
    while (1) { }

  } else if (packet->command == CMD_CRC) {
    uint32_t const address = packet->param1;
    uint32_t const length = packet->param2;
    uint16_t const crc16val = crc16_ccitt((uint8_t const*)address, length);

    response_buffer[0] = RESP_OK;
    response_buffer[1] = 2;
    response_buffer[2] = crc16val;
    response_buffer[3] = crc16val >> 8;
    return response_buffer;

  } else {
    response_buffer[0] = RESP_NG;
    return response_buffer;
  }
}

//--------------------------------------------------------------------+
// USB Vendor class
//--------------------------------------------------------------------+

extern uint8_t const desc_ms_os_20[];

enum
{
  VENDOR_REQUEST_WEBUSB = 1,
  VENDOR_REQUEST_MICROSOFT = 2
};

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) {
    return true;
  }

  switch (request->bmRequestType_bit.type) {
    case TUSB_REQ_TYPE_VENDOR:
      switch (request->bRequest) {
        case VENDOR_REQUEST_MICROSOFT:
          if (request->wIndex == 7) {
            // Get Microsoft OS 2.0 compatible descriptor
            uint16_t total_len;
            memcpy(&total_len, desc_ms_os_20 + 8, 2);

            return tud_control_xfer(rhport, request, (void*)(uintptr_t)desc_ms_os_20, total_len);
          } else {
            return false;
          }

        default: break;
      }
      break;

    default: break;
  }

  // stall unknown request
  return false;
}


void tud_vendor_rx_cb(uint8_t idx, const uint8_t *buffer, uint32_t bufsize) {
  (void)idx;
  (void)buffer;
  (void)bufsize;

  while (tud_vendor_available() < 64) {
    tud_task();
  }

  uint8_t buf[64];
  tud_vendor_read(buf, 64);

  const uint8_t* response_data = execute_command(buf);
  if (response_data != NULL) {
    tud_vendor_write(response_data, 64);
    tud_vendor_flush();
  }
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
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
  // This example doesn't use multiple report and report ID
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;

  const uint8_t* response_data = execute_command(buffer);
  if (response_data != NULL) {
    tud_hid_report(0x00, response_data, 64);
  }
}
