/*
  LOSTGUARD tracker (Arduino Uno / Nano + HC-05)

  Pins:
    HC-05 TXD -> D10          HC-05 RXD <- D11 (through the 1k / 2k divider)
    Green LED -> D4           Red LED -> D5
    Buzzer    -> D6           Button  -> D7 to GND
    HC-05 STATE -> D2 (optional, see USE_STATE_PIN)

  Commands from the app (each ends with a newline):
    FIND, STOP, STATUS, TEST, plus PING (heartbeat) and BYE (planned disconnect)

  Commands are accepted on D10/D11 and also on D0/D1 (hardware serial), so the
  sketch works with the HC-05 on either pair of pins. You can also type the
  commands into the Serial Monitor at 9600 baud to test without a phone.
*/

#include <SoftwareSerial.h>

// 0 = detect the phone from the app's heartbeat (works with 4 wires to the HC-05)
// 1 = detect the phone from the HC-05 STATE pin on D2 (only if that wire exists)
#define USE_STATE_PIN 0

#define BT_STATE 2
#define GREEN_LED 4
#define RED_LED 5
#define BUZZER 6
#define BUTTON 7
#define BT_RX 10   // Arduino receives here, from HC-05 TXD
#define BT_TX 11   // Arduino transmits here, to HC-05 RXD

SoftwareSerial bt(BT_RX, BT_TX);

const unsigned long HEARTBEAT_TIMEOUT = 6000;  // ms without data = phone lost

bool alarmMuted = false;
bool previousConnection = false;
bool finding = false;
bool heard = false;
unsigned long lastHeard = 0;
unsigned long testBeepUntil = 0;
bool previousButton = false;

char btLine[16];
byte btLength = 0;
char usbLine[16];
byte usbLength = 0;

void setup() {
  pinMode(BT_STATE, INPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);

  Serial.begin(9600);
  bt.begin(9600);
  Serial.println(F("LOSTGUARD STARTED"));
}

// Sends a reply to the phone on both possible wirings
void reply(const __FlashStringHelper *text) {
  bt.println(text);
  Serial.println(text);
}

void handleCommand(const char *c) {
  if (strcmp(c, "PING") == 0) {
    reply(F("PONG"));
  } else if (strcmp(c, "FIND") == 0) {
    finding = true;
    reply(F("FINDING"));
  } else if (strcmp(c, "STOP") == 0) {
    finding = false;
    testBeepUntil = 0;
    reply(F("STOPPED"));
  } else if (strcmp(c, "STATUS") == 0) {
    reply(finding ? F("ONLINE, alarm ON") : F("ONLINE, alarm OFF"));
  } else if (strcmp(c, "TEST") == 0) {
    testBeepUntil = millis() + 300;
    reply(F("TEST OK"));
  } else if (strcmp(c, "BYE") == 0) {
    // The app is disconnecting on purpose: stay quiet
    finding = false;
    heard = false;
    alarmMuted = true;
    previousConnection = false;
  } else {
    reply(F("UNKNOWN"));
  }
}

void readFrom(Stream &port, char *line, byte &length) {
  while (port.available()) {
    char ch = port.read();
    lastHeard = millis();
    heard = true;
    if (ch == '\n' || ch == '\r') {
      if (length > 0) {
        line[length] = '\0';
        length = 0;
        handleCommand(line);
      }
    } else if (length < 15) {
      line[length++] = toupper(ch);
    }
  }
}

bool phoneConnected() {
#if USE_STATE_PIN
  return digitalRead(BT_STATE) == HIGH;
#else
  return heard && (millis() - lastHeard < HEARTBEAT_TIMEOUT);
#endif
}

void loop() {
  readFrom(bt, btLine, btLength);
  readFrom(Serial, usbLine, usbLength);

  bool connected = phoneConnected();

  // Phone reconnect detection
  if (connected && !previousConnection) {
    alarmMuted = false;
  }
  if (!connected && previousConnection) {
    finding = false;
  }
  previousConnection = connected;

  bool pressed = digitalRead(BUTTON) == LOW;
  bool justPressed = pressed && !previousButton;
  previousButton = pressed;

  if (finding) {
    // Find Item: fast beeps and flashing LEDs until STOP or the button
    bool on = (millis() / 150) % 2 == 0;
    digitalWrite(GREEN_LED, on);
    digitalWrite(RED_LED, !on);
    digitalWrite(BUZZER, on);
    if (justPressed) {
      finding = false;
      reply(F("STOPPED"));
    }
  } else if (connected) {
    // Bluetooth connected
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, millis() < testBeepUntil ? HIGH : LOW);
  } else {
    // Bluetooth disconnected
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    if (pressed) {
      alarmMuted = true;
    }
    digitalWrite(BUZZER, alarmMuted ? LOW : HIGH);
  }

  delay(5);
}
