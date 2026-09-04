"""
Simulador de Sensores — para testar sem o hardware
====================================================
Gera leituras realistas de temperatura, umidade e pressão
e envia para o servidor Flask via HTTP POST.

Simula um ciclo diário típico do RS:
  - Temperatura: mínima de madrugada, máxima à tarde
  - Umidade: inversa da temperatura
  - Pressão: variação lenta com ruído

Uso:
    python simular.py                  # 1 leitura por segundo
    python simular.py --intervalo 60   # 1 por minuto (como na prática)
    python simular.py --rapido         # preenche 24h de dados rapidamente
"""

import argparse
import json
import math
import random
import time
import urllib.request

URL = "http://localhost:5000/api/dados"


def gerar_leitura(hora_decimal, ruido=True):
    """
    Gera uma leitura baseada na hora do dia (0-24).
    Os valores simulam um dia típico de inverno/primavera no RS.
    """
    # Temperatura: senoide com mínima ~6h e máxima ~15h
    temp_base = 18.0  # média
    temp_amplitude = 6.0  # variação
    temp = temp_base + temp_amplitude * math.sin((hora_decimal - 9) * math.pi / 12)

    # Umidade: inversamente correlacionada com temperatura
    umid_base = 70.0
    umid_amplitude = 15.0
    umid = umid_base - umid_amplitude * math.sin((hora_decimal - 9) * math.pi / 12)
    umid = max(30, min(99, umid))

    # Pressão: variação lenta (tendência + ruído)
    pres = 1013.0 + 3.0 * math.sin(hora_decimal * math.pi / 24)

    if ruido:
        temp += random.gauss(0, 0.3)
        umid += random.gauss(0, 1.5)
        pres += random.gauss(0, 0.4)

    return {
        "temperatura": round(temp, 1),
        "umidade": round(max(30, min(99, umid)), 1),
        "pressao": round(pres, 1),
    }


def enviar(dados):
    """Envia uma leitura para o servidor via POST."""
    payload = json.dumps(dados).encode("utf-8")
    req = urllib.request.Request(
        URL,
        data=payload,
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(req) as resp:
            return resp.status == 201
    except Exception as e:
        print(f"  Erro ao enviar: {e}")
        return False


def modo_rapido():
    """Preenche 24h de dados de uma vez (1 leitura por minuto = 1440 pontos)."""
    print("Preenchendo 24 horas de dados simulados...")
    enviados = 0
    for minuto in range(1440):
        hora = minuto / 60.0
        dados = gerar_leitura(hora)
        if enviar(dados):
            enviados += 1
        if minuto % 60 == 0:
            print(f"  Hora {int(hora):02d}:00 — {dados}")
    print(f"Pronto! {enviados} leituras enviadas.")


def modo_continuo(intervalo):
    """Envia leituras continuamente no intervalo especificado."""
    print(f"Enviando leituras a cada {intervalo}s — Ctrl+C para parar")
    print()
    contador = 0
    while True:
        # Usa a hora real para a simulação ficar coerente
        agora = time.localtime()
        hora_decimal = agora.tm_hour + agora.tm_min / 60.0 + agora.tm_sec / 3600.0
        dados = gerar_leitura(hora_decimal)

        ok = enviar(dados)
        contador += 1
        status = "✓" if ok else "✗"
        print(
            f"  [{status}] #{contador:04d}  "
            f"T={dados['temperatura']:5.1f}°C  "
            f"U={dados['umidade']:5.1f}%  "
            f"P={dados['pressao']:7.1f} hPa"
        )
        time.sleep(intervalo)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Simulador de sensores meteorológicos")
    parser.add_argument("--intervalo", type=float, default=1,
                        help="Segundos entre leituras (padrão: 1)")
    parser.add_argument("--rapido", action="store_true",
                        help="Preenche 24h de dados de uma vez")
    args = parser.parse_args()

    if args.rapido:
        modo_rapido()
    else:
        modo_continuo(args.intervalo)
