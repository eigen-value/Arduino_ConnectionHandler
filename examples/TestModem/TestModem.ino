/*
  ModemClass Test Sketch

  Tests the ModemClass interface for basic AT modem communication.
  This sketch exercises the main functionality of the ModemClass.
*/

#include "Arduino_ConnectionHandler.h"

// Pin definitions - adjust for your hardware

#define testModem MODEM

// Example URC handler
class TestUrcHandler : public ModemUrcHandler {
public:
  void handleUrc(const String& urc) override {
    Serial.print("[URC Handler] Received: ");
    Serial.println(urc);
  }
};

TestUrcHandler myUrcHandler;

// Test counters
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
  delay(2000);

  Serial.println("\n=== ModemClass Test Suite ===\n");

  // Test 1: Begin modem
  Serial.println("Test 1: Initialize modem");
  testModem.debug(Serial);  // Enable debug output
  int result = testModem.begin(true);
  printTestResult("Modem begin()", result == 1);
  delay(1000);

  // Test 2: Autosense (detect baud rate)
  Serial.println("\nTest 2: Autosense modem");
  result = testModem.autosense(10000);
  printTestResult("Modem autosense()", result == 1);
  delay(500);

  // Test 3: Send AT command
  Serial.println("\nTest 3: Send basic AT command");
  testModem.send("AT");
  result = testModem.waitForResponse(1000);
  printTestResult("Send AT", result == 1);
  delay(500);

  // Test 4: Send formatted command
  Serial.println("\nTest 4: Send formatted command");
  testModem.sendf("AT+CPIN?");
  result = testModem.waitForResponse(1000);
  printTestResult("Send AT+CPIN?", result == 1);
  delay(500);

  // Test 5: Check ready state
  Serial.println("\nTest 5: Check modem ready state");
  result = testModem.ready();
  printTestResult("Modem ready()", result == 1);
  delay(500);

  // Test 6: Add URC handler
  Serial.println("\nTest 6: Add URC handler");
  testModem.addUrcHandler(&myUrcHandler);
  printTestResult("Add URC handler", true);
  delay(500);

  // Test 7: Trigger some URCs (network registration)
  Serial.println("\nTest 7: Enable URCs (network registration)");
  testModem.send("AT+CREG=2");  // Enable network registration URCs
  result = testModem.waitForResponse(1000);
  printTestResult("Enable CREG URCs", result == 1);
  delay(500);

  // Only if DTR is used
  // // Test 8: Low power mode
  // Serial.println("\nTest 8: Enter low power mode");
  // result = testModem.lowPowerMode();
  // printTestResult("Low power mode", result == 1);
  // delay(2000);
  //
  // // Test 9: Exit low power mode
  // Serial.println("\nTest 9: Exit low power mode");
  // result = testModem.noLowPowerMode();
  // printTestResult("No low power mode", result == 1);
  // delay(500);

  // Test 10: Get modem info
  Serial.println("\nTest 10: Get modem information");
  String response;
  testModem.setResponseDataStorage(&response);
  testModem.send("AT+CGMI");  // Manufacturer
  result = testModem.waitForResponse(1000, &response);
  Serial.print("Manufacturer: ");
  Serial.println(response);
  printTestResult("Get manufacturer", result == 1 && response.length() > 0);
  delay(500);

  // Test 11: Get model
  Serial.println("\nTest 11: Get modem model");
  response = "";
  testModem.send("AT+CGMM");  // Model
  result = testModem.waitForResponse(1000, &response);
  Serial.print("Model: ");
  Serial.println(response);
  printTestResult("Get model", result == 1 && response.length() > 0);
  delay(500);

  // Test 12: Get IMEI
  Serial.println("\nTest 12: Get IMEI");
  response = "";
  testModem.send("AT+GSN");  // IMEI
  result = testModem.waitForResponse(1000, &response);
  Serial.print("IMEI: ");
  Serial.println(response);
  printTestResult("Get IMEI", result == 1 && response.length() > 0);
  delay(500);

  // Test 13: Signal quality
  Serial.println("\nTest 13: Check signal quality");
  response = "";
  testModem.send("AT+CSQ");
  result = testModem.waitForResponse(1000, &response);
  Serial.print("Signal quality: ");
  Serial.println(response);
  printTestResult("Get signal quality", result == 1);
  delay(500);

  // Test 14: Remove URC handler
  Serial.println("\nTest 14: Remove URC handler");
  testModem.removeUrcHandler(&myUrcHandler);
  printTestResult("Remove URC handler", true);
  delay(500);

  // Test 15: Disable debug
  Serial.println("\nTest 15: Disable debug output");
  testModem.noDebug();
  printTestResult("Disable debug", true);
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
}

void loop() {
  // Poll modem for URCs and responses
  testModem.poll();

  // Echo any data from modem to Serial
  if (SerialGSM.available()) {
    Serial.write(SerialGSM.read());
  }

  // Send manual commands from Serial to modem
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.length() > 0) {
      Serial.print("\n> Sending: ");
      Serial.println(command);

      testModem.send(command);
      String response;
      int result = testModem.waitForResponse(5000, &response);

      Serial.print("Response (");
      Serial.print(result);
      Serial.print("): ");
      Serial.println(response);
    }
  }

  delay(10);
}