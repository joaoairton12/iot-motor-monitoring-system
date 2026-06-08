// ============================================================
// SISTEMA IoT PARA MONITORAMENTO DE MOTORES ELÉTRICOS
// ============================================================

// Bibliotecas
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <PZEM004Tv30.h>
#include "secrets.h"

// ===================== CONFIGURAÇÕES MQTT =====================
// Tópico utilizado para publicação das medições elétricas.
#define MQTT_TOPIC          "tcc/motor/telemetria"

// Tópico utilizado para publicação do status operacional do ESP32.
#define MQTT_STATUS_TOPIC   "tcc/motor/status"

// Identificador do cliente MQTT. O MAC do ESP32 é concatenado posteriormente.
#define MQTT_CLIENT_ID      "ESP32-TCC"

// ===================== CONFIGURAÇÕES DO BARRAMENTO MODBUS =====================
// Pinos utilizados pela UART2 do ESP32 para comunicação com os módulos PZEM.
#define PZEM_RX_PIN         16
#define PZEM_TX_PIN         17

// Taxa de comunicação serial utilizada pelos módulos PZEM-004T.
#define SERIAL2_BAUD        9600

// Endereços Modbus atribuídos aos sensores de cada fase.
#define ADDR_FASE_R         0x01
#define ADDR_FASE_S         0x02
#define ADDR_FASE_T         0x03

// ===================== PARÂMETROS DE EXECUÇÃO =====================
// Intervalo entre ciclos de aquisição e publicação dos dados.
#define INTERVALO_LEITURA_MS    3000

// Tempo de espera entre leituras sequenciais dos módulos PZEM.
// Esse intervalo reduz a possibilidade de instabilidade no barramento serial compartilhado.
#define DELAY_MODBUS_MS         150

// Taxa de comunicação serial utilizada para depuração via monitor serial.
#define SERIAL_BAUD             115200

// Habilita ou desabilita a impressão das leituras no monitor serial.
#define DEBUG                   true

// ========================= ESTRUTURA DE DADOS =========================
// Estrutura utilizada para armazenar as medições de uma fase elétrica.
// O campo "valido" indica se todas as grandezas foram lidas corretamente.
struct MedicaoFase {
    float tensao;
    float corrente;
    float potencia_ativa;
    bool  valido;
};

// ========================= OBJETOS =========================
// Instâncias dos sensores PZEM-004T.
// Os três módulos compartilham a mesma interface serial, sendo diferenciados pelo endereço Modbus.
PZEM004Tv30 pzemR(Serial2, PZEM_RX_PIN, PZEM_TX_PIN, ADDR_FASE_R);
PZEM004Tv30 pzemS(Serial2, PZEM_RX_PIN, PZEM_TX_PIN, ADDR_FASE_S);
PZEM004Tv30 pzemT(Serial2, PZEM_RX_PIN, PZEM_TX_PIN, ADDR_FASE_T);

// Cliente seguro utilizado como base para a conexão MQTT via TLS.
WiFiClientSecure espClient;

// Cliente MQTT responsável pela conexão, publicação e manutenção da sessão com o broker.
PubSubClient mqttClient(espClient);

// Armazena o instante da última publicação, permitindo controlar o intervalo de envio.
unsigned long ultimoEnvio = 0;

// ========================= FUNÇÕES AUXILIARES =========================
// Verifica se um valor numérico é válido.
// Leituras NaN ou infinitas são consideradas inválidas.
bool valorValido(float valor) {
    return !isnan(valor) && !isinf(valor);
}

// Realiza a leitura das grandezas elétricas de uma fase.
// A função consulta tensão, corrente e potência ativa no respectivo módulo PZEM e define a leitura como válida somente se todos os valores forem consistentes.
MedicaoFase lerFase(PZEM004Tv30 &pzem) {
    MedicaoFase m;
    m.tensao = pzem.voltage();
    m.corrente = pzem.current();
    m.potencia_ativa = pzem.power();

    m.valido = valorValido(m.tensao) && 
               valorValido(m.corrente) && 
               valorValido(m.potencia_ativa);
    return m;
}

// Formata um campo numérico para composição do payload CSV.
// Caso o valor seja inválido, retorna "-1" como marcador de inconsistência.
String formatarCampo(float valor) {
    if (!valorValido(valor)) return "-1";
    return String(valor, 2);
}

