#include <avr/wdt.h>
#include <EEPROM.h>
#include <EEWrap.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ErriezLCDKeypadShield.h>
#include <elapsedMillis.h>

// Define Pins
#define PIN_BUZZ 3
#define ONE_WIRE_BUS 2
#define RELAY1 A2
#define RELAY2 A1
#define RELAY3 A4
#define RELAY4 A3

#define TEMPERATURE_PRECISION 12

// Initilize Libraries
OneWire oneWire(ONE_WIRE_BUS);       // Setup a oneWire instance to communicate with any OneWire device
DallasTemperature sensors(&oneWire); // Pass oneWire reference to DallasTemperature library
elapsedMillis timeElapsed;           //declare global if you don't want it reset every time loop runs
elapsedMillis timeoutElapsed;           //declare global if you don't want it reset every time loop runs
LCDKeypadShield shield;

unsigned int interval = 1000;   // delay in milliseconds between reading the sensors.
unsigned int timeout = 10000;   // delay in milliseconds between reading the sensors.
int deviceCount = 0;
byte lastKey = 0;
byte currentKey = 0;
int selectRow = 0;
int selectMenu = 0;
byte displayMode = 0;
bool flash;

// Local Variables to store the current temperature
float tempCH1;
float tempCH2;
float tempCH3;
float tempCH4;

// Local Variables for Sensor Addresses (To save on EEPROM wear)
DeviceAddress sensorA;
DeviceAddress sensorB;
DeviceAddress sensorC;
DeviceAddress sensorD;

// EEPROM (EEWrap) Variables for Sensor Addresses
uint8_e addr1[8] EEMEM;
uint8_e addr2[8] EEMEM;
uint8_e addr3[8] EEMEM;
uint8_e addr4[8] EEMEM;

// EEPROM (EEWrap) Variables for Set Temperatures
float_e setTempCH1 EEMEM;
float_e setTempCH2 EEMEM;
float_e setTempCH3 EEMEM;
float_e setTempCH4 EEMEM;

// EEPROM (EEWrap) Variables for Settings
bool_e firstRun EEMEM;
bool_e isCelsius EEMEM;
float_e differential EEMEM;
uint8_e firstRunStep EEMEM;
uint8_e highAlarm EEMEM;
uint8_e lowAlarm EEMEM;

void setup(void)
{
  // Enable Watchdog Timer
  watchdogSetup();
  
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);

  sensors.begin();  // Start up the library
  sensors.setResolution(TEMPERATURE_PRECISION);
  shield.backlightOn();

  if (analogRead(0) < 800) {
    shield.setCursor(0, 0);  // First character first line
    shield.print(F("Resetting..."));
    firstRun = false;
    isCelsius = false;
    differential = 0.5;
    firstRunStep = 0;
    highAlarm = 82;
    lowAlarm = 74;

    setTempCH1 = 78.0;
    setTempCH2 = 78.0;
    setTempCH3 = 78.0;
    setTempCH4 = 78.0;

    delay(1000);
    shield.setCursor(0, 1);  // First character second line
    shield.print(F("Reset Complete!"));
    delay(3000);
    shield.clear();
  }
  if (firstRunStep == 0) {
    shield.setCursor(0, 0);  // First character first line
    shield.print(F("Locating sensors"));
    shield.setCursor(0, 1);  // First character second line
    shield.print(F("Found: "));
    shield.setCursor(6, 1);
    deviceCount = sensors.getDeviceCount();
    shield.print(deviceCount, DEC);

    tone(PIN_BUZZ, 2000, 1000);
    delay(3000);
    shield.clear();
  }

  // Pull all the Sensor Addresses from EEPROM (EEWrap)
  for ( int i = 0 ; i < 8 ; ++i ) {
    sensorA[ i ] = addr1[ i ];
  }
  for ( int i = 0 ; i < 8 ; ++i ) {
    sensorB[ i ] = addr2[ i ];
  }
  for ( int i = 0 ; i < 8 ; ++i ) {
    sensorC[ i ] = addr3[ i ];
  }
  for ( int i = 0 ; i < 8 ; ++i ) {
    sensorD[ i ] = addr4[ i ];
  }

  if (!firstRun) {
    FirstRun();
  }
  sensors.setWaitForConversion(false);
}

