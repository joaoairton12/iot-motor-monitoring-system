// ========================= NODE-RED FUNCTION =========================
// Função responsável pelo tratamento do payload CSV enviado pelo ESP32.
//
// Estrutura esperada do payload:
// - 9 medições elétricas: tensão, corrente e potência das fases R, S e T;
// - 3 flags de validade: indicam se as leituras de cada fase são válidas;
// - 1 valor de RSSI: intensidade do sinal Wi-Fi do ESP32.
//
// Total esperado: 13 campos separados por vírgula.
// =====================================================================

// Converte o payload recebido para string, remove espaços extras
// e separa os campos utilizando a vírgula como delimitador.
const raw = msg.payload.toString().trim().split(",");

// ===== Validação da estrutura do payload =====
// Verifica se a quantidade de campos recebidos corresponde ao formato esperado.
// Caso o payload esteja incompleto ou fora do padrão, a mensagem é descartada.
if (raw.length !== 13) {
    node.error(`[ERRO] Payload inválido. Recebidos ${raw.length}, esperado 13`);
    return null;
}

// ===== Conversão dos campos =====
// Converte todos os campos recebidos de texto para valores numéricos.
const d = raw.map(v => parseFloat(v));

// ===== Validação de campo individual =====
// Considera inválidos valores NaN ou valores iguais a -1.
// O valor -1 é utilizado pelo firmware como marcador de leitura inválida.
function campoInvalido(v) {
    return Number.isNaN(v) || v === -1;
}

// ===== Conversão segura para armazenamento =====
// Retorna null para campos inválidos, permitindo que o banco de dados
// represente corretamente a ausência de uma leitura válida.
function valor(v) {
    return campoInvalido(v) ? null : v;
}

// ======================== CAMPOS ========================
// Objeto contendo os campos numéricos que serão armazenados no InfluxDB.
// Esses campos representam as medições elétricas, flags de validade e RSSI.
let fields = {

    // ===== FASE R =====
    // Grandezas elétricas medidas na fase R.
    tensao_r: valor(d[0]),
    corrente_r: valor(d[1]),
    potencia_r: valor(d[2]),

    // ===== FASE S =====
    // Grandezas elétricas medidas na fase S.
    tensao_s: valor(d[3]),
    corrente_s: valor(d[4]),
    potencia_s: valor(d[5]),

    // ===== FASE T =====
    // Grandezas elétricas medidas na fase T.
    tensao_t: valor(d[6]),
    corrente_t: valor(d[7]),
    potencia_t: valor(d[8]),

    // ===== FLAGS DE VALIDADE =====
    // Indicadores enviados pelo ESP32 para informar se a leitura de cada fase é válida.
    valido_r: d[9],
    valido_s: d[10],
    valido_t: d[11],

    // ===== RSSI =====
    // Intensidade do sinal Wi-Fi do ESP32.
    // Caso o valor recebido seja inválido, é armazenado 0 como valor padrão.
    rssi: Number.isFinite(d[12]) ? Math.round(d[12]) : 0
};

// ================= POTÊNCIA TOTAL =================
// Calcula a potência total apenas quando as três potências de fase são válidas.
// Caso alguma fase esteja sem leitura válida, a potência total é definida como null.
if (
    fields.potencia_r !== null &&
    fields.potencia_s !== null &&
    fields.potencia_t !== null
) {
    fields.potencia_total =
        fields.potencia_r +
        fields.potencia_s +
        fields.potencia_t;
} else {
    fields.potencia_total = null;
}

// ================= TAGS =================
// Tags utilizadas pelo InfluxDB para identificar o contexto da medição.
// Diferentemente dos fields, as tags são usadas principalmente para filtragem e agrupamento.
let tags = {
    dispositivo: "ESP32_TCC_UNIAVAN",
    tipo_carga: "motor_eletrico"
};

// ================= SAÍDA =================
// Estrutura a mensagem no formato esperado pelo nó InfluxDB do Node-RED.
// O primeiro objeto contém os campos e o segundo contém as tags.
msg.payload = [fields, tags];

return msg;