// Monta o payload CSV enviado via MQTT.
// A sequência inclui tensão, corrente e potência das três fases, flags de validade das leituras e o RSSI da conexão Wi-Fi.
String montarCSV(const MedicaoFase &r, const MedicaoFase &s, const MedicaoFase &t, long rssi) {
    String csv = 
        formatarCampo(r.tensao) + "," + formatarCampo(r.corrente) + "," + formatarCampo(r.potencia_ativa) + "," +
        formatarCampo(s.tensao) + "," + formatarCampo(s.corrente) + "," + formatarCampo(s.potencia_ativa) + "," +
        formatarCampo(t.tensao) + "," + formatarCampo(t.corrente) + "," + formatarCampo(t.potencia_ativa) + "," +
        String(r.valido ? 1 : 0) + "," + String(s.valido ? 1 : 0) + "," + String(t.valido ? 1 : 0) + "," +
        String(rssi);
    return csv;
}

// ========================= FUNÇÕES DE CONEXÃO =========================
// Garante que o ESP32 permaneça conectado à rede Wi-Fi.
// Caso a conexão seja perdida, inicia novas tentativas de autenticação.
void garantirWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.println(F("[Wi-Fi] Reconectando..."));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
        delay(500);
        Serial.print(".");
        tentativas++;
    }
}

// Garante a conexão do cliente MQTT com o broker.
// Durante a conexão, registra o mecanismo LWT para publicar "offline" caso o ESP32 seja desconectado inesperadamente.
void garantirMQTT() {
    if (mqttClient.connected()) return;

    // Gera um identificador único para o cliente MQTT a partir do ID e do MAC do ESP32.
    String clientId = String(MQTT_CLIENT_ID) + String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS, MQTT_STATUS_TOPIC, 1, true, "offline")) {
        // Após a conexão bem-sucedida, o próprio ESP32 publica o status "online".
        mqttClient.publish(MQTT_STATUS_TOPIC, "online", true);
        Serial.println(F("[MQTT] Conectado com LWT"));
    } else {
        Serial.println(F("[MQTT] Falha na conexão"));
        delay(3000);
    }
}

// Imprime no monitor serial as medições de uma fase.
// Utilizada apenas para depuração local durante os testes.
void imprimirFase(const char* nome, const MedicaoFase &fase) {
    Serial.printf(" [%s] ", nome);
    if (!fase.valido) {
        Serial.println(F("INVÁLIDA"));
        return;
    }
    Serial.printf("%.1fV | %.2fA | %.1fW\n", 
                  fase.tensao, fase.corrente, fase.potencia_ativa);
}

// ========================= SETUP =========================
void setup() {
    // Inicializa a comunicação serial utilizada para depuração.
    Serial.begin(SERIAL_BAUD);

    // Inicializa a UART2 utilizada para comunicação com os módulos PZEM.
    Serial2.begin(SERIAL2_BAUD, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);

    // Permite conexão TLS sem validação estrita do certificado.
    // Essa configuração simplifica a conexão com o broker em ambiente de prototipagem.
    espClient.setInsecure();

    // Define o endereço e a porta do broker MQTT.
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);

    // Ajusta o tamanho do buffer MQTT para comportar o payload CSV gerado.
    mqttClient.setBufferSize(512);
    
    // Realiza as conexões iniciais de rede e MQTT.
    garantirWiFi();
    garantirMQTT();
}

// ========================= LOOP PRINCIPAL =========================
void loop() {
    // Verifica continuamente a conectividade Wi-Fi e MQTT.
    garantirWiFi();
    garantirMQTT();

    // Mantém ativa a sessão MQTT e processa eventos internos da biblioteca.
    mqttClient.loop();

    // Executa um novo ciclo de leitura e publicação conforme o intervalo configurado.
    if (millis() - ultimoEnvio >= INTERVALO_LEITURA_MS) {
        ultimoEnvio = millis();

        // === Leitura sequencial das fases ===
        // As leituras são realizadas uma por vez, com pequeno intervalo entre elas,
        // para reduzir instabilidades na comunicação com os módulos PZEM.
        MedicaoFase faseR = lerFase(pzemR);
        delay(DELAY_MODBUS_MS);
        
        MedicaoFase faseS = lerFase(pzemS);
        delay(DELAY_MODBUS_MS);
        
        MedicaoFase faseT = lerFase(pzemT);

        // Obtém a intensidade do sinal Wi-Fi em dBm.
        long rssi = WiFi.RSSI();

        // Monta o payload CSV e publica no tópico de telemetria.
        String dadosCSV = montarCSV(faseR, faseS, faseT, rssi);
        mqttClient.publish(MQTT_TOPIC, dadosCSV.c_str());

        // Exibe as leituras no monitor serial quando o modo de depuração está habilitado.
        if (DEBUG) {
            Serial.println(F("--- Nova Leitura Trifásica ---"));
            imprimirFase("FASE R", faseR);
            imprimirFase("FASE S", faseS);
            imprimirFase("FASE T", faseT);
            Serial.printf(" [RSSI] %ld dBm\n", rssi);
            Serial.println(F("----------------------------------------\n"));
        }
    }
}