void loop() {
  wdt_reset();                  // Resets the Watchdog Timer.
  if (timeElapsed > interval)
  {
    if (displayMode == 0) {
      getTemp();
      processAlarms();
      processRelays();              // Process the relays (temperature controlled).
    }
    updateDisplay();              // Updates the display for the menu system.
    timeElapsed = 0;              // reset the counter to 0 so the counting starts over...
  }

  if (timeoutElapsed > timeout)
  {
    if (displayMode > 0) {
      displayMode = 0;
      selectMenu = 0;
    }
    interval = 1000;
    timeoutElapsed = 0;              // reset the counter to 0 so the counting starts over...
  }

  // Read buttons
  currentKey = shield.getButtons();
  if (currentKey != lastKey) {
    switch (currentKey) {
      case ButtonRight:
        tone(PIN_BUZZ, 3000, 50);
        if (displayMode == 0 || displayMode == 1) {
          selectRow++;
          if (selectRow > 3)  selectRow = 0;
          interval = 500;
          displayMode = 1;
          timeoutElapsed = 0;
          flash = true;
          updateDisplay();
        } else if (displayMode == 2) {
          interval = 500;
          flash = false;
          timeElapsed = 0;
          timeoutElapsed = 0;
          buttonPress(ButtonRight);
          updateDisplay();
        }
        break;
      case ButtonLeft:
        tone(PIN_BUZZ, 3000, 50);
        if (displayMode == 0 || displayMode == 1) {
          selectRow--;
          if (selectRow < 0)  selectRow = 3;
          interval = 500;
          displayMode = 1;
          timeoutElapsed = 0;
          flash = true;
          updateDisplay();
        } else if (displayMode == 2) {
          interval = 500;
          flash = false;
          timeElapsed = 0;
          timeoutElapsed = 0;
          buttonPress(ButtonLeft);
          updateDisplay();
        }
        break;
      case ButtonUp:
        tone(PIN_BUZZ, 3000, 50);
        if (displayMode == 0 || displayMode == 1) {
          tempUp();
          interval = 500;
          flash = false;
          timeElapsed = 0;
          updateDisplay();
        } else if (displayMode == 2) {
          interval = 500;
          flash = false;
          timeElapsed = 0;
          timeoutElapsed = 0;
          buttonPress(ButtonUp);
          updateDisplay();
        }
        break;
      case ButtonDown:
        tone(PIN_BUZZ, 3000, 50);
        if (displayMode == 0 || displayMode == 1) {
          tempDown();
          interval = 500;
          flash = false;
          timeElapsed = 0;
          updateDisplay();
        } else if (displayMode == 2) {
          interval = 500;
          flash = false;
          timeElapsed = 0;
          timeoutElapsed = 0;
          buttonPress(ButtonDown);
          updateDisplay();
        }
        break;
      case ButtonSelect:
        tone(PIN_BUZZ, 3000, 50);
        if (displayMode == 0) {
          displayMode = 2;
          interval = 500;
          flash = false;
          timeElapsed = 0;
          timeoutElapsed = 0;
          updateDisplay();
        } else if (displayMode == 1) {
          displayMode = 0;
          selectMenu = 0;
          interval = 1000;
        } else if (displayMode == 2) {
          interval = 500;
          flash = false;
          timeElapsed = 0;
          timeoutElapsed = 0;
          buttonPress(ButtonSelect);
          updateDisplay();
        }
        break;
      case ButtonNone:
        break;
      default:
        break;
    }
    lastKey = currentKey;
  }
}



void watchdogSetup(void) {
  cli();
  wdt_reset();
  /*
    WDTCSR configuration:
    WDIE = 1: Interrupt Enable
    WDE = 1 :Reset Enable
    See table for time-out variations:
    WDP3 = 0 :For 1000ms Time-out
    WDP2 = 1 :For 1000ms Time-out
    WDP1 = 1 :For 1000ms Time-out
    WDP0 = 0 :For 1000ms Time-out
  */
  // Enter Watchdog Configuration mode:
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  // Set Watchdog settings:
  WDTCSR = (0 << WDIE) | (1 << WDE) |
           (1 << WDP3) | (0 << WDP2) | (0 << WDP1) |
           (1 << WDP0);

  sei();
}



