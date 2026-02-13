#include "GPRS.h"

enum {
  GPRS_STATE_IDLE,
  GPRS_STATE_ATTACH,
  GPRS_STATE_WAIT_ATTACH_RESPONSE,
  GPRS_STATE_SET_APN,
  GPRS_STATE_WAIT_SET_APN_RESPONSE,
  GPRS_STATE_ACTIVATE_PDP,
  GPRS_STATE_WAIT_ACTIVATE_PDP_RESPONSE,
  GPRS_STATE_CHECK_IP,
  GPRS_STATE_WAIT_CHECK_IP_RESPONSE,

  GPRS_STATE_DEACTIVATE_PDP,
  GPRS_STATE_WAIT_DEACTIVATE_PDP_RESPONSE,
  GPRS_STATE_DEATTACH,
  GPRS_STATE_WAIT_DEATTACH_RESPONSE
};

GPRS::GPRS() :
  _apn(NULL),
  _username(NULL),
  _password(NULL),
  _status(IDLE),
  _timeout(0)
{
  MODEM.addUrcHandler(this);
}

GPRS::~GPRS()
{
  MODEM.removeUrcHandler(this);
}

GSM3_NetworkStatus_t GPRS::attachGPRS(const char* apn, const char* user_name, const char* password, bool synchronous)
{
  _apn = apn;
  _username = user_name;
  _password = password;

  _state = GPRS_STATE_ATTACH;
  _status = CONNECTING;

  if (synchronous) {
    unsigned long start = millis();

    while (ready() == 0) {
      if (_timeout && !((millis() - start) < _timeout)) {
        _status = ERROR;
        break;
      }

      delay(100);
    }
  } else {
    ready();
  }

  return _status;
}

GSM3_NetworkStatus_t GPRS::detachGPRS(bool synchronous)
{
  _state = GPRS_STATE_DEACTIVATE_PDP;

  if (synchronous) {
    while (ready() == 0) {
      delay(100);
    }
  } else {
    ready();
  }

  return _status;
}

int GPRS::ready()
{
  int ready = MODEM.ready();

  if (ready == 0) {
    return 0;
  }

  switch (_state) {
    case GPRS_STATE_IDLE:
    default: {
      break;
    }

    case GPRS_STATE_ATTACH: {
      MODEM.send("AT+CGATT=1");
      _state = GPRS_STATE_WAIT_ATTACH_RESPONSE;
      ready = 0;
      break;
    }

    case GPRS_STATE_WAIT_ATTACH_RESPONSE: {
      if (ready > 1) {
        _state = GPRS_STATE_IDLE;
        _status = ERROR;
      } else {
        _state = GPRS_STATE_SET_APN;
        ready = 0;
      }
      break;
    }

    case GPRS_STATE_SET_APN: {
      // EG915: AT+QICSGP=<contextID>,<context_type>,"<APN>","<username>","<password>",<authentication>
      // Context type: 1=IPv4
      // Authentication: 0=None, 1=PAP, 2=CHAP, 3=PAP or CHAP
      int auth = 0;
      if (_username && strlen(_username) > 0) {
        auth = 3;  // PAP or CHAP
      }

      MODEM.sendf("AT+QICSGP=1,1,\"%s\",\"%s\",\"%s\",%d",
                  _apn,
                  _username ? _username : "",
                  _password ? _password : "",
                  auth);
      _state = GPRS_STATE_WAIT_SET_APN_RESPONSE;
      ready = 0;
      break;
    }

    case GPRS_STATE_WAIT_SET_APN_RESPONSE: {
      if (ready > 1) {
        _state = GPRS_STATE_IDLE;
        _status = ERROR;
      } else {
        _state = GPRS_STATE_ACTIVATE_PDP;
        ready = 0;
      }
      break;
    }

    case GPRS_STATE_ACTIVATE_PDP: {
      // EG915: AT+QIACT=<contextID>
      MODEM.send("AT+QIACT=1");
      _state = GPRS_STATE_WAIT_ACTIVATE_PDP_RESPONSE;
      ready = 0;
      break;
    }

    case GPRS_STATE_WAIT_ACTIVATE_PDP_RESPONSE: {
      if (ready > 1) {
        // Might already be activated, check anyway
        _state = GPRS_STATE_CHECK_IP;
        ready = 0;
      } else {
        _state = GPRS_STATE_CHECK_IP;
        ready = 0;
      }
      break;
    }

    case GPRS_STATE_CHECK_IP: {
      MODEM.setResponseDataStorage(&_response);
      MODEM.send("AT+QIACT?");
      _state = GPRS_STATE_WAIT_CHECK_IP_RESPONSE;
      ready = 0;
      break;
    }

    case GPRS_STATE_WAIT_CHECK_IP_RESPONSE: {
      // Response: +QIACT: <contextID>,<context_state>,<context_type>,"<IP_address>"
      // Example: +QIACT: 1,1,1,"10.123.45.67"
      if (ready > 1) {
        _state = GPRS_STATE_IDLE;
        _status = ERROR;
      } else if (_response.indexOf("+QIACT: 1,1,1") >= 0) {
        // Context 1 is active (state=1) with IPv4 (type=1)
        _state = GPRS_STATE_IDLE;
        _status = GPRS_READY;
      } else {
        _state = GPRS_STATE_IDLE;
        _status = ERROR;
      }
      break;
    }

    case GPRS_STATE_DEACTIVATE_PDP: {
      // EG915: AT+QIDEACT=<contextID>
      MODEM.send("AT+QIDEACT=1");
      _state = GPRS_STATE_WAIT_DEACTIVATE_PDP_RESPONSE;
      ready = 0;
      break;
    }

    case GPRS_STATE_WAIT_DEACTIVATE_PDP_RESPONSE: {
      if (ready > 1) {
        _state = GPRS_STATE_IDLE;
        _status = ERROR;
      } else {
        _state = GPRS_STATE_DEATTACH;
        ready = 0;
      }
      break;
    }

    case GPRS_STATE_DEATTACH: {
      MODEM.send("AT+CGATT=0");
      _state = GPRS_STATE_WAIT_DEATTACH_RESPONSE;
      ready = 0;
      break;
    }

    case GPRS_STATE_WAIT_DEATTACH_RESPONSE: {
      if (ready > 1) {
        _state = GPRS_STATE_IDLE;
        _status = ERROR;
      } else {
        _state = GPRS_STATE_IDLE;
        _status = IDLE;
      }
      break;
    }
  }

  return ready;
}

