#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <CarEsp32.h> // Inclui a biblioteca do seu carro

// Definir o nome do dispositivo BLE
#define DEVICE_NAME "Carro_BLE_Control" // Nome descritivo para o carro

// UUIDs para o serviço e as características
// Você pode gerar UUIDs aleatórios online (ex: uuidgenerator.net)

#define SERVICE_UUID "4FAFCD20-178D-460B-AE94-D962B28B36EC" 
#define LED_CHARACTERISTIC_UUID "BEB5483E-36E1-4688-B7F5-EA07361B26A8" 
#define SENSOR_CHARACTERISTIC_UUID "A0B1C2D3-E4F5-6789-0123-456789ABCDEF"
#define SPEED_CHARACTERISTIC_UUID "B0C1D2E3-F4A5-6789-0123-456789ABCDEF" // NOVO: UUID para velocidade

// Pino do LED (se ainda for usado separadamente do controle do carro)
const int ledPin = 2; // Mantido para compatibilidade com o exemplo anterior do LED

// Objeto para as características BLE
BLECharacteristic *pLedCharacteristic;
BLECharacteristic *pSensorCharacteristic;
BLECharacteristic *pSpeedCharacteristic; // NOVO: Característica para velocidade

// Flag para saber se um cliente está conectado
bool deviceConnected = false;

// Variáveis do seu código original do carro
int velocidade = 1; // Velocidade máxima 255
float calibragemMotorA = 1; // Fator de ajuste para calibrar motores para ter aproximadamente a mesma rotação
float calibragemMotorB = 1; // Fator de ajuste para calibrar motores para ter aproximadamente a mesma rotação

CarEsp32 carEsp32; // Variável do Tipo CarEsp32 que é a placa que controla o carro

// Callback para eventos do Servidor BLE (conexão/desconexão)
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Cliente BLE conectado!");
        // Ao conectar, envia a velocidade atual para a página web
        pSpeedCharacteristic->setValue(String(velocidade).c_str());
        pSpeedCharacteristic->notify();
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Cliente BLE desconectado.");
        BLEDevice::startAdvertising(); // Reinicia a publicidade para permitir novas conexões
    }
};

// Callback para quando algo é escrito na característica do LED
class MyLedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue().c_str();

        if (value.length() > 0) {
            Serial.print("Comando LED recebido: ");
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

// NOVO: Callback para quando algo é escrito na característica da Velocidade
class MySpeedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value_str = pCharacteristic->getValue().c_str();
        int new_speed = atoi(value_str.c_str()); // Converte a string para inteiro

        // Limita a velocidade entre 0 e 255
        if (new_speed < 0) new_speed = 0;
        if (new_speed > 255) new_speed = 255;

        if (new_speed != velocidade) { // Só atualiza se o valor mudou
            velocidade = new_speed;
            carEsp32.ajustarVelocidade(velocidade);
            Serial.print("Velocidade atualizada via BLE: ");
            Serial.println(velocidade);

            // Notifica a página web sobre a nova velocidade (importante para manter sync)
            pSpeedCharacteristic->setValue(String(velocidade).c_str());
            pSpeedCharacteristic->notify();
        }
    }
    // Opcional: onRead() se você quiser lidar com leituras diretas, embora notify já cubra a maioria dos casos
};