void buttonPress(int x) {
  if (x == ButtonUp) {
    selectMenu--;
    if (selectMenu < 0) selectMenu = 0;
  }

  if (x == ButtonDown) {
    selectMenu++;
    if (selectMenu > 7) selectMenu = 7;
  }

  if (x == ButtonRight) {
    if (selectMenu == 0) {
      isCelsius = !isCelsius;
      convertSetTemps();
    } else if (selectMenu == 1) {
      differential += 0.1;
      if (differential > 4.0) differential = 4.0;
    } else if (selectMenu == 2) {
      lowAlarm++;
      if (isCelsius) {
        if (lowAlarm > 26) lowAlarm = 26;
      } else {
        if (lowAlarm > 80) lowAlarm = 80;
      }
    } else if (selectMenu == 3) {
      highAlarm++;
      if (isCelsius) {
        if (highAlarm > 33) highAlarm = 33;
      } else {
        if (highAlarm > 90) highAlarm = 90;
      }
    }
  }

  if (x == ButtonLeft) {
    if (selectMenu == 0) {
      isCelsius = !isCelsius;
      convertSetTemps();
    } else if (selectMenu == 1) {
      differential -= 0.1;
      if (differential < 0.2) differential = 0.2;
    } else if (selectMenu == 2) {
      lowAlarm--;
      if (isCelsius) {
        if (lowAlarm < 12) lowAlarm = 12;
      } else {
        if (lowAlarm < 55) lowAlarm = 55;
      }
    } else if (selectMenu == 3) {
      highAlarm--;
      if (isCelsius) {
        if (highAlarm < 18) highAlarm = 18;
      } else {
        if (highAlarm < 65) highAlarm = 65;
      }
    }
  }

  if (x == ButtonSelect) {
    if (selectMenu == 0) {
      isCelsius = !isCelsius;
      convertSetTemps();
    } else if (selectMenu == 4) {
      hardReset();
    } else if (selectMenu == 5) {
      testRelays();
    } else if (selectMenu == 6) {
      firstRun = false;
      firstRunStep = 0;
      UnplugSensors();
      FirstRun();
    } else if (selectMenu == 7) {
      displayMode = 0;
      selectMenu = 0;
      interval = 1000;
    }
  }
}


