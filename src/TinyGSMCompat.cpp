/*
This file is part of the Arduino_ConnectionHandler library.

  Copyright (c) 2019 Arduino SA

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

// === MODEM PINS ===
#define MODEM_RX 18
#define MODEM_TX 17
#define MODEM_PWRKEY 4  //TODO

#include "TinyGSMCompat.h"

// Global instances
HardwareSerial ModemSerial(1);
TinyGsm modem(ModemSerial);
GSM gsmAccess;
GPRS gprs;

// ============================================================================
// GSM Class Implementation
// ============================================================================

Tiny_NetworkStatus_t GSM::begin(const char* pin, bool restart, bool synchronous) {
  // Initialize UART
  ModemSerial.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(100);

  // Power on module if restart requested
  if (restart) {
    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(3000);  // Wait for module to boot

    modem.restart();
  }

  _status = CONNECTING;

  // Wait for network
  if (_debug) {
    Serial.println("Waiting for network...");
  }

  if (synchronous) {
    if (modem.waitForNetwork(60000)) {
      _status = GSM_READY;
      if (_debug) {
        Serial.println("Network connected");
      }
      return GSM_READY;
    } else {
      _status = ERROR;
      if (_debug) {
        Serial.println("Network connection failed");
      }
      return ERROR;
    }
  }

  return _status;
}

int GSM::isAccessAlive() {
  return modem.isNetworkConnected() ? 1 : 0;
}

bool GSM::shutdown() {
  modem.poweroff();
  _status = IDLE;
  return true;
}

Tiny_NetworkStatus_t GSM::status() {
  if (modem.isNetworkConnected()) {
    if (modem.isGprsConnected()) {
      return GPRS_READY;
    }
    return GSM_READY;
  }
  return _status;
}

String GSM::getIMEI() {
  return modem.getIMEI();
}

String GSM::getICCID() {
  return modem.getSimCCID();
}

int GSM::getSignalStrength() {
  return modem.getSignalQuality();
}

void GSM::setTimeout(unsigned long timeout) {
  _timeout = timeout;
}

unsigned long GSM::getTime() {
  return 0;
}
unsigned long GSM::getLocalTime() {
  return 0;
}

// ============================================================================
// GPRS Class Implementation
// ============================================================================

Tiny_NetworkStatus_t GPRS::attachGPRS(const char* apn, const char* user, const char* pass, bool synchronous) {
  if (synchronous) {
    if (modem.gprsConnect(apn, user, pass)) {
      _attached = true;
      return GPRS_READY;
    }
    return CONNECTING;
  }

  // Non-synchronous not fully implemented
  modem.gprsConnect(apn, user, pass);
  _attached = true;
  return GPRS_READY;
}

int GPRS::detachGPRS() {
  modem.gprsDisconnect();
  _attached = false;
  return 1;
}

IPAddress GPRS::getIPAddress() {
  return modem.localIP();
}

Tiny_NetworkStatus_t GPRS::status() {
  return modem.isGprsConnected() ? GPRS_READY : CONNECTING;
}

int GPRS::ping(const char* host, uint8_t ttl) {
  // TinyGSM doesn't have built-in ping, return success for compatibility
  return 1;
}

void GPRS::setTimeout(unsigned long timeout) {
  _timeout = timeout;
}


// ============================================================================
// GSMClient Class Implementation
// ============================================================================

GSMClient::GSMClient(bool synch) : _synch(synch) {
  _client = new TinyGsmClient(modem);
}

GSMClient::~GSMClient() {
  if (_client) {
    delete _client;
  }
}

int GSMClient::connect(IPAddress ip, uint16_t port) {
  char host[16];
  sprintf(host, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  return connect(host, port);
}

int GSMClient::connect(const char* host, uint16_t port) {
  return _client->connect(host, port) ? 1 : 0;
}

size_t GSMClient::write(uint8_t b) {
  return _client->write(b);
}

size_t GSMClient::write(const uint8_t* buf, size_t size) {
  return _client->write(buf, size);
}

int GSMClient::available() {
  return _client->available();
}

int GSMClient::read() {
  return _client->read();
}

int GSMClient::read(uint8_t* buf, size_t size) {
  return _client->read(buf, size);
}

int GSMClient::peek() {
  return _client->peek();
}

void GSMClient::flush() {
  _client->flush();
}

void GSMClient::stop() {
  _client->stop();
}

uint8_t GSMClient::connected() {
  return _client->connected();
}

GSMClient::operator bool() {
  return _client->connected();
}

IPAddress GSMClient::remoteIP() {
  // TinyGSM doesn't provide this, return placeholder
  return IPAddress(0, 0, 0, 0);
}

uint16_t GSMClient::remotePort() {
  // TinyGSM doesn't provide this, return placeholder
  return 0;
}

// ============================================================================
// GSMUDP Class Implementation
// ============================================================================

GSMUDP::GSMUDP() : _port(0), _bufferSize(0), _bufferPos(0), _begun(false) {
}

GSMUDP::~GSMUDP() {
  stop();
}

uint8_t GSMUDP::begin(uint16_t port) {
  _port = port;
  _begun = true;

  // For EG915, we need to configure UDP socket via AT commands
  // This is a simplified implementation - you may need to enhance it
  String cmd = "AT+QIOPEN=1,0,\"UDP\",\"0.0.0.0\",0," + String(port) + ",0";
  ModemSerial.println(cmd);
  delay(1000);

  return 1;
}

void GSMUDP::stop() {
  if (_begun) {
    ModemSerial.println("AT+QICLOSE=0");
    delay(500);
    _begun = false;
  }
}

int GSMUDP::beginPacket(IPAddress ip, uint16_t port) {
  char host[16];
  sprintf(host, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  return beginPacket(host, port);
}

int GSMUDP::beginPacket(const char* host, uint16_t port) {
  _remoteIP.fromString(host);
  _remotePort = port;
  _bufferPos = 0;
  return 1;
}

int GSMUDP::endPacket() {
  if (_bufferPos == 0) return 0;

  // Send UDP packet via AT commands
  String cmd = "AT+QISEND=0," + String(_bufferPos);
  ModemSerial.println(cmd);
  delay(100);

  ModemSerial.write(_buffer, _bufferPos);
  delay(100);

  _bufferPos = 0;
  return 1;
}

size_t GSMUDP::write(uint8_t b) {
  return write(&b, 1);
}

size_t GSMUDP::write(const uint8_t* buffer, size_t size) {
  if (_bufferPos + size > sizeof(_buffer)) {
    size = sizeof(_buffer) - _bufferPos;
  }

  memcpy(_buffer + _bufferPos, buffer, size);
  _bufferPos += size;

  return size;
}

int GSMUDP::parsePacket() {
  // Check for incoming UDP data
  // This is simplified - EG915 uses +QIURC: "recv" URC
  // You'll need to implement proper URC handling

  if (ModemSerial.available()) {
    String response = ModemSerial.readStringUntil('\n');
    if (response.indexOf("+QIURC: \"recv\"") >= 0) {
      // Parse data length and read into buffer
      // This needs proper implementation based on EG915 AT manual
      _bufferSize = ModemSerial.available();
      if (_bufferSize > sizeof(_buffer)) _bufferSize = sizeof(_buffer);
      ModemSerial.readBytes(_buffer, _bufferSize);
      _bufferPos = 0;
      return _bufferSize;
    }
  }

  return 0;
}

int GSMUDP::available() {
  return _bufferSize - _bufferPos;
}

int GSMUDP::read() {
  if (_bufferPos < _bufferSize) {
    return _buffer[_bufferPos++];
  }
  return -1;
}

int GSMUDP::read(unsigned char* buffer, size_t len) {
  size_t avail = _bufferSize - _bufferPos;
  if (len > avail) len = avail;

  memcpy(buffer, _buffer + _bufferPos, len);
  _bufferPos += len;

  return len;
}

int GSMUDP::peek() {
  if (_bufferPos < _bufferSize) {
    return _buffer[_bufferPos];
  }
  return -1;
}

void GSMUDP::flush() {
  _bufferPos = 0;
  _bufferSize = 0;
}

IPAddress GSMUDP::remoteIP() {
  return _remoteIP;
}

uint16_t GSMUDP::remotePort() {
  return _remotePort;
}
