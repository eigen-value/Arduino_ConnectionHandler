
#include "GSMClient.h"

enum {
  CLIENT_STATE_IDLE,
  CLIENT_STATE_OPEN_SOCKET,
  CLIENT_STATE_WAIT_OPEN_SOCKET,
  CLIENT_STATE_CLOSE_SOCKET,
  CLIENT_STATE_WAIT_CLOSE_SOCKET
};

GSMClient::GSMClient(bool synch) :
  GSMClient(-1, synch)
{
}

GSMClient::GSMClient(int socket, bool synch) :
  _synch(synch),
  _socket(socket),
  _connected(false),
  _state(CLIENT_STATE_IDLE),
  _ip((uint32_t)0),
  _host(NULL),
  _port(0),
  _ssl(false),
  _sslprofile(0),
  _writeSync(true),
  _rxBufferIndex(0),
  _rxBufferLength(0),
  _lastOpenResult(-1),
  _openComplete(false)
{
  MODEM.addUrcHandler(this);
}

GSMClient::~GSMClient()
{
  MODEM.removeUrcHandler(this);
}

int GSMClient::ready()
{
  int ready = MODEM.ready();

  if (ready == 0) {
    return 0;
  }

  switch (_state) {
    case CLIENT_STATE_IDLE:
    default: {
      break;
    }

    case CLIENT_STATE_OPEN_SOCKET: {
      // Find available socket (0-11)
      if (_socket == -1) {
        String response;
        for (int i = 0; i < 12; i++) {
          MODEM.sendf("AT+QISTATE=1,%d", i);
          if (MODEM.waitForResponse(100, &response) == 1) {
            if (response.indexOf("+QISTATE:") == -1) {
              _socket = i;
              break;
            }
          }
        }

        if (_socket == -1) {
          _state = CLIENT_STATE_IDLE;
          ready = 1;
          break;
        }
      }

      // Open socket
      // AT+QIOPEN=<contextID>,<connectID>,<service_type>,"<IP_address>/<domain_name>",<remote_port>[,<local_port>[,<access_mode>]]
      _lastOpenResult = -1;
      _openComplete = false;

      String serviceType = _ssl ? "TCP SSL" : "TCP";
      
      if (_host != NULL) {
        MODEM.sendf("AT+QIOPEN=1,%d,\"%s\",\"%s\",%d,0,0", 
                    _socket, serviceType.c_str(), _host, _port);
      } else {
        MODEM.sendf("AT+QIOPEN=1,%d,\"%s\",\"%d.%d.%d.%d\",%d,0,0", 
                    _socket, serviceType.c_str(), _ip[0], _ip[1], _ip[2], _ip[3], _port);
      }

      _state = CLIENT_STATE_WAIT_OPEN_SOCKET;
      ready = 0;
      break;
    }

    case CLIENT_STATE_WAIT_OPEN_SOCKET: {
      // Wait for +QIOPEN URC
      if (_openComplete) {
        if (_lastOpenResult == 0) {
          _connected = true;
          _state = CLIENT_STATE_IDLE;
        } else {
          _state = CLIENT_STATE_CLOSE_SOCKET;
          ready = 0;
        }
      } else {
        ready = 0;
      }
      break;
    }

    case CLIENT_STATE_CLOSE_SOCKET: {
      if (_socket >= 0) {
        MODEM.sendf("AT+QICLOSE=%d", _socket);
        _state = CLIENT_STATE_WAIT_CLOSE_SOCKET;
        ready = 0;
      } else {
        _state = CLIENT_STATE_IDLE;
      }
      break;
    }

    case CLIENT_STATE_WAIT_CLOSE_SOCKET: {
      _state = CLIENT_STATE_IDLE;
      _socket = -1;
      _connected = false;
      break;
    }
  }

  return ready;
}

int GSMClient::connect(IPAddress ip, uint16_t port)
{
  _ip = ip;
  _host = NULL;
  _port = port;
  _ssl = false;

  return connect();
}

int GSMClient::connectSSL(IPAddress ip, uint16_t port)
{
  _ip = ip;
  _host = NULL;
  _port = port;
  _ssl = true;

  return connect();
}

int GSMClient::connect(const char *host, uint16_t port)
{
  _ip = (uint32_t)0;
  _host = host;
  _port = port;
  _ssl = false;

  return connect();
}

int GSMClient::connectSSL(const char *host, uint16_t port)
{
  _ip = (uint32_t)0;
  _host = host;
  _port = port;
  _ssl = true;

  return connect();
}

