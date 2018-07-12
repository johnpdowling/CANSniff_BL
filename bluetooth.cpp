#include "bluetooth.h"

bluetooth::bluetooth(int powerPin, int rxPin, int txPin)
:
bluetoothSerial(rxPin, txPin)
{
  POWER_PIN = powerPin;
  pinMode(POWER_PIN, OUTPUT);
  
  if(rxPin == 0 && txPin == 1)
  {
    //hardware serial mode!
    bluetoothSerial.end();
    hardwareMode = true;
    DefaultBaud = DefaultBaudHW;
    Serial.begin(DefaultBaud);
    Serial.setTimeout(400);
  }
  else
  {
    DefaultBaud = DefaultBaudSW;
    bluetoothSerial.begin(DefaultBaud);
    bluetoothSerial.setTimeout(400);
  }
}

bluetooth::bluetooth(int rxPin, int txPin) : bluetooth(DEFAULT_BT_POWER, rxPin, txPin) { }

bluetooth::bluetooth(int powerPin) : bluetooth(powerPin, DEFAULT_BT_RX, DEFAULT_BT_TX) { }

bluetooth::bluetooth() : bluetooth(DEFAULT_BT_POWER, DEFAULT_BT_RX, DEFAULT_BT_TX) { }

int bluetooth::available()
{
  if(state == SET_UP)
  {
    return (hardwareMode ? Serial.available() : bluetoothSerial.available());
  }
  return 0;
}

int bluetooth::println(String message)
{
  if(state == SET_UP)
  {
    return (hardwareMode ? Serial.println(message) : bluetoothSerial.println(message));
  }
  return 0;
}

int bluetooth::write(unsigned char buf[], int len)
{
  if(state = SET_UP)
  {
    return (hardwareMode ? Serial.write(buf, len) : bluetoothSerial.write(buf, len));
  }
  return 0;
}

String bluetooth::readString()
{
  if(state == SET_UP)
  {
    return (hardwareMode ? Serial.readString() : bluetoothSerial.readString());
  }
  return "";
}

void bluetooth::flush()
{
  if(state == SET_UP)
  {
    return (hardwareMode ? Serial.flush() : bluetoothSerial.flush());
  }
}

bool bluetooth::find(byte buf[])
{
  if(state == SET_UP)
  {
    return (hardwareMode ? Serial.find(buf) : bluetoothSerial.find(buf));
  }
  return false;
}

char bluetooth::peek()
{
  if(state == SET_UP)
  {
    return (hardwareMode ? Serial.peek() : bluetoothSerial.peek());
  }
  return -1;
}

byte bluetooth::read()
{
  if(state == SET_UP)
  {
    return (hardwareMode ? Serial.read() : bluetoothSerial.read());
  }
  return -1;
}

int bluetooth::readBytes(byte buf[], int len)
{
  if(state == SET_UP)
  {
    return (hardwareMode ? Serial.readBytes(buf, len) : bluetoothSerial.readBytes(buf, len));
  }
  return 0;
}

