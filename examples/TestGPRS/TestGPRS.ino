/*
  GPRS Class Test Sketch for Quectel EG915

  Tests GPRS/data connection functionality
*/

#include <Arduino_ConnectionHandler.h>

// Configuration
const char* PIN = "";
const char* APN = "wap.tim.it";
const char* USERNAME = "";
const char* PASSWORD = "";

// Objects
GSM gsm(true);   // Debug enabled
GPRS gprs;

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

void printStatus(GSM3_NetworkStatus_t status) {
  Serial.print("Status: ");
  switch (status) {
    case ERROR: Serial.println("ERROR"); break;
    case IDLE: Serial.println("IDLE"); break;
    case CONNECTING: Serial.println("CONNECTING"); break;
    case GSM_READY: Serial.println("GSM_READY"); break;
    case GPRS_READY: Serial.println("GPRS_READY"); break;
    case TRANSPARENT_CONNECTED: Serial.println("TRANSPARENT_CONNECTED"); break;
    case GSM_OFF: Serial.println("GSM_OFF"); break;
    default: Serial.print("UNKNOWN ("); Serial.print(status); Serial.println(")"); break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("\n=== GPRS Class Test Suite for EG915 ===\n");

  // Test 1: Initialize GSM first
  Serial.println("Test 1: Initialize GSM module");
  GSM3_NetworkStatus_t status = gsm.begin(PIN, true, true);
  printStatus(status);
  printTestResult("GSM initialization", status == GSM_READY);

  if (status != GSM_READY) {
    Serial.println("\n✗ GSM not ready, cannot continue with GPRS tests");
    return;
  }

  delay(1000);

  // Test 2: Attach GPRS
  Serial.println("\nTest 2: Attach GPRS (may take 30-60 seconds)");
  unsigned long startTime = millis();
  status = gprs.attachGPRS(APN, USERNAME, PASSWORD, true);
  unsigned long elapsed = (millis() - startTime) / 1000;

  Serial.print("GPRS attach took ");
  Serial.print(elapsed);
  Serial.println(" seconds");
  printStatus(status);
  printTestResult("GPRS attach", status == GPRS_READY);

  if (status != GPRS_READY) {
    Serial.println("\n✗ GPRS not ready, skipping remaining tests");
    return;
  }

  delay(1000);

  // Test 3: Get IP address
  Serial.println("\nTest 3: Get assigned IP address");
  IPAddress ip = gprs.getIPAddress();
  Serial.print("IP Address: ");
  Serial.println(ip);
  bool hasValidIP = (ip[0] != 0 || ip[1] != 0 || ip[2] != 0 || ip[3] != 0);
  printTestResult("Get IP address", hasValidIP);
  delay(500);

  // Test 4: DNS resolution
  Serial.println("\nTest 4: DNS resolution (google.com)");
  IPAddress googleIP;
  int dnsResult = gprs.hostByName("google.com", googleIP);
  if (dnsResult == 1) {
    Serial.print("google.com resolved to: ");
    Serial.println(googleIP);
  } else {
    Serial.println("DNS resolution failed");
  }
  printTestResult("DNS resolution", dnsResult == 1);
  delay(500);

  // Test 5: Ping Google DNS
  Serial.println("\nTest 5: Ping Google DNS (8.8.8.8)");
  int pingTime = gprs.ping(IPAddress(8, 8, 8, 8));
  if (pingTime > 0) {
    Serial.print("Ping time: ");
    Serial.print(pingTime);
    Serial.println(" ms");
  } else {
    Serial.print("Ping failed with code: ");
    Serial.println(pingTime);
  }
  printTestResult("Ping by IP", pingTime > 0);
  delay(500);

  // Test 6: Ping by hostname
  Serial.println("\nTest 6: Ping by hostname (google.com)");
  pingTime = gprs.ping("google.com");
  if (pingTime > 0) {
    Serial.print("Ping time: ");
    Serial.print(pingTime);
    Serial.println(" ms");
  } else {
    Serial.print("Ping failed with code: ");
    Serial.println(pingTime);
  }
  printTestResult("Ping by hostname", pingTime > 0);
  delay(500);

  // Test 7: Check GPRS status
  Serial.println("\nTest 7: Check GPRS status");
  status = gprs.status();
  printStatus(status);
  printTestResult("GPRS status check", status == GPRS_READY);
  delay(500);

  // Test 8: Multiple ping test
  Serial.println("\nTest 8: Multiple pings (stress test)");
  bool allPingsOk = true;
  for (int i = 0; i < 3; i++) {
    Serial.print("  Ping ");
    Serial.print(i + 1);
    Serial.print(": ");

    pingTime = gprs.ping(IPAddress(8, 8, 8, 8));
    if (pingTime > 0) {
      Serial.print(pingTime);
      Serial.println(" ms");
    } else {
      Serial.println("FAILED");
      allPingsOk = false;
    }
    delay(1000);
  }
  printTestResult("Multiple pings", allPingsOk);
  delay(500);

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
  Serial.println("  status   - Check GPRS status");
  Serial.println("  ip       - Get IP address");
  Serial.println("  dns      - DNS resolve hostname");
  Serial.println("  ping     - Ping host");
  Serial.println("  detach   - Detach GPRS");
  Serial.println("  attach   - Attach GPRS");
  Serial.print("> ");
}

void loop() {
  MODEM.poll();

  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();

    if (command.length() == 0) {
      Serial.print("> ");
      return;
    }

    Serial.println();

    if (command == "status") {
      Serial.print("GPRS Status: ");
      printStatus(gprs.status());
    }
    else if (command == "ip") {
      IPAddress ip = gprs.getIPAddress();
      Serial.print("IP Address: ");
      Serial.println(ip);
    }
    else if (command.startsWith("dns ")) {
      String hostname = command.substring(4);
      hostname.trim();

      IPAddress ip;
      Serial.print("Resolving ");
      Serial.print(hostname);
      Serial.print("... ");

      if (gprs.hostByName(hostname.c_str(), ip)) {
        Serial.println(ip);
      } else {
        Serial.println("FAILED");
      }
    }
    else if (command.startsWith("ping ")) {
      String host = command.substring(5);
      host.trim();

      Serial.print("Pinging ");
      Serial.print(host);
      Serial.print("... ");

      int result = gprs.ping(host);
      if (result > 0) {
        Serial.print(result);
        Serial.println(" ms");
      } else {
        Serial.print("FAILED (code ");
        Serial.print(result);
        Serial.println(")");
      }
    }
    else if (command == "detach") {
      Serial.println("Detaching GPRS...");
      GSM3_NetworkStatus_t status = gprs.detachGPRS(true);
      Serial.print("Result: ");
      printStatus(status);
    }
    else if (command == "attach") {
      Serial.println("Attaching GPRS...");
      GSM3_NetworkStatus_t status = gprs.attachGPRS(APN, USERNAME, PASSWORD, true);
      Serial.print("Result: ");
      printStatus(status);
    }
    else {
      Serial.print("Unknown command: ");
      Serial.println(command);
    }

    Serial.print("\n> ");
  }

  delay(10);
}