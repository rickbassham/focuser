#define USEDHT22

#include <Arduino.h>
#include <AccelStepper.h>

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

#define MAXCOMMAND 8

#define CMD_START ':'
#define CMD_END '#'

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

AccelStepper stepper(AccelStepper::HALF4WIRE, 4, 6, 5, 7, true);

#ifdef USEDHT22
DHT dht(DHTPIN, DHTTYPE);
#endif

long millisLastMove = 0;

float temperature;
long millisLastTempCheck = 0;

void setup()
{
  Serial.begin(9600);
  dht.begin();

  speed = (long)MAXSPEED;

  stepper.setSpeed(float(MAXSPEED));
  stepper.setMaxSpeed(float(MAXSPEED));
  stepper.setAcceleration(float(64));

#ifdef USEDHT22
  temperature = dht.readTemperature();
#else
  temperature = 25.0;
#endif
}

void loop()
{
#ifdef USEDHT22
  // reading the temperature is expensive.
  // don't do it if we are current moving to prevent jitter.
  if (!isRunning && (millis() - millisLastTempCheck) > 10000)
  {
    float raw = dht.readTemperature();
    if (!isnan(raw))
    {
      temperature = raw;
    }
    millisLastTempCheck = millis();
  }
#endif
  
  
  read();

  if (commandReady)
  {
    if (strncmp(cmd, "ID", 2) == 0)
    {
      Serial.print("DEVID=moonlite#");
    }
    else if (strncmp(cmd, "PH", 2) == 0)
    {
      // Find home for Motor.
      stepper.moveTo(0);
      isRunning = 1;
    }
    else if (strncmp(cmd, "FG", 2) == 0)
    {
      // Go to the new position as set by the ":SNYYYY#" command.
      stepper.enableOutputs();
      isRunning = 1;
    }
    else if (strncmp(cmd, "FQ", 2) == 0)
    {
      // Immediately stop any focus motor movement.
      stepper.moveTo(stepper.currentPosition());
      stepper.run();
      isRunning = 0;
      stepper.stop();
    }
    else if (strncmp(cmd, "GC", 2) == 0)
    {
      Serial.print("02#");
    }
    else if (strncmp(cmd, "GB", 2) == 0)
    {
      Serial.print("00#"); // Full Stepped
    }
    else if (strncmp(cmd, "GH", 2) == 0)
    {
      Serial.print("00#"); // Full Stepped
    }
    else if (strncmp(cmd, "GT", 2) == 0)
    {
      char output[5];

      sprintf(output, "%04X", (long)(temperature * 2));
      Serial.print(output);
      Serial.print("#");
    }
    else if (strncmp(cmd, "GI", 2) == 0)
    {
      if (stepper.distanceToGo() != 0)
      {
        Serial.print("01#");
      }
      else
      {
        Serial.print("00#");
      }
    }
    else if (strncmp(cmd, "GN", 2) == 0)
    {
      // Returns the new position previously set by a ":SNYYYY" command where YYYY is a four-digit unsigned hex number.
      pos = -stepper.targetPosition();
      memset(tempString, 0, 10);
      sprintf(tempString, "%04X", pos);
      Serial.print(tempString);
      Serial.print("#");
    }
    else if (strncmp(cmd, "GP", 2) == 0)
    {
      // Returns the current position where YYYY is a four-digit unsigned hex number.
      memset(tempString, 0, 10);
      pos = -stepper.currentPosition();
      sprintf(tempString, "%04X", pos);
      Serial.print(tempString);
      Serial.print("#");
    }
    else if (strncmp(cmd, "GV", 2) == 0)
    {
      Serial.print("10#"); // Firmware version
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
        Serial.print("02#");
      }
      else if (speed == SPEED04)
      {
        Serial.print("04#");
      }
      else if (speed == SPEED08)
      {
        Serial.print("08#");
      }
      else if (speed == SPEED10)
      {
        Serial.print("10#");
      }
      else if (speed == SPEED20)
      {
        Serial.print("20#");
      }
    }

    resetCommand();
  }

  if (isRunning)
  {
    stepper.run();
    millisLastMove = millis();
  }
  else
  {
    // reported on INDI forum that some steppers "stutter" if disableOutputs is done repeatedly
    // over a short interval; hence we only disable the outputs and release the motor some seconds
    // after movement has stopped
    if ((millis() - millisLastMove) > 15000)
    {
      stepper.disableOutputs();
    }
  }

  if (stepper.distanceToGo() == 0)
  {
    stepper.run();
    isRunning = 0;
  }
}

void read()
{
  while (Serial.available() && !commandReady)
  {
    inChar = Serial.read();
    if (inChar != '#' && inChar != ':' && inChar != '\r' && inChar != '\n')
    {
      line[idx++] = inChar;
      if (idx >= MAXCOMMAND)
      {
        idx = MAXCOMMAND - 1;
      }
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

      if (len > 2)
      {
        strncpy(param, line + 2, len - 2);
      }
    }
  } // end while Serial.available()
}

void resetCommand()
{
  memset(line, 0, MAXCOMMAND);
  commandReady = false;
  idx = 0;
}

long hexstr2long(char *val)
{
  long ret = 0;
  ret = strtol(val, NULL, 16);
  return ret;
}
