# ♟️ bitboard-chess-engino ♟️

A moderately basic bitboard-based chess engine featuring **alpha-beta pruning** and **quiescence search**. The engine follows the **UCI (Universal Chess Interface)** protocol, making it compatible with popular GUIs like [Arena](http://www.playwitharena.de/).

---

## ♔ Current Architectural Limitations and TODOs
- ⚡ Enforces search only up to depth ≤ 8.
- ⏳ Time controls are not yet implemented.
- ♜ Implements alpha-beta pruning and quiescence search.

---

## ⚙️ Compilation

### 🐧 Unix / Linux:
1. Ensure you have a C compiler.
2. Run: `make`

### 🪟 Windows (MinGW-w64):
1. Install [MinGW-w64](https://www.mingw-w64.org/) or use MSYS2 MinGW.
2. In a terminal, run:
   - `mingw32-make`
   - or: `mingw32-make debug`
3. This produces an `.exe` file supported by Arena for Windows.

#### 💡 Note:
- On Debian/Ubuntu, you can install MinGW-w64 by running:
  ```bash
  sudo apt-get update
  sudo apt-get install mingw-w64
```
- then run *make* to compile the engine.
## 🎮 Usage with Arena (GUI)

[Arena](http://www.playwitharena.de/) is a free, user-friendly chess GUI that supports UCI engines:

1. Install Arena on your system (Windows or via Wine on Linux).
2. Add the engine (`funnyEngine` or `funnyEngine.exe`) via:
   **Engine → Install New Engine** in Arena.
3. ♟️ **Play**: Configure settings, select this engine in Arena, and start a new game!

---

## 🤝 Contributing

- 🕒 **Add Time Controls**: Time control code is not yet implemented.
- ♞ **Evaluation Enhancements**: Feel free to expand or refine the evaluation function.
- 🔍 **Search Improvements**: Add transposition tables, PV extraction, or deeper search heuristics.
- 🐛 **Bug Reports**: Open issues or pull requests if you spot any problems or have ideas.

---

### 🏁 Have fun playing with funnyEngine! 🏁

