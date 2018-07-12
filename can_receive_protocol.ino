

#include <SPI.h>
#include "mcp_can.h"
#include <avr/sleep.h>
#include <SoftwareSerial.h>
#include "bluetooth.h"

#define BLUETOOTH
////////////////////////////////
//#define ECHO
//#define DEBUG

void echo(String message)
{
  #ifdef ECHO
  Serial.println(message);
  #endif
}

unsigned long diff(unsigned long before, unsigned long after)
{
  //diff accounts for millis() wrapping around to zero
  return (before > after ? after + (MAX_LONG - before) : after - before);
}
#define RxD         7
#define TxD         6

#define BT_POWER    4

bluetooth BT(BT_POWER, RxD, TxD);
//bluetooth BT(BT_POWER, 0, 1); //hardware serial

// the cs pin of the version after v1.1 is default to D9
// v0.9b and v1.0 is default D10
const int SPI_CS_PIN = 9;

MCP_CAN CAN(SPI_CS_PIN);                                    // Set CS pin

bool interruptSet = false;
bool sleeping = false;

unsigned long lastCAN = 0;
unsigned long lastHB = 0;
const unsigned long CAN_SLEEP_TIMEOUT = 5000;

const unsigned long HEARTBEAT_SLEEP_TIMEOUT = 5000;

const unsigned char MSG_HEADER[] = { 'V', 'W' };
#define MSG_HEADER_LEN (sizeof(MSG_HEADER)/sizeof(unsigned char))

const unsigned int crc_16_table[] = {  0x0000, 0xCC01, 0xD801, 0x1400, 0xF001, 0x3C00, 0x2800, 0xE401,
                                       0xA001, 0x6C00, 0x7800, 0xB401, 0x5000, 0x9C01, 0x8801, 0x4400 };

unsigned long masks[] = { 0x1000, 0x1000 };

unsigned long filters[] = { 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000 };

void setup()
{
  Serial.begin(115200);

  startCAN();
  
  echo("Setting up Bluetooth...");
  BT.TurnOn();
  lastCAN = millis();
}

void startCAN()
{
  while (CAN_OK != CAN.begin(CAN_100KBPS))              // init can bus : baudrate = 500k
  {
      echo("CAN BUS Shield init fail");
      echo(" Init CAN BUS Shield again");
      delay(100);
  }
  echo("CAN BUS Shield init ok at 100k!");
  
  setCANFilter(0x00, filters[0]);
  setCANFilter(0x01, filters[1]);
  setCANFilter(0x02, filters[2]);
  setCANFilter(0x03, filters[3]);
  setCANFilter(0x04, filters[4]);
  setCANFilter(0x05, filters[5]);
  setCANMask(0x00, masks[0]);
  setCANMask(0x01, masks[1]);
}

byte setCANMask(byte maskNum, unsigned long maskValue)
{
  if(maskValue < 0x1000)
  {
    echo("Set Mask: " + String(maskValue, HEX));
    return CAN.init_Mask(maskNum, 0x00, maskValue);
  }
  echo("Mask out of range: " + String(maskValue, HEX));
  return CAN.init_Mask(maskNum, 0x00, 0x0);
}

byte setCANFilter(byte filterNum, unsigned long filterValue)
{
  if(filterValue < 0x1000)
  {
    echo("Set Filter: " + String(filterValue, HEX));
    return CAN.init_Filt(filterNum, 0x00, filterValue);
  }
  echo("Filter out of range: " + String(filterValue, HEX));
  return CAN.init_Filt(filterNum, 0x00, 0x0);
}

void MCP2515_ISR()
{
  echo("Hit the interrupt!");
  if(sleeping)
  {
    disableInterrupt();
  }
}

