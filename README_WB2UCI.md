# Anubis WB2UCI Bridge

A Python adapter that wraps the Anubis chess engine (WinBoard protocol) so it can be used in any UCI-compatible chess GUI.

## Quick Start

```bash
# Requirements: Python 3.8+, python-chess
pip install --break-system-packages python-chess

# Run with Anubis in same directory (needs .bin weight files alongside)
python3 wb2uci_anubis.py --engine ./Anubis_3-06_AVX2_HT100.exe
```

On Linux, the adapter auto-detects Windows binaries and runs them via Wine.

## Usage with Chess GUIs

Add the adapter as an engine in your UCI GUI:

- **Arena**: Engines → Install New Engine → select `wb2uci_anubis.py`
- **CuteChess**: `python3 wb2uci_anubis.py --engine /path/to/Anubis_3-06_AVX2_HT100.exe`
- **Lucas Chess**: Configure → Engines → New → UCI → select adapter

## Supported UCI Commands

| Command | Status |
|---------|--------|
| `uci` | ✓ |
| `isready` | ✓ |
| `ucinewgame` | ✓ |
| `position startpos` | ✓ |
| `position startpos moves ...` | ✓ (via setboard) |
| `position fen ... moves ...` | ✓ |
| `go depth N` | ✓ (time-based, no native depth limit) |
| `go movetime N` | ✓ |
| `go wtime/btime/winc/binc` | ✓ |
| `go infinite` | ✓ (analysis mode) |
| `stop` | ⚠ (via `?` command, produces error message) |
| `setoption name SyzygyPath` | ✓ |
| `setoption name Hash` | ⚠ (requires engine restart) |
| `setoption name Ponder` | ✓ |
| `quit` | ✓ |

## Anubis Binary Requirements

Place these files in the same directory as the adapter (or the engine path):

- `Anubis_3-06_AVX2_HT100.exe` (or AVX512 variant)
- `net00005_T0.bin` through `net00005_T9.bin` (NNUE weight files)

## Known Limitations

- Anubis doesn't support native depth limits (`sd` command) — `go depth N` uses generous time control instead
- Hash size is compile-time fixed in Anubis (350MB) — cannot be changed at runtime
- PV output uses SAN notation (Anubis native format), not UCI coordinate notation
- The engine's built-in `?` (stop) handler has a missing `continue` statement, causing error messages
- Opening book and EGTB paths use Windows-style paths by default
