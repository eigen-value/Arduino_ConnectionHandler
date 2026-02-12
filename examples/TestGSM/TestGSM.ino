/*
  GSM Class Test Sketch for Quectel EG915
  
  Tests the GSM class initialization and network registration
*/

#include <Arduino_ConnectionHandler.h>

// SIM PIN (leave empty "" if no PIN)
const char* PIN = "";

// Test with debug enabled
GSM gsm(true);  // Enable debug output

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
    case ERROR:
      Serial.println("ERROR");
      break;
    case IDLE:
      Serial.println("IDLE");
      break;
    case CONNECTING:
      Serial.println("CONNECTING");
      break;
    case GSM_READY:
      Serial.println("GSM_READY");
      break;
    case GPRS_READY:
      Serial.println("GPRS_READY");
      break;
    case TRANSPARENT_CONNECTED:
      Serial.println("TRANSPARENT_CONNECTED");
      break;
    case GSM_OFF:
      Serial.println("GSM_OFF");
      break;
    default:
      Serial.print("UNKNOWN (");
      Serial.print(status);
      Serial.println(")");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  
  Serial.println("\n=== GSM Class Test Suite for EG915 ===\n");

  // Test 1: Begin GSM (synchronous mode)
  Serial.println("Test 1: Initialize GSM module (synchronous, may take 30-60 seconds)");
  Serial.println("This will:");
  Serial.println("  - Reset and initialize modem");
  Serial.println("  - Check SIM card");
  Serial.println("  - Unlock SIM (if PIN provided)");
  Serial.println("  - Configure modem settings");
  Serial.println("  - Wait for network registration");
  Serial.println();
  
  unsigned long startTime = millis();
  GSM3_NetworkStatus_t status = gsm.begin(PIN, true, true);  // restart=true, synchronous=true
  unsigned long elapsed = (millis() - startTime) / 1000;
  
  Serial.print("Initialization took ");
  Serial.print(elapsed);
  Serial.println(" seconds");
  
  printStatus(status);
  printTestResult("GSM begin (synchronous)", status == GSM_READY);
  delay(1000);

  // Test 2: Check status
  Serial.println("\nTest 2: Check GSM status");
  status = gsm.status();
  printStatus(status);
  printTestResult("GSM status()", status == GSM_READY);
  delay(500);

  // Test 3: Check if access is alive (network registration)
  Serial.println("\nTest 3: Check network registration");
  int alive = gsm.isAccessAlive();
  Serial.print("Network registered: ");
  Serial.println(alive ? "YES" : "NO");
  printTestResult("isAccessAlive()", alive == 1);
  delay(500);

  // Test 4: Get network time (UTC)
  Serial.println("\nTest 4: Get network time (UTC)");
  unsigned long networkTime = gsm.getTime();
  if (networkTime > 0) {
    Serial.print("Network time (UTC): ");
    Serial.println(networkTime);
    
    // Convert to human readable
    time_t t = networkTime;
    struct tm* timeinfo = gmtime(&t);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", timeinfo);
    Serial.println(buffer);
  } else {
    Serial.println("Failed to get network time");
  }
  printTestResult("getTime()", networkTime > 1600000000);  // After Sept 2020
  delay(500);

  // Test 5: Get local time
  Serial.println("\nTest 5: Get local time");
  unsigned long localTime = gsm.getLocalTime();
  if (localTime > 0) {
    Serial.print("Local time: ");
    Serial.println(localTime);
    
    // Convert to human readable
    time_t t = localTime;
    struct tm* timeinfo = localtime(&t);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    Serial.println(buffer);
  } else {
    Serial.println("Failed to get local time");
  }
  printTestResult("getLocalTime()", localTime > 1600000000);
  delay(500);

  // Test 6: Enter low power mode
  Serial.println("\nTest 6: Enter low power mode");
  int lpmResult = gsm.lowPowerMode();
  Serial.print("Low power mode result: ");
  Serial.println(lpmResult);
  printTestResult("lowPowerMode()", lpmResult == 1);
  delay(3000);  // Wait in low power mode

  // Test 7: Exit low power mode
  Serial.println("\nTest 7: Exit low power mode");
  int noLpmResult = gsm.noLowPowerMode();
  Serial.print("No low power mode result: ");
  Serial.println(noLpmResult);
  printTestResult("noLowPowerMode()", noLpmResult == 1);
  delay(2000);  // Wait for modem to wake up

  // Test 8: Verify still registered after low power mode
  Serial.println("\nTest 8: Check registration after low power mode");
  alive = gsm.isAccessAlive();
  Serial.print("Still registered: ");
  Serial.println(alive ? "YES" : "NO");
  printTestResult("Registration after LPM", alive == 1);
  delay(500);

  // Test 9: Multiple status checks
  Serial.println("\nTest 9: Multiple status checks (stress test)");
  bool allAlive = true;
  for (int i = 0; i < 5; i++) {
    int check = gsm.isAccessAlive();
    Serial.print("  Check ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(check ? "OK" : "FAIL");
    if (!check) allAlive = false;
    delay(200);
  }
  printTestResult("Multiple status checks", allAlive);
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
    Serial.println("\nCommon issues:");
    Serial.println("- No SIM card or SIM not activated");
    Serial.println("- Wrong PIN code");
    Serial.println("- Poor signal strength (check antenna)");
    Serial.println("- Network not available in your area");
    Serial.println("- Modem not properly powered");
  }
  
  Serial.println("\n=== Interactive Mode ===");
  Serial.println("Type commands:");
  Serial.println("  status  - Check current status");
  Serial.println("  time    - Get network time");
  Serial.println("  local   - Get local time");
  Serial.println("  alive   - Check if registered");
  Serial.println("  lpm     - Enter low power mode");
  Serial.println("  nolpm   - Exit low power mode");
  Serial.println("  shutdown - Shutdown modem");
  Serial.print("> ");
}

