# Node-RED

Esta pasta contém os fluxos e funções utilizados para processamento dos dados recebidos via MQTT.

## Arquivos

### flows.json

Fluxo completo exportado do Node-RED contendo a integração entre MQTT, processamento dos dados e armazenamento no InfluxDB.

### tratamento_dos_dados.js

Função responsável pelo processamento da telemetria enviada pelo ESP32.

Principais atividades:

* Validação da estrutura do payload CSV;
* Conversão dos valores para formato numérico;
* Tratamento de leituras inválidas;
* Cálculo da potência ativa total;
* Estruturação dos dados em Fields e Tags;
* Preparação das informações para armazenamento no InfluxDB.

### lwt.js

Função responsável pelo tratamento das mensagens de status do dispositivo.
