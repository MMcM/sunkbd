/* 
  Copyright 2015 Mike McMahon

  LUFA Library
  Copyright 2014  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 * Keyboard driver. Initializes hardware and converts scanned input to USB events.
 */

#include "Keyboard.h"

/** Buffer to hold the previously generated Keyboard HID report, for comparison purposes inside the HID class driver. */
static USB_KeyboardReport_Data_t PrevKeyboardReport;

/** LUFA HID Class driver interface configuration and state information. This structure is
 *  passed to all HID Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_HID_Device_t Keyboard_HID_Interface =
{
  .Config =
  {
    .InterfaceNumber        = INTERFACE_ID_Keyboard,
    .ReportINEndpoint       =
    {
      .Address              = KEYBOARD_EPADDR,
      .Size                 = KEYBOARD_EPSIZE,
      .Banks                = 1,
    },
    .PrevReportINBuffer     = &PrevKeyboardReport,
    .PrevReportINBufferSize = sizeof(PrevKeyboardReport),
  },
};

typedef uint8_t HidUsageID;

static uint8_t EE_ClickerEnabled EEMEM = 0;

static HidUsageID KeysDown[16];
static uint8_t NKeysDown;
static uint8_t KeyboardLayout, LayoutDelay;
static bool ExpectReset, ExpectLayout;
static bool ClickerEnabled;

#define LOW 0
#define HIGH 1

// Micro has RX & TX LEDs and user LED as LED3 (D13); some other boards only have LED1.
#ifndef KEYDOWN_LED
#if LEDS_LED3 != 0
#define KEYDOWN_LED LEDS_LED3
#else
#define KEYDOWN_LED LEDS_LED1
#endif
#endif

// Taken from Linux kernel drivers/input/keyboard/sunkbd.c

#define SUNKBD_CMD_RESET        0x1
#define SUNKBD_CMD_BELLON       0x2
#define SUNKBD_CMD_BELLOFF      0x3
#define SUNKBD_CMD_CLICK        0xa
#define SUNKBD_CMD_NOCLICK      0xb
#define SUNKBD_CMD_SETLED       0xe
#define SUNKBD_CMD_LAYOUT       0xf

#define SUNKBD_RET_RESET        0xff
#define SUNKBD_RET_ALLUP        0x7f
#define SUNKBD_RET_LAYOUT       0xfe

#define SUNKBD_LAYOUT_5_MASK    0x20
#define SUNKBD_RELEASE          0x80
#define SUNKBD_KEY              0x7f

/*** Keyboard Map ***/

// Matches Linux kernel driver by correlating sunkbd_keycode and hid_keyboard.

