// NeXT non-ADB Keyboard to USB converter
// This will take an older NeXT keyboard, talk to it, and turn the keycodes into a USB keyboard
// Requires an Arduino Micro for the USB portion - but could be ported to another micro fairly easily
// Written by Limor Fried / Adafruit Industries
// Released under BSD license - thanks NetBSD! :)
//
// Timing reference thanks to http://m0115.web.fc2.com/
// Pinouts thanks to http://www.68k.org/~degs/nextkeyboard.html
// Keycodes from http://ftp.netbsd.org/pub/NetBSD/NetBSD-release-6/src/sys/arch/next68k/dev/

#include "wsksymdef.h"
#include "nextkeyboard.h"
#include <Keyboard.h>

// the timing per bit, 50microseconds
#define TIMING 50

// pick which pins you want to use
#define KEYBOARDOUT 3
#define KEYBOARDIN 2

// comment to speed things up, uncomment for help!
//#define DEBUG

// speed up reads by caching the 'raw' pin ports
volatile uint8_t *misoportreg;
uint8_t misopin;
// our little macro
#define readkbd() ((*misoportreg) & misopin)

// debugging/activity LED
#define LED LED_BUILTIN

#define NEXT_KMBUS_IDLE 0x200600
// XXX found this code was idle instead
#define NEXT_KMBUS_IDLE_2 0x200C00

// NeXT Keyboard Defines
// modifiers
#define NEXT_KB_CONTROL 0x2000
#define NEXT_KB_ALTERNATE_LEFT 0x40000
#define NEXT_KB_ALTERNATE_RIGHT 0x80000
#define NEXT_KB_COMMAND_LEFT 0x10000
#define NEXT_KB_COMMAND_RIGHT 0x20000
#define NEXT_KB_SHIFT_LEFT 0x4000
#define NEXT_KB_SHIFT_RIGHT 0x8000

// special command for setting LEDs
void setLEDs(bool leftLED, bool rightLED) {
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING *9);
  digitalWrite(KEYBOARDOUT, HIGH);
  delayMicroseconds(TIMING *3);
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING);

  if (leftLED)
      digitalWrite(KEYBOARDOUT, HIGH);
  else
      digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING);

  if (rightLED)
      digitalWrite(KEYBOARDOUT, HIGH);
  else
      digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING);
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING *7);
  digitalWrite(KEYBOARDOUT, HIGH);
}

void flashLEDs() {
  setLEDs(true, true);
  delay(1000);
  setLEDs(false, false);
  delay(500);
  setLEDs(true, true);
  delay(1000);
  setLEDs(false, false);
  delay(500);
}

void query() {
  // query the keyboard for data
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING *5);
  digitalWrite(KEYBOARDOUT, HIGH);
  delayMicroseconds(TIMING );
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING *3);
  digitalWrite(KEYBOARDOUT, HIGH);
}

void nextreset() {
  // reset the keyboard
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING);
  digitalWrite(KEYBOARDOUT, HIGH);
  delayMicroseconds(TIMING*4);
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING);
  digitalWrite(KEYBOARDOUT, HIGH);
  delayMicroseconds(TIMING*6);
  digitalWrite(KEYBOARDOUT, LOW);
  delayMicroseconds(TIMING*10);
  digitalWrite(KEYBOARDOUT, HIGH);
}


void setup() {
  // https://www.element14.com/community/thread/33253/l/arduino-micro-requires-reset-when-powering-on-to-boot-properly
  delay(1000);

  // set up pin directions
  pinMode(KEYBOARDOUT, OUTPUT);
  pinMode(KEYBOARDIN, INPUT);
  pinMode(LED, OUTPUT);

  misoportreg = portInputRegister(digitalPinToPort(KEYBOARDIN));
  misopin = digitalPinToBitMask(KEYBOARDIN);

  // according to http://cfile7.uf.tistory.com/image/14448E464F410BF22380BB
  query();
  delay(5);
  nextreset();
  delay(8);

  query();
  delay(5);
  nextreset();
  delay(8);

  flashLEDs();

  Keyboard.begin();

#ifdef DEBUG
  while (!Serial)
      Serial.begin(57600);
  Serial.println("NeXT");
#endif
}

uint32_t getresponse() {
  // bitbang timing, read 22 bits 50 microseconds apart
  cli();
  while ( readkbd() );
  delayMicroseconds(TIMING/2);
  uint32_t data = 0;
  for (uint8_t i=0; i < 22; i++) {
      if (readkbd())
        data |= ((uint32_t)1 << i);
      delayMicroseconds(TIMING);
  }
  sei();


  return data;
}