int GSMClient::connect()
{
  if (_socket != -1) {
    stop();
  }

  _state = CLIENT_STATE_OPEN_SOCKET;

  if (_synch) {
    unsigned long start = millis();
    while (ready() == 0 && millis() - start < 30000) {
      delay(100);
    }

    if (_socket == -1 || !_connected) {
      return 0;
    }
  }

  return 1;
}

void GSMClient::beginWrite(bool sync)
{
  _writeSync = sync;
}

size_t GSMClient::write(uint8_t c)
{
  return write(&c, 1);
}

size_t GSMClient::write(const uint8_t *buf)
{
  return write(buf, strlen((const char*)buf));
}

size_t GSMClient::write(const uint8_t* buf, size_t size)
{
  if (_writeSync) {
    while (ready() == 0);
  } else if (ready() == 0) {
    return 0;
  }

  if (_socket == -1 || !_connected) {
    return 0;
  }

  // EG915: AT+QISEND=<connectID>,<send_length>
  // Then wait for '>' and send binary data
  
  size_t written = 0;

  while (size > 0) {
    size_t chunkSize = size > 1460 ? 1460 : size;

    MODEM.sendf("AT+QISEND=%d,%d", _socket, chunkSize);

    // Wait for '>' prompt
    unsigned long start = millis();
    bool gotPrompt = false;
    String buffer = "";

    while (millis() - start < 5000) {
      while (MODEM._uart->available()) {
        char c = MODEM._uart->read();

        if (MODEM._debugPrint) {
          MODEM._debugPrint->write(c);
        }

        buffer += c;

        if (c == '>') {
          gotPrompt = true;
          break;
        }
      }

      if (gotPrompt) {
        break;
      }

      delay(1);
    }

    if (!gotPrompt) {
      Serial.println("\n[write] No prompt received");
      break;
    }

    // Small delay after prompt
    delay(50);

    // Send binary data
    MODEM._uart->write(buf + written, chunkSize);

    if (_writeSync) {
      // Wait for SEND OK or ERROR
      String response;
      start = millis();

      while (millis() - start < 10000) {
        while (MODEM._uart->available()) {
          char c = MODEM._uart->read();

          if (MODEM._debugPrint) {
            MODEM._debugPrint->write(c);
          }

          response += c;
        }

        if (response.indexOf("SEND OK") >= 0) {
          break;
        }

        if (response.indexOf("ERROR") >= 0) {
          Serial.println("\n[write] Got ERROR");
          return written;
        }

        delay(1);
      }

      if (response.indexOf("SEND OK") < 0) {
        Serial.println("\n[write] No SEND OK");
        break;
      }
    }

    written += chunkSize;
    size -= chunkSize;
  }

  return written;
}

void GSMClient::endWrite(bool /*sync*/)
{
  _writeSync = true;
}

uint8_t GSMClient::connected()
{
  MODEM.poll();

  if (_socket == -1 || !_connected) {
    return 0;
  }

  // Check socket state
  String response;
  MODEM.sendf("AT+QISTATE=1,%d", _socket);
  if (MODEM.waitForResponse(500, &response) == 1) {
    if (response.indexOf("+QISTATE:") < 0) {
      // Socket closed
      stop();
      return 0;
    }
  }

  return 1;
}

GSMClient::operator bool()
{
  return (_socket != -1 && _connected);
}

