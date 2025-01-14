#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <ArduinoNvs.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Define the Status structure
struct Status {
  float version;
  uint32_t idNum;
  uint32_t gmt;
  uint16_t airPa;
  float airDeg;
  uint8_t airRH;
  uint8_t airMps;
  uint16_t waterPa;
  float levelCm;
  float waterDeg;
  uint8_t batPc;
};

// Define the Config structure
struct Config {
  uint32_t idNum;
  uint32_t gmt;
  int8_t timeZone;
  uint16_t levelMaxDeltaPa;
  uint16_t levelMinDeltaPa;
  bool autoLevelEn;
  bool manualLevelEn;
  uint16_t manualLevelOnTime;
  uint16_t LevelMaxOnTime;
  float baroCalib;
  bool wifiEn;
};

Status status;
Config config;
int recordCount = 10; // Number of records to save in setup
BLECharacteristic *pCharacteristic;
BLEDescriptor *myBLECCCD = new BLE2902();
bool shallNotify_Indicate = false;
bool isSetupDone = false; // Flag to ensure setup logic runs only once

void saveStatus(const Status& status, int recordNumber) {
  bool ok = NVS.setBlob(String(recordNumber), (uint8_t*)&status, sizeof(status));
  if (ok) {
    Serial.println("Status saved successfully.");
  } else {
    Serial.println("Failed to save status.");
  }
}

void readStatus(Status& status, int recordNumber) {
  size_t blobSize = NVS.getBlobSize(String(recordNumber));
  if (blobSize == sizeof(status)) {
    bool ok = NVS.getBlob(String(recordNumber), (uint8_t*)&status, sizeof(status));
    if (ok) {
      Serial.println("Status read successfully.");
    } else {
      Serial.println("Failed to read status.");
    }
  } else {
    Serial.println("No status found.");
  }
}

void saveConfig(const Config& config) {
  bool ok = NVS.setBlob("config", (uint8_t*)&config, sizeof(config));
  if (ok) {
    Serial.println("Config saved successfully.");
  } else {
    Serial.println("Failed to save config.");
  }
}

void readConfig(Config& config) {
  size_t blobSize = NVS.getBlobSize("config");
  if (blobSize == sizeof(config)) {
    bool ok = NVS.getBlob("config", (uint8_t*)&config, sizeof(config));
    if (ok) {
      Serial.println("Config read successfully.");
    } else {
      Serial.println("Failed to read config.");
    }
  } else {
    Serial.println("No config found.");
  }
}

void updateConfig(Config& config, uint32_t idNum, uint32_t gmt, int8_t timeZone, uint16_t levelMaxDeltaPa, uint16_t levelMinDeltaPa, bool autoLevelEn, bool manualLevelEn, uint16_t manualLevelOnTime, uint16_t LevelMaxOnTime, float baroCalib, bool wifiEn) {
  config.idNum = idNum;
  config.gmt = gmt;
  config.timeZone = timeZone;
  config.levelMaxDeltaPa = levelMaxDeltaPa;
  config.levelMinDeltaPa = levelMinDeltaPa;
  config.autoLevelEn = autoLevelEn;
  config.manualLevelEn = manualLevelEn;
  config.manualLevelOnTime = manualLevelOnTime;
  config.LevelMaxOnTime = LevelMaxOnTime;
  config.baroCalib = baroCalib;
  config.wifiEn = wifiEn;
  saveConfig(config);
}

class CharCallbacks : public BLECharacteristicCallbacks {
  public:
    void onWrite(BLECharacteristic *pChar) {
      Serial.println("BLE Config Value:");
      // shallNotify_Indicate = true;
    }
};

class CCCDCallbacks : public BLEDescriptorCallbacks {
  public:
    void onWrite(BLEDescriptor *pDesc) {
      Serial.printf("[%ld]: Notification registered\n", millis());
    }
};

void setupBLE() {
  Serial.println("Starting BLE work!");

  // BLE setup
  BLEDevice::init("Indicate issue Server");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristic->setCallbacks(new CharCallbacks());
  myBLECCCD->setCallbacks(new CCCDCallbacks());
  pCharacteristic->addDescriptor(myBLECCCD);

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void setup() {
  Serial.begin(115200);
  NVS.begin();
}

void loop() {
  if (!isSetupDone) {
    setupBLE();

    // NVS setup
    NVS.setInt("recordCount", recordCount);

    for (int i = 0; i < recordCount; i++) {
      status = {1.0 + i, 12345 + i, 1609459200 + i, 1015 + i, 25.0 + i, 60 + i, 5 + i, 500 + i, 150.0 + i, 20.0 + i, 80 + i};
      saveStatus(status, i);
    }

    config = {12345, 1609459200, 0, 50, 10, true, false, 30, 60, 1.0, true};
    saveConfig(config);

    isSetupDone = true; // Set the flag to indicate setup is done
  }

  if (((BLE2902*)myBLECCCD)->getIndications()) {
    shallNotify_Indicate = false;
    int recordCount = NVS.getInt("recordCount", 0);
    if (recordCount > 0) {
      // Read and indicate status records one by one
      for (int i = 0; i < recordCount; i++) {
        readStatus(status, i);
        pCharacteristic->setValue((uint8_t*)&status, sizeof(status));
        pCharacteristic->indicate();
        delay(1000); // Delay for readability
      }
      // Decrement record count
      NVS.setInt("recordCount", 0);
    }
  }
}
