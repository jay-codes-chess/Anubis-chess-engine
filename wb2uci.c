/*
 * Anubis WB2UCI Bridge (C version)
 * Wraps the Anubis WinBoard engine behind a UCI interface.
 * Compile: x86_64-w64-mingw32-gcc -O2 -static -o anubis_wb2uci.exe wb2uci.c -lws2_32
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

/* === Constants === */
#define MAX_LINE 4096
#define MAX_PV 256
#define ENGINE_EXE "Anubis_3-06_AVX2_HT100.exe"
#define ENGINE_NAME "Anubis 3.06 (WB2UCI)"

/* === Board representation for FEN generation === */
typedef struct {
    char squares[64];  /* P,N,B,R,Q,K,p,n,b,r,q,k or '.' */
    int turn;          /* 0=white, 1=black */
    int castling[4];   /* K Q k q */
    int ep;            /* -1 or square index */
    int halfmove;
    int fullmove;
} Board;

/* === Engine process === */
static HANDLE engine_stdin_w = NULL;
static HANDLE engine_stdout_r = NULL;
static PROCESS_INFORMATION engine_pi;

/* === Search state === */
static int bestmove_found = 0;
static int my_time = 60000;
static int opp_time = 60000;

/* === Forward declarations === */
static void wb_send(const char *fmt, ...);
static int  wb_read_line(char *buf, int maxlen);
static void output_uci(const char *fmt, ...);
static int  parse_anubis_line(const char *line, char *bestmove_out, int bm_size);
static void handle_uci(const char *line);
static void send_position(void);
static void fen_from_startpos_moves(char *fen, int fen_size, int move_count, char moves[][8]);

/* === Position state === */
static char position_fen[256] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
static char pos_moves[500][8];
static int  pos_move_count = 0;

/* === Engine I/O === */

static void engine_init(void) {
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE stdin_r, stdin_w, stdout_r, stdout_w;
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    
    CreatePipe(&stdin_r, &stdin_w, &sa, 0);
    CreatePipe(&stdout_r, &stdout_w, &sa, 0);
    
    engine_stdin_w = stdin_w;
    engine_stdout_r = stdout_r;
    
    /* Don't inherit the parent's ends */
    SetHandleInformation(stdin_w, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdout_r, HANDLE_FLAG_INHERIT, 0);
    
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdin_r;
    si.hStdOutput = stdout_w;
    si.hStdError = stdout_w;
    
    /* Get directory of our executable */
    char exe_dir[MAX_PATH];
    GetModuleFileName(NULL, exe_dir, MAX_PATH);
    char *last_slash = strrchr(exe_dir, '\\');
    if (last_slash) *last_slash = '\0';
    
    char engine_path[MAX_PATH];
    snprintf(engine_path, MAX_PATH, "%s\\%s", exe_dir, ENGINE_EXE);
    
    /* Check if engine exists */
    if (GetFileAttributes(engine_path) == INVALID_FILE_ATTRIBUTES) {
        /* Try without path */
        snprintf(engine_path, MAX_PATH, "%s", ENGINE_EXE);
    }
    
    /* Launch engine */
    if (!CreateProcess(NULL, engine_path, NULL, NULL, TRUE,
                       0, NULL, exe_dir, &si, &engine_pi)) {
        char err[256];
        snprintf(err, 256, "Cannot launch: %s (error %lu)", engine_path, GetLastError());
        output_uci("info string ERROR: %s", err);
        exit(1);
    }
    
    CloseHandle(stdin_r);
    CloseHandle(stdout_w);
    
    /* WinBoard handshake */
    wb_send("xboard");
    /* Anubis expects xboard, then protover */
    wb_send("protover 2");
    
    /* Consume the feature line */
    char buf[MAX_LINE];
    Sleep(200);
    while (wb_read_line(buf, MAX_LINE) > 0) {
        if (strncmp(buf, "feature", 7) == 0) break;
    }
    
    wb_send("new");
    wb_send("post");
    wb_send("hard");
}