void bluetooth::kick()
{
  if(state == SET_UP)
  {
    return;
  }
  unsigned long BTtime = millis();
  switch(state)
  {
    case POWER_ON:
      if(BTtime > stateTimeout)
      {
        //waited for power on
        SetBluetoothState(CHECK_BAUD, 0);
        steptemp = 4;
        stepverify = true;
        echo("Power on!");
      }
      break;
    case CHECK_BAUD:
      switch(stepstate)
      {
        case START:
          //ping!
          if(hardwareMode)
          {
            Serial.flush();
            Serial.print("AT");
          }
          else
          {
            bluetoothSerial.flush();
            bluetoothSerial.print("AT");
          }
          stateTimeout = add(BTtime, 400);
          stepstate = WAITING;
          break;
        case WAITING:
          bool pingfail = false;
          if(hardwareMode ? Serial.available() : bluetoothSerial.available())
          {
            char okchars[2 + 1];
            String("OK").toCharArray(okchars, 2);
            if(hardwareMode ? Serial.find(okchars) : bluetoothSerial.find(okchars))
            {
              //ping!
              //Serial.println("OK!");
              SetBluetoothState(VERIFY_BAUD, 0);
            }
            else
            {
              pingfail = true;
            }
          }
          else
          {
            if(BTtime > stateTimeout)
            {
              pingfail = true;
            }
          }
          if(pingfail)
          {
            //ping failed
            stepverify = false;
            if(hardwareMode)
            {
              Serial.end();
              Serial.begin(BaudRates[steptemp]);
            }
            else
            {
              bluetoothSerial.end();
              bluetoothSerial.begin(BaudRates[steptemp]);
            }
            echo("Ping bad, new baud." + String(BaudRates[steptemp]));
            steptemp++;
            if(steptemp >= NumRates)
            {
              steptemp = 4;
            }
            stepstate = START;
          }
          break;
      }          
      break;
    case VERIFY_BAUD:
      if(!stepverify)
      {
        echo("Check Baud or Verify Baud fail.");
        SetBluetoothState(SETTING, 0);
        return;
      }
      //Serial.println("Verify Baud");
      verifySetting("AT+BAUD?", "OK+BAUD:" + String(DefaultBaud));
      break;
    case VERIFY_NAME:
      if(!stepverify)
      {
        echo("Verify Name fail.");
        SetBluetoothState(SETTING, 0);
        return;
      }
      //Serial.println("Verify Name");
      verifySetting("AT+NAME?", "OK+NAME:" + String(BT_NAME));
      break;
    case VERIFY_PIN:
      if(!stepverify)
      {
        echo("Verify PIN fail.");
        SetBluetoothState(SETTING, 0);
        return;
      }
      //Serial.println("Verify PIN");
      verifySetting("AT+PIN?", "OK+PIN:" + String(BT_PIN));
      break;
    case VERIFY_AUTH:
      if(!stepverify)
      {
        echo("Verify Auth fail.");
        SetBluetoothState(SETTING, 0);
        return;
      }
      //Serial.println("Verify Auth");
      verifySetting("AT+AUTH?", "1");
      break;
    case SETTING:
      if(stepverify)
      {
        echo("Bluetooth Set up!");
        SetBluetoothState(SET_UP, 0);
        return;
      }
      //Serial.println("Defaulting");
      int defaultBaudCode;
      for(defaultBaudCode = NumRates - 1; defaultBaudCode > 0; defaultBaudCode--)
      {
        if(BaudRates[defaultBaudCode] == DefaultBaud)
        {
          break;
        }
      }
      setSetting("AT", "", "OK");
      setSetting("AT+DEFAULT", "", "OK+DEFAULT");

      //Serial.println("Set Baud");
      setSetting("AT+BAUD", String(defaultBaudCode, HEX), "OK+Set:" + String(DefaultBaud));

      //Serial.println("Set Name");
      setSetting("AT+NAME", String(BT_NAME), "OK+Set:" + String(BT_NAME));

      //Serial.println("Set PIN");
      setSetting("AT+PIN", String(BT_PIN), "OK+Set:" + String(BT_PIN));

      //Serial.println("Set Auth");
      setSetting("AT+AUTH", "1", "OK+Set:1");

      SetBluetoothState(RESTARTING, 0);
      break;
    case RESTARTING:
      switch(stepstate)
      {
        case START:
          //ping!
          echo("Restart!");
          if(hardwareMode)
          {
            Serial.print("AT+RESTART");
          }
          else
          {
            bluetoothSerial.print("AT+RESTART");
          }
          stateTimeout = add(millis(), 2000);
          stepstate = WAITING;
          break;
        case WAITING:
          if(millis() > stateTimeout)
          {
            //restarted
            if(hardwareMode)
            {
              Serial.end();
              Serial.begin(DefaultBaud);
            }
            else
            {
              bluetoothSerial.end();
              bluetoothSerial.begin(DefaultBaud);
            }
            SetBluetoothState(POWER_ON, 0);
          }
          break;
      }          
      break;
  }
}

bool bluetooth::is_set_up()
{
  return state == SET_UP;
}

void bluetooth::TurnOn()
{
  digitalWrite(POWER_PIN, LOW);
  SetBluetoothState(POWER_ON, 2000);
}

void bluetooth::TurnOff()
{
  digitalWrite(POWER_PIN, HIGH);
}

