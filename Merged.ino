#include <Audio.h>
#include <Bounce2.h>
#include <EEPROM.h>
#include <genieArduino.h>
#include <SD.h>
#include <SerialFlash.h>
#include <SPI.h>
#include <Wire.h>

// ----------------------------------------------------------------
// INTERFACE VARIABLES
// ----------------------------------------------------------------

#define BUTTON_UP A4
#define BUTTON_DOWN A7
#define BUTTON_LEFT A3
#define BUTTON_RIGHT A6
#define BUTTON_ENTER A5

#define DISPLAY_RESET 2

#define MAIN_MENU 0
#define CONFIRM_ARRANGEMENT 1
#define SETTINGS 2
#define POWER_OFF 3
#define MAKE_ARRANGEMENT 4
#define ATTACH_DEVICE 5
#define CHOOSE_STRING 6
#define PLUCK_STRING 7
#define WAIT_SCREEN 8
#define COMPLETELY_TUNED 9
#define CHOOSE_HEADSTOCK 10

Bounce up = Bounce();
Bounce down = Bounce();
Bounce left = Bounce();
Bounce right = Bounce();
Bounce enter = Bounce();

Genie genie;

int screen = MAIN_MENU;
int select = CONFIRM_ARRANGEMENT;
bool started = false;
int string = 0;
int stringindex = 6;
int oldstring = 1;
int oldstringindex = 0;
int level = 2;
int tunecounter = 0;
int tunecounterindex = 0;
int allstrings = 1;
int tunedstrings[6] = {0, 0, 0, 0, 0, 0};
int sum = 0;

// ----------------------------------------------------------------
// TUNING VARIABLES
// ----------------------------------------------------------------

#define FORWARD 0
#define REVERSE 1

AudioInputAnalog          adc;
AudioAnalyzeNoteFrequency note;
AudioAnalyzePeak          peak;
AudioConnection           patchCord(adc, note);
AudioConnection           patchCord2(adc, peak);

float f;
float p;

int led_pin = 13;

int silent = 0;

bool string_detuned[6] = {true, true, true, true, true, true};
bool string_calibrated[6] = {true, true, true, true, true, true};
bool string_tuned[6] {true, true, true, true, true, true};

float string_low[6] = {81.94, 109.37, 145.98, 194.87, 245.52, 327.73};
float string_high[6] = {82.89, 110.64, 147.68, 197.14, 248.37, 331.54};

int speed_forward[6] = {100, 100, 100, 100, 100, 100};
int speed_reverse[6] = {100, 100, 100, 100, 100, 100};

bool waited = false;

int memory = 5;

float history[5];

int speed = 10; // will only work once!

int iteration = 0;

// ----------------------------------------------------------------
// MOTOR VARIABLES
// ----------------------------------------------------------------

int motor_pin[6] = {3, 4, 5, 6, 10, 9};

int direction_pin = 11;

// ----------------------------------------------------------------
// SETUP FUNCTION
// ----------------------------------------------------------------

void setup() {

  // set LED pin to output
  pinMode(led_pin, OUTPUT);

  // open serial port
  Serial.begin(9600);

  // delay for serial port initialisation
  delay(1000);

  // set up buttons, display, motors and memory
  buttonSetup();
  displaySetup();
  motorSetup();
  memorySetup();

  // allocate memory to audio library
  AudioMemory(30);
  note.begin(0.15);

}

// ----------------------------------------------------------------
// LOOP FUNCTION
// ----------------------------------------------------------------

