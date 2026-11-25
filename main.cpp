#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

constexpr int MAX_TURN = 200;         // maximum turn (days)
constexpr int START_GOLD = 500;       // initial gold
constexpr int START_WARRIORS = 3;     // initial warriors
constexpr int MOVE_COST = 10;         // move cost
constexpr int TRAIN_COST = 120;       // train cost
constexpr int WORK_INCOME = 15;       // income per warrior
constexpr int UPKEEP_PER_WARRIOR = 2; // upkeep per warrior
constexpr int HQ_MAX_LEVEL = 5;       // HQ max level
constexpr int BASE_MAX_LEVEL = 3;     // base max level
constexpr int HQ_HEAL_COST = 1000;    // HQ fix cost
constexpr int BASE_HEAL_COST = 500;   // base fix cost

struct HqLevelEntry {
  int upgrade_cost;
  int warrior_hp;
  int hp;
  int turret;
  int train_cap;
  int work_cap;
};

struct BaseLevelEntry {
  int cost;
  int hp;
  int turret;
  int work_cap;
};

constexpr HqLevelEntry HQ_LEVELS[HQ_MAX_LEVEL + 1] = {
    {0, 0, 0, 0, 0, 0},     {0, 4, 10, 1, 1, 1},    {600, 5, 15, 2, 1, 2},
    {1200, 6, 20, 2, 2, 3}, {2400, 7, 25, 3, 2, 4}, {3600, 8, 30, 3, 3, 5},
};
constexpr BaseLevelEntry BASE_LEVELS[BASE_MAX_LEVEL + 1] = {
    {0, 0, 0, 0},
    {300, 6, 1, 1},
    {600, 12, 1, 2},
    {1000, 18, 2, 3},
};

enum class Side : int { LEFT = 0, RIGHT = 1 };
enum class BType : int { HQ, BASE };
enum class WState : int { STATIONARY, MOVING };

inline Side opposite(Side s) {
  return s == Side::LEFT ? Side::RIGHT : Side::LEFT;
}
inline char side_char(Side s) { return s == Side::LEFT ? 'A' : 'B'; }
inline Side parse_side_char(char c) {
  return c == 'A' ? Side::LEFT : Side::RIGHT;
}

struct WarriorId {
  Side side = Side::LEFT;
  int num = 0;
  bool operator==(const WarriorId &o) const {
    return side == o.side && num == o.num;
  }
};

struct Warrior {
  WarriorId id;
  int region = 0;
  int hp = 0;
  WState state = WState::STATIONARY;
  int target = 0;
};

struct Building {
  int region = 0;
  Side side = Side::LEFT;
  BType type = BType::HQ;
  int level = 1;
  int hp = 10;

  int current_hp() const {
    return type == BType::HQ ? HQ_LEVELS[level].hp : BASE_LEVELS[level].hp;
  }
  int work_cap() const {
    return type == BType::HQ ? HQ_LEVELS[level].work_cap
                             : BASE_LEVELS[level].work_cap;
  }
};

struct GameMap {
  int N = 0, K = 0;
  std::vector<long long> x, y;
  std::vector<int> strongholds;
  std::vector<std::vector<int>> adj;

  Side my_side = Side::LEFT;
  int my_hq = 0;
  int opp_hq = 0;
};

struct GameState {
  int gold = START_GOLD; // current gold
  int my_countdown = 5;  // my remaining countdowns
  int opp_countdown = 5; // opponent's remaining countdowns
  std::vector<Warrior> warriors;
  std::vector<Building> buildings;
};

struct Actions {
  int train_n = 0;
  std::vector<std::pair<WarriorId, int>> moves;
  std::vector<int> upgrades;
};

static std::string readln() {
  std::string s;
  if (!std::getline(std::cin, s))
    std::exit(0);
  return s;
}

static std::vector<std::string> tokens(const std::string &s) {
  std::vector<std::string> out;
  std::istringstream is(s);
  for (std::string t; is >> t;)
    out.push_back(t);
  return out;
}

static WarriorId parse_warrior(const std::string &tok) {
  assert(!tok.empty() && (tok[0] == 'A' || tok[0] == 'B'));
  WarriorId id;
  id.side = parse_side_char(tok[0]);
  id.num = std::stoi(tok.substr(1));
  return id;
}

static std::string format_warrior(WarriorId id) {
  std::string s;
  s.push_back(side_char(id.side));
  s += std::to_string(id.num);
  return s;
}

static int hq_of(const GameMap &M, Side s) {
  return (s == Side::LEFT) ? 0 : M.N - 1;
}

static Building make_base(int region, Side s) {
  return Building{region, s, BType::BASE, 1, BASE_LEVELS[1].hp};
}

static void apply_upgrade(Building &b) {
  b.level += 1;
  b.hp = b.current_hp();
}

static int upgrade_cost(const Building &b) {
  if (b.type == BType::HQ)
    return HQ_LEVELS[b.level + 1].upgrade_cost;
  else
    return BASE_LEVELS[b.level + 1].cost;
}

static int max_level(const Building &b) {
  return b.type == BType::HQ ? HQ_MAX_LEVEL : BASE_MAX_LEVEL;
}

static void parse_init(GameMap &M, GameState &S) {
  {
    auto t = tokens(readln());
    assert(t.size() >= 2 && t[0] == "READY");
    M.my_side = (t[1] == "LEFT") ? Side::LEFT : Side::RIGHT;
  }
  {
    auto t = tokens(readln());
    M.N = std::stoi(t.at(0));
    M.K = std::stoi(t.at(1));
  }
  M.x.assign(M.N, 0);
  M.y.assign(M.N, 0);
  {
    auto t = tokens(readln()); // x_0 x_1 ... x_{N-1}
    for (int i = 0; i < M.N; ++i)
      M.x[i] = std::stoll(t.at(i));
  }
  {
    auto t = tokens(readln()); // y_0 y_1 ... y_{N-1}
    for (int i = 0; i < M.N; ++i)
      M.y[i] = std::stoll(t.at(i));
  }
  {
    auto t = tokens(readln()); // K strongholds
    M.strongholds.clear();
    M.strongholds.reserve(t.size());
    for (const auto &s : t)
      M.strongholds.push_back(std::stoi(s));
    std::sort(M.strongholds.begin(), M.strongholds.end());
  }
  M.adj.assign(M.N, {});
  for (int r = 0; r < M.N; ++r) {
    auto t = tokens(readln()); // deg n_1 n_2 ...
    int deg = std::stoi(t.at(0));
    auto &nb = M.adj[r];
    nb.reserve(deg);
    for (int j = 0; j < deg; ++j)
      nb.push_back(std::stoi(t.at(1 + j)));
    std::sort(nb.begin(), nb.end());
  }

  M.my_hq = hq_of(M, M.my_side);
  M.opp_hq = hq_of(M, opposite(M.my_side));

  S = GameState{};
  S.gold = START_GOLD;
  Side opp = opposite(M.my_side);
  for (int sfx = 1; sfx <= START_WARRIORS; ++sfx) {
    S.warriors.push_back(Warrior{.id = WarriorId{M.my_side, sfx},
                                 .region = M.my_hq,
                                 .hp = HQ_LEVELS[1].warrior_hp});
    S.warriors.push_back(Warrior{.id = WarriorId{opp, sfx},
                                 .region = M.opp_hq,
                                 .hp = HQ_LEVELS[1].warrior_hp});
  }
  S.buildings.push_back(Building{hq_of(M, Side::LEFT), Side::LEFT, BType::HQ, 1,
                                 HQ_LEVELS[1].hp});
  S.buildings.push_back(Building{hq_of(M, Side::RIGHT), Side::RIGHT, BType::HQ,
                                 1, HQ_LEVELS[1].hp});

  std::cout << "OK" << std::endl;
}

static bool read_turn_start(int &turn_index) {
  std::string line = readln();
  if (line == "FINISH")
    return false;
  auto t = tokens(line);
  assert(!t.empty() && t[0] == "START");
  turn_index = std::stoi(t.at(2));
  return true;
}

static Building *find_building(GameState &S, int region) {
  for (auto &b : S.buildings)
    if (b.region == region)
      return &b;
  return nullptr;
}

static Warrior *find_warrior(GameState &S, WarriorId id) {
  for (auto &w : S.warriors)
    if (w.id == id)
      return &w;
  return nullptr;
}

static void read_turn_result(GameState &S, const GameMap &M,
                             const Actions &submitted) {
  for (int region : submitted.upgrades) {
    Building *b = find_building(S, region);
    if (b == nullptr) {
      S.gold -= BASE_LEVELS[1].cost;
      S.buildings.push_back(make_base(region, M.my_side));
    } else {
      if (b->level >= max_level(*b)) {
        int cost = (b->type == BType::HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
        S.gold -= cost;
        b->hp = b->current_hp();
      } else {
        S.gold -= upgrade_cost(*b);
        apply_upgrade(*b);
      }
    }
  }

  for (const auto &[id, target] : submitted.moves) {
    Building *b = find_building(S, target);
    int cost = (b != nullptr && b->side == M.my_side) ? 0 : MOVE_COST;
    S.gold -= cost;
    if (Warrior *w = find_warrior(S, id)) {
      w->state = WState::MOVING;
      w->target = target;
    }
  }

  S.gold -= TRAIN_COST * submitted.train_n;

  {
    std::string line = readln();
    if (line == "FINISH")
      std::exit(0);
    auto t = tokens(line);
    assert(!t.empty() && t[0] == "TURN");
  }
  {
    auto t = tokens(readln());
    S.my_countdown = std::stoi(t.at(2));
    S.opp_countdown = std::stoi(t.at(4));
  }
  // UPGRADE
  {
    auto t = tokens(readln()); // "UPGRADE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln()); // "<A|B> <region>"
      Side s = parse_side_char(r.at(0)[0]);
      int region = std::stoi(r.at(1));
      Building *b = find_building(S, region);
      if (b == nullptr) {
        S.buildings.push_back(make_base(region, s));
      } else if (b->side != M.my_side) {
        if (b->level >= max_level(*b)) {
          b->hp = b->current_hp();
        } else {
          apply_upgrade(*b);
        }
      }
    }
  }
  // TRAIN
  {
    auto t = tokens(readln()); // "TRAIN N"
    int n = std::stoi(t.at(1));
    if (n > 0) {
      auto ids = tokens(readln());
      for (int i = 0; i < n; ++i) {
        WarriorId id = parse_warrior(ids.at(i));
        int hq_region = hq_of(M, id.side);
        Building *hq_b = find_building(S, hq_region);
        int hq_level = (hq_b != nullptr) ? hq_b->level : 1;
        S.warriors.push_back(Warrior{.id = id,
                                     .region = hq_region,
                                     .hp = HQ_LEVELS[hq_level].warrior_hp});
      }
    }
  }
  // MOVE
  {
    auto t = tokens(readln()); // "MOVE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      WarriorId id = parse_warrior(r.at(0));
      int region = std::stoi(r.at(1));
      if (Warrior *w = find_warrior(S, id)) {
        w->region = region;
        if (id.side == M.my_side && w->state == WState::MOVING &&
            w->region == w->target) {
          w->state = WState::STATIONARY;
        }
      }
    }
  }
  // DAMAGE
  {
    auto t = tokens(readln()); // "DAMAGE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      WarriorId id = parse_warrior(r.at(1));
      int damage = std::stoi(r.at(2));
      if (Warrior *w = find_warrior(S, id))
        w->hp -= damage;
    }
    S.warriors.erase(std::remove_if(S.warriors.begin(), S.warriors.end(),
                                    [](const Warrior &w) { return w.hp <= 0; }),
                     S.warriors.end());
  }
  // SIEGE
  {
    auto t = tokens(readln()); // "SIEGE N"
    int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln());
      int region = std::stoi(r.at(1));
      int damage = std::stoi(r.at(2));
      if (Building *b = find_building(S, region))
        b->hp -= damage;
    }
    S.buildings.erase(
        std::remove_if(S.buildings.begin(), S.buildings.end(),
                       [](const Building &b) { return b.hp <= 0; }),
        S.buildings.end());
  }
  (void)readln(); // "END"

  int income = 0;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side)
      continue;
    int count = 0;
    for (const auto &w : S.warriors) {
      if (w.id.side == M.my_side && w.region == b.region)
        ++count;
    }
    income += WORK_INCOME * std::min(count, b.work_cap());
  }
  S.gold += income;

  int alive = 0;
  for (const auto &w : S.warriors)
    if (w.id.side == M.my_side)
      ++alive;
  S.gold = std::max(0, S.gold - UPKEEP_PER_WARRIOR * alive);
}

struct Paths {
  std::vector<std::vector<double>> dist;
  std::vector<std::vector<int>> nxt;
};

static double euclid_ceil(const GameMap &M, int u, int v) {
  double dx = (double)(M.x[u] - M.x[v]);
  double dy = (double)(M.y[u] - M.y[v]);
  return std::ceil(std::sqrt(dx * dx + dy * dy));
}

static Paths calculate_paths(const GameMap &M) {
  const double INF = std::numeric_limits<double>::infinity();
  Paths P;
  P.dist.assign(M.N, std::vector<double>(M.N, INF));
  P.nxt.assign(M.N, std::vector<int>(M.N, -1));

  for (int i = 0; i < M.N; ++i) {
    P.dist[i][i] = 0.0;
    P.nxt[i][i] = i;
  }
  for (int u = 0; u < M.N; ++u) {
    for (int v : M.adj[u]) {
      double w = euclid_ceil(M, u, v);
      if (w < P.dist[u][v])
        P.dist[u][v] = w;
    }
  }

  for (int k = 0; k < M.N; ++k) {
    for (int u = 0; u < M.N; ++u) {
      if (P.dist[u][k] == INF)
        continue;
      for (int v = 0; v < M.N; ++v) {
        double cand = P.dist[u][k] + P.dist[k][v];
        if (cand < P.dist[u][v])
          P.dist[u][v] = cand;
      }
    }
  }

  for (int u = 0; u < M.N; ++u) {
    for (int v = 0; v < M.N; ++v) {
      if (u == v || P.dist[u][v] == INF)
        continue;
      double best_score = INF;
      for (int nb : M.adj[u]) {
        if (P.dist[nb][v] == INF)
          continue;
        double score = euclid_ceil(M, u, nb) + P.dist[nb][v];
        if (score < best_score) {
          best_score = score;
          P.nxt[u][v] = nb;
        }
      }
    }
  }
  return P;
}

// Returns the next step on the path from u to v.
// If the path is not reachable, returns -1.
static int next_step(const Paths &P, int u, int v) { return P.nxt[u][v]; }

// Returns the path from u to v.
// If the path is not reachable, returns an empty vector.
static std::vector<int> path(const Paths &P, int u, int v) {
  std::vector<int> out;
  if (P.nxt[u][v] == -1)
    return out;
  out.push_back(u);
  while (u != v) {
    u = P.nxt[u][v];
    out.push_back(u);
  }
  return out;
}

static void emit_command() { std::cout << "COMMAND\n"; }

static void emit_actions(const Actions &a) {
  for (const auto &[id, target] : a.moves) {
    std::cout << "MOVE " << format_warrior(id) << ' ' << target << '\n';
  }
  for (int r : a.upgrades) {
    std::cout << "UPGRADE " << r << '\n';
  }
  if (a.train_n > 0) {
    std::cout << "TRAIN " << a.train_n << '\n';
  }
}

static void emit_end() { std::cout << "END" << std::endl; }

//////////////////////////////////
//// WRITE YOUR STRATEGY HERE ////
//////////////////////////////////

struct RegionInfo;
struct MyStatus;
struct EnemyStatus;
struct Plan;


struct RegionInfo {
  int region = -1;
  bool is_stronghold = false;
  bool has_my_building = false;
  bool has_enemy_building = false;
  int my_warriors = 0;
  int enemy_warriors = 0;
  double dist_my_hq = 0.0;
  double dist_opp_hq = 0.0;
};

struct MyStatus {
  int gold = 0;
  int warriors = 0;
  int bases = 0;
  int hq_level = 1;
  int train_cap = 1;
  int work_cap = 0;
  int workers = 0;
  int idle_warriors = 0;
};

struct EnemyStatus {
  int warriors = 0;
  int bases = 0;
  int hq_level = 1;
  int train_cap = 1;
};

struct Plan {
  int desired_bases = 1;
  int desired_warriors = START_WARRIORS;
  int rally = -1;
};

struct ActionContext {
  Actions a;
  int budget = 0;
  std::vector<bool> used_warrior;
  std::vector<bool> used_upgrade_region;
  std::vector<int> planned_arrive;
  std::vector<int> planned_leave;

  ActionContext(const GameState &S, const GameMap &M) {
    budget = S.gold;
    used_warrior.assign(S.warriors.size(), false);
    used_upgrade_region.assign(M.N, false);
    planned_arrive.assign(M.N, 0);
    planned_leave.assign(M.N, 0);
  }
};

//forward declaration

// Forward declarations for strategy helpers

static const Building *find_building(const GameState &S, int region);

static bool is_stronghold(const GameMap &M, int region);
static bool has_building(const GameState &S, int region);
static bool has_my_building(const GameState &S, const GameMap &M, int region);

static int count_warriors_at(const GameState &S, Side side, int region);
static int count_stationary_warriors_at(const GameState &S, Side side, int region);
static int count_warriors_side(const GameState &S, Side side);
static int count_bases_side(const GameState &S, Side side);
static int hq_level_side(const GameState &S, const GameMap &M, Side side);
static int train_cap_side(const GameState &S, const GameMap &M, Side side);
static int total_work_cap_side(const GameState &S, Side side);
static int current_workers_side(const GameState &S, Side side);
static int count_idle_warriors(const GameState &S, Side side);

static int planned_my_workers_at(const GameState &S, const GameMap &M, const ActionContext &ctx, int region);
static int worker_shortage_at(const GameState &S, const GameMap &M, const ActionContext &ctx, const Building &b);

