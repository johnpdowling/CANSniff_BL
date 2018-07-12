#ifndef _BLUETOOTH_H_
#define _BLUETOOTH_H_
#include <Arduino.h>
#include <SoftwareSerial.h>

//#define ECHO

#define MAX_LONG    4294967295

#define DEFAULT_BT_POWER  4
#define DEFAULT_BT_RX     6
#define DEFAULT_BT_TX     7

const long DefaultBaudSW = 57600;
const long DefaultBaudHW = 115200;
const long BaudRates[] = { 0, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200}; //, 230400, 460800, 921600, 1382400 };
#define NumRates    (sizeof(BaudRates)/sizeof(long))

#define BT_NAME     "wompwomp"
#define BT_PIN      "2005"

enum BTStates
{
  NONE = 0,
  POWER_ON,
  CHECK_BAUD,
  VERIFY_BAUD,
  VERIFY_NAME,
  VERIFY_PIN,
  VERIFY_AUTH,
  SETTING,
  RESTARTING,
  SET_UP
};

enum stepStates
{
  START,
  WAITING
};

class bluetooth
{
  private:
    SoftwareSerial bluetoothSerial;
    long DefaultBaud = DefaultBaudSW;
    bool hardwareMode = false;
    
    int POWER_PIN;
    BTStates state = NONE;
    unsigned long stateTimeout = 0;
    stepStates stepstate = START;
    int steptemp;
    bool stepverify;
    void SetBluetoothState(BTStates state, unsigned long timeout);
    

    bool ping();
    void verifySetting(String query, String message);
    bool setSetting(String setting, String variable, String response);

    unsigned long add(unsigned long a, unsigned long b);
    unsigned long diff(unsigned long before, unsigned long after);
    void echo(String message);
  public:
    bluetooth(int powerPin, int rxPin, int txPin);
    bluetooth(int rxPin, int txPin);
    bluetooth(int powerPin);
    bluetooth();

    bool is_set_up();
    void kick();
    
    void TurnOff();
    void TurnOn();

    //wrappers
    int available();
    int write(unsigned char buf[], int len);
    int println(String message);
    String readString();
    void flush();
    bool find(byte buf[]);
    char peek();
    byte read();
    int readBytes(byte buf[], int len);
};

#endif