void loop() {

  genie.DoEvents();

  if (screen == MAIN_MENU) { //main menu
    menuhighlight();  // returns values of "select" between 1 to 4.
    menuchoice();     // uses returned values of "select" to determine value of "screen", and default "select" value for this screen.
  }

  if (screen == CONFIRM_ARRANGEMENT) { //confirm arrangement
    selectArrangement(); // returns values of "select" of 0 & 5.
    confirmArrangement();    // uses returned values of "select" to determine value of "screen" and default "select" value for this screen.
  }

  if (screen == SETTINGS) {
    settingshighlight();
    settingschoice();
  }

  if (screen == POWER_OFF) { //power off
    poweroff(); //displays splash screen, use enter button to return to main menu.
  }

  if (screen == MAKE_ARRANGEMENT) {  //make new arrangement (UNFINISHED!!!!!!!!!!!!!!!!!!!!!!!!!!!!)
    newarrangehighlight();
  }

  if (screen == ATTACH_DEVICE) { //attach device
    attachhighlight();
    attachchoice();
  }

  if (screen == CHOOSE_STRING) { //choose string
    selectstrhighlight();
    selectstrchoice(); //Can return string number.
    //Set flag here for tuning.

  }

  if (screen == PLUCK_STRING) { //displays request to pluck previously selected string/ next string
    if (started && (silent < 5)) {
      genie.WriteObject(GENIE_OBJ_FORM, 7, 0);
      screen = WAIT_SCREEN;
    }
    if (enter.update()) { //cancel process
      if (enter.read()) {
        genie.WriteObject(GENIE_OBJ_FORM, select, 0); //returns to choose string screen
        screen = select;
      }
    }
  }

  if (screen == WAIT_SCREEN) { //wait screen
    if (string_tuned[string]) { //if tuning code throws up flag denoting successful tuning
      selectNewString();
    } else if (silent > 4) {
      genie.WriteObject(GENIE_OBJ_FORM, 6, 0); //returns user to pluck string screen,
      screen = PLUCK_STRING;
    }

    if (enter.update()) { //user manually cancels process
      if (enter.read()) {
        string_tuned[string] = true;
        screen = select;
        genie.WriteObject(GENIE_OBJ_FORM, screen, 0); //returns to choose string screen
      }
    }
  }

  if (screen == COMPLETELY_TUNED) { //Guitar is fully tuned
    //Confirming the arrangement chosen. By default, the tick button is highlighted as select = 5 (see menuchoice(), if (Screen==1))
    selectFullyTuned();
    confirmFullyTuned();
  }

  if (screen == CHOOSE_HEADSTOCK) {
    selectHeadstock(); //highlights different headstock types
    confirmHeadstock();    //returns back to main menu
  }

  if (sample()) {

    if (!string_detuned[string]) {
      detune(string, REVERSE);
    } else if (!string_calibrated[string]) {
      calibrate(string, FORWARD);
    } else if (!string_tuned[string]) {
      tune(string);
    }

    silent = 0;

  } else {

    motorRun(string, 0);

    silent = silent + 1;

  }

  delay(100);

}

// ----------------------------------------------------------------
// INTERFACE FUNCTIONS
// ----------------------------------------------------------------

void displaySetup() {

  // open serial port
  Serial1.begin(9600);

  // attach display to serial port
  genie.Begin(Serial1);

  // reset display by toggling reset line
  digitalWrite(DISPLAY_RESET, LOW);
  delay(1000);
  digitalWrite(DISPLAY_RESET, HIGH);

}

// ----------------------------------------------------------------

void buttonSetup() {

  // set button pins to outputs
  pinMode(BUTTON_UP, INPUT);
  pinMode(BUTTON_DOWN, INPUT);
  pinMode(BUTTON_LEFT, INPUT);
  pinMode(BUTTON_RIGHT, INPUT);
  pinMode(BUTTON_ENTER, INPUT);
  pinMode(DISPLAY_RESET, OUTPUT);

  // attach button debouncers
  up.attach(BUTTON_UP);
  down.attach(BUTTON_DOWN);
  left.attach(BUTTON_LEFT);
  right.attach(BUTTON_RIGHT);
  enter.attach(BUTTON_ENTER);

  // configure button debouncers
  up.interval(5);
  down.interval(5);
  left.interval(5);
  right.interval(5);
  enter.interval(5);

}

// ----------------------------------------------------------------

void memorySetup() {

  // recover forward speeds from memory
  for (int i = 0; i < 6; i++) {

    speed_forward[i] = EEPROM.read(i);
    speed_reverse[i] = EEPROM.read(i + 6);

    Serial.printf("string %d: +%d / -%d\n", i + 1, speed_forward[i], speed_reverse[i]);

  }

  Serial.printf("\n");

}

