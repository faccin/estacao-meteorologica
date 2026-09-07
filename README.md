# 🌦️ Estação Meteorológica Educacional

**Colégio Franciscano Cristo Rei · Marau, RS**
Professor Regente: Dr. José Ricardo Faccin · Turma 9º Ano — 2026

Estação meteorológica off-grid para uso em sala de aula. Os alunos acompanham temperatura, umidade e pressão em tempo real pelo celular, conectando no WiFi da estação — sem depender de internet.

📖 **[Guia completo de montagem](https://faccin.github.io/estacao-meteorologica/)**

---

## Arquitetura

Dois módulos Heltec se comunicam por rádio **LoRa** (915 MHz), não por WiFi. Isso dá alcance de centenas de metros entre o ponto de coleta e a Orange Pi — o WiFi da Orange Pi fica livre só para os celulares dos alunos acessarem o painel.

```
┌──────────┐         ┌─────────────────┐    LoRa 915MHz    ┌─────────────────┐   USB    ┌─────────────┐   WiFi    ┌──────────┐
│  BMP280  │──I2C───▶│                 │───────────────────▶│                 │─────────▶│             │◀─────────▶│ Celular  │
│ (pressão)│         │  Heltec #1      │      rádio         │  Heltec #2      │  serial   │  Orange Pi  │           │ do aluno │
│  DHT22   │──GPIO──▶│  "nó remoto"    │                     │  "base"         │           │  Flask +    │           │ navegador│
│(umidade) │         │  + OLED         │                     │  + OLED         │           │  SQLite     │           └──────────┘
└──────────┘         └─────────────────┘                     └─────────────────┘           └─────────────┘
   sensores          lê → exibe → transmite            recebe → repassa por USB       recebe → armazena → serve
```

- **Heltec #1 (nó remoto):** fica junto aos sensores, onde quer que a coleta precise acontecer. Não precisa de WiFi nem de estar perto da Orange Pi.
- **Heltec #2 (base):** conectada por cabo USB na Orange Pi. Só recebe o rádio e repassa pela serial — não tem sensor nenhum ligado nela.
- **Orange Pi:** roda o hotspot (`EstacaoMeteo`), o servidor Flask e o script que lê a serial da base.

## Componentes

| Item | Função |
|---|---|
| 2× Heltec WiFi LoRa 32 V3/V4 (ESP32-S3 + SX1262 + OLED) | Nó remoto (sensores + transmissão) e base (recepção) |
| BMP280 | Sensor I2C — pressão (300–1100 hPa) e temperatura |
| DHT22 | Sensor digital — umidade (0–100%) e temperatura |
| Orange Pi Zero 2W | Hotspot WiFi + servidor Flask + SQLite |
| Cabo USB-C | Liga a Heltec base na Orange Pi |

## Início rápido

```bash
# Na Orange Pi
git clone https://github.com/faccin/estacao-meteorologica.git
cd estacao-meteorologica
pip3 install flask pyserial requests --break-system-packages

# Testar sem hardware (dados simulados)
python3 preencher_db.py
python3 app.py
# Acesse http://192.168.4.1:5000 conectado no WiFi EstacaoMeteo
```

Para o fluxo completo com hardware real, siga o **[guia de montagem](https://faccin.github.io/estacao-meteorologica/)**.

## Rede WiFi (para os alunos acessarem o painel)

- SSID: `EstacaoMeteo`
- Senha: `12345678`
- Painel: `http://192.168.4.1:5000`

## API REST

| Método | Endpoint | Descrição |
|---|---|---|
| `POST` | `/api/dados` | Recebe uma leitura (JSON: `temperatura`, `umidade`, `pressao`, `timestamp` opcional) |
| `GET` | `/api/atual` | Última leitura |
| `GET` | `/api/dados?horas=6` | Histórico das últimas N horas |
| `GET` | `/api/estatisticas?horas=24` | Mínimo, máximo e média do período |
| `GET` | `/api/exportar?horas=24` | Download CSV |

## Estrutura do projeto

```
estacao-meteorologica/
├── app.py                          # Servidor Flask (backend) — roda na Orange Pi
├── simular.py                      # Simulador de sensores via HTTP POST
├── preencher_db.py                 # Preenche o banco direto (teste rápido)
├── leitor_serial_orangepi.py       # Lê a serial da base LoRa e envia pro Flask
├── estacao.service                 # Serviço systemd do Flask
├── estacao-lora.service            # Serviço systemd do leitor serial
├── SETUP_ORANGE_PI.md              # Guia de configuração da Orange Pi
├── static/
│   └── index.html                  # Interface web (frontend)
├── docs/
│   └── index.html                  # Guia completo (GitHub Pages)
└── firmware/
    ├── no_remoto_sensores/
    │   └── no_remoto_sensores.ino  # Firmware do Heltec #1 (sensores + LoRa TX)
    └── base_receptora/
        └── base_receptora.ino      # Firmware do Heltec #2 (LoRa RX + USB)
```

## Uso em sala de aula

1. Ligue a Orange Pi (o hotspot e o painel sobem sozinhos, sem precisar de tela ou teclado).
2. Ligue as duas Heltecs — o nó remoto começa a transmitir, a base recebe e repassa.
3. Os alunos conectam o celular no WiFi `EstacaoMeteo` e abrem `192.168.4.1:5000`.
4. O painel mostra os dados em tempo real, com histórico e exportação em CSV para os alunos analisarem em planilha.

## Licença

Projeto educacional — Colégio Franciscano Cristo Rei.
