#!/usr/bin/env python3
import asyncio, aiohttp, chess, chess.engine, chess.pgn, io, json, os, time
from pathlib import Path
from datetime import datetime, timezone
from collections import defaultdict
import tqdm.asyncio
import stats_cpp  # C++ модуль

USERNAME = "testg123"   # <-- замени на свой ник
STOCKFISH_PATH = "/usr/bin/stockfish"
CACHE_DIR = Path("cache")
CACHE_DIR.mkdir(exist_ok=True)
CACHE_FILE = CACHE_DIR / "analyzed.json"

DEPTH = 19
TIME_PER_MOVE = 0.1
MAX_NEW_GAMES = 200

# ---------- Загрузка игр ----------
async def fetch_games_month(session, year, month):
    url = f"https://api.chess.com/pub/player/{USERNAME}/games/{year}/{month:02d}"
    async with session.get(url) as resp:
        if resp.status == 200:
            data = await resp.json()
            return data.get("games", [])
        return []

async def fetch_all_games():
    today = datetime.now()
    async with aiohttp.ClientSession() as session:
        tasks = []
        for y in range(2020, today.year + 1):
            for m in range(1, 13):
                if y == today.year and m > today.month:
                    break
                tasks.append(fetch_games_month(session, y, m))
        results = await tqdm.asyncio.tqdm.gather(*tasks, desc="Fetching months")
        all_games = []
        seen = set()
        for games in results:
            for g in games:
                url = g.get("url", "")
                if url not in seen:
                    seen.add(url)
                    all_games.append(g)
        return all_games

# ---------- Извлечение расширенных данных из PGN (без legal_moves/time_spent) ----------
def extract_extra(pgn_text):
    try:
        game = chess.pgn.read_game(io.StringIO(pgn_text))
    except:
        return None
    if game is None:
        return None
    headers = dict(game.headers)
    end_ts = 0
    end_time_str = headers.get("EndTime", "")
    if end_time_str.isdigit():
        end_ts = int(end_time_str)
    else:
        date_str = headers.get("Date", "")
        time_str = headers.get("EndTime", "")
        if not time_str:
            time_str = headers.get("UTCTime", "")
        if date_str and time_str:
            try:
                dt_str = date_str.replace(".", "-") + "T" + time_str + "Z"
                dt = datetime.strptime(dt_str, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
                end_ts = int(dt.timestamp())
            except:
                pass
    time_control = headers.get("TimeControl", "")
    clocks = []
    node = game
    while node.next():
        node = node.next()
        c = node.clock()
        if c is not None:
            clocks.append(c)
    material_after_10 = 0
    board = game.board()
    for i, move in enumerate(game.mainline_moves()):
        board.push(move)
        if i == 9:
            material_after_10 = len(board.piece_map())
    early_mate = False
    if "checkmate" in headers.get("Termination", "").lower():
        if len(list(game.mainline_moves())) <= 15:
            early_mate = True
    return {
        "end_time": end_ts,
        "time_control": time_control,
        "clocks": clocks,
        "material_after_10": material_after_10 if material_after_10 else 0,
        "development_move": 0,
        "early_mate": early_mate
    }

# ---------- Анализ Stockfish ----------
async def analyze_pgn(pgn_text):
    try:
        game = chess.pgn.read_game(io.StringIO(pgn_text))
    except:
        return None
    if game is None:
        return None
    board = game.board()
    evals = []
    transport, engine = await chess.engine.popen_uci(STOCKFISH_PATH)
    try:
        for move in game.mainline_moves():
            info = await engine.analyse(board, chess.engine.Limit(time=TIME_PER_MOVE, depth=DEPTH))
            score = info["score"].pov(board.turn)
            cp = score.score() if not score.is_mate() else (10000 if score.mate() > 0 else -10000)
            evals.append(cp)
            board.push(move)
    finally:
        await engine.quit()
    return evals

# ---------- Кеширование ----------
def load_cache():
    if CACHE_FILE.exists():
        return json.loads(CACHE_FILE.read_text())
    return {}

def save_cache(cache):
    CACHE_FILE.write_text(json.dumps(cache, indent=2))

# ---------- Главный процесс ----------
async def main():
    print("📥 Fetching games...")
    all_games = await fetch_all_games()
    print(f"Total games on chess.com: {len(all_games)}")

    cache = load_cache()
    new_games = [g for g in all_games if g.get("url") not in cache]
    new_games = new_games[:MAX_NEW_GAMES]
    print(f"New games to analyze: {len(new_games)} (cache size: {len(cache)})")

    for g in tqdm.tqdm(new_games, desc="Stockfish analysis"):
        pgn = g.get("pgn", "")
        evals = await analyze_pgn(pgn)
        extra = extract_extra(pgn)
        if evals and extra:
            cache[g["url"]] = {
                "pgn": pgn,
                "evals": evals,
                **extra
            }
    save_cache(cache)
    print("💾 Cache updated, computing stats with C++...")
    cache_json_str = json.dumps(cache)
    stats_json_str = stats_cpp.compute(cache_json_str, USERNAME)
    stats = json.loads(stats_json_str)

    docs_dir = Path("docs")
    docs_dir.mkdir(exist_ok=True)
    (docs_dir / "stats.json").write_text(json.dumps(stats, indent=2, ensure_ascii=False))
    print("✅ stats.json ready! Open docs/index.html")

if __name__ == "__main__":
    asyncio.run(main())