#include <TFT9341Touch.h>
#include <ISD4004.h>

// Simulated time variables instead of rtc
const char compile_date[] = __DATE__;
const char compile_time[] = __TIME__;
unsigned long startMillis;
unsigned long lastUpdateMillis;
int currentHour = 0;
int currentMinute = 0;
int currentSecond = 0;

const int LED_PIN = 13;
ISD4004 voice(5, 8); // SS, INT
tft9341touch LcdTouch(10, 9, 7, 2); // CS, DC, TCS, TIRQ

class MedicationSchedule;
MedicationSchedule* currentSchedule = nullptr;
const int lightThreshold = 30;
String recipientNameInput;
uint16_t x, y;
int ButtonNum;
String recipientName;
const int MAX_MEDICATION_TIMES = 3;
int medicationHours[MAX_MEDICATION_TIMES];
int medicationMinutes[MAX_MEDICATION_TIMES];
int boxesPins[MAX_MEDICATION_TIMES] = {A1, A2, A3};
int numMedicationsUsed = 0; // Number of medication hours entered
int checkDurationMinutes = 30; // Default alert duration in minutes
const unsigned long MILLISECONDS_PER_MINUTE = 60000; // 60 seconds * 1000 milliseconds
unsigned long maxPlaybackTime = 30 * MILLISECONDS_PER_MINUTE; // 30 minutes


class MedicationSchedule {
  public:
    MedicationSchedule(const String& recipientName, const int* medicationHours, const int* medicationMinutes, int numMedicationsUsed, int checkDuration)
      : recipientName(recipientName), numMedicationsUsed(numMedicationsUsed), checkDuration(checkDuration), backlogs(0), alarmsOn(0) {
      medicationTimes = new int[numMedicationsUsed];
      isMedicationTaken = new bool[numMedicationsUsed];
      for (int i = 0; i < numMedicationsUsed; ++i) {
        medicationTimes[i] = getTotalMinutes(medicationHours[i], medicationMinutes[i]);
        isMedicationTaken[i] = false;
      }
    }

    ~MedicationSchedule() {
      delete[] medicationTimes;
      delete[] isMedicationTaken;
    }

    //check if its time to take madication
    void checkMedicationTime(int currentHour, int currentMinutes) {
      for (int i = 0; i < numMedicationsUsed; ++i) {
        if (!isMedicationTaken[i]) {
          int currentTime = getTotalMinutes(currentHour, currentMinutes );
          int medicationTime = medicationTimes[i];
          if (isInTimeRange(i, currentTime)) {
            if (wasBoxOpened(i)) {
              setMedicationTaken(i);
            }
            else {
              if (medicationTime == currentTime) {
                activateNotification(i);
              }
              if (medicationTime + checkDuration == currentTime) {
                ++backlogs;
                activateAlarm();
              }
            }
          }
        }
      }
      if(checkForAlarmStop()){
        voice.StopRequested();
      }
      
    }

    void displayOnScreen(String message) {
      LcdTouch.fillScreen(WHITE);
      LcdTouch.print(30, 50, const_cast<char*>(message.c_str()), 2, MAGENTA);
    }

    void resetDay() {
      for (int i = 0; i < numMedicationsUsed; ++i) {
        isMedicationTaken[i] = false;
      }
      if (backlogs == numMedicationsUsed) {
        activateLight();
      }
      else {
        deactivateLight();
      }
      backlogs = 0;
      alarmsOn = 0;
    }

  private:
    String recipientName;
    int* medicationTimes;
    bool* isMedicationTaken;
    int checkDuration;
    int numMedicationsUsed;
    int backlogs;
    int alarmsOn;

    void setMedicationTaken(int index) {
      isMedicationTaken[index] = true;
      deactivateNotification(index);
    }

    //display massege that its time to take the medication
    void activateNotification(int index) {
      voice.PlayInt(0);
      displayOnScreen("Time to take your medication!");
    }

