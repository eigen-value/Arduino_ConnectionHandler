/*
  GSMClient Test Sketch for Quectel EG915

  Tests TCP client functionality (required for Arduino IoT Cloud)
*/

#include "Arduino_ConnectionHandler.h"

// Configuration
const char* PIN = "";
const char* APN = "wap.tim.it";
const char* USERNAME = "";
const char* PASSWORD = "";

// Test HTTP server
const char* TEST_HOST = "example.com";
const int TEST_PORT = 80;

// Objects
GSM gsm(true);
GPRS gprs;
GSMClient client;

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

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("\n=== GSMClient Test Suite for EG915 ===\n");

  // Test 1: Initialize GSM
  Serial.println("Test 1: Initialize GSM");
  GSM3_NetworkStatus_t status = gsm.begin(PIN, true, true);
  printTestResult("GSM initialization", status == GSM_READY);

  if (status != GSM_READY) {
    Serial.println("\n✗ Cannot continue without GSM");
    return;
  }

  delay(1000);

  // Test 2: Attach GPRS
  Serial.println("\nTest 2: Attach GPRS");
  status = gprs.attachGPRS(APN, USERNAME, PASSWORD, true);
  printTestResult("GPRS attach", status == GPRS_READY);

  if (status != GPRS_READY) {
    Serial.println("\n✗ Cannot continue without GPRS");
    return;
  }

  delay(1000);

  // Test 3: Connect to HTTP server
  Serial.println("\nTest 3: Connect to HTTP server (example.com:80)");
  Serial.print("Connecting to ");
  Serial.print(TEST_HOST);
  Serial.print(":");
  Serial.print(TEST_PORT);
  Serial.println("...");

  int result = client.connect(TEST_HOST, TEST_PORT);
  printTestResult("TCP connect", result == 1);

  if (result != 1) {
    Serial.println("\n✗ Connection failed, skipping remaining tests");
    return;
  }

  delay(1000);

  // Test 4: Check connection status
  Serial.println("\nTest 4: Check connection status");
  bool isConnected = client.connected();
  Serial.print("Connected: ");
  Serial.println(isConnected ? "YES" : "NO");
  printTestResult("Connection status", isConnected);
  delay(500);

  // Test 5: Send HTTP request
  Serial.println("\nTest 5: Send HTTP GET request");
  String request = "GET / HTTP/1.1\r\n";
  request += "Host: example.com\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";

  size_t written = client.write((const uint8_t*)request.c_str(), request.length());
  Serial.print("Wrote ");
  Serial.print(written);
  Serial.print("/");
  Serial.print(request.length());
  Serial.println(" bytes");
  printTestResult("Send HTTP request", written == request.length());
  delay(2000);

  // Test 6: Receive HTTP response
  Serial.println("\nTest 6: Receive HTTP response");
  bool gotResponse = false;
  String response = "";

  unsigned long start = millis();
  while (millis() - start < 10000) {
    if (client.available()) {
      char c = client.read();
      response += c;

      if (response.indexOf("\r\n\r\n") > 0) {
        gotResponse = true;
        break;
      }
    }
    delay(10);
  }

  if (gotResponse) {
    Serial.println("Response headers:");
    int headerEnd = response.indexOf("\r\n\r\n");
    Serial.println(response.substring(0, headerEnd));

    // Check for HTTP 200 OK
    bool is200 = response.indexOf("200 OK") > 0;
    printTestResult("Receive HTTP 200 OK", is200);
  } else {
    Serial.println("No response received");
    printTestResult("Receive HTTP response", false);
  }
  delay(500);

  // Test 7: Close connection
  Serial.println("\nTest 7: Close connection");
  client.stop();
  delay(500);

  bool isClosed = !client.connected();
  Serial.print("Connection closed: ");
  Serial.println(isClosed ? "YES" : "NO");
  printTestResult("Close connection", isClosed);
  delay(500);

  // Test 8: Reconnect
  Serial.println("\nTest 8: Reconnect and send another request");
  result = client.connect(TEST_HOST, TEST_PORT);
  printTestResult("Reconnect", result == 1);

  if (result == 1) {
    client.write((const uint8_t*)request.c_str(), request.length());
    delay(1000);

    bool hasData = false;
    start = millis();
    while (millis() - start < 5000) {
      if (client.available()) {
        hasData = true;
        break;
      }
      delay(100);
    }

    printTestResult("Receive after reconnect", hasData);
    client.stop();
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
    Serial.println("\nYour GSMClient is ready for Arduino IoT Cloud!");
  } else {
    Serial.println("\n✗ SOME TESTS FAILED");
  }

  Serial.println("\n=== Interactive Mode ===");
  Serial.println("Commands:");
  Serial.println("  connect <host> <port> - Connect to server");
  Serial.println("  send <text>           - Send text");
  Serial.println("  read                  - Read available data");
  Serial.println("  close                 - Close connection");
  Serial.print("> ");
}

void loop() {
  MODEM.poll();

  // Show incoming data
  if (client.available()) {
    Serial.print("[RX] ");
    while (client.available()) {
      Serial.write(client.read());
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

    if (command.startsWith("connect ")) {
      int space = command.indexOf(' ', 8);
      if (space > 0) {
        String host = command.substring(8, space);
        int port = command.substring(space + 1).toInt();

        Serial.print("Connecting to ");
        Serial.print(host);
        Serial.print(":");
        Serial.print(port);
        Serial.print("... ");

        if (client.connect(host.c_str(), port)) {
          Serial.println("OK");
        } else {
          Serial.println("FAILED");
        }
      }
    }
    else if (command.startsWith("send ")) {
      String text = command.substring(5);
      text += "\r\n";

      size_t written = client.write((const uint8_t*)text.c_str(), text.length());
      Serial.print("Sent ");
      Serial.print(written);
      Serial.println(" bytes");
    }
    else if (command == "read") {
      Serial.println("Reading...");
      unsigned long start = millis();
      while (millis() - start < 3000) {
        if (client.available()) {
          Serial.write(client.read());
        }
        delay(10);
      }
      Serial.println();
    }
    else if (command == "close") {
      client.stop();
      Serial.println("Connection closed");
    }
    else {
      Serial.print("Unknown command: ");
      Serial.println(command);
    }

    Serial.print("\n> ");
  }

  delay(10);
}