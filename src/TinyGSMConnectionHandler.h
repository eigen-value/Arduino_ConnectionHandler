/*
This file is part of the Arduino_ConnectionHandler library.

  Copyright (c) 2019 Arduino SA

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/


#ifndef TINY_GSMCONNECTION_HANDLER_H
#define TINY_GSMCONNECTION_HANDLER_H

/******************************************************************************
  INCLUDE
 ******************************************************************************/

#include "ConnectionHandlerInterface.h"

#if defined(ARDUINO_ARCH_ESP32)
  #include <compat/TinyGSMCompat.h>
#endif

#ifndef BOARD_HAS_TINY_GSM
  #error "Board doesn't support GSM"
#endif

/******************************************************************************
  CLASS DECLARATION
 ******************************************************************************/

class TinyGSMConnectionHandler: public ConnectionHandler {
  public:
    TinyGSMConnectionHandler();
    TinyGSMConnectionHandler(const char * pin, const char * apn, const char * login, const char * pass, bool const keep_alive = true);


    virtual unsigned long getTime() override;
    virtual Client & getClient() override { return _gsm_client; };
    virtual UDP & getUDP() override { return _gsm_udp; };


  protected:

    virtual NetworkConnectionState update_handleInit         () override;
    virtual NetworkConnectionState update_handleConnecting   () override;
    virtual NetworkConnectionState update_handleConnected    () override;
    virtual NetworkConnectionState update_handleDisconnecting() override;
    virtual NetworkConnectionState update_handleDisconnected () override;


  private:

    GSM _gsm;
    GPRS _gprs;
    GSMUDP _gsm_udp;
    GSMClient _gsm_client;
};



#endif //TINY_GSMCONNECTION_HANDLER_H
