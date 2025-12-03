#include <SoftwareSerial.h>
#include "HX711.h"
#include <TM1637Display.h>
#include <avr/wdt.h>

#define startButton A2
#define btn1 A3
#define btn2 A4
#define btn3 A5

#define R1 10
#define R2 11
#define R3 12
#define ON LOW
#define OFF HIGH

#define BUZZER_PIN 13

void beepShort()
{
  tone(BUZZER_PIN, 2500);
  delay(120);
  noTone(BUZZER_PIN);
}
void beepError()
{
  tone(BUZZER_PIN, 800);
  delay(250);
  noTone(BUZZER_PIN);
}

#define RX 8
#define TX 9
SoftwareSerial link(RX, TX);

#define HX_DT A0
#define HX_SCK A1
HX711 scale;

TM1637Display d1(6, 7);
TM1637Display d2(4, 5);
TM1637Display d3(2, 3);

long hxOffset = 0;
float hxScale = 210.0;
float heavyLimit = 5.0;

const int SMALL_TIME = 60;
const int MEDIUM_TIME = 180;
const int LARGE_TIME = 300;

bool busyA2 = false;
bool waitingSelection = false;
bool rewardPending = false;

bool r1run = false, r2run = false, r3run = false;
unsigned long r1end = 0, r2end = 0, r3end = 0;

int selectionSec = 0;

bool b1state = false, b2state = false, b3state = false;
unsigned long blinkAt = 0;

unsigned long selectionStartTime = 0;
int transactionTimeoutSec = 10;

long readScaleWithTimeout(unsigned long timeoutMs = 100)
{
  unsigned long start = millis();
  while (!scale.is_ready())
  {
    if (millis() - start > timeoutMs)
      return 0;
    delay(1);
  }
  return scale.read();
}

float grams()
{
  long raw = readScaleWithTimeout();
  return -(raw - hxOffset) / hxScale;
}

void tare()
{
  long sum = 0;
  for (int i = 0; i < 3; i++)
  {
    sum += readScaleWithTimeout();
    delay(5);
  }
  hxOffset = sum / 3;
  Serial.print("[A1] TARE NEW OFFSET=");
  Serial.println(hxOffset);
}

void showTimer(TM1637Display &d, int sec, bool blink)
{
  int m = sec / 60;
  int s = sec % 60;
  uint8_t dots = blink ? 0b01000000 : 0;
  d.showNumberDecEx(m * 100 + s, dots, true);
}

void resetDisplays()
{
  d1.showNumberDecEx(0, 0, true);
  d2.showNumberDecEx(0, 0, true);
  d3.showNumberDecEx(0, 0, true);
  Serial.println("[A1] RESET DISPLAYS");
}

void startRelay(int id, int sec)
{
  Serial.print("[A1] START RELAY ");
  Serial.print(id);
  Serial.print(" TIME=");
  Serial.println(sec);

  if (id == 1)
  {
    r1run = true;
    r1end = millis() + sec * 1000UL;
    digitalWrite(R1, ON);
  }
  if (id == 2)
  {
    r2run = true;
    r2end = millis() + sec * 1000UL;
    digitalWrite(R2, ON);
  }
  if (id == 3)
  {
    r3run = true;
    r3end = millis() + sec * 1000UL;
    digitalWrite(R3, ON);
  }
}

void updateRelays()
{
  unsigned long now = millis();

  if (now - blinkAt >= 1000)
  {
    blinkAt = now;
    b1state = !b1state;
    b2state = !b2state;
    b3state = !b3state;
  }

  if (r1run)
  {
    if (now >= r1end)
    {
      r1run = false;
      digitalWrite(R1, OFF);
      d1.showNumberDecEx(0, 0, true);
      Serial.println("[A1] RELAY 1 END");
    }
    else
      showTimer(d1, (r1end - now) / 1000, b1state);
  }

  if (r2run)
  {
    if (now >= r2end)
    {
      r2run = false;
      digitalWrite(R2, OFF);
      d2.showNumberDecEx(0, 0, true);
      Serial.println("[A1] RELAY 2 END");
    }
    else
      showTimer(d2, (r2end - now) / 1000, b2state);
  }

  if (r3run)
  {
    if (now >= r3end)
    {
      r3run = false;
      digitalWrite(R3, OFF);
      d3.showNumberDecEx(0, 0, true);
      Serial.println("[A1] RELAY 3 END");
    }
    else
      showTimer(d3, (r3end - now) / 1000, b3state);
  }
}

