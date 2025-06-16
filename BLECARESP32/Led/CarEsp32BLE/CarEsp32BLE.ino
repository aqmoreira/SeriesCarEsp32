#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Definir o nome do dispositivo BLE
#define DEVICE_NAME "ESP32_LED_Control"

// UUIDs para o serviço e a característica
// Você pode gerar UUIDs aleatórios online (ex: uuidgenerator.net)
#define SERVICE_UUID "4FAFCD20-178D-460B-AE94-D962B28B36EC"
#define CHARACTERISTIC_UUID "BEB5483E-36E1-4688-B7F5-EA07361B26A8"

// Pino do LED
const int ledPin = 2;

// Objeto para a característica BLE
BLECharacteristic *pCharacteristic;

// Callback para quando algo é escrito na característica BLE
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        // CORREÇÃO 1: Converte o String retornado por getValue() para std::string
        std::string value = pCharacteristic->getValue().c_str(); 

        if (value.length() > 0) {
            Serial.print("Valor recebido: ");
            for (int i = 0; i < value.length(); i++)
                Serial.print(value[i]);
            Serial.println();

            if (value == "1") {
                digitalWrite(ledPin, HIGH); // Acende o LED
                Serial.println("LED ON");
            } else if (value == "0") {
                digitalWrite(ledPin, LOW);  // Apaga o LED
                Serial.println("LED OFF");
            }
        }
    }
};

void setup() {
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW); // LED começa desligado

    // Inicializa o dispositivo BLE
    BLEDevice::init(DEVICE_NAME);

    // Cria o servidor BLE
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new BLEServerCallbacks()); // Opcional, para eventos do servidor

    // Cria o serviço BLE
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Cria a característica BLE
    pCharacteristic = pService->createCharacteristic(
                                CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ |
                                BLECharacteristic::PROPERTY_WRITE
                            );
    // Define a função de callback para quando a característica é escrita
    pCharacteristic->setCallbacks(new MyCallbacks());

    // Adiciona uma descrição opcional para a característica (cliente pode ler isso)
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("0"); // Valor inicial da característica

    // Inicia o serviço
    pService->start();

    // Inicia a publicidade (advertising) para que outros dispositivos possam descobrir o ESP32
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    // CORREÇÃO 2: Remove o operador '-->' inválido
    pAdvertising->setMinPreferred(0x06);  // Exemplo de otimização para conexão rápida
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("ESP32 BLE iniciado. Aguardando conexões...");
}

void loop() {
    // O loop pode ser usado para outras tarefas.
    // A comunicação BLE é baseada em eventos via callbacks.
    delay(100);
}