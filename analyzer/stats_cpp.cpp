#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <regex>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <map>

namespace py = pybind11;
using json = nlohmann::json;

json parse_headers(const std::string& pgn) {
    json h;
    std::regex re(R"(\[(\w+)\s+\"([^\"]*)\"\])");
    for (std::sregex_iterator it(pgn.begin(), pgn.end(), re), end; it != end; ++it)
        h[(*it)[1]] = (*it)[2];
    return h;
}

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower(a[i]) != std::tolower(b[i])) return false;
    return true;
}

std::vector<std::string> extract_moves_san(const std::string& pgn) {
    std::vector<std::string> moves;
    std::string body = pgn;
    auto pos = body.find("\n\n");
    if (pos != std::string::npos) body = body.substr(pos + 2);
    std::regex comment(R"(\{[^}]*\})");
    body = std::regex_replace(body, comment, "");
    std::regex move_regex(R"(\b([Oo0]-[Oo0](-[Oo0])?|[KQRBN]?[a-h]?[1-8]?x?[a-h][1-8](=[QRBN])?[+#]?)(?:\s|$))");
    std::sregex_iterator it(body.begin(), body.end(), move_regex), end;
    for (; it != end; ++it) {
        std::string san = (*it)[1].str(); // берём первую группу
        if (san == "1-0" || san == "0-1" || san == "1/2-1/2") continue;
        moves.push_back(san);
    }
    return moves;
}

struct MoveProps {
    char piece = 'P';
    bool capture = false, check = false, checkmate = false;
    bool kingside_castle = false, queenside_castle = false;
    bool promotion = false;
    char promotion_piece = 0;
};

MoveProps parse_san(const std::string& san) {
    MoveProps m;
    if (san.empty()) return m;
    if (san == "O-O" || san == "0-0") { m.piece = 'K'; m.kingside_castle = true; return m; }
    if (san == "O-O-O" || san == "0-0-0") { m.piece = 'K'; m.queenside_castle = true; return m; }
    std::string s = san;
    if (!s.empty() && s.back() == '+') { m.check = true; s.pop_back(); }
    else if (!s.empty() && s.back() == '#') { m.checkmate = true; s.pop_back(); }
    auto eq = s.find('=');
    if (eq != std::string::npos && eq + 1 < s.size()) {
        m.promotion = true;
        m.promotion_piece = s[eq + 1];
        s = s.substr(0, eq);
    }
    if (s.empty()) return m;
    char first = s[0];
    if (first >= 'A' && first <= 'Z' && first != 'O') m.piece = first;
    else m.piece = 'P';
    m.capture = (s.find('x') != std::string::npos);
    return m;
}

long long safe_ll(const json& j, const std::string& key) {
    if (j.contains(key) && !j[key].is_null()) return j[key].get<long long>();
    return 0LL;
}
int safe_int(const json& j, const std::string& key) {
    if (j.contains(key) && !j[key].is_null()) return j[key].get<int>();
    return 0;
}
bool safe_bool(const json& j, const std::string& key) {
    if (j.contains(key) && !j[key].is_null()) return j[key].get<bool>();
    return false;
}

