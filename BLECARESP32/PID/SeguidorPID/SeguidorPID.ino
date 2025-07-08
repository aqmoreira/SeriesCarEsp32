#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <CarEsp32.h> // Inclua a biblioteca do seu carro CarEsp32.h

// --- Configurações BLE ---
#define DEVICE_NAME "Carro_PID_1" // Nome do dispositivo Bluetooth
bool linhaPreta = false;    // Estou usando aqui linha branca, use true se tiver linha preta


// UUIDs para o serviço e as características
// IMPORTANTE: Mantenha esses UUIDs EXATAMENTE iguais (e em MINÚSCULAS) na sua página HTML.
#define SERVICE_UUID              "4fafcd20-178d-460b-ae94-d962b28b36ec" // Serviço principal
#define KP_CHARACTERISTIC_UUID    "c0d1e2f3-a4b5-6789-0123-456789abcdef" // Para a constante Kp do PID
#define KI_CHARACTERISTIC_UUID    "d0e1f2a3-b4c5-6789-0123-456789abcdef" // Para a constante Ki do PID
#define KD_CHARACTERISTIC_UUID    "e0f1a2b3-c4d5-6789-0123-456789abcdef" // Para a constante Kd do PID
#define SPEED_CHARACTERISTIC_UUID "f0c1d2e3-f4a5-6789-0123-456789abcdef" // Para controle de velocidade

// Objetos para as características BLE
BLECharacteristic *pKpCharacteristic;    
BLECharacteristic *pKiCharacteristic;    
BLECharacteristic *pKdCharacteristic;    

BLECharacteristic *pSpeedCharacteristic; // NOVO: Objeto para a característica de velocidade

// Flag para saber se um cliente está conectado via BLE
bool deviceConnected = false;

// --- Variáveis do Carro ---
int velocidadeBase = 150; 
int MOTOR_PWM_MIN = 160;
float calibragemMotorA = 1.00; // Motor Esquerdo (A) - Seu fator calibrado
float calibragemMotorB = 1.00;   // Motor Direito (B) - Seu fator calibrado

CarEsp32 carEsp32; 

// --- Variáveis PID ---
float Kp = 10.0;  // Constante Proporcional (valor inicial para ajuste)
float Ki = 0.0;   // Constante Integral (geralmente comeca em 0)
float Kd = 2.0;   // Constante Derivativa (valor inicial para ajuste)

float error = 0; 
float previousError = 0; 
float integral = 0; 
float derivative = 0; 

unsigned long lastTime = 0; 

// --- Callbacks BLE ---

// Callback para eventos de conexão/desconexão do servidor BLE
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Cliente BLE conectado!");
        // Ao conectar, envia os valores atuais para a página web para sincronização
        pKpCharacteristic->setValue(String(Kp, 2).c_str()); 
        pKpCharacteristic->notify();
        pKiCharacteristic->setValue(String(Ki, 2).c_str());
        pKiCharacteristic->notify();
        pKdCharacteristic->setValue(String(Kd, 2).c_str());
        pKdCharacteristic->notify();
        pSpeedCharacteristic->setValue(String(velocidadeBase,2).c_str());
        pSpeedCharacteristic->notify();
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Cliente BLE desconectado.");
        
        // Removi 'carEsp32.pararMotor();' daqui, para ser gerenciado apenas no loop principal.
        BLEDevice::startAdvertising(); 
    }
};

// Callback para quando a característica Kp é escrita
class MyKpCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value_str = pCharacteristic->getValue().c_str();
        float new_kp = atof(value_str.c_str()); 

        if (new_kp != Kp) {
            Kp = new_kp;
            Serial.print("Kp atualizado via BLE: ");
            Serial.println(Kp, 2); 
            pKpCharacteristic->setValue(String(Kp, 2).c_str());
            pKpCharacteristic->notify();
        }
    }
};

