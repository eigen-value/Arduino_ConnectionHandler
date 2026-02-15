
#include "GSMUdp.h"

GSMUDP::GSMUDP() :
  _socket(-1),
  _packetReceived(false),
  _txIp((uint32_t)0),
  _txHost(NULL),
  _txPort(0),
  _txSize(0),
  _rxIp((uint32_t)0),
  _rxPort(0),
  _rxSize(0),
  _rxIndex(0)
{
  MODEM.addUrcHandler(this);
}

GSMUDP::~GSMUDP()
{
  MODEM.removeUrcHandler(this);
}

uint8_t GSMUDP::begin(uint16_t port)
{
  String response;

  // EG915: AT+QIOPEN=<contextID>,<connectID>,<service_type>,<IP_address>/<domain_name>,<remote_port>,<local_port>[,<access_mode>]
  // For UDP server (listening), use service_type="UDP SERVICE"
  // For UDP client, we'll use "UDP" and open when sending
  
  // First, find an available socket (0-11)
  for (int i = 0; i < 12; i++) {
    MODEM.sendf("AT+QISTATE=1,%d", i);
    if (MODEM.waitForResponse(100, &response) == 1) {
      // Check if this socket is available (not in response means available)
      if (response.indexOf("+QISTATE:") == -1) {
        _socket = i;
        break;
      }
    }
  }

  if (_socket < 0) {
    return 0;  // No available sockets
  }

  // Open UDP socket in service mode (listening on local port)
  // AT+QIOPEN=1,<connectID>,"UDP SERVICE","127.0.0.1",0,<local_port>,0
  MODEM.sendf("AT+QIOPEN=1,%d,\"UDP SERVICE\",\"127.0.0.1\",0,%d,0", _socket, port);
  
  // Wait for +QIOPEN URC
  unsigned long start = millis();
  bool opened = false;
  
  while (millis() - start < 10000) {
    MODEM.poll();
    
    // Check if socket opened (the URC handler will detect this)
    MODEM.send("AT+QISTATE=1," + String(_socket));
    if (MODEM.waitForResponse(100, &response) == 1) {
      if (response.indexOf("+QISTATE:") >= 0) {
        opened = true;
        break;
      }
    }
    delay(100);
  }

  if (!opened) {
    _socket = -1;
    return 0;
  }

  return 1;
}

void GSMUDP::stop()
{
  if (_socket < 0) {
    return;
  }

  // EG915: AT+QICLOSE=<connectID>
  MODEM.sendf("AT+QICLOSE=%d", _socket);
  MODEM.waitForResponse(10000);

  _socket = -1;
}

int GSMUDP::beginPacket(IPAddress ip, uint16_t port)
{
  if (_socket < 0) {
    return 0;
  }

  _txIp = ip;
  _txHost = NULL;
  _txPort = port;
  _txSize = 0;

  return 1;
}

int GSMUDP::beginPacket(const char *host, uint16_t port)
{
  if (_socket < 0) {
    return 0;
  }

  _txIp = (uint32_t)0;
  _txHost = host;
  _txPort = port;
  _txSize = 0;

  return 1;
}

int GSMUDP::endPacket()
{
  if (_socket < 0 || _txSize == 0) {
    return 0;
  }

  // EG915: AT+QISEND=<connectID>,<send_length>[,<RAI>]
  // Then wait for '>' prompt and send data
  
  // Build destination
  String dest;
  if (_txHost != NULL) {
    dest = _txHost;
  } else {
    dest = String(_txIp[0]) + "." + String(_txIp[1]) + "." + 
           String(_txIp[2]) + "." + String(_txIp[3]);
  }

  // For UDP, need to send with destination
  // AT+QISEND=<connectID>,<send_length>,"<remote_IP>",<remote_port>
  String command = "AT+QISEND=" + String(_socket) + "," + String(_txSize) + 
                   ",\"" + dest + "\"," + String(_txPort);
  
  MODEM.send(command);
  
  // Wait for '>' prompt
  if (MODEM.waitForPrompt(5000) < 0) {
    return 0;
  }

  // Send the actual data (binary)
  MODEM._uart->write(_txBuffer, _txSize);
  
  // Wait for SEND OK
  String response;
  for (unsigned long start = millis(); millis() - start < 10000; ) {
    if (MODEM._uart->available()) {
      char c = MODEM._uart->read();
      response += c;
      if (MODEM._debugPrint) {
        MODEM._debugPrint->write(c);
      }
      if (response.startsWith("SEND OK") && response.endsWith("\r\n")) {
        return 1;
      } else if (response.endsWith("\r\n")) {
        response = "";
      }
    }
  }

  return 0;
}

size_t GSMUDP::write(uint8_t b)
{
  return write(&b, sizeof(b));
}

size_t GSMUDP::write(const uint8_t *buffer, size_t size)
{
  if (_socket < 0) {
    return 0;
  }

  size_t spaceAvailable = sizeof(_txBuffer) - _txSize;

  if (size > spaceAvailable) {
    size = spaceAvailable;
  }

  memcpy(&_txBuffer[_txSize], buffer, size);
  _txSize += size;

  return size;
}

