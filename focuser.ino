//#define DEBUG
#define USEDHT22

#include <Arduino.h>
#include <AccelStepper.h>

#ifdef DEBUG
#include <SoftwareSerial.h>
#endif

#ifdef USEDHT22

#include <DHT.h>

#define DHTTYPE DHT22
#define DHTPIN 8

#endif

#define SPEED02 long(256)
#define SPEED04 long(128)
#define SPEED08 long(64)
#define SPEED10 long(32)
#define SPEED20 long(16)
#define MAXSPEED float(SPEED02)

#define MAXCOMMAND 16

#define CMD_START ':'
#define CMD_END '#'

#ifdef DEBUG
SoftwareSerial debug(2, 3); // RX, TX
#endif

bool commandReady(false);
char inChar;
int idx = 0;

int isRunning = 0;

long pos = 0;
long speed = 0;

char cmd[MAXCOMMAND];
char param[MAXCOMMAND];
char line[MAXCOMMAND];

char tempString[10];

AccelStepper stepper(AccelStepper::FULL4WIRE, 4, 6, 5, 7, true);

#ifdef USEDHT22
DHT dht(DHTPIN, DHTTYPE);
#endif

float temperature;
long millisLastTempCheck = millis();

void setup()
{
  Serial.begin(9600);
  while (!Serial) {
    ;
  }

#ifdef DEBUG
  debug.begin(9600);
  debug.println("debugger active");
#endif

#ifdef USEDHT22
  dht.begin();
#endif

  speed = (long)MAXSPEED;

  stepper.setMaxSpeed(float(speed));
  stepper.setAcceleration(float(speed));
  stepper.setCurrentPosition(-20000);
  stepper.enableOutputs();
  isRunning = 1;

#ifdef USEDHT22
  temperature = dht.readTemperature();
#else
  temperature = 25.0;
#endif
}

void debugWrite(char * str)
{
#ifdef DEBUG
  debug.println(str);
#endif
}

void write(char * str)
{
#ifdef DEBUG
  debug.print("response - ");
  debug.print(str);
  debug.println("");
#endif

  Serial.print(str);
}

void handleCommand()
{
    debugWrite(cmd);

    if (strncmp(cmd, "ID", 2) == 0)
    {
      write("DEVID=moonlite#");
    }
    else if (strncmp(cmd, "C", 1) == 0)
    {
      // Do nothing.
    }
    else if (strncmp(cmd, "PH", 2) == 0)
    {
      // Find home for Motor.
      stepper.moveTo(0);
      isRunning = 1;
      stepper.run();
    }
    else if (strncmp(cmd, "FG", 2) == 0)
    {
      // Go to the new position as set by the ":SNYYYY#" command.
      isRunning = 1;
    }
    else if (strncmp(cmd, "FQ", 2) == 0)
    {
      // Immediately stop any focus motor movement.
      stepper.stop();
    }
    else if (strncmp(cmd, "GC", 2) == 0)
    {
      write("02#");
    }
    else if (strncmp(cmd, "GB", 2) == 0)
    {
      write("00#"); // Full Stepped
    }
    else if (strncmp(cmd, "GH", 2) == 0)
    {
      write("00#"); // Full Stepped
    }
    else if (strncmp(cmd, "GT", 2) == 0)
    {
      char output[5];

      sprintf(output, "%04X", (long)(temperature * 2));
      write(output);
      write("#");
    }
    else if (strncmp(cmd, "GI", 2) == 0)
    {
      if (isRunning)
      {
        write("01#");
      }
      else
      {
        write("00#");
      }
    }
    else if (strncmp(cmd, "GN", 2) == 0)
    {
      // Returns the new position previously set by a ":SNYYYY" command where YYYY is a four-digit unsigned hex number.
      pos = -stepper.targetPosition();
      memset(tempString, 0, 10);
      sprintf(tempString, "%04X", pos);
      write(tempString);
      write("#");
    }
    else if (strncmp(cmd, "GP", 2) == 0)
    {
      // Returns the current position where YYYY is a four-digit unsigned hex number.
      memset(tempString, 0, 10);
      pos = -stepper.currentPosition();
      sprintf(tempString, "%04X", pos);
      write(tempString);
      write("#");
    }
    else if (strncmp(cmd, "GV", 2) == 0)
    {
      write("10"); // Firmware version
    }
    else if (strncmp(cmd, "SN", 2) == 0)
    {
      // Set the new position where YYYY is a four-digit unsigned hex number.
      long pos = hexstr2long(param);
      stepper.moveTo(-pos);
    }
    else if (strncmp(cmd, "SP", 2) == 0)
    {
      // Set the current position where YYYY is a four-digit unsigned hex number.
      long pos = hexstr2long(param);
      stepper.setCurrentPosition(-pos);
    }
    else if (strncmp(cmd, "SD", 2) == 0)
    {
      if (strncmp(param, "02", 2) == 0)
      {
        speed = SPEED02;
      }
      else if (strncmp(param, "04", 2) == 0)
      {
        speed = SPEED04;
      }
      else if (strncmp(param, "08", 2) == 0)
      {
        speed = SPEED08;
      }
      else if (strncmp(param, "10", 2) == 0)
      {
        speed = SPEED10;
      }
      else if (strncmp(param, "20", 2) == 0)
      {
        speed = SPEED20;
      }

      stepper.setMaxSpeed(float(speed));
    }
    else if (strncmp(cmd, "GD", 2) == 0)
    {
      if (speed == SPEED02)
      {
        write("02#");
      }
      else if (speed == SPEED04)
      {
        write("04#");
      }
      else if (speed == SPEED08)
      {
        write("08#");
      }
      else if (speed == SPEED10)
      {
        write("10#");
      }
      else if (speed == SPEED20)
      {
        write("20#");
      }
    }
}

void checkTemp()
{
#ifdef USEDHT22
  // reading the temperature is expensive.
  // don't do it if we are current moving to prevent jitter.
  if (!isRunning && (millis() - millisLastTempCheck) > 10000)
  {
    debugWrite("checking temp");

    float raw = dht.readTemperature();
    if (!isnan(raw))
    {
      temperature = raw;
    }
    millisLastTempCheck = millis();
  }
#endif
}

void loop()
{
  checkTemp();

  read();

  if (commandReady)
  {
    handleCommand();
    resetCommand();
  }

  if (isRunning)
  {
    stepper.run();

    if (stepper.distanceToGo() == 0)
    {
      stepper.run();
      isRunning = 0;
    }
  }
}

void read()
{
  while (Serial.available() && !commandReady)
  {
    inChar = Serial.read();

    if (inChar == ':')
    {
      resetCommand();
    }
    else if (inChar == '#')
    {
      commandReady = true;

      memset(cmd, 0, MAXCOMMAND);
      memset(param, 0, MAXCOMMAND);

      int len = strlen(line);

      if (len >= 2)
      {
        strncpy(cmd, line, 2);
      }
      else
      {
        strncpy(cmd, line, len);
      }

      if (len > 2)
      {
        strncpy(param, line + 2, len - 2);
      }
    }
    else
    {
      line[idx++] = inChar;
    }
  } // end while Serial.available()
}

void resetCommand()
{
  memset(line, 0, MAXCOMMAND);
  memset(cmd, 0, MAXCOMMAND);
  memset(param, 0, MAXCOMMAND);
  commandReady = false;
  idx = 0;
}

long hexstr2long(char *val)
{
  long ret = 0;
  ret = strtol(val, NULL, 16);
  return ret;
}