// Callback para quando a característica Ki é escrita
class MyKiCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value_str = pCharacteristic->getValue().c_str();
        float new_ki = atof(value_str.c_str()); 

        if (new_ki != Ki) {
            Ki = new_ki;
            Serial.print("Ki atualizado via BLE: ");
            Serial.println(Ki, 2);
            pKiCharacteristic->setValue(String(Ki, 2).c_str());
            pKiCharacteristic->notify();
        }
    }
};

// Callback para quando a característica Kd é escrita
class MyKdCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value_str = pCharacteristic->getValue().c_str();
        float new_kd = atof(value_str.c_str()); 

        if (new_kd != Kd) {
            Kd = new_kd;
            Serial.print("Kd atualizado via BLE: ");
            Serial.println(Kd, 2);
            pKdCharacteristic->setValue(String(Kd, 2).c_str());
            pKdCharacteristic->notify();
        }
    }
};

// NOVO: Callback para quando a característica de Velocidade é escrita
class MySpeedCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value_str = pCharacteristic->getValue().c_str();
        int new_speed = atoi(value_str.c_str()); 

        // Limita a velocidade entre 0 e 255
        new_speed = constrain(new_speed, 0, 255); 

        if (new_speed != velocidadeBase) {
            velocidadeBase = new_speed;
            Serial.print("Velocidade Base atualizada via BLE: ");
            Serial.println(velocidadeBase);

            // Notifica a página web sobre a nova velocidade
            pSpeedCharacteristic->setValue(String(velocidadeBase).c_str());
            pSpeedCharacteristic->notify();
        }
    }
};


// --- Função Setup ---
void setup() {
    Serial.begin(115200); 

    Serial.println("Inicializando Carro PID Constants BLE...");

    carEsp32.ajustarVelocidade(velocidadeBase); 
    carEsp32.calibrarMotorA(calibragemMotorA);
    carEsp32.calibrarMotorB(calibragemMotorB);
    carEsp32.setLinhaCorPreta(linhaPreta); // Usa a variavel global linhaPreta


    BLEDevice::init(DEVICE_NAME);

    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    // --- Configuração das Características BLE ---

    // Característica para Kp (Leitura, Escrita e Notificação)
    pKpCharacteristic = pService->createCharacteristic(
                                KP_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ | 
                                BLECharacteristic::PROPERTY_WRITE | 
                                BLECharacteristic::PROPERTY_NOTIFY
                            );
    pKpCharacteristic->setCallbacks(new MyKpCallbacks());
    pKpCharacteristic->addDescriptor(new BLE2902());
    pKpCharacteristic->setValue(String(Kp, 2).c_str()); 

    // Característica para Ki (Leitura, Escrita e Notificação)
    pKiCharacteristic = pService->createCharacteristic(
                                KI_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ | 
                                BLECharacteristic::PROPERTY_WRITE | 
                                BLECharacteristic::PROPERTY_NOTIFY
                            );
    pKiCharacteristic->setCallbacks(new MyKiCallbacks());
    pKiCharacteristic->addDescriptor(new BLE2902());
    pKiCharacteristic->setValue(String(Ki, 2).c_str());

    // Característica para Kd (Leitura, Escrita e Notificação)
    pKdCharacteristic = pService->createCharacteristic(
                                KD_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ | 
                                BLECharacteristic::PROPERTY_WRITE | 
                                BLECharacteristic::PROPERTY_NOTIFY
                            );
    pKdCharacteristic->setCallbacks(new MyKdCallbacks());
    pKdCharacteristic->addDescriptor(new BLE2902());
    pKdCharacteristic->setValue(String(Kd, 2).c_str());

    // NOVO: Característica para Velocidade (Leitura, Escrita e Notificação)
    pSpeedCharacteristic = pService->createCharacteristic(
                                SPEED_CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_READ |
                                BLECharacteristic::PROPERTY_WRITE |
                                BLECharacteristic::PROPERTY_NOTIFY 
                            );
    pSpeedCharacteristic->setCallbacks(new MySpeedCallbacks());
    pSpeedCharacteristic->addDescriptor(new BLE2902());
    pSpeedCharacteristic->setValue(String(velocidadeBase,2).c_str()); // Valor inicial da velocidade base

    


    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID); 
    pAdvertising->setScanResponse(true); 
    pAdvertising->setMinPreferred(0x06); 
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("ESP32 BLE para Ajuste PID iniciado. Aguardando conexoes...");

    lastTime = millis(); 
}

