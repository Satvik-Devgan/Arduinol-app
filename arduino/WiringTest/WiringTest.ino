/*
  LOSTGUARD wiring test

  Finds out why the web page says "the Arduino is not answering".
  It tries the HC-05 at several speeds, 8 seconds each, over and over:
    - everything the phone sends is printed in the Serial Monitor
    - every second it sends "HELLO FROM ARDUINO AT <speed>" to the phone
    - the green LED flashes whenever data arrives from the HC-05

  How to use:
    1. Upload this sketch. Open the Serial Monitor at 9600 baud
       (on a Mac, on the USB port, not cu.HC-05).
    2. Open the LostGuard page, connect to the HC-05 and leave it connected.
    3. Watch the Serial Monitor and the page's activity log for about a minute.

  Wiring it expects (same as the LostGuard sketch):
    HC-05 TXD -> D10
    D11 -> 1k -> HC-05 RXD, with 2k from HC-05 RXD to GND
*/

#include <SoftwareSerial.h>

#define GREEN_LED 4
#define RED_LED 5
#define BT_RX 10   // from HC-05 TXD
#define BT_TX 11   // to HC-05 RXD, through the divider

SoftwareSerial bt(BT_RX, BT_TX);

const long RATES[] = {9600, 38400, 57600, 19200, 115200};
const byte RATE_COUNT = sizeof(RATES) / sizeof(RATES[0]);
const unsigned long STEP_MS = 8000;

byte rateIndex = 0;
unsigned long stepStarted = 0;
unsigned long lastHello = 0;
unsigned long lastByteAt = 0;
unsigned long bytesThisStep = 0;

char line[40];
byte lineLength = 0;

void startRate() {
  bt.end();
  bt.begin(RATES[rateIndex]);
  stepStarted = millis();
  bytesThisStep = 0;
  lineLength = 0;
  Serial.println();
  Serial.print(F("=== Trying the HC-05 at "));
  Serial.print(RATES[rateIndex]);
  Serial.println(F(" baud ==="));
}

void setup() {
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  Serial.begin(9600);
  Serial.println(F("LOSTGUARD WIRING TEST"));
  Serial.println(F("Keep the LostGuard page connected. It sends PING every 2 seconds."));
  startRate();
}

void loop() {
  while (bt.available()) {
    char c = bt.read();
    lastByteAt = millis();
    bytesThisStep++;
    if (c == '\n' || c == '\r') {
      if (lineLength > 0) {
        line[lineLength] = '\0';
        Serial.print(F("  received: "));
        Serial.println(line);
        lineLength = 0;
      }
    } else if (lineLength < sizeof(line) - 1) {
      line[lineLength++] = (c >= 32 && c < 127) ? c : '?';
    }
  }

  if (millis() - lastHello >= 1000) {
    lastHello = millis();
    bt.print(F("HELLO FROM ARDUINO AT "));
    bt.println(RATES[rateIndex]);
  }

  bool recent = millis() - lastByteAt < 300;
  digitalWrite(GREEN_LED, recent ? HIGH : LOW);
  digitalWrite(RED_LED, recent ? LOW : HIGH);

  if (millis() - stepStarted >= STEP_MS) {
    Serial.print(F("  bytes received at this speed: "));
    Serial.println(bytesThisStep);
    rateIndex = (rateIndex + 1) % RATE_COUNT;
    startRate();
  }
}
