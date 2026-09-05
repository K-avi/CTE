# CTE — C Tablić Engine

[![Language](https://img.shields.io/badge/C-GNU23%20%2F%20C23-blue.svg)](https://en.wikipedia.org/wiki/C23_(C_standard_revision))
[![License](https://img.shields.io/badge/License-GPLv3-green.svg)](LICENSE.txt)
[![Sanitizers](https://img.shields.io/badge/Sanitizers-ASan%20%7C%20UBSan-brightgreen.svg)]()
[![CI](https://github.com/K-avi/CTE/actions/workflows/ci.yml/badge.svg)](https://github.com/K-avi/CTE/actions/workflows/ci.yml)

**CTE** (*C Tablić Engine*) is a small card game engine and interactive player for **Tablić** (a popular Balkan card game, variant of [Tablanette](https://en.wikipedia.org/wiki/Tablanette)).

The project features both **Interactive CLI** and **ncurses TUI** interfaces.

---

## What is Tablić?

Tablić is a fishing card game popular in the Balkans, played with a standard 52-card deck:
- **Card Values :** Number cards 2–10 have their face value. Aces count as **1** or **11**. Face cards count as: Jack = **12**, Queen = **13**, King = **14**.
- **Capturing :** A played card can capture any combination of table cards whose sum equals the played card's value, or any **exact partition** of multiple subsets summing to that value in a single move (e.g., King $14$ captures $\{Q(13) + A(1) = 14\}$ and $\{8 + 6 = 14\}$ and $\{9 + 5 = 14\}$ simultaneously).
- **Tablić Bonus :** Clearing all cards from the table earns a **Tablić** (+1 point).
- **Scoring :** 22 card points exist in the deck (Aces, picture cards, tens, plus bonus points for $10\diamondsuit$ and $2\clubsuit$). The player/team with the majority of captured cards ($\ge 27$) receives a **+3 points bonus**.

---

## Features

- **Universal Player Modes :**
  - **2 Players** (4 deals of 6 cards).
  - **3 Players** (4 deals of 4 cards).
  - **4 Players** (4 deals of 3 cards, individual or **2v2 Team Mode** with aggregated scores and team majority bonuses).

- **AI Evaluators & Tree Search Engine :**
  - `random` : Uniform random legal move selector.
  - `dumb` : Passive drop evaluator.
  - `greedy` : Instant heuristic maximizer (card points + Tablić + captured card count).
  - `cheater` : **Minimax search engine with Alpha-Beta pruning**. This engine cheats !! It reads the cards from your hand.
- **Multi-Backend Architecture (SWAR bitboard & Array reference) :**
  - **SWAR Bitboard Engine (Default) :** 64-bit board representation using 4-bit SWAR rank pattern matching, 100% L1-resident ($7.1\text{ KB}$ LUT), zero dynamic heap allocations in the search hot path ($5.0\times$ speedup over the reference array implementation).
  - **Array Reference Oracle :** Combinatorial subset-sum partition backtracking used for continuous 2-way differential fuzzing.
  - See [`docs/backends.md`](docs/backends.md) for architectural details and benchmark methodology.
- **Interfaces :**
  - **Interactive CLI** : Clean tabular dashboards with Unicode ($\spadesuit\heartsuit\diamondsuit\clubsuit$) and ASCII ($S/H/D/C$) rendering styles.
  - **ncurses TUI** : Colorized suit pairs (red for $\heartsuit/\diamondsuit$, white/cyan for $\spadesuit/\clubsuit$), live capture preview on the table, and keyboard navigation.

---

## ncurses TUI Preview

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ [CTE - TABLIĆ TUI]  Round: 1  |  Deck: 36 cards left  |  Mode: 2v2 Teams     │
├──────────────────────────────────────────────────────┬───────────────────────┤
│ === PLAYERS & SCORES ===                             │ === TABLE (4 cards) ===
│ > Player 1 (Human) [T1] : 16 cards ( 8 pts, 1 tablic)│ [ 10♦ ] [  J♠ ]       │
│   Bot 1 (Cheater)  [T2] : 11 cards ( 5 pts, 0 tablic)│ [  7♥ ] [  2♣ ]       │
│   Bot 2 (Greedy)   [T1] :  5 cards ( 2 pts, 0 tablic)│                       │
│   Bot 3 (Cheater)  [T2] :  4 cards ( 2 pts, 0 tablic)│                       │
├──────────────────────────────────────────────────────┴───────────────────────┤
│ === Player 1's HAND (4 cards) ===                                            │
│   [ Q♦ ]   [ 10♠ ]   [ 7♣ ]   [ 4♠ ]                                         │
├──────────────────────────────────────────────────────────────────────────────┤
│ === AVAILABLE MOVES (Navigation: [UP/DOWN] - Play: [ENTER] - Quit: [q]) ===  │
│  > [0] Play Q♦ -> Take [ 7♥, 2♣, 4♠ ] -> +2 pts (4 cards) [TABLIC!]          │
│    [1] Play 10♠ -> Take [ 10♦ ]       -> +2 pts (2 cards)                    │
│    [2] Drop 7♣                        -> Drop (0 pt)                         │
├──────────────────────────────────────────────────────────────────────────────┤
│ Last Action : [Bot 3 (Cheater)] played : Play K♣ -> Take [ J♠, 2♣ ]          │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## Building & Running

### Requirements
- C23/GNU23 compatible compiler (`gcc` or `clang`).
- GNU `make`.
- `ncurses` / `ncursesw` (`libncursesw` for wide UTF-8 support).

### Compilation
```bash
# Build release binaries, test suites, and benchmarks:
make all

# Run complete functional test suite (T1–T34):
make run-test

# Run 2-way differential fuzzing suite (BT1–BT4):
make run-test-bitboard

# Run all test suites under AddressSanitizer/UBSan:
make tests

# Run performance benchmark suite:
make run-bench

# Install binary and manpage to system (or custom PREFIX):
sudo make install
```

### Usage Examples

```bash
# Launch interactive ncurses TUI with main menu:
./build/cte

# Play a quick match against the Minimax Cheater bot in TUI:
./build/cte -m tui -g h-vs-ai -a cheater -p "MyProfile"

# 4-player 2v2 Team match in TUI:
./build/cte -m tui -n 4 -t -g h-vs-ai -a greedy,cheater,greedy

# Simulate an 8-participant Round Robin tournament in CLI:
./build/cte -T round-robin -P Alice:human -P Bob:greedy -P Bot1:cheater -P Bot2:random

# Knockout Cup single-elimination tournament to 51 points:
./build/cte -T cup -P Champion:greedy -P Challenger:cheater -w 51

# Consult player Elo leaderboard:
./build/cte -L
```

### Command-Line Options
```text
Options:
  -n, --players <number>     Number of players: 2 (default), 3, or 4
  -t, --team                 Enable 4-player 2v2 team mode (valid only with -n 4)
  -a, --ai-type <list>       AI strategy or comma-separated list (e.g. greedy or greedy,cheater)
                             Supported: random (default), dumb, greedy, cheater
  -m, --mode <mode>          UI mode: cli (default), tui, gui
  -s, --style <style>        Card render style: unicode (default), ascii
  -g, --game <mode>          Game mode: h-vs-ai (default), h-vs-h, ai-vs-ai
  -w, --winning-score <pts>  Target score to win the match (default: 101)
  -c, --rounds <number>      Max number of rounds/deck cycles (default: 0 = unlimited)
  -r, --seed <number>        RNG seed (default: system time)
  -T, --tournament <type>    Run tournament directly: round-robin or cup
  -P, --participant <spec>   Add participant: 'name:type' (human/random/dumb/greedy/cheater)
      --persist-ai           Persist AI bots in profile database (default: transient)
  -p, --profile <name>       Active player profile for tracking statistics and Elo
  -L, --leaderboard          Display player Elo leaderboard and exit
  -h, --help                 Display help message and exit
```

---

## Architecture & Roadmap

```
cte/
├── include/
│   ├── card.h                  # Card lookup tables, formatting & Fisher-Yates shuffle
│   ├── player.h                # Player state & won cards tracking
│   ├── move.h                  # Move validation (is_legal), state transition (play_move) & scoring
│   ├── game.h                  # Game orchestration (init_game, run_round, pos_from_game, cte_set_backend)
│   ├── eval.h                  # Evaluators, default Elo ratings & cte_get_evaluator
│   ├── minmax.h                # Compact 60-byte L1 cache snapshot & Alpha-Beta search
│   ├── engine.h                # Abstract s_cte_engine_backend interface & backend registry
│   ├── backend_bitboard.h      # SWAR Rank Patterns bitboard move generator prototypes
│   ├── bitboard_rank_tables.h  # Compact 7.1 KB rank LUT constants & types
│   ├── tournament.h            # Round Robin & Knockout cup tournament engine
│   ├── profile.h               # XDG atomic binary profile database & Elo rating math
│   ├── front_cli.h             # Terminal CLI frontend & observer callbacks
│   ├── front_tui.h             # ncurses interactive TUI frontend & menu system
│   └── cte.h                   # Master umbrella header
├── src/                        # Implementation source modules
├── test/                       # Functional (T1–T34) & differential bitboard (BT1–BT4) tests
├── tools/                      # Benchmarks & Python table generator
├── docs/                       # Architectural docs (backends.md) & Unix manpage (cte.1)
└── main.c                      # CLI entry point & POSIX option parser
```

### Upcoming Milestones
- **SIMD Vectorization Backend :** Inter-game batch vectorization (GNU vector extensions then maybe MIPPv2 (>ᴗ•) ! ) for parallel rollouts.
- **Fair Information-Set AI :** Perfect Information Monte Carlo (PIMC determinization) for non-cheating competitive play.
- **Monte Carlo Tree Search (MCTS) :** Multi-threaded self-play search.

---

## AI-Assisted Development Disclosure

In the spirit of scientific and engineering transparency:
This project is developed as a pair-programming exploration leveraging **Generative AI tools** (Google DeepMind Antigravity / Gemini Advanced Agentic Coding). 
The primary objective of this repository is to critically benchmark, stress-test, and evaluate the capabilities of state-of-the-art agentic AI systems in building robust, code in C.

---

## License

This project is licensed under the **GNU General Public License v3.0** (GPLv3) — see the [LICENSE.txt](LICENSE.txt) file for details.