int GSMUDP::parsePacket()
{
  MODEM.poll();

  if (_socket < 0) {
    return 0;
  }

  if (!_packetReceived) {
    return 0;
  }
  _packetReceived = false;

  // EG915 UDP: AT+QIRD=<connectID>[,<read_length>]
  // Response: +QIRD: <length>,"<source_IP>",<source_port>
  MODEM.sendf("AT+QIRD=%d,%d", _socket, sizeof(_rxBuffer));

  String response;
  unsigned long start = millis();

  // Read response line-by-line to get the +QIRD: header
  while (millis() - start < 5000) {
    if (MODEM._uart->available()) {
      char c = MODEM._uart->read();

      if (MODEM._debugPrint) {
        MODEM._debugPrint->write(c);
      }

      response += c;

      if (response.startsWith("AT+QIRD") && response.endsWith("\r\n")) {
        response = "";
      } else if (response.startsWith("+QIRD: ") && c == '\n') {
        break;
      }
    }
  }

  if (!response.startsWith("+QIRD: ")) {
    return 0;
  }

  // Parse: +QIRD: 48,"132.163.97.4",123
  response.remove(0, 7);  // Remove "+QIRD: "
  response.trim();

  // Extract length (first number before comma)
  int firstComma = response.indexOf(',');
  if (firstComma < 0) {
    return 0;
  }

  String lengthStr = response.substring(0, firstComma);
  _rxSize = lengthStr.toInt();

  if (_rxSize <= 0) {
    return 0;
  }

  // Extract source IP (between quotes)
  int firstQuote = response.indexOf('"');
  int secondQuote = response.indexOf('"', firstQuote + 1);
  if (firstQuote > 0 && secondQuote > firstQuote) {
    String ipStr = response.substring(firstQuote + 1, secondQuote);
    _rxIp.fromString(ipStr);
  } else {
    _rxIp = IPAddress(0, 0, 0, 0);
  }

  // Extract source port (after last comma)
  int lastComma = response.lastIndexOf(',');
  if (lastComma > 0) {
    String portStr = response.substring(lastComma + 1);
    portStr.trim();
    _rxPort = portStr.toInt();
  } else {
    _rxPort = 0;
  }

  // Now read the binary data
  start = millis();
  size_t bytesRead = 0;

  while (bytesRead < _rxSize && millis() - start < 5000) {
    if (MODEM._uart->available()) {
      _rxBuffer[bytesRead++] = MODEM._uart->read();
    } else {
      delay(1);
    }
  }

  if (bytesRead != _rxSize) {
    Serial.print("Expected ");
    Serial.print(_rxSize);
    Serial.print(" bytes, got ");
    Serial.println(bytesRead);
    _rxSize = 0;
    return 0;
  }

  _rxIndex = 0;

  // Read trailing OK
  delay(50);
  while (MODEM._uart->available()) {
    char c = MODEM._uart->read();
    if (MODEM._debugPrint) {
      MODEM._debugPrint->write(c);
    }
  }

  Serial.print("Received ");
  Serial.print(_rxSize);
  Serial.print(" bytes from ");
  Serial.print(_rxIp);
  Serial.print(":");
  Serial.println(_rxPort);

  return _rxSize;
}

int GSMUDP::available()
{
  if (_socket < 0) {
    return 0;
  }

  return (_rxSize - _rxIndex);
}

int GSMUDP::read()
{
  byte b;

  if (read(&b, sizeof(b)) == 1) {
    return b;
  }

  return -1;
}

int GSMUDP::read(unsigned char* buffer, size_t len)
{
  size_t readMax = available();

  if (len > readMax) {
    len = readMax;
  }

  memcpy(buffer, &_rxBuffer[_rxIndex], len);

  _rxIndex += len;

  return len;
}

int GSMUDP::peek()
{
  if (available() > 0) {
    return _rxBuffer[_rxIndex];
  }

  return -1;
}

void GSMUDP::flush()
{
  _rxIndex = 0;
  _rxSize = 0;
}

IPAddress GSMUDP::remoteIP()
{
  return _rxIp;
}

uint16_t GSMUDP::remotePort()
{
  return _rxPort;
}

void GSMUDP::handleUrc(const String& urc)
{
  // +QIURC: "recv",<connectID>
  if (urc.startsWith("+QIURC: \"recv\",")) {
    // Parse: +QIURC: "recv",0
    // Extract socket number (after the last comma)
    int lastComma = urc.lastIndexOf(',');

    if (lastComma > 0) {
      String socketStr = urc.substring(lastComma + 1);
      socketStr.trim();
      int socket = socketStr.toInt();

      if (socket == _socket) {
        _packetReceived = true;
      }
    }
  }
  // EG915 URC: +QIURC: "closed",<connectID>
  else if (urc.startsWith("+QIURC: \"closed\",")) {
    int socket = urc.substring(18).toInt();

    if (socket == _socket) {
      // This socket closed
      _socket = -1;
      _rxIndex = 0;
      _rxSize = 0;
    }
  }
  // Socket opened notification
  else if (urc.startsWith("+QIOPEN: ")) {
    // +QIOPEN: <connectID>,<err>
    // err=0 means success
    int commaIndex = urc.indexOf(',');
    if (commaIndex > 0) {
      int socket = urc.substring(9, commaIndex).toInt();
      int err = urc.substring(commaIndex + 1).toInt();

      if (socket == _socket && err != 0) {
        // Failed to open
        _socket = -1;
      }
    }
  }
}