# ♟️ Chess Insights — Free alternative to Diamond analytics

**Python + C++ + Stockfish** → 10+ metrics on a beautiful dashboard.  
Hosted on GitHub Pages, auto‑updated via Actions.

## How to run locally (Arch/Ubuntu)
1. Install Stockfish: `sudo pacman -S stockfish` or `sudo apt install stockfish`
2. `pip install -r analyzer/requirements.txt`
3. In `fetch_and_analyze.py`, replace `your_chesscom_username` with your own nickname.
4. In `stats_cpp.cpp`, also replace the nickname in the line `static std::string USERNAME = ...`
5. Build the C++ module: `cd analyzer && python setup.py build_ext --inplace`
6. Run: `python analyzer/fetch_and_analyze.py`
7. Open `docs/index.html` in your browser.

## Autodeploy
- Enable GitHub Pages for the `/docs` folder in your repository settings.
- GitHub Actions will update the statistics daily.

## 👨‍💻 Author
**Ivan (IvanKulkin)**  
Developer and creator of the project.  
- GitHub: [@vansGAMee](https://github.com/vansGAMee/MQ-Chess.git)

### 🧠 Original MQ-Chess metric
Loss Index (MQ-Chess) — an original accuracy metric developed by me in 2024.  
Learn more in the [repository](https://github.com/vansGAMee/MQ-Chess.git).
