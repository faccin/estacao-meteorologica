"""
Estação Meteorológica Educacional — Backend
=============================================
Servidor Flask que recebe dados dos sensores (Heltec V4),
armazena em SQLite e serve uma interface web para os alunos.

Uso:
    python app.py

O Heltec envia dados via HTTP POST para /api/dados com JSON:
    {"temperatura": 25.3, "umidade": 72.1, "pressao": 1013.25}

Os alunos acessam a interface conectando no hotspot WiFi
da Orange Pi e abrindo http://192.168.4.1 no navegador.
"""

import sqlite3
import json
import os
from datetime import datetime, timedelta
from flask import Flask, request, jsonify, send_from_directory

# ── Configuração ──────────────────────────────────────────────
app = Flask(__name__, static_folder="static")
DB_PATH = os.path.join(os.path.dirname(__file__), "estacao.db")


# ── Banco de dados ────────────────────────────────────────────
def conectar_db():
    """Abre conexão com o SQLite e retorna (conexão, cursor)."""
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def inicializar_db():
    """Cria a tabela de leituras se não existir."""
    conn = conectar_db()
    conn.execute("""
        CREATE TABLE IF NOT EXISTS leituras (
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp  TEXT    NOT NULL,
            temperatura REAL,
            umidade     REAL,
            pressao     REAL
        )
    """)
    conn.commit()
    conn.close()


# ── Rotas da API ──────────────────────────────────────────────

@app.route("/api/dados", methods=["POST"])
def receber_dados():
    """
    Recebe uma leitura dos sensores via POST JSON.
    Campos esperados: temperatura, umidade, pressao
    """
    dados = request.get_json(force=True)

    temperatura = dados.get("temperatura")
    umidade = dados.get("umidade")
    pressao = dados.get("pressao")

    if temperatura is None or umidade is None or pressao is None:
        return jsonify({"erro": "Campos obrigatórios: temperatura, umidade, pressao"}), 400

    agora = datetime.now().isoformat(timespec="seconds")

    conn = conectar_db()
    conn.execute(
        "INSERT INTO leituras (timestamp, temperatura, umidade, pressao) VALUES (?, ?, ?, ?)",
        (agora, temperatura, umidade, pressao),
    )
    conn.commit()
    conn.close()

    return jsonify({"status": "ok", "timestamp": agora}), 201


@app.route("/api/dados", methods=["GET"])
def listar_dados():
    """
    Retorna as leituras recentes em JSON.
    Parâmetros opcionais:
        ?ultimas=N     → últimas N leituras (padrão: 360 = ~6h a cada 1min)
        ?horas=H       → leituras das últimas H horas
    """
    conn = conectar_db()

    horas = request.args.get("horas", type=float)
    ultimas = request.args.get("ultimas", default=360, type=int)

    if horas:
        desde = (datetime.now() - timedelta(hours=horas)).isoformat(timespec="seconds")
        rows = conn.execute(
            "SELECT * FROM leituras WHERE timestamp >= ? ORDER BY id ASC",
            (desde,),
        ).fetchall()
    else:
        rows = conn.execute(
            "SELECT * FROM leituras ORDER BY id DESC LIMIT ?", (ultimas,)
        ).fetchall()
        rows = list(reversed(rows))  # ordem cronológica

    conn.close()

    leituras = [
        {
            "timestamp": r["timestamp"],
            "temperatura": r["temperatura"],
            "umidade": r["umidade"],
            "pressao": r["pressao"],
        }
        for r in rows
    ]

    return jsonify(leituras)


@app.route("/api/atual", methods=["GET"])
def leitura_atual():
    """Retorna apenas a leitura mais recente."""
    conn = conectar_db()
    row = conn.execute(
        "SELECT * FROM leituras ORDER BY id DESC LIMIT 1"
    ).fetchone()
    conn.close()

    if not row:
        return jsonify({"erro": "Nenhuma leitura registrada ainda"}), 404

    return jsonify({
        "timestamp": row["timestamp"],
        "temperatura": row["temperatura"],
        "umidade": row["umidade"],
        "pressao": row["pressao"],
    })


@app.route("/api/estatisticas", methods=["GET"])
def estatisticas():
    """
    Retorna min, max e média das últimas N horas (padrão: 24).
    Útil para os alunos analisarem variações diárias.
    """
    horas = request.args.get("horas", default=24, type=float)
    desde = (datetime.now() - timedelta(hours=horas)).isoformat(timespec="seconds")

    conn = conectar_db()
    row = conn.execute("""
        SELECT
            COUNT(*)            AS total_leituras,
            MIN(temperatura)    AS temp_min,
            MAX(temperatura)    AS temp_max,
            AVG(temperatura)    AS temp_media,
            MIN(umidade)        AS umid_min,
            MAX(umidade)        AS umid_max,
            AVG(umidade)        AS umid_media,
            MIN(pressao)        AS pres_min,
            MAX(pressao)        AS pres_max,
            AVG(pressao)        AS pres_media
        FROM leituras
        WHERE timestamp >= ?
    """, (desde,)).fetchone()
    conn.close()

    if row["total_leituras"] == 0:
        return jsonify({"erro": "Sem dados no período"}), 404

    return jsonify({
        "periodo_horas": horas,
        "total_leituras": row["total_leituras"],
        "temperatura": {
            "min": round(row["temp_min"], 1),
            "max": round(row["temp_max"], 1),
            "media": round(row["temp_media"], 1),
        },
        "umidade": {
            "min": round(row["umid_min"], 1),
            "max": round(row["umid_max"], 1),
            "media": round(row["umid_media"], 1),
        },
        "pressao": {
            "min": round(row["pres_min"], 1),
            "max": round(row["pres_max"], 1),
            "media": round(row["pres_media"], 1),
        },
    })


@app.route("/api/exportar", methods=["GET"])
def exportar_csv():
    """
    Exporta os dados como CSV — os alunos podem baixar e
    abrir no Excel/Calc para fazer suas próprias análises.
    """
    horas = request.args.get("horas", default=24, type=float)
    desde = (datetime.now() - timedelta(hours=horas)).isoformat(timespec="seconds")

    conn = conectar_db()
    rows = conn.execute(
        "SELECT timestamp, temperatura, umidade, pressao FROM leituras WHERE timestamp >= ? ORDER BY id ASC",
        (desde,),
    ).fetchall()
    conn.close()

    linhas = ["timestamp,temperatura_C,umidade_%,pressao_hPa"]
    for r in rows:
        linhas.append(f'{r["timestamp"]},{r["temperatura"]},{r["umidade"]},{r["pressao"]}')

    from flask import Response
    return Response(
        "\n".join(linhas),
        mimetype="text/csv",
        headers={"Content-Disposition": "attachment; filename=dados_estacao.csv"},
    )


# ── Servir a interface web ────────────────────────────────────

@app.route("/")
def index():
    return send_from_directory("static", "index.html")


# ── Inicialização ─────────────────────────────────────────────

if __name__ == "__main__":
    inicializar_db()
    print("=" * 50)
    print("  Estação Meteorológica Educacional")
    print("  Acesse: http://192.168.4.1:5000")
    print("=" * 50)
    # host 0.0.0.0 para aceitar conexões de qualquer dispositivo na rede
    app.run(host="0.0.0.0", port=5000, debug=False)