// ----------------------------------------------------------------

// highlight option on screen 0
void menuhighlight() {

  if (up.update()) { //highlight "confirm arrangement"
    if (up.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 0, 0);
      select = CONFIRM_ARRANGEMENT; //Defines what Enter button does later on
    }
  }

  if (right.update()) { //highlight "Settings"
    if (right.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 0, 1);
      select = SETTINGS;
    }
  }

  if (down.update()) { //highlight "power off"
    if (down.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 0, 2);
      select = POWER_OFF;
    }
  }

  if (left.update()) { //Highlight "make new arrangement"
    if (left.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 0, 3);
      select = MAKE_ARRANGEMENT;
    }
  }

}

// ----------------------------------------------------------------

// confirm option on screen 0
void menuchoice() {

  if (enter.update()) {
    if (enter.read()) {

      // set new screen
      screen = select;

      // display new screen with default image
      genie.WriteObject(GENIE_OBJ_FORM, screen, 0);
      genie.WriteObject(GENIE_OBJ_USERIMAGES, screen, 0);

      //default selection function
      if (screen == CONFIRM_ARRANGEMENT) {
        select = ATTACH_DEVICE;
      } else if (screen == SETTINGS) {
        level = 0;
        select = MAIN_MENU;
      } else if (screen == POWER_OFF) {
        select = MAIN_MENU;
      } else if (screen == MAKE_ARRANGEMENT) {
        select = MAIN_MENU;
      }

    }
  }

}

// ----------------------------------------------------------------

// select arrangement on screen 1
void selectArrangement() {
  //Confirming the arrangement chosen. By default, the tick button is highlighted as select = 5 (see menuchoice(), if (Screen==1))
  if (right.update()) {
    if (right.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 1, 1);//Highlights Return Button
      select = MAIN_MENU; //selection main menu
    }
  }
  if (left.update()) {
    if (left.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 1, 0); //Highlights Tick Button.
      select = ATTACH_DEVICE;  //selection attach device
    }
  }
}

// ----------------------------------------------------------------

// confirm arrangement selection on screen 1
void confirmArrangement() { //Screen 1
  if (enter.update()) {
    if (enter.read()) {
      screen = select;
      genie.WriteObject(GENIE_OBJ_FORM, screen, 0); //go to form 0(main menu) or form 5 (attach device)
      genie.WriteObject(GENIE_OBJ_USERIMAGES, screen, 0); //Display default image on each form (

      for (int i = 0; i < 6; i++) { //Sets all strings to "Untuned"
        tunedstrings[i] = 0;
      }
      sum = 0;

      if (select == MAIN_MENU) {
        select = CONFIRM_ARRANGEMENT; //default selection is confirm arrangement
      }
      if (select == ATTACH_DEVICE) {
        select = CHOOSE_STRING; //default selection is tick, on Attach device screen
      }
    }
  }
}

// ----------------------------------------------------------------

void settingshighlight() { //Screen 2
  if (down.update()) { //If down button is pressed, goes down a level. Level 1 = Strings, Level 2 = Guitar, Level 3 = Return
    if (down.read()) {
      if (level < 2) {
        level++;
        //highlight object defined by level. Level = 1: Calibrate Guitar, Level = 2: Select Headstock, Level = 3: Return to Main Menu
        genie.WriteObject(GENIE_OBJ_USERIMAGES, 2, level);
        Serial.println(level);
      }
    }
  }
  if (up.update()) { //If up button is pressed, goes up a level. Level 1 = Strings, Level 2 = Guitar, Level 3 = Return
    if (up.read()) {
      if (level > 0) {
        level--;
        genie.WriteObject(GENIE_OBJ_USERIMAGES, 2, level);
        Serial.println(level);
      }
    }
  }
}