static bool is_unbuilt_stronghold(const GameState &S, const GameMap &M, int region);
static bool can_take_warrior_from_region(const GameState &S, const GameMap &M, const ActionContext &ctx, int region);
static bool can_use_warrior_for_defense(const GameState &S, const GameMap &M, const ActionContext &ctx, int source_region, int target_region);

static MyStatus analyze_my_status(const GameState &S, const GameMap &M);
static EnemyStatus analyze_enemy_status(const GameState &S, const GameMap &M);

static int desired_base_count_by_economy(const GameMap &M, const MyStatus &my);
static int choose_initial_rally(const GameMap &M, const Paths &P);
static Plan make_plan(const GameState &S, const GameMap &M, const Paths &P, int turn);

static int level_upgrade_cost_only(const Building &b);
static bool has_base_builder_or_candidate(const GameState &S, const GameMap &M, const ActionContext &ctx);
static int reserve_gold_for_base_construction(const GameState &S, const GameMap &M, const ActionContext &ctx);
static const Building *my_hq_building(const GameState &S, const GameMap &M);

static const Building *choose_saturated_base_to_upgrade(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx);
static const Building *choose_base_to_upgrade(const GameState &S, const GameMap &M, const Paths &P);

static bool add_upgrade(ActionContext &ctx, int region, int cost);
static bool has_current_stationary_warrior_for_upgrade(const GameState &S, const GameMap &M, const ActionContext &ctx, int region);
static bool try_upgrade_building(const GameState &S, const GameMap &M, ActionContext &ctx, const Building &b, int reserve);

static int count_pending_base_sites(const GameState &S, const GameMap &M, const ActionContext &ctx);
static bool should_hard_reserve_for_expansion(const GameState &S, const GameMap &M, const MyStatus &my, const ActionContext &ctx);

static void upgrade_economy(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx);

static bool add_move(ActionContext &ctx, const GameState &S, const GameMap &M, int warrior_index, int target);
static bool add_train(ActionContext &ctx, int n);
static int remaining_train_cap(const MyStatus &my, const ActionContext &ctx);

static void build_bases_under_workers(const GameState &S, const GameMap &M, ActionContext &ctx);

static int path_hops(const Paths &P, int from, int to);
static int my_reinforcements_to_region(const GameState &S, const GameMap &M, int region);
static int enemy_threat_near_region(const GameState &S, const GameMap &M, const Paths &P, int region);
static int count_enemy_within_hops(const GameState &S, const GameMap &M, const Paths &P, int region, int max_hops);
static int enemy_hq_blob_count(const GameState &S, const GameMap &M, const Paths &P);
static int enemy_committed_to_my_hq_count(const GameState &S, const GameMap &M);

static bool enemy_rush_incoming(const GameState &S, const GameMap &M, const Paths &P, int turn);
static bool enemy_rush_preparing(const GameState &S, const GameMap &M, const Paths &P, int turn);
static bool hq_under_attack_now(const GameState &S, const GameMap &M, const Paths &P);

static int choose_primary_defense_target(const GameState &S, const GameMap &M, const Paths &P);
static int defense_shortage_at_building(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, const Building &b);
static void emergency_train_for_defense(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx);
static int find_best_defender_for_region(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int target_region);
static void defend_buildings(const GameState &S, const GameMap &M, const Paths &P, ActionContext &ctx);


static int choose_pressure_target(const GameState &S, const GameMap &M, const Paths &P);
static int find_best_pressure_attacker(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int target, int hq_guard);
static int required_hq_guard(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn);
static void attack_enemy(const GameState &S, const GameMap &M, const Paths &P, const Plan &plan, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx);

static bool already_targeted_region(const GameState &S, const GameMap &M, int region);
static double dist_to_nearest_my_building(const GameState &S, const GameMap &M, const Paths &P, int region);
static double expansion_score(const GameState &S, const GameMap &M, const Paths &P, int region);
static int find_best_expander(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int target);
static void send_expanders(const GameState &S, const GameMap &M, const Paths &P, const Plan &plan, const MyStatus &my, int turn, ActionContext &ctx);

static int enemy_hq_required_attackers_light(const GameState &S, const GameMap &M, const Paths &P);
static bool should_finish_enemy_hq(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn);

static bool enemy_rush_committed_or_close(const GameState &S, const GameMap &M, const Paths &P);
static void defend_hq_for_incoming_rush(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx);

static bool is_finish_front_unit(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int warrior_index, int stage, int hq_guard);
static int count_finish_attack_pool(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int hq_guard);
static int find_finish_attack_pool_unit(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int hq_guard);
static int choose_finish_stage(const GameState &S, const GameMap &M, const Paths &P);

static bool should_siege_enemy_bases(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn);
static int enemy_base_required_attackers_siege(const GameState &S, const GameMap &M, const Paths &P, int target);
static int choose_stage_for_target(const GameState &S, const GameMap &M, const Paths &P, int target);
static bool should_prepare_finish_simple(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn);

static bool can_use_warrior_for_finish(const GameState &S, const GameMap &M, const ActionContext &ctx, int warrior_index, int hq_guard);

static bool is_snowball_attack_pool_unit(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int warrior_index, int stage, int target, int hq_guard);
static int count_snowball_attack_pool(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int target, int hq_guard);
static int find_snowball_attack_pool_unit(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int target, int hq_guard);
static int find_best_snowball_gatherer(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int target, int hq_guard);

static void siege_enemy_base_grouped(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, int hq_guard, int surplus, ActionContext &ctx);

static int finish_launch_gold_reserve(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, const ActionContext &ctx, int turn);
static int siege_launch_gold_reserve(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, const ActionContext &ctx, int turn);
static int operation_gold_reserve(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, const ActionContext &ctx, int turn);

static bool add_train_with_reserve(ActionContext &ctx, int n, int reserve);
static void train_units_with_reserve(const GameState &S, const GameMap &M, const Plan &plan, const MyStatus &my, int turn, ActionContext &ctx, int reserve);

static int early_base_target_count(const GameMap &M);

static int advantage_score(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy);
static bool has_growth_advantage(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy, int turn);
static bool should_growth_snowball(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn);
static bool has_siege_advantage(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn);

static const Building *choose_growth_base_to_upgrade(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx);
static void upgrade_for_growth_snowball(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx);

static bool should_prepare_finish_simple(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn);
static bool should_lock_attack_for_expansion(const GameState &S, const GameMap &M, const MyStatus &my, const ActionContext &ctx, int turn);
static int expansion_gold_reserve(const GameState &S, const GameMap &M, const MyStatus &my, const ActionContext &ctx, int turn);

static int base_upgrade_power_side(const GameState &S, Side side);
static int economy_power_side(const GameState &S, const GameMap &M, Side side);
static bool has_growth_upgrade_debt(const GameState &S, const GameMap &M, const MyStatus &my);
static bool enemy_is_scaling_or_not_behind(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy);
static int desired_army_lead_for_growth(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy);
static void train_to_keep_army_lead(const GameState &S, const GameMap &M, const Plan &plan, const MyStatus &my, const EnemyStatus &enemy, ActionContext &ctx, int reserve);

static int next_core_growth_upgrade_cost(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const ActionContext &ctx);
static bool has_core_upgrade_debt(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const ActionContext &ctx);
static bool should_gold_growth_mode(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx);
static bool should_opening_pressure_mode(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx);
static int opening_pressure_train_target(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy);
static void train_for_opening_pressure(const GameState &S, const GameMap &M, const Plan &plan, const MyStatus &my, const EnemyStatus &enemy, ActionContext &ctx, int reserve);

static int moving_enemy_targeting_region_count(const GameState &S, const GameMap &M, int region);
static int moving_enemy_threat_near_region(const GameState &S, const GameMap &M, const Paths &P, int region);
static int moving_enemy_within_hops(const GameState &S, const GameMap &M, const Paths &P, int region, int max_hops);

static int owned_or_pending_bases(const GameState &S, const GameMap &M, const MyStatus &my, const ActionContext &ctx);
static int economy_income_power_side(const GameState &S, Side side);
static bool should_allow_growth_upgrade(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx);



static bool is_fast_safe_expansion_candidate(const GameState &S, const GameMap &M, const Paths &P, int region);
static bool has_fast_expansion_opportunity(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx);
static int count_fast_expansion_opportunities(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx);

static int opening_expansion_pending_limit(const GameState &S, const GameMap &M, const MyStatus &my, const ActionContext &ctx);
static bool should_opening_expansion_phase(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx);
static int opening_expansion_train_target(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, const ActionContext &ctx);
static void train_for_opening_expansion(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx);
static void do_opening_expansion(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx);
static void do_opening_pressure(const GameState &S, const GameMap &M, const Paths &P, const Plan &plan, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx);
static void do_gold_snowball(const GameState &S, const GameMap &M, const Paths &P, const Plan &plan, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx);
static bool has_any_action(const ActionContext &ctx);

static bool has_ready_base_builder(const GameState &S, const GameMap &M, const ActionContext &ctx);
static bool should_end_opening_pressure(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx);

static bool is_enemy_side_region(const GameMap &M, const Paths &P, int region);
static int count_pressure_units(const GameState &S, const GameMap &M, const Paths &P);
static int pressure_unit_limit(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy);
static bool pressure_budget_left(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy);

static bool is_fast_safe_expansion_candidate(const GameState &S, const GameMap &M, const Paths &P, int region);
static bool has_fast_expansion_opportunity(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx);
static int count_fast_expansion_opportunities(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx);
static bool should_prioritize_expansion(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx);
static bool has_home_expansion_candidate(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx);

static bool should_late_spend_mode(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx);
static void do_late_spend(const GameState &S, const GameMap &M, const Paths &P, const Plan &plan, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx);

static bool claim_pressure_gain(const GameState &S, const GameMap &M, const Paths &P, ActionContext &ctx);
static int choose_claimable_pressure_site(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx);
static int find_best_claim_unit(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int target);

static int enemy_final_hq_rush_count(const GameState &S, const GameMap &M, const Paths &P);
static int enemy_final_hq_rush_eta(const GameState &S, const GameMap &M, const Paths &P);
static int my_hq_required_defenders_light(const GameState &S, const GameMap &M, const Paths &P);
static bool enemy_is_final_hq_rushing(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy);
static bool can_win_hq_race_now(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, const ActionContext &ctx, int turn);
static bool launch_final_hq_race(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx);
static void defend_final_hq_race(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx);
static bool handle_final_hq_race(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx);

static bool use_growth_surplus_hq_army(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx);
static int choose_growth_attack_target(const GameState &S, const GameMap &M, const Paths &P);

static bool should_lock_hq_growth_upgrade(const GameState &S, const GameMap &M, const MyStatus &my);
static bool enemy_is_skipping_core_upgrades(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy);

static bool has_decisive_growth_lead(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy);

static void fill_workers(const GameState &S, const GameMap &M, const Paths &P, ActionContext &ctx);
static void train_units(const GameState &S, const GameMap &M, const Plan &plan, const MyStatus &my, int turn, ActionContext &ctx);



static Actions decide(const GameState &S, const GameMap &M, const Paths &P, int turn);

//start

static const Building *find_building(const GameState &S, int region) {
  for (const auto &b : S.buildings) {
    if (b.region == region) {
      return &b;
    }
  }

  return nullptr;
}

static bool is_stronghold(const GameMap &M, int region) {
  return std::binary_search(M.strongholds.begin(), M.strongholds.end(), region);
}

static bool has_building(const GameState &S, int region) {
  return find_building(S, region) != nullptr;
}

static bool has_my_building(const GameState &S, const GameMap &M, int region) {
  const Building *b = find_building(S, region);
  return b != nullptr && b->side == M.my_side;
}

static int count_warriors_at(const GameState &S, Side side, int region) {
  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side == side && w.region == region) {
      ++cnt;
    }
  }

  return cnt;
}

static int count_stationary_warriors_at(const GameState &S, Side side, int region) {
  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side == side && w.region == region && w.state == WState::STATIONARY) {
      ++cnt;
    }
  }

  return cnt;
}

static int planned_my_workers_at(const GameState &S, const GameMap &M, const ActionContext &ctx, int region) {
  int here = count_stationary_warriors_at(S, M.my_side, region);

  // 이미 이전 턴에 이 지역으로 이동 중인 전사도 예약된 worker/수비대로 본다.
  // 기존 코드는 현재 턴의 planned_arrive만 세서, 먼 전초기지로 매 턴 한 명씩 계속 보내는 문제가 있었다.
  for (const auto &w : S.warriors) {
    if (w.id.side == M.my_side && w.state == WState::MOVING && w.target == region) {
      ++here;
    }
  }

  here -= ctx.planned_leave[region];
  here += ctx.planned_arrive[region];
  return here;
}

static int worker_shortage_at(const GameState &S, const GameMap &M, const ActionContext &ctx, const Building &b) {
  if (b.side != M.my_side) {
    return 0;
  }

  if (count_warriors_at(S, opposite(M.my_side), b.region) > 0) {
    return 0;
  }

  int here = planned_my_workers_at(S, M, ctx, b.region);
  return std::max(0, b.work_cap() - here);
}

static bool is_unbuilt_stronghold(const GameState &S, const GameMap &M, int region) {
  return is_stronghold(M, region) && !has_building(S, region);
}

static bool can_take_warrior_from_region(const GameState &S, const GameMap &M, const ActionContext &ctx, int region) {
  if (is_unbuilt_stronghold(S, M, region)) {
    return false;
  }

  const Building *b = find_building(S, region);

  if (b == nullptr || b->side != M.my_side) {
    return true;
  }

  int here = planned_my_workers_at(S, M, ctx, region);
  int required = b->work_cap();

  if (ctx.used_upgrade_region[region]) {
    required = std::max(required, 1);
  }

  return here > required;
}

static bool can_use_warrior_for_defense(const GameState &S, const GameMap &M, const ActionContext &ctx, int source_region, int target_region) {
  if (source_region == target_region) {
    return false;
  }

  if (is_unbuilt_stronghold(S, M, source_region)) {
    return false;
  }

  const Building *b = find_building(S, source_region);

  if (b == nullptr) {
    return true;
  }

  if (b->side != M.my_side) {
    return false;
  }

  int here = planned_my_workers_at(S, M, ctx, source_region);

  if (target_region == M.my_hq) {
    if (b->type == BType::HQ) {
      return here > 2;
    }

    return here > 0;
  }

  if (b->type == BType::HQ) {
    return here > 2;
  }

  return here > 1;
}

static int find_best_worker_for_building(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, const Building &target_building) {
  int best_idx = -1;
  double best_dist = std::numeric_limits<double>::infinity();

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    const Warrior &w = S.warriors[i];

    if (ctx.used_warrior[i]) {
      continue;
    }

    if (w.id.side != M.my_side) {
      continue;
    }

    if (w.state != WState::STATIONARY) {
      continue;
    }

    if (w.region == target_building.region) {
      continue;
    }

    if (!can_take_warrior_from_region(S, M, ctx, w.region)) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), w.region) > 0) {
      continue;
    }

    double d = P.dist[w.region][target_building.region];

    if (d < best_dist) {
      best_dist = d;
      best_idx = i;
    }
  }

  return best_idx;
}



static int count_warriors_side(const GameState &S, Side side) {
  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side == side) {
      ++cnt;
    }
  }

  return cnt;
}

static int count_bases_side(const GameState &S, Side side) {
  int cnt = 0;

  for (const auto &b : S.buildings) {
    if (b.side == side && b.type == BType::BASE) {
      ++cnt;
    }
  }

  return cnt;
}

static int hq_level_side(const GameState &S, const GameMap &M, Side side) {
  const Building *hq = find_building(S, hq_of(M, side));

  if (hq == nullptr) {
    return 1;
  }

  return hq->level;
}

static int train_cap_side(const GameState &S, const GameMap &M, Side side) {
  int level = hq_level_side(S, M, side);
  return HQ_LEVELS[level].train_cap;
}

static int total_work_cap_side(const GameState &S, Side side) {
  int cap = 0;

  for (const auto &b : S.buildings) {
    if (b.side == side) {
      cap += b.work_cap();
    }
  }

  return cap;
}

static int current_workers_side(const GameState &S, Side side) {
  int workers = 0;

  for (const auto &b : S.buildings) {
    if (b.side != side) {
      continue;
    }

    int here = count_warriors_at(S, side, b.region);
    workers += std::min(here, b.work_cap());
  }

  return workers;
}

static int count_idle_warriors(const GameState &S, Side side) {
  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side == side && w.state == WState::STATIONARY) {
      ++cnt;
    }
  }

  return cnt;
}

static MyStatus analyze_my_status(const GameState &S, const GameMap &M) {
  MyStatus st;

  st.gold = S.gold;
  st.warriors = count_warriors_side(S, M.my_side);
  st.bases = count_bases_side(S, M.my_side);
  st.hq_level = hq_level_side(S, M, M.my_side);
  st.train_cap = train_cap_side(S, M, M.my_side);
  st.work_cap = total_work_cap_side(S, M.my_side);
  st.workers = current_workers_side(S, M.my_side);
  st.idle_warriors = count_idle_warriors(S, M.my_side);

  return st;
}

static EnemyStatus analyze_enemy_status(const GameState &S, const GameMap &M) {
  Side enemy = opposite(M.my_side);

  EnemyStatus st;

  st.warriors = count_warriors_side(S, enemy);
  st.bases = count_bases_side(S, enemy);
  st.hq_level = hq_level_side(S, M, enemy);
  st.train_cap = train_cap_side(S, M, enemy);

  return st;
}

static int early_base_target_count(const GameMap &M) {
  int max_reasonable = std::max(1, ((int)M.strongholds.size() + 1) / 2);
  return std::min(4, max_reasonable);
}

static int owned_or_pending_bases(const GameState &S, const GameMap &M, const MyStatus &my, const ActionContext &ctx) {
  return my.bases + count_pending_base_sites(S, M, ctx);
}