void updateDisplay() {
  if (displayMode == 1) {
    if (selectRow == 0) {
      shield.clear();
      if (!flash) {
        shield.setCursor(0, 0);
        shield.print(setTempCH1);
        flash = true;
      } else {
        flash = false;
      }
      shield.setCursor(8, 0);
      shield.print(setTempCH2);
      shield.setCursor(0, 1);
      shield.print(setTempCH3);
      shield.setCursor(8, 1);
      shield.print(setTempCH4);
    }

    if (selectRow == 1) {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(setTempCH1);
      if (!flash) {
        shield.setCursor(8, 0);
        shield.print(setTempCH2);
        flash = true;
      } else {
        flash = false;
      }
      shield.setCursor(0, 1);
      shield.print(setTempCH3);
      shield.setCursor(8, 1);
      shield.print(setTempCH4);
    }

    if (selectRow == 2) {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(setTempCH1);
      shield.setCursor(8, 0);
      shield.print(setTempCH2);
      if (!flash) {
        shield.setCursor(0, 1);
        shield.print(setTempCH3);
        flash = true;
      } else {
        flash = false;
      }
      shield.setCursor(8, 1);
      shield.print(setTempCH4);
    }

    if (selectRow == 3) {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(setTempCH1);
      shield.setCursor(8, 0);
      shield.print(setTempCH2);
      shield.setCursor(0, 1);
      shield.print(setTempCH3);
      if (!flash) {
        shield.setCursor(8, 1);
        shield.print(setTempCH4);
        flash = true;
      } else {
        flash = false;
      }
    }



  } else if (displayMode == 2) {
    if (selectMenu == 0) {
      shield.clear();
      shield.setCursor(0, 0);
      if (!flash) {
        if (isCelsius) shield.print(F("Unit: Celsius"));
        else shield.print(F("Unit: Fahrenheit"));
        flash = true;
      } else {
        flash = false;
      }
      shield.setCursor(0, 1);
      shield.print(F("Temp Swing:"));
      shield.setCursor(12, 1);
      shield.print(differential);
    }

    if (selectMenu == 1) {
      shield.clear();
      shield.setCursor(0, 0);
      if (isCelsius) shield.print(F("Unit: Celsius"));
      else shield.print(F("Unit: Fahrenheit"));
      shield.setCursor(0, 1);
      if (!flash) {
        shield.print(F("Temp Swing:"));
        shield.setCursor(12, 1);
        shield.print(differential);
        flash = true;
      } else {
        flash = false;
      }
    }

    if (selectMenu == 2) {
      shield.clear();
      shield.setCursor(0, 0);
      if (!flash) {
        shield.print(F("Low Alarm:"));
        shield.setCursor(11, 0);
        shield.print(lowAlarm);
        flash = true;
      } else {
        flash = false;
      }
      shield.setCursor(0, 1);
      shield.print(F("High Alarm:"));
      shield.setCursor(12, 1);
      shield.print(highAlarm);
    }

    if (selectMenu == 3) {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(F("Low Alarm:"));
      shield.setCursor(11, 0);
      shield.print(lowAlarm);
      shield.setCursor(0, 1);
      if (!flash) {
        shield.print(F("High Alarm:"));
        shield.setCursor(12, 1);
        shield.print(highAlarm);
        flash = true;
      } else {
        flash = false;
      }
    }

    if (selectMenu == 4) {
      shield.clear();
      shield.setCursor(0, 0);
      if (!flash) {
        shield.print(F("Hard Reset"));
        flash = true;
      } else {
        flash = false;
      }
      shield.setCursor(0, 1);
      shield.print(F("Test Outlets"));
    }

    if (selectMenu == 5) {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(F("Hard Reset"));
      shield.setCursor(0, 1);
      if (!flash) {
        shield.print(F("Test Outlets"));
        flash = true;
      } else {
        flash = false;
      }
    }

    if (selectMenu == 6) {
      shield.clear();
      shield.setCursor(0, 0);
      if (!flash) {
        shield.print(F("Setup Sensors"));
        flash = true;
      } else {
        flash = false;
      }
      shield.setCursor(0, 1);
      shield.print(F("Exit Menu"));
    }

    if (selectMenu == 7) {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(F("Setup Sensors"));
      shield.setCursor(0, 1);
      if (!flash) {
        shield.print(F("Exit Menu"));
        flash = true;
      } else {
        flash = false;
      }
    }
  }
}


void tempUp() {
  if (selectRow == 0) {
    if (displayMode == 1) {
      timeoutElapsed = 0;
      setTempCH1 += 0.1;
      if (isCelsius) {
        if (setTempCH1 > 33) setTempCH1 = 33;
      } else {
        if (setTempCH1 > 92) setTempCH1 = 92;
      }
    } else {
      displayMode = 1;
    }
  }

  if (selectRow == 1) {
    if (displayMode == 1) {
      timeoutElapsed = 0;
      setTempCH2 += 0.1;
      if (isCelsius) {
        if (setTempCH2 > 33) setTempCH2 = 33;
      } else {
        if (setTempCH2 > 92) setTempCH2 = 92;
      }
    } else {
      displayMode = 1;
    }
  }

  if (selectRow == 2) {
    if (displayMode == 1) {
      timeoutElapsed = 0;
      setTempCH3 += 0.1;
      if (isCelsius) {
        if (setTempCH3 > 33) setTempCH3 = 33;
      } else {
        if (setTempCH3 > 92) setTempCH3 = 92;
      }
    } else {
      displayMode = 1;
    }
  }

  if (selectRow == 3) {
    if (displayMode == 1) {
      timeoutElapsed = 0;
      setTempCH4 += 0.1;
      if (isCelsius) {
        if (setTempCH4 > 33) setTempCH4 = 33;
      } else {
        if (setTempCH4 > 92) setTempCH4 = 92;
      }
    } else {
      displayMode = 1;
    }
  }
}


