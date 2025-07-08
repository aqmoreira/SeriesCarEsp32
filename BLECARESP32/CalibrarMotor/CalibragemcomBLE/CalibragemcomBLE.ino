#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <CarEsp32.h> // Inclua a biblioteca do seu carro CarEsp32.h

// --- Configurações BLE ---
#define DEVICE_NAME "Carro_Motor_Calibrator" // Nome do dispositivo Bluetooth para calibração

// UUIDs para o serviço e as características
#define SERVICE_UUID                  "4fafcd20-178d-460b-ae94-d962b28b36ec" // Mantém o mesmo serviço principal
#define MOTOR_A_SPEED_CHARACTERISTIC_UUID "50A1B2C3-D4E5-6789-0123-456789ABCDEF" // NOVO: Velocidade Motor A
#define MOTOR_B_SPEED_CHARACTERISTIC_UUID "60F1E2D3-C4B5-6789-0123-456789ABCDEF" // NOVO: Velocidade Motor B
#define TEST_TIME_CHARACTERISTIC_UUID   "70A1B2C3-D4E5-6789-0123-456789ABCDEF" // NOVO: Tempo do teste
#define START_STOP_CHARACTERISTIC_UUID  "80F1E2D3-C4B5-6789-0123-456789ABCDEF" // NOVO: Iniciar/Parar teste

// Objetos para as características BLE
BLECharacteristic *pMotorASpeedCharacteristic;
BLECharacteristic *pMotorBSpeedCharacteristic;
BLECharacteristic *pTestTimeCharacteristic;
BLECharacteristic *pStartStopCharacteristic;

// Flag para saber se um cliente está conectado via BLE
bool deviceConnected = false;

// --- Variáveis de Calibração de Motor ---
int motorASpeed = 150; // Velocidade padrão para Motor A
int motorBSpeed = 150; // Velocidade padrão para Motor B
long testDurationMs = 2000; // Duração do teste em milissegundos (padrão: 2 segundos)
bool testRunning = false; // Flag para controlar o estado do teste
unsigned long testStartTime = 0; // Marca de tempo do início do teste

// --- Variáveis do Carro (permanecem, mas ajuste de fatores será manual para teste) ---
// Durante o teste, podemos considerar calibragemMotorA e B como 1.0 ou ajustá-las manualmente aqui
float calibragemMotorA = 1.0; 
float calibragemMotorB = 1.0; 
CarEsp32 carEsp32; 

// --- Callbacks BLE ---

// Callback para eventos de conexão/desconexão do servidor BLE
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Cliente BLE conectado!");
        // Envia os valores iniciais para a página web
        pMotorASpeedCharacteristic->setValue(String(motorASpeed).c_str());
        pMotorASpeedCharacteristic->notify();
        pMotorBSpeedCharacteristic->setValue(String(motorBSpeed).c_str());
        pMotorBSpeedCharacteristic->notify();
        pTestTimeCharacteristic->setValue(String(testDurationMs).c_str());
        pTestTimeCharacteristic->notify();
        pStartStopCharacteristic->setValue("0"); // Inicializa como parado
        pStartStopCharacteristic->notify();
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Cliente BLE desconectado.");
        // Garante que os motores parem ao desconectar
        carEsp32.pararMotor(); 
        testRunning = false;
        BLEDevice::startAdvertising(); 
    }
};

// NOVO: Callback para Velocidade do Motor A
class MyMotorASpeedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value_str = pCharacteristic->getValue().c_str();
        int new_speed = atoi(value_str.c_str());
        new_speed = constrain(new_speed, 0, 255); // Limita 0-255

        if (new_speed != motorASpeed) {
            motorASpeed = new_speed;
            Serial.print("Motor A Speed updated: "); Serial.println(motorASpeed);
            pMotorASpeedCharacteristic->setValue(String(motorASpeed).c_str());
            pMotorASpeedCharacteristic->notify();
        }
    }
};

// NOVO: Callback para Velocidade do Motor B
class MyMotorBSpeedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value_str = pCharacteristic->getValue().c_str();
        int new_speed = atoi(value_str.c_str());
        new_speed = constrain(new_speed, 0, 255); // Limita 0-255

        if (new_speed != motorBSpeed) {
            motorBSpeed = new_speed;
            Serial.print("Motor B Speed updated: "); Serial.println(motorBSpeed);
            pMotorBSpeedCharacteristic->setValue(String(motorBSpeed).c_str());
            pMotorBSpeedCharacteristic->notify();
        }
    }
};

