/*
  Chess Timer consisting of code half stolen from the Internet, and half stolen from me.
  Perfectly balanced, as all things should be.

*/
#include <TM1637Display.h>
#include <SoftReset.h>

#define BTN_MID 3 // Pause/reset button
#define BTN_RIGHT 4
#define BTN_LEFT 5
#define LED_LEFT 6
#define LED_RIGHT 7
#define DISP_LEFT_CLK 8
#define DISP_LEFT_DIO 9
#define DISP_RIGHT_CLK 10
#define DISP_RIGHT_DIO 11
#define BUZZER 12
#define RIGHT 0
#define LEFT 1

TM1637Display displayLeft(DISP_LEFT_CLK, DISP_LEFT_DIO);
TM1637Display displayRight(DISP_RIGHT_CLK, DISP_RIGHT_DIO);

int LEFT_STATE = LOW;
int RIGHT_STATE = LOW;
int MID_STATE = LOW;
int LEFT_BTN_STATE = HIGH;
int MID_BTN_STATE = HIGH;
int RIGHT_BTN_STATE = HIGH;
int LAST_LEFT_BTN_STATE = HIGH;
int LAST_RIGHT_BTN_STATE = HIGH;
int LAST_MID_BTN_STATE = HIGH;
int leftReading;
int rightReading;
int midReading;
int activeSide = 0;
int startingSide = 0;
int losingSide = 0;
unsigned long timePassedForLeftMs;
unsigned long timePassedForRightMs;
unsigned long lastChangeMs;
unsigned long startTime = 0;
unsigned long chessTimeSecs = 300;
boolean gameStarted = false;
boolean gamePaused = false;
boolean gameFinished = false;
unsigned long lastLosingBuzzTime = 0;
unsigned long holdTime = 3000;
int losingBuzzCount = 0;
int losingBuzzThreshold = 3;
unsigned long lastLeftDebounceTime = 0;  // the last time the output pin was toggled
unsigned long lastRightDebounceTime = 0;  // the last time the output pin was toggled
unsigned long lastMidDebounceTime = 0;  // the last time the output pin was toggled
unsigned long midBtnLastPressed = 0;
unsigned long midBtnLastHeld = 0;
unsigned long midBtnLastPressedMenu = 0;
unsigned long pauseTime = 0;
unsigned long elapsedPauseTime = 0;
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

int timeControlIndex = 4;
unsigned long bonusTime = 4;
unsigned long timeControlArr[] = {60, 120, 180, 240, 300, 360, 420, 480, 540, 600};
unsigned long bonusTimeArr[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(9600); // For debugging
  
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LEFT, OUTPUT);
  pinMode(RIGHT, OUTPUT);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_MID, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  // set initial LED state
  digitalWrite(LEFT, LEFT_STATE);
  digitalWrite(RIGHT, RIGHT_STATE);

  displayLeft.clear();
  displayRight.clear();
  displayLeft.setBrightness(5);
  displayRight.setBrightness(5);
  chessTimeSecs = timeControlArr[timeControlIndex];
  // Set the display initially
  displayTime(&displayLeft, chessTimeSecs, true);
  displayTime(&displayRight, chessTimeSecs, true);
  
  buzz();
}

// the loop function runs over and over again forever
void loop() {
  // Debounce all switches
  debounceSwitches();
  if (gameStarted == false)
  {
    if (MID_BTN_STATE == LOW)
    {
      onSwitchMidPressMenu();
    }
    if (LEFT_BTN_STATE == LOW)
    {
      Serial.print("Left pressed; right goes first\n");
      startTime = millis();
      setLeftLed(LOW);
      setRightLed(HIGH);
      gameStarted = true;
      activeSide = RIGHT;
      startingSide = RIGHT;
      buzz();
    }
    if (RIGHT_BTN_STATE == LOW)
    {
      Serial.print("Right pressed; left goes first\n");
      startTime = millis();
      setRightLed(LOW);
      setLeftLed(HIGH);
      gameStarted = true;
      activeSide = LEFT;
      startingSide = LEFT;
      buzz();
    }
  }
  else // Game is started, countdown
  {
    if (!gameFinished)
    {
      onSwitchMidPress();
      if (!gamePaused)
      {
        if (activeSide == LEFT)
        {
          displayChessState();
          onSwitchLeftPress();
        }
        else
        if (activeSide == RIGHT)
        {
          displayChessState();
          onSwitchRightPress();
        }
      }
    }
    else // Game is finished. Blink losing side then reset.
    {
      if (losingBuzzCount >= losingBuzzThreshold)
      {
        soft_restart();
      }
      TM1637Display* display = activeSide == LEFT ? &displayLeft : &displayRight;
      if (millis() - lastLosingBuzzTime >= 2000)
      {
        displayTimeNumber(display, 0, true);
        buzzLosingSide();
        delay(1000);
        lastLosingBuzzTime = millis();
        losingBuzzCount++;
      }
      else
      {
        display->clear();
      }
    }
  }
  LAST_LEFT_BTN_STATE = leftReading;
  LAST_RIGHT_BTN_STATE = rightReading;
  LAST_MID_BTN_STATE = midReading;
}

