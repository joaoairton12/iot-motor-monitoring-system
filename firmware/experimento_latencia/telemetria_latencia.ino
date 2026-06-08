// Payload CSV:
// seq,timestamp_esp_ms,tensao,corrente,potencia,rssi
// ============================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <PZEM004Tv30.h>
#include "secrets.h"
#include <time.h>
#include <sys/time.h>

// ========================= MQTT =========================
#define MQTT_HOST           "39dee4b9ba64439997bc849ccebd56fb.s1.eu.hivemq.cloud"
#define MQTT_PORT           8883
#define MQTT_TOPIC          "tcc/motor/latencia"
#define MQTT_STATUS_TOPIC   "tcc/motor/status_latencia"
#define MQTT_CLIENT_ID      "ESP32-TCC-LATENCIA"

// ========================= PZEM =========================
#define PZEM_RX_PIN         16
#define PZEM_TX_PIN         17
#define SERIAL2_BAUD        9600
#define ADDR_PZEM           0x01

// ========================= CONFIGURAÇÕES =========================
#define INTERVALO_LEITURA_MS    3000
#define SERIAL_BAUD             115200
#define DEBUG                   true

// ========================= NTP =========================
#define NTP_SERVER          "pool.ntp.org"
#define GMT_OFFSET_SEC      -10800     // Brasil UTC-3
#define DAYLIGHT_OFFSET_SEC 0

// ========================= OBJETOS =========================
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN, ADDR_PZEM);

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

unsigned long ultimoEnvio = 0;
uint32_t seq = 0;

// ========================= ESTRUTURA =========================
struct Medicao {
    float tensao;
    float corrente;
    float potencia;
    bool valido;
};

// ========================= FUNÇÕES AUXILIARES =========================
bool valorValido(float valor) {
    return !isnan(valor) && !isinf(valor);
}

Medicao lerSensor() {
    Medicao m;

    m.tensao = pzem.voltage();
    m.corrente = pzem.current();
    m.potencia = pzem.power();

    m.valido = valorValido(m.tensao) &&
               valorValido(m.corrente) &&
               valorValido(m.potencia);

    return m;
}

String formatarCampo(float valor) {
    if (!valorValido(valor)) return "-1";
    return String(valor, 2);
}

uint64_t timestampUnixMs() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return ((uint64_t)tv.tv_sec * 1000ULL) + (tv.tv_usec / 1000ULL);
}

bool horarioSincronizado() {
    time_t now = time(nullptr);

    // Se o timestamp for maior que 2024-01-01, considera sincronizado
    return now > 1704067200;
}

void sincronizarNTP() {
    Serial.println(F("[NTP] Sincronizando horário..."));

    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    int tentativas = 0;
    while (!horarioSincronizado() && tentativas < 30) {
        delay(500);
        Serial.print(".");
        tentativas++;
    }

    Serial.println();

    if (horarioSincronizado()) {
        Serial.println(F("[NTP] Horário sincronizado com sucesso."));
    } else {
        Serial.println(F("[NTP] Falha ao sincronizar horário."));
    }
}

String montarCSV(uint32_t seq, uint64_t timestampEspMs, const Medicao &m, long rssi) {
    String csv =
        String(seq) + "," +
        String((unsigned long long)timestampEspMs) + "," +
        formatarCampo(m.tensao) + "," +
        formatarCampo(m.corrente) + "," +
        formatarCampo(m.potencia) + "," +
        String(rssi);

    return csv;
}

// ========================= CONEXÕES =========================
void garantirWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.println(F("[Wi-Fi] Conectando..."));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 30) {
        delay(500);
        Serial.print(".");
        tentativas++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print(F("[Wi-Fi] Conectado. IP: "));
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(F("[Wi-Fi] Falha na conexão."));
    }
}

void garantirMQTT() {
    if (mqttClient.connected()) return;

    String clientId = String(MQTT_CLIENT_ID) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);

    Serial.println(F("[MQTT] Conectando..."));

    if (mqttClient.connect(
            clientId.c_str(),
            MQTT_USER,
            MQTT_PASS,
            MQTT_STATUS_TOPIC,
            1,
            true,
            "offline"
        )) {
        mqttClient.publish(MQTT_STATUS_TOPIC, "online", true);
        Serial.println(F("[MQTT] Conectado com LWT."));
    } else {
        Serial.print(F("[MQTT] Falha. Estado: "));
        Serial.println(mqttClient.state());
        delay(3000);
    }
}

// ========================= SETUP =========================
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    Serial.println(F("\n=== TESTE DE LATÊNCIA INTERNA ==="));

    Serial2.begin(SERIAL2_BAUD, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);

    espClient.setInsecure();
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setBufferSize(512);

    garantirWiFi();
    sincronizarNTP();
    garantirMQTT();
}

// ========================= LOOP =========================
void loop() {
    garantirWiFi();

    if (WiFi.status() == WL_CONNECTED && !horarioSincronizado()) {
        sincronizarNTP();
    }

    garantirMQTT();
    mqttClient.loop();

    if (millis() - ultimoEnvio >= INTERVALO_LEITURA_MS) {
        ultimoEnvio = millis();

        if (!horarioSincronizado()) {
            Serial.println(F("[ERRO] Horário não sincronizado. Mensagem não enviada."));
            return;
        }

        Medicao medicao = lerSensor();
        long rssi = WiFi.RSSI();

        uint64_t timestampEspMs = timestampUnixMs();
        seq++;

        String payload = montarCSV(seq, timestampEspMs, medicao, rssi);

        bool publicado = mqttClient.publish(MQTT_TOPIC, payload.c_str());

        if (DEBUG) {
            Serial.println(F("--- Mensagem de Latência ---"));
            Serial.print(F("SEQ: "));
            Serial.println(seq);

            Serial.print(F("Timestamp ESP ms: "));
            Serial.println((unsigned long long)timestampEspMs);

            Serial.print(F("Payload: "));
            Serial.println(payload);

            Serial.print(F("Publicado MQTT: "));
            Serial.println(publicado ? "SIM" : "NAO");

            Serial.print(F("RSSI: "));
            Serial.print(rssi);
            Serial.println(F(" dBm"));

            Serial.println(F("-----------------------------\n"));
        }
    }
}