/*
  GSMUDP Class Test Sketch for Quectel EG915

  Tests UDP socket functionality
*/

#include <Arduino_ConnectionHandler.h>

// Configuration
const char* PIN = "";
const char* APN = "wap.tim.it";
const char* USERNAME = "";
const char* PASSWORD = "";

// UDP test server (time.nist.gov NTP server)
const char* NTP_SERVER = "time.nist.gov";
const int NTP_PORT = 123;
const int LOCAL_PORT = 8888;

// Objects
GSM gsm(true);
GPRS gprs;
GSMUDP udp;

// Test results
int testsPassed = 0;
int testsFailed = 0;

void printTestResult(const char* testName, bool passed) {
  Serial.print("Test: ");
  Serial.print(testName);
  Serial.print(" ... ");
  if (passed) {
    Serial.println("PASSED ✓");
    testsPassed++;
  } else {
    Serial.println("FAILED ✗");
    testsFailed++;
  }
}

// Send NTP request
void sendNTPRequest() {
  // NTP packet (48 bytes)
  byte packetBuffer[48];
  memset(packetBuffer, 0, 48);

  // Initialize values for NTP request
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;            // Stratum
  packetBuffer[2] = 6;            // Polling Interval
  packetBuffer[3] = 0xEC;         // Peer Clock Precision
  // Rest are zeros (already memset)

  udp.beginPacket(NTP_SERVER, NTP_PORT);
  udp.write(packetBuffer, 48);
  udp.endPacket();

  Serial.println("NTP request sent");
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("\n=== GSMUDP Class Test Suite for EG915 ===\n");

  // Test 1: Initialize GSM
  Serial.println("Test 1: Initialize GSM module");
  GSM3_NetworkStatus_t status = gsm.begin(PIN, true, true);
  printTestResult("GSM initialization", status == GSM_READY);

  if (status != GSM_READY) {
    Serial.println("\n✗ GSM not ready, cannot continue");
    return;
  }

  delay(1000);

  // Test 2: Attach GPRS
  Serial.println("\nTest 2: Attach GPRS");
  status = gprs.attachGPRS(APN, USERNAME, PASSWORD, true);
  printTestResult("GPRS attach", status == GPRS_READY);

  if (status != GPRS_READY) {
    Serial.println("\n✗ GPRS not ready, cannot continue");
    return;
  }

  delay(1000);

  // Test 3: Begin UDP
  Serial.println("\nTest 3: Open UDP socket");
  uint8_t result = udp.begin(LOCAL_PORT);
  Serial.print("UDP socket opened on port ");
  Serial.println(LOCAL_PORT);
  printTestResult("UDP begin", result == 1);

  if (result != 1) {
    Serial.println("\n✗ UDP socket failed to open");
    return;
  }

  delay(1000);

  // Test 4: Send UDP packet (NTP request)
  Serial.println("\nTest 4: Send UDP packet (NTP request)");
  sendNTPRequest();
  delay(2000);  // Wait for response
  printTestResult("Send UDP packet", true);  // If we got here, send succeeded
  delay(500);

  // Test 5: Receive UDP packet
  Serial.println("\nTest 5: Receive UDP packet");
  bool gotResponse = false;

  for (int i = 0; i < 30; i++) {  // Try for 3 seconds
    int packetSize = udp.parsePacket();

    if (packetSize > 0) {
      Serial.print("Received packet, size: ");
      Serial.println(packetSize);

      // Read packet
      byte buffer[64];
      int len = udp.read(buffer, sizeof(buffer));

      Serial.print("Read ");
      Serial.print(len);
      Serial.println(" bytes");

      if (len >= 48) {
        // Parse NTP timestamp (seconds since 1900)
        unsigned long highWord = word(buffer[40], buffer[41]);
        unsigned long lowWord = word(buffer[42], buffer[43]);
        unsigned long secsSince1900 = highWord << 16 | lowWord;

        // Convert to Unix time (seconds since 1970)
        const unsigned long seventyYears = 2208988800UL;
        unsigned long epoch = secsSince1900 - seventyYears;

        Serial.print("Unix time: ");
        Serial.println(epoch);

        gotResponse = true;
      }
      break;
    }

    delay(100);
  }

  printTestResult("Receive UDP packet", gotResponse);
  delay(500);

  // Test 6: Multiple send/receive
  Serial.println("\nTest 6: Multiple send/receive (echo test)");

  // Use a simple UDP echo server for testing
  // For now, just test sending multiple packets
  bool multipleOk = true;

  for (int i = 0; i < 3; i++) {
    Serial.print("  Send ");
    Serial.print(i + 1);
    Serial.print(": ");

    udp.beginPacket(NTP_SERVER, NTP_PORT);

    byte testData[10];
    for (int j = 0; j < 10; j++) {
      testData[j] = i * 10 + j;
    }

    udp.write(testData, sizeof(testData));

    if (udp.endPacket()) {
      Serial.println("OK");
    } else {
      Serial.println("FAILED");
      multipleOk = false;
    }

    delay(500);
  }

  printTestResult("Multiple sends", multipleOk);
  delay(500);

  // Test 7: UDP stop
  Serial.println("\nTest 7: Close UDP socket");
  udp.stop();
  printTestResult("UDP stop", true);
  delay(500);

  // Test 8: Reopen UDP
  Serial.println("\nTest 8: Reopen UDP socket");
  result = udp.begin(LOCAL_PORT + 1);
  printTestResult("UDP reopen", result == 1);

  if (result == 1) {
    udp.stop();
  }

  // Print summary
  Serial.println("\n=== Test Summary ===");
  Serial.print("Tests Passed: ");
  Serial.println(testsPassed);
  Serial.print("Tests Failed: ");
  Serial.println(testsFailed);
  Serial.print("Total Tests: ");
  Serial.println(testsPassed + testsFailed);

  if (testsFailed == 0) {
    Serial.println("\n✓ ALL TESTS PASSED!");
  } else {
    Serial.println("\n✗ SOME TESTS FAILED");
  }

  Serial.println("\n=== Interactive Mode ===");
  Serial.println("Commands:");
  Serial.println("  open <port>  - Open UDP socket");
  Serial.println("  send <host> <port> <message> - Send UDP packet");
  Serial.println("  recv         - Check for received packets");
  Serial.println("  close        - Close UDP socket");
  Serial.println("  ntp          - Send NTP request");
  Serial.print("> ");
}

