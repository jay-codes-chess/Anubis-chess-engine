#!/usr/bin/env python3
"""
Anubis WB2UCI Bridge
Translates UCI protocol on stdin/stdout to WinBoard protocol for the Anubis engine.
"""

import subprocess
import sys
import os
import re
import time
import threading
import queue
import argparse
import chess
import select

# === Anubis WB Output Format (from Pantalla.c + Pensar.c) ===
# PV info:  "%u %d %u %u \t%s"  = depth score centiseconds nodes <TAB> pv_moves
# Bestmove: "\nmove %s"
# Hint:     "Hint: %s"
# Feature:  "feature done=0" / "feature ... done=1"
# Pong:     "pong %d"
# Illegal:  "Illegal move: %s"
# Mate:     "0-1 {Anubis recibe jaque mate ...}"
# Draw:     "1/2-1/2 {Anubis est� ahogado}"
# Telluser:  "telluser ..."
# EGTB:     "EGTBs cargadas"
# NNUE:     "#NNUE cargado"

# === Engine Output -> UCI Translation ===

def wb_score_to_uci(score):
    """Convert WB score (centipawns) to UCI format. ±99999 = mate."""
    if score <= -99990:
        # Mate in N for opponent
        return "mate -1"
    if score >= 99990:
        return "mate 1"
    return f"cp {score}"


def wb_move_to_uci(move_str):
    """Convert WB algebraic (e.g. 'e2e4', 'e7e8q') to UCI format."""
    return move_str.lower()


def parse_anubis_line(line):
    """
    Parse one line of Anubis WB output.
    Returns: (type, data_dict) or (None, None) for unhandled lines.
    
    Types: 'info', 'bestmove', 'hint', 'pong', 'error', 'ready'
    """
    line = line.strip()
    if not line:
        return (None, None)
    
    # Debug/mirror lines from Anubis start with *DBG*
    if '*DBG*' in line:
        return ('debug', {'text': line})
    
    # Feature handshake
    if line.startswith('feature '):
        return ('feature', {'text': line})
    
    # Pong
    m = re.match(r'pong (\d+)', line)
    if m:
        return ('pong', {'n': int(m.group(1))})
    
    # Best move
    m = re.match(r'move (\S+)', line)
    if m:
        return ('bestmove', {'move': wb_move_to_uci(m.group(1))})
    
    # Hint (ponder move)
    m = re.match(r'Hint: (\S+)', line)
    if m:
        return ('hint', {'move': wb_move_to_uci(m.group(1))})
    
    # Mate result
    if 'jaque mate' in line or 'checkmated' in line:
        return ('result', {'text': line})
    
    # Draw / stalemate
    if '1/2-1/2' in line or 'stalemate' in line:
        return ('result', {'text': line})
    
    # Illegal move echo
    if line.startswith('Illegal move'):
        return ('illegal', {'text': line})
    
    # Error from engine
    if line.startswith('Error') or line.startswith('telluser Error'):
        return ('error', {'text': line})
    
    # EGTB / NNUE loaded messages
    if 'EGTBs cargadas' in line or 'NNUE cargado' in line or 'EGTBs loaded' in line:
        return ('ready', {'text': line})
    
    # PV info: depth score centiseconds nodes \t pv
    # Format from ImprimirCadenaGUI: "%u %d %u %u \t%s"
    # e.g.: "9 42 123 45678 \te2e4 e7e5"
    m = re.match(r'(\d+) ([+-]?\d+) (\d+) (\d+)\s*\t\s*(.+)', line)
    if m:
        depth = int(m.group(1))
        score = int(m.group(2))
        time_cs = int(m.group(3))
        nodes = int(m.group(4))
        pv_str = m.group(5)
        pv_moves = [wb_move_to_uci(mv) for mv in pv_str.split() if mv]
        return ('info', {
            'depth': depth,
            'score': wb_score_to_uci(score),
            'time': time_cs * 10,  # centiseconds -> milliseconds
            'nodes': nodes,
            'pv': pv_moves
        })
    
    # Alternate PV info without tab (less common)
    m = re.match(r'(\d+) ([+-]?\d+) (\d+) (\d+)\s+(.+)', line)
    if m:
        depth = int(m.group(1))
        score = int(m.group(2))
        time_cs = int(m.group(3))
        nodes = int(m.group(4))
        pv_str = m.group(5)
        pv_moves = [wb_move_to_uci(mv) for mv in pv_str.split() if mv]
        return ('info', {
            'depth': depth,
            'score': wb_score_to_uci(score),
            'time': time_cs * 10,
            'nodes': nodes,
            'pv': pv_moves
        })
    
    # Unknown line
    return ('unknown', {'text': line})


