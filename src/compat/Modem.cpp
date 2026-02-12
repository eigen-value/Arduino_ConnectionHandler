
#include "Modem.h"
// === MODEM PINS ===
#define MODEM_RX 18
#define MODEM_TX 17
#define MODEM_PWRKEY 13
#define MODEM_RSTKEY 11

#define MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS 20

ModemUrcHandler* ModemClass::_urcHandlers[MAX_URC_HANDLERS] = { NULL };
Print* ModemClass::_debugPrint = NULL;

ModemClass::ModemClass(HardwareSerial& uart, unsigned long baud, int resetPin, int dtrPin) :
  _uart(&uart),
  _baud(baud),
  _resetPin(resetPin),
  _dtrPin(dtrPin),
  _lowPowerMode(false),
  _lastResponseOrUrcMillis(0),
  _atCommandState(AT_COMMAND_IDLE),
  _ready(1),
  _responseDataStorage(NULL)
{
  _buffer.reserve(64);
}

int ModemClass::begin(bool restart)
{
  _uart->begin(_baud > 115200 ? 115200 : _baud, SERIAL_8N1, MODEM_RX, MODEM_TX);

  if (_resetPin > -1 && restart) {
    pinMode(_resetPin, OUTPUT);
    digitalWrite(_resetPin, LOW);
    delay(100);
    digitalWrite(_resetPin, HIGH);
    delay(1000);
    digitalWrite(_resetPin, LOW);

    // CRITICAL: Wait for RDY message from EG915
    Serial.println("Waiting for modem to boot...");

    unsigned long start = millis();
    bool rdyReceived = false;

    while (millis() - start < 15000) {  // 15 second timeout
      while (_uart->available()) {
        char c = _uart->read();
        Serial.write(c);  // Echo to debug

        static String bootMsg = "";
        bootMsg += c;

        if (bootMsg.indexOf("RDY") >= 0) {
          rdyReceived = true;
          Serial.println("\n✓ Modem ready!");
          break;
        }

        // Keep buffer reasonable
        if (bootMsg.length() > 100) {
          bootMsg = bootMsg.substring(bootMsg.length() - 50);
        }
      }

      if (rdyReceived) break;
      delay(100);
    }

    if (!rdyReceived) {
      Serial.println("WARNING: Did not receive RDY message");
    }

    delay(2000);  // Additional safety delay
  }

  // Now safe to send AT commands
  // Disable echo
  send("ATE0");
  waitForResponse(1000);

  return 1;
}

void ModemClass::end()
{
  _uart->end();
  digitalWrite(_resetPin, HIGH);

  if (_dtrPin > -1) {
    digitalWrite(_dtrPin, LOW);
  }
}

void ModemClass::debug()
{
  debug(Serial);
}

void ModemClass::debug(Print& p)
{
  _debugPrint = &p;
}

void ModemClass::noDebug()
{
  _debugPrint = NULL;
}

int ModemClass::autosense(unsigned int timeout)
{
  for (unsigned long start = millis(); (millis() - start) < timeout;) {
    if (noop() == 1) {
      return 1;
    }

    delay(100);
  }

  return 0;
}

int ModemClass::noop()
{
  send("AT");

  return (waitForResponse() == 1);
}

int ModemClass::reset()
{
  send("AT+CFUN=16");

  return (waitForResponse(1000) == 1);
}

int ModemClass::lowPowerMode()
{
  if (_dtrPin > -1) {
    _lowPowerMode = true;

    digitalWrite(_dtrPin, HIGH);

    return 1;
  }

  return 0;
}

int ModemClass::noLowPowerMode()
{
  if (_dtrPin > -1) {
    _lowPowerMode = false;

    digitalWrite(_dtrPin, LOW);

    return 1;
  }

  return 0;
}

size_t ModemClass::write(uint8_t c)
{
  return _uart->write(c);
}

size_t ModemClass::write(const uint8_t* buf, size_t size)
{
  return _uart->write(buf, size);
}

