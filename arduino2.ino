#include <SoftwareSerial.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>

#define RX 8
#define TX 9
SoftwareSerial link(RX, TX);

Servo topLid;
Servo bottomLid;
Servo solarServo;

#define IR_PIN 4
#define TRIG 3
#define ECHO 2
#define BIN_TRIG 10
#define BIN_ECHO 11

#define LED_GREEN 12
#define LED_RED 13

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define LDR_L A0
#define LDR_R A1

int bottleCount = 0;

float tooCloseCM = 5;
float clearCM = 70;

float LARGE_H = 15;
float MEDIUM_H = 25;

unsigned long lastBinCheck = 0;
const unsigned long BIN_CHECK_INTERVAL = 60000;

bool busy = false;
bool waiting = false;
bool sizing = false;
bool afterClose = false;
bool tooCloseRecovery = false;
bool recoveryClear = false;
bool rejectRecovery = false;
bool solarEnabled = true;

unsigned long detectTime = 0;
unsigned long closeWait = 0;
unsigned long recoveryStart = 0;
unsigned long rejectStart = 0;

bool binOK = true;

int solarPos = 90;
int SOLAR_CENTER = 90;
int SOLAR_MIN_ANGLE = 0;
int SOLAR_MAX_ANGLE = 180;
int SOLAR_NIGHT_THRESHOLD = 40;

long ultraRead(int trig, int echo)
{
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long u = pulseIn(echo, HIGH, 30000);
  if (u == 0)
    return 999;
  return (u * 0.034) / 2;
}

float heightCM() { return ultraRead(TRIG, ECHO); }
float binCM() { return ultraRead(BIN_TRIG, BIN_ECHO); }

String detectSize(float h)
{
  if (h < LARGE_H)
    return "L";
  if (h < MEDIUM_H)
    return "M";
  return "S";
}

void openTop()
{
  if (binOK)
  {
    topLid.write(0);
    Serial.println("[A2] TOP OPEN");
  }
}

void closeTop()
{
  topLid.write(90);
  Serial.println("[A2] TOP CLOSE");
}

void openBottom()
{
  if (binOK)
  {
    bottomLid.write(0);
    Serial.println("[A2] BOTTOM OPEN");
  }
}

void closeBottom()
{
  bottomLid.write(90);
  Serial.println("[A2] BOTTOM CLOSE");
}

void ledGreen()
{
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);
  Serial.println("[A2] LED GREEN");
}

void ledRed()
{
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, HIGH);
  Serial.println("[A2] LED RED");
}

void lcdCount()
{
  if (!binOK)
    return;

  lcd.clear();
  lcd.print("Bottle count:");
  lcd.setCursor(0, 1);
  lcd.print(bottleCount);

  Serial.print("[A2] LCD COUNT=");
  Serial.println(bottleCount);
}

void lcdFull()
{
  lcd.clear();
  lcd.print("BIN IS FULL");
  Serial.println("[A2] LCD FULL");
}

void sendReady()
{
  if (!binOK)
    return;
  if (digitalRead(IR_PIN) == 0)
    return;

  link.println("READY");
  Serial.println("[A2] SEND READY");
}

void updateBinStatus()
{
  float d = binCM();

  Serial.print("[A2] BIN DIST=");
  Serial.println(d);

  if (d < 10)
  {
    if (binOK)
      Serial.println("[A2] BIN -> FULL");

    binOK = false;
    ledRed();
    lcdFull();
  }
  else
  {
    if (!binOK)
      Serial.println("[A2] BIN -> OK");

    binOK = true;
    ledGreen();
    lcdCount();
  }

  lastBinCheck = millis();
}

void fullResetState()
{
  busy = false;
  waiting = false;
  sizing = false;
  afterClose = false;
  tooCloseRecovery = false;
  recoveryClear = false;
  rejectRecovery = false;
  solarEnabled = true;

  closeTop();
  closeBottom();
  solarServo.write(SOLAR_CENTER);
  solarPos = SOLAR_CENTER;

  Serial.println("[A2] FULL RESET");
}

void solarTrack()
{
  if (!solarEnabled)
    return;

  long sumL = 0, sumR = 0;
  for (int i = 0; i < 10; i++)
  {
    sumL += analogRead(LDR_L);
    sumR += analogRead(LDR_R);
    delay(1);
  }

  int L = sumL / 10;
  int R = sumR / 10;

  if (L < SOLAR_NIGHT_THRESHOLD && R < SOLAR_NIGHT_THRESHOLD)
    return;

  int diff = L - R;
  if (abs(diff) < 200)
    return;

  int step = diff > 0 ? 1 : -1;
  int target = solarPos + step;

  if (target < SOLAR_MIN_ANGLE)
    target = SOLAR_MIN_ANGLE;
  if (target > SOLAR_MAX_ANGLE)
    target = SOLAR_MAX_ANGLE;

  if (target != solarPos)
  {
    solarPos = target;
    solarServo.write(solarPos);
  }
}