static int economy_income_power_side(const GameState &S, Side side) {
  int income = 0;

  for (const auto &b : S.buildings) {
    if (b.side != side) {
      continue;
    }

    int here = count_warriors_at(S, side, b.region);
    income += std::min(here, b.work_cap()) * WORK_INCOME;
  }

  return income;
}

static bool should_prioritize_expansion(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx) {
  (void)enemy;
  (void)turn;

  int pending = count_pending_base_sites(S, M, ctx);
  int desired = desired_base_count_by_economy(M, my);
  int owned_or_pending = my.bases + pending;

  bool has_candidate = has_home_expansion_candidate(S, M, P, ctx) ||
                       has_fast_expansion_opportunity(S, M, P, ctx);

  bool has_builder_waiting = has_ready_base_builder(S, M, ctx);
  bool still_expanding = owned_or_pending < desired && has_candidate;
  bool economy_not_filled = my.workers < std::max(0, my.work_cap - 1);
  bool army_not_free = my.warriors <= my.work_cap + pending + 2;

  if (has_builder_waiting) {
    return true;
  }

  if (pending > 0 && army_not_free) {
    return true;
  }

  if (still_expanding && economy_not_filled) {
    return true;
  }

  if (still_expanding && army_not_free) {
    return true;
  }

  return false;
}

static bool should_lock_attack_for_expansion(const GameState &S, const GameMap &M, const MyStatus &my, const ActionContext &ctx, int turn) {
  (void)turn;

  int pending = count_pending_base_sites(S, M, ctx);
  int owned_or_pending = my.bases + pending;

  bool has_expansion_room = owned_or_pending < desired_base_count_by_economy(M, my);
  bool has_builder_waiting = has_ready_base_builder(S, M, ctx);
  bool needs_workers = my.workers < my.work_cap;
  bool army_not_free = my.warriors <= my.work_cap + pending + 2;

  if (pending > 0 && army_not_free) {
    return true;
  }

  if (has_builder_waiting) {
    return true;
  }

  if (has_expansion_room && needs_workers) {
    return true;
  }

  if (has_expansion_room && army_not_free) {
    return true;
  }

  return false;
}

static int expansion_gold_reserve(const GameState &S, const GameMap &M, const MyStatus &my, const ActionContext &ctx, int turn) {
  int target = early_base_target_count(M);
  int pending = count_pending_base_sites(S, M, ctx);
  int owned_or_pending = my.bases + pending;

  if (turn < 80 && owned_or_pending < target) {
    return BASE_LEVELS[1].cost;
  }

  if (owned_or_pending < target && my.bases < 3) {
    return BASE_LEVELS[1].cost;
  }

  return reserve_gold_for_base_construction(S, M, ctx);
}

static int base_upgrade_power_side(const GameState &S, Side side) {
  int score = 0;

  for (const auto &b : S.buildings) {
    if (b.side != side) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    score += b.level * 4;
    score += b.work_cap() * 2;
    score += BASE_LEVELS[b.level].turret * 3;
  }

  return score;
}

static int economy_power_side(const GameState &S, const GameMap &M, Side side) {
  int hq_level = hq_level_side(S, M, side);
  int bases = count_bases_side(S, side);
  int workers = current_workers_side(S, side);
  int work_cap = total_work_cap_side(S, side);
  int base_power = base_upgrade_power_side(S, side);

  int score = 0;
  score += bases * 6;
  score += base_power * 2;
  score += workers * 3;
  score += work_cap * 2;
  score += hq_level * 5;

  return score;
}

static int advantage_score(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy) {
  int enemy_workers = current_workers_side(S, opposite(M.my_side));

  int my_economy = economy_power_side(S, M, M.my_side);
  int enemy_economy = economy_power_side(S, M, opposite(M.my_side));

  int army_adv = my.warriors - enemy.warriors;
  int worker_adv = my.workers - enemy_workers;
  int economy_adv = my_economy - enemy_economy;
  int hq_adv = my.hq_level - enemy.hq_level;

  int score = 0;
  score += economy_adv;
  score += worker_adv * 2;
  score += army_adv;
  score += hq_adv * 4;

  if (my.work_cap > 0 && my.workers >= my.work_cap - 1) {
    score += 3;
  }

  return score;
}

static bool has_growth_upgrade_debt(const GameState &S, const GameMap &M, const MyStatus &my) {
  const Building *hq = my_hq_building(S, M);

  if (hq != nullptr) {
    if (hq->level < 3 && my.bases >= 2) {
      return true;
    }

    if (hq->level < 4 && my.bases >= 4 && my.workers >= my.work_cap - 1) {
      return true;
    }
  }

  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    if (b.level >= BASE_MAX_LEVEL) {
      continue;
    }

    int here = count_warriors_at(S, M.my_side, b.region);

    if (b.level == 1) {
      return true;
    }

    if (here >= b.work_cap()) {
      return true;
    }
  }

  return false;
}

static int next_core_growth_upgrade_cost(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const ActionContext &ctx) {
  const Building *hq = my_hq_building(S, M);

  if (hq != nullptr) {
    if (hq->level == 1 && my.bases >= 2) {
      return HQ_LEVELS[2].upgrade_cost;
    }

    if (hq->level == 2 && my.bases >= 3 && my.workers >= std::max(0, my.work_cap - 1)) {
      return HQ_LEVELS[3].upgrade_cost;
    }

    if (hq->level == 3 && my.bases >= 4 && my.workers >= std::max(0, my.work_cap - 1)) {
      return HQ_LEVELS[4].upgrade_cost;
    }
  }

  const Building *base = choose_growth_base_to_upgrade(S, M, P, ctx);

  if (base != nullptr) {
    return level_upgrade_cost_only(*base);
  }

  return 0;
}

static bool has_core_upgrade_debt(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const ActionContext &ctx) {
  int cost = next_core_growth_upgrade_cost(S, M, P, my, ctx);

  if (cost <= 0) {
    return false;
  }

  if (my.gold < cost + TRAIN_COST) {
    return true;
  }

  return false;
}

static bool is_home_side_stronghold(const GameState &S, const GameMap &M, const Paths &P, int region) {
  (void)S;

  double my_dist = P.dist[M.my_hq][region];
  double enemy_dist = P.dist[M.opp_hq][region];

  return my_dist <= enemy_dist * 1.05;
}

static bool is_home_expansion_candidate(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int region) {
  if (!is_unbuilt_stronghold(S, M, region)) {
    return false;
  }

  if (!is_home_side_stronghold(S, M, P, region)) {
    return false;
  }

  if (ctx.used_upgrade_region[region]) {
    return false;
  }

  if (ctx.planned_arrive[region] > 0) {
    return false;
  }

  if (already_targeted_region(S, M, region)) {
    return false;
  }

  if (count_warriors_at(S, opposite(M.my_side), region) > 0) {
    return false;
  }

  return true;
}

static int committed_my_units_to_region(const GameState &S, const GameMap &M,
                                        const ActionContext &ctx, int region) {
  int cnt = 0;

  cnt += count_warriors_at(S, M.my_side, region);
  cnt += ctx.planned_arrive[region];

  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side) {
      continue;
    }

    if (w.state == WState::MOVING && w.target == region) {
      ++cnt;
    }
  }

  return cnt;
}

static int find_surplus_hq_unit(const GameState &S, const GameMap &M,
                                const Paths &P, const MyStatus &my,
                                const ActionContext &ctx, int turn) {
  const Building *hq = my_hq_building(S, M);

  if (hq == nullptr) {
    return -1;
  }

  int hq_guard = required_hq_guard(S, M, P, my, turn);
  int keep_at_hq = std::max(hq->work_cap(), hq_guard);

  int available_here = planned_my_workers_at(S, M, ctx, M.my_hq);

  if (available_here <= keep_at_hq) {
    return -1;
  }

  int best_idx = -1;
  int best_num = -1;

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    const Warrior &w = S.warriors[i];

    if (ctx.used_warrior[i]) {
      continue;
    }

    if (w.id.side != M.my_side) {
      continue;
    }

    if (w.state != WState::STATIONARY) {
      continue;
    }

    if (w.region != M.my_hq) {
      continue;
    }

    if (w.id.num > best_num) {
      best_num = w.id.num;
      best_idx = i;
    }
  }

  return best_idx;
}

static int choose_surplus_army_target(const GameState &S, const GameMap &M,
                                      const Paths &P, const MyStatus &my,
                                      const EnemyStatus &enemy,
                                      const ActionContext &ctx, int turn) {
  (void)my;
  (void)enemy;
  (void)turn;

  int defense_target = choose_primary_defense_target(S, M, P);

  if (defense_target != -1 && defense_target != M.my_hq) {
    const Building *b = find_building(S, defense_target);

    if (b != nullptr && b->side == M.my_side) {
      int enemy_pressure =
          count_warriors_at(S, opposite(M.my_side), defense_target) +
          moving_enemy_targeting_region_count(S, M, defense_target);

      int committed = committed_my_units_to_region(S, M, ctx, defense_target);

      if (enemy_pressure > 0 && committed < enemy_pressure + 2) {
        return defense_target;
      }
    }
  }

  int pressure_target = choose_pressure_target(S, M, P);

  if (pressure_target != -1 && pressure_target != M.opp_hq) {
    const Building *b = find_building(S, pressure_target);

    int committed = committed_my_units_to_region(S, M, ctx, pressure_target);

    int cap = 2;

    if (b != nullptr && b->side == opposite(M.my_side)) {
      cap = enemy_base_required_attackers_siege(S, M, P, pressure_target) + 1;
    }

    if (committed < cap) {
      return pressure_target;
    }
  }

  return -1;
}

static bool use_surplus_hq_army(const GameState &S, const GameMap &M,
                                const Paths &P, const MyStatus &my,
                                const EnemyStatus &enemy,
                                int turn, ActionContext &ctx) {
  int target = choose_surplus_army_target(S, M, P, my, enemy, ctx, turn);

  if (target == -1) {
    return false;
  }

  int move_budget = ctx.budget / MOVE_COST;

  if (move_budget <= 0) {
    return false;
  }

  int target_cap = 2;

  const Building *b = find_building(S, target);

  if (b != nullptr && b->side == opposite(M.my_side)) {
    target_cap = enemy_base_required_attackers_siege(S, M, P, target) + 1;
  }

  if (b != nullptr && b->side == M.my_side) {
    int enemy_pressure =
        count_warriors_at(S, opposite(M.my_side), target) +
        moving_enemy_targeting_region_count(S, M, target);

    target_cap = std::max(1, enemy_pressure + 2);
  }

  int committed = committed_my_units_to_region(S, M, ctx, target);
  int can_send = std::min(move_budget, target_cap - committed);

  if (can_send <= 0) {
    return false;
  }

  int sent = 0;

  while (sent < can_send) {
    int warrior_idx = find_surplus_hq_unit(S, M, P, my, ctx, turn);

    if (warrior_idx == -1) {
      break;
    }

    if (!add_move(ctx, S, M, warrior_idx, target)) {
      break;
    }

    ++sent;
  }

  return sent > 0;
}

static double home_expansion_score(const GameState &S, const GameMap &M, const Paths &P, int region) {
  double nearest_my = dist_to_nearest_my_building(S, M, P, region);
  double my_dist = P.dist[M.my_hq][region];
  double enemy_dist = P.dist[M.opp_hq][region];

  double score = 0.0;
  score += nearest_my * 1.5;
  score += my_dist * 0.3;
  score -= (enemy_dist - my_dist) * 0.2;

  return score;
}

static int choose_home_expansion_target(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx) {
  int best_region = -1;
  double best_score = std::numeric_limits<double>::infinity();

  for (int r : M.strongholds) {
    if (!is_home_expansion_candidate(S, M, P, ctx, r)) {
      continue;
    }

    double score = home_expansion_score(S, M, P, r);

    if (score < best_score) {
      best_score = score;
      best_region = r;
    }
  }

  return best_region;
}

static int count_home_expansion_candidates(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx) {
  int cnt = 0;

  for (int r : M.strongholds) {
    if (is_home_expansion_candidate(S, M, P, ctx, r)) {
      ++cnt;
    }
  }

  return cnt;
}

static bool has_home_expansion_candidate(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx) {
  return count_home_expansion_candidates(S, M, P, ctx) > 0;
}

static bool enemy_is_skipping_core_upgrades(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy) {
  int my_base_power = base_upgrade_power_side(S, M.my_side);
  int enemy_base_power = base_upgrade_power_side(S, opposite(M.my_side));

  if (enemy.bases >= 3 && enemy.hq_level <= 2 && my.hq_level >= 3) {
    return true;
  }

  if (enemy.bases >= 3 && enemy_base_power <= enemy.bases * 10) {
    return true;
  }

  if (my.hq_level >= enemy.hq_level + 1 && my_base_power >= enemy_base_power + 10) {
    return true;
  }

  return false;
}

static bool should_gold_growth_mode(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx) {
  if (my.bases < 2) {
    return false;
  }

  if (my.warriors + 4 < enemy.warriors) {
    return false;
  }

  bool pressure_available = pressure_budget_left(S, M, P, my, enemy);
  bool core_debt = has_core_upgrade_debt(S, M, P, my, ctx);
  int core_cost = next_core_growth_upgrade_cost(S, M, P, my, ctx);

  int my_income = economy_income_power_side(S, M.my_side);
  int enemy_income = economy_income_power_side(S, opposite(M.my_side));
  int score = advantage_score(S, M, my, enemy);

  bool workers_ready = my.workers >= std::max(0, my.work_cap - 1);
  bool base_ready = my.bases >= 3;
  bool target_bases_ready = my.bases >= early_base_target_count(M);
  bool hq_lock = should_lock_hq_growth_upgrade(S, M, my);
  bool enemy_skipping_upgrades = enemy_is_skipping_core_upgrades(S, M, my, enemy);

  if (core_debt && core_cost > 0 && ctx.budget >= core_cost) {
    return true;
  }

  if (!workers_ready) {
    return true;
  }

  if (turn >= 70 && enemy_skipping_upgrades && pressure_available && !hq_lock && !has_decisive_growth_lead(S, M, my, enemy)) {
    return false;
  }
  if (target_bases_ready && workers_ready) {
    return true;
  }

  if (base_ready && workers_ready && score >= 4) {
    return true;
  }

  if (my_income < enemy_income + WORK_INCOME && base_ready && workers_ready) {
    return true;
  }

  if (enemy_income >= my_income + WORK_INCOME * 2) {
    return true;
  }

  if (!pressure_available && enemy_is_scaling_or_not_behind(S, M, my, enemy)) {
    return true;
  }

  return false;
}

static int choose_growth_attack_target(const GameState &S, const GameMap &M, const Paths &P) {
  int best_region = -1;
  double best_score = std::numeric_limits<double>::infinity();

  for (const auto &b : S.buildings) {
    if (b.side != opposite(M.my_side)) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    int enemy_here = count_warriors_at(S, opposite(M.my_side), b.region);
    int enemy_near = count_enemy_within_hops(S, M, P, b.region, 1);
    int my_committed = my_reinforcements_to_region(S, M, b.region);

    if (my_committed >= 2) {
      continue;
    }

    double score = 0.0;
    score += P.dist[M.my_hq][b.region] * 0.2;
    score += P.dist[b.region][M.opp_hq] * 0.1;
    score += b.level * 8.0;
    score += b.hp * 1.5;
    score += enemy_here * 12.0;
    score += enemy_near * 4.0;

    if (score < best_score) {
      best_score = score;
      best_region = b.region;
    }
  }

  return best_region;
}

static bool should_growth_attack_enemy_base(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx) {
  if (turn < 85) {
    return false;
  }

  if (hq_under_attack_now(S, M, P)) {
    return false;
  }

  if (enemy_rush_committed_or_close(S, M, P)) {
    return false;
  }

  if (my.bases < 3) {
    return false;
  }

  if (my.workers < std::max(0, my.work_cap - 1)) {
    return false;
  }

  if (has_growth_upgrade_debt(S, M, my) && ctx.budget < TRAIN_COST * 2) {
    return false;
  }

  int hq_guard = required_hq_guard(S, M, P, my, turn);
  int free_army = my.warriors - my.work_cap - hq_guard;

  if (free_army < 6) {
    return false;
  }

  if (my.warriors + 2 < enemy.warriors) {
    return false;
  }

  int target = choose_growth_attack_target(S, M, P);

  if (target == -1) {
    return false;
  }

  int stage = choose_stage_for_target(S, M, P, target);
  int required = enemy_base_required_attackers_siege(S, M, P, target);
  int ready = count_snowball_attack_pool(S, M, P, ctx, stage, target, hq_guard);

  if (ready < required + 1) {
    return false;
  }

  return true;
}

static bool growth_attack_enemy_base(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx) {
  (void)enemy;

  if (!should_growth_attack_enemy_base(S, M, P, my, enemy, turn, ctx)) {
    return false;
  }

  int target = choose_growth_attack_target(S, M, P);

  if (target == -1) {
    return false;
  }

  int hq_guard = required_hq_guard(S, M, P, my, turn);
  int stage = choose_stage_for_target(S, M, P, target);
  int required = enemy_base_required_attackers_siege(S, M, P, target);
  int ready = count_snowball_attack_pool(S, M, P, ctx, stage, target, hq_guard);
  int affordable = ctx.budget / MOVE_COST;
  int can_send = std::min(ready, affordable);

  if (can_send < required + 1) {
    return false;
  }

  int send_limit = std::min(can_send, required + 2);
  int sent = 0;

  while (sent < send_limit) {
    int warrior_idx = find_snowball_attack_pool_unit(S, M, P, ctx, stage, target, hq_guard);

    if (warrior_idx == -1) {
      break;
    }

    if (!add_move(ctx, S, M, warrior_idx, target)) {
      break;
    }

    ++sent;
  }

  return sent > 0;
}

