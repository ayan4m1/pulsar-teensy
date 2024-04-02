#include <Arduino.h>
#include <Button2.h>
#include <SD.h>
#include <SPI.h>
#include <USBHost_t36.h>

typedef struct {
  char cmd;
  double val;
} command_t;

#define SWITCH_PIN 1
#define SWITCH_HOLD_TIME_MS 2000

// customize as necessary
#define FILE_PATH "/test.csv"
#define CSV_SEPARATOR ','
#define MAX_COMMAND_LENGTH 16
#define MAX_COMMANDS 16
#define DEVICE_BAUD 115200

// do not modify
#define HOST_BAUD 9600

// comment to disable debug logging
#define DEBUG_LOGGING
// comment to let system start without debugger connection
#define WAIT_FOR_DEBUGGER
#ifdef DEBUG_LOGGING
#define LOG Serial
#else
#define LOG Serial1
#endif

USBHost usbHost;
USBSerial usbSerial(usbHost);

bool initialized = false;
bool driverActive = false;
bool serialActive = false;
const char *driverName = "SERIAL";

command_t commands[MAX_COMMANDS];
uint8_t commandCount = 0;

Button2 btn = Button2();

void handleButtonPress(Button2 &btn) {
  LOG.println(F("[BUTTON] Pressed!"));
  if (!driverActive || !serialActive) {
    LOG.println(F("[DEV] Serial connection is not active!"));
    return;
  }
  LOG.println(F("[HOST] Sending commands..."));

  for (uint8_t i = 0; i < commandCount; i++) {
    const command_t cmd = commands[i];
    char cmdStr[MAX_COMMAND_LENGTH];

    uint16_t wattage = 0, duration = 0;

    switch (cmd.cmd) {
      case 'W': {
        wattage = (uint16_t)cmd.val;
        LOG.printf(F("[ACT] Setting wattage to %dW\n"), wattage);
        sprintf(cmdStr, "P=%dW", wattage);
        usbSerial.println(cmdStr);
        break;
      }
      case 'F': {
        duration = (uint16_t)cmd.val;
        LOG.printf(F("[ACT] Firing for %ds\n"), duration);
        sprintf(cmdStr, "F=%dS", duration);
        usbSerial.println(cmdStr);
        delay(duration * 1e3);
        break;
      }
      case 'P': {
        duration = (uint16_t)cmd.val;
        LOG.printf(F("[ACT] Pausing for %ds\n"), duration);
        delay(duration * 1e3);
        break;
      }
    }
  }

  LOG.println(F("[HOST] Done sending commands..."));
}

void handleDeviceDisconnect() {
  if (serialActive) {
    LOG.println(F("[HOST] Closing serial connection to device..."));
    serialActive = false;
  }

  if (driverActive) {
    LOG.printf(F("[DEV] %s Disconnected\n"), driverName);
    driverActive = false;
    btn.reset();
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void handleDeviceConnect() {
  LOG.println(F("[HOST] Opening serial connection to device..."));
  usbSerial.begin(HOST_BAUD, USBHOST_SERIAL_8N1);
  while (!usbSerial) {
    delay(100);
  }
  serialActive = true;
  LOG.println(F("[HOST] Opened serial connection!"));

  const uint8_t *mfg = usbSerial.manufacturer();
  const uint8_t *prod = usbSerial.product();
  const uint8_t *serNum = usbSerial.serialNumber();

  if (!mfg || !*mfg) {
    mfg = 0;
  }
  if (!prod || !*prod) {
    prod = 0;
  }
  if (!serNum || !*serNum) {
    serNum = 0;
  }

  LOG.printf(F("[DEV] Connected %s (%x:%x)\n"), driverName,
             usbSerial.idVendor(), usbSerial.idProduct());
  LOG.printf(F("[DEV] Device %s, %s (%s)\n"), mfg, prod, serNum);

  btn.begin(SWITCH_PIN);
  btn.setTapHandler(&handleButtonPress);
  digitalWrite(LED_BUILTIN, HIGH);
  LOG.println(F("[BUTTON] Waiting for press..."));
  driverActive = true;
}

bool loadCSV() {
  if (!SD.begin(BUILTIN_SDCARD)) {
    LOG.println(F("[SD] SD card not present or corrupt!"));
    return false;
  }

  if (!SD.exists(FILE_PATH)) {
    LOG.println(F("[SD] test.csv does not exist!"));
    return false;
  }

  File file = SD.open(FILE_PATH);
  uint8_t index = 0;
  while (true) {
    String cmdStr = file.readStringUntil(',').trim();

    if (cmdStr.length() == 0) {
      break;
    }

    char cmd = cmdStr.c_str()[0];
    double val = file.parseFloat();

    if (cmd != 'W' && cmd != 'F' && cmd != 'P') {
      LOG.printf(F("[CSV] Invalid command %c in CSV!\n"), cmd);
      return false;
    } else if (isnan(val) || val < 0 || val > 1e3) {
      LOG.printf(F("[CSV] Invalid number %.2f in CSV!\n"), val);
      return false;
    }

    commands[index++] = {cmd, val};
    LOG.printf(F("[CSV] Parsed line %d\nCommand: %c\nValue: %.2f\n"), index,
               cmd, val);
  }

  commandCount = index + 1;
  LOG.printf(F("[CSV] Loaded %d commands\n"), commandCount);

  file.close();
  return true;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  LOG.begin(DEVICE_BAUD);
#ifdef DEBUG_LOGGING
#ifdef WAIT_FOR_DEBUGGER
  while (!Serial) {
    delay(100);
  }
#endif
#endif
  LOG.println(F("[HOST] Connected to debugger!"));

  if (!loadCSV()) {
    return;
  }

  usbHost.begin();
  initialized = true;
}

void loop() {
  if (!initialized) {
    delay(1000);
    return;
  }

  usbHost.Task();
  btn.loop();

  if (*&usbSerial == driverActive) {
    return;
  }

  if (driverActive) {
    handleDeviceDisconnect();
  } else {
    handleDeviceConnect();
  }
}