void loop() {
  // Poll modem
  MODEM.poll();
  
  // Interactive commands
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
      Serial.print("GSM Status: ");
      printStatus(gsm.status());
    }
    else if (command == "time") {
      unsigned long t = gsm.getTime();
      if (t > 0) {
        Serial.print("UTC: ");
        Serial.print(t);
        Serial.print(" (");
        time_t tt = t;
        struct tm* ti = gmtime(&tt);
        char buf[80];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", ti);
        Serial.print(buf);
        Serial.println(")");
      } else {
        Serial.println("Failed to get time");
      }
    }
    else if (command == "local") {
      unsigned long t = gsm.getLocalTime();
      if (t > 0) {
        Serial.print("Local: ");
        Serial.print(t);
        Serial.print(" (");
        time_t tt = t;
        struct tm* ti = localtime(&tt);
        char buf[80];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ti);
        Serial.print(buf);
        Serial.println(")");
      } else {
        Serial.println("Failed to get local time");
      }
    }
    else if (command == "alive") {
      int alive = gsm.isAccessAlive();
      Serial.print("Network registered: ");
      Serial.println(alive ? "YES" : "NO");
    }
    else if (command == "lpm") {
      Serial.println("Entering low power mode...");
      int result = gsm.lowPowerMode();
      Serial.print("Result: ");
      Serial.println(result ? "OK" : "FAILED");
    }
    else if (command == "nolpm") {
      Serial.println("Exiting low power mode...");
      int result = gsm.noLowPowerMode();
      Serial.print("Result: ");
      Serial.println(result ? "OK" : "FAILED");
    }
    else if (command == "shutdown") {
      Serial.println("Shutting down modem...");
      bool result = gsm.shutdown();
      Serial.print("Result: ");
      Serial.println(result ? "OK" : "FAILED");
      Serial.println("Modem is now OFF. Reset to restart.");
    }
    else {
      Serial.print("Unknown command: ");
      Serial.println(command);
    }
    
    Serial.print("\n> ");
  }
  
  delay(10);
}