void settingschoice() {//Screen 2
  if (enter.update()) {
    if (enter.read()) {

      if (level == 0) {
        //calibrate = 1;
        screen = ATTACH_DEVICE;
        genie.WriteObject(GENIE_OBJ_FORM, screen, 0); //go to form 5 (attach device)
        genie.WriteObject(GENIE_OBJ_USERIMAGES, screen, 0); //Display default image on form

        for (int i = 0; i < 6; i++) { //Sets all strings to "Untuned"
          tunedstrings[i] = 0;
        }
        sum = 0;
        select = CHOOSE_STRING; //default selection is tick, on Attach device screen
      }

      if (level == 1) {
        Serial.println(level);
        genie.WriteObject(GENIE_OBJ_FORM, 9, 0); //Select Headstock Form
        genie.WriteObject(GENIE_OBJ_USERIMAGES, 15, 0);//Highlight Fender Headstock Image
        screen = CHOOSE_HEADSTOCK;
        select = MAIN_MENU; //default selection goes to main menu
      }

      if (level == 2) {//Return to main menu
        Serial.println(level);
        genie.WriteObject(GENIE_OBJ_FORM, 0, 0); //Main menu Form
        genie.WriteObject(GENIE_OBJ_USERIMAGES, 0, 1); //Highlight confirm arrangement
        screen = MAIN_MENU;
        select = SETTINGS; //default selection is confirm arrangement
      }
    }
  }
}

// ----------------------------------------------------------------

void poweroff() { //Screen 3
  if (enter.update()) {
    if (enter.read()) {
      screen = select; //go to selection defined by if statements/ previous screen.
      genie.WriteObject(GENIE_OBJ_FORM, screen, 0);
      genie.WriteObject(GENIE_OBJ_USERIMAGES, screen, 2);
      select = POWER_OFF; //default selection is "Confirm arrangement"
    }
  }
}

// ----------------------------------------------------------------

void newarrangehighlight() { //Screen 4
  if (enter.update()) {
    if (enter.read()) {
      screen = select; //go to selection defined by if statements/ previous screen.
      genie.WriteObject(GENIE_OBJ_FORM, screen, 0);
      genie.WriteObject(GENIE_OBJ_USERIMAGES, screen, 3);
      select = MAKE_ARRANGEMENT; //default selection is "Confirm arrangement"
    }
  }
}

// ----------------------------------------------------------------

void attachhighlight() { //Screen 5
  //Confirming the device has been attached. Tick button is selected by default as select = 6, (see confirmArrangement(), if screen==5)
  if (right.update()) {
    if (right.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 5, 1); //Highlight Return Button
      select = CONFIRM_ARRANGEMENT; //select confirm arrangement
    }
  }
  if (left.update()) {
    if (left.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 5, 0); //Highlight Tick Button
      select = CHOOSE_STRING; //select "choose string(s)"
    }
  }
}

// ----------------------------------------------------------------

void attachchoice() { //Screen 5
  if (enter.update()) {
    if (enter.read()) {
      screen = select; //go to selection defined by if statements
      genie.WriteObject(GENIE_OBJ_FORM, screen, 0); //Go to either Select String (Screen 6), or Confirm Arrangement Screen (Screen 1)

      if (select == CHOOSE_STRING) {
        string = 0; //Highlight 1st string by default when on Level 1
        level = 2; //highlight guitar by default
      }
      if (select == CONFIRM_ARRANGEMENT) {
        select = ATTACH_DEVICE; //highlight tick button on Confirm Arrangement screen (Screen 1)
      }
    }
  }
}

// ----------------------------------------------------------------