static HidUsageID KeyMap[128] PROGMEM = {
  0,                            // 0x00
  HID_KEYBOARD_SC_STOP,
  HID_KEYBOARD_SC_VOLUME_DOWN,
  HID_KEYBOARD_SC_AGAIN,
  HID_KEYBOARD_SC_VOLUME_UP,
  HID_KEYBOARD_SC_F1,
  HID_KEYBOARD_SC_F2,
  HID_KEYBOARD_SC_F10,
  HID_KEYBOARD_SC_F3,           // 0x08
  HID_KEYBOARD_SC_F11,
  HID_KEYBOARD_SC_F4,
  HID_KEYBOARD_SC_F12,
  HID_KEYBOARD_SC_F5,
  HID_KEYBOARD_SC_RIGHT_ALT,
  HID_KEYBOARD_SC_F6,
  HID_KEYBOARD_SC_F13,          // Unlabeled between Help and F1; KEY_MACRO (112) has no HID usage.
  HID_KEYBOARD_SC_F7,           // 0x10
  HID_KEYBOARD_SC_F8,
  HID_KEYBOARD_SC_F9,
  HID_KEYBOARD_SC_LEFT_ALT,
  HID_KEYBOARD_SC_UP_ARROW,
  HID_KEYBOARD_SC_PAUSE,
  HID_KEYBOARD_SC_PRINT_SCREEN,
  HID_KEYBOARD_SC_SCROLL_LOCK,
  HID_KEYBOARD_SC_LEFT_ARROW,   // 0x18
  HID_KEYBOARD_SC_MENU,
  HID_KEYBOARD_SC_UNDO,
  HID_KEYBOARD_SC_DOWN_ARROW,
  HID_KEYBOARD_SC_RIGHT_ARROW,
  HID_KEYBOARD_SC_ESCAPE,
  HID_KEYBOARD_SC_1_AND_EXCLAMATION,
  HID_KEYBOARD_SC_2_AND_AT,
  HID_KEYBOARD_SC_3_AND_HASHMARK, // 0x20
  HID_KEYBOARD_SC_4_AND_DOLLAR,
  HID_KEYBOARD_SC_5_AND_PERCENTAGE,
  HID_KEYBOARD_SC_6_AND_CARET,
  HID_KEYBOARD_SC_7_AND_AMPERSAND,
  HID_KEYBOARD_SC_8_AND_ASTERISK,
  HID_KEYBOARD_SC_9_AND_OPENING_PARENTHESIS,
  HID_KEYBOARD_SC_0_AND_CLOSING_PARENTHESIS,
  HID_KEYBOARD_SC_MINUS_AND_UNDERSCORE, // 0x28
  HID_KEYBOARD_SC_EQUAL_AND_PLUS,
  HID_KEYBOARD_SC_GRAVE_ACCENT_AND_TILDE,
  HID_KEYBOARD_SC_BACKSPACE,
  HID_KEYBOARD_SC_INSERT,
  HID_KEYBOARD_SC_MUTE,
  HID_KEYBOARD_SC_KEYPAD_SLASH,
  HID_KEYBOARD_SC_KEYPAD_ASTERISK,
  HID_KEYBOARD_SC_POWER,        // 0x30
  HID_KEYBOARD_SC_SELECT,
  HID_KEYBOARD_SC_KEYPAD_DOT_AND_DELETE,
  HID_KEYBOARD_SC_COPY,
  HID_KEYBOARD_SC_HOME,
  HID_KEYBOARD_SC_TAB,
  HID_KEYBOARD_SC_Q,
  HID_KEYBOARD_SC_W,
  HID_KEYBOARD_SC_E,            // 0x38
  HID_KEYBOARD_SC_R,
  HID_KEYBOARD_SC_T,
  HID_KEYBOARD_SC_Y,
  HID_KEYBOARD_SC_U,
  HID_KEYBOARD_SC_I,
  HID_KEYBOARD_SC_O,
  HID_KEYBOARD_SC_P,
  HID_KEYBOARD_SC_OPENING_BRACKET_AND_OPENING_BRACE, // 0x40
  HID_KEYBOARD_SC_CLOSING_BRACKET_AND_CLOSING_BRACE,
  HID_KEYBOARD_SC_DELETE,
  HID_KEYBOARD_SC_APPLICATION,
  HID_KEYBOARD_SC_KEYPAD_7_AND_HOME,
  HID_KEYBOARD_SC_KEYPAD_8_AND_UP_ARROW,
  HID_KEYBOARD_SC_KEYPAD_9_AND_PAGE_UP,
  HID_KEYBOARD_SC_KEYPAD_MINUS,
  HID_KEYBOARD_SC_EXECUTE,      // 0x48
  HID_KEYBOARD_SC_PASTE,
  HID_KEYBOARD_SC_END,
  0,
  HID_KEYBOARD_SC_LEFT_CONTROL,
  HID_KEYBOARD_SC_A,
  HID_KEYBOARD_SC_S,
  HID_KEYBOARD_SC_D,
  HID_KEYBOARD_SC_F,            // 0x50
  HID_KEYBOARD_SC_G,
  HID_KEYBOARD_SC_H,
  HID_KEYBOARD_SC_J,
  HID_KEYBOARD_SC_K,
  HID_KEYBOARD_SC_L,
  HID_KEYBOARD_SC_SEMICOLON_AND_COLON,
  HID_KEYBOARD_SC_APOSTROPHE_AND_QUOTE,
  HID_KEYBOARD_SC_BACKSLASH_AND_PIPE, // 0x58
  HID_KEYBOARD_SC_ENTER,
  HID_KEYBOARD_SC_KEYPAD_ENTER,
  HID_KEYBOARD_SC_KEYPAD_4_AND_LEFT_ARROW,
  HID_KEYBOARD_SC_KEYPAD_5,
  HID_KEYBOARD_SC_KEYPAD_6_AND_RIGHT_ARROW,
  HID_KEYBOARD_SC_KEYPAD_0_AND_INSERT,
  HID_KEYBOARD_SC_FIND,
  HID_KEYBOARD_SC_PAGE_UP,      // 0x60
  HID_KEYBOARD_SC_CUT,
  HID_KEYBOARD_SC_NUM_LOCK,
  HID_KEYBOARD_SC_LEFT_SHIFT,
  HID_KEYBOARD_SC_Z,
  HID_KEYBOARD_SC_X,
  HID_KEYBOARD_SC_C,
  HID_KEYBOARD_SC_V,
  HID_KEYBOARD_SC_B,            // 0x68
  HID_KEYBOARD_SC_N,
  HID_KEYBOARD_SC_M,
  HID_KEYBOARD_SC_COMMA_AND_LESS_THAN_SIGN,
  HID_KEYBOARD_SC_DOT_AND_GREATER_THAN_SIGN,
  HID_KEYBOARD_SC_SLASH_AND_QUESTION_MARK,
  HID_KEYBOARD_SC_RIGHT_SHIFT,
  HID_KEYBOARD_SC_F14,          // Line Feed; KEY_LINEFEED (101) has no HID usage.
  HID_KEYBOARD_SC_KEYPAD_1_AND_END, // 0x70
  HID_KEYBOARD_SC_KEYPAD_2_AND_DOWN_ARROW,
  HID_KEYBOARD_SC_KEYPAD_3_AND_PAGE_DOWN,
  0,
  0,
  0,
  HID_KEYBOARD_SC_HELP,
  HID_KEYBOARD_SC_CAPS_LOCK,
  HID_KEYBOARD_SC_LEFT_GUI,     // 0x78
  HID_KEYBOARD_SC_SPACE,
  HID_KEYBOARD_SC_RIGHT_GUI,
  HID_KEYBOARD_SC_PAGE_DOWN,
  HID_KEYBOARD_SC_NON_US_BACKSLASH_AND_PIPE,
  HID_KEYBOARD_SC_KEYPAD_PLUS,
  0,
  0
};