static bool should_opening_pressure_mode(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx) {
  if (should_gold_growth_mode(S, M, P, my, enemy, turn, ctx)) {
    return false;
  }

  if (should_end_opening_pressure(S, M, P, my, enemy, turn, ctx)) {
    return false;
  }

  if (!pressure_budget_left(S, M, P, my, enemy)) {
    return false;
  }

  if (my.warriors < my.work_cap + 2) {
    return false;
  }

  if (my.warriors + 3 < enemy.warriors) {
    return false;
  }

  return true;
}

static int pressure_unit_limit(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy) {
  int limit = 4;

  if (my.warriors >= enemy.warriors + 3) {
    limit = 6;
  }

  if (my.bases >= enemy.bases + 1) {
    limit = 5;
  }

  return limit;
}

static bool is_enemy_side_region(const GameMap &M, const Paths &P, int region) {
  return P.dist[M.opp_hq][region] < P.dist[M.my_hq][region];
}

static int count_pressure_units(const GameState &S, const GameMap &M, const Paths &P) {
  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side) {
      continue;
    }

    if (is_enemy_side_region(M, P, w.region)) {
      ++cnt;
      continue;
    }

    if (w.state == WState::MOVING && is_enemy_side_region(M, P, w.target)) {
      ++cnt;
      continue;
    }
  }

  return cnt;
}

static bool pressure_budget_left(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy) {
  int pressure_units = count_pressure_units(S, M, P);
  int limit = pressure_unit_limit(S, M, my, enemy);

  return pressure_units < limit;
}

static int opening_pressure_train_target(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy) {
  (void)S;
  (void)M;

  int target = 7;

  if (my.bases >= 3) {
    target = 9;
  }

  if (enemy.warriors > my.warriors + 2) {
    target = std::max(target, enemy.warriors);
  }

  return target;
}

static void train_for_opening_pressure(const GameState &S, const GameMap &M, const Plan &plan, const MyStatus &my, const EnemyStatus &enemy, ActionContext &ctx, int reserve) {
  (void)S;
  (void)M;
  (void)plan;

  int target_warriors = opening_pressure_train_target(S, M, my, enemy);
  int current = my.warriors + ctx.a.train_n;

  if (current >= target_warriors) {
    return;
  }

  int need = target_warriors - current;
  int cap = std::min(remaining_train_cap(my, ctx), need);

  while (cap > 0 && ctx.budget - reserve >= TRAIN_COST) {
    if (!add_train(ctx, 1)) {
      break;
    }

    --cap;
  }
}

static bool enemy_is_scaling_or_not_behind(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy) {
  int my_economy = economy_power_side(S, M, M.my_side);
  int enemy_economy = economy_power_side(S, M, opposite(M.my_side));

  if (enemy_economy >= my_economy - 6) {
    return true;
  }

  if (enemy.bases >= my.bases && enemy.hq_level >= my.hq_level) {
    return true;
  }

  if (base_upgrade_power_side(S, opposite(M.my_side)) > base_upgrade_power_side(S, M.my_side)) {
    return true;
  }

  return false;
}

static bool has_growth_advantage(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy, int turn) {
  (void)turn;

  if (my.bases < 2) {
    return false;
  }

  if (my.work_cap > 0 && my.workers < my.work_cap - 3) {
    return false;
  }

  if (my.warriors + 4 < enemy.warriors) {
    return false;
  }

  int score = advantage_score(S, M, my, enemy);

  if (score >= 4) {
    return true;
  }

  if (has_growth_upgrade_debt(S, M, my)) {
    return true;
  }

  if (enemy_is_scaling_or_not_behind(S, M, my, enemy)) {
    return true;
  }

  return false;
}

static bool should_growth_snowball(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn) {
  if (should_prepare_finish_simple(S, M, P, my, enemy, turn)) {
    return false;
  }

  if (should_siege_enemy_bases(S, M, P, my, enemy, turn)) {
    return false;
  }

  return has_growth_advantage(S, M, my, enemy, turn);
}

static int desired_base_count_by_economy(const GameMap &M, const MyStatus &my) {
  int max_reasonable = std::max(1, ((int)M.strongholds.size() + 1) / 2);

  int desired = my.bases + 1;

  if (my.workers >= std::max(0, my.work_cap - 1)) {
    desired = my.bases + 2;
  }

  if (my.gold >= BASE_LEVELS[1].cost + MOVE_COST) {
    desired = std::max(desired, my.bases + 2);
  }

  return std::min(desired, max_reasonable);
}

static int choose_initial_rally(const GameMap &M, const Paths &P) {
  std::vector<int> p = path(P, M.my_hq, M.opp_hq);

  if (p.empty()) {
    return M.my_hq;
  }

  int idx = (int)p.size() * 2 / 3;
  if (idx >= (int)p.size()) {
    idx = (int)p.size() - 1;
  }

  return p[idx];
}

static Plan make_plan(const GameState &S, const GameMap &M, const Paths &P, int turn) {
  MyStatus my = analyze_my_status(S, M);
  EnemyStatus enemy = analyze_enemy_status(S, M);

  Plan plan;
  plan.desired_bases = desired_base_count_by_economy(M, my);
  plan.rally = choose_initial_rally(M, P);

  int worker_need = my.work_cap;
  int expander_need = my.bases < plan.desired_bases ? 1 : 0;

  int army_need = 1;

  if (my.bases >= 3) {
    army_need = 2;
  }

  if (my.bases >= early_base_target_count(M) && my.hq_level >= 2) {
    army_need = 3;
  }

  if (enemy.warriors > my.warriors + 2) {
    army_need += enemy.warriors - my.warriors - 1;
  }

  plan.desired_warriors = worker_need + expander_need + army_need;
  plan.desired_warriors = std::max(plan.desired_warriors, START_WARRIORS);

  return plan;
}

static int level_upgrade_cost_only(const Building &b) {
  if (b.level >= max_level(b)) {
    return -1;
  }

  return upgrade_cost(b);
}

static bool has_base_builder_or_candidate(const GameState &S, const GameMap &M, const ActionContext &ctx) {
  for (int r : M.strongholds) {
    if (has_building(S, r)) {
      continue;
    }

    if (ctx.used_upgrade_region[r]) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), r) > 0) {
      continue;
    }

    if (count_stationary_warriors_at(S, M.my_side, r) > 0) {
      return true;
    }

    if (ctx.planned_arrive[r] > 0) {
      return true;
    }

    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side) {
        continue;
      }

      if (w.state == WState::MOVING && w.target == r) {
        return true;
      }
    }
  }

  return false;
}

static int reserve_gold_for_base_construction(const GameState &S, const GameMap &M, const ActionContext &ctx) {
  if (has_base_builder_or_candidate(S, M, ctx)) {
    return BASE_LEVELS[1].cost;
  }

  return 0;
}

static const Building *my_hq_building(const GameState &S, const GameMap &M) {
  const Building *hq = find_building(S, M.my_hq);

  if (hq == nullptr || hq->side != M.my_side || hq->type != BType::HQ) {
    return nullptr;
  }

  return hq;
}

static const Building *choose_saturated_base_to_upgrade(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx) {
  const Building *best = nullptr;
  double best_score = std::numeric_limits<double>::infinity();

  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    if (b.level >= 2) {
      continue;
    }

    int workers = planned_my_workers_at(S, M, ctx, b.region);

    if (workers < b.work_cap()) {
      continue;
    }

    double score = P.dist[M.my_hq][b.region];

    if (score < best_score) {
      best_score = score;
      best = &b;
    }
  }

  return best;
}

static const Building *choose_base_to_upgrade(const GameState &S, const GameMap &M, const Paths &P) {
  const Building *best = nullptr;
  double best_score = std::numeric_limits<double>::infinity();

  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    if (b.level >= 2) {
      continue;
    }

    double score = P.dist[M.my_hq][b.region];

    if (score < best_score) {
      best_score = score;
      best = &b;
    }
  }

  return best;
}

static bool add_upgrade(ActionContext &ctx, int region, int cost) {
  if (region < 0 || region >= (int)ctx.used_upgrade_region.size()) {
    return false;
  }

  if (ctx.used_upgrade_region[region]) {
    return false;
  }

  if (cost < 0 || ctx.budget < cost) {
    return false;
  }

  ctx.a.upgrades.push_back(region);
  ctx.used_upgrade_region[region] = true;
  ctx.budget -= cost;

  return true;
}

static bool has_current_stationary_warrior_for_upgrade(const GameState &S, const GameMap &M, const ActionContext &ctx, int region) {
  int here = count_stationary_warriors_at(S, M.my_side, region);
  here -= ctx.planned_leave[region];

  return here > 0;
}

static bool try_upgrade_building(const GameState &S, const GameMap &M, ActionContext &ctx, const Building &b, int reserve) {
  if (count_warriors_at(S, opposite(M.my_side), b.region) > 0) {
    return false;
  }

  if (!has_current_stationary_warrior_for_upgrade(S, M, ctx, b.region)) {
    return false;
  }

  int cost = level_upgrade_cost_only(b);

  if (cost < 0) {
    return false;
  }

  if (ctx.budget - reserve < cost) {
    return false;
  }

  return add_upgrade(ctx, b.region, cost);
}

static int count_pending_base_sites(const GameState &S, const GameMap &M, const ActionContext &ctx) {
  int cnt = 0;

  for (int r : M.strongholds) {
    if (has_building(S, r)) {
      continue;
    }

    if (ctx.used_upgrade_region[r]) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), r) > 0) {
      continue;
    }

    bool pending = false;

    int here = count_stationary_warriors_at(S, M.my_side, r);
    here -= ctx.planned_leave[r];
    here += ctx.planned_arrive[r];

    if (here > 0) {
      pending = true;
    }

    for (const auto &w : S.warriors) {
      if (w.id.side != M.my_side) {
        continue;
      }

      if (w.state == WState::MOVING && w.target == r) {
        pending = true;
      }
    }

    if (pending) {
      ++cnt;
    }
  }

  return cnt;
}

static bool should_hard_reserve_for_expansion(const GameState &S, const GameMap &M, const MyStatus &my, const ActionContext &ctx) {
  int pending = count_pending_base_sites(S, M, ctx);
  int max_reasonable = std::max(1, ((int)M.strongholds.size() + 1) / 2);
  int early_target = std::min(4, max_reasonable);

  if (my.bases + pending < early_target) {
    return true;
  }

  return pending > 0;
}


static bool should_block_upgrade_for_home_expansion(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, const ActionContext &ctx) {
  (void)my;
  (void)enemy;

  if (!has_home_expansion_candidate(S, M, P, ctx)) {
    return false;
  }

  int pending = count_pending_base_sites(S, M, ctx);

  if (pending < 2) {
    return true;
  }

  return false;
}


static void upgrade_economy(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx) {

  EnemyStatus enemy = analyze_enemy_status(S, M);

  if (should_block_upgrade_for_home_expansion(S, M, P, my, enemy, ctx)) {
    return;
  }
  if (!should_allow_growth_upgrade(S, M, P, my, enemy, turn, ctx)) {
    return;
  }

  if (should_finish_enemy_hq(S, M, P, my, enemy, turn)) {
  return;
  }

  int reserve = reserve_gold_for_base_construction(S, M, ctx);
  bool rush_incoming = enemy_rush_incoming(S, M, P, turn);

  int target = early_base_target_count(M);
  int owned_pending = owned_or_pending_bases(S, M, my, ctx);

  bool fast_expand = has_fast_expansion_opportunity(S, M, P, ctx);
  bool gold_growth = should_gold_growth_mode(S, M, P, my, enemy, turn, ctx);
  int my_income = economy_income_power_side(S, M.my_side);
  int enemy_income = economy_income_power_side(S, opposite(M.my_side));
  bool income_lagging = enemy_income >= my_income + WORK_INCOME * 2;

  if (!rush_incoming && !fast_expand && (gold_growth || income_lagging || my.workers >= std::max(0, my.work_cap - 1))) {
    const Building *base = choose_base_to_upgrade(S, M, P);

    if (base != nullptr) {
      try_upgrade_building(S, M, ctx, *base, reserve);
    }
  }

  const Building *hq = my_hq_building(S, M);

  if (hq != nullptr) {
    int target = early_base_target_count(M);
    int owned_pending = owned_or_pending_bases(S, M, my, ctx);

    if (hq->level == 1 && my.bases >= 2) {
      bool enough_bases = owned_pending >= std::min(3, target);
      bool gold_growth = should_gold_growth_mode(S, M, P, my, enemy, turn, ctx);
      int my_income = economy_income_power_side(S, M.my_side);
      int enemy_income = economy_income_power_side(S, opposite(M.my_side));
      bool income_lagging = enemy_income >= my_income + WORK_INCOME * 2;

      if ((enough_bases || gold_growth || income_lagging) && try_upgrade_building(S, M, ctx, *hq, reserve + TRAIN_COST)) {
        return;
      }
    }

    if (hq->level == 2 && my.bases >= 4 && my.warriors >= my.work_cap) {
      if (try_upgrade_building(S, M, ctx, *hq, reserve + TRAIN_COST)) {
        return;
      }
    }

    if (hq->level == 3 && my.bases >= 5 && my.warriors >= my.work_cap + 4) {
      if (try_upgrade_building(S, M, ctx, *hq, reserve + TRAIN_COST * 2)) {
        return;
      }
    }
  }

  if (!rush_incoming && my.bases >= 3) {
    const Building *base = choose_base_to_upgrade(S, M, P);

    if (base != nullptr) {
      try_upgrade_building(S, M, ctx, *base, reserve + TRAIN_COST);
    }
  }
}

static void upgrade_for_siege(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int extra_reserve, ActionContext &ctx) {
  int reserve = TRAIN_COST + extra_reserve;

  const Building *hq = my_hq_building(S, M);

  if (hq != nullptr) {
    if (hq->level == 2 && my.bases >= 3 && my.warriors >= my.work_cap) {
      if (try_upgrade_building(S, M, ctx, *hq, reserve)) {
        return;
      }
    }

    if (hq->level == 3 && my.bases >= 4 && my.warriors >= my.work_cap + 6 && ctx.budget >= reserve + HQ_LEVELS[4].upgrade_cost) {
      if (try_upgrade_building(S, M, ctx, *hq, reserve)) {
        return;
      }
    }
  }

  const Building *front = nullptr;
  double best_dist = std::numeric_limits<double>::infinity();

  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    if (b.level >= 2) {
      continue;
    }

    double d = P.dist[b.region][M.opp_hq];

    if (d < best_dist) {
      best_dist = d;
      front = &b;
    }
  }

  if (front != nullptr) {
    try_upgrade_building(S, M, ctx, *front, reserve);
  }
}

static int desired_army_lead_for_growth(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy) {
  int my_economy = economy_power_side(S, M, M.my_side);
  int enemy_economy = economy_power_side(S, M, opposite(M.my_side));

  int lead = 2;

  if (my_economy >= enemy_economy) {
    lead = 3;
  }

  if (my_economy >= enemy_economy + 12) {
    lead = 4;
  }

  if (my.hq_level >= 3) {
    lead = std::max(lead, 4);
  }

  return lead;
}

static void train_to_keep_army_lead(const GameState &S, const GameMap &M, const Plan &plan, const MyStatus &my, const EnemyStatus &enemy, ActionContext &ctx, int reserve) {
  (void)plan;

  int lead = desired_army_lead_for_growth(S, M, my, enemy);

  int target_warriors = enemy.warriors + lead;
  target_warriors = std::max(target_warriors, my.work_cap);

  if (my.bases >= early_base_target_count(M)) {
    target_warriors = std::max(target_warriors, my.work_cap + 1);
  }

  int current = my.warriors + ctx.a.train_n;

  if (current >= target_warriors) {
    return;
  }

  int need = target_warriors - current;
  int cap = std::min(remaining_train_cap(my, ctx), need);

  while (cap > 0 && ctx.budget - reserve >= TRAIN_COST) {
    if (!add_train(ctx, 1)) {
      break;
    }

    --cap;
  }
}

static const Building *choose_growth_base_to_upgrade(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx) {
  const Building *best = nullptr;
  double best_score = std::numeric_limits<double>::infinity();

  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    if (b.level >= BASE_MAX_LEVEL) {
      continue;
    }

    int workers = planned_my_workers_at(S, M, ctx, b.region);

    if (workers < b.work_cap()) {
      continue;
    }

    double score = b.level * 100000.0;
    score += P.dist[M.my_hq][b.region];

    if (P.dist[b.region][M.opp_hq] < P.dist[M.my_hq][M.opp_hq] * 0.6) {
      score -= 3000.0;
    }

    if (score < best_score) {
      best_score = score;
      best = &b;
    }
  }

  return best;
}

static bool should_allow_growth_upgrade(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx) {
  bool fast_expand = has_fast_expansion_opportunity(S, M, P, ctx);

  int my_income = economy_income_power_side(S, M.my_side);
  int enemy_income = economy_income_power_side(S, opposite(M.my_side));
  bool income_lagging = enemy_income >= my_income + WORK_INCOME * 2;

  if (fast_expand && !income_lagging) {
    return false;
  }

  if (should_gold_growth_mode(S, M, P, my, enemy, turn, ctx)) {
    return true;
  }

  if (income_lagging) {
    return true;
  }

  if (!fast_expand && my.workers >= std::max(0, my.work_cap - 1)) {
    return true;
  }

  return false;
}

