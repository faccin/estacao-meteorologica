#!/usr/bin/env python3
"""
Le dados JSON vindos pela porta serial (Heltec Base via USB)
e envia para o Flask local (POST /api/dados).
"""

import json
import sys
import time

import requests
import serial
import serial.tools.list_ports

FLASK_URL = "http://localhost:5000/api/dados"
BAUD_RATE = 115200


def encontrar_porta():
    portas = serial.tools.list_ports.comports()
    candidatos = ["CP210", "CH340", "USB Serial", "USB2.0-Serial", "USB-SERIAL"]
    for p in portas:
        if any(c in (p.description or "") for c in candidatos):
            return p.device
    if portas:
        return portas[0].device
    return None


def conectar(porta):
    print(f"Conectando na porta {porta} @ {BAUD_RATE} baud...")
    ser = serial.Serial(porta, BAUD_RATE, timeout=2)
    time.sleep(2)
    return ser


def main():
    porta = encontrar_porta()
    if not porta:
        print("Nenhuma porta serial encontrada.")
        print("Verifique se a Heltec base esta conectada via USB.")
        sys.exit(1)

    ser = conectar(porta)
    print("Aguardando dados da estacao base...")

    while True:
        try:
            linha = ser.readline().decode("utf-8", errors="ignore").strip()
            if not linha:
                continue

            if not linha.startswith("{"):
                print(f"[serial] {linha}")
                continue

            dados = json.loads(linha)
            print(f"[recebido] {dados}")

            resp = requests.post(FLASK_URL, json=dados, timeout=5)
            if resp.status_code == 200 or resp.status_code == 201:
                print("[OK] Enviado para o Flask")
            else:
                print(f"[FALHA] Flask respondeu {resp.status_code}")

        except json.JSONDecodeError:
            print(f"[aviso] Linha nao e JSON valido: {linha}")

        except serial.SerialException as e:
            print(f"[erro serial] {e} - tentando reconectar em 5s...")
            try:
                ser.close()
            except Exception:
                pass
            time.sleep(5)
            nova_porta = encontrar_porta()
            if nova_porta:
                ser = conectar(nova_porta)

        except requests.exceptions.RequestException as e:
            print(f"[erro requests] {e}")

        except KeyboardInterrupt:
            print("\nEncerrando...")
            ser.close()
            sys.exit(0)


if __name__ == "__main__":
    main()