/*** Keyboard Interface ***/

static void SunKbd_Init(void)
{
  uint8_t ee;

  Serial_Init(1200, false);

  NKeysDown = 0;

  KeyboardLayout = 0xFF;
  LayoutDelay = 100;
  ExpectReset = ExpectLayout = false;

  ee = eeprom_read_byte(&EE_ClickerEnabled);
  if (ee == 0xFF) {
    ee = 0;
    eeprom_write_byte(&EE_ClickerEnabled, ee);
  }
  ClickerEnabled = (bool)ee;
}

static void SunKbd_Task(void)
{
  uint8_t key;
  int16_t in;
  int i;

  in = Serial_ReceiveByte();
  if (in < 0) return;

  key = (uint8_t)in;

  if (ExpectReset) {
    ExpectReset = false;
  }
  else if (ExpectLayout) {
    KeyboardLayout = key;
    ExpectLayout = false;
  }
  else if (key == SUNKBD_RET_ALLUP) {
    NKeysDown = 0;
  }
  else if (key == SUNKBD_RET_RESET) {
    ExpectReset = true;
  }
  else if (key == SUNKBD_RET_LAYOUT) {
    ExpectLayout = true;
  }
  else if (key & SUNKBD_RELEASE) {
    key &= SUNKBD_KEY;
    for (i = 0; i < NKeysDown; i++) {
      if (KeysDown[i] == key) {
        NKeysDown--;
        while (i < NKeysDown) {
          KeysDown[i] = KeysDown[i+1];
          i++;
        }
        break;
      }
    }
  }
  else {
    if (NKeysDown < sizeof(KeysDown)) {
      KeysDown[NKeysDown++] = key;
    }
  }

  if (NKeysDown > 0) {
    LEDs_TurnOnLEDs(KEYDOWN_LED);
  }
  else {
    LEDs_TurnOffLEDs(KEYDOWN_LED);
  }
}