static void upgrade_for_growth_snowball(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx) {
  EnemyStatus enemy = analyze_enemy_status(S, M);

  if (!should_allow_growth_upgrade(S, M, P, my, enemy, turn, ctx)) {
    return;
  }

  int reserve = reserve_gold_for_base_construction(S, M, ctx);
  const Building *hq = my_hq_building(S, M);

  if (hq != nullptr && should_lock_hq_growth_upgrade(S, M, my)) {
    if (hq->level == 1 && my.bases >= 2) {
      try_upgrade_building(S, M, ctx, *hq, reserve);
      return;
    }

    if (hq->level == 2 && my.bases >= 3 && my.workers >= std::max(0, my.work_cap - 1)) {
      try_upgrade_building(S, M, ctx, *hq, reserve);
      return;
    }

    if (hq->level == 3 && my.bases >= 4 && my.workers >= std::max(0, my.work_cap - 1)) {
      try_upgrade_building(S, M, ctx, *hq, reserve + TRAIN_COST);
      return;
    }
  }

  const Building *base = choose_growth_base_to_upgrade(S, M, P, ctx);

  if (base != nullptr) {
    if (try_upgrade_building(S, M, ctx, *base, reserve)) {
      return;
    }
  }

  if (hq != nullptr) {
    if (hq->level == 4 && my.bases >= 5 && my.workers >= std::max(0, my.work_cap - 1)) {
      if (try_upgrade_building(S, M, ctx, *hq, reserve + TRAIN_COST)) {
        return;
      }
    }
  }
}



static bool add_move(ActionContext &ctx, const GameState &S, const GameMap &M, int warrior_index, int target) {
  if (warrior_index < 0 || warrior_index >= (int)S.warriors.size()) {
    return false;
  }

  const Warrior &w = S.warriors[warrior_index];

  if (ctx.used_warrior[warrior_index]) {
    return false;
  }

  if (w.id.side != M.my_side) {
    return false;
  }

  if (w.state != WState::STATIONARY) {
    return false;
  }

  if (w.region == target) {
    return false;
  }

  int move_cost = MOVE_COST;

  if (has_my_building(S, M, target)) {
    move_cost = 0;
  }

  if (ctx.budget < move_cost) {
    return false;
  }

  ctx.a.moves.emplace_back(w.id, target);
  ctx.used_warrior[warrior_index] = true;
  ctx.planned_leave[w.region]++;
  ctx.planned_arrive[target]++;
  ctx.budget -= move_cost;

  return true;
}

static bool add_train(ActionContext &ctx, int n) {
  if (n <= 0) {
    return false;
  }

  int cost = TRAIN_COST * n;

  if (ctx.budget < cost) {
    return false;
  }

  ctx.a.train_n += n;
  ctx.budget -= cost;

  return true;
}

static bool add_train_with_reserve(ActionContext &ctx, int n, int reserve) {
  if (n <= 0) {
    return false;
  }

  int cost = TRAIN_COST * n;

  if (ctx.budget - reserve < cost) {
    return false;
  }

  ctx.a.train_n += n;
  ctx.budget -= cost;

  return true;
}

static void train_units_with_reserve(const GameState &S, const GameMap &M, const Plan &plan, const MyStatus &my, int turn, ActionContext &ctx, int reserve){
  (void)S;
  (void)M;

  int cap = remaining_train_cap(my, ctx);
  int desired = plan.desired_warriors;
  int current = my.warriors + ctx.a.train_n;

  reserve = std::max(reserve, expansion_gold_reserve(S, M, my, ctx, turn));

  while (cap > 0 && current < desired) {
    if (!add_train_with_reserve(ctx, 1, reserve)) {
      break;
    }

    --cap;
    ++current;
  }
}

static int remaining_train_cap(const MyStatus &my, const ActionContext &ctx) {
  return std::max(0, my.train_cap - ctx.a.train_n);
}



static void build_bases_under_workers(const GameState &S, const GameMap &M, ActionContext &ctx) {
  for (int r : M.strongholds) {
    if (has_building(S, r)) {
      continue;
    }

    if (ctx.used_upgrade_region[r]) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), r) > 0) {
      continue;
    }

    int here = count_stationary_warriors_at(S, M.my_side, r);
    here -= ctx.planned_leave[r];

    if (here <= 0) {
      continue;
    }

    if (!add_upgrade(ctx, r, BASE_LEVELS[1].cost)) {
      break;
    }
  }
}

static int path_hops(const Paths &P, int from, int to) {
  std::vector<int> p = path(P, from, to);

  if (p.empty()) {
    return 1000000;
  }

  return (int)p.size() - 1;
}

static int my_reinforcements_to_region(const GameState &S, const GameMap &M, int region) {
  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side) {
      continue;
    }

    if (w.state == WState::MOVING && w.target == region) {
      ++cnt;
    }
  }

  return cnt;
}

static int projected_main_path_index(const GameMap &M, const Paths &P, int region, int tolerance_hops) {
  std::vector<int> p = path(P, M.opp_hq, M.my_hq);

  if (p.empty()) {
    return -1;
  }

  int best_idx = -1;
  int best_hops = tolerance_hops + 1;

  for (int i = 0; i < (int)p.size(); ++i) {
    int hops = path_hops(P, region, p[i]);

    if (hops < best_hops) {
      best_hops = hops;
      best_idx = i;
    }
  }

  if (best_hops > tolerance_hops) {
    return -1;
  }

  return best_idx;
}

static bool near_my_non_hq_base(const GameState &S, const GameMap &M, const Paths &P, int region) {
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    if (path_hops(P, region, b.region) <= 1) {
      return true;
    }
  }

  return false;
}

static int enemy_main_path_hq_pressure_count(const GameState &S, const GameMap &M, const Paths &P, int numerator, int denominator) {
  std::vector<int> p = path(P, M.opp_hq, M.my_hq);

  if (p.empty()) {
    return 0;
  }

  int last = (int)p.size() - 1;
  int threshold = last * numerator / denominator;
  int final_segment = std::max(0, last - 1);

  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side != opposite(M.my_side)) {
      continue;
    }

    int idx = projected_main_path_index(M, P, w.region, 1);

    if (idx < threshold) {
      continue;
    }

    if (idx < final_segment && near_my_non_hq_base(S, M, P, w.region)) {
      continue;
    }

    ++cnt;
  }

  return cnt;
}

static int enemy_threat_near_region(const GameState &S, const GameMap &M, const Paths &P, int region) {
  int threat = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side != opposite(M.my_side)) {
      continue;
    }

    int hops = path_hops(P, w.region, region);

    if (hops <= 1) {
      threat += 4;
    } else if (hops <= 3) {
      threat += 2;
    } else if (hops <= 6) {
      threat += 1;
    }
  }

  return threat;
}

static int moving_enemy_targeting_region_count(const GameState &S, const GameMap &M, int region) {
  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side != opposite(M.my_side)) {
      continue;
    }

    if (w.state != WState::MOVING) {
      continue;
    }

    if (w.target == region) {
      ++cnt;
    }
  }

  return cnt;
}

static int moving_enemy_within_hops(const GameState &S, const GameMap &M, const Paths &P, int region, int max_hops) {
  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side != opposite(M.my_side)) {
      continue;
    }

    if (w.state != WState::MOVING) {
      continue;
    }

    if (path_hops(P, w.region, region) <= max_hops) {
      ++cnt;
    }
  }

  return cnt;
}

static int moving_enemy_threat_near_region(const GameState &S, const GameMap &M, const Paths &P, int region) {
  int threat = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side != opposite(M.my_side)) {
      continue;
    }

    if (w.state != WState::MOVING) {
      continue;
    }

    int hops = path_hops(P, w.region, region);

    if (w.target == region) {
      threat += 4;
    } else if (hops <= 1) {
      threat += 3;
    } else if (hops <= 3) {
      threat += 1;
    }
  }

  return threat;
}

static int count_enemy_within_hops(const GameState &S, const GameMap &M, const Paths &P, int region, int max_hops) {
  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side != opposite(M.my_side)) {
      continue;
    }

    if (path_hops(P, w.region, region) <= max_hops) {
      ++cnt;
    }
  }

  return cnt;
}

static int enemy_hq_blob_count(const GameState &S, const GameMap &M, const Paths &P) {
  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side != opposite(M.my_side)) {
      continue;
    }

    if (path_hops(P, w.region, M.opp_hq) <= 1) {
      ++cnt;
    }
  }

  return cnt;
}

static int enemy_committed_to_my_hq_count(const GameState &S, const GameMap &M) {
  int cnt = 0;

  for (const auto &w : S.warriors) {
    if (w.id.side != opposite(M.my_side)) {
      continue;
    }

    if (w.state == WState::MOVING && w.target == M.my_hq) {
      ++cnt;
    }
  }

  return cnt;
}

static bool enemy_rush_incoming(const GameState &S, const GameMap &M, const Paths &P, int turn) {
  (void)turn;

  int enemy_here = count_warriors_at(S, opposite(M.my_side), M.my_hq);
  int committed = enemy_committed_to_my_hq_count(S, M);
  int moving_close = moving_enemy_within_hops(S, M, P, M.my_hq, 5);

  if (enemy_here > 0) {
    return true;
  }

  if (committed >= 3) {
    return true;
  }

  if (moving_close >= 4) {
    return true;
  }

  return false;
}

static bool enemy_rush_committed_or_close(const GameState &S, const GameMap &M, const Paths &P) {
  int enemy_here = count_warriors_at(S, opposite(M.my_side), M.my_hq);
  int committed = enemy_committed_to_my_hq_count(S, M);
  int enemy_near = count_enemy_within_hops(S, M, P, M.my_hq, 2);

  if (enemy_here > 0) {
    return true;
  }

  if (committed >= 4) {
    return true;
  }

  if (enemy_near >= 6 && committed >= 2) {
    return true;
  }

  return false;
}

static bool enemy_rush_preparing(const GameState &S, const GameMap &M, const Paths &P, int turn) {
  (void)turn;

  int committed = enemy_committed_to_my_hq_count(S, M);
  int enemy_here = count_warriors_at(S, opposite(M.my_side), M.my_hq);
  int enemy_near = count_enemy_within_hops(S, M, P, M.my_hq, 2);

  if (enemy_here > 0) {
    return true;
  }

  if (committed >= 4) {
    return true;
  }

  if (enemy_near >= 6 && committed >= 2) {
    return true;
  }

  return false;
}

static bool hq_under_attack_now(const GameState &S, const GameMap &M, const Paths &P) {
  (void)P;

  int enemy_here = count_warriors_at(S, opposite(M.my_side), M.my_hq);

  return enemy_here > 0;
}

static int choose_primary_defense_target(const GameState &S, const GameMap &M, const Paths &P) {
  if (count_warriors_at(S, opposite(M.my_side), M.my_hq) > 0) {
    return M.my_hq;
  }

  if (moving_enemy_targeting_region_count(S, M, M.my_hq) >= 2) {
    return M.my_hq;
  }

  int best_region = -1;
  int best_score = 0;

  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    if (b.type == BType::HQ) {
      continue;
    }

    int enemy_here = count_warriors_at(S, opposite(M.my_side), b.region);
    int committed = moving_enemy_targeting_region_count(S, M, b.region);
    int moving_close = moving_enemy_within_hops(S, M, P, b.region, 2);

    if (enemy_here == 0 && committed == 0 && moving_close < 2) {
      continue;
    }

    int score = 0;
    score += enemy_here * 500;
    score += committed * 300;
    score += moving_close * 100;

    if (score > best_score) {
      best_score = score;
      best_region = b.region;
    }
  }

  return best_region;
}

static int defense_shortage_at_building(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, const Building &b) {
  if (b.side != M.my_side) {
    return 0;
  }

  int enemy_here = count_warriors_at(S, opposite(M.my_side), b.region);
  int threat = enemy_here * 4 + moving_enemy_threat_near_region(S, M, P, b.region);

  if (threat <= 0) {
    return 0;
  }

  int current = planned_my_workers_at(S, M, ctx, b.region);
  int incoming = my_reinforcements_to_region(S, M, b.region);
  int have = current + incoming;

  int desired = b.work_cap();

  if (b.type == BType::HQ) {
    desired += 3;
  } else {
    desired += 1;
  }

  desired += std::min(8, threat);

  if (b.type == BType::HQ) {
    desired = std::max(desired, 5);
  } else {
    desired = std::max(desired, 2);
  }

  return std::max(0, desired - have);
}

static void emergency_train_for_defense(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx) {
  bool immediate = hq_under_attack_now(S, M, P);
  bool preparing = enemy_rush_preparing(S, M, P, turn);

  if (!immediate && !preparing) {
    return;
  }

  int cap = remaining_train_cap(my, ctx);

  while (cap > 0 && ctx.budget >= TRAIN_COST) {
    if (!add_train(ctx, 1)) {
      break;
    }

    --cap;
  }
}

static int find_best_defender_for_region(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int target_region) {
  int best_idx = -1;
  double best_dist = std::numeric_limits<double>::infinity();

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    const Warrior &w = S.warriors[i];

    if (ctx.used_warrior[i]) {
      continue;
    }

    if (w.id.side != M.my_side) {
      continue;
    }

    if (w.state != WState::STATIONARY) {
      continue;
    }

    if (!can_use_warrior_for_defense(S, M, ctx, w.region, target_region)) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), w.region) > 0) {
      continue;
    }

    double d = P.dist[w.region][target_region];

    if (d < best_dist) {
      best_dist = d;
      best_idx = i;
    }
  }

  return best_idx;
}

static bool should_lock_hq_growth_upgrade(const GameState &S, const GameMap &M, const MyStatus &my) {
  const Building *hq = my_hq_building(S, M);

  if (hq == nullptr) {
    return false;
  }

  if (hq->level < 2 && my.bases >= 2) {
    return true;
  }

  if (hq->level < 3 && my.bases >= 3 && my.workers >= std::max(0, my.work_cap - 1)) {
    return true;
  }

  if (hq->level < 4 && my.bases >= 4 && my.workers >= std::max(0, my.work_cap - 1)) {
    return true;
  }

  return false;
}

static bool has_decisive_growth_lead(const GameState &S, const GameMap &M, const MyStatus &my, const EnemyStatus &enemy) {
  int my_income = economy_income_power_side(S, M.my_side);
  int enemy_income = economy_income_power_side(S, opposite(M.my_side));
  int score = advantage_score(S, M, my, enemy);

  if (score >= 14) {
    return true;
  }

  if (my_income >= enemy_income + WORK_INCOME * 3 && my.warriors >= enemy.warriors) {
    return true;
  }

  if (my.bases >= enemy.bases + 2 && my.warriors >= enemy.warriors - 1) {
    return true;
  }

  if (my.hq_level >= enemy.hq_level + 2 && my.warriors >= enemy.warriors) {
    return true;
  }

  return false;
}

static void recall_idle_units_to_hq_for_rush(const GameState &S, const GameMap &M, ActionContext &ctx) {
  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    const Warrior &w = S.warriors[i];

    if (ctx.used_warrior[i]) {
      continue;
    }

    if (w.id.side != M.my_side) {
      continue;
    }

    if (w.state != WState::STATIONARY) {
      continue;
    }

    if (w.region == M.my_hq) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), w.region) > 0) {
      continue;
    }

    add_move(ctx, S, M, i, M.my_hq);
  }
}

static void defend_hq_for_incoming_rush(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx) {
  int enemy_here = count_warriors_at(S, opposite(M.my_side), M.my_hq);
  int close = count_enemy_within_hops(S, M, P, M.my_hq, 5);
  int committed = enemy_committed_to_my_hq_count(S, M);

  int current = planned_my_workers_at(S, M, ctx, M.my_hq);
  int incoming = my_reinforcements_to_region(S, M, M.my_hq);
  int have = current + incoming;

  int desired = required_hq_guard(S, M, P, my, turn);
  desired = std::max(desired, enemy_here + close + committed / 2 + 3);
  desired = std::min(desired, my.warriors);

  while (have < desired) {
    int warrior_idx = find_best_defender_for_region(S, M, P, ctx, M.my_hq);

    if (warrior_idx == -1) {
      break;
    }

    if (!add_move(ctx, S, M, warrior_idx, M.my_hq)) {
      break;
    }

    ++have;
  }
}

static void defend_buildings(const GameState &S, const GameMap &M, const Paths &P, ActionContext &ctx) {
  int target = choose_primary_defense_target(S, M, P);

  if (target == -1) {
    return;
  }

  const Building *b = find_building(S, target);

  if (b == nullptr || b->side != M.my_side) {
    return;
  }

  int enemy_here = count_warriors_at(S, opposite(M.my_side), target);
  int close = count_enemy_within_hops(S, M, P, target, 3);
  int wide = count_enemy_within_hops(S, M, P, target, 7);

  int current = planned_my_workers_at(S, M, ctx, target);
  int incoming = my_reinforcements_to_region(S, M, target);
  int have = current + incoming;

  int desired = b->work_cap();

  if (b->type == BType::HQ) {
    desired += 4;
    desired += enemy_here;
    desired += close;
    desired += wide / 2;
    desired = std::max(desired, 7);
  } else {
    desired += enemy_here;
    desired += close / 2;
    desired = std::max(desired, b->work_cap() + 1);
    desired = std::min(desired, 2);
  }

  desired = std::min(desired, count_warriors_side(S, M.my_side));

  while (have < desired) {
    int warrior_idx = find_best_defender_for_region(S, M, P, ctx, target);

    if (warrior_idx == -1) {
      break;
    }

    if (!add_move(ctx, S, M, warrior_idx, target)) {
      break;
    }

    ++have;
  }
}

static int choose_pressure_target(const GameState &S, const GameMap &M, const Paths &P) {
  int best_region = -1;
  double best_score = std::numeric_limits<double>::infinity();

  for (const auto &b : S.buildings) {
    if (b.side != opposite(M.my_side)) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    int enemy_here = count_warriors_at(S, opposite(M.my_side), b.region);

    double score = P.dist[M.my_hq][b.region];
    score += enemy_here * 3000.0;

    if (score < best_score) {
      best_score = score;
      best_region = b.region;
    }
  }

  return best_region;
}