static void wb_send(const char *fmt, ...) {
    char buf[MAX_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, MAX_LINE, fmt, ap);
    va_end(ap);
    
    size_t len = strlen(buf);
    buf[len] = '\n';
    buf[len+1] = '\0';
    
    DWORD written;
    WriteFile(engine_stdin_w, buf, (DWORD)strlen(buf), &written, NULL);
}

static int wb_read_line(char *buf, int maxlen) {
    static char leftover[4096] = "";
    static int leftover_len = 0;
    
    int pos = 0;
    
    /* Check leftover first */
    for (int i = 0; i < leftover_len; i++) {
        if (leftover[i] == '\n' || leftover[i] == '\r') {
            if (pos > 0 && leftover[i] == '\r' && i+1 < leftover_len && leftover[i+1] == '\n')
                i++;
            memmove(leftover, leftover + i + 1, leftover_len - i - 1);
            leftover_len -= i + 1;
            buf[pos] = '\0';
            return pos;
        }
        if (pos < maxlen - 1) buf[pos++] = leftover[i];
    }
    leftover_len = 0;
    
    /* Read new data */
    DWORD avail;
    while (1) {
        if (!PeekNamedPipe(engine_stdout_r, NULL, 0, NULL, &avail, NULL))
            return -1;
        if (avail == 0) {
            if (pos > 0) { buf[pos] = '\0'; return pos; }
            return 0;
        }
        
        char chunk[1024];
        DWORD nread;
        if (!ReadFile(engine_stdout_r, chunk, sizeof(chunk)-1, &nread, NULL) || nread == 0)
            return -1;
        
        for (DWORD i = 0; i < nread; i++) {
            if (chunk[i] == '\n' || chunk[i] == '\r') {
                if (pos > 0 && chunk[i] == '\r' && i+1 < nread && chunk[i+1] == '\n')
                    i++;
                /* Save rest as leftover */
                int rest = (int)(nread - i - 1);
                if (rest > 0) {
                    memcpy(leftover, chunk + i + 1, rest);
                    leftover_len = rest;
                }
                buf[pos] = '\0';
                return pos;
            }
            if (pos < maxlen - 1) buf[pos++] = chunk[i];
        }
    }
}

/* === Output === */

static void output_uci(const char *fmt, ...) {
    char buf[MAX_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, MAX_LINE, fmt, ap);
    va_end(ap);
    printf("%s\n", buf);
    fflush(stdout);
}

/* === Anubis output parser === */