std::string compute(const std::string& cache_json_str, const std::string& username) {
    json cache = json::parse(cache_json_str);
    json stats;

    int total_games = 0, wins = 0, losses = 0, draws = 0;
    int win_white = 0, win_black = 0;
    std::unordered_map<char, int> piece_moves = {{'P',0},{'N',0},{'B',0},{'R',0},{'Q',0},{'K',0}};
    std::unordered_map<char, int> captures_by_piece = {{'P',0},{'N',0},{'B',0},{'R',0},{'Q',0},{'K',0}};
    int castles_k = 0, castles_q = 0, promotions = 0;
    std::unordered_map<char, int> promo_to = {{'Q',0},{'R',0},{'B',0},{'N',0}};
    int checks_given = 0;
    int brilliant = 0, great = 0, best = 0, mistakes = 0, blunders = 0;
    double sum_acpl = 0.0;
    int acpl_cnt = 0;
    double sum_acpl_phase[3] = {0,0,0}; int phase_cnt[3] = {0,0,0};
    std::unordered_map<int, int> hour_games, hour_wins;
    std::unordered_map<int, int> dow_games, dow_wins;
    std::unordered_map<std::string, int> eco_count, eco_win;
    std::vector<std::pair<long long, int>> rating_history;
    long long rating_sum = 0;
    int rating_count = 0;
    std::unordered_map<std::string, std::pair<int,int>> rivals;
    int loss_timeout = 0, loss_mate = 0, loss_resign = 0;
    int cur_win = 0, cur_loss = 0, max_win = 0, max_loss = 0;
    std::unordered_map<char, int> mating_piece;
    std::vector<int> first_mistake_moves;
    int endgame_games = 0, endgame_wins = 0;
    int gambit_games = 0, gambit_wins = 0;
    int early_mates = 0, saved_draws = 0;
    int vs_stronger = 0, win_vs_stronger = 0, vs_weaker = 0, win_vs_weaker = 0;
    int tilt_games = 0, tilt_wins = 0;
    int early_session_games = 0, early_session_wins = 0;
    int late_session_games = 0, late_session_wins = 0;
    int blunder_in_winning = 0, winning_positions = 0;
    int aggressive_opponent_games = 0, aggressive_opponent_wins = 0;
    double total_loss_index = 0.0;
    int loss_index_games = 0;
    std::vector<double> loss_index_per_game;

    struct GameInfo {
        long long end_time;
        std::string url, result, color, opponent_name;
        bool is_win;
        int player_elo, opponent_elo, material_after_10;
        std::vector<double> evals;
        std::vector<std::string> moves_san;
        std::vector<int> legal_moves;
        std::vector<double> time_spent;
    };
    std::vector<GameInfo> all_games_vec;
    std::map<std::string, std::vector<GameInfo>> games_by_day;

    for (auto& [url, item] : cache.items()) {
        std::string pgn = item["pgn"];
        std::vector<double> evals = item["evals"].get<std::vector<double>>();
        long long end_time = safe_ll(item, "end_time");
        int material_after_10 = safe_int(item, "material_after_10");
        bool early_mate_flag = safe_bool(item, "early_mate");
        std::vector<int> legal_moves = item.value("legal_moves", std::vector<int>());
        std::vector<double> time_spent = item.value("time_spent", std::vector<double>());
        std::vector<int> clocks = item.value("clocks", std::vector<int>());

        json headers = parse_headers(pgn);
        std::string white = headers.value("White", "");
        std::string black = headers.value("Black", "");
        std::string result = headers.value("Result", "*");
        std::string eco = headers.value("ECO", "?");
        std::string termination = headers.value("Termination", "");
        int welo = std::stoi(headers.value("WhiteElo", "0"));
        int belo = std::stoi(headers.value("BlackElo", "0"));

        std::string color;
        if (iequals(white, username)) color = "white";
        else if (iequals(black, username)) color = "black";
        else continue;

        int player_elo = (color == "white") ? welo : belo;
        int opponent_elo = (color == "white") ? belo : welo;
        std::string opponent_name = (color == "white") ? black : white;

        if (opponent_elo > 0) { rating_sum += opponent_elo; rating_count++; }
        if (player_elo > 0) rating_history.emplace_back(end_time, player_elo);

        total_games++;
        rivals[opponent_name].first++;
        bool is_win = false;
        if ((color == "white" && result == "1-0") || (color == "black" && result == "0-1")) {
            wins++; is_win = true;
            if (color == "white") win_white++; else win_black++;
            rivals[opponent_name].second++;
        } else if (result == "1/2-1/2") draws++;
        else losses++;

        if (is_win) { cur_win++; cur_loss = 0; max_win = std::max(max_win, cur_win); }
        else if (result == "1/2-1/2") { cur_win = cur_loss = 0; }
        else { cur_loss++; cur_win = 0; max_loss = std::max(max_loss, cur_loss); }

        auto moves_san = extract_moves_san(pgn);
        if (early_mate_flag) early_mates++;

        if (!is_win && result != "1/2-1/2") {
            bool mate = false, timeout = false;
            if (!moves_san.empty()) {
                MoveProps last = parse_san(moves_san.back());
                if (last.checkmate) mate = true;
            }
            if (!clocks.empty()) {
                int last_clock_idx = clocks.size() - 1;
                while (last_clock_idx >= 0 &&
                       ((color == "white" && last_clock_idx % 2 != 0) ||
                        (color == "black" && last_clock_idx % 2 == 0))) last_clock_idx--;
                if (last_clock_idx >= 0 && clocks[last_clock_idx] == 0) timeout = true;
            }
            if (mate) loss_mate++;
            else if (timeout) loss_timeout++;
            else loss_resign++;
        }

        eco_count[eco]++;
        if (is_win) eco_win[eco]++;

        if (!moves_san.empty() && is_win) {
            MoveProps last = parse_san(moves_san.back());
            if (last.checkmate) mating_piece[last.piece]++;
        }

        bool first_mistake_done = false;
        for (size_t i = 0; i < evals.size() && i < moves_san.size(); i++) {
            MoveProps mp = parse_san(moves_san[i]);
            piece_moves[mp.piece]++;
            if (mp.capture) captures_by_piece[mp.piece]++;
            if (mp.kingside_castle) castles_k++;
            if (mp.queenside_castle) castles_q++;
            if (mp.promotion) { promotions++; promo_to[mp.promotion_piece]++; }
            if (mp.check) checks_given++;

            bool player_move = (color == "white" && i % 2 == 0) || (color == "black" && i % 2 == 1);
            if (player_move && i > 0) {
                double p_eval = evals[i-1], c_eval = evals[i];
                if (color == "white") p_eval = -p_eval;
                else c_eval = -c_eval;
                double diff = c_eval - p_eval;
                double abs_diff = std::abs(diff);
                sum_acpl += abs_diff;
                acpl_cnt++;
                int phase = (i < 10) ? 0 : (i < 40 ? 1 : 2);
                sum_acpl_phase[phase] += abs_diff;
                phase_cnt[phase]++;

                if (diff > 600) brilliant++;
                else if (diff > 200) great++;
                else if (diff > 100) best++;
                else if (diff < -300) blunders++;
                else if (diff < -100) mistakes++;

                if (!first_mistake_done && diff < -100) {
                    first_mistake_moves.push_back(i / 2 + 1);
                    first_mistake_done = true;
                }
                if (evals[i-1] >= 300 && diff < -100) blunder_in_winning++;
                if (evals[i-1] >= 300) winning_positions++;
            }
        }

        if (moves_san.size() > 50) { endgame_games++; if (is_win) endgame_wins++; }
        if (material_after_10 > 0 && material_after_10 < 78) { gambit_games++; if (is_win) gambit_wins++; }
        if (result == "1/2-1/2") {
            for (auto e : evals) if (e <= -200) { saved_draws++; break; }
        }
        if (opponent_elo > 0 && player_elo > 0) {
            if (opponent_elo > player_elo + 100) { vs_stronger++; if (is_win) win_vs_stronger++; }
            else if (opponent_elo < player_elo - 100) { vs_weaker++; if (is_win) win_vs_weaker++; }
        }
        if (end_time > 0) {
            time_t t = end_time;
            struct tm* utc = gmtime(&t);
            int hour = utc->tm_hour, dow = utc->tm_wday;
            hour_games[hour]++; dow_games[dow]++;
            if (is_win) { hour_wins[hour]++; dow_wins[dow]++; }
            char dk[11]; strftime(dk, sizeof(dk), "%Y-%m-%d", utc);
            games_by_day[dk].push_back({end_time, url, result, color, opponent_name, is_win, player_elo,
                                        opponent_elo, material_after_10, evals, moves_san, legal_moves, time_spent});
        }
        if (material_after_10 > 0 && material_after_10 < 72) { aggressive_opponent_games++; if (is_win) aggressive_opponent_wins++; }

        // MQ-Chess (original)
        if (!legal_moves.empty() && !time_spent.empty() && evals.size() >= 2) {
            struct OrigMoveInput { int bestEval, playedEval, legalMoves; double timeSpent; };
            std::vector<OrigMoveInput> game_moves;
            for (size_t i = 0; i < evals.size() - 1 && i < legal_moves.size() && i < time_spent.size(); ++i) {
                bool is_white_move = (i % 2 == 0);
                bool is_player_move = (color == "white" && is_white_move) || (color == "black" && !is_white_move);
                if (!is_player_move) continue;
                OrigMoveInput mi;
                mi.bestEval = static_cast<int>(evals[i]);
                mi.playedEval = -static_cast<int>(evals[i+1]);
                mi.legalMoves = legal_moves[i];
                mi.timeSpent = time_spent[i];
                if (mi.legalMoves <= 1) continue;
                if (mi.timeSpent >= 0.0 && mi.timeSpent < 0.4) continue;
                game_moves.push_back(mi);
            }
            if (!game_moves.empty()) {
                auto cpToWinChance = [](int cp) {
                    if (cp > 1000) return 1.0;
                    if (cp < -1000) return -1.0;
                    return 2.0 / (1.0 + std::exp(-0.003624 * cp)) - 1.0;
                };
                double sumDeltaWeighted = 0.0, sumWeights = 0.0;
                for (const auto& m : game_moves) {
                    double Wbest = cpToWinChance(m.bestEval);
                    double Wplayed = cpToWinChance(m.playedEval);
                    double deltaW = std::abs(Wbest - Wplayed);
                    double weight = std::log(m.legalMoves + 1.0);
                    sumDeltaWeighted += deltaW * weight;
                    sumWeights += weight;
                }
                if (sumWeights > 0.0) {
                    double avgDeltaW = sumDeltaWeighted / sumWeights;
                    double loss_index = avgDeltaW * 100.0;
                    total_loss_index += loss_index;
                    loss_index_games++;
                    loss_index_per_game.push_back(loss_index);
                }
            }
        }
    }

    // Psychological aggregates
    for (auto& [date, vec] : games_by_day) {
        std::sort(vec.begin(), vec.end(), [](const GameInfo& a, const GameInfo& b) { return a.end_time < b.end_time; });
        for (auto& g : vec) all_games_vec.push_back(g);
    }
    std::sort(all_games_vec.begin(), all_games_vec.end(), [](const GameInfo& a, const GameInfo& b) { return a.end_time < b.end_time; });
    for (size_t i = 1; i < all_games_vec.size(); ++i) {
        bool prev_loss = false;
        if (all_games_vec[i-1].color == "white" && all_games_vec[i-1].result == "0-1") prev_loss = true;
        if (all_games_vec[i-1].color == "black" && all_games_vec[i-1].result == "1-0") prev_loss = true;
        if (prev_loss) { tilt_games++; if (all_games_vec[i].is_win) tilt_wins++; }
    }
    for (auto& [date, vec] : games_by_day) {
        std::sort(vec.begin(), vec.end(), [](const GameInfo& a, const GameInfo& b) { return a.end_time < b.end_time; });
        for (size_t idx = 0; idx < vec.size(); ++idx) {
            if (idx < 3) { early_session_games++; if (vec[idx].is_win) early_session_wins++; }
            else if (idx >= 10) { late_session_games++; if (vec[idx].is_win) late_session_wins++; }
        }
    }

    // JSON output (same as before)
    stats["total_games"] = total_games;
    stats["wins"] = wins; stats["losses"] = losses; stats["draws"] = draws;
    stats["win_white"] = win_white; stats["win_black"] = win_black;
    stats["avg_opponent_rating"] = rating_count > 0 ? (double)rating_sum / rating_count : 0;

    std::map<std::string, std::vector<int>> month_ratings;
    for (auto& [ts, elo] : rating_history) {
        if (ts == 0) continue;
        time_t t = ts; struct tm* utc = gmtime(&t);
        char buf[8]; strftime(buf, sizeof(buf), "%Y-%m", utc);
        month_ratings[buf].push_back(elo);
    }
    json rating_progress = json::array();
    for (auto& [m, elos] : month_ratings) {
        double avg = std::accumulate(elos.begin(), elos.end(), 0.0) / elos.size();
        rating_progress.push_back({{"month", m}, {"rating", avg}});
    }
    stats["rating_progress"] = rating_progress;

    int total_piece_moves = 0;
    for (auto& [_, c] : piece_moves) total_piece_moves += c;
    json piece_percent;
    for (auto& [p, c] : piece_moves) piece_percent[std::string(1, p)] = total_piece_moves > 0 ? c * 100.0 / total_piece_moves : 0;
    stats["piece_moves_percent"] = piece_percent;
    stats["captures_by_piece"] = json(captures_by_piece);
    stats["castles_kingside"] = castles_k; stats["castles_queenside"] = castles_q;
    stats["promotions"] = promotions; stats["promotion_to"] = json(promo_to);
    stats["checks_given"] = checks_given;
    stats["brilliant"] = brilliant; stats["great"] = great; stats["best"] = best;
    stats["mistakes"] = mistakes; stats["blunders"] = blunders;
    stats["acpl"] = acpl_cnt > 0 ? sum_acpl / acpl_cnt : 0;
    json acpl_phases;
    for (int i = 0; i < 3; i++) acpl_phases.push_back(phase_cnt[i] > 0 ? sum_acpl_phase[i] / phase_cnt[i] : 0);
    stats["acpl_by_phase"] = acpl_phases;

    json hour_stats;
    for (auto& [h, cnt] : hour_games) {
        double wr = hour_wins[h] * 100.0 / cnt;
        hour_stats[std::to_string(h)] = {{"games", cnt}, {"winrate", wr}};
    }
    stats["hour_stats"] = hour_stats;
    json dow_stats;
    const std::string dow_names[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    for (int d = 0; d < 7; d++) {
        int cnt = dow_games[d];
        double wr = cnt > 0 ? dow_wins[d] * 100.0 / cnt : 0;
        dow_stats[dow_names[d]] = {{"games", cnt}, {"winrate", wr}};
    }
    stats["day_of_week_stats"] = dow_stats;

    json openings = json::array();
    for (auto& [eco, cnt] : eco_count) {
        double wr = eco_win[eco] * 100.0 / cnt;
        openings.push_back({{"eco", eco}, {"count", cnt}, {"winrate", wr}});
    }
    std::sort(openings.begin(), openings.end(), [](const json& a, const json& b) { return a["count"].get<int>() > b["count"].get<int>(); });
    stats["openings"] = openings;

    json rivals_arr = json::array();
    for (auto& [name, p] : rivals) {
        double wr = p.second * 100.0 / p.first;
        rivals_arr.push_back({{"opponent", name}, {"games", p.first}, {"winrate", wr}});
    }
    std::sort(rivals_arr.begin(), rivals_arr.end(), [](const json& a, const json& b) { return a["games"].get<int>() > b["games"].get<int>(); });
    stats["top_rivals"] = rivals_arr;

    stats["longest_win_streak"] = max_win; stats["longest_loss_streak"] = max_loss;
    stats["loss_timeout"] = loss_timeout; stats["loss_checkmate"] = loss_mate; stats["loss_resign"] = loss_resign;
    json mp; for (auto& [p, c] : mating_piece) mp[std::string(1, p)] = c; stats["mating_piece"] = mp;
    if (!first_mistake_moves.empty()) {
        double avg = std::accumulate(first_mistake_moves.begin(), first_mistake_moves.end(), 0.0) / first_mistake_moves.size();
        stats["avg_first_mistake_move"] = avg;
    } else stats["avg_first_mistake_move"] = 0;
    stats["endgame_percentage"] = total_games > 0 ? endgame_games * 100.0 / total_games : 0;
    stats["endgame_winrate"] = endgame_games > 0 ? endgame_wins * 100.0 / endgame_games : 0;
    stats["gambit_winrate"] = gambit_games > 0 ? gambit_wins * 100.0 / gambit_games : 0;
    stats["early_mates"] = early_mates;
    stats["saved_draws"] = saved_draws;
    stats["vs_stronger_winrate"] = vs_stronger > 0 ? win_vs_stronger * 100.0 / vs_stronger : 0;
    stats["vs_weaker_winrate"] = vs_weaker > 0 ? win_vs_weaker * 100.0 / vs_weaker : 0;
    stats["tilt_winrate"] = tilt_games > 0 ? tilt_wins * 100.0 / tilt_games : 0;
    stats["tilt_games"] = tilt_games;
    stats["early_session_winrate"] = early_session_games > 0 ? early_session_wins * 100.0 / early_session_games : 0;
    stats["late_session_winrate"] = late_session_games > 0 ? late_session_wins * 100.0 / late_session_games : 0;
    stats["blunder_in_winning_rate"] = winning_positions > 0 ? (double)blunder_in_winning * 100.0 / winning_positions : 0;
    stats["aggressive_opponent_winrate"] = aggressive_opponent_games > 0 ? (double)aggressive_opponent_wins * 100.0 / aggressive_opponent_games : 0;
    stats["loss_index_avg"] = loss_index_games > 0 ? total_loss_index / loss_index_games : 0.0;
    stats["loss_index_per_game"] = loss_index_per_game;

    return stats.dump();
}

PYBIND11_MODULE(stats_cpp, m) {
    m.doc() = "Chess stats engine";
    m.def("compute", &compute, "Compute all stats", py::arg("cache_json"), py::arg("username"));
}