void tempDown() {
  if (selectRow == 0) {
    if (displayMode == 1) {
      timeoutElapsed = 0;
      setTempCH1 -= 0.1;
      if (isCelsius) {
        if (setTempCH1 < 15) setTempCH1 = 15;
      } else {
        if (setTempCH1 < 60) setTempCH1 = 60;
      }
    } else {
      displayMode = 1;
    }
  }

  if (selectRow == 1) {
    if (displayMode == 1) {
      timeoutElapsed = 0;
      setTempCH2 -= 0.1;
      if (isCelsius) {
        if (setTempCH2 < 15) setTempCH2 = 15;
      } else {
        if (setTempCH2 < 60) setTempCH2 = 60;
      }
    } else {
      displayMode = 1;
    }
  }

  if (selectRow == 2) {
    if (displayMode == 1) {
      timeoutElapsed = 0;
      setTempCH3 -= 0.1;
      if (isCelsius) {
        if (setTempCH3 < 15) setTempCH3 = 15;
      } else {
        if (setTempCH3 < 60) setTempCH3 = 60;
      }
    } else {
      displayMode = 1;
    }
  }

  if (selectRow == 3) {
    if (displayMode == 1) {
      timeoutElapsed = 0;
      setTempCH4 -= 0.1;
      if (isCelsius) {
        if (setTempCH4 < 15) setTempCH4 = 15;
      } else {
        if (setTempCH4 < 60) setTempCH4 = 60;
      }
    } else {
      displayMode = 1;
    }
  }
}



void getTemp() {
  sensors.requestTemperatures();
  shield.clear();

  if (sensorA[0] != 0) {
    if (isCelsius) tempCH1 = sensors.getTempC(sensorA);
    else tempCH1 = sensors.getTempF(sensorA);
    shield.setCursor(0, 0);

    if (tempCH1 == DEVICE_DISCONNECTED_C || tempCH1 == DEVICE_DISCONNECTED_F) {
      shield.print(F("???"));
    } else if (tempCH1 == 185) {
      shield.print(F("INIT"));
    } else {
      shield.print(tempCH1);
    }

  } else {
    shield.setCursor(0, 0);
    shield.print(F("---"));
  }


  if (sensorB[0] != 0) {
    if (isCelsius) tempCH2 = sensors.getTempC(sensorB);
    else tempCH2 = sensors.getTempF(sensorB);
    shield.setCursor(8, 0);

    if (tempCH2 == DEVICE_DISCONNECTED_C || tempCH2 == DEVICE_DISCONNECTED_F) {
      shield.print(F("???"));
    } else if (tempCH2 == 185) {
      shield.print(F("INIT"));
    } else {
      shield.print(tempCH2);
    }

  } else {
    shield.setCursor(8, 0);
    shield.print(F("---"));
  }


  if (sensorC[0] != 0) {
    if (isCelsius) tempCH3 = sensors.getTempC(sensorC);
    else tempCH3 = sensors.getTempF(sensorC);
    shield.setCursor(0, 1);

    if (tempCH3 == DEVICE_DISCONNECTED_C || tempCH3 == DEVICE_DISCONNECTED_F) {
      shield.print(F("???"));
    } else if (tempCH3 == 185) {
      shield.print(F("INIT"));
    } else {
      shield.print(tempCH3);
    }

  } else {
    shield.setCursor(0, 1);
    shield.print(F("---"));
  }


  if (sensorD[0] != 0) {
    if (isCelsius) tempCH4 = sensors.getTempC(sensorD);
    else tempCH4 = sensors.getTempF(sensorD);
    shield.setCursor(8, 1);

    if (tempCH4 == DEVICE_DISCONNECTED_C || tempCH4 == DEVICE_DISCONNECTED_F) {
      shield.print(F("???"));
    } else if (tempCH4 == 185) {
      shield.print(F("INIT"));
    } else {
      shield.print(tempCH4);
    }

  } else {
    shield.setCursor(8, 1);
    shield.print(F("---"));
  }
}