// --- Função Loop ---
void loop() {
    unsigned long currentTime = millis();
    float deltaT = (currentTime - lastTime) / 1000.0; 
    lastTime = currentTime;

    // 1. Leitura dos Sensores de Linha
    bool s1 = carEsp32.getSensorLinha(SL1); 
    bool s2 = carEsp32.getSensorLinha(SL2); 
    bool s3 = carEsp32.getSensorLinha(SL3); 
    bool s4 = carEsp32.getSensorLinha(SL4); 
    bool s5 = carEsp32.getSensorLinha(SL5); 

    // 2. Cálculo do Erro para o PID
    // Seus pesos ajustados: -5, -1, 0, 1, 5
    if(( s1== 0 )&&(s2== 0 )&&(s3== 0 )&&(s4== 0 )&&(s5== 1 ))    { error = 4;}
    else if((s1== 0 )&&(s2== 0 )&&(s3== 0 )&&(s4== 1 )&&(s5== 1 ))   { error = 3;}
    else if((s1== 0 )&&(s2== 0 )&&(s3== 0 )&&(s4== 1 )&&(s5== 0 ))   { error = 2;}
    else if((s1== 0 )&&(s2== 0 )&&(s3== 1 )&&(s4== 1 )&&(s5== 0 ))   { error = 1;}
    else if((s1== 0 )&&(s2== 0 )&&(s3== 1 )&&(s4== 0 )&&(s5== 0 ))   { error = 0;} // CENTRAL
    else if((s1== 0 )&&(s2== 1 )&&(s3== 1 )&&(s4== 0 )&&(s5== 0 ))   { error =- 1;}
    else if((s1== 0 )&&(s2== 1 )&&(s3== 0 )&&(s4== 0 )&&(s5== 0 ))   { error = -2;}
    else if((s1== 1 )&&(s2== 1 )&&(s3== 0 )&&(s4== 0 )&&(s5== 0 ))   { error = -3;}
    else if((s1== 1 )&&(s2== 0 )&&(s3== 0 )&&(s4== 0 )&&(s5== 0 ))   { error = -4;}
    // Condicoes especiais:
    else if((s1== 1 )&&(s2== 1 )&&(s3== 1 )&&(s4== 1 )&&(s5== 1 ))   { carEsp32.pararMotor(); error = 0;} // Todos na linha (encruzilhada, parar)
    else if((s1== 0 )&&(s2== 0 )&&(s3== 0 )&&(s4== 0 )&&(s5== 0 ))   { carEsp32.pararMotor(); error = 0;} // Nenhum na linha (perda total)
    else { // Caso para zona morta entre S3 e S2/S4, ou outras combinacoes nao listadas
        if (previousError > 0) { 
            error = 2; // Mantem um error para virar para a esquerda
        } else if (previousError < 0) { 
            error = -2; // Mantem um error para virar para a direita
        } else { 
            error = 0; 
        }
    }

    // 3. Cálculo PID
    float proportional = Kp * error; // AJUSTADO: Kp multiplicado aqui
    integral += error * deltaT;
    // Anti-windup: Limita o termo integral.
    if (integral > 100) integral = 100; 
    if (integral < -100) integral = -100;
    derivative = (error - previousError) / deltaT;
    if (deltaT == 0) derivative = 0; // Evita divisão por zero

    float pidOutput = proportional + (Ki * integral) + (Kd * derivative);
    previousError = error; 

    // 4. Aplicação do PID na Velocidade dos Motores (AGORA COM REVERSÃO)
    // Calcula as velocidades brutas que podem ser negativas ou maiores que 255
    int motorEsquerdoSpeedRaw = velocidadeBase + pidOutput;  
    int motorDireitoSpeedRaw = velocidadeBase - pidOutput; 

    // Define a direção: se o valor bruto for negativo, inverte a direção
    // (true para frente na minha lógica, que será invertido para sua lib)
    bool sentidoEsquerdoParaFrente = (motorEsquerdoSpeedRaw >= 0);  
    bool sentidoDireitoParaFrente = (motorDireitoSpeedRaw >= 0);   

    // Pega o valor absoluto da velocidade para aplicar o PWM
    int finalVeloEsquerdo = abs(motorEsquerdoSpeedRaw);
    int finalVeloDireito = abs(motorDireitoSpeedRaw);

    // Garante que o PWM final esteja dentro do range 0-255
    finalVeloEsquerdo = constrain(finalVeloEsquerdo, 0, 255);
    finalVeloDireito = constrain(finalVeloDireito, 0, 255);
    
    // Aplica o limiar mínimo de PWM para garantir que o motor gire quando não está parado
    // (Se o finalVeloX for > 0 e < MOTOR_PWM_MIN, eleva para MOTOR_PWM_MIN)
    if (finalVeloEsquerdo > 0 && finalVeloEsquerdo < MOTOR_PWM_MIN) {
        finalVeloEsquerdo = MOTOR_PWM_MIN;
    }
    if (finalVeloDireito > 0 && finalVeloDireito < MOTOR_PWM_MIN) {
        finalVeloDireito = MOTOR_PWM_MIN;
    }

    // 5. Acionar Motores
    // Chama acionarMotorA/B com a direcao e o PWM final (que é sempre positivo)
    // Passamos o INVERSO de sentidoXParaFrente, pois (true=frente) na minha logica,
    // mas (false=frente) na sua biblioteca carEsp32 no nosso carrinho.
    carEsp32.acionarMotorA(!sentidoDireitoParaFrente, finalVeloDireito);
    carEsp32.acionarMotorB(!sentidoEsquerdoParaFrente, finalVeloEsquerdo); 
   
       
    
    // Debugging PID e Velocidades (saída no Serial Monitor)
   
    Serial.print("Error: "); Serial.print(error, 2);
    Serial.print("\tP: "); Serial.print(proportional, 2);
    Serial.print("\tI: "); Serial.print(integral, 2);
    Serial.print("\tD: "); Serial.print(derivative, 2);
    Serial.print("\tOutput: "); Serial.print(pidOutput, 2);
    
    Serial.print("\tMotorA: "); Serial.print(finalVeloDireito); // Motor Direito (Motor A na sua lib)
    Serial.print("\tSentidoA: "); Serial.print(sentidoDireitoParaFrente?"Frente":"Reverso"); 
    Serial.print("\tMotorB: "); Serial.print(finalVeloEsquerdo); // Motor Esquerdo (Motor B na sua lib)
    Serial.print("\tSentidoB: "); Serial.println(sentidoEsquerdoParaFrente?"Frente":"Reverso"); 

    

    // Lógica de Velocidade Base via Botões SW (se houver botões físicos)
    // Seus valores ajustados para os botões SW.
    int old_velocidadeBase = velocidadeBase; // Usado para detectar mudanca nos botoes

    // É importante garantir que os pinos SW1 a SW4 estejam configurados como INPUT_PULLUP no setup
    if(!digitalRead(SW1)){
        velocidadeBase = 170; 
        Serial.println("Velocidade Base ajustada via SW1: 170");
    } else if(!digitalRead(SW2)){ 
        velocidadeBase = 175; 
        Serial.println("Velocidade Base ajustada via SW2: 175");
    } else if(!digitalRead(SW3)){
        velocidadeBase = 180; 
        Serial.println("Velocidade Base ajustada via SW3: 180");
    } else if(!digitalRead(SW4)){
        velocidadeBase = 190; 
        Serial.println("Velocidade Base ajustada via SW4: 190");
    }

    // NOVO: Notificar a pagina BLE se a velocidadeBase mudou via botoes SW
    if (velocidadeBase != old_velocidadeBase && deviceConnected) {
        pSpeedCharacteristic->setValue(String(velocidadeBase).c_str());
        pSpeedCharacteristic->notify();
    }
    
    delay(20); 
}