void loop() {
  digitalWrite(LED, LOW);
  delay(20);
  uint32_t resp;
  query();
  resp = getresponse();
/*
#ifdef DEBUG
  Serial.print("getresponse(): resp is 0x"); Serial.print(resp, HEX);
  Serial.print("\n");
#endif
*/
  // check for a 'idle' response, we'll do nothing
  if ((resp == NEXT_KMBUS_IDLE) || (resp == NEXT_KMBUS_IDLE_2)) {
/*
#ifdef DEBUG
  Serial.println("keyboard is idle");
#endif
*/
    return;
  }

  // turn on the LED when we get real resposes!
  digitalWrite(LED, HIGH);

  // keycode is the lower 7 bits
  uint8_t keycode = resp & 0xFF;
  keycode /= 2;

#ifdef DEBUG
  Serial.print("getresponse() after keycode: resp is 0x"); Serial.print(resp, HEX);
  Serial.print("\n");
  Serial.print("keycode: ["); Serial.print(keycode); Serial.print("]");
  Serial.print("\n");
#endif

  // modifiers! you can remap these here,
  // but I suggest doing it in the OS instead
  if (resp & NEXT_KB_CONTROL) {
    Keyboard.press(KEY_LEFT_CTRL);
  } else {
    Keyboard.release(KEY_LEFT_CTRL);
  }

  if (resp & NEXT_KB_SHIFT_LEFT) {
    Keyboard.press(KEY_LEFT_SHIFT);
  } else {
    Keyboard.release(KEY_LEFT_SHIFT);
  }
  if (resp & NEXT_KB_SHIFT_RIGHT) {
    Keyboard.press(KEY_RIGHT_SHIFT);
  } else {
    Keyboard.release(KEY_RIGHT_SHIFT);
  }
  boolean shiftPressed = (resp & (NEXT_KB_SHIFT_LEFT|NEXT_KB_SHIFT_RIGHT)) != 0;

  // turn on shift LEDs if shift is held down
  if (shiftPressed)
    setLEDs(true, true);
  else
    setLEDs(false, false);

  if (resp & NEXT_KB_COMMAND_LEFT) {
    Keyboard.press(KEY_LEFT_GUI);
  } else {
    Keyboard.release(KEY_LEFT_GUI);
  }
  if (resp & NEXT_KB_COMMAND_RIGHT) {
    Keyboard.press(KEY_RIGHT_GUI);
  } else {
    Keyboard.release(KEY_RIGHT_GUI);
  }
  if (resp & NEXT_KB_ALTERNATE_LEFT) {
    Keyboard.press(KEY_LEFT_ALT);
  } else {
    Keyboard.release(KEY_LEFT_ALT);
  }
  if (resp & NEXT_KB_ALTERNATE_RIGHT) {
    Keyboard.press(KEY_RIGHT_ALT);
  } else {
    Keyboard.release(KEY_RIGHT_ALT);
  }
  if (keycode == 0) return;

  bool found = false;
  int i = 0 ;
  while (!found) {

          // Prevent potential infinite loops if we get an unexpected key
          // XXX Max number?
          if (i > 1024) {
#ifdef DEBUG
                  Serial.println("Unexpected keypress detected, err 1!");
#endif
                  return;
          }

          // MAGIC - what is this calculation?

          if (nextkbd_keydesc_us[i*3] != keycode) {
                  i ++ ;
                  continue;
          }

          found = true;
  }

  if (!found) {
#ifdef DEBUG
          Serial.println("Unexpected keypress detected, err 2!");
#endif
          return;

  }

  keysym_t keydesc = nextkbd_keydesc_us[i*3+1];
  char ascii = (char) keydesc;

#ifdef DEBUG
  Serial.println("Found for keycode at index*3+1 in nextkbd_keydesc_us");
  Serial.print("This index was: "); Serial.println(i);
  Serial.print("The index in nextkbd_keydesc_us was: "); Serial.println(i*3);
  Serial.print("The matching keycode at nextkbd_keydesc_us was: "); Serial.println(keydesc);
  Serial.print("The +1 keydesc is: HEX 0x "); Serial.println(keydesc, HEX);
  Serial.print("This is ascii char: "); Serial.println(ascii);
#endif

  int code;
  switch (keydesc) {

          case KS_KP_Enter:
          case KS_Return:    code = KEY_RETURN; break;
          case KS_Escape:    code = KEY_ESC; break;
          case KS_BackSpace: code = KEY_BACKSPACE; break;
          case KS_Up:        code = KEY_UP_ARROW; break;
          case KS_Down:      code = KEY_DOWN_ARROW; break;
          case KS_Left:      code = KEY_LEFT_ARROW; break;
          case KS_Right:     code = KEY_RIGHT_ARROW; break;
          case KS_equal:     code = KS_equal; break;

                             // hacks for two tricky numpad keys
          case KS_KP_Equal:  code = (shiftPressed ? KS_bar : ascii); break;
          case KS_KP_Divide:
                             if (shiftPressed) {
                                     Keyboard.release(KEY_RIGHT_SHIFT);
                                     Keyboard.release(KEY_LEFT_SHIFT);

                                     code = KS_backslash;
                             } else {
                                     code = ascii;
                             }
                             break;

                             // remap the other special keys because the KeyboardMouse can't send proper vol/brightness anyway
          case KS_AudioLower:  code = KEY_INSERT; break;
          case KS_AudioRaise:  code = KEY_DELETE; break;
          case KS_Cmd_BrightnessUp:    code = KEY_PAGE_UP; break;
          case KS_Cmd_BrightnessDown:  code = KEY_PAGE_DOWN; break;

          case 0:
          default:
               code = ascii;
#ifdef DEBUG
               Serial.println("Switch default ... setting code to ascii");
               Serial.println(ascii);
               Serial.println(code);
#endif

  }



  // keypress
  if (((resp & 0xF00) == 0x400) || ((resp & 0xF00) == 0x800)) {
#ifdef DEBUG
          Serial.print("PRESS KEYCODE v: ");
          Serial.println(code);
#endif
          Keyboard.press(code);
          return;
  }

  // release
  if (((resp & 0xF00) == 0x500) || ((resp & 0xF00) == 0x900) || ((resp & 0xF00) == 0xA00)){
#ifdef DEBUG
          Serial.print("RELEASE KEYCODE ^: ");
          Serial.println(code);
#endif
          Keyboard.release(code);
          return;
  }

  // re-press shift if need be
  if (keydesc == KS_KP_Divide && shiftPressed) {
          if (resp & NEXT_KB_SHIFT_LEFT)
                  Keyboard.press(KEY_LEFT_SHIFT);
          if (resp & NEXT_KB_SHIFT_RIGHT)
                  Keyboard.press(KEY_RIGHT_SHIFT);
  }

  return;
}