// NOVO: Callback para Tempo de Teste
class MyTestTimeCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value_str = pCharacteristic->getValue().c_str();
        long new_time = atol(value_str.c_str()); // Converte para long

        // Limita o tempo para valores razoáveis (ex: 100ms a 10s)
        new_time = constrain(new_time, 100, 10000); 

        if (new_time != testDurationMs) {
            testDurationMs = new_time;
            Serial.print("Test Duration updated: "); Serial.println(testDurationMs);
            pTestTimeCharacteristic->setValue(String(testDurationMs).c_str());
            pTestTimeCharacteristic->notify();
        }
    }
};

// NOVO: Callback para Iniciar/Parar Teste
class MyStartStopCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value_str = pCharacteristic->getValue().c_str();
        bool command = (value_str == "1"); // "1" para iniciar, "0" para parar

        if (command && !testRunning) { // Iniciar o teste
            testRunning = true;
            testStartTime = millis();
            carEsp32.acionarMotorA(false, motorASpeed); // Aciona Motor A
            carEsp32.acionarMotorB(false, motorBSpeed); // Aciona Motor B
            Serial.print("Test Started! Motor A: "); Serial.print(motorASpeed);
            Serial.print(", Motor B: "); Serial.print(motorBSpeed);
            Serial.print(", Duration: "); Serial.print(testDurationMs); Serial.println("ms");
            pStartStopCharacteristic->setValue("1");
            pStartStopCharacteristic->notify();
        } else if (!command && testRunning) { // Parar o teste manualmente
            carEsp32.pararMotor();
            testRunning = false;
            Serial.println("Test stopped manually.");
            pStartStopCharacteristic->setValue("0");
            pStartStopCharacteristic->notify();
        }
    }
};

// --- Função Setup ---
void setup() {
    Serial.begin(115200); 

    Serial.println("Inicializando Carro Motor Calibrator BLE...");

    // Inicializa os fatores de calibração para 1.0 para testar a diferença bruta
    carEsp32.calibrarMotorA(1.0);
    carEsp32.calibrarMotorB(1.0); 
    // A função ajustarVelocidade não será usada aqui para definir velocidades globais,
    // as velocidades serão definidas diretamente via acionarMotorA/B.
    
    BLEDevice::init(DEVICE_NAME);
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // --- Configuração das Novas Características BLE ---

    // Característica para Velocidade Motor A
    pMotorASpeedCharacteristic = pService->createCharacteristic(
                                MOTOR_A_SPEED_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY    
                            );
    pMotorASpeedCharacteristic->setCallbacks(new MyMotorASpeedCallbacks()); 
    pMotorASpeedCharacteristic->addDescriptor(new BLE2902());
    pMotorASpeedCharacteristic->setValue(String(motorASpeed).c_str()); 

    // Característica para Velocidade Motor B
    pMotorBSpeedCharacteristic = pService->createCharacteristic(
                                MOTOR_B_SPEED_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY    
                            );
    pMotorBSpeedCharacteristic->setCallbacks(new MyMotorBSpeedCallbacks()); 
    pMotorBSpeedCharacteristic->addDescriptor(new BLE2902());
    pMotorBSpeedCharacteristic->setValue(String(motorBSpeed).c_str()); 

    // Característica para Tempo de Teste
    pTestTimeCharacteristic = pService->createCharacteristic(
                                TEST_TIME_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY    
                            );
    pTestTimeCharacteristic->setCallbacks(new MyTestTimeCallbacks()); 
    pTestTimeCharacteristic->addDescriptor(new BLE2902());
    pTestTimeCharacteristic->setValue(String(testDurationMs).c_str()); 

    // Característica para Iniciar/Parar Teste
    pStartStopCharacteristic = pService->createCharacteristic(
                                START_STOP_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY    
                            );
    pStartStopCharacteristic->setCallbacks(new MyStartStopCallbacks()); 
    pStartStopCharacteristic->addDescriptor(new BLE2902());
    pStartStopCharacteristic->setValue("0"); // Parado inicialmente

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID); 
    pAdvertising->setScanResponse(true); 
    pAdvertising->setMinPreferred(0x06); 
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("ESP32 BLE para Calibracao de Motor iniciado. Aguardando conexões...");

}

// --- Função Loop ---
void loop() {
    if (testRunning) {
        if (millis() - testStartTime >= testDurationMs) {
            carEsp32.pararMotor();
            testRunning = false;
            Serial.println("Test duration ended. Motors stopped.");
            // Notifica a página web que o teste terminou
            if (deviceConnected) {
                pStartStopCharacteristic->setValue("0");
                pStartStopCharacteristic->notify();
            }
        }
    }
    
    // Pequeno atraso para estabilidade, mesmo em modo de calibração
    delay(10); 
}