    void deactivateNotification(int index) {
      LcdTouch.fillScreen(BLACK);
    }

    //start alarm
    void activateAlarm() {
      ++alarmsOn;
      voice.PlayLooped(0,maxPlaybackTime,10);
    }

    void deactivateAlarm() {
      --alarmsOn;
    }

    void activateLight() {
      digitalWrite(LED_PIN, HIGH);
    }

    void deactivateLight() {
      digitalWrite(LED_PIN, LOW);
    }

    int getTotalMinutes(int hours, int minutes) {
      return hours * 60 + minutes;
    }

    //check if time is in time range for this box
    bool isInTimeRange(int index, int currentTime) {
      int medicationTime = medicationTimes[index];
      int startTime = medicationTime - checkDuration;
      int endTime = medicationTime + checkDuration;
      return currentTime >= startTime && currentTime <= endTime;
    }

    //check if the box was open (if the madication was taken)
    bool wasBoxOpened(int index) {
      int lightValue = analogRead(boxesPins[index]);
      return lightValue >= lightThreshold;
    }

    //check if box was opend during alarm
    bool checkForAlarmStop() {
      if (alarmsOn == 0) {
        return true;
      }
      for (int i = 0; i < numMedicationsUsed; ++i) {
        if (wasBoxOpened(i)) {
        deactivateAlarm();
        }
      }
      return false;
    }
    
};


void parseCompileDateTime() {
  char month[4];
  int day, year;
  sscanf(compile_date, "%s %d %d", month, &day, &year);
  sscanf(compile_time, "%d:%d:%d", &currentHour, &currentMinute, &currentSecond);
}