void setup()
{
  Serial.begin(9600);
  link.begin(4800);
  delay(100);

  pinMode(IR_PIN, INPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BIN_TRIG, OUTPUT);
  pinMode(BIN_ECHO, INPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  topLid.attach(5);
  bottomLid.attach(6);
  solarServo.attach(7);

  closeTop();
  closeBottom();
  solarServo.write(SOLAR_CENTER);

  lcd.init();
  lcd.backlight();

  updateBinStatus();

  Serial.println("[A2 READY]");
}

void loop()
{
  if (!waiting && !sizing && !afterClose && !rejectRecovery && !tooCloseRecovery)
    solarTrack();

  if (millis() - lastBinCheck >= BIN_CHECK_INTERVAL && !busy)
    updateBinStatus();

  String s = "";
  if (link.available())
  {
    s = link.readStringUntil('\n');
    Serial.println("MSG:" + s);
    s.trim();
    Serial.print("[A2 RECV] ");
    Serial.println(s);
  }

  if (!binOK)
  {
    if (s == "START" || s == "ACCEPT" || s == "REJECT")
    {
      link.println("REJECT");
      Serial.println("[A2] BLOCKED: BIN FULL");
      return;
    }
  }

  if (s == "RESET")
  {
    fullResetState();
    updateBinStatus();
    link.println("READY");
    Serial.println("[A2] RESET COMPLETE");
    return;
  }

  int ir = digitalRead(IR_PIN);
  float h = heightCM();

  if (s == "START" && !busy)
  {
    busy = true;
    solarEnabled = false;

    waiting = true;
    openTop();
    Serial.println("[A2] START");
  }

  if (waiting)
  {
    if (h < tooCloseCM)
    {
      tooCloseRecovery = true;
      waiting = false;
      recoveryStart = millis();
      Serial.println("[A2] TOO CLOSE");
      return;
    }

    if (ir == 0)
    {
      waiting = false;
      detectTime = millis();
      sizing = true;
      Serial.println("[A2] BOTTLE DETECT");
    }
  }

  if (sizing)
  {
    if (millis() - detectTime >= 500)
    {
      closeTop();
      sizing = false;
      afterClose = true;
      closeWait = millis();
      Serial.println("[A2] CLOSE TOP -> MEASURE");
    }
  }

  if (afterClose)
  {
    if (millis() - closeWait >= 500)
    {
      float hh = heightCM();
      String sz = detectSize(hh);

      delay(20);
      link.print("SIZE:");
      link.println(sz);
      delay(20);

      Serial.print("[A2] SIZE=");
      Serial.println(sz);

      afterClose = false;
    }
  }

  if (s == "WEIGH")
  {
    link.println("DO_WEIGH");
    Serial.println("[A2] DO_WEIGH");
  }

  if (s == "ACCEPT")
  {
    openBottom();
    delay(700);
    closeBottom();

    bottleCount++;
    updateBinStatus();

    busy = false;
    solarEnabled = true;

    sendReady();
    lcdCount();

    Serial.println("[A2] ACCEPT DONE");
  }

  if (s == "REJECT")
  {
    openTop();
    delay(1200);
    rejectRecovery = true;
    rejectStart = millis();
    Serial.println("[A2] REJECT OPEN");
  }

  if (tooCloseRecovery)
  {
    if (millis() - recoveryStart >= 2000)
    {
      if (heightCM() > clearCM)
      {
        tooCloseRecovery = false;
        recoveryClear = true;
        Serial.println("[A2] TOO CLOSE CLEARED");
      }
    }
  }

  if (recoveryClear)
  {
    if (millis() - recoveryStart >= 4000)
    {
      recoveryClear = false;
      busy = false;
      solarEnabled = true;
      sendReady();
      Serial.println("[A2] READY AFTER TOO CLOSE");
    }
  }

  if (rejectRecovery)
  {
    if (millis() - rejectStart >= 2000)
    {
      closeTop();
      rejectRecovery = false;
      busy = false;
      solarEnabled = true;
      sendReady();
      Serial.println("[A2] READY AFTER REJECT");
    }
  }

  delay(10);
}
