// =============================================
// STATUS LWT (Last Will and Testament)
// =============================================

// Converte o payload recebido para texto, remove espaços extras e padroniza o conteúdo para letras minúsculas.
const status = msg.payload.toString().trim().toLowerCase();

// ================= CAMPOS =================
// Campo numérico utilizado para representar o estado operacional do ESP32.
// 1 = dispositivo online
// 0 = dispositivo offline
let fields = {
    esp_online: (status === "online") ? 1 : 0
};

// ================= TAGS =================
// Tag utilizada para identificação do dispositivo responsável pela publicação da informação de status.
let tags = {
    dispositivo: "ESP32_TCC_UNIAVAN"
};

// ================= SAÍDA =================
// Estrutura a mensagem no formato esperado pelo nó InfluxDB.
msg.payload = [fields, tags];

return msg;