static void UpdateSunLEDs(uint8_t LEDMask)
{
  Serial_SendByte(SUNKBD_CMD_SETLED);
  Serial_SendByte(LEDMask);
}

static void SetClickerEnabled(bool enabled)
{
  Serial_SendByte(enabled ? SUNKBD_CMD_CLICK : SUNKBD_CMD_NOCLICK);
  ClickerEnabled = enabled;
  eeprom_write_byte(&EE_ClickerEnabled, (uint8_t)enabled);
}

#ifndef DEBUG_UNMAPPED
#define DEBUG_UNMAPPED 0
#endif

#if DEBUG_UNMAPPED

static HidUsageID encodeHighForDebug(uint8_t code) {
  switch (code) {
  case 0x00:
    return HID_KEYBOARD_SC_G;
  case 0x10:
    return HID_KEYBOARD_SC_H;
  case 0x20:
    return HID_KEYBOARD_SC_I;
  case 0x30:
    return HID_KEYBOARD_SC_J;
  case 0x40:
    return HID_KEYBOARD_SC_K;
  case 0x50:
    return HID_KEYBOARD_SC_L;
  case 0x60:
    return HID_KEYBOARD_SC_M;
  case 0x70:
    return HID_KEYBOARD_SC_N;
  case 0x80:
    return HID_KEYBOARD_SC_O;
  case 0x90:
    return HID_KEYBOARD_SC_P;
  case 0xA0:
    return HID_KEYBOARD_SC_Q;
  case 0xB0:
    return HID_KEYBOARD_SC_R;
  case 0xC0:
    return HID_KEYBOARD_SC_S;
  case 0xD0:
    return HID_KEYBOARD_SC_T;
  case 0xE0:
    return HID_KEYBOARD_SC_U;
  case 0xF0:
    return HID_KEYBOARD_SC_V;
  default:
    return 0;
  }
}

static HidUsageID encodeLowForDebug(uint8_t code) {
  switch (code) {
  case 0x00:
    return HID_KEYBOARD_SC_0_AND_CLOSING_PARENTHESIS;
  case 0x10:
    return HID_KEYBOARD_SC_1_AND_EXCLAMATION;
  case 0x02:
    return HID_KEYBOARD_SC_2_AND_AT;
  case 0x03:
    return HID_KEYBOARD_SC_3_AND_HASHMARK;
  case 0x04:
    return HID_KEYBOARD_SC_4_AND_DOLLAR;
  case 0x05:
    return HID_KEYBOARD_SC_5_AND_PERCENTAGE;
  case 0x06:
    return HID_KEYBOARD_SC_6_AND_CARET;
  case 0x07:
    return HID_KEYBOARD_SC_7_AND_AMPERSAND;
  case 0x08:
    return HID_KEYBOARD_SC_8_AND_ASTERISK;
  case 0x09:
    return HID_KEYBOARD_SC_9_AND_OPENING_PARENTHESIS;
  case 0x0A:
    return HID_KEYBOARD_SC_A;
  case 0x0B:
    return HID_KEYBOARD_SC_B;
  case 0x0C:
    return HID_KEYBOARD_SC_C;
  case 0x0D:
    return HID_KEYBOARD_SC_D;
  case 0x0E:
    return HID_KEYBOARD_SC_E;
  case 0x0F:
    return HID_KEYBOARD_SC_F;
  default:
    return 0;
  }
}

