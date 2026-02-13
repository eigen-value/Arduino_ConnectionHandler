
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
  unsigned long start = millis();
  bool gotPrompt = false;
  
  while (millis() - start < 5000) {
    if (MODEM._uart->available()) {
      char c = MODEM._uart->read();
      if (MODEM._debugPrint) {
        MODEM._debugPrint->write(c);
      }
      if (c == '>') {
        gotPrompt = true;
        break;
      }
    }
  }

  if (!gotPrompt) {
    return 0;
  }

  // Send the actual data (binary)
  MODEM._uart->write(_txBuffer, _txSize);
  
  // Wait for SEND OK
  String response;
  if (MODEM.waitForResponse(10000, &response) == 1) {
    if (response.indexOf("SEND OK") >= 0 || response == "OK") {
      return 1;
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

  String response;

  // EG915: AT+QIRD=<connectID>[,<read_length>]
  // Response: +QIRD: <read_actual_length><CR><LF><data>
  MODEM.sendf("AT+QIRD=%d,%d", _socket, sizeof(_rxBuffer));
  if (MODEM.waitForResponse(10000, &response) != 1) {
    return 0;
  }

  // Parse response: +QIRD: <length>
  if (!response.startsWith("+QIRD: ")) {
    return 0;
  }

  // Extract length
  int lengthEnd = response.indexOf('\n');
  if (lengthEnd == -1) {
    return 0;
  }

  String lengthStr = response.substring(7, lengthEnd);
  lengthStr.trim();
  _rxSize = lengthStr.toInt();

  if (_rxSize == 0) {
    return 0;
  }

  // Read the actual data from UART
  unsigned long start = millis();
  size_t bytesRead = 0;
  
  while (bytesRead < _rxSize && millis() - start < 5000) {
    if (MODEM._uart->available()) {
      _rxBuffer[bytesRead++] = MODEM._uart->read();
    }
  }

  if (bytesRead != _rxSize) {
    _rxSize = 0;
    return 0;
  }

  _rxIndex = 0;

  // Note: EG915 doesn't provide source IP/port in AT+QIRD response
  // Would need to parse from +QIURC: "recv" notification
  // For now, set to unknown
  _rxIp = IPAddress(0, 0, 0, 0);
  _rxPort = 0;

  MODEM.poll();

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
  // EG915 URC: +QIURC: "recv",<connectID>,<currentrecvlength>
  if (urc.startsWith("+QIURC: \"recv\",")) {
    // Parse: +QIURC: "recv",0,123
    int firstComma = urc.indexOf(',');
    int secondComma = urc.indexOf(',', firstComma + 1);
    
    if (firstComma > 0 && secondComma > firstComma) {
      String socketStr = urc.substring(firstComma + 1, secondComma);
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