void loop()
{
  //echo("loop");
  // put your main code here, to run repeatedly:
  unsigned char len = 0;
  unsigned char buf[8];
  unsigned int canId;
  unsigned int crc;
  //String message = "";
  if(CAN_MSGAVAIL == CAN.checkReceive())            // check if data coming
  {
    lastCAN = millis();
    CAN.readMsgBuf(&len, buf);    // read data,  len: data length, buf: data buf

    canId = CAN.getCanId();
  }
  /*else if(CAN_CTRLERROR == CAN.checkError())
  {
    //lastCAN = loopTime;
  }
  */
  if(!BT.is_set_up())
  {
    BT.kick();
  }
  else
  {
    if(len > 0)
    {
      unsigned int msglen = sizeof(canId) + len;
      unsigned char sendbytes[msglen];
      sendbytes[0] = (unsigned char)(canId >> 8);
      sendbytes[1] = (unsigned char)(canId & 0x00FF);
      for(unsigned char i = 0; i < len; i++)
      {
        sendbytes[2 + i] = buf[i];
      }
      if(sendMessage(0x00, sendbytes, msglen) > 0)
      {
        echo("CAN Message sent!");
      }
    }
    
    if(BT.available())
    {
      echo("message!");
      if(BT.find((byte*)MSG_HEADER))
      {
        echo("header!");
        char btPeek = BT.peek();
        echo("peek " + String(btPeek, HEX));
        
        if(btPeek > 2 && btPeek < 10)
        {
          unsigned char message[btPeek];
          if(BT.readBytes(message, btPeek) == btPeek)
          {
            echo("message read!" + String(sizeof(message), HEX));
            if(validMessage(message))
            {
              echo("valid message!");
              parseMessage(message);
            }
            else
            {
              echo("bad checksum!");
            }
          }
        }
        else if (btPeek == 0)
        {
          BT.read();
        }
      }
    }
  }
  
  unsigned long loopTime = millis();
  #ifdef DEBUG
  if(diff(lastCAN, loopTime) > CAN_SLEEP_TIMEOUT)
  {
    //debug!
    lastCAN = millis();
    len = 8;
    canId = (unsigned int)(lastCAN % 0x1000); //0x0123;
    unsigned int msglen = sizeof(canId) + len;
    unsigned char sendbytes[msglen];
    sendbytes[0] = (unsigned char)(canId >> 8);
    sendbytes[1] = (unsigned char)(canId & 0x00FF);
    for(unsigned char i = 0; i < len; i++)
    {
      sendbytes[2 + i] = i + 1;
    }
    sendMessage(0x00, sendbytes, msglen);
  }
  #else
  if(diff(lastCAN, loopTime) > CAN_SLEEP_TIMEOUT && diff(lastHB, loopTime) > HEARTBEAT_SLEEP_TIMEOUT)
  {
    //sleepy time
    //BT.println("Sleep!");
    sleepNow();
  }
  #endif
//  unsigned int foo = 0;
//  foo = bumpCRC(foo, 0x05);
//  foo = bumpCRC(foo, 0x01);
//  foo = bumpCRC(foo, 0x00);
//  echo("crc " + String(foo, HEX));
}

int sendMessage(unsigned char type, byte buf[], unsigned int len)
{
  unsigned int crc;
  int msglen = MSG_HEADER_LEN + sizeof(byte) + sizeof(type) + len + sizeof(crc);
  unsigned char sendbytes[msglen];
  for(int i = 0; i < MSG_HEADER_LEN; i++)
  {
    sendbytes[i] = MSG_HEADER[i];
  }
  sendbytes[MSG_HEADER_LEN] = sizeof(byte) + sizeof(type) + len + sizeof(crc);
  crc = bumpCRC(crc, sendbytes[MSG_HEADER_LEN]);
  sendbytes[MSG_HEADER_LEN + 1] = type;
  crc = bumpCRC(crc, sendbytes[MSG_HEADER_LEN + 1]);
  for(unsigned char i = 0; i < len; i++)
  {
    sendbytes[MSG_HEADER_LEN + 2 + i] = buf[i];
    crc = bumpCRC(crc, buf[i]);
  }
  //add CRC
  sendbytes[MSG_HEADER_LEN + 2 + len] = (unsigned char)(crc >> 8);
  sendbytes[MSG_HEADER_LEN + 3 + len] = (unsigned char)(crc & 0x00FF);
  return BT.write(sendbytes, msglen);
}

bool validMessage(unsigned char buf[])
{
  byte len = buf[0];
  unsigned int crc = 0;
  for(int i = 0; i < len - 2; i++)
  {
    //calc crc
    crc = bumpCRC(crc, buf[i]);
  }
  //compare to last two bytes
  return buf[len - 2] == (byte)(crc >> 8) && buf[len - 1] == (byte)(crc & 0x00FF);
}

