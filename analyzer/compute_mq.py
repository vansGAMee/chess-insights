#!/usr/bin/env python3
"""Вычисляет Loss Index (MQ-Chess) по кешу и обновляет stats.json."""

import json
import chess.pgn
import io
from pathlib import Path
import sys

try:
    import stats_cpp
except ImportError:
    print("❌ C++ модуль stats_cpp не найден. Скомпилируйте: python setup.py build_ext --inplace")
    sys.exit(1)

USERNAME = "testg123"   # <-- замени на свой ник
CACHE_FILE = Path("cache/analyzed.json")
STATS_FILE = Path("docs/stats.json")

if not CACHE_FILE.exists():
    print("❌ Кеш не найден. Сначала запустите fetch_and_analyze.py")
    sys.exit(1)

with open(CACHE_FILE) as f:
    cache = json.load(f)

print(f"Найдено {len(cache)} игр в кеше. Добавляю legal_moves и time_spent...")

for url, game_data in cache.items():
    if "legal_moves" in game_data and "time_spent" in game_data:
        continue
    pgn_text = game_data.get("pgn", "")
    try:
        game = chess.pgn.read_game(io.StringIO(pgn_text))
    except:
        continue
    if game is None:
        continue

    legal_moves_list = []
    time_spent_list = []
    board = game.board()
    clocks = game_data.get("clocks", [])

    mainline = list(game.mainline_moves())
    for i, move in enumerate(mainline):
        legal_moves_list.append(board.legal_moves.count())
        board.push(move)

        # Время на ход: если нет данных, ставим -1.0 (как в оригинальном MQ-Chess)
        spent = -1.0
        if i < len(clocks):
            if i > 0:
                spent = max(0.0, clocks[i-1] - clocks[i])
        time_spent_list.append(spent)

    game_data["legal_moves"] = legal_moves_list
    game_data["time_spent"] = time_spent_list

with open(CACHE_FILE, "w") as f:
    json.dump(cache, f, indent=2)
print("✅ legal_moves и time_spent добавлены.")

print("Пересчитываю статистику с учётом MQ-Chess...")
cache_json_str = json.dumps(cache)
stats_json_str = stats_cpp.compute(cache_json_str, USERNAME)
stats = json.loads(stats_json_str)

STATS_FILE.parent.mkdir(exist_ok=True)
with open(STATS_FILE, "w") as f:
    json.dump(stats, f, indent=2, ensure_ascii=False)

print(f"✅ Loss Index: {stats.get('loss_index_avg', 0):.2f}%")
print("✅ stats.json обновлён. Откройте дашборд.")