void onSwitchRightPress()
{
  if (RIGHT_BTN_STATE == LOW && LEFT_BTN_STATE == HIGH) // Right pressed, make it left's turn
    {
      elapsedPauseTime = 0;
      updateActualTimePassed();
      setLeftLed(HIGH);
      setRightLed(LOW);
      activeSide = LEFT;
      buzz();
      Serial.print("right pressed");
    }
}

void onSwitchMidPress()
{
  if (MID_BTN_STATE == LOW && (millis() - midBtnLastPressed > 1000)) // Mid pressed, pause the game
  {
    if (gameFinished)
    {
      soft_restart();
    }
    Serial.print("MID PRESSED");
    buzz();
    buzz();
    gamePaused = !gamePaused;
    if (gamePaused)
    {
      pauseTime = millis();
      //elapsedPauseTime = -1;
    }
    else
    {
      long elapsedPauseTimeTemp = millis() - pauseTime;
      elapsedPauseTime = elapsedPauseTimeTemp;
      updateActualTimePassed();
    }
    midBtnLastPressed = millis();
    if (millis() - midBtnLastHeld > holdTime)
    {
      Serial.print("SET BUTTON LAST HELD");
      midBtnLastHeld = millis();
    }
    Serial.print("mid pressed");
  }
  if (MID_BTN_STATE == LOW && millis() - midBtnLastHeld > holdTime)
  {
    soft_restart();
  }
}

void onSwitchMidPressMenu()
{
  if (MID_BTN_STATE == LOW && (millis() - midBtnLastPressedMenu > 200)) // Mid pressed, pause the game
  {
    Serial.print("MID PRESSED MENU");
    buzz();
    if (timeControlIndex >= 9)
    {
      timeControlIndex = 0;
    }
    else
    {
      timeControlIndex++;
    }

    chessTimeSecs = timeControlArr[timeControlIndex];
    Serial.print("CHESS TIME SECS ");
    Serial.print(chessTimeSecs);
    bonusTime = bonusTimeArr[timeControlIndex];
    displayTime(&displayLeft, chessTimeSecs, true);
    displayTime(&displayRight, chessTimeSecs, true);
    midBtnLastPressedMenu = millis();
    
  }
}

void onSwitchLeftPress()
{
  if (LEFT_BTN_STATE == LOW && RIGHT_BTN_STATE == HIGH) // Left pressed, make it right's turn
  {
    elapsedPauseTime = 0;
    updateActualTimePassed();
    setRightLed(HIGH);
    setLeftLed(LOW);
    activeSide = RIGHT;
    buzz();
    Serial.print("left pressed");
  }
}

void setLeftLed(int state)
{
  digitalWrite(LED_LEFT, state);
}

void setRightLed(int state)
{
  digitalWrite(LED_RIGHT, state);
}

void buzz()
{
  tone(BUZZER, 100, 100);
}

void buzzLosingSide()
{
  tone(BUZZER, 100, 1000);
}