static int find_best_pressure_attacker(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int target, int hq_guard) {
  int best_idx = -1;
  double best_dist = std::numeric_limits<double>::infinity();

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    const Warrior &w = S.warriors[i];

    if (ctx.used_warrior[i]) {
      continue;
    }

    if (w.id.side != M.my_side) {
      continue;
    }

    if (w.state != WState::STATIONARY) {
      continue;
    }

    if (w.region == target) {
      continue;
    }

    if (is_unbuilt_stronghold(S, M, w.region)) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), w.region) > 0) {
      continue;
    }

    const Building *home_b = find_building(S, w.region);

    if (home_b != nullptr && home_b->side == M.my_side) {
      int here = planned_my_workers_at(S, M, ctx, w.region);

      if (home_b->type == BType::HQ) {
        if (here <= hq_guard) {
          continue;
        }
      } else {
        if (here <= 1) {
          continue;
        }
      }
    } else {
      if (!can_take_warrior_from_region(S, M, ctx, w.region)) {
        continue;
      }
    }

    double d = P.dist[w.region][target];

    if (d < best_dist) {
      best_dist = d;
      best_idx = i;
    }
  }

  return best_idx;
}

static int enemy_hq_required_attackers(const GameState &S, const GameMap &M, const Paths &P) {
  const Building *hq = find_building(S, M.opp_hq);

  int level = 1;
  int hp = HQ_LEVELS[1].hp;
  int turret = HQ_LEVELS[1].turret;

  if (hq != nullptr) {
    level = hq->level;
    hp = hq->hp;
    turret = HQ_LEVELS[level].turret;
  }

  int enemy_here = count_warriors_at(S, opposite(M.my_side), M.opp_hq);
  int enemy_near = count_enemy_within_hops(S, M, P, M.opp_hq, 1);
  int support = std::max(0, enemy_near - enemy_here);

  int required = 0;

  required += enemy_here;
  required += (support + 1) / 2;
  required += (hp + 1) / 2;
  required += turret * 2;
  required += 3;

  if (level >= 4) {
    required += 2;
  }

  return std::max(8, required);
}

static bool is_finish_front_unit(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int warrior_index, int stage, int hq_guard) {
  if (warrior_index < 0 || warrior_index >= (int)S.warriors.size()) {
    return false;
  }

  const Warrior &w = S.warriors[warrior_index];

  if (ctx.used_warrior[warrior_index]) {
    return false;
  }

  if (w.id.side != M.my_side) {
    return false;
  }

  if (w.state != WState::STATIONARY) {
    return false;
  }

  if (w.region == M.opp_hq) {
    return false;
  }

  if (count_warriors_at(S, opposite(M.my_side), w.region) > 0) {
    return false;
  }

  double stage_to_hq = P.dist[stage][M.opp_hq];
  double unit_to_hq = P.dist[w.region][M.opp_hq];

  if (w.region != stage && unit_to_hq > stage_to_hq) {
    return false;
  }

  if (w.region == M.my_hq) {
    int here = planned_my_workers_at(S, M, ctx, M.my_hq);

    if (here <= hq_guard) {
      return false;
    }
  }

  return true;
}

static int count_finish_attack_pool(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int hq_guard) {
  int cnt = 0;

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    if (is_finish_front_unit(S, M, P, ctx, i, stage, hq_guard)) {
      ++cnt;
    }
  }

  return cnt;
}

static int find_finish_attack_pool_unit(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int hq_guard) {
  int best_idx = -1;
  double best_dist = std::numeric_limits<double>::infinity();

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    if (!is_finish_front_unit(S, M, P, ctx, i, stage, hq_guard)) {
      continue;
    }

    const Warrior &w = S.warriors[i];
    double d = P.dist[w.region][M.opp_hq];

    if (d < best_dist) {
      best_dist = d;
      best_idx = i;
    }
  }

  return best_idx;
}

static bool should_prepare_finish_simple(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn) {
  (void)S;
  (void)M;
  (void)P;
  (void)my;
  (void)enemy;
  (void)turn;

  return false;
}

static int enemy_base_required_attackers_siege(const GameState &S, const GameMap &M, const Paths &P, int target) {
  const Building *b = find_building(S, target);

  if (b == nullptr) {
    return 5;
  }

  int enemy_here = count_warriors_at(S, opposite(M.my_side), target);
  int enemy_near = count_enemy_within_hops(S, M, P, target, 1);
  int support = std::max(0, enemy_near - enemy_here);

  int required = 0;
  required += enemy_here;
  required += (support + 1) / 2;
  required += (b->hp + 3) / 4;
  required += BASE_LEVELS[b->level].turret;
  required += 2;

  if (b->level >= 3) {
    required += 3;
  }

  return std::max(6, required);
}

static bool has_siege_advantage(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn) {
  if (turn < 150) {
    return false;
  }

  if (turn >= 180) {
    return false;
  }

  if (has_growth_upgrade_debt(S, M, my)) {
    return false;
  }

  if (my.hq_level < 4) {
    return false;
  }

  if (!has_growth_advantage(S, M, my, enemy, turn)) {
    return false;
  }

  if (my.bases < early_base_target_count(M)) {
    return false;
  }

  if (my.work_cap > 0 && my.workers < my.work_cap - 1) {
    return false;
  }

  int score = advantage_score(S, M, my, enemy);

  if (score < 22) {
    return false;
  }

  int hq_guard = required_hq_guard(S, M, P, my, turn);
  int free_army = my.warriors - my.work_cap - hq_guard;

  if (free_army < 18) {
    return false;
  }

  if (my.warriors < enemy.warriors + 10) {
    return false;
  }

  return true;
}

static bool should_siege_enemy_bases(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn) {
  if (!has_siege_advantage(S, M, P, my, enemy, turn)) {
    return false;
  }

  for (const auto &b : S.buildings) {
    if (b.side == opposite(M.my_side) && b.type == BType::BASE) {
      return true;
    }
  }

  return false;
}

static int finish_launch_gold_reserve(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, const ActionContext &ctx, int turn) {
  if (!should_prepare_finish_simple(S, M, P, my, enemy, turn)) {
    return 0;
  }

  int hq_guard = required_hq_guard(S, M, P, my, turn);
  int stage = choose_finish_stage(S, M, P);
  int required = enemy_hq_required_attackers_light(S, M, P);
  int ready = count_finish_attack_pool(S, M, P, ctx, stage, hq_guard);

  if (ready < required - 2) {
    return 0;
  }

  int reserve_units = std::min(ready, required + 4);
  return reserve_units * MOVE_COST;
}

static int siege_launch_gold_reserve(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, const ActionContext &ctx, int turn) {
  if (!should_siege_enemy_bases(S, M, P, my, enemy, turn)) {
    return 0;
  }

  int target = choose_pressure_target(S, M, P);

  if (target == -1) {
    return 0;
  }

  int hq_guard = required_hq_guard(S, M, P, my, turn);
  int stage = choose_stage_for_target(S, M, P, target);
  int required = enemy_base_required_attackers_siege(S, M, P, target);
  int ready = count_snowball_attack_pool(S, M, P, ctx, stage, target, hq_guard);

  if (ready < required - 2) {
    return 0;
  }

  int reserve_units = std::min(ready, required + 3);
  return reserve_units * MOVE_COST;
}

static int operation_gold_reserve(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, const ActionContext &ctx, int turn) {
  int reserve = finish_launch_gold_reserve(S, M, P, my, enemy, ctx, turn);

  if (reserve > 0) {
    return reserve;
  }

  return siege_launch_gold_reserve(S, M, P, my, enemy, ctx, turn);
}

static bool should_late_spend_mode(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx) {
  (void)enemy;
  (void)turn;

  int pending = count_pending_base_sites(S, M, ctx);
  int desired = desired_base_count_by_economy(M, my);
  int owned_or_pending = my.bases + pending;

  bool has_expansion_candidate = has_home_expansion_candidate(S, M, P, ctx) ||
                                 has_fast_expansion_opportunity(S, M, P, ctx);

  bool expansion_saturated = owned_or_pending >= desired || !has_expansion_candidate;
  bool workers_ready = my.workers >= std::max(0, my.work_cap - 1);
  bool no_pending_expansion = pending == 0;

  if (!expansion_saturated) {
    return false;
  }

  if (!workers_ready) {
    return false;
  }

  if (!no_pending_expansion) {
    return false;
  }

  int my_income = economy_income_power_side(S, M.my_side);

  const Building *hq = my_hq_building(S, M);
  int next_hq_cost = 0;

  if (hq != nullptr && hq->level < HQ_MAX_LEVEL) {
    next_hq_cost = level_upgrade_cost_only(*hq);
  }

  bool hq_upgrade_ready = next_hq_cost > 0 && ctx.budget >= next_hq_cost;
  bool gold_overflow = ctx.budget >= std::max(TRAIN_COST * 10, my_income * 4);

  if (hq_upgrade_ready) {
    return true;
  }

  if (gold_overflow) {
    return true;
  }

  return false;
}

static bool should_prepare_final_rush(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx) {
  (void)enemy;
  (void)turn;

  int pending = count_pending_base_sites(S, M, ctx);
  int desired = desired_base_count_by_economy(M, my);
  int owned_or_pending = my.bases + pending;

  bool has_expansion_candidate = has_home_expansion_candidate(S, M, P, ctx) ||
                                 has_fast_expansion_opportunity(S, M, P, ctx);

  bool expansion_done = owned_or_pending >= desired || !has_expansion_candidate;
  bool workers_done = my.workers >= std::max(0, my.work_cap - 1);
  bool no_pending = pending == 0;

  if (!expansion_done) {
    return false;
  }

  if (!workers_done) {
    return false;
  }

  if (!no_pending) {
    return false;
  }

  const Building *hq = my_hq_building(S, M);
  bool hq_done = hq == nullptr || hq->level >= HQ_MAX_LEVEL;
  bool hq_high = hq == nullptr || hq->level >= HQ_MAX_LEVEL - 1;

  int my_income = economy_income_power_side(S, M.my_side);

  bool huge_gold = ctx.budget >= std::max(TRAIN_COST * 12, my_income * 4);

  if (hq_done && huge_gold) {
    return true;
  }

  if (hq_high && huge_gold && my.workers >= my.work_cap) {
    return true;
  }

  return false;
}

static const Building *choose_late_score_upgrade(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx) {
  const Building *hq = my_hq_building(S, M);

  if (hq != nullptr && hq->level < HQ_MAX_LEVEL) {
    int cost = level_upgrade_cost_only(*hq);

    if (cost > 0 &&
        ctx.budget >= cost &&
        count_warriors_at(S, opposite(M.my_side), hq->region) == 0 &&
        !ctx.used_upgrade_region[hq->region] &&
        has_current_stationary_warrior_for_upgrade(S, M, ctx, hq->region)) {
      return hq;
    }
  }

  const Building *best = nullptr;
  double best_score = std::numeric_limits<double>::infinity();

  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    if (b.level >= BASE_MAX_LEVEL) {
      continue;
    }

    if (ctx.used_upgrade_region[b.region]) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), b.region) > 0) {
      continue;
    }

    if (!has_current_stationary_warrior_for_upgrade(S, M, ctx, b.region)) {
      continue;
    }

    int cost = level_upgrade_cost_only(b);

    if (cost <= 0 || ctx.budget < cost) {
      continue;
    }

    double score = 0.0;
    score += b.level * 100000.0;
    score += P.dist[b.region][M.opp_hq] * 0.2;
    score += P.dist[M.my_hq][b.region] * 0.1;

    if (score < best_score) {
      best_score = score;
      best = &b;
    }
  }

  return best;
}

static void upgrade_for_late_score(const GameState &S, const GameMap &M, const Paths &P, ActionContext &ctx) {
  while (true) {
    const Building *b = choose_late_score_upgrade(S, M, P, ctx);

    if (b == nullptr) {
      break;
    }

    if (!try_upgrade_building(S, M, ctx, *b, 0)) {
      break;
    }
  }
}

static int late_train_target(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn) {
  (void)enemy;
  (void)turn;

  int hq_guard = required_hq_guard(S, M, P, my, turn);
  int hq_required = enemy_hq_required_attackers_light(S, M, P);

  int target = my.work_cap + hq_guard + hq_required + 3;

  return target;
}

static void train_for_late_spend(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx) {
  int target = late_train_target(S, M, P, my, enemy, turn);
  int current = my.warriors + ctx.a.train_n;
  int cap = remaining_train_cap(my, ctx);

  while (cap > 0 && current < target && ctx.budget >= TRAIN_COST) {
    if (!add_train(ctx, 1)) {
      break;
    }

    --cap;
    ++current;
  }
}

static void do_late_spend(const GameState &S, const GameMap &M, const Paths &P, const Plan &plan, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx) {
  (void)plan;

  build_bases_under_workers(S, M, ctx);
  fill_workers(S, M, P, ctx);

  upgrade_for_late_score(S, M, P, ctx);

  bool final_rush = should_prepare_final_rush(S, M, P, my, enemy, turn, ctx);

  if (final_rush) {
    train_for_late_spend(S, M, P, my, enemy, turn, ctx);

    if (!has_any_action(ctx)) {
      attack_enemy(S, M, P, plan, my, enemy, turn, ctx);
    }
  }
}

static void siege_enemy_base_grouped(const GameState &S, const GameMap &M, const Paths &P,
                                     const MyStatus &my, int turn,
                                     int hq_guard, int surplus, ActionContext &ctx) {
  (void)my;
  (void)turn;
  (void)surplus;

  int target = choose_pressure_target(S, M, P);

  if (target == -1) {
    return;
  }

  int stage = choose_stage_for_target(S, M, P, target);
  int ready = count_snowball_attack_pool(S, M, P, ctx, stage, target, hq_guard);
  int required = enemy_base_required_attackers_siege(S, M, P, target);
  int affordable = ctx.budget / MOVE_COST;
  int can_send = std::min(ready, affordable);

  int already_committed = count_warriors_at(S, M.my_side, target) +
                          my_reinforcements_to_region(S, M, target) +
                          ctx.planned_arrive[target];

  if (already_committed >= required + 1) {
    return;
  }

  if (already_committed + can_send < required + 3) {
    return;
  }

  int send_limit = std::min(can_send, required + 1 - already_committed);

  if (send_limit <= 0) {
    return;
  }

  int sent = 0;

  while (sent < send_limit) {
    int warrior_idx = find_snowball_attack_pool_unit(S, M, P, ctx, stage, target, hq_guard);

    if (warrior_idx == -1) {
      break;
    }

    if (!add_move(ctx, S, M, warrior_idx, target)) {
      break;
    }

    ++sent;
  }
}

static bool should_finish_enemy_hq(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn) {
  if (turn < 115) {
    return false;
  }

  if (my.bases < 3 && turn < 160) {
    return false;
  }

  if (my.hq_level < 3 && turn < 160) {
    return false;
  }

  int hq_guard = required_hq_guard(S, M, P, my, turn);
  int free_army = my.warriors - my.work_cap - hq_guard;
  int required = enemy_hq_required_attackers(S, M, P);

  if (free_army < required + 2) {
    return false;
  }

  if (turn < 170 && my.warriors < enemy.warriors + 4) {
    return false;
  }

  return true;
}

static int choose_finish_stage(const GameState &S, const GameMap &M, const Paths &P) {
  int best_region = M.my_hq;
  double best_score = P.dist[M.my_hq][M.opp_hq] + 100000.0;

  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), b.region) > 0) {
      continue;
    }

    double score = P.dist[b.region][M.opp_hq];

    int enemy_near = count_enemy_within_hops(S, M, P, b.region, 2);

    if (enemy_near > 0) {
      score += enemy_near * 5000.0;
    }

    if (score < best_score) {
      best_score = score;
      best_region = b.region;
    }
  }

  return best_region;
}

static int choose_claimable_pressure_site(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx) {
  int best_region = -1;
  double best_score = std::numeric_limits<double>::infinity();

  for (int r : M.strongholds) {
    if (has_building(S, r)) {
      continue;
    }

    if (ctx.used_upgrade_region[r]) {
      continue;
    }

    if (ctx.planned_arrive[r] > 0) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), r) > 0) {
      continue;
    }

    bool contested = P.dist[M.opp_hq][r] <= P.dist[M.my_hq][r] * 1.35;

    if (!contested) {
      continue;
    }

    double score = dist_to_nearest_my_building(S, M, P, r);

    if (is_enemy_side_region(M, P, r)) {
      score *= 0.7;
    }

    if (score < best_score) {
      best_score = score;
      best_region = r;
    }
  }

  return best_region;
}

static int find_best_claim_unit(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int target) {
  int best_idx = -1;
  double best_score = std::numeric_limits<double>::infinity();

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    const Warrior &w = S.warriors[i];

    if (ctx.used_warrior[i]) {
      continue;
    }

    if (w.id.side != M.my_side) {
      continue;
    }

    if (w.state != WState::STATIONARY) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), w.region) > 0) {
      continue;
    }

    if (!can_take_warrior_from_region(S, M, ctx, w.region)) {
      continue;
    }

    double score = P.dist[w.region][target];

    if (is_enemy_side_region(M, P, w.region)) {
      score *= 0.5;
    }

    if (score < best_score) {
      best_score = score;
      best_idx = i;
    }
  }

  return best_idx;
}

static bool claim_pressure_gain(const GameState &S, const GameMap &M, const Paths &P, ActionContext &ctx) {
  build_bases_under_workers(S, M, ctx);

  if (has_any_action(ctx)) {
    return true;
  }

  int target = choose_claimable_pressure_site(S, M, P, ctx);

  if (target == -1) {
    return false;
  }

  int warrior_idx = find_best_claim_unit(S, M, P, ctx, target);

  if (warrior_idx == -1) {
    return false;
  }

  return add_move(ctx, S, M, warrior_idx, target);
}

