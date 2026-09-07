# Configuração da Orange Pi Zero 2W

Este guia documenta os passos **testados e funcionando** para transformar a Orange Pi em um hotspot WiFi off-grid, rodando o servidor Flask da estação meteorológica. Sistema: Armbian (Debian trixie).

---

## 1. Acesso inicial

Conecte um monitor + teclado USB (só na primeira vez) ou acesse via SSH se a Orange Pi já estiver em alguma rede. Usuário padrão do Armbian pede para criar login no primeiro boot.

```bash
sudo apt update && sudo apt upgrade -y
```

---

## 2. Hotspot WiFi (hostapd + dnsmasq)

A Orange Pi usada aqui **não tem NetworkManager** nem `dhcpcd` — o gerenciamento de rede é via `netplan` + `systemd-networkd`. Os passos abaixo já contornam isso.

### 2.1 Instalar os pacotes

```bash
sudo apt install hostapd dnsmasq -y
sudo systemctl unmask hostapd
sudo systemctl stop hostapd dnsmasq
```

### 2.2 Desativar qualquer config de wifi-cliente existente

Verifique se não há um arquivo netplan configurando a `wlan0` como cliente wifi (procurando por uma rede tipo `access-points:`):

```bash
grep -rl "wlan0" /etc/netplan/
```

Se encontrar, comente ou apague o bloco `wifis: wlan0:` desse arquivo — ele vai brigar com o modo Access Point.

### 2.3 IP estático na wlan0

O netplan trata interfaces wifi de forma especial, então o truque é declarar a `wlan0` como se fosse uma interface `ethernets` — isso faz o `systemd-networkd` só atribuir o IP, sem tentar autenticar como cliente wifi (quem cuida do rádio é o hostapd).

```bash
sudo tee /etc/netplan/99-hotspot.yaml > /dev/null << 'EOF'
network:
  version: 2
  renderer: networkd
  ethernets:
    wlan0:
      dhcp4: no
      dhcp6: no
      addresses:
        - 192.168.4.1/24
EOF

sudo chmod 600 /etc/netplan/99-hotspot.yaml
sudo netplan apply
```

### 2.4 Configurar o hostapd

```bash
sudo mkdir -p /etc/hostapd
sudo tee /etc/hostapd/hostapd.conf > /dev/null << 'EOF'
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
wpa_passphrase=12345678
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
EOF

sudo sed -i 's|#DAEMON_CONF=""|DAEMON_CONF="/etc/hostapd/hostapd.conf"|' /etc/default/hostapd
```

> ⚠️ **Não inclua `wpa_pairwise=TKIP`** — esse protocolo é rejeitado por celulares Android mais recentes e impede a conexão. Use só `rsn_pairwise=CCMP`.
> Troque `12345678` por outra senha se quiser, desde que tenha 8+ caracteres.

### 2.5 Configurar o dnsmasq (DHCP)

**Importante:** o Armbian já vem com `systemd-resolved` ocupando a porta 53 (DNS). Por isso, desativamos a parte de DNS do dnsmasq e deixamos só o DHCP:

```bash
sudo tee -a /etc/dnsmasq.conf > /dev/null << 'EOF'
interface=wlan0
dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h
port=0
EOF
```

### 2.6 Ativar tudo permanentemente

```bash
sudo systemctl enable hostapd dnsmasq
sudo systemctl start hostapd
sudo systemctl start dnsmasq
```

### 2.7 Confirmar

```bash
ip addr show wlan0        # deve mostrar 192.168.4.1/24
sudo systemctl status hostapd dnsmasq --no-pager
```

No celular, a rede **EstacaoMeteo** deve aparecer disponível. Conecte com a senha configurada — o problema mais comum se não conectar é o DHCP não estar de pé (`systemctl status dnsmasq`) ou a interface sem carrier (isso se resolve sozinho assim que o hostapd sobe e coloca a wlan0 em modo AP).

---

## 3. Servidor Flask

### 3.1 Copiar os arquivos do projeto

```bash
git clone https://github.com/faccin/estacao-meteorologica.git
cd estacao-meteorologica
pip3 install flask pyserial requests --break-system-packages
```

### 3.2 Testar sem hardware

```bash
python3 preencher_db.py   # gera 24h de dados simulados
python3 app.py
```

Acesse `http://192.168.4.1:5000` de um dispositivo conectado no hotspot.

### 3.3 Subir automaticamente no boot

```bash
sudo tee /etc/systemd/system/estacao.service > /dev/null << EOF
[Unit]
Description=Estacao Meteorologica - Servidor Flask
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=$(pwd)
ExecStart=/usr/bin/python3 app.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now estacao.service
```

---

## 4. Leitor da base LoRa (recebe os dados dos sensores)

A Heltec "base" fica conectada por USB na Orange Pi e repassa os dados recebidos via LoRa como linhas JSON na serial. O script `leitor_serial_orangepi.py` lê isso e envia para o Flask.

```bash
sudo systemctl enable --now estacao-lora.service
```

Verifique os logs para confirmar que está recebendo dados:

```bash
sudo journalctl -u estacao-lora -f
```

---

## 5. Uso 100% sem tela (opcional)

Depois de confirmar que tudo sobe sozinho no boot, dá para desativar a tela de login no console local (o acesso continua funcionando por SSH ou pela própria estação):

```bash
sudo systemctl disable getty@tty1.service
```

## 6. Desligar com segurança

```bash
sudo shutdown -h now
```

Nunca desconecte a alimentação sem rodar esse comando antes — pode corromper o cartão SD/eMMC.