void displayTime(TM1637Display* display, unsigned long time, boolean dot)
{
  unsigned long mins = time / 60;
  unsigned long secs = time % 60;
  long timeNumber = mins * 100 + secs;
  displayTimeNumber(display, timeNumber, dot);
}
void displayTimeNumber(TM1637Display* display, unsigned long number, boolean dot)
{
  uint8_t digits[] = { 0, 0, 0, 0 };
  digits[3] = display->encodeDigit(number % 10);
  number /= 10;
  digits[2] = display->encodeDigit(number % 10);
  number /= 10;
  uint8_t d = display->encodeDigit(number % 10);
  if (dot)
  {
    d = addDot(d);
  }
  digits[1] = d;
  number /= 10;
  int m = number % 10;
  if (m > 0) {
    digits[0] = display->encodeDigit(m);
  }
  display->setSegments(digits, 4, 0);
}

uint8_t addDot(uint8_t digit)
{
  return digit | 0x80;
}
unsigned long getActualTimePassedForSide(byte side)
{
  unsigned long value = side == LEFT ? timePassedForLeftMs : timePassedForRightMs;
  unsigned long now = millis();
  if (side == activeSide)
  {
    if (side == startingSide)
    {
      value += now - startTime - lastChangeMs; // -- TODO: handle millis overflow!
    }
    else
    {
      value += now - lastChangeMs; // -- TODO: handle millis overflow!
    }
  }
  return value;
}

unsigned long lastDisplayedTime = 0;
boolean lastDisplayedDots = true;
void displayChessState() {
  TM1637Display* display = activeSide == LEFT ? &displayLeft : &displayRight;
  unsigned long timeMs = getActualTimePassedForSide(activeSide);
  unsigned long timeLeft = chessTimeSecs - timeMs / 1000;
  boolean displayDots = (timeMs / 250) % 2;
  if (timeLeft <= 0)
  {
    //setState(STATE_CHESS_FINISHED);
    timeLeft = 0;
    if (activeSide == LEFT)
    {
      Serial.print("Left loses");
      gameFinished = true;
      losingSide = LEFT;
    }
    else
    if (activeSide == RIGHT)
    {
      Serial.print("Right loses");
      gameFinished = true;
      losingSide = RIGHT;
    }
  }
  if ((timeLeft != lastDisplayedTime) || (lastDisplayedDots != displayDots))
  {
    displayTime(display, timeLeft, displayDots);
    lastDisplayedTime = timeLeft;
    lastDisplayedDots = displayDots;
  }
}

void updateActualTimePassed()
{
  unsigned long now = millis();
  unsigned long value = now - lastChangeMs - elapsedPauseTime; // -- TODO: handle millis overflow!
  Serial.print("Decreasing with:");
  Serial.println(value);
  if (activeSide == LEFT)
  {
    timePassedForLeftMs += value;
  }
  else
  {
    timePassedForRightMs += value;
  }
  lastChangeMs = now;
  Serial.print("Last change time ms:");
  Serial.println(lastChangeMs);
  lastDisplayedTime = chessTimeSecs + 1;
}

void debounceSwitches()
{
  
  leftReading = digitalRead(BTN_LEFT);
  rightReading = digitalRead(BTN_RIGHT);
  midReading = digitalRead(BTN_MID);

  // If button state has changed
  if (leftReading != LAST_LEFT_BTN_STATE)
  {
    lastLeftDebounceTime = millis();
  }
  // If button state has changed
  if (rightReading != LAST_RIGHT_BTN_STATE)
  {
    lastRightDebounceTime = millis();
  }

  if (midReading != LAST_MID_BTN_STATE)
  {
    lastMidDebounceTime = millis();
  }
  
  if ((millis() - lastLeftDebounceTime) > debounceDelay)
  {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:
    if (leftReading != LEFT_BTN_STATE)
    {
      LEFT_BTN_STATE = leftReading;
      if (LEFT_BTN_STATE == LOW)
      {
        LEFT_STATE = !LEFT_STATE;
      }
    }
  }
  
  if ((millis() - lastRightDebounceTime) > debounceDelay)
  {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:
    if (rightReading != RIGHT_BTN_STATE)
    {
      RIGHT_BTN_STATE = rightReading;
      if (RIGHT_BTN_STATE == LOW)
      {
        RIGHT_STATE = !RIGHT_STATE;
      }
    }
  }
  
  if ((millis() - lastMidDebounceTime) > debounceDelay)
  {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:
    if (midReading != MID_BTN_STATE)
    {
      MID_BTN_STATE = midReading;
      if (MID_BTN_STATE == LOW)
      {
        MID_STATE = !MID_STATE;
      }
    }
  }
}