void selectstrhighlight() { //Screen 6
  if (down.update()) { //If down button is pressed, goes down a level. Level 1 = Strings, Level 2 = Guitar, Level 3 = Return
    if (down.read()) {
      if (level < 3) {
        level++;
      }
    }
  }
  if (up.update()) { //If up button is pressed, goes up a level. Level 1 = Strings, Level 2 = Guitar, Level 3 = Return
    if (up.read()) {
      if (level > 1) {
        level--;
      }
    }
  }

  if (level == 1) {
    if (right.update()) {
      if (right.read()) {
        oldstring = string; //Remembers what the previous string was, so can highlight it appropriately when not selected
        string++;
        if (string > 5) {
          string = 0;
        }
      }
    }
    if (left.update()) {
      if (left.read()) {
        oldstring = string; //Remembers what the previous string was, so can highlight it appropriately when not selected
        string--;
        if (string < 0) { //Strings are "Wrapped". Pressing left on far left string goes to far right string
          string = 5;
        }
      }
    }

    stringindex = string + 6; //!!!!!!!!!!!!!!!!!!!!THESE MIGHT CHANGE IF MORE IMAGES ARE ADDED EARLIER. CHANGE THE MODIFIER TO FIX IT.
    oldstringindex = oldstring + 6;

    genie.WriteObject(GENIE_OBJ_USERIMAGES, stringindex, 1); //highlights the selected string
    if (tunedstrings[oldstring] == 0) { //if previous highlighted string was not tuned:
      genie.WriteObject(GENIE_OBJ_USERIMAGES, oldstringindex, 0); //unhighlights the previously highlighted string
    }
    else { //else:
      genie.WriteObject(GENIE_OBJ_USERIMAGES, oldstringindex, 2); //highlight the string with green to denote "previously tuned"
    }
    genie.WriteObject(GENIE_OBJ_USERIMAGES, 12, 0); //unhighlight the guitar
    genie.WriteObject(GENIE_OBJ_USERIMAGES, 13, 0); //unhighlight tick button
  }

  if (level == 2) {
    for (int strings = 0; strings < 6; strings++) {
      if (tunedstrings[strings] == 0) {
        genie.WriteObject(GENIE_OBJ_USERIMAGES, strings + 6, 0); //unhighlight every string but tuned ones.
      }
      else {
        genie.WriteObject(GENIE_OBJ_USERIMAGES, strings + 6, 2);
      }
    }
    genie.WriteObject(GENIE_OBJ_USERIMAGES, 12, 1); //highlight the guitar
    genie.WriteObject(GENIE_OBJ_USERIMAGES, 13, 0); //unhighlight return button
  }

  if (level == 3) {
    genie.WriteObject(GENIE_OBJ_USERIMAGES, 12, 0); //unhighlight the guitar
    genie.WriteObject(GENIE_OBJ_USERIMAGES, 13, 1); //highlight the return button
  }
}

// ----------------------------------------------------------------

void selectstrchoice() { //Screen 6. Potentially have this return a string number, not a void.
  if (enter.update()) {
    if (enter.read()) {
      if (level == 1) {
        genie.WriteObject(GENIE_OBJ_USERIMAGES, stringindex, 3); //requests user to pluck string that was highlighted in "selectstrhighlight" func
        genie.WriteObject(GENIE_OBJ_USERIMAGES, 13, 1); //highlights return button
        select = CHOOSE_STRING;
        screen = PLUCK_STRING;
        allstrings = 0;
        string_tuned[string] = false;
      }
      if (level == 2) { //if selecting all strings
        tunecounter = 0; //resets the tuner counter if this option is selected
        for (int i = 0; i < 6; i++) {
          tunedstrings[i] = 0;
        }
        allstringsfunc();
      }
      if (level == 3) {
        genie.WriteObject(GENIE_OBJ_FORM, 1, 0); //returns to confirm arrangement screen
        select = ATTACH_DEVICE; //highlights tick button on confirm arrangement screen by default
        screen = CONFIRM_ARRANGEMENT;
      }
    }
  }
}

// ----------------------------------------------------------------

void selectHeadstock() { //Screen 10
  if (right.update()) {
    if (right.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 15, 1);
      select = SETTINGS;//selection main menu
    }
  }
  if (left.update()) {
    if (left.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 15, 0);
      select = SETTINGS;// selection main menu
    }
  }
}

// ----------------------------------------------------------------

void confirmHeadstock() {//Screen 10
  if (enter.update()) {
    if (enter.read()) {
      screen = select; //go to selection defined by if statements in headstock highlight/ previous screen
      genie.WriteObject(GENIE_OBJ_FORM, screen, 0);
      genie.WriteObject(GENIE_OBJ_USERIMAGES, screen, 0);
      level = 0; //default selection is confirm arrangement
    }
  }
}

// ----------------------------------------------------------------

