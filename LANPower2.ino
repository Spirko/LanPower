/*
   LANPower: 4-channel 12 V power controller for a LAN.
    When reset, the devices are turned on one-at-a-time according to
    preset delays.  This allows the first device to start and be ready
    before the second device turns on, etc.

   Hardware config:
    ATTiny85, running at 5 V
     Digital Pin 2: Relay 1 driver (modem)
     Digital Pin 3: Relay 2 driver (WiFi Router)
     Digital Pin 4: Relay 3 driver (Upstairs Switch)
     Digital Pin 5: Relay 4 driver (Upstairs WiFi AP)
     Digital Pin 9:   Button 1 (Reset Modem Only)
     Digital Pin 10:  Button 2 (Reset LAN Only)
     Digital Pin 11:  Button 3 (n/c)
     Digital Pin 12:  Button 4 (n/c)
    Relay board, 5 V, 4-channels with isolators
     Using normally-closed contacts so the relays don't need
     to be always energized.  There is a chance of a small power
     blip before the ATTiny starts the initialization sequence by
     energizing the relays, but that isn't long enough for anything
     to start booting.
*/

#include <stdint.h>
#include <limits.h>
#include "pitches.h"

#define DEBUG_LEVEL -1
#define dbSerial Serial
#include "dprint.h"

#define BUZZER 7

#define Q 0.2

/*********************************************************************************
   Tunes code
*/
typedef struct {
  int N;
  int melody[15];
  float rhythm[15];
} Tune;

Tune tunes[] = {
  { 1, { NOTE_C6 }, {Q} },
  { 2, { NOTE_C6, NOTE_C6 }, { Q, Q } },
  { 0, { }, { } },
  {
    6,
    { NOTE_G4, NOTE_C5, NOTE_E5, NOTE_G5, NOTE_E5, NOTE_G5 },
    { Q / 3, Q / 3, Q / 3, 3 * Q / 4, Q / 4, 2 * Q }
  }
};

// Mario power-up sound.
const int N5 = 15;
const int melody5[] = { NOTE_G4, NOTE_B4, NOTE_D5, NOTE_G5, NOTE_B5,
                        NOTE_GS4, NOTE_C5, NOTE_DS5, NOTE_GS5, NOTE_C6,
                        NOTE_AS4, NOTE_D5, NOTE_F5, NOTE_AS5, NOTE_D6
                      };
const float beats5[] = { Q / 10, Q / 10, Q / 10, Q / 10, Q / 10,
                         Q / 10, Q / 10, Q / 10, Q / 10, Q / 10,
                         Q / 10, Q / 10, Q / 10, Q / 10, Q / 10
                       };


/********************************************************************************
   Alarms declarations
*/
typedef enum action { NONE,
                      MODEM_ON, MODEM_OFF,
                      ROUTER_ON, ROUTER_OFF,
                      SWITCH_ON, SWITCH_OFF,
                      EXT_ON, EXT_OFF,
                      RESET_LAN,
                    } Action;
typedef struct {
  Action what = NONE;
  unsigned long when = 0;
} Alarm;
#define NALARMS 30
Alarm alarms[NALARMS];


#define NRELAY 4
// Which pins are the relays driven from, and what state is on or off?
const uint8_t pins[NRELAY] = { 2, 3, 4, 5 };
const uint8_t on[NRELAY] = { HIGH, HIGH, HIGH, HIGH };
const uint8_t off[NRELAY] = { LOW, LOW, LOW, LOW };

// Buttons are potentially connected to these pins.
// Currently, only button 0 (pin 9) and 1 (pin 10) are used.
const uint8_t buttons[NRELAY] = { 9, 10, 11, 12 };

const unsigned long buttonDelay = 5000;

void setup() {
  dbegin(9600);
  dprintln(0, "LanPower getting ready.");

  pinMode(BUZZER, OUTPUT);
  pinMode(13, OUTPUT);

  for (unsigned int i = 0; i < NRELAY; i++) {
    pinMode(buttons[i], INPUT_PULLUP);
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], on[i]);
  }
  resetModem();

}

void play(Tune t)
{
  int i;
  const int measure = 2000;  // Two beats per second
  for (i = 0; i < t.N; i++) {
    int duration = measure * t.rhythm[i];
    tone(BUZZER, t.melody[i], duration - 50);
    delay(duration);
  }
  noTone(BUZZER);
}

unsigned long getTime() {
  unsigned long tmptime = millis();
  // if (tmptime == ULONG_MAX) return 0; // For when ULONG_MAX is a special time
  return tmptime;
}

void resetModem(void) {
  //  if (alarms[0] != ULONG_MAX) return;

  // unsigned long curtime = getTime();

  dprint(0, millis()); dprintln(0, ": resetModem()");

  // Turn the modem off now and back on in 5 seconds.
  setAlarm(alarms, NALARMS, MODEM_OFF, millis());
  setAlarm(alarms, NALARMS, MODEM_ON, millis() + 5000);
  // setAlarm(alarms, NALARMS, RESET_LAN, millis() + 6000);
  setAlarm(alarms, NALARMS, RESET_LAN, millis() + 180000);
}