static bool can_use_warrior_for_finish(const GameState &S, const GameMap &M, const ActionContext &ctx, int warrior_index, int hq_guard) {
  if (warrior_index < 0 || warrior_index >= (int)S.warriors.size()) {
    return false;
  }

  const Warrior &w = S.warriors[warrior_index];

  if (ctx.used_warrior[warrior_index]) {
    return false;
  }

  if (w.id.side != M.my_side) {
    return false;
  }

  if (w.state != WState::STATIONARY) {
    return false;
  }

  if (is_unbuilt_stronghold(S, M, w.region)) {
    return false;
  }

  if (count_warriors_at(S, opposite(M.my_side), w.region) > 0) {
    return false;
  }

  const Building *b = find_building(S, w.region);

  if (b != nullptr && b->side == M.my_side && b->type == BType::HQ) {
    int here = planned_my_workers_at(S, M, ctx, w.region);

    if (here <= hq_guard) {
      return false;
    }
  }

  return true;
}

static int choose_stage_for_target(const GameState &S, const GameMap &M, const Paths &P, int target) {
  int best_region = M.my_hq;
  double best_score = P.dist[M.my_hq][target] + 100000.0;

  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    if (b.type != BType::BASE) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), b.region) > 0) {
      continue;
    }

    double score = P.dist[b.region][target];

    int enemy_near = count_enemy_within_hops(S, M, P, b.region, 2);

    if (enemy_near > 0) {
      score += enemy_near * 5000.0;
    }

    if (score < best_score) {
      best_score = score;
      best_region = b.region;
    }
  }

  return best_region;
}

static bool is_snowball_attack_pool_unit(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int warrior_index, int stage, int target, int hq_guard) {
  if (!can_use_warrior_for_finish(S, M, ctx, warrior_index, hq_guard)) {
    return false;
  }

  const Warrior &w = S.warriors[warrior_index];

  if (w.region == target) {
    return false;
  }

  double stage_to_target = P.dist[stage][target];
  double unit_to_target = P.dist[w.region][target];

  if (w.region != stage && unit_to_target > stage_to_target) {
    return false;
  }

  return true;
}

static int count_snowball_attack_pool(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int target, int hq_guard) {
  int cnt = 0;

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    if (is_snowball_attack_pool_unit(S, M, P, ctx, i, stage, target, hq_guard)) {
      ++cnt;
    }
  }

  return cnt;
}

static int find_snowball_attack_pool_unit(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int target, int hq_guard) {
  int best_idx = -1;
  double best_dist = std::numeric_limits<double>::infinity();

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    if (!is_snowball_attack_pool_unit(S, M, P, ctx, i, stage, target, hq_guard)) {
      continue;
    }

    const Warrior &w = S.warriors[i];
    double d = P.dist[w.region][target];

    if (d < best_dist) {
      best_dist = d;
      best_idx = i;
    }
  }

  return best_idx;
}

static int find_best_snowball_gatherer(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int target, int hq_guard) {
  int best_idx = -1;
  double best_dist = std::numeric_limits<double>::infinity();
  double stage_to_target = P.dist[stage][target];

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    const Warrior &w = S.warriors[i];

    if (!can_use_warrior_for_finish(S, M, ctx, i, hq_guard)) {
      continue;
    }

    if (w.region == stage) {
      continue;
    }

    if (w.region == target) {
      continue;
    }

    if (P.dist[w.region][target] < stage_to_target) {
      continue;
    }

    double d = P.dist[w.region][stage];

    if (d < best_dist) {
      best_dist = d;
      best_idx = i;
    }
  }

  return best_idx;
}

static int find_best_finish_gatherer(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int hq_guard) {
  int best_idx = -1;
  double best_dist = std::numeric_limits<double>::infinity();
  double stage_to_enemy_hq = P.dist[stage][M.opp_hq];

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    const Warrior &w = S.warriors[i];

    if (!can_use_warrior_for_finish(S, M, ctx, i, hq_guard)) {
      continue;
    }

    if (w.region == stage) {
      continue;
    }

    if (w.region == M.opp_hq) {
      continue;
    }

    if (P.dist[w.region][M.opp_hq] < stage_to_enemy_hq) {
      continue;
    }

    double d = P.dist[w.region][stage];

    if (d < best_dist) {
      best_dist = d;
      best_idx = i;
    }
  }

  return best_idx;
}

static int affordable_enemy_hq_moves(const ActionContext &ctx) {
  return ctx.budget / MOVE_COST;
}

static int enemy_hq_required_attackers_light(const GameState &S, const GameMap &M, const Paths &P) {
  const Building *hq = find_building(S, M.opp_hq);

  int level = 1;
  int hp = HQ_LEVELS[1].hp;
  int turret = HQ_LEVELS[1].turret;

  if (hq != nullptr) {
    level = hq->level;
    hp = hq->hp;
    turret = HQ_LEVELS[level].turret;
  }

  int enemy_here = count_warriors_at(S, opposite(M.my_side), M.opp_hq);
  int enemy_near = count_enemy_within_hops(S, M, P, M.opp_hq, 1);
  int support = std::max(0, enemy_near - enemy_here);

  int required = 0;
  required += enemy_here;
  required += (support + 2) / 3;
  required += (hp + 4) / 5;
  required += turret;
  required += 2;

  if (enemy_here == 0 && support == 0) {
    required -= 1;
  }

  if (level <= 2) {
    required -= 1;
  }

  return std::max(6, required);
}

static bool should_launch_finish_now(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int stage, int hq_guard, int turn) {
  int ready = count_finish_attack_pool(S, M, P, ctx, stage, hq_guard);
  int affordable = affordable_enemy_hq_moves(ctx);
  int can_send = std::min(ready, affordable);

  int required = enemy_hq_required_attackers_light(S, M, P);
  int turns_left = MAX_TURN - turn;
  int stage_to_hq = path_hops(P, stage, M.opp_hq);

  if (can_send <= 0) {
    return false;
  }

  if (turns_left <= stage_to_hq + 3) {
    return can_send >= std::max(5, required - 3);
  }

  return can_send >= required;
}



static void finish_enemy_hq_grouped(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, int hq_guard, int surplus, ActionContext &ctx) {
  (void)my;

  int stage = choose_finish_stage(S, M, P);

  if (should_launch_finish_now(S, M, P, ctx, stage, hq_guard, turn)) {
    int ready = count_finish_attack_pool(S, M, P, ctx, stage, hq_guard);
    int affordable = affordable_enemy_hq_moves(ctx);
    int send_limit = std::min(ready, affordable);

    int sent = 0;

    while (sent < send_limit) {
      int warrior_idx = find_finish_attack_pool_unit(S, M, P, ctx, stage, hq_guard);

      if (warrior_idx == -1) {
        break;
      }

      if (!add_move(ctx, S, M, warrior_idx, M.opp_hq)) {
        break;
      }

      ++sent;
    }

    return;
  }

  int gather_limit = std::min(40, surplus);
  int gathered = 0;

  while (gathered < gather_limit) {
    int warrior_idx = find_best_finish_gatherer(S, M, P, ctx, stage, hq_guard);

    if (warrior_idx == -1) {
      break;
    }

    if (!add_move(ctx, S, M, warrior_idx, stage)) {
      break;
    }

    ++gathered;
  }
}

static int required_hq_guard(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn) {
  int guard = 2;

  int enemy_here = count_warriors_at(S, opposite(M.my_side), M.my_hq);
  int past_half = enemy_main_path_hq_pressure_count(S, M, P, 1, 2);
  int past_two_thirds = enemy_main_path_hq_pressure_count(S, M, P, 2, 3);

  if (turn <= 50) {
    guard = 3;
  }

  if (past_half >= 5) {
    guard = std::max(guard, 4);
  }

  if (past_half >= 8) {
    guard = std::max(guard, 7);
  }

  if (past_two_thirds >= 6) {
    guard = std::max(guard, 9);
  }

  if (enemy_here > 0) {
    guard = std::max(guard, enemy_here + 5);
  }

  return std::min(guard, my.warriors);
}

static void attack_enemy(const GameState &S, const GameMap &M, const Paths &P, const Plan &plan, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx) {
  (void)plan;

  if (hq_under_attack_now(S, M, P)) {
    return;
  }

  bool rush_incoming = enemy_rush_incoming(S, M, P, turn);
  bool hard_rush = enemy_rush_committed_or_close(S, M, P);
  bool finish_prepare = should_prepare_finish_simple(S, M, P, my, enemy, turn);
  bool siege = should_siege_enemy_bases(S, M, P, my, enemy, turn);

  if (hard_rush) {
    return;
  }

  int hq_guard = required_hq_guard(S, M, P, my, turn);
  int surplus = std::max(0, my.warriors - my.workers - hq_guard);

  if (surplus <= 0) {
    return;
  }

  if (finish_prepare) {
    finish_enemy_hq_grouped(S, M, P, my, turn, hq_guard, surplus, ctx);
    return;
  }

  if (siege) {
    siege_enemy_base_grouped(S, M, P, my, turn, hq_guard, surplus, ctx);
    return;
  }

  if (should_end_opening_pressure(S, M, P, my, enemy, turn, ctx)) {
    return;
  }

  if (!pressure_budget_left(S, M, P, my, enemy)) {
    return;
  }

  int target = choose_pressure_target(S, M, P);

  if (target == -1) {
    return;
  }

  if (target == M.opp_hq) {
    return;
  }

  int send_limit = 2;

  if (!rush_incoming && turn >= 80 && surplus >= 8) {
    send_limit = 3;
  }

  if (rush_incoming) {
    send_limit = 1;
  }

  int already_committed = count_warriors_at(S, M.my_side, target) +
                          my_reinforcements_to_region(S, M, target) +
                          ctx.planned_arrive[target];
  int target_pressure_cap = (!rush_incoming && turn >= 80 && surplus >= 8) ? 4 : 2;

  if (already_committed >= target_pressure_cap) {
    return;
  }

  send_limit = std::min(send_limit, target_pressure_cap - already_committed);
  send_limit = std::min(send_limit, surplus);

  int sent = 0;

  while (sent < send_limit) {
    int warrior_idx = find_best_pressure_attacker(S, M, P, ctx, target, hq_guard);

    if (warrior_idx == -1) {
      break;
    }

    if (!add_move(ctx, S, M, warrior_idx, target)) {
      break;
    }

    ++sent;
  }
}

static bool already_targeted_region(const GameState &S, const GameMap &M, int region) {
  for (const auto &w : S.warriors) {
    if (w.id.side != M.my_side) {
      continue;
    }

    if (w.region == region) {
      return true;
    }

    if (w.state == WState::MOVING && w.target == region) {
      return true;
    }
  }

  return false;
}

static double dist_to_nearest_my_building(const GameState &S, const GameMap &M, const Paths &P, int region) {
  double best = std::numeric_limits<double>::infinity();

  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    best = std::min(best, P.dist[b.region][region]);
  }

  return best;
}

static bool is_fast_safe_expansion_candidate(const GameState &S, const GameMap &M, const Paths &P, int region) {
  if (!is_unbuilt_stronghold(S, M, region)) {
    return false;
  }

  if (count_warriors_at(S, M.my_side, region) > 0) {
    return false;
  }

  if (count_warriors_at(S, opposite(M.my_side), region) > 0) {
    return false;
  }

  double my_dist = P.dist[M.my_hq][region];
  double enemy_dist = P.dist[M.opp_hq][region];
  double nearest_my = dist_to_nearest_my_building(S, M, P, region);

  if (my_dist > enemy_dist * 1.10) {
    return false;
  }

  if (nearest_my > my_dist + 2500.0) {
    return false;
  }

  return true;
}

static bool has_fast_expansion_opportunity(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx) {
  return count_fast_expansion_opportunities(S, M, P, ctx) > 0;
}

static int count_fast_expansion_opportunities(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx) {
  int cnt = 0;

  for (int r : M.strongholds) {
    if (!is_fast_safe_expansion_candidate(S, M, P, r)) {
      continue;
    }

    if (ctx.used_upgrade_region[r]) {
      continue;
    }

    if (already_targeted_region(S, M, r)) {
      continue;
    }

    if (ctx.planned_arrive[r] > 0) {
      continue;
    }

    ++cnt;
  }

  return cnt;
}

static int choose_fast_expansion_target(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx) {
  int best_region = -1;
  double best_score = std::numeric_limits<double>::infinity();

  for (int r : M.strongholds) {
    if (!is_fast_safe_expansion_candidate(S, M, P, r)) {
      continue;
    }

    if (ctx.used_upgrade_region[r]) {
      continue;
    }

    if (already_targeted_region(S, M, r)) {
      continue;
    }

    if (ctx.planned_arrive[r] > 0) {
      continue;
    }

    double score = expansion_score(S, M, P, r);

    if (score < best_score) {
      best_score = score;
      best_region = r;
    }
  }

  return best_region;
}

static double expansion_score(const GameState &S, const GameMap &M, const Paths &P, int region) {
  double my_hq_dist = P.dist[M.my_hq][region];
  double opp_hq_dist = P.dist[M.opp_hq][region];
  double nearest_my = dist_to_nearest_my_building(S, M, P, region);

  double score = 0.0;

  score += my_hq_dist * 0.6;
  score += nearest_my * 0.8;

  if (my_hq_dist < opp_hq_dist) {
    score -= 1500.0;
  } else {
    score += (my_hq_dist - opp_hq_dist) * 0.8;
  }

  double center_balance = std::abs(my_hq_dist - opp_hq_dist);
  score -= std::max(0.0, 3000.0 - center_balance) * 0.15;

  return score;
}

static int find_best_expander(const GameState &S, const GameMap &M, const Paths &P, const ActionContext &ctx, int target) {
  int best_idx = -1;
  double best_dist = std::numeric_limits<double>::infinity();

  for (int i = 0; i < (int)S.warriors.size(); ++i) {
    const Warrior &w = S.warriors[i];

    if (ctx.used_warrior[i]) {
      continue;
    }

    if (w.id.side != M.my_side) {
      continue;
    }

    if (w.state != WState::STATIONARY) {
      continue;
    }

    if (w.region == target) {
      continue;
    }
    
    if (is_unbuilt_stronghold(S, M, w.region)) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), w.region) > 0) {
      continue;
    }

    if (!can_take_warrior_from_region(S, M, ctx, w.region)) {
      continue;
    }

    double d = P.dist[w.region][target];

    if (d < best_dist) {
      best_dist = d;
      best_idx = i;
    }
  }

  return best_idx;
}

static int opening_expansion_pending_limit(const GameState &S, const GameMap &M, const MyStatus &my, const ActionContext &ctx) {
  (void)S;

  int desired = desired_base_count_by_economy(M, my);
  int pending = count_pending_base_sites(S, M, ctx);
  int owned_or_pending = my.bases + pending;
  int remaining = std::max(0, desired - owned_or_pending);

  int limit = 1;

  if (my.warriors >= my.work_cap + 3) {
    limit = 2;
  }

  if (my.warriors >= my.work_cap + 6) {
    limit = 3;
  }

  if (my.workers >= std::max(0, my.work_cap - 1) && my.warriors >= my.work_cap + 4) {
    limit = std::max(limit, 3);
  }

  if (my.gold >= BASE_LEVELS[1].cost * 2) {
    limit = std::max(limit, 2);
  }

  return std::max(1, std::min(limit, remaining + pending));
}
static bool should_opening_expansion_phase(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx) {
  (void)enemy;
  (void)turn;

  int home_candidates = count_home_expansion_candidates(S, M, P, ctx);
  int fast_candidates = count_fast_expansion_opportunities(S, M, P, ctx);
  int pending = count_pending_base_sites(S, M, ctx);
  int limit = opening_expansion_pending_limit(S, M, my, ctx);

  int target = early_base_target_count(M);
  int owned_or_pending = my.bases + pending;

  if (pending > 0 && my.bases < target) {
    return true;
  }

  if (owned_or_pending < target && (home_candidates > 0 || fast_candidates > 0)) {
    return true;
  }

  if ((home_candidates > 0 || fast_candidates > 0) && pending < limit) {
    return true;
  }

  return false;
}

static int opening_expansion_train_target(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, const ActionContext &ctx) {
  int candidates = std::max(
    count_home_expansion_candidates(S, M, P, ctx),
    count_fast_expansion_opportunities(S, M, P, ctx)
  );

  int pending = count_pending_base_sites(S, M, ctx);
  int limit = opening_expansion_pending_limit(S, M, my, ctx);

  int wanted_pending = std::min(limit, candidates);
  int expander_shortage = std::max(0, wanted_pending - pending);

  int target = my.work_cap + expander_shortage + 1;
  target = std::max(target, START_WARRIORS);

  return target;
}

static bool has_ready_base_builder(const GameState &S, const GameMap &M, const ActionContext &ctx) {
  for (int r : M.strongholds) {
    if (has_building(S, r)) {
      continue;
    }

    if (ctx.used_upgrade_region[r]) {
      continue;
    }

    if (count_warriors_at(S, opposite(M.my_side), r) > 0) {
      continue;
    }

    int here = count_stationary_warriors_at(S, M.my_side, r);
    here -= ctx.planned_leave[r];

    if (here > 0) {
      return true;
    }
  }

  return false;
}

static void train_for_opening_expansion(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx) {
  int target = opening_expansion_train_target(S, M, P, my, turn, ctx);
  int current = my.warriors + ctx.a.train_n;
  int cap = remaining_train_cap(my, ctx);

  if (has_ready_base_builder(S, M, ctx) && ctx.budget < BASE_LEVELS[1].cost) {
    return;
  }

  while (cap > 0 && current < target && ctx.budget >= TRAIN_COST) {
    if (!add_train(ctx, 1)) {
      break;
    }

    --cap;
    ++current;
  }
}

static void send_expanders(const GameState &S, const GameMap &M, const Paths &P, const Plan &plan, const MyStatus &my, int turn, ActionContext &ctx) {
  (void)plan;
  (void)my;

  int pending = count_pending_base_sites(S, M, ctx);

  int max_pending = opening_expansion_pending_limit(S, M, my, ctx);

  while (pending < max_pending) {
    if (ctx.budget < MOVE_COST) {
      break;
    }

    int target = choose_fast_expansion_target(S, M, P, ctx);

    if (target == -1) {
      target = choose_home_expansion_target(S, M, P, ctx);
    }

    if (target == -1) {
      break;
    }

    int warrior_idx = find_best_expander(S, M, P, ctx, target);

    if (warrior_idx == -1) {
      break;
    }

    if (!add_move(ctx, S, M, warrior_idx, target)) {
      break;
    }

    ++pending;
  }
}