static int parse_anubis_line(const char *line, char *bestmove_out, int bm_size) {
    /* Skip debug lines */
    if (strstr(line, "*DBG*")) return 0;
    
    /* Feature handshake - ignore */
    if (strncmp(line, "feature", 7) == 0) return 0;
    
    /* Pong */
    if (strncmp(line, "pong", 4) == 0) return 0;
    
    /* Best move: "move e2e4" */
    if (strncmp(line, "move ", 5) == 0) {
        const char *move = line + 5;
        while (*move == ' ') move++;
        snprintf(bestmove_out, bm_size, "bestmove %s", move);
        return 1; /* bestmove */
    }
    
    /* Hint (ponder move) */
    if (strncmp(line, "Hint:", 5) == 0) return 0;
    
    /* Mate/draw results */
    if (strstr(line, "jaque mate") || strstr(line, "1/2-1/2") || strstr(line, "stalemate"))
        return 0;
    
    /* Illegal move echo */
    if (strncmp(line, "Illegal move", 12) == 0) {
        output_uci("info string ILLEGAL FROM ENGINE: %s", line);
        return 0;
    }
    
    /* Error messages */
    if (strncmp(line, "Error", 5) == 0 || strstr(line, "telluser Error")) {
        output_uci("info string ENGINE: %s", line);
        return 0;
    }
    
    /* NNUE/EGTB loaded */
    if (strstr(line, "NNUE cargado") || strstr(line, "EGTBs cargadas"))
        return 0;
    
    /* PV info: "depth score centiseconds nodes \tpv" */
    /* Format: "9 26 123 45678 \te2e4 e7e5" */
    /* The separator is a TAB character */
    int depth, score, cs, nodes;
    char pv[MAX_PV];
    const char *tab = strchr(line, '\t');
    
    if (tab) {
        if (sscanf(line, "%d %d %d %d", &depth, &score, &cs, &nodes) == 4) {
            const char *pv_start = tab + 1;
            while (*pv_start == ' ' || *pv_start == '\t') pv_start++;
            snprintf(pv, MAX_PV, "%s", pv_start);
            
            /* Convert score to UCI format */
            char score_str[32];
            if (score <= -99990)
                snprintf(score_str, 32, "mate -1");
            else if (score >= 99990)
                snprintf(score_str, 32, "mate 1");
            else
                snprintf(score_str, 32, "cp %d", score);
            
            output_uci("info depth %d score %s time %d nodes %d pv %s",
                       depth, score_str, cs * 10, nodes, pv);
            return 2; /* info */
        }
    }
    
    /* Alternate PV without tab */
    if (sscanf(line, "%d %d %d %d %[^\n]", &depth, &score, &cs, &nodes, pv) == 5) {
        char score_str[32];
        if (score <= -99990)
            snprintf(score_str, 32, "mate -1");
        else if (score >= 99990)
            snprintf(score_str, 32, "mate 1");
        else
            snprintf(score_str, 32, "cp %d", score);
        
        output_uci("info depth %d score %s time %d nodes %d pv %s",
                   depth, score_str, cs * 10, nodes, pv);
        return 2;
    }
    
    return 0;
}

/* === FEN generation === */

