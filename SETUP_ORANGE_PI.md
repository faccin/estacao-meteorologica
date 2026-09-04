# Setup da Orange Pi Zero 2W — Estação Meteorológica Educacional

## 1. Sistema operacional

Gravar o **Armbian** (Debian Bookworm) no cartão microSD:
- Download: https://www.armbian.com/orange-pi-zero-2w/
- Gravar com balenaEtcher ou `dd`

Primeiro boot: criar usuário, definir senha, conectar via Ethernet ou WiFi temporário para instalar pacotes.

---

## 2. Instalar dependências

```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y python3 python3-pip hostapd dnsmasq
pip3 install flask --break-system-packages
```

---

## 3. Configurar o hotspot WiFi

Os alunos vão conectar no WiFi da Orange Pi — sem precisar de internet.

### 3.1 — Configurar IP estático na interface WiFi

Editar `/etc/network/interfaces` (ou usar nmcli se o Armbian usar NetworkManager):

```
# /etc/network/interfaces (adicionar ao final)
auto wlan0
iface wlan0 inet static
    address 192.168.4.1
    netmask 255.255.255.0
```

### 3.2 — hostapd (ponto de acesso)

Criar `/etc/hostapd/hostapd.conf`:

```
interface=wlan0
driver=nl80211
ssid=EstacaoMeteo
hw_mode=g
channel=7
wmm_enabled=0
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=meteorologia
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
```

Editar `/etc/default/hostapd` e definir:
```
DAEMON_CONF="/etc/hostapd/hostapd.conf"
```

### 3.3 — dnsmasq (DHCP para os alunos)

Fazer backup e criar novo `/etc/dnsmasq.conf`:

```bash
sudo mv /etc/dnsmasq.conf /etc/dnsmasq.conf.bak
```

```
# /etc/dnsmasq.conf
interface=wlan0
dhcp-range=192.168.4.10,192.168.4.50,255.255.255.0,24h
address=/#/192.168.4.1
```

> A linha `address=/#/...` faz com que **qualquer URL** digitada no celular redirecione para a estação — os alunos não precisam lembrar o IP!

### 3.4 — Ativar tudo

```bash
sudo systemctl unmask hostapd
sudo systemctl enable hostapd dnsmasq
sudo systemctl start hostapd dnsmasq
```

---

## 4. Copiar o projeto

Transferir a pasta `estacao-meteorologica/` para a Orange Pi (via SCP, pendrive, ou git):

```bash
scp -r estacao-meteorologica/ usuario@192.168.x.x:~/
```

---

## 5. Rodar automaticamente no boot

Criar o serviço systemd:

```bash
sudo nano /etc/systemd/system/estacao.service
```

```ini
[Unit]
Description=Estação Meteorológica Educacional
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/home/usuario/estacao-meteorologica
ExecStart=/usr/bin/python3 app.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Ativar:

```bash
sudo systemctl daemon-reload
sudo systemctl enable estacao
sudo systemctl start estacao
```

---

## 6. Conexão do Heltec V4

O Heltec envia dados via HTTP POST para `http://192.168.4.1:5000/api/dados`.

**Opção A — WiFi direto:**
O Heltec conecta no hotspot da Orange Pi via WiFi e faz POST pela rede.

**Opção B — USB Serial:**
O Heltec conecta via USB na Orange Pi. Um script lê a serial e posta:

```python
# serial_bridge.py
import serial, json, urllib.request, time

ser = serial.Serial('/dev/ttyUSB0', 115200)

while True:
    linha = ser.readline().decode().strip()
    try:
        dados = json.loads(linha)
        payload = json.dumps(dados).encode()
        req = urllib.request.Request(
            'http://localhost:5000/api/dados',
            data=payload,
            headers={'Content-Type': 'application/json'}
        )
        urllib.request.urlopen(req)
        print(f"OK: {dados}")
    except Exception as e:
        print(f"Erro: {e}")
```

---

## 7. Uso em sala de aula

1. Ligar a Orange Pi (alimentação USB-C, 5V/2A)
2. Esperar ~30 segundos para o boot
3. Os alunos conectam no WiFi **EstacaoMeteo** (senha: `meteorologia`)
4. Abrem qualquer página no navegador → redirecionados para a estação
5. Dados em tempo real, gráficos e exportação CSV disponíveis

### Para testar sem o Heltec:

```bash
python3 simular.py --rapido      # preenche 24h de dados
python3 simular.py               # simula leituras contínuas
```

---

## Rede WiFi

| Parâmetro | Valor |
|-----------|-------|
| SSID | EstacaoMeteo |
| Senha | meteorologia |
| IP do servidor | 192.168.4.1 |
| URL da interface | http://192.168.4.1:5000 |

---

## Solução de problemas

- **Alunos não encontram a rede:** `sudo systemctl restart hostapd`
- **Página não abre:** `sudo systemctl status estacao` para ver logs
- **Dados não aparecem:** verificar se o Heltec está enviando com `curl http://localhost:5000/api/atual`
