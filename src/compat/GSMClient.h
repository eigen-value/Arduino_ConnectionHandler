
#ifndef GSMCLIENT_H
#define GSMCLIENT_H

#include <Arduino.h>
#include <Client.h>
#include <IPAddress.h>
#include "Modem.h"

class GSMClient : public Client, public ModemUrcHandler {

public:
    GSMClient(bool synch = true);
    GSMClient(int socket, bool synch);
    virtual ~GSMClient();

    virtual int ready();

    virtual int connect(IPAddress ip, uint16_t port);
    virtual int connect(const char *host, uint16_t port);
    virtual int connectSSL(IPAddress ip, uint16_t port);
    virtual int connectSSL(const char *host, uint16_t port);

    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buf);
    virtual size_t write(const uint8_t *buf, size_t size);
    virtual int available();
    virtual int read();
    virtual int read(uint8_t *buf, size_t size);
    virtual int peek();
    virtual void flush();
    virtual void stop();
    virtual uint8_t connected();
    virtual operator bool();

    void beginWrite(bool sync = false);
    void endWrite(bool sync = true);

    void setCertificateValidationLevel(uint8_t ssl);

    virtual void handleUrc(const String& urc);

    IPAddress remoteIP();
    uint16_t remotePort();

private:
    int connect();

    bool _synch;
    int _socket;
    bool _connected;
    int _state;
    IPAddress _ip;
    const char* _host;
    uint16_t _port;
    bool _ssl;
    uint8_t _sslprofile;
    bool _writeSync;
    String _response;

    uint8_t _rxBuffer[1460];
    size_t _rxBufferIndex;
    size_t _rxBufferLength;

    volatile int _lastOpenResult;
    volatile bool _openComplete;
};

#endif