void resetLAN(void) {
  // Don't reset if the network is still being reset.
  // unsigned long curtime = getTime();
  dprint(0, millis()); dprintln(0, ": resetLAN()");

  if (checkAlarm(alarms, NALARMS, ROUTER_ON) >= 0 || checkAlarm(alarms, NALARMS, ROUTER_OFF) >= 0) return;


  // Turn the LAN off now.
  // The router gets turned back on in 5 seconds.
  // The switch and WiFi extender get turned back on in a minute.
  setAlarm(alarms, NALARMS, ROUTER_OFF, millis());
  setAlarm(alarms, NALARMS, SWITCH_OFF, millis());
  setAlarm(alarms, NALARMS, EXT_OFF, millis());
  setAlarm(alarms, NALARMS, ROUTER_ON, millis() + 5000);
  setAlarm(alarms, NALARMS, SWITCH_ON, millis() + 60000);
  setAlarm(alarms, NALARMS, EXT_ON, millis() + 60000);
}

void checkAlarms(void) {
  // unsigned long curtime = getTime();

  // Check to see if it's time to do anything.
  while (true) {
    Action a = nextAlarm(alarms, NALARMS);
    if (a != NONE) {
      dprint(0, millis());
      dprint(0, ": Found alarm ");
      dprintln(0, a);
    }
    switch (a) {
      case MODEM_ON:   digitalWrite(pins[0], on[0]);  play(tunes[0]); break;
      case ROUTER_ON:  digitalWrite(pins[1], on[1]);  play(tunes[1]); break;
      case SWITCH_ON:  digitalWrite(pins[2], on[2]);  play(tunes[2]); break;
      case EXT_ON:     digitalWrite(pins[3], on[3]);  play(tunes[3]); break;
      case MODEM_OFF:  digitalWrite(pins[0], off[0]); tone(BUZZER, 500, 200); break;
      case ROUTER_OFF: digitalWrite(pins[1], off[1]); tone(BUZZER, 500, 200); break;
      case SWITCH_OFF: digitalWrite(pins[2], off[2]); tone(BUZZER, 500, 200); break;
      case EXT_OFF:    digitalWrite(pins[3], off[3]); tone(BUZZER, 500, 200); break;
      case RESET_LAN:  resetLAN(); break;
      case NONE:       break;
    }
    if (a == NONE) break;
  }

  // Heartbeat
  digitalWrite(13, millis() % 1000 < 250);
}

void loop() {
  if (getButtonEvent(buttons[1]) == LOW) resetModem();
  if (getButtonEvent(buttons[0]) == LOW) resetLAN();

  checkAlarms();

}

/*****************************************************************************************
   Button Code
*/

/* getButtonEvent(pin) - Edge detects the debounced button push.
    This prevents multiple detections of a single button push.
*/
int getButtonEvent(uint8_t pin) {
  static int prevState[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
  int curState = getButton(pin);

  if (pin > 13) return HIGH;

  if (curState == prevState[pin]) return -1;              // Only return LOW the first time.

  prevState[pin] = curState;     // Remember for next time.
  return curState;          // Return the actual state.

}

/*
   getButton(pin) - returns the digitalRead value of the pin with debouncing.
 *  * Edge detection changes the state of the button.
 *  * Any change within (debounceDelay) is ignored.
   This version is "twitchy".  Any noise will trigger a button state change.
*/
int getButton(uint8_t pin) {
  const unsigned long debounceDelay = 500;
  // static int lockval[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
  static int lastval[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
  static unsigned long lasttime[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  if (pin > 13) return HIGH;

  int curval = digitalRead(pin);
  unsigned long curtime = millis();

  // Decide what to do based on when the last button state change occurred.
  if (lasttime[pin] == 0) {
    // There was never a previous button state.  Save it now.
    lastval[pin] = curval;
    lasttime[pin] = curtime;
  } else if (curtime - lasttime[pin] < debounceDelay) {
    // The last button state change was very recent.  Ignore this reading.
  } else if (curval != lastval[pin]) {
    // Edge detected after the delay.  This is a real button state change.
    lastval[pin] = curval;
    lasttime[pin] = curtime;
  }

  return lastval[pin];
}

/*********************************************************************************
    Alarm code
*/

/* Sets a new alarm.  Returns -1 on failure. */
int setAlarm(Alarm a[], int n, Action what, unsigned long when) {
  dprint(0, when); dprint(0, ": setAlarm "); dprintln(0, what);
  int i;
  for (i = 0; i < n; i++) {
    if (a[i].what == NONE) {
      a[i].what = what;
      a[i].when = when;
      return i;
    }
  }
  return -1; // Ran out of alarms.
}
/* Determines if this type of alarm is currently active. */
int checkAlarm(Alarm a[], int n, Action what) {
  int i;
  for (i = 0; i < n; i++) {
    if (a[i].what == what) {
      return i;
    }
  }
  return -1;
}
/* Does the first alarm that is set and expired */
Action nextAlarm(Alarm a[], int n) {
  int i;
  for (i = 0; i < n; i++) {
    if (a[i].what != NONE && (millis() - a[i].when < ULONG_MAX / 2)) {
      //      dprint(0, "curtime: "); dprintln(0, millis());
      //      dprint(0, "a[i].when: "); dprintln(0, a[i].when);
      //      dprint(0, "diff: "); dprintln(0, millis()-a[i].when);
      Action tmp = a[i].what;
      a[i].what = NONE;
      return tmp;
    }
  }
  return NONE;
}