void UnplugSensors () {
  sensors.begin();
  deviceCount = sensors.getDeviceCount();
  if (deviceCount > 0) {
    shield.clear();
    shield.setCursor(0, 0);
    shield.print(F("Please unplug"));
    shield.setCursor(0, 1);
    shield.print(F("all temp sensors"));

    while (deviceCount > 0) {
      sensors.begin();
      deviceCount = sensors.getDeviceCount();
      wdt_reset();  // Resets the Watchdog Timer.
      delay(200);
    }
    delay(1000);
    shield.clear();
  }
}



void FirstRun () {
  bool skip = false;

  // Configure CH1 Temp Sensor
  if (firstRunStep == 0) {
    UnplugSensors();
    firstRunStep = 1;
  }
  if (firstRunStep == 1) {
    shield.clear();
    shield.setCursor(0, 0);
    shield.print(F("Plug in CH1"));
    shield.setCursor(0, 1);
    shield.print(F("temp sensor now."));

    while (deviceCount != 1) {
      sensors.begin();
      deviceCount = sensors.getDeviceCount();
      wdt_reset();  // Resets the Watchdog Timer.
      delay(200);
    }
    shield.clear();
    shield.setCursor(0, 0);
    shield.print(F("Sensor Found!"));
    shield.setCursor(0, 1);

    // Store Sensors Address to EEPROM (EEWrap)
    sensors.getAddress(sensorA, 0);
    for ( int i = 0 ; i < 8 ; ++i ) {
      addr1[ i ] = sensorA[ i ];
    }
    delay(1000);
    firstRunStep = 2;
  }



  // Configure CH2 Temp Sensor
  if (firstRunStep == 2) {
    UnplugSensors();
    firstRunStep = 3;
  }
  if (firstRunStep == 3) {
    shield.clear();
    shield.setCursor(0, 0);
    shield.print(F("Plug in CH2"));
    shield.setCursor(0, 1);
    shield.print(F("temp sensor now."));

    while (deviceCount != 1) {
      // Check for Skip Keypress
      if (shield.getButtons() == ButtonSelect) {
        skip = true;
        break;
      }
      sensors.begin();
      deviceCount = sensors.getDeviceCount();
      wdt_reset();  // Resets the Watchdog Timer.
      delay(200);
    }

    if (!skip) {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(F("Sensor Found!"));
      shield.setCursor(0, 1);

      // Store Sensors Address to EEPROM (EEWrap)
      sensors.getAddress(sensorB, 0);
      for ( int i = 0 ; i < 8 ; ++i ) {
        addr2[ i ] = sensorB[ i ];
      }
    } else {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(F("CH2 Skipped!"));
      sensorB[0] = 0;
      addr2[0] = 0;
      skip = false;
    }
    delay(1000);
    firstRunStep = 4;
  }



  // Configure CH3 Temp Sensor
  if (firstRunStep == 4) {
    UnplugSensors();
    firstRunStep = 5;
  }
  if (firstRunStep == 5) {
    shield.clear();
    shield.setCursor(0, 0);
    shield.print(F("Plug in CH3"));
    shield.setCursor(0, 1);
    shield.print(F("temp sensor now."));

    while (deviceCount != 1) {
      // Check for Skip Keypress
      if (shield.getButtons() == ButtonSelect) {
        skip = true;
        break;
      }
      sensors.begin();
      deviceCount = sensors.getDeviceCount();
      wdt_reset();  // Resets the Watchdog Timer.
      delay(200);
    }

    if (!skip) {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(F("Sensor Found!"));
      shield.setCursor(0, 1);

      // Store Sensors Address to EEPROM (EEWrap)
      sensors.getAddress(sensorC, 0);
      for ( int i = 0 ; i < 8 ; ++i ) {
        addr3[ i ] = sensorC[ i ];
      }
    } else {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(F("CH3 Skipped!"));
      sensorC[0] = 0;
      addr3[0] = 0;
      skip = false;
    }
    delay(1000);
    firstRunStep = 6;
  }



  // Configure CH4 Temp Sensor
  if (firstRunStep == 6) {
    UnplugSensors();
    firstRunStep = 7;
  }
  if (firstRunStep == 7) {
    shield.clear();
    shield.setCursor(0, 0);
    shield.print(F("Plug in CH4"));
    shield.setCursor(0, 1);
    shield.print(F("temp sensor now."));

    while (deviceCount != 1) {
      // Check for Skip Keypress
      if (shield.getButtons() == ButtonSelect) {
        skip = true;
        break;
      }
      sensors.begin();
      deviceCount = sensors.getDeviceCount();
      wdt_reset();  // Resets the Watchdog Timer.
      delay(200);
    }

    if (!skip) {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(F("Sensor Found!"));
      shield.setCursor(0, 1);

      // Store Sensors Address to EEPROM (EEWrap)
      sensors.getAddress(sensorD, 0);
      for ( int i = 0 ; i < 8 ; ++i ) {
        addr4[ i ] = sensorD[ i ];
      }
    } else {
      shield.clear();
      shield.setCursor(0, 0);
      shield.print(F("CH4 Skipped!"));
      sensorD[0] = 0;
      addr4[0] = 0;
      skip = false;
    }
    delay(1000);
    firstRunStep = 0;
    firstRun = true;
    skip = false;
    
    shield.clear();
    shield.setCursor(0, 0);
    shield.print(F("Plug in all"));
    shield.setCursor(0, 1);
    shield.print(F("sensors & reset."));
    while (true) {
      ; // Run Forever!
    }
  }
}