static void fen_from_startpos_moves(char *fen, int fen_size, int move_count, char moves[][8]) {
    /* Board: a1=0, h1=7, a8=56, h8=63 */
    /* Start FEN: "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" */
    /* Rank 8 (a8-h8): black pieces, Rank 1 (a1-h1): white pieces */
    char board[64];
    /* Fill board in a1-first order */
    const char *rank8 = "rnbqkbnr";  /* Black back rank */
    const char *rank7 = "pppppppp";  /* Black pawns */
    const char *rank2 = "PPPPPPPP";  /* White pawns */
    const char *rank1 = "RNBQKBNR";  /* White back rank */
    
    for (int f = 0; f < 8; f++) {
        board[0*8 + f] = rank1[f];   /* a1-h1 */
        board[1*8 + f] = rank2[f];   /* a2-h2 */
        board[2*8 + f] = '.';        /* a3-h3 */
        board[3*8 + f] = '.';        /* a4-h4 */
        board[4*8 + f] = '.';        /* a5-h5 */
        board[5*8 + f] = '.';        /* a6-h6 */
        board[6*8 + f] = rank7[f];   /* a7-h7 */
        board[7*8 + f] = rank8[f];   /* a8-h8 */
    }
    
    int turn = 0; /* 0=white */
    int castling[4] = {1, 1, 1, 1}; /* K Q k q */
    int ep = -1;
    int halfmove = 0;
    int fullmove = 1;
    
    for (int m = 0; m < move_count; m++) {
        const char *uci = moves[m];
        int from_col = uci[0] - 'a';
        int from_row = uci[1] - '1';
        int to_col = uci[2] - 'a';
        int to_row = uci[3] - '1';
        int from = from_row * 8 + from_col;
        int to = to_row * 8 + to_col;
        char promo = (uci[4] && uci[4] != ' ') ? uci[4] : 0;
        
        char piece = board[from];
        char captured = board[to];
        
        /* Move piece */
        board[to] = piece;
        board[from] = '.';
        
        /* En passant capture */
        if (piece == 'P' && captured == '.' && from_col != to_col) {
            board[to - 8] = '.'; /* captured black pawn */
        }
        if (piece == 'p' && captured == '.' && from_col != to_col) {
            board[to + 8] = '.'; /* captured white pawn */
        }
        
        /* Set en passant square */
        if (piece == 'P' && from_row == 1 && to_row == 3) ep = to;
        else if (piece == 'p' && from_row == 6 && to_row == 4) ep = to;
        else ep = -1;
        
        /* Promotion */
        if (promo) {
            board[to] = (turn == 0) ? (promo & ~32) : (promo | 32);
        }
        
        /* Update castling rights */
        if (piece == 'K') { castling[0] = 0; castling[1] = 0; }
        if (piece == 'k') { castling[2] = 0; castling[3] = 0; }
        if (piece == 'R' && from == 0) castling[1] = 0; /* a1 */
        if (piece == 'R' && from == 7) castling[0] = 0; /* h1 */
        if (piece == 'r' && from == 56) castling[3] = 0; /* a8 */
        if (piece == 'r' && from == 63) castling[2] = 0; /* h8 */
        if (captured == 'R' && to == 0) castling[1] = 0;
        if (captured == 'R' && to == 7) castling[0] = 0;
        if (captured == 'r' && to == 56) castling[3] = 0;
        if (captured == 'r' && to == 63) castling[2] = 0;
        
        /* Castling moves */
        if (piece == 'K' && from == 4 && to == 6) { board[5] = 'R'; board[7] = '.'; } /* O-O */
        if (piece == 'K' && from == 4 && to == 2) { board[3] = 'R'; board[0] = '.'; } /* O-O-O */
        if (piece == 'k' && from == 60 && to == 62) { board[61] = 'r'; board[63] = '.'; }
        if (piece == 'k' && from == 60 && to == 58) { board[59] = 'r'; board[56] = '.'; }
        
        halfmove = (piece == 'P' || piece == 'p' || captured != '.') ? 0 : halfmove + 1;
        if (turn == 1) fullmove++;
        turn = 1 - turn;
    }
    
    /* Build FEN string */
    int pos = 0;
    for (int r = 7; r >= 0; r--) {
        int empty = 0;
        for (int c = 0; c < 8; c++) {
            char p = board[r * 8 + c];
            if (p == '.') {
                empty++;
            } else {
                if (empty > 0) { pos += snprintf(fen + pos, fen_size - pos, "%d", empty); empty = 0; }
                fen[pos++] = p;
            }
        }
        if (empty > 0) pos += snprintf(fen + pos, fen_size - pos, "%d", empty);
        if (r > 0) fen[pos++] = '/';
    }
    
    fen[pos++] = ' ';
    fen[pos++] = (turn == 0) ? 'w' : 'b';
    fen[pos++] = ' ';
    
    int any_castling = 0;
    if (castling[0]) { fen[pos++] = 'K'; any_castling = 1; }
    if (castling[1]) { fen[pos++] = 'Q'; any_castling = 1; }
    if (castling[2]) { fen[pos++] = 'k'; any_castling = 1; }
    if (castling[3]) { fen[pos++] = 'q'; any_castling = 1; }
    if (!any_castling) fen[pos++] = '-';
    
    /* En passant */
    if (ep >= 0) {
        pos += snprintf(fen + pos, fen_size - pos, " %c%c",
                        'a' + (ep % 8), '1' + (ep / 8));
    } else {
        fen[pos++] = ' ';
        fen[pos++] = '-';
    }
    
    pos += snprintf(fen + pos, fen_size - pos, " %d %d", halfmove, fullmove);
    fen[pos] = '\0';
}

static void send_position(void) {
    if (pos_move_count == 0) {
        wb_send("new");
        return;
    }
    
    char fen[256];
    fen_from_startpos_moves(fen, 256, pos_move_count, pos_moves);
    wb_send("setboard %s", fen);
}

