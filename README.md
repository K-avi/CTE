# CTE — C Tablić Engine

[![Language](https://img.shields.io/badge/C-GNU23%20%2F%20C23-blue.svg)](https://en.wikipedia.org/wiki/C23_(C_standard_revision))
[![License](https://img.shields.io/badge/License-GPLv3-green.svg)](LICENSE.txt)
[![Sanitizers](https://img.shields.io/badge/Sanitizers-ASan%20%7C%20UBSan-brightgreen.svg)]()

**CTE** (*C Tablić Engine*) is a small card game engine and interactive player for **Tablić** (a popular Balkan card game, variant of [Tablanette](https://en.wikipedia.org/wiki/Tablanette)).

The project features both **Interactive CLI** and **ncurses TUI** interfaces.

---

## 🃏 What is Tablić?

Tablić is a traditional fishing card game played with a standard 52-card deck:
- **Card Values :** Number cards 2–10 have their face value. Aces count as **1** or **11**. Face cards count as: Jack = **12**, Queen = **13**, King = **14**.
- **Capturing :** A played card can capture any combination of table cards whose sum equals the played card's value, or any **exact partition** of multiple subsets summing to that value in a single move (e.g., King $14$ captures $\{Q(13) + A(1) = 14\}$ and $\{8 + 6 = 14\}$ and $\{9 + 5 = 14\}$ simultaneously).
- **Tablić Bonus :** Clearing all cards from the table earns a **Tablić** (+1 point).
- **Scoring :** 22 card points exist in the deck (Aces, picture cards, tens, plus bonus points for $10\diamondsuit$ and $2\clubsuit$). The player/team with the majority of captured cards ($\ge 27$) receives a **+3 points bonus**.

---

## ✨ Features

- **Universal Player Modes :**
  - **2 Players** (4 deals of 6 cards).
  - **3 Players** (4 deals of 4 cards).
  - **4 Players** (4 deals of 3 cards, individual or **2v2 Team Mode** with aggregated scores and team majority bonuses).

- **AI Evaluators & Tree Search Engine :**
  - `random` : Uniform random legal move selector.
  - `dumb` : Passive drop evaluator.
  - `greedy` : Instant heuristic maximizer (card points + Tablić + captured card count).
  - `cheater` : **Minimax search engine with Alpha-Beta pruning**. This engine cheats !! It reads the cards from your hand.
- **Interfaces :**
  - **Interactive CLI** : Clean tabular dashboards with Unicode ($\spadesuit\heartsuit\diamondsuit\clubsuit$) and ASCII ($S/H/D/C$) rendering styles.
  - **ncurses TUI** : Colorized suit pairs (red for $\heartsuit/\diamondsuit$, white/cyan for $\spadesuit/\clubsuit$), live capture preview on the table, and keyboard navigation.

---

## 🖥️ ncurses TUI Preview

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

## 🚀 Building & Running

### Requirements
- C23/GNU23 compatible compiler (`gcc` or `clang`).
- GNU `make`.
- `ncurses` / `ncursesw` (`libncursesw` for wide UTF-8 support).

### Compilation
```bash
# Build release binaries and test suite:
make all

# Run complete test suite under AddressSanitizer/UBSan:
make run-test
```

### Usage Examples

```bash
# Launch interactive ncurses TUI against the Minimax Cheater bot:
./build/cte -m tui -g h-vs-ai -a cheater

# 3-player game in TUI with mixed AI strategies:
./build/cte -m tui -n 3 -g h-vs-ai -a greedy,cheater

# 4-player 2v2 Team match in TUI:
./build/cte -m tui -n 4 -t -g h-vs-ai -a greedy,cheater,greedy

# Fast CLI headless AI duel (10 deck cycles):
./build/cte -m cli -g ai-vs-ai -a greedy,cheater -c 10 -s ascii
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
  -h, --help                 Display help message and exit
```

---

## 🧪 Architecture & Roadmap

```
cte/
├── include/
│   ├── card.h        # Card lookup tables & formatting
│   ├── player.h      # Player state & won cards tracking
│   ├── move.h        # Combinatorial exact partition DP & move scoring
│   ├── game.h        # Universal dealing (12/P), round lifecycle & team scoring
│   ├── eval.h        # Generic evaluator interface & heuristic evaluators
│   ├── minmax.h      # Compact 64-byte L1 cache snapshot & Alpha-Beta search
│   ├── front_cli.h   # Terminal CLI frontend & observer callbacks
│   ├── front_tui.h   # ncurses interactive TUI frontend
│   └── cte.h         # Umbrella header
├── src/              # Implementation source modules
├── test/             # Unit tests and multi-seed fuzzing suite (T1–T25)
└── main.c            # CLI entry point & POSIX option parser
```

### Upcoming Milestones
- **Backend-Agnostic Abstraction Layer :** Abstract engine interfaces to swap execution backends dynamically.
- **64-bit Bitboard Engine :** Represent 52-card sets in `uint64_t` registers with hardware BMI2 instructions (`POPCNT`, `TZCNT`, `BLSR`).
- **SIMD Vectorization Backend :** Inter-game batch vectorization (AVX2 then maybe MIPPv2 (>ᴗ•) ! ) for massively parallel rollouts.
- **Fair Information-Set AI :** Perfect Information Monte Carlo (PIMC determinization) for non-cheating competitive play.
- **Monte Carlo Tree Search (MCTS) :** Multi-threaded self-play search.

---

## 🤖 AI-Assisted Development Disclosure

In the spirit of scientific and engineering transparency:
This project is developed as a pair-programming exploration leveraging **Generative AI tools** (Google DeepMind Antigravity / Gemini Advanced Agentic Coding). 
The primary objective of this repository is to critically benchmark, stress-test, and evaluate the capabilities of state-of-the-art agentic AI systems in building robust, code in C.

---

## 📄 License

This project is licensed under the **GNU General Public License v3.0** (GPLv3) — see the [LICENSE.txt](LICENSE.txt) file for details.