void bluetooth::SetBluetoothState(BTStates newstate, unsigned long timeout)
{
  state = newstate;
  stateTimeout = add(millis(), timeout);
  stepstate = START;
}

bool bluetooth::ping() 
{
  if(hardwareMode)
  {
    Serial.flush();
    Serial.print("AT");
  }
  else
  {
    bluetoothSerial.flush();
    bluetoothSerial.print("AT");
  }
  char okchars[2 + 1];
  String("OK").toCharArray(okchars, 2);
  return (hardwareMode ? Serial.find(okchars) : bluetoothSerial.find(okchars));
}

void bluetooth::verifySetting(String query, String message) 
{
  if(hardwareMode)
  {
    Serial.print(query);
    Serial.flush();
  }
  else
  {
    bluetoothSerial.print(query);
    bluetoothSerial.flush();
  }
  char messagechars[message.length() + 1];
  message.toCharArray(messagechars, message.length());
  //if(foo.endsWith(message))
  if(hardwareMode ? Serial.find(messagechars) : bluetoothSerial.find(messagechars))
  {
    SetBluetoothState((BTStates)((int)state+1), 0);
  }
  else
  {
    //Serial.println(query + " " + foo + " ! " + message);
    stepverify = false;
  }
  return;
  /*
  switch(stepstate)
  {
    case START:
      if(hardwareMode)
      {
        Serial.flush();
        Serial.print(query);
      }
      else
      {
        bluetoothSerial.flush();
        bluetoothSerial.print(query);
      }
      
      stateTimeout = add(millis(), 400);
      stepstate = WAITING;
      break;
    case WAITING:
      if(hardwareMode ? Serial.available() : bluetoothSerial.available())
      {
        if(hardwareMode ? Serial.readString().endsWith(message) : bluetoothSerial.readString().endsWith(message))
        {
          SetBluetoothState((BTStates)((int)state+1), 0);
        }
        else
        {
          stepverify = false;
        }
      }
      else
      {
        if(millis() > stateTimeout)
        {
          stepverify = false;
        }
      }
      break;
  }*/
}

bool bluetooth::setSetting(String setting, String variable, String response) {
  String foo;
  //while(foo != response)
  {
    //send(setting + variable);
    if(hardwareMode)
    {
      Serial.print(setting + variable);
      Serial.flush();
    }
    else
    {
      bluetoothSerial.print(setting + variable);
      bluetoothSerial.flush();
    }
    char responsechars[response.length() + 1];
    response.toCharArray(responsechars, response.length());
    //foo = bluetoothSerial.readString();
    //if(foo.endsWith(response))
    if(hardwareMode ? Serial.find(responsechars) : bluetoothSerial.find(responsechars))
    {
      //Serial.println(response);
      return true;
    }
    else
    {
      echo(setting + variable + " : " + foo + " ! " + response + ".");
      return false;
    }
    //echo(foo);
  }
  return false;
  /*
  switch(stepstate)
  {
    case START:
      //ping!
      //Serial.println("Sending " +setting+variable);
      bluetoothSerial.print(setting + variable);
      bluetoothSerial.flush();
      stateTimeout = add(millis(), 400);
      stepstate = WAITING;
      break;
    case WAITING:
      if(bluetoothSerial.available())
      {
        char responsechars[response.length() + 1];
        response.toCharArray(responsechars, response.length());
        if(bluetoothSerial.readString().endsWith(response))
        {
          SetBluetoothState((BTStates)((int)state+1), 0);
        }
        else
        {
          Serial.println("Bullshit response, try again. wanted: " + response);
          stepstate = START;
        }
      }
      else
      {
        if(millis() > stateTimeout)
        {
          Serial.println("Timeout on response. wanted: " + response);
          stepstate = START;
        }
      }
      break;
  }  
  */
}

void bluetooth::echo(String message)
{
  #ifdef ECHO
  Serial.println(message);
  #endif
}

unsigned long bluetooth::add(unsigned long a, unsigned long b)
{
  //account for millis() wrapping around to zero
  return (b > (MAX_LONG - a) ? b - (MAX_LONG - a) : a + b);
}

unsigned long bluetooth::diff(unsigned long before, unsigned long after)
{
  //diff accounts for millis() wrapping around to zero
  return (before > after ? after + (MAX_LONG - before) : after - before);
}