/* === UCI command handler === */

static void handle_uci(const char *line) {
    if (strcmp(line, "uci") == 0) {
        output_uci("id name %s", ENGINE_NAME);
        output_uci("id author Jose Carlos Martinez Galan");
        output_uci("option name SyzygyPath type string default <empty>");
        output_uci("option name Hash type spin default 350 min 1 max 8192");
        output_uci("option name Ponder type check default true");
        output_uci("uciok");
    }
    else if (strcmp(line, "isready") == 0) {
        wb_send("ping 1");
        Sleep(50);
        output_uci("readyok");
    }
    else if (strcmp(line, "ucinewgame") == 0) {
        bestmove_found = 0;
        pos_move_count = 0;
        wb_send("new");
    }
    else if (strncmp(line, "setoption", 9) == 0) {
        char name[128] = "";
        char value[128] = "";
        const char *n = strstr(line, "name ");
        const char *v = strstr(line, "value ");
        if (n) sscanf(n + 5, "%127s", name);
        if (v) sscanf(v + 6, "%127s", value);
        
        if (strcmp(name, "SyzygyPath") == 0) {
            wb_send("option SyzygyPath=%s", value);
        } else if (strcmp(name, "Hash") == 0) {
            output_uci("info string Hash set to %s MB (requires engine restart)", value);
        } else if (strcmp(name, "Ponder") == 0) {
            if (strcmp(value, "true") == 0) wb_send("hard");
            else wb_send("easy");
        }
    }
    else if (strncmp(line, "position", 8) == 0) {
        bestmove_found = 0;
        pos_move_count = 0;
        
        const char *p = line + 9; /* skip "position " */
        
        /* Check if startpos or fen */
        if (strncmp(p, "startpos", 8) == 0) {
            p += 8;
        } else if (strncmp(p, "fen ", 4) == 0) {
            /* For FEN positions, extract FEN and use setboard directly */
            p += 4;
            char fen[256];
            int i = 0;
            while (*p && strncmp(p, " moves", 6) != 0 && i < 255) {
                fen[i++] = *p++;
            }
            fen[i] = '\0';
            /* Strip trailing space */
            while (i > 0 && fen[i-1] == ' ') fen[--i] = '\0';
            wb_send("setboard %s", fen);
            /* Skip moves for now - setboard handles the position */
            pos_move_count = -1; /* signal: position already set */
        }
        
        /* Parse moves */
        if (pos_move_count >= 0) {
            const char *moves_start = strstr(p, "moves ");
            if (moves_start) {
                moves_start += 6;
                const char *mp = moves_start;
                while (*mp && pos_move_count < 500) {
                    /* Skip spaces */
                    while (*mp == ' ') mp++;
                    if (!*mp) break;
                    
                    /* Read move (4 chars for normal, 5 for promotion) */
                    int len = 0;
                    while (mp[len] && mp[len] != ' ') len++;
                    if (len >= 4 && len <= 5) {
                        strncpy(pos_moves[pos_move_count], mp, len);
                        pos_moves[pos_move_count][len] = '\0';
                        pos_move_count++;
                    }
                    mp += len;
                }
            }
        }
    }
    else if (strncmp(line, "go", 2) == 0) {
        bestmove_found = 0;
        const char *p = line + 2;
        
        int wtime = 0, btime = 0, winc = 0, binc = 0;
        int movestogo = 0, depth = 0, movetime = 0;
        int analyze = 0;
        
        while (*p) {
            while (*p == ' ') p++;
            if (strncmp(p, "wtime ", 6) == 0) { sscanf(p+6, "%d", &wtime); p += 6; while(*p>='0'&&*p<='9')p++; }
            else if (strncmp(p, "btime ", 6) == 0) { sscanf(p+6, "%d", &btime); p += 6; while(*p>='0'&&*p<='9')p++; }
            else if (strncmp(p, "winc ", 5) == 0) { sscanf(p+5, "%d", &winc); p += 5; while(*p>='0'&&*p<='9')p++; }
            else if (strncmp(p, "binc ", 5) == 0) { sscanf(p+5, "%d", &binc); p += 5; while(*p>='0'&&*p<='9')p++; }
            else if (strncmp(p, "movestogo ", 10) == 0) { sscanf(p+10, "%d", &movestogo); p += 10; while(*p>='0'&&*p<='9')p++; }
            else if (strncmp(p, "depth ", 6) == 0) { sscanf(p+6, "%d", &depth); p += 6; while(*p>='0'&&*p<='9')p++; }
            else if (strncmp(p, "movetime ", 9) == 0) { sscanf(p+9, "%d", &movetime); p += 9; while(*p>='0'&&*p<='9')p++; }
            else if (strncmp(p, "infinite", 8) == 0) { analyze = 1; p += 8; }
            else if (strncmp(p, "ponder", 6) == 0) { p += 6; }
            else p++;
        }
        
        if (analyze) {
            wb_send("analyze");
            return;
        }
        
        /* Set up position */
        send_position();
        
        /* Set time control */
        int is_white = (pos_move_count % 2) == 0;
        int mytime = is_white ? (wtime > 0 ? wtime : my_time) : (btime > 0 ? btime : my_time);
        int myinc = is_white ? winc : binc;
        
        if (movetime > 0) {
            int secs = movetime / 1000;
            if (secs < 1) secs = 1;
            wb_send("level 40 0:%02d 0", secs);
            wb_send("time %d", movetime / 10);
        } else {
            int secs = mytime / 1000;
            if (secs < 1) secs = 120;
            int inc = myinc / 1000;
            int moves = movestogo > 0 ? movestogo : 40;
            wb_send("level %d %d:%02d %d", moves, secs/60, secs%60, inc);
            wb_send("time %d", mytime / 10);
        }
        
        wb_send("go");
    }
    else if (strcmp(line, "stop") == 0) {
        /* Anubis '?' handler has a bug (missing continue) but COM_STOP works */
        wb_send("?");
    }
    else if (strcmp(line, "quit") == 0) {
        wb_send("quit");
        Sleep(200);
        TerminateProcess(engine_pi.hProcess, 0);
        CloseHandle(engine_pi.hProcess);
        CloseHandle(engine_pi.hThread);
        exit(0);
    }
}

