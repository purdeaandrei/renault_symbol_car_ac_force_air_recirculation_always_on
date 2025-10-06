
#include <EEPROM.h>

#define BUTTON_AIR_RECYCLING_PIN 2
#define LED_AIR_RECYCLING_ENABLED_PIN 4
#define LED_OFF_PIN 5
#define LED_DEMISTING_ENABLED_PIN 6

#define BUZZER_PIN 3

#define MINIMUM_LONG_PRESS_MS 3000

// The following define the current state of operation:

// TRUE means that we are note OFF (disabled), and we have short-pressed the recycling button
// and we desire the recycling to be kept on.
#define TRUE 1

// FALSE means that we have short-pressed the recycling button while recycling was enabled
// This means that we are okay with having it be disabled.
// Recycling disabled does not need to be enforced, we have not seen the car turn on
// the recycling on its own.
#define FALSE 2

// OFF means that we have long-pressed the recycling button, to disable the functionality
// of this module. When we're on OFF mode, short presses of the recycling button work
// normally, as if the module was not installed into the AC. Long-press the button again
// to turn the module back on.
#define OFF 3

// These are ideal encoded state values, as they show up in eeprom:
// They are designed to be very different from each other to make it possible
// to detect the correct value even in the case of bit-errors.
#define TRUEVAL 0xE0
#define FALSEVAL 0x1C
#define OFFVAL 0x03

#define TESTING 0
#define LOGGING 0

// This is a function that counts the number of high bits in a byte
int cntBits(uint8_t value)
{
  int i;
  int cnt = 0;
  for (i=0;i<8;i++)
  {
    cnt += value & 1;
    value >>= 1;
  }
  return cnt;
}

// This function interprets a byte from eeprom, and decides if it is one
// of the states, or if the eeprom location is considered empty
int interpretVal(uint8_t value)
{
  if ((value == 0) || (value == 0xff)) return 0; // <-- these are empty values
  if (cntBits(value ^ TRUEVAL) <=2 ) return TRUE;
  if (cntBits(value ^ FALSEVAL) <=2 ) return FALSE;
  if (cntBits(value ^ OFFVAL) <=1 ) return OFF;
  return 0;
}

// In order to maximize lifetime of the eeprom, we don't store the current
// state in a single location. Instead the eeprom starts storing the state
// at the highest eeprom location, with all other locations empty. When
// this is overwritten, the memory location below it becomes not-empty,
// and its value takes precedence.
// This function returns the currently valid state stored in eeprom,
// and that is always at the lowest non-empty location.
char getValue()
{
  for (int i = 0 ; i < EEPROM.length() ; i++) {
    int ival = interpretVal(EEPROM.read(i));
    if (ival)
    {
      // This location is non-empty, and as such we have found our state.
      return ival;
    }
  }
  return OFF;
}

uint8_t getEncodedValue(int value)
{
  if(value==TRUE) return TRUEVAL;
  if(value==FALSE) return FALSEVAL;
  return OFFVAL;
}

void setValue(char value)
{
  uint8_t encodedValue = getEncodedValue(value);
  int ival = interpretVal(EEPROM.read(0));
  if (ival)
  {
    // When the eeprom is full, erase it
    for (int j = 0 ; j < EEPROM.length() ; j++) {
      EEPROM.write(j, 0xff);
    }
  }
  int i;
  for (i = 0 ; i <= EEPROM.length() ; i++) {
    ival = (i == EEPROM.length()) ? TRUE : interpretVal(EEPROM.read(i));
    if (ival)
    {
      do { 
        i--;
        EEPROM.write(i, encodedValue);
      } while ((EEPROM.read(i) != encodedValue) && (i>=0));
      break;
    }
  }
  if (i<0)
  {
    for (int j = 0 ; j < EEPROM.length() ; j++) {
      EEPROM.write(j, encodedValue);
    }
  }
}

void stat()
{
  #if LOGGING
    Serial.print("eeprom words: ");
    Serial.print(EEPROM.length(), DEC);
    for (int i = 0 ; i < EEPROM.length() ; i++) {
      if ((i % 32)==0) Serial.println("");
      Serial.print(EEPROM.read(i), HEX);
      Serial.print(" ");
    }
    Serial.println("");
  #endif
}

void beep(unsigned int ms)
{
  pinMode(BUZZER_PIN, OUTPUT);
  uint32_t hz = 4592;
  unsigned int delaymicro = 1000000UL / hz / 2;
  uint32_t tot = 0;
  while(1) {
    digitalWrite(3, 1);
    delayMicroseconds(delaymicro); tot += delaymicro;
    digitalWrite(3, 0);
    delayMicroseconds(delaymicro); tot += delaymicro;
    if (tot > ms * 1000UL) break;
  }
  //pinMode(BUZZER_PIN, INPUT);
}

void beeptimes(int n)
{
  int i;
  for (i=0;i<n-1;i++)
  {
    beep(100);
    delay(200);
  }
  beep(100); 
}