void hardReset() {
  shield.clear();
  shield.setCursor(0, 0);  // First character first line
  shield.print(F("Resetting..."));
  firstRun = false;
  isCelsius = false;
  differential = 0.5;
  firstRunStep = 0;
  highAlarm = 82;
  lowAlarm = 74;

  setTempCH1 = 78.0;
  setTempCH2 = 78.0;
  setTempCH3 = 78.0;
  setTempCH4 = 78.0;

  delay(2000);
  tone(PIN_BUZZ, 1000, 1000);
  shield.clear();
  shield.setCursor(0, 0);  // First character second line
  shield.print(F("Reset Complete!"));
  shield.setCursor(0, 1);
  shield.print(F("Reset Device Now"));
  while (true) {
    ; // wait for user to press the reset button
  }
}


void testRelays() {
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW);
  digitalWrite(RELAY4, LOW);
  delay(1000);
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);
  delay(1000);
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW);
  digitalWrite(RELAY4, LOW);
  delay(1000);
  digitalWrite(RELAY1, HIGH);
  digitalWrite(RELAY2, HIGH);
  digitalWrite(RELAY3, HIGH);
  digitalWrite(RELAY4, HIGH);
}

void processAlarms() {
  if (sensorA[0] != 0 && (tempCH1 == DEVICE_DISCONNECTED_C || tempCH1 == DEVICE_DISCONNECTED_F)) {
    tone(PIN_BUZZ, 2000, 1000);
    return;
  } else if (sensorB[0] != 0 && (tempCH2 == DEVICE_DISCONNECTED_C || tempCH2 == DEVICE_DISCONNECTED_F)) {
    tone(PIN_BUZZ, 2000, 1000);
    return;
  } else if (sensorC[0] != 0 && (tempCH3 == DEVICE_DISCONNECTED_C || tempCH3 == DEVICE_DISCONNECTED_F)) {
    tone(PIN_BUZZ, 2000, 1000);
    return;
  } else if (sensorD[0] != 0 && (tempCH4 == DEVICE_DISCONNECTED_C || tempCH4 == DEVICE_DISCONNECTED_F)) {
    tone(PIN_BUZZ, 2000, 1000);
    return;
  }

  if (sensorA[0] != 0 && tempCH1 >= highAlarm) {
    tone(PIN_BUZZ, 2000, 800);
  } else if (sensorB[0] != 0 && tempCH2 >= highAlarm) {
    tone(PIN_BUZZ, 2000, 800);
  } else if (sensorC[0] != 0 && tempCH3 >= highAlarm) {
    tone(PIN_BUZZ, 2000, 800);
  } else if (sensorD[0] != 0 && tempCH4 >= highAlarm) {
    tone(PIN_BUZZ, 2000, 800);
  } else if (sensorA[0] != 0 && tempCH1 <= lowAlarm) {
    tone(PIN_BUZZ, 2000, 500);
  } else if (sensorB[0] != 0 && tempCH2 <= lowAlarm) {
    tone(PIN_BUZZ, 2000, 500);
  } else if (sensorC[0] != 0 && tempCH3 <= lowAlarm) {
    tone(PIN_BUZZ, 2000, 500);
  } else if (sensorD[0] != 0 && tempCH4 <= lowAlarm) {
    tone(PIN_BUZZ, 2000, 500);
  }
}