void selectNewString() {
  genie.WriteObject(GENIE_OBJ_FORM, CHOOSE_STRING, 0); //go back to string selection screen
  if (allstrings == 1) {
    tunedstrings[tunecounter] = 1; //sets value in array specified by tunecounter
    tunecounter++; //increments counter each time this option is selected
    genie.WriteObject(GENIE_OBJ_USERIMAGES, tunecounterindex, 2); //marks previously selected string as tuned
    allstringsfunc();//display request to pluck next string along
  }
  else {
    tunedstrings[string] = 1;
    screen = CHOOSE_STRING;
    //display "choose string" screen, with green highlighted string to denote a tuned string
  }
  sum = 0;
  for (int i = 0 ; i < 6 ; i++) {
    sum = sum + tunedstrings[i];
    Serial.println(tunedstrings[i]);
  }
  if (sum == 6) {
    genie.WriteObject(GENIE_OBJ_FORM, 8, 0); //completely tuned screen
    genie.WriteObject(GENIE_OBJ_USERIMAGES, 14, 0);
    screen = 9;
    select = 0;
  }

  Serial.println(sum);

}

// ----------------------------------------------------------------

void allstringsfunc() {
  tunecounterindex = tunecounter + 6;
  genie.WriteObject(GENIE_OBJ_USERIMAGES, tunecounterindex, 3); //requests user to pluck next string along
  genie.WriteObject(GENIE_OBJ_USERIMAGES, 12, 0); //unhighlights the guitar
  genie.WriteObject(GENIE_OBJ_USERIMAGES, 13, 1); //highlights return button
  select = CHOOSE_STRING;
  screen = PLUCK_STRING;
  allstrings = 1;
}

// ----------------------------------------------------------------

void selectFullyTuned() {
  if (right.update()) {
    if (right.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 14, 1);//Highlights Return Button
      select = CHOOSE_STRING; //selection selectstrings (screen 6)
    }
  }
  if (left.update()) {
    if (left.read()) {
      genie.WriteObject(GENIE_OBJ_USERIMAGES, 14, 0); //Highlights Tick Button.
      select = MAIN_MENU;  //selection main menu (Screen 0)
    }
  }
}

// ----------------------------------------------------------------

void confirmFullyTuned() {
  if (enter.update()) {
    if (enter.read()) {
      genie.WriteObject(GENIE_OBJ_FORM, select, 0); //returns to choose string screen (if select = 6), returns to main menu (if select = 0)
      screen = select;
      sum = 0;
      select = CONFIRM_ARRANGEMENT;
      level = 2;
      string = 0;
      for (int i = 0; i < 6; i++) { //Sets all strings to "Untuned"
        tunedstrings[i] = 0;
      }
    }
  }
}

// ----------------------------------------------------------------
// TUNING FUNCTIONS
// ----------------------------------------------------------------

bool sample() {

  // check note availability
  if (note.available()) {

    // read frequency and peak voltage
    f = note.read();
    p = peak.read() * 1.2;

    // remove mains interference and check peak voltage then raise flag
    if ((f > 55) && (p > 0.3)) {
      return true;
    } else {
      return false;
    }

  } else {
    return false;
  }

}

// ----------------------------------------------------------------

void detune(int string, int direction) {

  digitalWrite(led_pin, LOW);

  // log note and peak voltage
  Serial.printf("detuning: %3.2f Hz (%3.2f V)\n", f, p);

  if ((direction == FORWARD) && (f < string_high[string])) {

    // tigthten string
    motorRun(string, 100);

  } else if ((direction == REVERSE) && (f > string_low[string])) {

    // loosen string
    motorRun(string, -100);

  } else {

    // stop motor
    motorRun(string, 0);

    // raise success flag
    string_detuned[string] = true;

    digitalWrite(led_pin, HIGH);

    // log status
    Serial.printf("detuning: done\n\n");

    delay(1000);

  }

}

// ----------------------------------------------------------------

