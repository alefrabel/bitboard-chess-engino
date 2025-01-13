# ♟️ bitboard-chess-engino ♟️

A bitboard-based chess engine featuring alpha-beta pruning and quiescence search. 
The engine follows the UCI (Universal Chess Interface) protocol, making it compatible with popular GUIs like Arena (http://www.playwitharena.de/).

-------------------------------------------------------------------------------
## ♔ TO KNOW
- ⚡Enforces search only up to depth ≤ 8.
- ⏳ Time controls are not yet implemented.
- ♜ Implements alpha-beta pruning and quiescence search.

-------------------------------------------------------------------------------
## ⚙️ COMPILATION

### 🐧 UNIX / LINUX:
1. Ensure you have a C compiler (e.g., GCC).
2. Run: make, make debug or make clean

### 🪟 WINDOWS (MinGW-w64):
1. Install MinGW-w64 (https://www.mingw-w64.org/) or use MSYS2 MinGW.
2. In a terminal:
   ```bash
   mingw32-make
   ```
4. This produces an .exe file supported by Arena on Windows.

### 📝 Note:
- On Debian/Ubuntu, you can install MinGW-w64 by:
```bash
sudo apt-get update
sudo apt-get install mingw-w64
```
Then run make to compile the engine.

-------------------------------------------------------------------------------
## 💻 ENGINE COMMANDS & TERMINAL USAGE

You can also interact with the engine in a terminal by typing UCI commands:

1. **uci**
   - Asks the engine to identify itself and list UCI options (if any).

2. **isready**
   - The engine responds “readyok” once it’s ready.

3. **position startpos**
   - Sets the board to the standard starting position.

4. **position fen <fen_string>**
   - Sets the board to a position described by a FEN string.
   Example:
       position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
   (FEN = Forsyth-Edwards Notation).

5. **moves <move_list>**
   - Applies a list of moves in algebraic notation to the current position.
     Example:
       position startpos moves e2e4 e7e5

6. **go depth <d>**
   - Tells the engine to search for a best move up to depth <d>.
     Example: go depth 6
   - The engine outputs something like: bestmove c7c5

7. **stop**
   - Tells the engine to stop searching immediately.

8. **quit**
   - Exits the engine.

-------------------------------------------------------------------------------
## ♟️ EXAMPLE TERMINAL PLAY-THROUGH

1. **Start the engine:**
   - ./funnyEngine (Linux) 
   - funnyEngine.exe (Windows)

2. **uci**
   - Engine responds with identification, then “uciok”.

3. **isready**
   - Engine responds “readyok” when it’s ready.

4. **position startpos**
   - Sets the starting position.

5. **go depth 5**
   - Engine searches and outputs a best move (ex. e2e4).

6. **position startpos moves e2e4**
   - Makes the move e2e4. You can type startpos moves e2e4 <your move> to make your move and then run “go depth 5” again for the engine’s reply.

Repeat by updating the position with each move. This lets you alternate moves with the engine purely via the terminal.

-------------------------------------------------------------------------------
## 🎮 USAGE WITH ARENA (GUI)

Arena (http://www.playwitharena.de/) is a free, user-friendly chess GUI supporting UCI:

1. Install Arena (Windows or via Wine on Linux).
2. In Arena, add the engine (funnyEngine or funnyEngine.exe) via “Engine → Install New Engine”.
3. Configure and start a new game against the engine.

-------------------------------------------------------------------------------
## 🤝 CONTRIBUTING

- ⏳Add Time Controls: Not yet implemented.
- ♞ Evaluation Enhancements: Expand or refine the evaluation function.
- 🔍 Search Improvements: Add transposition tables, PV extraction, or deeper search heuristics.
- 🐛 Bug Reports: Open issues or pull requests if you see any problems or have ideas.

-------------------------------------------------------------------------------
# 🏁 Have fun playing with funnyEngine! 🏁