void processRelays() {
  // Temp is low. Turn relay on.
  if (sensorA[0] != 0 && tempCH1 != DEVICE_DISCONNECTED_C && tempCH1 != DEVICE_DISCONNECTED_F && tempCH1 != 185 && tempCH1 <= setTempCH1 - differential) {
    digitalWrite(RELAY1, LOW);
  }
  if (sensorB[0] != 0 && tempCH2 != DEVICE_DISCONNECTED_C && tempCH2 != DEVICE_DISCONNECTED_F && tempCH2 <= setTempCH2 - differential) {
    digitalWrite(RELAY2, LOW);
  }
  if (sensorC[0] != 0 && tempCH3 != DEVICE_DISCONNECTED_C && tempCH3 != DEVICE_DISCONNECTED_F && tempCH3 <= setTempCH3 - differential) {
    digitalWrite(RELAY3, LOW);
  }
  if (sensorD[0] != 0 && tempCH4 != DEVICE_DISCONNECTED_C && tempCH4 != DEVICE_DISCONNECTED_F && tempCH4 <= setTempCH4 - differential) {
    digitalWrite(RELAY4, LOW);
  }

  // Temp has reached the set temp. Turn relay off.
  if (sensorA[0] != 0 && (tempCH1 >= setTempCH1 || tempCH1 == DEVICE_DISCONNECTED_C || tempCH1 == DEVICE_DISCONNECTED_F || tempCH1 == 185)) {
    digitalWrite(RELAY1, HIGH);
  }
  if (sensorB[0] != 0 && (tempCH2 >= setTempCH2 || tempCH2 == DEVICE_DISCONNECTED_C || tempCH2 == DEVICE_DISCONNECTED_F || tempCH2 == 185)) {
    digitalWrite(RELAY2, HIGH);
  }
  if (sensorC[0] != 0 && (tempCH3 >= setTempCH3 || tempCH3 == DEVICE_DISCONNECTED_C || tempCH3 == DEVICE_DISCONNECTED_F || tempCH3 == 185)) {
    digitalWrite(RELAY3, HIGH);
  }
  if (sensorD[0] != 0 && (tempCH4 >= setTempCH4 || tempCH4 == DEVICE_DISCONNECTED_C || tempCH4 == DEVICE_DISCONNECTED_F || tempCH4 == 185)) {
    digitalWrite(RELAY4, HIGH);
  }
}

void convertSetTemps() {
  if (isCelsius) {
    setTempCH1 = DallasTemperature::toCelsius(setTempCH1);
    setTempCH2 = DallasTemperature::toCelsius(setTempCH2);
    setTempCH3 = DallasTemperature::toCelsius(setTempCH3);
    setTempCH4 = DallasTemperature::toCelsius(setTempCH4);
    highAlarm = DallasTemperature::toCelsius(highAlarm);
    lowAlarm = DallasTemperature::toCelsius(lowAlarm);
  } else {
    setTempCH1 = DallasTemperature::toFahrenheit(setTempCH1);
    setTempCH2 = DallasTemperature::toFahrenheit(setTempCH2);
    setTempCH3 = DallasTemperature::toFahrenheit(setTempCH3);
    setTempCH4 = DallasTemperature::toFahrenheit(setTempCH4);
    highAlarm = DallasTemperature::toFahrenheit(highAlarm);
    lowAlarm = DallasTemperature::toFahrenheit(lowAlarm);
  }
}