void updateTime() {
  currentSecond++;
  if (currentSecond >= 60) {
    currentSecond = 0;
    currentMinute++;
    if (currentMinute >= 60) {
      currentMinute = 0;
      currentHour++;
      if (currentHour >= 24) {
        currentHour = 0;
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  parseCompileDateTime();
  startMillis = millis();
  lastUpdateMillis = startMillis;

  LcdTouch.begin();
  LcdTouch.setRotation(0);
  LcdTouch.setTextSize(2);
  LcdTouch.set(3780, 372, 489, 3811); // Calibration
  pinMode(LED_PIN, OUTPUT);
  screenMain();

}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastUpdateMillis >= 1000) {
    updateTime();
    lastUpdateMillis = currentMillis;

    if (currentSchedule != nullptr) {
      currentSchedule->checkMedicationTime(currentHour, currentMinute);
    }

    if (currentHour == 0 && currentMinute == 0 && currentSecond == 0) {
      if (currentSchedule != nullptr) {
        currentSchedule->resetDay();
      }
    }
  }
}

void screenMain() {
  ButtonNum = 0;
  Serial.println("Updating main screen...");
  LcdTouch.fillScreen(WHITE);
  LcdTouch.print(30, 50, const_cast<char*>("Medication Reminder"), 2, MAGENTA);
  // Display button to create new medication schedule
  LcdTouch.drawButton(1, 40, 100, 300, 50, 10, CYAN, WHITE, const_cast<char*>("Create New Schedule"), 2);
  // Add new button for sound recording
  LcdTouch.drawButton(2, 40, 170, 300, 50, 10, CYAN, WHITE, const_cast<char*>("Record Alarm Sound"), 2);

  do {
    if (LcdTouch.touched()) {
      LcdTouch.readTouch();
      x = LcdTouch.xTouch;
      y = LcdTouch.yTouch;
      ButtonNum = LcdTouch.ButtonTouch(x, y);
    }
  } while (ButtonNum != 1 && ButtonNum != 2);

  if (ButtonNum == 1) {
    screenAddName();
  } else if (ButtonNum == 2) {
    screenRecordSound();
  }
}


void screenRecordSound() {
  LcdTouch.fillScreen(WHITE);
  LcdTouch.print(30, 30, const_cast<char*>("Record Alarm Sound"), 2, MAGENTA);

  // Draw record button
  LcdTouch.drawButton(1, 40, 80, 260, 50, 10, RED, WHITE, const_cast<char*>("Start Recording"), 2);
  // Draw play button
  LcdTouch.drawButton(2, 40, 150, 260, 50, 10, GREEN, WHITE, const_cast<char*>("Play Recording"), 2);
  // Draw back button
  LcdTouch.drawButton(3, 40, 220, 260, 50, 10, BLUE, WHITE, const_cast<char*>("Back to Main"), 2);

  bool isRecording = false;
  unsigned long recordStartTime = 0;
  const unsigned long MAX_RECORD_TIME = 10000; // 10 seconds max recording time

  while (true) {
    if (LcdTouch.touched()) {
      LcdTouch.readTouch();
      x = LcdTouch.xTouch;
      y = LcdTouch.yTouch;
      int ButtonNum = LcdTouch.ButtonTouch(x, y);

      switch (ButtonNum) {
        case 1: // Start/Stop Recording
          if (!isRecording) {
            isRecording = true;
            recordStartTime = millis();
            voice.StartRecord(0); // Start recording at address 0
            LcdTouch.drawButton(1, 40, 80, 260, 50, 10, RED, WHITE, const_cast<char*>("Stop Recording"), 2);
          } else {
            isRecording = false;
            voice.StopRecord();
            LcdTouch.drawButton(1, 40, 80, 260, 50, 10, RED, WHITE, const_cast<char*>("Start Recording"), 2);
          }
          break;
        case 2: // Play Recording
          voice.PlayInt(0); // Play recording from address 0
          break;
        case 3: // Back to Main
          screenMain();
          return;
      }
    }

    // Check if max recording time is reached
    if (isRecording && (millis() - recordStartTime >= MAX_RECORD_TIME)) {
      isRecording = false;
      voice.StopRecord();
      LcdTouch.drawButton(1, 40, 80, 260, 50, 10, RED, WHITE, const_cast<char*>("Start Recording"), 2);
    }
  }
}


void drawNameInputButtons() {
  // Draw character buttons
  int buttonWidth = 25;
  int buttonHeight = 25;
  int buttonMarginX = 5;
  int buttonMarginY = 5;
  int rows = 3;
  int cols = 9;
  int buttonIndex = 2;
  char character = 'A';
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      //            int buttonIndex = row * cols + col;
      if (buttonIndex >= 28) break;
      String buttonText = String(character);
      int buttonX = buttonMarginX + col * (buttonWidth + buttonMarginX);
      int buttonY = 40 + row * (buttonHeight + buttonMarginY);
      LcdTouch.drawButton(buttonIndex + 1, buttonX, buttonY, buttonWidth, buttonHeight, 10, CYAN, WHITE, const_cast<char*>(buttonText.c_str()), 2);
      character++;
      buttonIndex++;
    }

  }

  // Space button
  LcdTouch.drawButton(1, buttonMarginX + (cols - 1) * (buttonWidth + buttonMarginX), 40 + 2 * (buttonHeight + buttonMarginY), buttonWidth, buttonHeight, 10, CYAN, WHITE, const_cast<char*>(" "), 2);

  // "Set Hours" button
  LcdTouch.drawButton(2, 80, 180, 150, 50, 10, CYAN, WHITE, const_cast<char*>("set Hours"), 2);
}