void ModemClass::send(const char* command)
{
  if (_lowPowerMode) {
    digitalWrite(_dtrPin, LOW);
    delay(5);
  }

  // compare the time of the last response or URC and ensure
  // at least 20ms have passed before sending a new command
  unsigned long delta = millis() - _lastResponseOrUrcMillis;
  if(delta < MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS) {
    delay(MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS - delta);
  }

  _uart->println(command);
  _uart->flush();
  _atCommandState = AT_COMMAND_IDLE;
  _ready = 0;
}

void ModemClass::sendf(const char *fmt, ...)
{
  char buf[BUFSIZ];

  va_list ap;
  va_start((ap), (fmt));
  vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
  va_end(ap);

  send(buf);
}

int ModemClass::waitForResponse(unsigned long timeout, String* responseDataStorage)
{
  _responseDataStorage = responseDataStorage;
  for (unsigned long start = millis(); (millis() - start) < timeout;) {
    int r = ready();

    if (r != 0) {
      _responseDataStorage = NULL;
      return r;
    }
  }

  _responseDataStorage = NULL;
  _buffer = "";
  return -1;
}

int ModemClass::waitForPrompt(unsigned long timeout)
{
  for (unsigned long start = millis(); (millis() - start) < timeout;) {
    ready();

    if (_buffer.endsWith(">")) {
      return 1;
    }
  }

  return -1;
}

int ModemClass::ready()
{
  poll();

  return _ready;
}

void ModemClass::poll()
{
  while (_uart->available()) {
    char c = _uart->read();

    if (_debugPrint) {
      _debugPrint->write(c);
    }

    _buffer += c;

    switch (_atCommandState) {
      case AT_COMMAND_IDLE:
      default: {

        if (_buffer.startsWith("AT") && _buffer.endsWith("\r\n")) {
          _atCommandState = AT_RECEIVING_RESPONSE;
          _buffer = "";
        }  else if (_buffer.endsWith("\r\n")) {
          _buffer.trim();

          if (_buffer.length()) {
            _lastResponseOrUrcMillis = millis();

            for (int i = 0; i < MAX_URC_HANDLERS; i++) {
              if (_urcHandlers[i] != NULL) {
                _urcHandlers[i]->handleUrc(_buffer);
              }
            }
          }

          _buffer = "";
        }

        break;
      }

      case AT_RECEIVING_RESPONSE: {
        if (c == '\n') {
          _lastResponseOrUrcMillis = millis();

          int responseResultIndex = _buffer.lastIndexOf("OK\r\n");
          if (responseResultIndex != -1) {
            _ready = 1;
          } else {
            responseResultIndex = _buffer.lastIndexOf("ERROR\r\n");
            if (responseResultIndex != -1) {
              _ready = 2;
            } else {
              responseResultIndex = _buffer.lastIndexOf("NO CARRIER\r\n");
              if (responseResultIndex != -1) {
                _ready = 3;
              }
            }
          }

          if (_ready != 0) {
            if (_lowPowerMode) {
              digitalWrite(_dtrPin, HIGH);
            }

            if (_responseDataStorage != NULL) {
              _buffer.remove(responseResultIndex);
              _buffer.trim();

              *_responseDataStorage = _buffer;

              _responseDataStorage = NULL;
            }

            _atCommandState = AT_COMMAND_IDLE;
            _buffer = "";
            return;
          }
        }
        break;
      }
    }
  }
}

void ModemClass::setResponseDataStorage(String* responseDataStorage)
{
  _responseDataStorage = responseDataStorage;
}

void ModemClass::addUrcHandler(ModemUrcHandler* handler)
{
  for (int i = 0; i < MAX_URC_HANDLERS; i++) {
    if (_urcHandlers[i] == NULL) {
      _urcHandlers[i] = handler;
      break;
    }
  }
}

void ModemClass::removeUrcHandler(ModemUrcHandler* handler)
{
  for (int i = 0; i < MAX_URC_HANDLERS; i++) {
    if (_urcHandlers[i] == handler) {
      _urcHandlers[i] = NULL;
      break;
    }
  }
}

void ModemClass::setBaudRate(unsigned long baud)
{
  _baud = baud;
}

HardwareSerial SerialGSM(1);
ModemClass MODEM(SerialGSM, 115200, MODEM_PWRKEY, -1);
