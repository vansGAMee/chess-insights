# ♟️ Chess Insights — бесплатный аналог Diamond аналитики

**Python + C++ + Stockfish** → 10+ метрик на красивом дашборде.  
Хостинг на GitHub Pages, автообновление через Actions.

## Как запустить локально (Arch/Ubuntu)
1. Установите Stockfish: `sudo pacman -S stockfish` или `sudo apt install stockfish`
2. `pip install -r analyzer/requirements.txt`
3. В `fetch_and_analyze.py` замените `your_chesscom_username` на свой ник.
4. В `stats_cpp.cpp` также замените ник в строке `static std::string USERNAME = ...`
5. Соберите C++ модуль: `cd analyzer && python setup.py build_ext --inplace`
6. Запустите: `python analyzer/fetch_and_analyze.py`
7. Откройте `docs/index.html` в браузере.

## Автодеплой
- Включите GitHub Pages для папки `/docs` в настройках репозитория.
- GitHub Actions будет ежедневно обновлять статистику.


## 👨‍💻 Автор
**Ivan (IvanKulkin)**  
Разработчик и автор проекта.  
- GitHub: [@vansGAMee](https://github.com/vansGAMee/MQ-Chess.git)   

### 🧠 Авторская метрика MQ-Chess
Loss Index (MQ-Chess) — оригинальная метрика точности игры, разработанная мной в 2024 году.  
Подробнее в [репозитории](https://github.com/vansGAMee/MQ-Chess.git) (https://github.com/vansGAMee/MQ-Chess.git).