void setup()
{
  Serial.begin(9600);
  link.begin(4800);
  wdt_disable();

  pinMode(startButton, INPUT_PULLUP);
  pinMode(btn1, INPUT_PULLUP);
  pinMode(btn2, INPUT_PULLUP);
  pinMode(btn3, INPUT_PULLUP);

  pinMode(R1, OUTPUT);
  pinMode(R2, OUTPUT);
  pinMode(R3, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(R1, OFF);
  digitalWrite(R2, OFF);
  digitalWrite(R3, OFF);

  d1.setBrightness(7);
  d2.setBrightness(7);
  d3.setBrightness(7);

  scale.begin(HX_DT, HX_SCK);
  hxOffset = 0;
  resetDisplays();

  Serial.println("[A1] SETUP DONE");
  Serial.println("[A1] SENDING RESET TO A2");

  link.println("RESET");
  delay(200);
  link.println("READY");

  Serial.println("[A1] READY SENT");
}

void loop()
{
  updateRelays();

  if (waitingSelection &&
      millis() - selectionStartTime > transactionTimeoutSec * 1000UL)
  {
    waitingSelection = false;
    rewardPending = false;
    Serial.println("[A1] TIMEOUT CANCEL");
    resetDisplays();
  }

  if (digitalRead(startButton) == LOW)
  {
    static unsigned long last = 0;
    if (millis() - last > 300)
    {
      Serial.println("[A1] START BUTTON PRESSED");
      if (!busyA2 && !rewardPending)
      {
        beepShort();
        tare();
        link.println("START");
        busyA2 = true;
        Serial.println("[A1] SEND START");
      }
      last = millis();
    }
  }

  if (waitingSelection)
  {
    static unsigned long t1 = 0, t2 = 0, t3 = 0;

    if (digitalRead(btn1) == LOW && millis() - t1 > 300)
    {
      beepShort();
      if (r1run)
        r1end += selectionSec * 1000UL;
      else
        startRelay(1, selectionSec);
      waitingSelection = false;
      rewardPending = false;
      t1 = millis();
    }

    if (digitalRead(btn2) == LOW && millis() - t2 > 300)
    {
      beepShort();
      if (r2run)
        r2end += selectionSec * 1000UL;
      else
        startRelay(2, selectionSec);
      waitingSelection = false;
      rewardPending = false;
      t2 = millis();
    }

    if (digitalRead(btn3) == LOW && millis() - t3 > 300)
    {
      beepShort();
      if (r3run)
        r3end += selectionSec * 1000UL;
      else
        startRelay(3, selectionSec);
      waitingSelection = false;
      rewardPending = false;
      t3 = millis();
    }
  }

  if (link.available())
  {
    String msg = link.readStringUntil('\n');
    Serial.println("MSG:" + msg);
    msg.trim();

    Serial.print("[A1 RECV] ");
    Serial.println(msg);

    if (msg == "READY")
      busyA2 = false;

    if (msg.startsWith("SIZE:"))
    {
      String sz = msg.substring(5);
      if (sz == "S")
        selectionSec = SMALL_TIME;
      else if (sz == "M")
        selectionSec = MEDIUM_TIME;
      else
        selectionSec = LARGE_TIME;

      link.println("WEIGH");
    }

    if (msg == "DO_WEIGH")
    {
      float g = grams();

      Serial.println(g);

      if (g <= heavyLimit)
      {
        link.println("ACCEPT");
        resetDisplays();
        waitingSelection = true;
        rewardPending = true;
        selectionStartTime = millis();
      }
      else
      {
        beepError();
        link.println("REJECT");
        resetDisplays();
      }
    }
  }

  delay(20);
}