#endif

static void FillKeyReport(USB_KeyboardReport_Data_t* KeyboardReport)
{
  HidUsageID usage;
  int i, n;
  int shifts;

  shifts = 0;
  n = 0;
  for (i = 0; i < NKeysDown; i++) {
    usage = pgm_read_byte(&KeyMap[KeysDown[i]]);
    if ((KeyboardLayout & SUNKBD_LAYOUT_5_MASK) == 0) {
      // Codes that are reused on Type 5.
      switch (usage) {
      case HID_KEYBOARD_SC_MUTE:
        usage = HID_KEYBOARD_SC_KEYPAD_EQUAL_SIGN;
        break;
      }
    }
    switch (usage) {
    case 0:
#if DEBUG_UNMAPPED
      if (n+3 <= sizeof(KeyboardReport->KeyCode)) {
        KeyboardReport->KeyCode[n++] = HID_KEYBOARD_SC_X;
        KeyboardReport->KeyCode[n++] = encodeHighForDebug(KeysDown[i] & 0xF0);
        KeyboardReport->KeyCode[n++] = encodeLowForDebug(KeysDown[i] & 0x0F);
      }
#endif
      break;
    case HID_KEYBOARD_SC_LEFT_CONTROL:
      shifts |= HID_KEYBOARD_MODIFIER_LEFTCTRL;
      break;
    case HID_KEYBOARD_SC_LEFT_SHIFT:
      shifts |= HID_KEYBOARD_MODIFIER_LEFTSHIFT;
      break;
    case HID_KEYBOARD_SC_LEFT_ALT:
      shifts |= HID_KEYBOARD_MODIFIER_LEFTALT;
      break;
    case HID_KEYBOARD_SC_LEFT_GUI:
      shifts |= HID_KEYBOARD_MODIFIER_LEFTGUI;
      break;
    case HID_KEYBOARD_SC_RIGHT_CONTROL:
      shifts |= HID_KEYBOARD_MODIFIER_RIGHTCTRL;
      break;
    case HID_KEYBOARD_SC_RIGHT_SHIFT:
      shifts |= HID_KEYBOARD_MODIFIER_RIGHTSHIFT;
      break;
    case HID_KEYBOARD_SC_RIGHT_ALT:
      shifts |= HID_KEYBOARD_MODIFIER_RIGHTALT;
      break;
    case HID_KEYBOARD_SC_RIGHT_GUI:
      shifts |= HID_KEYBOARD_MODIFIER_RIGHTGUI;
      break;
    default:
      if (n < sizeof(KeyboardReport->KeyCode)) {
        KeyboardReport->KeyCode[n] = usage;
      }
      n++;
      break;
    }
  }
    
  KeyboardReport->Modifier = shifts;
  
  if (n > sizeof(KeyboardReport->KeyCode)) {
    for (i = 0; i < sizeof(KeyboardReport->KeyCode); i++) {
      KeyboardReport->KeyCode[i] = HID_KEYBOARD_SC_ERROR_ROLLOVER;
    }
  }

}

/*** Device Application ***/

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
  SetupHardware();

  LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
  GlobalInterruptEnable();

  while (true) {
    SunKbd_Task();
    HID_Device_USBTask(&Keyboard_HID_Interface);
    USB_USBTask();
  }
}

/** Configures the board hardware and keyboard pins. */
void SetupHardware(void)
{
#if (ARCH == ARCH_AVR8)
  /* Disable watchdog if enabled by bootloader/fuses */
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  /* Disable clock division */
  clock_prescale_set(clock_div_1);
#elif (ARCH == ARCH_XMEGA)
  /* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
  XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
  XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

  /* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
  XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
  XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

  PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
#endif

  /* Hardware Initialization */
  SunKbd_Init();
  LEDs_Init();
  USB_Init();
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
  LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
  LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
  bool ConfigSuccess = true;

  ConfigSuccess &= HID_Device_ConfigureEndpoints(&Keyboard_HID_Interface);

  USB_Device_EnableSOFEvents();

  LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
  HID_Device_ProcessControlRequest(&Keyboard_HID_Interface);
}

