# Firmware

Esta pasta contém os firmwares desenvolvidos para o projeto.

## Estrutura

### telemetria_motor_v1.0

Firmware principal do sistema.

Responsável pela:

* Aquisição das grandezas elétricas através dos módulos PZEM-004T v4.0;
* Leitura das tensões, correntes e potências das fases R, S e T;
* Coleta da intensidade do sinal Wi-Fi (RSSI);
* Publicação dos dados via MQTT utilizando o broker HiveMQ Cloud;
* Publicação do estado operacional do dispositivo através do mecanismo Last Will and Testament (LWT).

### telemetria_latencia

Firmware utilizado nos experimentos de avaliação da latência MQTT.

Este firmware envia mensagens contendo identificador sequencial e timestamp, permitindo calcular o tempo decorrido entre a publicação realizada pelo ESP32 e o recebimento da mensagem pelo Node-RED.

## Configuração

Assim como no firmware principal, as credenciais de acesso à rede Wi-Fi e ao broker MQTT devem ser armazenadas no arquivo `secrets.h`.