int GSMClient::read(uint8_t *buf, size_t size)
{
  if (_socket == -1 || !_connected) {
    return 0;
  }

  if (size == 0) {
    return 0;
  }

  // Read from local buffer first
  if (_rxBufferIndex < _rxBufferLength) {
    size_t availableBytes = _rxBufferLength - _rxBufferIndex;
    size_t toRead = size < availableBytes ? size : availableBytes;

    memcpy(buf, _rxBuffer + _rxBufferIndex, toRead);
    _rxBufferIndex += toRead;

    return toRead;
  }

  // Buffer is empty, fetch more data from modem
  _rxBufferLength = 0;
  _rxBufferIndex = 0;

  // Send AT+QIRD command
  MODEM.sendf("AT+QIRD=%d,%d", _socket, sizeof(_rxBuffer));

  // Read response line-by-line until we find +QIRD:
  String line = "";
  int dataLength = 0;
  unsigned long start = millis();
  bool foundQird = false;

  while (millis() - start < 5000) {
    if (MODEM._uart->available()) {
      char c = MODEM._uart->read();

      if (MODEM._debugPrint) {
        MODEM._debugPrint->write(c);
      }

      if (c == '\n') {
        line.trim();

        // Check if this is the +QIRD: line
        if (line.startsWith("+QIRD: ")) {
          String lengthStr = line.substring(7);
          lengthStr.trim();
          dataLength = lengthStr.toInt();
          foundQird = true;
          break;
        }

        line = "";
      } else if (c != '\r') {
        line += c;
      }
    } else {
      delay(1);
    }
  }

  if (!foundQird || dataLength <= 0) {
    return 0;
  }

  // Now read exactly dataLength bytes of BINARY data
  // Do NOT print to debug - it's binary!
  start = millis();
  size_t bytesRead = 0;

  while (bytesRead < dataLength && millis() - start < 5000) {
    if (MODEM._uart->available()) {
      _rxBuffer[bytesRead++] = MODEM._uart->read();
    } else {
      delay(1);
    }
  }

  if (bytesRead != dataLength) {
    Serial.print("[read] Expected ");
    Serial.print(dataLength);
    Serial.print(" bytes, got ");
    Serial.println(bytesRead);
    return 0;
  }

  _rxBufferLength = bytesRead;
  _rxBufferIndex = 0;

  // Read and display the trailing "\r\nOK\r\n"
  delay(100);
  while (MODEM._uart->available()) {
    char c = MODEM._uart->read();
    if (MODEM._debugPrint) {
      MODEM._debugPrint->write(c);
    }
  }

  // Now return data from buffer
  size_t toRead = size < _rxBufferLength ? size : _rxBufferLength;
  memcpy(buf, _rxBuffer, toRead);
  _rxBufferIndex = toRead;

  return toRead;
}

int GSMClient::read()
{
  uint8_t b;

  if (read(&b, 1) == 1) {
    return b;
  }

  return -1;
}

int GSMClient::available()
{
  if (_socket == -1 || !_connected) {
    return 0;
  }

  // Check local buffer first
  if (_rxBufferIndex < _rxBufferLength) {
    return _rxBufferLength - _rxBufferIndex;
  }

  // Poll to process URCs
  MODEM.poll();

  // Query modem for unread bytes count
  // AT+QIRD=<id>,0 returns: +QIRD: <total>,<have_read>,<unread>
  String response;
  MODEM.sendf("AT+QIRD=%d,0", _socket);

  if (MODEM.waitForResponse(1000, &response) != 1) {
    return 0;
  }

  if (!response.startsWith("+QIRD: ")) {
    return 0;
  }

  // Parse the third number (unread count)
  // Format: +QIRD: 838,0,838
  //                      ^^^
  int firstComma = response.indexOf(',');
  int secondComma = response.indexOf(',', firstComma + 1);

  if (secondComma < 0) {
    return 0;
  }

  String unreadStr = response.substring(secondComma + 1);
  unreadStr.trim();

  int unread = unreadStr.toInt();

  // If there's unread data, it will be fetched on next read() call
  return unread;
}

int GSMClient::peek()
{
  if (_rxBufferIndex < _rxBufferLength) {
    return _rxBuffer[_rxBufferIndex];
  }

  return -1;
}

void GSMClient::flush()
{
  _rxBufferIndex = 0;
  _rxBufferLength = 0;
}

void GSMClient::stop()
{
  if (_socket < 0) {
    return;
  }

  MODEM.sendf("AT+QICLOSE=%d", _socket);
  MODEM.waitForResponse(10000);

  _socket = -1;
  _connected = false;
  _state = CLIENT_STATE_IDLE;
  _rxBufferIndex = 0;
  _rxBufferLength = 0;
}

void GSMClient::handleUrc(const String& urc)
{
  // +QIOPEN: <connectID>,<err>
  if (urc.startsWith("+QIOPEN: ")) {
    int commaIndex = urc.indexOf(',');

    if (commaIndex > 0) {
      int socket = urc.substring(9, commaIndex).toInt();
      int err = urc.substring(commaIndex + 1).toInt();

      if (socket == _socket) {
        _lastOpenResult = err;
        _openComplete = true;
      }
    }
  }
  // +QIURC: "recv",<connectID>
  else if (urc.startsWith("+QIURC: \"recv\",")) {
    // Data available notification (handled by available/read)
  }
  // +QIURC: "closed",<connectID>
  else if (urc.startsWith("+QIURC: \"closed\",")) {
    int socket = urc.substring(18).toInt();

    if (socket == _socket) {
      _connected = false;
    }
  }
}

void GSMClient::setCertificateValidationLevel(uint8_t ssl)
{
  _sslprofile = ssl;
}

IPAddress GSMClient::remoteIP()
{
  // EG915 doesn't easily provide this, return 0
  return IPAddress(0, 0, 0, 0);
}

uint16_t GSMClient::remotePort()
{
  return _port;
}