void parseMessage(byte buf[])
{
  switch(buf[1])
  {
    case 0x01:
      //get mask
      if(buf[0] == 0x05)
      {
        //correct length
        if(buf[2] == 0x0 || buf[2] == 0x1)
        {
          byte response[3];
          getMaskResponse(buf[2], response);
          sendMessage(0x01, response, 3);
        }
      }
      break;
    case 0x11:
      //set mask
      if(buf[0] == 0x07)
      {
        //correct length
        if(buf[2] == 0x0 || buf[2] == 0x1)
        {
          unsigned long mask = buf[3];
          mask <<= 8;
          mask += buf[4];
          //if(setCANMask(buf[2], mask) == MCP2515_OK)
          {
            masks[buf[2]] = mask;
          }
          byte response[3];
          getMaskResponse(buf[2], response);
          sendMessage(0x01, response, 3);
        }
      }
      break;
    case 0x02:
      //get filter
      if(buf[0] == 0x05)
      {
        //correct length
        if(buf[2] >= 0x0 && buf[2] <= 0x6)
        {
          byte response[3];
          getFilterResponse(buf[2], response);
          sendMessage(0x02, response, 3);
        }
      }
      break;
    case 0x12:
      //set filter
      if(buf[0] == 0x07)
      {
        //correct length
        if(buf[2] >= 0x0 && buf[2] <= 0x6)
        {
          unsigned long filter = buf[3];
          filter <<= 8;
          filter += buf[4];
          //if(setCANFilter(buf[2], filter) == MCP2515_OK)
          {
            filters[buf[2]] = filter;
            //setCANMask(0x00, masks[0]);
            //setCANMask(0x01, masks[1]);
          }
          byte response[3];
          getFilterResponse(buf[2], response);
          sendMessage(0x02, response, 3);
          if(filter < 0x1000 && filter > 0x0000)
          {
            parseMessage(new byte[7] {0x07, 0x11, 0x00, 0x03, 0xFF, 0x00, 0x00});
            parseMessage(new byte[7] {0x07, 0x11, 0x01, 0x03, 0xFF, 0x00, 0x00});
          }
        }
      }
      break;
    case 0x03:
      //heartbeat
      if(buf[0] == 0x04)
      {
        //correct length
        echo("Heartbeat!");
        lastHB = millis();
      }
      break;
    case 0x04:
      //start
      if(buf[0] == 0x04)
      {
        //correct length
        echo("Start!");
        startCAN();
      }
      break;
  }
}

void getMaskResponse(byte mask, byte* buf)
{
  buf[0] = mask;
  buf[1] = (byte)(masks[mask] >> 8);
  buf[2] = (byte)(masks[mask] & 0x00FF);
}

void getFilterResponse(byte filter, byte* buf)
{
  buf[0] = filter;
  buf[1] = (byte)(filters[filter] >> 8);
  buf[2] = (byte)(filters[filter] & 0x00FF);
}

unsigned int bumpCRC(unsigned int crc, byte value)
{
  unsigned int r = crc_16_table[crc & 0x000F];
  crc = (unsigned int)((crc >> 4) & 0x0FFF);
  crc = (unsigned int)(crc ^ r ^ crc_16_table[value & 0x0F]);
  /* now compute checksum of upper four bits of ucData */
  r = crc_16_table[crc & 0x000F];
  crc = (unsigned int)((crc >> 4) & 0x0FFF);
  crc = (unsigned int)(crc ^ r ^ crc_16_table[(value >> 4) & 0x0F]);
  return crc;
}

void sleepNow()
{
  echo("Sleepy time!");
  BT.flush();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  //delay(100);
  digitalWrite(2, HIGH);
  enableInterrupt();
  BT.TurnOff();
  delay(5);
  sleeping = true;
  sleep_mode();

  //wakeup
  sleep_disable();
  wakeUp();
}

void wakeUp()
{
  echo("Wake up!");
  sleeping = false;
  BT.TurnOn();
}

void enableInterrupt()
{
  echo("Enabling interrupt.");
  interruptSet = true;
  attachInterrupt(0, MCP2515_ISR, FALLING); // start interrupt
}

void disableInterrupt()
{
  echo("Disabling interrupt");
  interruptSet = false;
  detachInterrupt(0); //stop interrupt
}