/** Event handler for the USB device Start Of Frame event. */
void EVENT_USB_Device_StartOfFrame(void)
{
  HID_Device_MillisecondElapsed(&Keyboard_HID_Interface);

  if ((KeyboardLayout == 0xFF) &&
      (LayoutDelay > 0)) {
    LayoutDelay--;
    if (LayoutDelay == 0) {
      Serial_SendByte(SUNKBD_CMD_LAYOUT); // Request layout.
      if (ClickerEnabled) {
        Serial_SendByte(SUNKBD_CMD_CLICK);
      }
    }
  }
}

/** HID class driver callback function for the creation of HID reports to the host.
 *
 *  \param[in]     HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in,out] ReportID    Report ID requested by the host if non-zero, otherwise callback should set to the generated report ID
 *  \param[in]     ReportType  Type of the report to create, either HID_REPORT_ITEM_In or HID_REPORT_ITEM_Feature
 *  \param[out]    ReportData  Pointer to a buffer where the created report should be stored
 *  \param[out]    ReportSize  Number of bytes written in the report (or zero if no report is to be sent)
 *
 *  \return Boolean \c true to force the sending of the report, \c false to let the library determine if it needs to be sent
 */
bool CALLBACK_HID_Device_CreateHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                         uint8_t* const ReportID,
                                         const uint8_t ReportType,
                                         void* ReportData,
                                         uint16_t* const ReportSize)
{
  switch (ReportType) {
  case HID_REPORT_ITEM_In:
    {
      USB_KeyboardReport_Data_t* KeyboardReport = (USB_KeyboardReport_Data_t*)ReportData;
      FillKeyReport(KeyboardReport);
      *ReportSize = sizeof(USB_KeyboardReport_Data_t);
    }
    return false;
  case HID_REPORT_ITEM_Feature:
    {
      uint8_t* FeatureReport = (uint8_t*)ReportData;
      FeatureReport[0] = (uint8_t)KeyboardLayout;
      FeatureReport[1] = (uint8_t)ClickerEnabled;
      *ReportSize = 2;
    }
    return true;
  default:
    *ReportSize = 0;
    return false;
  }
}

/** HID class driver callback function for the processing of HID reports from the host.
 *
 *  \param[in] HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in] ReportID    Report ID of the received report from the host
 *  \param[in] ReportType  The type of report that the host has sent, either HID_REPORT_ITEM_Out or HID_REPORT_ITEM_Feature
 *  \param[in] ReportData  Pointer to a buffer where the received report has been stored
 *  \param[in] ReportSize  Size in bytes of the received HID report
 */
void CALLBACK_HID_Device_ProcessHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                          const uint8_t ReportID,
                                          const uint8_t ReportType,
                                          const void* ReportData,
                                          const uint16_t ReportSize)
{
  switch (ReportType) {
  case HID_REPORT_ITEM_Out:
    if (ReportSize > 0) {
      uint8_t* LEDReport = (uint8_t*)ReportData;
      uint8_t  LEDMask   = 0;

      if (*LEDReport & HID_KEYBOARD_LED_NUMLOCK)
        LEDMask |= (1 << 0);
      if (*LEDReport & HID_KEYBOARD_LED_COMPOSE)
        LEDMask |= (1 << 1);
      if (*LEDReport & HID_KEYBOARD_LED_SCROLLLOCK)
        LEDMask |= (1 << 2);
      if (*LEDReport & HID_KEYBOARD_LED_CAPSLOCK)
        LEDMask |= (1 << 3);

      UpdateSunLEDs(LEDMask);
    }
    break;
  case HID_REPORT_ITEM_Feature:
    if (ReportSize > 1) {
      uint8_t* FeatureReport = (uint8_t*)ReportData;
      SetClickerEnabled(FeatureReport[1]);
    }
    break;
  }
}
