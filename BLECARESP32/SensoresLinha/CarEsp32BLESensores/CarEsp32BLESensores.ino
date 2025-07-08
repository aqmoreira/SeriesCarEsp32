#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <CarEsp32.h> // Inclui a biblioteca do seu carro

// Definir o nome do dispositivo BLE e Tipo linha
#define DEVICE_NAME "Carro_BLE_Sensores" // Nome  descritivo para o carro
bool linhaPreta = false;    //cor da linha Preta = true , Branca = False

// UUIDs para o serviço e as características
#define SERVICE_UUID "4FAFCD20-178D-460B-AE94-D962B28B36EC" // Mantém o do LED
#define LED_CHARACTERISTIC_UUID "BEB5483E-36E1-4688-B7F5-EA07361B26A8" // Para o LED
#define SENSOR_CHARACTERISTIC_UUID "A0B1C2D3-E4F5-6789-0123-456789ABCDEF" // NOVO: Para os sensores

// Pino do LED (se ainda for usado separadamente do controle do carro)
const int ledPin = 2; // Mantido para compatibilidade com o exemplo anterior do LED

// Objeto para as características BLE
BLECharacteristic *pLedCharacteristic;
BLECharacteristic *pSensorCharacteristic; // Característica para sensores

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

void setup() {
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin, LOW); // LED começa desligado

    Serial.println("Inicializando Carro e BLE...");

    // Inicialização do seu código de carro
    carEsp32.ajustarVelocidade(velocidade);
    carEsp32.calibrarMotorA(calibragemMotorA);
    carEsp32.calibrarMotorB(calibragemMotorB);
    carEsp32.setLinhaCorPreta(linhaPreta);

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
                                BLECharacteristic::PROPERTY_NOTIFY // Permite que o ESP32 envie dados
                            );
    pSensorCharacteristic->addDescriptor(new BLE2902()); // Descriptor para notificações
    pSensorCharacteristic->setValue("00000"); // Valor inicial dos sensores (e.g., 5 zeros para 5 sensores)


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

    Serial.print("Sensores: ");
    Serial.println(sensors_str);

    // Envia o estado dos sensores via BLE se houver um cliente conectado
    if (deviceConnected) {
        // Verifica se o valor mudou para evitar spam de notificações
        static String last_sensors_str = ""; // static para manter o valor entre chamadas de loop
        if (sensors_str != last_sensors_str) {
            pSensorCharacteristic->setValue(sensors_str.c_str());
            pSensorCharacteristic->notify(); // Envia a notificação
            last_sensors_str = sensors_str;
        }
    }

    // Lógica do carro seguidor de linha Bang Bang (S2,S3 S4) - Sensores do meio apenas
    // Adaptações para usar as variáveis booleanas dos sensores diretamente
    if(s2 == false && s4 == true){
        carEsp32.acionarMotorA(true, velocidade);
        carEsp32.acionarMotorB(false, velocidade);
    } else if(s2 == true && s4 == false){
        carEsp32.acionarMotorB(true, velocidade);
        carEsp32.acionarMotorA(false, velocidade);
    } else if(s2 == false && s3 == true && s4 == false){
        carEsp32.acionarMotor(false, false, velocidade);
    } else if(s1 && s2 && s3 && s4 && s5){
        carEsp32.pararMotor();
        delay(100);
    }
    // Considerar adicionar uma lógica "else" final se nenhum dos "if/else if" acima for verdadeiro,
    // para garantir um comportamento padrão do carro, como seguir reto ou parar.
    // Ex: else { carEsp32.acionarMotor(false, false, velocidade); } // Seguir reto se nenhum desvio for detectado

    // Lógica de velocidade via botões SW (mantida do seu código)
    if(!digitalRead(SW1)){
        velocidade = 160;
        carEsp32.ajustarVelocidade(velocidade);
        Serial.println("Velocidade 160 (SW1)");
    }
    if(!digitalRead(SW2)){
        velocidade = 170;
        carEsp32.ajustarVelocidade(velocidade);
        Serial.println("Velocidade 170 (SW2)");
    }
    if(!digitalRead(SW3)){
        velocidade = 200 ;
        carEsp32.ajustarVelocidade(velocidade);
        Serial.println("Velocidade 200 (SW3)");
    }
    if(!digitalRead(SW4)){
        velocidade = 230;
        carEsp32.ajustarVelocidade(velocidade);
        Serial.println("Velocidade 230 (SW4)");
    }
    
    delay(50); // Um pequeno delay para não sobrecarregar o loop e o BLE
}