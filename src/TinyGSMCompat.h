/*
This file is part of the Arduino_ConnectionHandler library.

  Copyright (c) 2019 Arduino SA

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef TINY_GSM_COMPAT_H
#define TINY_GSM_COMPAT_H

// ESP32 implementation using TinyGSM
#define TINY_GSM_MODEM_BG96
#include <TinyGsmClient.h>
//#include <Arduino.h>

extern HardwareSerial ModemSerial;
extern TinyGsm modem;

enum Tiny_NetworkStatus_t { ERROR, IDLE, CONNECTING, GSM_READY, GPRS_READY, TRANSPARENT_CONNECTED, GSM_OFF };

// GSM class - mimics MKRGSM's GSM class
class GSM {
public:

  GSM(bool debug = false) : _debug(debug) {}

  Tiny_NetworkStatus_t begin(const char* pin = 0, bool restart = true, bool synchronous = true);
  int isAccessAlive();
  bool shutdown();

  Tiny_NetworkStatus_t status();
  String getIMEI();
  String getICCID();
  int getSignalStrength();

  void setTimeout(unsigned long timeout);

  unsigned long getTime();
  unsigned long getLocalTime();


private:
  bool _debug;
  Tiny_NetworkStatus_t _status = IDLE;
  unsigned long _timeout = 0;

};

// GPRS class - mimics MKRGSM's GPRS class
class GPRS {
public:
  Tiny_NetworkStatus_t attachGPRS(const char* apn, const char* user = "", const char* pass = "", bool synchronous = true);
  int detachGPRS();
  IPAddress getIPAddress();
  Tiny_NetworkStatus_t status();
  int ping(const char* host, uint8_t ttl = 128);

  void setTimeout(unsigned long timeout);

private:
  bool _attached = false;
  unsigned long _timeout = 0;
};

// GSMClient class - mimics MKRGSM's GSMClient (TCP)
class GSMClient : public Client {
public:
  GSMClient(bool synch = true);
  virtual ~GSMClient();


  // Client interface implementation
  virtual int connect(IPAddress ip, uint16_t port);
  virtual int connect(const char* host, uint16_t port);
  virtual size_t write(uint8_t);
  virtual size_t write(const uint8_t* buf, size_t size);
  virtual int available();
  virtual int read();
  virtual int read(uint8_t* buf, size_t size);
  virtual int peek();
  virtual void flush();
  virtual void stop();
  virtual uint8_t connected();
  virtual operator bool();

  // Additional methods
  IPAddress remoteIP();
  uint16_t remotePort();

private:
  TinyGsmClient* _client;
  bool _synch;
};

// GSMUDP class - mimics MKRGSM's GSMUDP (UDP)
class GSMUDP : public UDP {
public:
  GSMUDP();
  virtual ~GSMUDP();

  // UDP interface implementation
  virtual uint8_t begin(uint16_t port);
  virtual void stop();
  virtual int beginPacket(IPAddress ip, uint16_t port);
  virtual int beginPacket(const char* host, uint16_t port);
  virtual int endPacket();
  virtual size_t write(uint8_t);
  virtual size_t write(const uint8_t* buffer, size_t size);
  virtual int parsePacket();
  virtual int available();
  virtual int read();
  virtual int read(unsigned char* buffer, size_t len);
  virtual int read(char* buffer, size_t len) { return read((unsigned char*)buffer, len); }
  virtual int peek();
  virtual void flush();

  // Additional methods
  IPAddress remoteIP();
  uint16_t remotePort();

private:
  uint16_t _port;
  IPAddress _remoteIP;
  uint16_t _remotePort;
  uint8_t _buffer[1460];  // UDP buffer
  size_t _bufferSize;
  size_t _bufferPos;
  bool _begun;
};

// Global instances
extern GSM gsmAccess;
extern GPRS gprs;

#endif //TINY_GSM_COMPAT_H