static void do_opening_expansion(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx) {
  build_bases_under_workers(S, M, ctx);

  if (has_ready_base_builder(S, M, ctx) && ctx.budget < BASE_LEVELS[1].cost) {
    return;
  }

  send_expanders(S, M, P, Plan{}, my, turn, ctx);

  if (has_ready_base_builder(S, M, ctx) && ctx.budget < BASE_LEVELS[1].cost) {
    return;
  }

  train_for_opening_expansion(S, M, P, my, turn, ctx);
  fill_workers(S, M, P, ctx);
}

static void do_opening_pressure(const GameState &S, const GameMap &M, const Paths &P,
                                const Plan &plan, const MyStatus &my,
                                const EnemyStatus &enemy, int turn, ActionContext &ctx) {
  if (claim_pressure_gain(S, M, P, ctx)) {
    return;
  }

  attack_enemy(S, M, P, plan, my, enemy, turn, ctx);

  if (!has_any_action(ctx)) {
    if (use_surplus_hq_army(S, M, P, my, enemy, turn, ctx)) {
      return;
    }
  }

  if (!has_any_action(ctx)) {
    train_for_opening_pressure(S, M, plan, my, enemy, ctx, 0);
  }

  if (!has_any_action(ctx)) {
    fill_workers(S, M, P, ctx);
  }
}

static bool should_end_opening_pressure(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, const ActionContext &ctx) {
  int score = advantage_score(S, M, my, enemy);

  int my_income = economy_income_power_side(S, M.my_side);
  int enemy_income = economy_income_power_side(S, opposite(M.my_side));

  bool workers_ready = my.workers >= std::max(0, my.work_cap - 1);
  bool base_ready = my.bases >= 3;
  bool target_bases_ready = my.bases >= early_base_target_count(M);
  bool pressure_available = pressure_budget_left(S, M, P, my, enemy);
  bool hq_lock = should_lock_hq_growth_upgrade(S, M, my);
  bool enemy_skipping_upgrades = enemy_is_skipping_core_upgrades(S, M, my, enemy);

  if (turn >= 70 && enemy_skipping_upgrades && pressure_available && !hq_lock && !has_decisive_growth_lead(S, M, my, enemy)) {
    return false;
  }

  if (score >= 8) {
    return true;
  }

  if (target_bases_ready && workers_ready) {
    return true;
  }

  if (base_ready && workers_ready && my_income >= enemy_income) {
    return true;
  }

  if (my.bases >= enemy.bases + 1 && my_income >= enemy_income) {
    return true;
  }

  if (base_ready && workers_ready && my.warriors >= enemy.warriors) {
    return true;
  }

  return false;
}

static void do_gold_snowball(const GameState &S, const GameMap &M, const Paths &P,
                             const Plan &plan, const MyStatus &my,
                             const EnemyStatus &enemy, int turn, ActionContext &ctx) {
  if (claim_pressure_gain(S, M, P, ctx)) {
    return;
  }

  build_bases_under_workers(S, M, ctx);
  if (has_any_action(ctx)) {
    return;
  }

  fill_workers(S, M, P, ctx);
  if (has_any_action(ctx)) {
    return;
  }

  upgrade_for_growth_snowball(S, M, P, my, turn, ctx);
  if (has_any_action(ctx)) {
    return;
  }

  if (growth_attack_enemy_base(S, M, P, my, enemy, turn, ctx)) {
    return;
  }

  if (use_growth_surplus_hq_army(S, M, P, my, enemy, turn, ctx)) {
    return;
  }

  int reserve = operation_gold_reserve(S, M, P, my, enemy, ctx, turn);

  if (!has_growth_upgrade_debt(S, M, my)) {
    train_to_keep_army_lead(S, M, plan, my, enemy, ctx, reserve);
  }

  if (!has_any_action(ctx) && !has_growth_upgrade_debt(S, M, my)) {
    train_units(S, M, plan, my, turn, ctx);
  }
}

static int choose_growth_surplus_target(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, const ActionContext &ctx, int turn) {
  (void)my;
  (void)enemy;
  (void)turn;

  int defense_target = choose_primary_defense_target(S, M, P);

  if (defense_target != -1 && defense_target != M.my_hq) {
    const Building *b = find_building(S, defense_target);

    if (b != nullptr && b->side == M.my_side) {
      int enemy_pressure = count_warriors_at(S, opposite(M.my_side), defense_target) + moving_enemy_targeting_region_count(S, M, defense_target);
      int committed = committed_my_units_to_region(S, M, ctx, defense_target);

      if (enemy_pressure > 0 && committed < enemy_pressure + 1) {
        return defense_target;
      }
    }
  }

  return -1;
}

static bool use_growth_surplus_hq_army(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx) {
  int target = choose_growth_surplus_target(S, M, P, my, enemy, ctx, turn);

  if (target == -1) {
    return false;
  }

  int move_budget = ctx.budget / MOVE_COST;

  if (move_budget <= 0) {
    return false;
  }

  int enemy_pressure = count_warriors_at(S, opposite(M.my_side), target) + moving_enemy_targeting_region_count(S, M, target);
  int committed = committed_my_units_to_region(S, M, ctx, target);
  int target_cap = std::max(1, enemy_pressure + 1);
  int can_send = std::min(move_budget, target_cap - committed);

  if (can_send <= 0) {
    return false;
  }

  int sent = 0;

  while (sent < can_send) {
    int warrior_idx = find_surplus_hq_unit(S, M, P, my, ctx, turn);

    if (warrior_idx == -1) {
      break;
    }

    if (!add_move(ctx, S, M, warrior_idx, target)) {
      break;
    }

    ++sent;
  }

  return sent > 0;
}

static bool newly_trained_unit_can_matter(const GameState &S, const GameMap &M, const Paths &P, int turn) {
  int stage = choose_finish_stage(S, M, P);
  int from_hq_to_stage = path_hops(P, M.my_hq, stage);
  int stage_to_enemy_hq = path_hops(P, stage, M.opp_hq);
  int turns_left = MAX_TURN - turn;

  return from_hq_to_stage + stage_to_enemy_hq + 3 <= turns_left;
}

static void train_units(const GameState &S, const GameMap &M, const Plan &plan, const MyStatus &my, int turn, ActionContext &ctx) {
  const Building *hq = find_building(S, M.my_hq);

  if (hq == nullptr) {
    return;
  }

  if (should_hard_reserve_for_expansion(S, M, my, ctx)) {
    if (ctx.budget < BASE_LEVELS[1].cost + TRAIN_COST) {
      return;
    }
  }

  int expected_warriors = my.warriors + ctx.a.train_n;
  int desired_warriors = plan.desired_warriors;

  if (expected_warriors >= desired_warriors) {
    return;
  }

  int reserve = expansion_gold_reserve(S, M, my, ctx, turn);

  int shortage = desired_warriors - expected_warriors;
  int max_train = std::min(remaining_train_cap(my, ctx), shortage);

  while (max_train > 0 && ctx.budget - reserve >= TRAIN_COST) {
    add_train(ctx, 1);
    --max_train;
  }
}

static void fill_workers(const GameState &S, const GameMap &M, const Paths &P, ActionContext &ctx) {
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) {
      continue;
    }

    // HQ가 아닌 전방 기지가 적에게 맞고 있거나 바로 위협받는 중이면
    // 경제 worker 보충을 하지 않는다. 이 상황은 별도 전선 방어 로직에서 다뤄야 한다.
    if (b.type == BType::BASE) {
      if (count_warriors_at(S, opposite(M.my_side), b.region) > 0) {
        continue;
      }

      if (moving_enemy_targeting_region_count(S, M, b.region) > 0) {
        continue;
      }

      if (moving_enemy_within_hops(S, M, P, b.region, 1) > 0) {
        continue;
      }
    }

    int need = worker_shortage_at(S, M, ctx, b);

    while (need > 0) {
      int warrior_idx = find_best_worker_for_building(S, M, P, ctx, b);

      if (warrior_idx == -1) {
        break;
      }

      if (!add_move(ctx, S, M, warrior_idx, b.region)) {
        break;
      }

      --need;
    }
  }
}

static bool has_any_action(const ActionContext &ctx) {
  return ctx.a.train_n > 0 || !ctx.a.moves.empty() || !ctx.a.upgrades.empty();
}

static int enemy_final_hq_rush_count(const GameState &S, const GameMap &M, const Paths &P) {
  int cnt = 0;
  Side enemy_side = opposite(M.my_side);

  for (const auto &w : S.warriors) {
    if (w.id.side != enemy_side) {
      continue;
    }

    if (w.region == M.my_hq) {
      cnt += 3;
      continue;
    }

    if (w.state == WState::MOVING && w.target == M.my_hq) {
      cnt += 2;
      continue;
    }

    int hops = path_hops(P, w.region, M.my_hq);

    if (hops <= 2) {
      cnt += 2;
      continue;
    }

    if (hops <= 4 && P.dist[w.region][M.my_hq] < P.dist[w.region][M.opp_hq]) {
      cnt += 1;
      continue;
    }
  }

  return cnt;
}

static int my_hq_required_defenders_light(const GameState &S, const GameMap &M, const Paths &P) {
  (void)P;

  const Building *hq = find_building(S, M.my_hq);

  int level = 1;
  int hp = HQ_LEVELS[1].hp;
  int turret = HQ_LEVELS[1].turret;

  if (hq != nullptr) {
    level = hq->level;
    hp = hq->hp;
    turret = HQ_LEVELS[level].turret;
  }

  int enemy_here = count_warriors_at(S, opposite(M.my_side), M.my_hq);
  int my_here = count_warriors_at(S, M.my_side, M.my_hq);

  int required = 0;
  required += enemy_here;
  required += (hp + 4) / 5;
  required += turret;
  required += 2;

  required -= my_here / 2;

  if (level >= 4) {
    required -= 1;
  }

  return std::max(5, required);
}

static bool enemy_is_final_hq_rushing(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy) {
  int enemy_total = enemy.warriors;
  int committed_direct = enemy_committed_to_my_hq_count(S, M);
  int enemy_here = count_warriors_at(S, opposite(M.my_side), M.my_hq);

  bool my_economy_mature =
      my.bases >= early_base_target_count(M) &&
      my.workers >= std::max(0, my.work_cap - 1) &&
      my.hq_level >= 4;

  if (!my_economy_mature) {
    return false;
  }

  if (enemy_here >= 3) {
    return true;
  }

  if (enemy_total >= 10 &&
      committed_direct >= std::max(8, enemy_total * 3 / 4)) {
    return true;
  }

  return false;
}

static int enemy_final_hq_rush_eta(const GameState &S, const GameMap &M, const Paths &P) {
  int best = 1000000;
  Side enemy_side = opposite(M.my_side);

  for (const auto &w : S.warriors) {
    if (w.id.side != enemy_side) {
      continue;
    }

    if (w.region == M.my_hq) {
      return 0;
    }

    if (w.state == WState::MOVING && w.target == M.my_hq) {
      best = std::min(best, path_hops(P, w.region, M.my_hq));
      continue;
    }

    if (P.dist[w.region][M.my_hq] < P.dist[w.region][M.opp_hq]) {
      best = std::min(best, path_hops(P, w.region, M.my_hq));
      continue;
    }
  }

  return best;
}

static bool can_win_hq_race_now(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, const ActionContext &ctx, int turn) {
  (void)enemy;

  int hq_guard = required_hq_guard(S, M, P, my, turn);
  int stage = choose_finish_stage(S, M, P);

  int ready = count_finish_attack_pool(S, M, P, ctx, stage, hq_guard);
  int affordable = ctx.budget / MOVE_COST;
  int can_send = std::min(ready, affordable);

  int required = enemy_hq_required_attackers_light(S, M, P);

  if (can_send < required) {
    return false;
  }

  int my_eta = path_hops(P, stage, M.opp_hq);
  int enemy_eta = enemy_final_hq_rush_eta(S, M, P);

  if (enemy_eta == 1000000) {
    return true;
  }

  return my_eta <= enemy_eta + 1;
}

static bool launch_final_hq_race(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx) {
  int hq_guard = required_hq_guard(S, M, P, my, turn);
  int stage = choose_finish_stage(S, M, P);

  int required = enemy_hq_required_attackers_light(S, M, P);
  int ready = count_finish_attack_pool(S, M, P, ctx, stage, hq_guard);
  int affordable = ctx.budget / MOVE_COST;
  int send_limit = std::min(ready, affordable);

  if (send_limit < required) {
    return false;
  }

  send_limit = std::min(send_limit, required + 4);

  int sent = 0;

  while (sent < send_limit) {
    int warrior_idx = find_finish_attack_pool_unit(S, M, P, ctx, stage, hq_guard);

    if (warrior_idx == -1) {
      break;
    }

    if (!add_move(ctx, S, M, warrior_idx, M.opp_hq)) {
      break;
    }

    ++sent;
  }

  return sent > 0;
}

static void defend_final_hq_race(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, int turn, ActionContext &ctx) {
  defend_hq_for_incoming_rush(S, M, P, my, turn, ctx);
  recall_idle_units_to_hq_for_rush(S, M, ctx);

  int cap = remaining_train_cap(my, ctx);

  while (cap > 0 && ctx.budget >= TRAIN_COST) {
    if (!add_train(ctx, 1)) {
      break;
    }

    --cap;
  }
}

static bool handle_final_hq_race(const GameState &S, const GameMap &M, const Paths &P, const MyStatus &my, const EnemyStatus &enemy, int turn, ActionContext &ctx) {
  if (!enemy_is_final_hq_rushing(S, M, P, my, enemy)) {
    return false;
  }

  if (can_win_hq_race_now(S, M, P, my, enemy, ctx, turn)) {
    if (launch_final_hq_race(S, M, P, my, turn, ctx)) {
      return true;
    }
  }

  defend_final_hq_race(S, M, P, my, turn, ctx);
  return true;
}

static Actions decide(const GameState &S, const GameMap &M, const Paths &P, int turn) {
  ActionContext ctx(S, M);

  MyStatus my = analyze_my_status(S, M);
  EnemyStatus enemy = analyze_enemy_status(S, M);
  Plan plan = make_plan(S, M, P, turn);

  bool final_hq_race = handle_final_hq_race(S, M, P, my, enemy, turn, ctx);

  if (final_hq_race) {
    return ctx.a;
  }

  bool immediate_hq_danger = hq_under_attack_now(S, M, P);
  bool hard_rush = enemy_rush_committed_or_close(S, M, P);

  if (immediate_hq_danger || hard_rush) {
    defend_hq_for_incoming_rush(S, M, P, my, turn, ctx);
    recall_idle_units_to_hq_for_rush(S, M, ctx);
    emergency_train_for_defense(S, M, P, my, turn, ctx);
    train_units(S, M, plan, my, turn, ctx);

    if (has_any_action(ctx)) {
      return ctx.a;
    }
  }

  bool expansion_phase = should_opening_expansion_phase(S, M, P, my, enemy, turn, ctx);
  bool expansion_priority = should_prioritize_expansion(S, M, P, my, enemy, turn, ctx);

  if (expansion_phase || expansion_priority) {
    do_opening_expansion(S, M, P, my, turn, ctx);

    if (has_any_action(ctx)) {
      return ctx.a;
    }

    return ctx.a;
  }

  bool late_spend = should_late_spend_mode(S, M, P, my, enemy, turn, ctx);

  if (late_spend) {
    do_late_spend(S, M, P, plan, my, enemy, turn, ctx);

    if (has_any_action(ctx)) {
      return ctx.a;
    }
  }

  bool opening_pressure = should_opening_pressure_mode(S, M, P, my, enemy, turn, ctx);

  if (opening_pressure) {
    do_opening_pressure(S, M, P, plan, my, enemy, turn, ctx);

    if (has_any_action(ctx)) {
      return ctx.a;
    }
  }

  bool gold_growth = should_gold_growth_mode(S, M, P, my, enemy, turn, ctx);

  if (gold_growth || should_end_opening_pressure(S, M, P, my, enemy, turn, ctx)) {
    do_gold_snowball(S, M, P, plan, my, enemy, turn, ctx);

    if (has_any_action(ctx)) {
      return ctx.a;
    }
  }

  bool tactical_attack = should_prepare_finish_simple(S, M, P, my, enemy, turn) ||
                       should_siege_enemy_bases(S, M, P, my, enemy, turn);

  if (tactical_attack) {
    attack_enemy(S, M, P, plan, my, enemy, turn, ctx);

    if (has_any_action(ctx)) {
      return ctx.a;
    }
  }

  build_bases_under_workers(S, M, ctx);
  fill_workers(S, M, P, ctx);
  upgrade_economy(S, M, P, my, turn, ctx);

  bool expansion_priority_after_macro = should_prioritize_expansion(S, M, P, my, enemy, turn, ctx);

  if (!expansion_priority_after_macro && !has_any_action(ctx)) {
    attack_enemy(S, M, P, plan, my, enemy, turn, ctx);
  }

  if (!has_any_action(ctx)) {
    train_units(S, M, plan, my, turn, ctx);
  }

  return ctx.a;
}

int main() {
  using namespace std;

  GameMap M;
  GameState S;
  parse_init(M, S);              // initialize the game
  Paths P = calculate_paths(M); // calculate the shortest paths

  int turn;
  while (read_turn_start(turn)) {
    Actions a = decide(S, M, P, turn);
    emit_command();
    emit_actions(a);
    emit_end();
    read_turn_result(S, M, a);
  }

  return 0;
}