/* === Main === */

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    
    /* Start engine */
    engine_init();
    
    /* Main loop: block on stdin, process engine output after 'go' */
    char uci_line[MAX_LINE];
    
    while (fgets(uci_line, MAX_LINE, stdin)) {
        /* Strip newline */
        size_t len = strlen(uci_line);
        while (len > 0 && (uci_line[len-1] == '\n' || uci_line[len-1] == '\r'))
            uci_line[--len] = '\0';
        
        if (len == 0) continue;
        
        handle_uci(uci_line);
        
        /* After 'go', wait for bestmove from engine */
        if (strncmp(uci_line, "go", 2) == 0) {
            char engine_line[MAX_LINE];
            char bestmove_str[MAX_LINE] = "";
            
            while (1) {
                int n = wb_read_line(engine_line, MAX_LINE);
                if (n <= 0) { Sleep(10); continue; }
                
                int result = parse_anubis_line(engine_line, bestmove_str, MAX_LINE);
                if (result == 1) {
                    /* bestmove found */
                    output_uci("%s", bestmove_str);
                    break;
                }
            }
        }
        
        if (strcmp(uci_line, "quit") == 0) break;
    }
    
    /* Cleanup */
    wb_send("quit");
    Sleep(200);
    TerminateProcess(engine_pi.hProcess, 0);
    CloseHandle(engine_pi.hProcess);
    CloseHandle(engine_pi.hThread);
    return 0;
}
