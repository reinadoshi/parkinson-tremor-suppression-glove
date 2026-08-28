#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ESP32Servo.h>

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
Servo myServo;

// Tremor Detection Variables
float tremorThreshold = 2.5;
int tremorCounter = 0;
int restCounter = 0;
bool motorRotated = false;

// BLE Connection Callback
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        BLEDevice::startAdvertising();
    }
};

void setupBLE() {
    BLEDevice::init("TremorLink_2026");

    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );

    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();
    pServer->getAdvertising()->start();
}

void handleHardwareLogic(float filteredMagnitude) {

    // Servo control
    if (filteredMagnitude > tremorThreshold) {
        tremorCounter++;
        restCounter = 0;
    } else {
        restCounter++;
    }

    // Activate servo if tremor persists
    if (tremorCounter >= 20 && !motorRotated) {
        myServo.write(90);
        motorRotated = true;
    }

    // Reset servo after stable period
    if (restCounter >= 30 && motorRotated) {
        myServo.write(0);
        motorRotated = false;
        tremorCounter = 0;
    }

    // Stream tremor data over BLE
    if (deviceConnected) {

        uint8_t bleVal =
            (uint8_t)constrain(filteredMagnitude * 20, 0, 255);

        pCharacteristic->setValue(&bleVal, 1);
        pCharacteristic->notify();
    }
}