void setup() {
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW); // LED começa desligado

    Serial.println("Inicializando Carro e BLE...");

    // Inicialização do seu código de carro
    carEsp32.ajustarVelocidade(velocidade);
    carEsp32.calibrarMotorA(calibragemMotorA);
    carEsp32.calibrarMotorB(calibragemMotorB);

    // Inicializa o dispositivo BLE
    BLEDevice::init(DEVICE_NAME);

    // Cria o servidor BLE
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Cria o serviço BLE
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // --- Característica para o LED ---
    pLedCharacteristic = pService->createCharacteristic(
                                LED_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ |
                                BLECharacteristic::PROPERTY_WRITE
                            );
    pLedCharacteristic->setCallbacks(new MyLedCallbacks());
    pLedCharacteristic->addDescriptor(new BLE2902());
    pLedCharacteristic->setValue("0"); // Valor inicial do LED

    // --- Característica para os Sensores ---
    pSensorCharacteristic = pService->createCharacteristic(
                                SENSOR_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ |
                                BLECharacteristic::PROPERTY_NOTIFY 
                            );
    pSensorCharacteristic->addDescriptor(new BLE2902()); 
    pSensorCharacteristic->setValue("00000"); // Valor inicial dos sensores

    // --- NOVO: Característica para Velocidade ---
    pSpeedCharacteristic = pService->createCharacteristic(
                                SPEED_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ |    // Pode ser lida
                                BLECharacteristic::PROPERTY_WRITE |   // Pode ser escrita (alterada)
                                BLECharacteristic::PROPERTY_NOTIFY    // Notifica se o valor mudar
                            );
    pSpeedCharacteristic->setCallbacks(new MySpeedCallbacks()); // Callback para escrita
    pSpeedCharacteristic->addDescriptor(new BLE2902());
    pSpeedCharacteristic->setValue(String(velocidade).c_str()); // Valor inicial (velocidade padrão)


    // Inicia o serviço
    pService->start();

    // Inicia a publicidade (advertising)
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("ESP32 BLE para Carro iniciado. Aguardando conexões...");
}

void loop() {
    // Leitura e lógica dos sensores de linha do seu código original
    bool s1 = carEsp32.getSensorLinha(SL1);
    bool s2 = carEsp32.getSensorLinha(SL2);
    bool s3 = carEsp32.getSensorLinha(SL3);
    bool s4 = carEsp32.getSensorLinha(SL4);
    bool s5 = carEsp32.getSensorLinha(SL5);

    // Converte os estados booleanos para uma string "0" ou "1"
    String sensors_str = String(s1 ? "1" : "0") +
                         String(s2 ? "1" : "0") +
                         String(s3 ? "1" : "0") +
                         String(s4 ? "1" : "0") +
                         String(s5 ? "1" : "0");

    // Envia o estado dos sensores via BLE se houver um cliente conectado
    if (deviceConnected) {
        static String last_sensors_str = ""; 
        if (sensors_str != last_sensors_str) {
            pSensorCharacteristic->setValue(sensors_str.c_str());
            pSensorCharacteristic->notify(); 
            last_sensors_str = sensors_str;
        }
    }

    // Lógica do carro seguidor de linha (mantida do seu código)
    if(s2 == true && s4 == false){
        carEsp32.acionarMotorA(true, velocidade);
        carEsp32.acionarMotorB(false, velocidade);
    } else if(s2 == false && s4 == true){
        carEsp32.acionarMotorB(true, velocidade);
        carEsp32.acionarMotorA(false, velocidade);
    } else if(s2 == true && s3 == false && s4 == true){
        carEsp32.acionarMotor(false, false, velocidade);
    } else if(!s1 && !s2 && !s3 && !s4 && !s5){
        carEsp32.pararMotor();
        delay(100);
    }

    // Lógica de velocidade via botões SW (modificada para notificar via BLE)
    int old_velocidade = velocidade; // Salva a velocidade antes de verificar os botões

    if(!digitalRead(SW1)){
        velocidade = 255;
    } else if(!digitalRead(SW2)){ // Usar else if aqui é importante para priorizar
        velocidade = 170;
    } else if(!digitalRead(SW3)){
        velocidade = 200;
    } else if(!digitalRead(SW4)){
        velocidade = 230;
    }

    // Se a velocidade foi alterada pelos botões físicos
    if (velocidade != old_velocidade) {
        carEsp32.ajustarVelocidade(velocidade);
        Serial.print("Velocidade alterada via SW: ");
        Serial.println(velocidade);
        
        // Notifica a página web sobre a nova velocidade
        if (deviceConnected) {
            pSpeedCharacteristic->setValue(String(velocidade).c_str());
            pSpeedCharacteristic->notify();
        }
    }
    
    delay(50); // Um pequeno delay para não sobrecarregar o loop e o BLE
}