/*
  LOSTGUARD tracker firmware (Arduino Uno / Nano + HC-05)

  Wiring (same as the LostGuard wiring diagram):
    HC-05 TXD  -> D10
    HC-05 RXD  <- D11 through the 1k / 2k voltage divider
    Green LED  -> D4 (220 ohm)
    Red LED    -> D5 (220 ohm)
    Buzzer     -> D6
    Button     -> D7 to GND
    HC-05 STATE -> D2 (optional, see USE_STATE_PIN)

  Commands from the LostGuard app (each ends with a newline):
    FIND    start the find-me alarm (buzzer + LEDs)
    STOP    silence the alarm
    STATUS  reply with the tracker state
    TEST    short beep
    PING    heartbeat, sent by the app every 2 seconds
    BYE     the app is disconnecting on purpose, do not alarm

  Behaviour:
    Phone connected     green LED on. Hold the button to test the buzzer.
    Phone lost          red LED on and the buzzer beeps (separation alarm).
                        Press the button to silence it.
    Just powered on     red LED on, silent, until the phone connects once.
*/

#include <SoftwareSerial.h>

// Set to 1 only if the HC-05 STATE pin is wired to D2.
// With 0 the tracker uses the app's heartbeat, which works with the
// four-wire hookup (VCC, GND, TXD, RXD) shown in the wiring diagram.
#define USE_STATE_PIN 0

const byte bluetoothState = 2;
const byte greenLED = 4;
const byte redLED = 5;
const byte buzzer = 6;
const byte button = 7;
const byte btRxPin = 10;  // Arduino receives here, from HC-05 TXD
const byte btTxPin = 11;  // Arduino transmits here, to HC-05 RXD

SoftwareSerial bt(btRxPin, btTxPin);

const unsigned long HEARTBEAT_TIMEOUT = 6000;  // ms without PING = phone lost

char command[16];
byte commandLength = 0;

unsigned long lastHeard = 0;
bool heard = false;      // something arrived from the phone
bool wasLinked = false;
bool armed = false;      // true once the phone has connected
bool muted = false;      // separation alarm silenced with the button
bool finding = false;    // FIND mode active
unsigned long testBeepUntil = 0;

bool lastButton = HIGH;
unsigned long lastButtonChange = 0;

void setup() {
  pinMode(bluetoothState, INPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(button, INPUT_PULLUP);

  Serial.begin(9600);  // USB serial monitor, for debugging
  bt.begin(9600);      // HC-05 default data baud rate
  Serial.println(F("LostGuard tracker ready"));
}

bool isLinked() {
#if USE_STATE_PIN
  return digitalRead(bluetoothState) == HIGH;
#else
  return heard && (millis() - lastHeard < HEARTBEAT_TIMEOUT);
#endif
}

void handleCommand(const char *c) {
  if (strcmp(c, "PING") == 0) {
    bt.println(F("PONG"));
    return;
  }

  Serial.print(F("Command: "));
  Serial.println(c);

  if (strcmp(c, "FIND") == 0) {
    finding = true;
    bt.println(F("FINDING"));
  } else if (strcmp(c, "STOP") == 0) {
    finding = false;
    testBeepUntil = 0;
    bt.println(F("STOPPED"));
  } else if (strcmp(c, "STATUS") == 0) {
    bt.print(F("ONLINE, alarm "));
    bt.print(finding ? F("ON") : F("OFF"));
    bt.print(F(", up "));
    bt.print(millis() / 1000);
    bt.println(F(" s"));
  } else if (strcmp(c, "TEST") == 0) {
    testBeepUntil = millis() + 300;
    bt.println(F("TEST OK"));
  } else if (strcmp(c, "BYE") == 0) {
    // Planned disconnect: go quiet instead of raising the alarm
    armed = false;
    finding = false;
    heard = false;
  } else {
    bt.println(F("UNKNOWN"));
  }
}

void readBluetooth() {
  while (bt.available()) {
    char ch = bt.read();
    lastHeard = millis();
    heard = true;

    if (ch == '\n' || ch == '\r') {
      if (commandLength > 0) {
        command[commandLength] = '\0';
        commandLength = 0;
        handleCommand(command);
      }
    } else if (commandLength < sizeof(command) - 1) {
      command[commandLength++] = toupper(ch);
    }
  }
}

void loop() {
  readBluetooth();

  bool linked = isLinked();
  if (linked && !wasLinked) {
    armed = true;     // phone is here: watch for separation from now on
    muted = false;
    Serial.println(F("Phone connected"));
  }
  if (!linked && wasLinked) {
    finding = false;
    Serial.println(F("Phone lost"));
  }
  wasLinked = linked;

  // Button with a simple debounce
  bool pressed = digitalRead(button) == LOW;
  bool reading = pressed ? LOW : HIGH;
  if (reading != lastButton && millis() - lastButtonChange > 40) {
    lastButton = reading;
    lastButtonChange = millis();
    if (pressed) {
      if (finding) {
        finding = false;            // found it: stop the find-me alarm
        if (linked) bt.println(F("STOPPED"));
      } else if (!linked && armed) {
        muted = true;               // silence the separation alarm
      } else if (linked) {
        bt.println(F("BUTTON"));
      }
    }
  }

  bool fastBlink = (millis() / 150) % 2 == 0;
  bool slowBlink = (millis() / 500) % 2 == 0;

  if (finding) {
    // Find-me: fast beeps and flashing LEDs
    digitalWrite(greenLED, fastBlink);
    digitalWrite(redLED, !fastBlink);
    digitalWrite(buzzer, fastBlink);
  } else if (linked) {
    // Phone connected
    digitalWrite(greenLED, HIGH);
    digitalWrite(redLED, LOW);
    bool testBeep = millis() < testBeepUntil;
    digitalWrite(buzzer, (pressed || testBeep) ? HIGH : LOW);
  } else {
    // Phone not connected
    digitalWrite(greenLED, LOW);
    digitalWrite(redLED, HIGH);
    digitalWrite(buzzer, (armed && !muted && slowBlink) ? HIGH : LOW);
  }
}
