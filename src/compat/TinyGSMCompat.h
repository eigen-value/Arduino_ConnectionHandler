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

#include "GSM.h"
#include "GPRS.h"
#include "GSMClient.h"
#include "GSMUdp.h"

#endif //TINY_GSM_COMPAT_H