void pressButton()
{
  digitalWrite(BUTTON_AIR_RECYCLING_PIN, 0);
  pinMode(BUTTON_AIR_RECYCLING_PIN, OUTPUT);
  digitalWrite(BUTTON_AIR_RECYCLING_PIN, 0);
  delay(200);
  #if TESTING
  pinMode(BUTTON_AIR_RECYCLING_PIN, INPUT_PULLUP);
  #else
  pinMode(BUTTON_AIR_RECYCLING_PIN, INPUT);
  #endif
  while (digitalRead(BUTTON_AIR_RECYCLING_PIN)==0); // wait for it to go back up
}

int currentValue = 0;
int previousButton = 1;
uint32_t pressed_time = 0;
int waiting_for_release = 0;
int disable_scanning = 0;
uint32_t disable_scanning_time = 0;

void setup() {
  #if LOGGING
    Serial.begin(9600);
  #endif
  #if TESTING
  pinMode(BUTTON_AIR_RECYCLING_PIN, INPUT_PULLUP);
  pinMode(LED_AIR_RECYCLING_ENABLED_PIN, INPUT_PULLUP);
  pinMode(LED_OFF_PIN, OUTPUT);
  pinMode(LED_DEMISTING_ENABLED_PIN, OUTPUT);
  digitalWrite(LED_OFF_PIN, 0);
  digitalWrite(LED_DEMISTING_ENABLED_PIN, 0);
  #else
  pinMode(BUTTON_AIR_RECYCLING_PIN, INPUT);
  pinMode(LED_AIR_RECYCLING_ENABLED_PIN, INPUT);
  pinMode(LED_OFF_PIN, INPUT);
  pinMode(LED_DEMISTING_ENABLED_PIN, INPUT);
  #endif
  delay(700);
  currentValue = getValue();
  previousButton = digitalRead(BUTTON_AIR_RECYCLING_PIN);
}
 
void loop() {
  if (digitalRead(LED_OFF_PIN))
  {
    while (digitalRead(LED_OFF_PIN)) {}
    delay(700); // After the air conditioner is turned on, for roughly 700 ms, if the recirculation button is pressed it won't work.
  }
  int button = digitalRead(BUTTON_AIR_RECYCLING_PIN);
  if ((button == 0) && previousButton)
  {
    pressed_time = millis();
    #if LOGGING
      Serial.println("Pressed");
    #endif
    waiting_for_release = 1;
  }
  if ((button == 0) && waiting_for_release && ((millis() - pressed_time) > MINIMUM_LONG_PRESS_MS))
  {
    #if LOGGING
      Serial.println("Timeout");
    #endif
    waiting_for_release = 0;
    // Enable or disable my functionality now.
    if (currentValue == OFF)
    {
      beeptimes(4);
      setValue(TRUE);
      currentValue = TRUE;
    } else {
      beeptimes(3);
      setValue(OFF);
      currentValue = OFF;
    }
    while (digitalRead(BUTTON_AIR_RECYCLING_PIN)==0); // wait for button to be actually released before doing anything
    delay(200); // And wait 200 more ms just to make sure everything is stable.
  }
  if (button && waiting_for_release)
  {
    // release_event. Now we change whether we want to force it off or on.
    #if LOGGING
      Serial.println("Released");
    #endif
    waiting_for_release = 0;
    if (currentValue != OFF)
    {
      if (digitalRead(LED_AIR_RECYCLING_ENABLED_PIN))
      {
        if (currentValue != TRUE) setValue(TRUE);
        currentValue = TRUE;
        beeptimes(2);
      } else {
        if (currentValue != FALSE) setValue(FALSE);
        currentValue = FALSE;
        beeptimes(1);
      }
      delay(200); // And wait 200 ms just to make sure everything is stable. Button press itself should have already set the desired state.
    }
  }
  previousButton = button;
  if (disable_scanning && ((millis() - disable_scanning_time) > 2000))
  {
    #if LOGGING
      Serial.println("Enable Scanning");
    #endif
    disable_scanning = 0;
  }
  if ((!disable_scanning) && (!waiting_for_release) && (!digitalRead(LED_OFF_PIN)) && (!digitalRead(LED_DEMISTING_ENABLED_PIN)))
  {
    /* We are only operating if we are not in any type of auto mode */
    int recyclingEnabled = digitalRead(LED_AIR_RECYCLING_ENABLED_PIN);
    if (digitalRead(LED_OFF_PIN) || digitalRead(LED_DEMISTING_ENABLED_PIN)) return; // make sure again we're not in an auto mode. (in case it was just turned on)
    if (((!recyclingEnabled) && currentValue == TRUE))
    {
      // Air conditioner decided to disable recycling, so we must force re-enable it
      #if LOGGING
        Serial.println("Pushing");
      #endif
      pressButton();
      #if LOGGING
        Serial.println("Pushed");
      #endif
      beeptimes(1);
      disable_scanning_time = millis();
      disable_scanning = 1;
      #if LOGGING
        Serial.println("Done");
      #endif
    }
  }
}