class AnubisWB2UCI:
    """Bridge between UCI GUI and Anubis WinBoard engine."""
    
    def __init__(self, engine_path, debug=False):
        self.engine_path = engine_path
        self.debug = debug
        self.engine = None
        self.ponder_enabled = False
        self.analyze_mode = False
        self.my_time = 60000  # ms
        self.opp_time = 60000
        self.time_inc = 0
        self.moves_to_go = 0
        self.depth_limit = 0
        self.movetime = 0
        self.position = 'startpos'
        self.moves = []
        self.bestmove_found = False
        
    def log(self, msg):
        if self.debug:
            print(f"# WB2UCI: {msg}", file=sys.stderr, flush=True)
    
    def start(self):
        """Launch the engine."""
        self.log(f"Starting engine: {self.engine_path}")
        
        # Detect Windows binary and wrap with wine if needed
        cmd = [self.engine_path]
        if not self.engine_path.endswith('.exe'):
            # Try to detect PE binary
            try:
                with open(self.engine_path, 'rb') as f:
                    if f.read(2) == b'MZ':
                        cmd = ['wine', self.engine_path]
            except OSError:
                pass
        else:
            cmd = ['wine', self.engine_path]
        
        self.log(f"Command: {cmd}")
        self.engine = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=False,  # Binary mode - handle encoding ourselves
            bufsize=0,
            cwd=os.path.dirname(self.engine_path) or '.',
            env={**os.environ, 'WINEDEBUG': '-all'}
        )
        
        # WinBoard handshake
        self.log("Sending xboard...")
        self.wb_send("xboard")
        # The engine expects xboard, then protover
        self.wb_send("protover 2")
        # Read the feature line
        time.sleep(0.2)
        self.log("Handshake complete")
        self.wb_send("new")
        self.wb_send("post")
        self.wb_send("hard")
        
    def wb_send(self, cmd):
        """Send a command to the engine."""
        if self.debug:
            self.log(f">>> {cmd}")
        try:
            self.engine.stdin.write(cmd.encode('latin-1') + b"\n")
            self.engine.stdin.flush()
        except (BrokenPipeError, OSError) as e:
            self.log(f"ERROR sending to engine: {e}")
    
    def stop(self):
        """Stop the engine."""
        if self.engine:
            self.wb_send("quit")
            try:
                self.engine.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.engine.kill()
                self.engine.wait()
            self.engine = None
    
    def send_position(self):
        """Send current position to engine using WB2 protocol."""
        # Anubis supports 'setboard FEN' which is the most reliable method.
        # For startpos, we can just use 'new'.
        
        if self.position == 'startpos' and not self.moves:
            self.wb_send("new")
            return
        
        if self.position == 'startpos':
            board = chess.Board()
            for move_uci in self.moves:
                board.push_uci(move_uci)
            fen = board.fen()
        else:
            board = chess.Board(self.position)
            for move_uci in self.moves:
                board.push_uci(move_uci)
            fen = board.fen()
        
        # Use setboard to set the exact position
        self.wb_send(f"setboard {fen}")
    
    def process_input(self):
        """Main loop: read UCI from stdin, output UCI to stdout via engine."""
        # Start engine
        self.start()
        
        # Thread to read engine output
        output_queue = queue.Queue()
        
        def read_engine():
            try:
                buf = b""
                while True:
                    chunk = self.engine.stdout.read(1)
                    if not chunk:
                        if buf:
                            output_queue.put(buf.decode('latin-1').rstrip('\n').rstrip('\r'))
                        break
                    buf += chunk
                    if chunk == b'\n':
                        output_queue.put(buf.decode('latin-1').rstrip('\n').rstrip('\r'))
                        buf = b""
            except Exception as e:
                self.log(f"Engine stdout closed: {e}")
            output_queue.put(None)  # Sentinel
        
        reader_thread = threading.Thread(target=read_engine, daemon=True)
        reader_thread.start()
        
        try:
            # State: 0=idle, 1=waiting_for_bestmove
            state = 0
            
            while True:
                # Check for engine output
                engine_lines = []
                while True:
                    try:
                        line = output_queue.get_nowait()
                        if line is None:
                            self.log("Engine terminated")
                            return
                        engine_lines.append(line)
                    except queue.Empty:
                        break
                
                found_bestmove = False
                for line in engine_lines:
                    if self.debug:
                        self.log(f"<<< {line}")
                    parsed = parse_anubis_line(line)
                    if parsed[0] == 'bestmove':
                        if not self.bestmove_found:
                            print(f"bestmove {parsed[1]['move']}", flush=True)
                            self.bestmove_found = True
                            found_bestmove = True
                            state = 0
                    elif parsed[0] == 'info':
                        d = parsed[1]
                        pv = ' '.join(d['pv']) if d['pv'] else ''
                        print(f"info depth {d['depth']} score {d['score']} "
                              f"time {d['time']} nodes {d['nodes']} "
                              f"pv {pv}", flush=True)
                    elif parsed[0] == 'error':
                        print(f"info string ERROR: {parsed[1]['text']}", flush=True)
                    elif parsed[0] == 'illegal':
                        print(f"info string ILLEGAL FROM ENGINE: {parsed[1]['text']}", flush=True)
                    elif parsed[0] == 'debug' and self.debug:
                        print(f"info string debug: {parsed[1]['text']}", flush=True)
                    elif parsed[0] == 'pong':
                        pass
                    elif parsed[0] == 'result':
                        state = 0
                
                # Only read UCI input when not waiting for bestmove
                if state == 0:
                    if select.select([sys.stdin], [], [], 0.01)[0]:
                        uci_line = sys.stdin.readline()
                        if not uci_line:
                            break
                        uci_line = uci_line.strip()
                        self.handle_uci(uci_line)
                        # If we just sent 'go', switch to waiting state
                        if uci_line.startswith("go"):
                            state = 1
                else:
                    # Still waiting, just sleep briefly and poll
                    time.sleep(0.01)
                    
        except KeyboardInterrupt:
            pass
        finally:
            self.stop()
    
    def handle_uci(self, line):
        """Handle one UCI command from the GUI."""
        if self.debug:
            self.log(f"UCI: {line}")
        
        if line == "uci":
            print("id name Anubis 3.06 (WB2UCI)")
            print("id author Jose Carlos Martinez Galan")
            print("option name SyzygyPath type string default <empty>")
            print("option name Hash type spin default 350 min 1 max 8192")
            print("option name Ponder type check default true")
            print("uciok", flush=True)
            
        elif line == "isready":
            self.wb_send("ping 1")
            # Wait a bit for pong, then respond
            time.sleep(0.1)
            print("readyok", flush=True)
            
        elif line == "ucinewgame":
            self.bestmove_found = False
            self.position = 'startpos'
            self.moves = []
            self.wb_send("new")
            
        elif line.startswith("setoption"):
            # Handle setoption
            # setoption name SyzygyPath value /path/to/syzygy
            m = re.match(r'setoption name (\S+(?:\s+\S+)*?)(?:\s+value\s+(.*))?', line)
            if m:
                name = m.group(1).strip().lower()
                value = m.group(2).strip() if m.group(2) else ""
                
                if name == "syzygypath":
                    self.wb_send(f"option SyzygyPath={value}")
                elif name == "hash":
                    # Anubis hash is compile-time fixed, can't change at runtime
                    # But we can report it
                    mb = int(value) if value else 350
                    print(f"info string Hash set to {mb} MB (requires engine restart for Anubis)", flush=True)
                elif name == "ponder":
                    if value.lower() == "true":
                        self.wb_send("hard")
                        self.ponder_enabled = True
                    else:
                        self.wb_send("easy")
                        self.ponder_enabled = False
                        
        elif line.startswith("position"):
            # position startpos | position fen ... 
            # ... moves e2e4 e7e5 ... 
            parts = line.split()
            if parts[1] == "startpos":
                self.position = "startpos"
                move_idx = 3 if len(parts) > 2 and parts[2] == "moves" else 2
            elif parts[1] == "fen":
                # position fen FEN_STRING
                fen_parts = []
                i = 2
                while i < len(parts) and parts[i] != "moves":
                    fen_parts.append(parts[i])
                    i += 1
                self.position = ' '.join(fen_parts)
                move_idx = i + 1 if i < len(parts) else len(parts)  # skip 'moves'
            else:
                # position FEN_STRING (without 'fen' keyword)
                fen_parts = []
                i = 1
                while i < len(parts) and parts[i] != "moves":
                    fen_parts.append(parts[i])
                    i += 1
                self.position = ' '.join(fen_parts)
                move_idx = i + 1 if i < len(parts) else len(parts)
            
            # Extract moves
            self.moves = parts[move_idx:] if move_idx < len(parts) else []
            self.bestmove_found = False
            
        elif line.startswith("go"):
            self.bestmove_found = False
            parts = line.split()
            
            # Parse UCI go parameters
            wtime = btime = winc = binc = 0
            movestogo = 0
            depth = 0
            movetime = 0
            
            i = 1
            while i < len(parts):
                if parts[i] == "wtime" and i+1 < len(parts):
                    wtime = int(parts[i+1])
                    i += 2
                elif parts[i] == "btime" and i+1 < len(parts):
                    btime = int(parts[i+1])
                    i += 2
                elif parts[i] == "winc" and i+1 < len(parts):
                    winc = int(parts[i+1])
                    i += 2
                elif parts[i] == "binc" and i+1 < len(parts):
                    binc = int(parts[i+1])
                    i += 2
                elif parts[i] == "movestogo" and i+1 < len(parts):
                    movestogo = int(parts[i+1])
                    i += 2
                elif parts[i] == "depth" and i+1 < len(parts):
                    depth = int(parts[i+1])
                    i += 2
                elif parts[i] == "movetime" and i+1 < len(parts):
                    movetime = int(parts[i+1])
                    i += 2
                elif parts[i] == "infinite":
                    # Analyze mode
                    self.wb_send("analyze")
                    return
                elif parts[i] == "ponder":
                    self.ponder_enabled = True
                    i += 1
                else:
                    i += 1
            
            # Determine which color Anubis is playing
            # We track this by counting moves — after even number of moves, it's Black's turn
            is_white = len(self.moves) % 2 == 0
            
            # Send WB time commands
            if is_white:
                self.my_time = wtime if wtime > 0 else self.my_time
                my_inc = winc
            else:
                self.my_time = btime if btime > 0 else self.my_time
                my_inc = binc
            
            # Build the position (new + usermove sequence)
            # Note: Anubis handles its own search state — we just send go
            self.send_position()
            
            # Send time control
            if movetime > 0:
                # Fixed time per move — Anubis doesn't support 'st', use a tight level
                secs = max(1, movetime // 1000)
                self.wb_send(f"level 40 0:{secs:02d} 0")
                self.wb_send(f"time {movetime // 10}")
            else:
                # Normal time control
                secs = (self.my_time // 1000) if self.my_time > 0 else 120
                inc = my_inc // 1000 if my_inc > 0 else 0
                moves = movestogo if movestogo > 0 else 40
                mins = secs // 60
                secs_remainder = secs % 60
                self.wb_send(f"level {moves} {mins}:{secs_remainder:02d} {inc}")
                self.wb_send(f"time {self.my_time // 10}")
            
            # Go!
            self.wb_send("go")
            
        elif line == "stop":
            self.wb_send("?")
            
        elif line == "quit":
            self.stop()
            sys.exit(0)


def main():
    parser = argparse.ArgumentParser(description="Anubis WB2UCI Bridge")
    parser.add_argument("--engine", "-e", required=True,
                        help="Path to Anubis engine executable")
    parser.add_argument("--debug", "-d", action="store_true",
                        help="Enable debug output to stderr")
    args = parser.parse_args()
    
    if not os.path.exists(args.engine):
        print(f"Error: Engine not found: {args.engine}", file=sys.stderr)
        sys.exit(1)
    
    bridge = AnubisWB2UCI(args.engine, debug=args.debug)
    bridge.process_input()


if __name__ == "__main__":
    main()
