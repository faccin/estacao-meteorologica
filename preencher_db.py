"""
Preenche o banco com 24h de dados simulados — inserção direta no SQLite.
Muito mais rápido que o simular.py (segundos em vez de minutos).

Uso:
    python preencher_db.py
"""

import sqlite3
import math
import random
import os
from datetime import datetime, timedelta

DB_PATH = os.path.join(os.path.dirname(__file__), "estacao.db")


def main():
    # Criar/conectar ao banco
    conn = sqlite3.connect(DB_PATH)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS leituras (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp  TEXT    NOT NULL,
            temperatura REAL,
            umidade     REAL,
            pressao     REAL
        )
    """)

    # Limpar dados antigos
    conn.execute("DELETE FROM leituras")
    conn.commit()

    agora = datetime.now()
    inicio = agora - timedelta(hours=24)

    dados = []
    for minuto in range(1440):
        hora = minuto / 60.0
        ts = inicio + timedelta(minutes=minuto)

        # Temperatura: senoide com mínima ~6h e máxima ~15h
        temp = 18.0 + 6.0 * math.sin((hora - 9) * math.pi / 12)
        temp += random.gauss(0, 0.3)

        # Umidade: inversamente correlacionada com temperatura
        umid = 70.0 - 15.0 * math.sin((hora - 9) * math.pi / 12)
        umid = max(30, min(99, umid + random.gauss(0, 1.5)))

        # Pressão: variação lenta com ruído
        pres = 1013.0 + 3.0 * math.sin(hora * math.pi / 24)
        pres += random.gauss(0, 0.4)

        dados.append((
            ts.isoformat(timespec="seconds"),
            round(temp, 1),
            round(umid, 1),
            round(pres, 1),
        ))

    conn.executemany(
        "INSERT INTO leituras (timestamp, temperatura, umidade, pressao) VALUES (?, ?, ?, ?)",
        dados,
    )
    conn.commit()
    conn.close()

    print(f"Pronto! {len(dados)} leituras inseridas.")
    print(f"  Primeiro: {dados[0][0]}")
    print(f"  Último:   {dados[-1][0]}")
    print(f"  Temp min/max: {min(d[1] for d in dados):.1f} / {max(d[1] for d in dados):.1f} °C")


if __name__ == "__main__":
    main()
