# Consultas InfluxDB

Consultas Flux utilizadas nos painéis do Grafana para visualização dos dados.


## Status do ESP32

Retorna o último estado conhecido do ESP32 utilizando as mensagens publicadas pelo mecanismo Last Will and Testament (LWT) do MQTT.

```flux
from(bucket: "monitoramento_motores")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "status_esp32")
  |> filter(fn: (r) => r["_field"] == "esp_online")
  |> filter(fn: (r) => r["dispositivo"] == "ESP32_TCC_UNIAVAN")
  |> last()
```

### Observação

* `1` → Online
* `0` → Offline

O intervalo fixo de 24 horas permite que o painel continue exibindo o último estado conhecido do dispositivo mesmo quando não houver dados recentes de telemetria.

---

## RSSI Atual (Gauge)

Consulta utilizada para acompanhar a intensidade do sinal Wi-Fi do ESP32 ao longo do tempo.

```flux
from(bucket: "monitoramento_motores")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "grandezas_eletricas")
  |> filter(fn: (r) => r["_field"] == "rssi")
  |> filter(fn: (r) => r["dispositivo"] == "ESP32_TCC_UNIAVAN")
  |> filter(fn: (r) => r["tipo_carga"] == "motor_eletrico")
  |> last()
```

### Observação

Valores de RSSI mais próximos de zero indicam melhor intensidade de sinal, enquanto valores mais negativos indicam pior qualidade de conexão.

---

## Potência Ativa Total (Gauge)

Consulta utilizada para exibir a potência ativa total calculada a partir da soma das potências das fases R, S e T.

```flux
from(bucket: "monitoramento_motores")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "grandezas_eletricas")
  |> filter(fn: (r) => r["_field"] == "potencia_total")
  |> filter(fn: (r) => r["dispositivo"] == "ESP32_TCC_UNIAVAN")
  |> filter(fn: (r) => r["tipo_carga"] == "motor_eletrico")
  |> last()
```

---

## Tensão ao Longo do Tempo

Consulta utilizada para visualizar a evolução temporal da tensão elétrica nas fases R, S e T.

```flux
from(bucket: "monitoramento_motores")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "grandezas_eletricas")
  |> filter(fn: (r) => contains(value: r["_field"], set: ["tensao_r", "tensao_s", "tensao_t"]))
  |> filter(fn: (r) => r["dispositivo"] == "ESP32_TCC_UNIAVAN")
  |> filter(fn: (r) => r["tipo_carga"] == "motor_eletrico")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

---

## Corrente ao Longo do Tempo

Consulta utilizada para visualizar a corrente elétrica nas fases R, S e T ao longo do tempo.

```flux
from(bucket: "monitoramento_motores")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "grandezas_eletricas")
  |> filter(fn: (r) => contains(value: r["_field"], set: ["corrente_r", "corrente_s", "corrente_t"]))
  |> filter(fn: (r) => r["dispositivo"] == "ESP32_TCC_UNIAVAN")
  |> filter(fn: (r) => r["tipo_carga"] == "motor_eletrico")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

---

## Potência ao Longo do Tempo

Consulta utilizada para visualizar a potência ativa medida nas fases R, S e T ao longo do tempo.

```flux
from(bucket: "monitoramento_motores")
  |> range(start: v.timeRangeStart, stop: v.timeRangeStop)
  |> filter(fn: (r) => r["_measurement"] == "grandezas_eletricas")
  |> filter(fn: (r) => contains(value: r["_field"], set: ["potencia_r", "potencia_s", "potencia_t"]))
  |> filter(fn: (r) => r["dispositivo"] == "ESP32_TCC_UNIAVAN")
  |> filter(fn: (r) => r["tipo_carga"] == "motor_eletrico")
  |> aggregateWindow(every: v.windowPeriod, fn: mean, createEmpty: false)
  |> yield(name: "mean")
```

---

## Tensão Atual (Painel Stat)

Retorna o último valor registrado para as tensões das fases R, S e T.

```flux
from(bucket: "monitoramento_motores")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "grandezas_eletricas")
  |> filter(fn: (r) => contains(value: r["_field"], set: ["tensao_r", "tensao_s", "tensao_t"]))
  |> filter(fn: (r) => r["dispositivo"] == "ESP32_TCC_UNIAVAN")
  |> filter(fn: (r) => r["tipo_carga"] == "motor_eletrico")
  |> last()
```

---

## Corrente Atual (Painel Stat)

Retorna o último valor registrado para as correntes das fases R, S e T.

```flux
from(bucket: "monitoramento_motores")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "grandezas_eletricas")
  |> filter(fn: (r) => contains(value: r["_field"], set: ["corrente_r", "corrente_s", "corrente_t"]))
  |> filter(fn: (r) => r["dispositivo"] == "ESP32_TCC_UNIAVAN")
  |> filter(fn: (r) => r["tipo_carga"] == "motor_eletrico")
  |> last()
```

---

## Potência Atual (Painel Stat)

Retorna o último valor registrado para as potências das fases R, S e T.

```flux
from(bucket: "monitoramento_motores")
  |> range(start: -24h)
  |> filter(fn: (r) => r["_measurement"] == "grandezas_eletricas")
  |> filter(fn: (r) => contains(value: r["_field"], set: ["potencia_r", "potencia_s", "potencia_t"]))
  |> filter(fn: (r) => r["dispositivo"] == "ESP32_TCC_UNIAVAN")
  |> filter(fn: (r) => r["tipo_carga"] == "motor_eletrico")
  |> last()
```

---

## Observações

Quando o ESP32 estiver desligado ou sem comunicação com o broker MQTT, os gráficos de telemetria poderão exibir "No data", uma vez que não existem novas medições sendo recebidas.

Os painéis Stat utilizam consultas com `last()` e janela fixa de 24 horas para exibir o último valor conhecido mesmo quando o dispositivo estiver offline.