void screenAddName() {
  const int maxChars = 10; // Maximum characters allowed

  LcdTouch.fillScreen(WHITE);
  LcdTouch.print(30, 10, const_cast<char*>("Enter Recipient Name"), 2, MAGENTA);

  drawNameInputButtons();

  // Input area
  LcdTouch.drawRect(30, 130, 250, 30, BLACK);

  do {
    if (LcdTouch.touched()) {
      LcdTouch.readTouch();
      x = LcdTouch.xTouch;
      y = LcdTouch.yTouch;
      ButtonNum = LcdTouch.ButtonTouch(x, y);
      Serial.println(ButtonNum);

      if (ButtonNum >= 3 && ButtonNum <= 28) {
        Serial.println("char: ");
        Serial.println(ButtonNum);
        char pressedChar = (char)('A' + ButtonNum - 3);
        if (recipientNameInput.length() < maxChars) {
          recipientNameInput += pressedChar;
          LcdTouch.fillRect(31, 151, 248, 38, WHITE);
          LcdTouch.print(35, 160, const_cast<char*>(recipientNameInput.c_str()), 2, BLACK);
        }
      } else if (ButtonNum == 1) { // Space
        Serial.println("space: ");
        Serial.println(ButtonNum);
        if (recipientNameInput.length() < maxChars) {
          recipientNameInput += " ";
          LcdTouch.fillRect(31, 151, 248, 38, WHITE);
          LcdTouch.print(35, 160, const_cast<char*>(recipientNameInput.c_str()), 2, BLACK);
        }
      }
    }
  } while (ButtonNum != 2); // Set Hours
  Serial.println("end: ");
  Serial.println(ButtonNum);
  recipientName = recipientNameInput;
  Serial.println(recipientName);
  screenAddHours();
}




int mone1 = 0; // For minutes
int mone2 = 0; // For hours

void screenAddHours() {
  LcdTouch.fillScreen(WHITE);
  LcdTouch.print(30, 10, const_cast<char*>("Add Medication Hours"), 2, MAGENTA);

  // Draw buttons
  LcdTouch.drawButton(1, 148, 38, 30, 20, 10, BLUE, WHITE, "h+", 2);
  LcdTouch.drawButton(2, 148, 134, 30, 20, 10, BLUE, WHITE, "h-", 2);
  LcdTouch.drawButton(3, 237, 38, 30, 20, 10, BLUE, WHITE, "m+", 2);
  LcdTouch.drawButton(4, 237, 134, 30, 20, 10, BLUE, WHITE, "m-", 2);

  // Draw rectangles
  LcdTouch.fillRect(225, 65, 50, 60, CYAN);
  LcdTouch.fillRect(137, 65, 50, 60, CYAN);

  // Additional buttons
  LcdTouch.drawButton(5, 15, 170, 150, 50, 10, CYAN, WHITE, const_cast<char*>("Add Time"), 2);
  LcdTouch.drawButton(6, 180, 170, 150, 50, 10, CYAN, WHITE, const_cast<char*>("Finish"), 2);

  updateTimeDisplay();
  updateAddedTimesDisplay();

  while (true) {
    if (LcdTouch.touched()) {
      x, y;
      LcdTouch.readTouch();
      x = LcdTouch.xTouch;
      y = LcdTouch.yTouch;
      int ButtonNum = LcdTouch.ButtonTouch(x, y);

      switch (ButtonNum) {
        case 1: // Hours +
          mone2++;
          if (mone2 > 23) mone2 = 23;
          break;
        case 2: // Hours -
          mone2--;
          if (mone2 < 0) mone2 = 0;
          break;
        case 3: // Minutes +
          mone1++;
          if (mone1 > 59) mone1 = 59;
          break;
        case 4: // Minutes -
          mone1--;
          if (mone1 < 0) mone1 = 0;
          break;
        case 5: // Add Time
          if (numMedicationsUsed < MAX_MEDICATION_TIMES) {
            medicationHours[numMedicationsUsed] = mone2;
            medicationMinutes[numMedicationsUsed] = mone1;
            numMedicationsUsed++;
            updateAddedTimesDisplay();
          } else {
            LcdTouch.print(25, 220, "Max times reached", 2, RED, WHITE);
            delay(1000);
            LcdTouch.fillRect(25, 220, 280, 30, WHITE); // Clear message
          }
          break;
        case 6: // Finish
          if (numMedicationsUsed > 0) {
            screenSetcheckDuration();
            return;
          } else {
            LcdTouch.print(25, 220, "Add at least one time", 2, RED, WHITE);
            delay(1000);
            LcdTouch.fillRect(25, 220, 280, 30, WHITE); // Clear message
          }
          break;
      }

      updateTimeDisplay();
      delay(200); // Debounce delay
    }
  }
}

