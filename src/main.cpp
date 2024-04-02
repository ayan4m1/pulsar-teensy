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
// todo: file browser UI of some sort
#define FILE_PATH "/test.csv"
#define CSV_SEPARATOR ','
#define MAX_COMMAND_LENGTH 16
#define MAX_COMMANDS 16
#define DEVICE_BAUD 115200
#define MAX_PUFF_SECONDS 20
#define MAX_PAUSE_SECONDS 120
#define USB_PRODUCT_MATCH_STRING "DNA"

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
  LOG.println(F("[TRIGGER] Pressed!"));
  if (!driverActive || !serialActive) {
    LOG.println(F("[DEV] Serial connection inactive, abort!"));
    return;
  }
  LOG.printf(F("[HOST] Sending %d commands...\n"), commandCount);

  for (uint8_t i = 0; i < commandCount; i++) {
    const command_t cmd = commands[i];
    char cmdStr[MAX_COMMAND_LENGTH];

    uint16_t value = (uint16_t)cmd.val;

    switch (cmd.cmd) {
      case 'W': {
        LOG.printf(F("[ACT] Setting wattage to %dW\n"), value);
        sprintf(cmdStr, "P=%dW", value);
        usbSerial.println(cmdStr);
        break;
      }
      case 'F': {
        LOG.printf(F("[ACT] Firing for %ds\n"), value);
        sprintf(cmdStr, "F=%dS", value);
        usbSerial.println(cmdStr);
        delay(value * 1e3);
        break;
      }
      case 'P': {
        LOG.printf(F("[ACT] Pausing for %ds\n"), value);
        delay(value * 1e3);
        break;
      }
    }
  }

  LOG.println(F("[HOST] Done sending commands..."));
}

void handleDeviceDisconnect() {
  if (serialActive) {
    LOG.println(F("[HOST] Closing serial connection..."));
    usbSerial.end();
    serialActive = false;
  }

  if (driverActive) {
    LOG.println(F("[TRIGGER] Stopping monitor..."));
    btn.reset();
    driverActive = false;
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void handleDeviceConnect() {
  LOG.println(F("[HOST] Opening serial connection..."));
  usbSerial.begin(HOST_BAUD, USBHOST_SERIAL_8N1);
  while (!usbSerial) {
    delay(100);
  }
  serialActive = true;
  LOG.println(F("[HOST] Serial connection open!"));
  digitalWrite(LED_BUILTIN, HIGH);

  const char *mfg = (const char *)usbSerial.manufacturer();
  const char *prod = (const char *)usbSerial.product();
  const char *serNum = (const char *)usbSerial.serialNumber();

  if (!serNum || !*serNum) {
    serNum = '\0';
  }

  LOG.printf(F("[DEV] USB IDs %x:%x\n"), usbSerial.idVendor(),
             usbSerial.idProduct());
  LOG.printf(F("[DEV] Device %s, %s (%s)\n"), mfg, prod, serNum);

  if (strstr(prod, USB_PRODUCT_MATCH_STRING) == NULL) {
    LOG.println(F("[DEV] Not a DNA!"));
    usbSerial.end();
    serialActive = false;
    LOG.println(F("[HOST] Serial connection closed!"));
    return;
  }

  btn.begin(SWITCH_PIN);
  btn.setLongClickTime(SWITCH_HOLD_TIME_MS);
  btn.setLongClickHandler(&handleButtonPress);
  LOG.println(F("[TRIGGER] Starting monitor..."));
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
  while (index < MAX_COMMANDS) {
    String cmdStr = file.readStringUntil(',').trim();

    if (cmdStr.length() == 0) {
      break;
    }

    char cmd = cmdStr.c_str()[0];
    double val = file.parseFloat();

    if (cmd != 'W' && cmd != 'F' && cmd != 'P') {
      LOG.printf(F("[CSV] Invalid command %c in CSV!\n"), cmd);
      file.close();
      return false;
    } else if (cmd == 'W' && (val < 5 || val > 400)) {
      LOG.printf(F("[CSV] %.2f watts is outside the range 5-400W!\n"), val);
      file.close();
      return false;
    } else if (cmd == 'F' && (val < 1 || val > MAX_PUFF_SECONDS)) {
      LOG.printf(F("[CSV] Fire duration of %.2f is outside the range 1-%ds"),
                 val, MAX_PUFF_SECONDS);
      file.close();
      return false;
    } else if (cmd == 'P' && (val < 1 || val > MAX_PAUSE_SECONDS)) {
      LOG.printf(F("[CSV] Pause duration of %.2f is outside the range 1-%ds"),
                 val, MAX_PAUSE_SECONDS);
      file.close();
      return false;
    }

    commands[index++] = {cmd, val};
    LOG.printf(F("[CSV] Parsed line %d\nCommand: %c\nValue: %.2f\n"), index,
               cmd, val);
  }
  file.close();

  commandCount = index + 1;
  if (commandCount == 0) {
    LOG.println(F("[CSV] No commands loaded!"));
    return false;
  }
  LOG.printf(F("[CSV] Loaded %d commands\n"), commandCount);
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
    LOG.println(F("[CSV] Invalid CSV supplied, cannot continue!"));
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
