# Estação Meteorológica Educacional

**Colégio Franciscano Cristo Rei · Marau, RS**
**Professor Regente:** Dr. José Ricardo Faccin · Turma 9º Ano — 2026

Estação meteorológica off-grid para uso em sala de aula. Os alunos acessam dados de temperatura, umidade e pressão em tempo real pelo celular, conectando no WiFi da estação — sem precisar de internet.

**[Guia completo de montagem (GitHub Pages)](https://faccin.github.io/estacao-meteorologica/)**

## Arquitetura

```
┌──────────┐              ┌─────────────┐              ┌─────────────┐           ┌──────────┐
│  BMP280  │───I2C───────▶│             │  WiFi POST   │             │  WiFi     │ Celular  │
│ (pressão)│              │  Heltec V4  │─────────────▶│  Orange Pi  │◀─────────▶│ do aluno │
│  DHT22   │───GPIO 7───▶│  ESP32-S3   │              │  Flask +    │           │ navegador│
│(umidade) │              │  + OLED     │              │  SQLite     │           └──────────┘
└──────────┘              └─────────────┘              └─────────────┘
   sensores               lê → exibe → envia         recebe → armazena → serve
```

## Componentes

| Componente | Função |
|---|---|
| **Heltec WiFi LoRa 32 V3** | Microcontrolador ESP32-S3 — lê os sensores e envia via WiFi |
| **Orange Pi Zero 2W** | Servidor web — recebe dados, armazena em SQLite, serve a interface |
| **BMP280** | Sensor I2C — pressão atmosférica (300–1100 hPa) e temperatura |
| **DHT22** | Sensor digital — umidade relativa (0–100%) e temperatura |

## Início rápido (simulador no PC)

Não precisa do hardware para testar:

```bash
# Instalar dependência
pip install flask

# Iniciar o servidor
python app.py &

# Preencher 24h de dados simulados (instantâneo)
python preencher_db.py

# Abrir no navegador: http://localhost:5000
```

O simulador contínuo também está disponível:

```bash
python simular.py                 # 1 leitura por segundo
python simular.py --intervalo 60  # 1 por minuto (ritmo real)
```

## Interface web

A interface mostra:

- **Leituras em tempo real** — temperatura, umidade e pressão com atualização a cada 10s
- **Gráficos interativos** — períodos de 1h, 3h, 6h, 12h e 24h
- **Tendência barométrica** — indica se o tempo vai mudar
- **Estatísticas** — mínima, máxima e média de 24h
- **Exportação CSV** — os alunos baixam os dados para analisar no Excel

## Ligações elétricas

| Sensor | Pino | Heltec V4 |
|---|---|---|
| BMP280 SDA | → | GPIO 41 |
| BMP280 SCL | → | GPIO 42 |
| DHT22 DATA | → | GPIO 7 |
| Ambos VCC | → | 3.3V |
| Ambos GND | → | GND |

## API REST

| Método | Endpoint | Descrição |
|---|---|---|
| `POST` | `/api/dados` | Recebe leitura dos sensores (JSON) |
| `GET` | `/api/atual` | Última leitura |
| `GET` | `/api/dados?horas=6` | Histórico das últimas N horas |
| `GET` | `/api/estatisticas?horas=24` | Min, max e média do período |
| `GET` | `/api/exportar?horas=24` | Download CSV |

## Estrutura do projeto

```
estacao-meteorologica/
├── app.py                  # Servidor Flask (backend)
├── simular.py              # Simulador de sensores (HTTP POST)
├── preencher_db.py         # Preenchimento rápido do banco (SQLite direto)
├── static/
│   └── index.html          # Interface web (frontend)
├── docs/
│   └── index.html          # Guia completo (GitHub Pages)
└── firmware/
    └── estacao_heltec/
        └── estacao_heltec.ino  # Firmware do Heltec V4
```

## Em sala de aula

1. Ligar a Orange Pi (USB-C 5V/2A) — aguardar ~30s
2. Ligar o Heltec V4 (USB ou bateria LiPo)
3. Alunos conectam no WiFi **EstacaoMeteo** (senha: `meteorologia`)
4. Abrem o navegador → interface abre automaticamente
5. Dados em tempo real + gráficos + exportação CSV

## Rede WiFi (em campo)

| | |
|---|---|
| **SSID** | `EstacaoMeteo` |
| **Senha** | `meteorologia` |
| **Interface** | `http://192.168.4.1:5000` |

## Licença

MIT

---

*Heltec V4 + Orange Pi Zero 2W + BMP280 + DHT22*
*Colégio Franciscano Cristo Rei · Marau, RS*