void calibrate(int string, int direction) {

  float total = 0;
  float average = 0;

  digitalWrite(led_pin, LOW);

  // store frequency reading
  history[iteration] = f;

  Serial.printf("calibrating: %3.2f Hz (%d)\n", f, speed);

  // check for enough values to proceed
  if (iteration >= (memory - 1)) {

    // calculate total
    for (int i = 0; i < memory; i++) {
      total = total + history[i];
    }

    // calculate average
    average = total / float(memory);

    if (direction == FORWARD) {

      if (f < string_high[string]) {

        // check for stalling (no change in frequency)
        if ((abs(f - average) < 0.5) && (speed <= 100)) {

          // increment speed
          speed = speed + 5;

          // set motor speed
          motorRun(string, speed);

          // log status
          Serial.printf("calibrating: stalled\n\n");

        } else {

          // log status
          Serial.printf("calibrating: continue\n\n");

        }

      } else {

        // set motor off
        motorRun(string, 0);

        // save speed
        speed_forward[string] = speed;

        // update EEPROM
        EEPROM.update(string, speed);

        // raise calibrated flag
        string_calibrated[string] = true;

        digitalWrite(led_pin, HIGH);

        // log frequency, averge frequency, and speed and status
        Serial.printf("calibrating: done\n\n");

        // wait
        delay(1000);

      }

    }

    if (direction == REVERSE) {

      // check for movement (small change in frequency)
      if ((abs(f - average) < 0.5) && (speed <= 100)) {

        // increment speed
        speed = speed + 5;

        // set motor speed
        motorRun(string, speed * (-1));

        // log status
        Serial.printf("calibrating: stalled\n\n");

      } else {

        // set motor off
        motorRun(string, 0);

        // save speed
        speed_reverse[string] = speed;

        // update EEPROM
        EEPROM.update(string, speed);

        // raise calibrated flag
        string_calibrated[string] = true;

        digitalWrite(led_pin, HIGH);

        // log frequency, averge frequency, and speed and status
        Serial.printf("calibrating: done\n\n");

        // wait
        delay(1000);

      }

    }

    // reset iteration
    iteration = 0;

  } else {

    // increment iteration
    iteration = iteration + 1; //check if this resets iterations when guitar voltage drops below threshold voltage

  }

}

// ----------------------------------------------------------------

void tune(int string) {

  started = true;

  digitalWrite(led_pin, LOW);

  // log note and peak voltage
  Serial.printf("tuning: %3.2f Hz (%3.2f V)\n", f, p);

  if (f > string_high[string]) {

    // loosen string (over-tuned)
    motorRun(string, speed_reverse[string] * (-1));

    // lower waited flag
    waited = false;

  } else if (f < string_low[string]) {

    // tighten string (under-tuned)
    motorRun(string, speed_forward[string]);

    // lower waited flag
    waited = false;

  } else {

    if (!waited) {

      // set motor off
      motorRun(string, 0);

      // delay for settling
      delay(500);

      // raise waited flag
      waited = true;

      // log status
      Serial.printf("tuning: waiting\n\n");

    } else {

      // reset waited flag
      waited = false;

      // reset started flag
      started = false;

      // raise tuned flag
      string_tuned[string] = true;

      // set LED on
      digitalWrite(led_pin, HIGH);

      // log status
      Serial.printf("tuning: done\n");

      delay(1000);

    }

  }

}

// ----------------------------------------------------------------
// MOTOR FUNCTIONS
// ----------------------------------------------------------------

void motorSetup() {

  // set motor control pins to outputs
  for (int i = 0; i < 6; i++) {
    pinMode(motor_pin[i], OUTPUT);
  }

  // set direction control pin to outputs
  pinMode(direction_pin, OUTPUT);

}

// ----------------------------------------------------------------

void motorRun(int motor, int speed) {

  // define duty cycle and direction variables
  int duty = 0;
  int direction = 0;

  // map power percentage to duty cycle
  if (speed >= 0) {
    duty = map(abs(speed), 0, 100, 0, 255);
    direction = 0;
  } else {
    duty = map(abs(speed), 0, 100, 255, 0);
    direction = 1;
  }

  // set all motors off
  for (int i = 0; i < 6; i++) {
    analogWrite(motor_pin[i], direction * 255);
  }

  // set requested motor on
  analogWrite(motor_pin[motor], duty);

  // set direction pin
  digitalWrite(direction_pin, direction);

}
