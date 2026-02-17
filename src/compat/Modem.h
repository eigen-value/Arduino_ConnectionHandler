
#ifndef _COMPAT_MODEM_INCLUDED_H
#define _COMPAT_MODEM_INCLUDED_H

#include <stdarg.h>
#include <stdio.h>

#if defined(ESP32)
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
#endif

#include <Arduino.h>

class ModemUrcHandler {
public:
    virtual void handleUrc(const String& urc) = 0;
};

class ModemClass {
public:

    class Lock {
    public:
        explicit Lock(ModemClass& m, uint32_t timeoutMs = 0)
          : _m(m), _locked(false)
        {
            _locked = _m.lock(timeoutMs);
        }

        ~Lock() {
            if (_locked) _m.unlock();
        }

        // optional: allow checking
        bool locked() const { return _locked; }

    private:
        ModemClass& _m;
        bool _locked;
    };

    ModemClass(HardwareSerial& uart, unsigned long baud, int resetPin, int dtrPin);

    int begin(bool restart = true);
    void end();

    void debug();
    void debug(Print& p);
    void noDebug();

    int autosense(unsigned int timeout = 10000);

    int noop();
    int reset();

    int lowPowerMode();
    int noLowPowerMode();

    size_t write(uint8_t c);
    size_t write(const uint8_t*, size_t);

    void send(const char* command);
    void send(const String& command) { send(command.c_str()); }
    void sendf(const char *fmt, ...);

    int waitForResponse(unsigned long timeout = 100, String* responseDataStorage = NULL);
    int waitForPrompt(unsigned long timeout = 500);
    int ready();
    void poll();
    void setResponseDataStorage(String* responseDataStorage);

    void addUrcHandler(ModemUrcHandler* handler);
    void removeUrcHandler(ModemUrcHandler* handler);

    void setBaudRate(unsigned long baud);

    bool lock(uint32_t timeoutMs = 30000);
    void unlock();

private:
#if defined(ESP32)
    SemaphoreHandle_t _mutex = nullptr;
#endif
    HardwareSerial* _uart;
    unsigned long _baud;
    int _resetPin;
    int _dtrPin;
    bool _lowPowerMode;
    unsigned long _lastResponseOrUrcMillis;

    enum {
        AT_COMMAND_IDLE,
        AT_RECEIVING_RESPONSE
      } _atCommandState;
    int _ready;
    String _buffer;
    String* _responseDataStorage;

    friend class GSMUDP;
    friend class GSMClient;

#define MAX_URC_HANDLERS 10 // 7 sockets + GPRS + GSMLocation + GSMVoiceCall
    static ModemUrcHandler* _urcHandlers[MAX_URC_HANDLERS];
    static Print* _debugPrint;
};

extern ModemClass MODEM;
extern HardwareSerial SerialGSM;

#endif