void updateTimeDisplay() {
  char str[6];
  char str1[3];
  char str2[3];

  sprintf(str, "%02d:%02d", mone2, mone1);
  sprintf(str1, "%02d", mone1);
  sprintf(str2, "%02d", mone2);

  LcdTouch.print(25, 100, str, 2, BLACK, CYAN);
  LcdTouch.print(245, 85, str1, 2, BLACK, CYAN);
  LcdTouch.print(155, 85, str2, 2, BLACK, CYAN);
}

void updateAddedTimesDisplay() {
  LcdTouch.fillRect(25, 220, 280, 60, WHITE); // Clear previous times display
  for (int i = 0; i < numMedicationsUsed; i++) {
    char timeStr[10];
    sprintf(timeStr, "%02d:%02d", medicationHours[i], medicationMinutes[i]);
    LcdTouch.print(25 + (i * 70), 220, timeStr, 2, BLACK, WHITE);
  }
}


void screenSetcheckDuration() {

  LcdTouch.fillScreen(WHITE);
  LcdTouch.print(30, 30, const_cast<char*>("Set Alert Duration"), 2, MAGENTA);

  // Draw buttons for increasing and decreasing duration
  LcdTouch.drawButton(1, 40, 80, 100, 50, 10, BLUE, WHITE, "+", 2);
  LcdTouch.drawButton(2, 200, 80, 100, 50, 10, BLUE, WHITE, "-", 2);

  // Draw finish create schedule button
  LcdTouch.drawButton(3, 40, 170, 260, 50, 10, CYAN, WHITE, const_cast<char*>("Finish Create Schedule"), 1);

  updatecheckDurationDisplay(checkDurationMinutes);

  while (true) {
    if (LcdTouch.touched()) {
      uint16_t x, y;
      LcdTouch.readTouch();
      x = LcdTouch.xTouch;
      y = LcdTouch.yTouch;
      int ButtonNum = LcdTouch.ButtonTouch(x, y);

      switch (ButtonNum) {
        case 1: // Increase duration
          checkDurationMinutes += 5;
          if (checkDurationMinutes > 60) checkDurationMinutes = 60; // Max 1 hours
          break;
        case 2: // Decrease duration
          checkDurationMinutes -= 5;
          if (checkDurationMinutes < 5) checkDurationMinutes = 5; // Min 5 minutes
          break;
        case 3: // Finish Create Schedule
          // Save the schedule and return to main screen
          saveSchedule();
          screenMain();
      }

      updatecheckDurationDisplay(checkDurationMinutes);

    }
  }
}

void updatecheckDurationDisplay(int duration) {
  char str[20];
  sprintf(str, "Duration: %d min", duration);
  LcdTouch.fillRect(40, 140, 260, 30, WHITE); // Clear previous duration display
  LcdTouch.print(40, 140, str, 2, BLACK, WHITE);
}


void resetGlobal() {
  recipientNameInput = "";
  recipientName = "";
  numMedicationsUsed = 0;
  checkDurationMinutes = 30; // Reset to default value

  // Reset medication times
  for (int i = 0; i < MAX_MEDICATION_TIMES; i++) {
    medicationHours[i] = 0;
    medicationMinutes[i] = 0;
  }

  // Reset screen-specific variables
  mone1 = 0; // For minutes
  mone2 = 0; // For hours
}


void saveSchedule() {
  Serial.println(recipientName);
  Serial.println(numMedicationsUsed);
  Serial.println(checkDurationMinutes);
  if (currentSchedule != nullptr) {
    delete currentSchedule;
    currentSchedule = nullptr;
  }

  currentSchedule = new MedicationSchedule(recipientName, medicationHours, medicationMinutes, numMedicationsUsed, checkDurationMinutes);
  resetGlobal();
}