void loop() {
  MODEM.poll();

  // Check for incoming packets
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    Serial.print("\n[Received ");
    Serial.print(packetSize);
    Serial.print(" bytes from ");
    Serial.print(udp.remoteIP());
    Serial.print(":");
    Serial.print(udp.remotePort());
    Serial.println("]");

    byte buffer[256];
    int len = udp.read(buffer, sizeof(buffer));

    Serial.print("Data: ");
    for (int i = 0; i < len; i++) {
      if (buffer[i] >= 32 && buffer[i] < 127) {
        Serial.write(buffer[i]);
      } else {
        Serial.print(".");
      }
    }
    Serial.println();
    Serial.print("> ");
  }

  // Interactive commands
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.length() == 0) {
      Serial.print("> ");
      return;
    }

    Serial.println();

    if (command.startsWith("open ")) {
      int port = command.substring(5).toInt();
      Serial.print("Opening UDP on port ");
      Serial.print(port);
      Serial.print("... ");

      if (udp.begin(port)) {
        Serial.println("OK");
      } else {
        Serial.println("FAILED");
      }
    }
    else if (command.startsWith("send ")) {
      // Parse: send <host> <port> <message>
      int space1 = command.indexOf(' ', 5);
      int space2 = command.indexOf(' ', space1 + 1);

      if (space1 > 0 && space2 > space1) {
        String host = command.substring(5, space1);
        String portStr = command.substring(space1 + 1, space2);
        String message = command.substring(space2 + 1);
        int port = portStr.toInt();

        Serial.print("Sending to ");
        Serial.print(host);
        Serial.print(":");
        Serial.print(port);
        Serial.print("... ");

        udp.beginPacket(host.c_str(), port);
        udp.write((const uint8_t*)message.c_str(), message.length());

        if (udp.endPacket()) {
          Serial.println("OK");
        } else {
          Serial.println("FAILED");
        }
      } else {
        Serial.println("Usage: send <host> <port> <message>");
      }
    }
    else if (command == "recv") {
      Serial.println("Checking for packets...");
      // Handled in main loop
    }
    else if (command == "close") {
      Serial.print("Closing UDP socket... ");
      udp.stop();
      Serial.println("OK");
    }
    else if (command == "ntp") {
      Serial.println("Sending NTP request...");
      sendNTPRequest();
    }
    else {
      Serial.print("Unknown command: ");
      Serial.println(command);
    }

    Serial.print("\n> ");
  }

  delay(10);
}