IPAddress GPRS::getIPAddress()
{
  String response;

  // EG915: AT+QIACT? returns IP address
  MODEM.send("AT+QIACT?");
  if (MODEM.waitForResponse(1000, &response) == 1) {
    // Parse: +QIACT: 1,1,1,"10.123.45.67"
    int startQuote = response.indexOf('"');
    int endQuote = response.lastIndexOf('"');

    if (startQuote >= 0 && endQuote > startQuote) {
      String ipStr = response.substring(startQuote + 1, endQuote);

      IPAddress ip;
      if (ip.fromString(ipStr)) {
        return ip;
      }
    }
  }

  return IPAddress(0, 0, 0, 0);
}

void GPRS::setTimeout(unsigned long timeout)
{
  _timeout = timeout;
}

int GPRS::hostByName(const char* hostname, IPAddress& result)
{
  String response;

  // EG915: AT+QIDNSGIP=<contextID>,"<domain_name>"
  MODEM.sendf("AT+QIDNSGIP=1,\"%s\"", hostname);
  if (MODEM.waitForResponse(60000, &response) != 1) {
    return 0;
  }

  // Response: +QIURC: "dnsgip","<IP_address>"
  // Or: +QIURC: "dnsgip",<err>,<DNS_ttl>

  // Wait for URC
  _dnsResult = "";
  unsigned long start = millis();
  while (millis() - start < 5000) {
    MODEM.poll();
    if (_dnsResult.length() > 0) {
      break;
    }
    delay(10);
  }

  if (_dnsResult.length() > 0 && result.fromString(_dnsResult)) {
    return 1;
  }

  return 0;
}

int GPRS::ping(const char* hostname, uint8_t ttl)
{
  String response;

  _pingResult = 0;

  // EG915: AT+QPING=<contextID>,"<host>"[,<timeout>[,<pingnum>]]
  MODEM.sendf("AT+QPING=1,\"%s\",4,4", hostname);  // 4s timeout, 4 pings
  if (MODEM.waitForResponse(1000) != 1) {
    return GPRS_PING_ERROR;
  }

  // Wait for ping results (URCs)
  for (unsigned long start = millis(); (millis() - start) < 20000 && (_pingResult == 0);) {
    MODEM.poll();
    delay(10);
  }

  if (_pingResult == 0) {
    _pingResult = GPRS_PING_TIMEOUT;
  }

  return _pingResult;
}

int GPRS::ping(const String &hostname, uint8_t ttl)
{
  return ping(hostname.c_str(), ttl);
}

int GPRS::ping(IPAddress ip, uint8_t ttl)
{
  String host;
  host.reserve(15);

  host += ip[0];
  host += '.';
  host += ip[1];
  host += '.';
  host += ip[2];
  host += '.';
  host += ip[3];

  return ping(host, ttl);
}

GSM3_NetworkStatus_t GPRS::status()
{
  MODEM.poll();

  return _status;
}

void GPRS::handleUrc(const String& urc)
{
  // EG915 Ping URC: +QPING: <result>,"<IP_address>",<bytes>,<time>,<ttl>
  // Final summary: +QPING: <finresult>,<sent>,<rcvd>,<lost>,<min>,<max>,<avg>
  if (urc.startsWith("+QPING: 0,")) {
    // Individual ping success: +QPING: 0,"8.8.8.8",32,45,113
    int lastComma = urc.lastIndexOf(',');
    int secondLastComma = urc.lastIndexOf(',', lastComma - 1);

    if (secondLastComma > 0) {
      String timeStr = urc.substring(secondLastComma + 1, lastComma);
      _pingResult = timeStr.toInt();

      if (_pingResult <= 0) {
        _pingResult = GPRS_PING_ERROR;
      }
    }
  } else if (urc.startsWith("+QPING: ")) {
    // Check for error codes
    char resultCode = urc.charAt(8);
    if (resultCode == '5' || resultCode == '6' || resultCode == '5' || resultCode == '6') {
      // 569 = DNS resolution failed, 567 = Network error
      _pingResult = GPRS_PING_UNKNOWN_HOST;
    } else if (resultCode != '0') {
      _pingResult = GPRS_PING_ERROR;
    }
  } else if (urc.startsWith("+QIURC: \"dnsgip\",\"")) {
    // DNS resolution result
    int startQuote = 18;  // After +QIURC: "dnsgip","
    int endQuote = urc.indexOf('"', startQuote);

    if (endQuote > startQuote) {
      _dnsResult = urc.substring(startQuote, endQuote);
    }
  } else if (urc.startsWith("+QIURC: \"pdpdeact\"")) {
    // PDP context deactivated
    _status = IDLE;
  }
}