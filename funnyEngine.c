#include <stdio.h>
#include <stdlib.h>
#include <immintrin.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <assert.h>

//time 
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/*
// ****************MACROS************* \\
*/

//bitboard definition as U64
typedef unsigned long long bb;
//moves as U64
typedef unsigned long long moves;

#define WHITE    0
#define BLACK    1
#define ALL      2

#define ROOK     0
#define BISHOP   1

#define FLAG_NORMAL     0
#define FLAG_ENPASSANT  1
#define FLAG_PROMOTION  2  
#define FLAG_CASTLE     3

#define PROMOTE_KNIGHT  0
#define PROMOTE_BISHOP  1
#define PROMOTE_ROOK    2
#define PROMOTE_QUEEN   3

#define QUIET_MOVES     0x1
#define CAPTURE_MOVES   0x2
#define ALL_MOVES       (QUIET_MOVES | CAPTURE_MOVES)
#define ESCAPE_MOVES    0x4

#define MAX_MOVES 256

#define BIT(square) (1ULL << (square))
#define SET_BIT(bitboard, square) ((bitboard |= (1ULL << (square))))
#define GET_BIT(bitboard, square) ((bitboard & (1ULL << (square))))
#define POP_BIT(bitboard, square) ((GET_BIT(bitboard, square) ? bitboard ^= (1ULL << (square)) : 0)) 
#define GET_MOVE(source_square, target_square, flag, promotion) ((source_square) | ((target_square) << 6) | ((flag) << 12) | ((promotion) << 14))
#define CLEAR_LS1B(bitboard) (bitboard &= bitboard - 1) //RECENT UPDATE
/*
// ****************UTILITIES************* \\
*/

const char *SQUARE_TO_COORDINATES[] = 
{
"a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
"a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
"a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
"a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
"a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
"a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
"a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
"a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
};

enum 
{
    a8, b8, c8, d8, e8, f8, g8, h8,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a1, b1, c1, d1, e1, f1, g1, h1,
};

// Human readable biboard indexing by piece(and colour: W, b)
enum {P, N, B, R, Q, K, p, n, b, r, q, k};

char ascii_pieces[12] = "PNBRQKpnbrqk";

int char_pieces[] = 
{
    ['P'] = P,
    ['N'] = N,
    ['B'] = B,
    ['R'] = R,
    ['Q'] = Q,
    ['K'] = K,
    ['p'] = p,
    ['n'] = n,
    ['b'] = b,
    ['r'] = r,
    ['q'] = q,
    ['k'] = k
};

const bb NOT_A_FILE = 18374403900871474942ULL; // <- the number corresponding to the table in binary

const bb NOT_H_FILE = 9187201950435737471ULL;

const bb NOT_HG_FILE = 4557430888798830399ULL; 

const  bb NOT_AB_FILES = 18229723555195321596ULL;

static inline int count_ones(bb bitboard) 
{
#ifdef __POPCNT__
    return _mm_popcnt_u64(bitboard);
#else
    int count = 0;
    while (bitboard) 
    {
        count++;
        bitboard &= bitboard - 1; 
    }
    return count;
#endif
}

static inline int count_tz(bb bitboard) 
{
#ifdef __BMI__
    return _tzcnt_u64(bitboard); // returns 64 
#else
    if(bitboard)
    {
        return count_ones((bitboard & (~bitboard + 1)) - 1); 
    }
    else
        return -1; // but here returns -1, this shouldn't cause conflicts
#endif
}

static inline moves get_move_from_square(const moves *move) 
{
    return *move & 0x3F;
}

static inline moves get_move_target_square(const moves *move)
{
    return (*move >> 6) & 0x3F;
}

static inline moves get_move_flags(const moves *move)
{
    return (*move >> 12) & 0x3;
}

static inline moves get_move_promotion(const moves *move)
{
    return (*move >> 14) & 0x3;
}

static inline moves get_move_capture(const moves *move)
{
    return (*move >> 16) & 0x7;
}

static inline moves get_move_castling(const moves *move)
{
    return (*move >> 19) & 0xF;
}

static inline moves get_move_enpassant(const moves *move)
{
    int square = (*move >> 24) & 0x3F;
    return (square == 7) ? -1 : square;  // h1 represents no en passant
}

static inline moves get_move_halfmove(const moves *move)
{
    return (*move >> 30) & 0x7F;
}

static inline void set_move_capture(moves *move, int piece) 
{
   *move |= ((piece + 1) & 0x7) << 16;
}

static inline void set_move_castling(moves *move, int rights) 
{
   *move |= (rights & 0xF) << 19; 
}

static inline void set_move_enpassant(moves *move, int square)
{
    *move |= ((square == -1 ? 7 : square) & 0x3F) << 24;
}

static inline void set_move_halfmove(moves *move, int count) 
{
   *move |= (count & 0x7FULL) << 30;
}

static inline bb between(int square1, int square2) 
{
    bb between_squares = 0ULL;

    bb bit1 = BIT(square1);
    bb bit2 = BIT(square2);

    if ((square1 / 8 - square2 / 8) == (square1 % 8 - square2 % 8)) 
    { 
        for (int i = square1 + 9; i < square2; i += 9)
        {
            SET_BIT(between_squares, i);
        }
        for (int i = square1 - 9; i > square2; i -= 9)
        {
            SET_BIT(between_squares, i);
        }

    } 
    else if ((square1 / 8 - square2 / 8) == -(square1 % 8 - square2 % 8)) 
    { 
        for (int i = square1 + 7; i < square2; i += 7)
        {
            SET_BIT(between_squares, i);
        }
        for (int i = square1 - 7; i > square2; i -= 7)
        {
            SET_BIT(between_squares, i);
        }
    }

    else if (square1 / 8 == square2 / 8) 
    {
        for (int i = square1 + 1; i < square2; i++)
        {
            SET_BIT(between_squares, i);
        }
        for (int i = square1 - 1; i > square2; i--)
        {
            SET_BIT(between_squares, i);
        }
    }

    else if (square1 % 8 == square2 % 8) 
    {
        for (int i = square1 + 8; i < square2; i += 8)
        {
            SET_BIT(between_squares, i);
        }
        for (int i = square1 - 8; i > square2; i -= 8)
        {
            SET_BIT(between_squares, i);
        }
    }

    return between_squares;
} 

static inline bb ray(int pole_square, int line_square) 
{
    bb ray = 0ULL;
    
    int pole_file = pole_square % 8;
    int pole_rank = pole_square / 8;
    int line_file = line_square % 8;
    int line_rank = line_square / 8;
    
    int file_delta = line_file - pole_file;
    int rank_delta = line_rank - pole_rank;
    
    if(abs(file_delta) == abs(rank_delta)) 
    {
        int file_dir = file_delta > 0 ? 1 : -1;
        int rank_dir = rank_delta > 0 ? 1 : -1;
        int i = pole_rank;
        int j = pole_file;
        while (i >= 0 && i < 8 && j >= 0 && j < 8) 
        {
            SET_BIT(ray, i * 8 + j);
            i += rank_dir;
            j += file_dir; 
        }
    }

    else if(file_delta == 0 || rank_delta == 0) 
    {
        int file_dir = file_delta != 0 ? (file_delta > 0 ? 1 : -1) : 0;
        int rank_dir = rank_delta != 0 ? (rank_delta > 0 ? 1 : -1) : 0;
        int i = pole_rank;
        int j = pole_file;
        
        while (i >= 0 && i < 8 && j >= 0 && j < 8) 
        {
            SET_BIT(ray, i * 8 + j);
            i += rank_dir;
            j += file_dir;
        }
    }
    
    return ray;
}

/*
// ****************BITBOARDS->BOARD************* \\
*/

//  print a single binary bitboard
void print_bitboard(bb bitboard) 
{
    printf("\n");
    for (int i = 0; i < 8; i++) 
    {
        printf(" %d ", 8 - i);
        for (int j = 0; j < 8; j++) 
        {
            int square = i * 8 + j;
            printf(" %d", GET_BIT(bitboard, square) ? 1 : 0);
        }
        printf("\n");
    }
    printf("\n    a b c d e f g h\n");
    printf("    Bitboard: %llu\n\n", bitboard);
}

typedef struct  
{
    bb pieces[12];  
    bb occupancies[3]; 
} Bitboards;

void init_board(Bitboards *board) 
{
    memset(board->pieces, 0, sizeof(board->pieces));
    memset(board->occupancies, 0, sizeof(board->occupancies));
}

void update_occupancies(Bitboards *board) 
{
    board->occupancies[WHITE] = 0ULL; 
    board->occupancies[BLACK] = 0ULL; 
    board->occupancies[ALL] = 0ULL; 

    // Update white pieces
    for (int piece = P; piece <= K; piece++) 
    {
        board->occupancies[WHITE] |= board->pieces[piece];
        board->occupancies[ALL] |= board->pieces[piece];
    }

    // Update black pieces
    for (int piece = p; piece <= k; piece++) 
    {
        board->occupancies[BLACK] |= board->pieces[piece];
        board->occupancies[ALL] |= board->pieces[piece];
    }
}

long long get_current_time_ms() 
{
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart * 1000LL) / frequency.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000LL);
#endif
}

/*
// ****************CHESSBOARD************* \\
*/

// Constants for castling rights
enum 
{
    W_KING_SIDE = 1,
    W_QUEEN_SIDE = 2,
    B_KING_SIDE = 4,
    B_QUEEN_SIDE = 8
};

typedef struct 
{
    Bitboards board;  

    int side_to_move;     
    int enpassant_square; 
    int castling_rights;  
    int half_moves;
    int full_moves;
    
} ChessBoard;


void init_chessboard(ChessBoard *chessboard) 
{
    init_board(&chessboard->board);
    chessboard->side_to_move = 0;      
    chessboard->enpassant_square = -1; 
    chessboard->castling_rights = 0;    
}

void print_chessboard(const ChessBoard *chessboard)
{

    for (int rank = 0; rank < 8; rank++) 
    {
        printf("%d ", 8 - rank);
        for (int file = 0; file < 8; file++) 
        {
            int square = rank * 8 + file;
            int piece = -1;

            for (int i = 0; i < 12; i++) 
            {
                if (GET_BIT(chessboard->board.pieces[i], square)) 
                {
                    piece = i;
                    break;
                }
            }
            printf(" %c", (piece == -1) ? '.' : "PNBRQKpnbrqk"[piece]);
        }
        printf("\n");
    }
    printf("  a b c d e f g h\n");

    printf("%s's turn\n", chessboard->side_to_move == 0 ? "White" : "Black");
    printf("En passant: %s\n", chessboard->enpassant_square == -1 ? "none" : SQUARE_TO_COORDINATES[chessboard->enpassant_square]);
    printf("Castling rights: %s%s%s%s\n",
        (chessboard->castling_rights & W_KING_SIDE) ? "K" : "",
        (chessboard->castling_rights & W_QUEEN_SIDE) ? "Q" : "",
        (chessboard->castling_rights & B_KING_SIDE) ? "k" : "",
        (chessboard->castling_rights & B_QUEEN_SIDE) ? "q" : "");
    printf("Half-moves: %d - Full-moves: %d\n", chessboard->half_moves, chessboard->full_moves);    
}

/*
// ****************PARSING FEN STRINGS************* \\
*/

enum FEN_ERRORS {
    FEN_OKAY = 0,
    FEN_INVALID_CHAR = 1,
    FEN_STRING_MALFORMED = 2,
    FEN_MISSING_SLASH = 3,
    FEN_EXPECTED_SPACE = 4,
    FEN_INVALID_CASTLING_RIGHT = 5,
    FEN_INVALID_COLOUR = 6, 
    FEN_INVALID_ENPASSANT = 7, 
    FEN_INVALID_HALFMOVE = 8, 
    FEN_INVALID_FULLMOVE = 9,
};

const char* get_error(int error_code)
{
    switch (error_code)
    {
        case FEN_OKAY:
            return "Parsing succhessful";
        case FEN_INVALID_CHAR:
            return "Invalid character found in FEN string";
        case FEN_STRING_MALFORMED:
            return "FEN string is malformed or truncated";
        case FEN_MISSING_SLASH:
            return "Missing '/' between ranks in board position";
        case FEN_EXPECTED_SPACE:
            return "Expected space separator in FEN string";
        case FEN_INVALID_CASTLING_RIGHT:
            return "Invalid castling rights specification (must be combination of KQkq or -)";
        case FEN_INVALID_COLOUR:
            return "Invalid side to move (must be 'w' or 'b')";
        case FEN_INVALID_ENPASSANT:
            return "Invalid enpassant";
        case FEN_INVALID_FULLMOVE:
            return "Invalid fullmove counter"; 
        case FEN_INVALID_HALFMOVE:
            return "Invalid halfmove counter";     
        default:
            return "Unknown FEN parsing error";
    }

}

int parse_fen(ChessBoard *chessboard, char *fen) 
{
    if (!chessboard || !fen) 
        return FEN_STRING_MALFORMED;
    
    init_chessboard(chessboard);
    
    const char valid_pieces[] = "PNBRQKpnbrqk";
    
    for (int rank = 0; rank < 8; rank++) 
    {
        for (int file = 0; file < 8; file++) 
        {
            if (*fen == '\0') 
                return FEN_STRING_MALFORMED;
                
            int square = rank * 8 + file;

            if (isalpha(*fen)) 
            {
                if (!strchr(valid_pieces, *fen))
                    return FEN_INVALID_CHAR;
                    
                int piece = char_pieces[(unsigned char)*fen];  
                if (piece < 0 || piece >= 12)  
                    return FEN_INVALID_CHAR;
                    
                SET_BIT(chessboard->board.pieces[piece], square);
            } 
            else if (isdigit(*fen)) 
            {
                int skip = *fen - '0';
                if (skip < 1 || skip > 8)  
                    return FEN_INVALID_CHAR;
                file += skip - 1;
            } 
            else 
                return FEN_INVALID_CHAR;
                
            fen++;
        }

        if (rank < 7) 
        {
            if (*fen != '/') 
                return FEN_MISSING_SLASH;
            fen++;
        }
    }

    if (*fen != ' ') 
        return FEN_EXPECTED_SPACE;
    fen++;

    if (*fen == 'w')
        chessboard->side_to_move = WHITE;
    else if (*fen == 'b')
        chessboard->side_to_move = BLACK;
    else
        return FEN_INVALID_COLOUR;
    fen++;

    if (*fen != ' ')
        return FEN_EXPECTED_SPACE;
    fen++;

    chessboard->castling_rights = 0;  
    if (*fen != '-') 
    {
        for (int i = 0; i < 4 && *fen != ' '; i++, fen++) 
        {
            switch (*fen) 
            {
                case 'K': chessboard->castling_rights |= W_KING_SIDE; break;
                case 'Q': chessboard->castling_rights |= W_QUEEN_SIDE; break;
                case 'k': chessboard->castling_rights |= B_KING_SIDE; break;
                case 'q': chessboard->castling_rights |= B_QUEEN_SIDE; break;
                default: return FEN_INVALID_CASTLING_RIGHT;
            }
        }
    }
    else
        fen++;

    if (*fen != ' ')
        return FEN_EXPECTED_SPACE;
    fen++;

    if (*fen == '-') 
    {
        chessboard->enpassant_square = -1;
        fen++;
    }
    else 
    {
        if (strlen(fen) < 2)  
            return FEN_INVALID_ENPASSANT;
            
        if (*fen >= 'a' && *fen <= 'h' && fen[1] >= '1' && fen[1] <= '8') 
        {
            int file = *fen - 'a';
            int rank = 8 - (fen[1] - '0');
            chessboard->enpassant_square = rank * 8 + file;
            fen += 2;
        } 
        else 
            return FEN_INVALID_ENPASSANT;
    }

    if (*fen != ' ')
        return FEN_EXPECTED_SPACE;
    fen++;

    char *endptr;
    long half_moves = strtol(fen, &endptr, 10);
    if (endptr == fen || half_moves < 0 || half_moves > 100)
        return FEN_INVALID_HALFMOVE;
    chessboard->half_moves = (int)half_moves;
    fen = endptr;

    if (*fen != ' ')
        return FEN_EXPECTED_SPACE;
    fen++;

    long full_moves = strtol(fen, &endptr, 10);
    if (endptr == fen || full_moves < 1 || full_moves > INT_MAX)
        return FEN_INVALID_FULLMOVE;
    chessboard->full_moves = (int)full_moves;

    chessboard->board.occupancies[WHITE] = 0;
    chessboard->board.occupancies[BLACK] = 0;
    chessboard->board.occupancies[ALL] = 0;

    for (int piece = P; piece <= K; piece++) 
    {
        chessboard->board.occupancies[WHITE] |= chessboard->board.pieces[piece];
        chessboard->board.occupancies[ALL] |= chessboard->board.pieces[piece];
    }

    for (int piece = p; piece <= k; piece++) 
    {
        chessboard->board.occupancies[BLACK] |= chessboard->board.pieces[piece];
        chessboard->board.occupancies[ALL] |= chessboard->board.pieces[piece];
    }

    return FEN_OKAY;
}

/*
// ****************ATTACKS************* \\
*/ 

// *******  attack table *******
bb pawn_attacks[2][64];

// ******* knight attack table *******
bb knight_attacks[64];

// ******* king attack table *******
bb king_attacks[64];

// ******* bishop attack masks *******
bb bishop_masks[64];

// ******* rook attack masks *******
bb rook_masks[64];

// ******* bishop attack table *******
bb bishop_attacks[64][512];

// ******* rook attack table *******
bb rook_attacks[64][4096];

// ******* array with #positions the BISHOP can reach from a certain square *******
const int BISHOP_AVAILABLE_SQUARES[64] = 
{
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6,
};

// ******* array with #positions the ROOK can reach from a certain square *******
const int ROOK_AVAILABLE_SQUARES[64] = 
{
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12,
};


// ************** GENERATE PAWN ATTACKS *****************
bb mask_pawn_attacks(int side, int square)
{
    bb attacks = 0ULL;

    bb bitboard = 0ULL;

    SET_BIT(bitboard, square); 

    //white pawn
    if (!side)
    {
        if ((bitboard >> 7) & NOT_A_FILE) attacks |= (bitboard >> 7); 
        if ((bitboard >> 9) & NOT_H_FILE) attacks |= (bitboard >> 9);
    } 

    else
    //black
    {
        if ((bitboard << 7) & NOT_H_FILE) attacks |= (bitboard << 7); 
        if ((bitboard << 9) & NOT_A_FILE) attacks |= (bitboard << 9);
    }
    return attacks;
}

// ************** GENERATE KNIGHT ATTACKS *****************
bb mask_knight_attacks(int square)
{

    bb attacks = 0ULL;

    bb bitboard = 0ULL;
    
    SET_BIT(bitboard, square);
 
    if ((bitboard >> 17) & NOT_H_FILE) attacks |= (bitboard >> 17);
    if ((bitboard >> 15) & NOT_A_FILE) attacks |= (bitboard >> 15);
    if ((bitboard << 17) & NOT_A_FILE) attacks |= (bitboard << 17);
    if ((bitboard << 15) & NOT_H_FILE) attacks |= (bitboard << 15);
    if ((bitboard >> 10) & NOT_HG_FILE) attacks |= (bitboard >> 10);
    if ((bitboard >> 6) & NOT_AB_FILES) attacks |= (bitboard >> 6);
    if ((bitboard << 10) & NOT_AB_FILES) attacks |= (bitboard << 10);
    if ((bitboard << 6) & NOT_HG_FILE) attacks |= (bitboard << 6);

    return attacks;
}

// ************** GENERATE KING ATTACKS *****************
bb mask_king_attacks(int square)
{
    bb attacks = 0ULL;

    bb bitboard = 0ULL;

    SET_BIT(bitboard, square);
 
    if ((bitboard >> 7) & NOT_A_FILE) attacks |= (bitboard >> 7);
    attacks |= (bitboard >> 8);  
    if ((bitboard >> 9) & NOT_H_FILE) attacks |= (bitboard >> 9);
    if ((bitboard >> 1) & NOT_H_FILE) attacks |= (bitboard >> 1);
    if ((bitboard << 1) & NOT_A_FILE) attacks |= (bitboard << 1);
    if ((bitboard << 7) & NOT_H_FILE) attacks |= (bitboard << 7);
    attacks |= (bitboard << 8); 
    if ((bitboard << 9) & NOT_A_FILE) attacks |= (bitboard << 9);

    return attacks;
}

// ************** GENERATE BISHOP ATTACKS *****************
bb mask_bishop_attacks(int square)
{
    bb attacks = 0ULL;

    int i, j;

    int ti = square / 8;
    int tj = square % 8;

    for (i = ti + 1, j = tj + 1; i < 7 && j < 7; i++, j++)
    {
        attacks |= BIT(i * 8 + j);
    }

    for (i = ti - 1, j = tj + 1; i > 0 && j < 7; i--, j++)
    {
        attacks |= BIT(i * 8 + j);
    }

    for (i = ti + 1, j = tj - 1; i < 7 && j > 0; i++, j--)
    {
        attacks |= BIT(i * 8 + j);
    }

    for (i = ti - 1, j = tj - 1; i > 0 && j > 0; i--, j--)
    {
        attacks |= BIT(i * 8 + j);
    }

    return attacks;
}

// ************** GENERATE BISHOP ATTACKS WITH BLOCKERS *****************
bb bishop_attacks_with_blockers(int square, bb blocker)
{
    bb attacks = 0ULL;

    int i, j;

    int ti = square / 8;
    int tj = square % 8;

    for (i = ti + 1, j = tj + 1; i < 8 && j < 8; i++, j++)
    {
        attacks |= BIT(i * 8 + j);
        if (BIT(i * 8 + j) & blocker) break; 
    }

    for (i = ti - 1, j = tj + 1; i >= 0 && j < 8; i--, j++)
    { 
        attacks |= BIT(i * 8 + j);
        if (BIT(i * 8 + j) & blocker) break;
    }

    for (i = ti + 1, j = tj - 1; i < 8 && j >= 0; i++, j--)
    {
        attacks |= BIT(i * 8 + j);
        if (BIT(i * 8 + j) & blocker) break;
    }

    for (i = ti - 1, j = tj - 1; i >= 0 && j >= 0; i--, j--)
    {
        attacks |= BIT(i*8 + j);
        if (BIT(i * 8 + j) & blocker) break;
    }

    return attacks;
}

// ************** GENERATE ROOK ATTACKS *****************
bb mask_rook_attacks(int square)
{
    bb attacks = 0ULL;
    
    int i, j;

    int ti = square / 8;
    int tj = square % 8;
 
    // mask rook occupancy
    for (i = ti + 1; i < 7; i++)
    {
        attacks |= BIT(i*8 + tj);
    }

    for (i = ti - 1; i > 0; i--)
    {
        attacks |= BIT(i * 8 + tj);
    }

    for (j = tj + 1; j < 7; j++)
    {
        attacks |= BIT(ti * 8 + j);
    }

    for (j = tj - 1; j > 0; j--)
    {
        attacks |= BIT(ti * 8 + j);
    }

    return attacks;
}

// ************** GENERATE ROOK ATTACKS WITH BLOCKERS ****************
bb rook_attacks_with_blockers(int square, bb blocker)
{
    bb attacks = 0ULL;

    int i, j;

    int ti = square / 8;
    int tj = square % 8;
 
    // mask rook occupancy
    for (i = ti + 1; i < 8; i++)
    {
        attacks |= BIT(i * 8 + tj);
        if (BIT(i * 8 + tj) & blocker) break;
    }

    for (i = ti - 1; i >= 0; i--)
    {
        attacks |= BIT(i * 8 + tj);
        if (BIT(i * 8 + tj) & blocker) break;
    }

    for (j = tj + 1; j < 8; j++)
    {
        attacks |= BIT(ti * 8 + j);
        if (BIT(ti * 8 + j) & blocker) break;
    }

    for (j = tj - 1; j >= 0; j--)
    {
        attacks |= BIT(ti * 8 + j);
        if (BIT(ti * 8 + j) & blocker) break;
    }

    return attacks;
}

/*
// ****************GENERATE MaGic NuMbers************* \\
*/

// ****************RANDOM NUMBERS************* 

// We implement simple functions based on the xor shift algorithm - see wiki

unsigned int state = 206050;

unsigned int random_U32_xorshift()
{
    unsigned int number = state;

    number ^= number << 13;
    number ^= number >> 17;
    number ^= number << 5;

    state = number;

    return number;
}

bb random_U64()
{
    bb num1, num2, num3, num4;
 
    num1 = (bb)(random_U32_xorshift()) & 0xFFFF;
    num2 = (bb)(random_U32_xorshift()) & 0xFFFF;
    num3 = (bb)(random_U32_xorshift()) & 0xFFFF;
    num4 = (bb)(random_U32_xorshift()) & 0xFFFF;

    return num1 | (num2 << 16) | (num3 << 32) | (num4 << 48);
}

bb random_sparse_U64()
{
    return (random_U64() & random_U64() & random_U64());
}

// ****************DEFAULT MaGic NuMbers************* 
bb rook_magic_numbers[64] = 
{
    0xa80004000801021ULL, 0x200130040822200ULL, 0x1100200240110008ULL,
    0x180100081280004ULL, 0xc080040002800800ULL, 0x29000e0100182400ULL,
    0x880010012000880ULL, 0x200010020804402ULL, 0x10800080400030ULL,
    0x100400050002009ULL, 0x41802001100080ULL, 0x2000800800100082ULL, 
    0x2000800800040080ULL, 0x8c08800400020080ULL, 0x90808001002200ULL,
    0xc0800848802100ULL, 0x2020918002204000ULL, 0x510084020004000ULL,
    0x2200808010002000ULL, 0x4680420010220008ULL, 0x8048004040040200ULL,
    0x2008004000280ULL, 0x634440001b00802ULL, 0x8001020000a04401ULL,
    0x640008001f080ULL, 0x90005840002000ULL, 0x2901001100200040ULL,
    0x102400a00201200ULL, 0x160040080800800ULL, 0x4010040080800200ULL,
    0x1083080c00100239ULL, 0x804010a00104084ULL, 0x980002000c00040ULL,
    0x201000400040ULL, 0x20008020801000ULL, 0x10001080800800ULL,
    0x6820080080800400ULL, 0x82e000402000810ULL, 0x180d000411005200ULL,
    0x5000041000082ULL, 0x124001a8808005ULL, 0x5482402010024001ULL,
    0x2010100020008080ULL, 0x820100101000aULL, 0x4008080004008080ULL,
    0x2000810020004ULL, 0x40821008440041ULL, 0x4280010040820004ULL,
    0x800400080002480ULL, 0x10c0400020100040ULL, 0xc200801000200080ULL,
    0xc01012010000900ULL, 0x404800800040080ULL, 0xa04800400020080ULL,
    0x8027000402000100ULL, 0x8048044150a00ULL, 0x24116041088001ULL,
    0x210080104001ULL, 0x1000102004090041ULL, 0x5012081000050121ULL,
    0x1842006004110882ULL, 0x881000400481609ULL, 0xa181004c104ULL,
    0x512008044002102ULL,
};
    

bb bishop_magic_numbers[64] =
{
    0xa018200801c84188ULL, 0x2a008010503b900ULL, 0x8111210411000440ULL,
    0x9041100000004ULL, 0x1110882000001805ULL, 0x40201108800a203ULL,
    0x4402082404040000ULL, 0x240048184800ULL, 0x4008400204042080ULL,
    0x6001848108010111ULL, 0x200100162002244ULL, 0x40400882480ULL,
    0x400c020210088000ULL, 0x100082410084008ULL, 0x80020104200420ULL,
    0x805804806b001ULL, 0x40001010022080ULL, 0x20240204040082ULL,
    0x2010122080d0500ULL, 0x8000c20242000ULL, 0x88803400a08400ULL,
    0x2040120802402aULL, 0x4206010048248400ULL, 0x200500821080ULL,
    0x42200809a0082184ULL, 0x92900020490200ULL, 0x88404004041180ULL,
    0x42080104004008ULL, 0x5010102104001ULL, 0x2e008218080120ULL,
    0x460a020300482240ULL, 0x641022060442420ULL, 0x1022024080200880ULL,
    0x431082000020480ULL, 0xc004002402480441ULL, 0x50200800150106ULL,
    0x12008400120020ULL, 0x8810220200008884ULL, 0x60080101240400b1ULL,
    0x2020320020281ULL, 0x802012088502140ULL, 0x201240200a001ULL,
    0x2680120101123000ULL, 0x8a0040c010400a09ULL, 0x4001c00109020200ULL,
    0x4004008820401202ULL, 0x88100468208c1042ULL, 0x30026202200443ULL,
    0x2004012410440040ULL, 0x4001010130021000ULL, 0x4020042402080000ULL,
    0x2800100084040048ULL, 0x930804010410100ULL, 0x1000087090008000ULL,
    0x412202114008100ULL, 0x9221802009e0001ULL, 0x92a802101202090ULL,
    0x80404904100300ULL, 0x150000022011000ULL, 0x8404024000208804ULL,
    0x54208011020204ULL, 0x40004428100102ULL, 0x40205214480080ULL,
    0x160041000450024ULL,  
};

// Index each possible blocker comination resultin in the same attack mask
bb indexed_occupancy_pattern(int index, int bits_in_mask, bb attack_mask)
{
    bb occupancy = 0ULL;

    for (int count = 0; count < bits_in_mask; count++)
    {
        int square = count_tz(attack_mask);

        POP_BIT(attack_mask, square);

        if (index & (1 << count))
        {
           occupancy |= BIT(square); 
        }        
    }

    return occupancy;
}

/*
// ****************GENERATE MAGIC************* \\
*/

bb generate_magic(int square, int piece, int available_squares)
//relevant bits is the number of sqaures that can block our piece's movements from a given position (square)
{
    bb occupancies[4096]; 

    bb attacks[4096];

    bb used_attacks[4096];

    bb attack_mask = piece ?  mask_bishop_attacks(square) : mask_rook_attacks(square);
    
    int max_occupancies = 1 << available_squares; //occupancy indices corresponds to the number of ways pieces can be arranged on the blocking squares

    for (int index = 0; index < max_occupancies; index++)
    {
        occupancies[index] = indexed_occupancy_pattern(index, available_squares, attack_mask); 

        attacks[index] = piece? bishop_attacks_with_blockers(square, occupancies[index]) : rook_attacks_with_blockers(square, occupancies[index]);
    }

    //test magics
    for (int count = 0; count < 10000000; count++)
    {
        //generate candidate
        bb magic_number = random_sparse_U64();

        //skip invalid instances
        if (count_ones((attack_mask * magic_number) & 0xFF00000000000000) < 6) continue; 

        memset(used_attacks, 0ULL, sizeof(used_attacks));

        bool valid = true;

        //test
        for (int index = 0; valid && index < max_occupancies; index++)
        {
            //MAGIC FORMULA
            int magic_index = (int)((occupancies[index] * magic_number) >> (64 - available_squares));

            //check that the magic_number maps equivalent attack patterns to the same magic index

            if (used_attacks[magic_index] == 0ULL)
            {
                used_attacks[magic_index] = attacks[index];
            }
            else if (used_attacks[magic_index] != attacks[index])
            {
                valid = false;
            }

        }
        
        if (valid)
        {
            return magic_number;
        }
    }
    printf("Magic number doesn't work :("); 
    return 0ULL;
}

void init_magic_numbers()
{
    for (int square = 0; square < 64; square++)
    {
        rook_magic_numbers[square] = generate_magic(square, ROOK, ROOK_AVAILABLE_SQUARES[square]);
    }
    for (int square = 0; square < 64; square++)
    {
        bishop_magic_numbers[square] = generate_magic(square, BISHOP, BISHOP_AVAILABLE_SQUARES[square]);
    }    
}


/*
// ****************INITIALISE ATTACK TABLES************* \\
*/

// **************** INIT LEAPERS ATTACKS ****************
void init_leapers_attacks()
{
    for (int square = 0; square < 64; square ++)
    {
        pawn_attacks[WHITE][square] = mask_pawn_attacks(WHITE, square); 
        pawn_attacks[BLACK][square] = mask_pawn_attacks(BLACK, square); 

        knight_attacks[square] = mask_knight_attacks(square);

        king_attacks[square] = mask_king_attacks(square);
    }   
}

// ****************INIT SLIDER ATTACKS************* 

void init_sliders_attacks(int piece)
{
    for (int square = 0; square < 64; square++)
    {
        bishop_masks[square] = mask_bishop_attacks(square);
        rook_masks[square] = mask_rook_attacks(square);

        bb attack_mask = piece ? bishop_masks[square] : rook_masks[square];

        int relevant_bits_count = piece ? BISHOP_AVAILABLE_SQUARES[square] : ROOK_AVAILABLE_SQUARES[square];

        int occupancy_indicies = (1 << relevant_bits_count);
        
        for (int index = 0; index < occupancy_indicies; index++)
        {
            if (piece)
            {
                bb occupancy = indexed_occupancy_pattern(index, relevant_bits_count, attack_mask);
                
                int magic_index = (occupancy * bishop_magic_numbers[square]) >> (64 - BISHOP_AVAILABLE_SQUARES[square]);
                
                bishop_attacks[square][magic_index] = bishop_attacks_with_blockers(square, occupancy);
            }
            // rook
            else
            {
                bb occupancy = indexed_occupancy_pattern(index, relevant_bits_count, attack_mask);
                
                int magic_index = (occupancy * rook_magic_numbers[square]) >> (64 - ROOK_AVAILABLE_SQUARES[square]);
                
                rook_attacks[square][magic_index] = rook_attacks_with_blockers(square, occupancy);
            
            }
        }
    }
}

void init_all()
{
    init_leapers_attacks();
    init_sliders_attacks(BISHOP);
    init_sliders_attacks(ROOK);
}

static inline bb hashed_bishop_attacks(int square, bb occupancy)
{
    
    occupancy &= bishop_masks[square];
    occupancy *= bishop_magic_numbers[square];
    occupancy >>= 64 - BISHOP_AVAILABLE_SQUARES[square];
    
    return bishop_attacks[square][occupancy];
}

static inline bb hashed_rook_attacks(int square, bb occupancy)
{
    occupancy &= rook_masks[square];
    occupancy *= rook_magic_numbers[square];
    occupancy >>= 64 - ROOK_AVAILABLE_SQUARES[square];

    return rook_attacks[square][occupancy];
}

static inline bb hashed_queen_attacks(int square, bb occupancy)
{
    bb queen_attacks = 0ULL;

    bb rook_attacks = hashed_rook_attacks(square, occupancy);
    bb bishop_attacks = hashed_bishop_attacks(square, occupancy);

    queen_attacks = rook_attacks | bishop_attacks;
    
    return queen_attacks;
}


static inline bb get_attacked_squares(const ChessBoard* chessboard, int side)
{
    bb attacked = 0ULL;

    for (int square = 0; square < 64; square++)
    {
        if (GET_BIT(chessboard->board.pieces[!side ? P : p], square))
        {
            attacked |= pawn_attacks[side][square];
        }
    }

    for (int square = 0; square < 64; square++)
    {
        if (GET_BIT(chessboard->board.pieces[!side ? N : n], square))
        {
            attacked |= knight_attacks[square];
        }    
    }

    for (int sq = 0; sq < 64; sq++)
    {
        if (GET_BIT(chessboard->board.pieces[!side ? K : k], sq))
            attacked |= king_attacks[sq];
    }

    for (int square = 0; square < 64; square++)
    {
        if (GET_BIT(chessboard->board.pieces[!side ? B : b], square))
        {
            attacked |= hashed_bishop_attacks(square, chessboard->board.occupancies[ALL]);
        }
    }

    for (int sq = 0; sq < 64; sq++)
    {
        if (GET_BIT(chessboard->board.pieces[!side ? R : r], sq))
            attacked |= hashed_rook_attacks(sq, chessboard->board.occupancies[ALL]);
    }

    for (int sq = 0; sq < 64; sq++)
    {
        if (GET_BIT(chessboard->board.pieces[!side ? Q : q], sq))
            attacked |= hashed_queen_attacks(sq, chessboard->board.occupancies[ALL]);
    }

    return attacked;
}

/*
// ****************GENERATE MOVES************* \\
*/

typedef struct
{
    bb checkers;
    bb pinned;
    bb attacked;
    bb checkray; // semiline starting at the checker piece going through the king | checker squares
} BoardState;

static inline void init_board_state(const ChessBoard* board, BoardState* state) {
    const int us = board->side_to_move;
    const int them = !us;

    state->attacked = get_attacked_squares(board, them);
    
    state->checkers = 0ULL;
    int king_square = count_tz(board->board.pieces[!us ? K : k]);
    if (king_square < 0 || king_square >= 64) 
    {
        printf("\nFATAL ERROR: King not found or invalid position detected!\n");
        printf("This indicates a serious board corruption.\n");
        printf("Program will terminate to prevent undefined behavior.\n");
        exit(1);  
    }

    state->checkers |= pawn_attacks[us][king_square] & board->board.pieces[them ? p : P];
    state->checkers |= knight_attacks[king_square] & board->board.pieces[them ? n : N];
    bb bishop_mask = hashed_bishop_attacks(king_square, board->board.occupancies[ALL]);
    bb rook_mask = hashed_rook_attacks(king_square, board->board.occupancies[ALL]);
    state->checkers |= bishop_mask & (board->board.pieces[them ? b : B] | board->board.pieces[them ? q : Q]);
    state->checkers |= rook_mask & (board->board.pieces[them ? r : R] | board->board.pieces[them ? q : Q]);
    
    state->checkray = 0ULL;

    bb sliding_checkers = state->checkers & ~(board->board.pieces[them ? p : P] | board->board.pieces[them ? n : N]);
    while (sliding_checkers)
    {         
        int checker_square = count_tz(sliding_checkers);         
        state->checkray |= ray(checker_square, king_square);         
        CLEAR_LS1B(sliding_checkers);     
    }
    state->checkray |= (state->checkers & (board->board.pieces[them ? p : P] | board->board.pieces[them ? n : N]));

    state->pinned = 0ULL;

    bb potential_bishop_pinners = board->board.pieces[them ? b : B] | board->board.pieces[them ? q : Q];
    while (potential_bishop_pinners) {
        int pinner_square = count_tz(potential_bishop_pinners);

        if (abs((pinner_square % 8) - (king_square % 8)) ==
            abs((pinner_square / 8) - (king_square / 8))) {
            bb between_squares = between(king_square, pinner_square);
            if (count_ones(between_squares & board->board.occupancies[ALL]) == 1 &&
                (between_squares & board->board.occupancies[us])) {
                state->pinned |= between_squares & board->board.occupancies[us];
            }
        }

        POP_BIT(potential_bishop_pinners, pinner_square);
    }

    bb potential_rook_pinners = board->board.pieces[them ? r : R] | board->board.pieces[them ? q : Q];
    while (potential_rook_pinners) {
        int pinner_square = count_tz(potential_rook_pinners);

        if ((pinner_square % 8) == (king_square % 8) ||
            (pinner_square / 8) == (king_square / 8)) {
            bb between_squares = between(king_square, pinner_square);
            if (count_ones(between_squares & board->board.occupancies[ALL]) == 1 &&
                (between_squares & board->board.occupancies[us])) {
                state->pinned |= between_squares & board->board.occupancies[us];
            }
        }

        POP_BIT(potential_rook_pinners, pinner_square);
    }
}

static inline int count_moves(const moves* movelist)
{
    const moves* move;
    for (move = movelist; *move; move++);
    return move - movelist;
}

void print_moves(moves *movelist) 
{
    int move_count = count_moves(movelist);
    printf("Generated %d moves:\n", move_count);
    for (int i = 0; i < move_count; i++) 
    {
        printf("Move %d: %s -> %s\n", i + 1,
            SQUARE_TO_COORDINATES[get_move_from_square(&movelist[i])],
            SQUARE_TO_COORDINATES[get_move_target_square(&movelist[i])]);
    }
}

static inline moves* generate_knight_moves(const ChessBoard* board, moves* movelist, bb targets) 
{
    const int us = board->side_to_move;
    const int them = !us;
    bb knights = board->board.pieces[!us ? N : n];
    
    moves* current = movelist;
    
    if (!knights) 
    {
        return current;
    }

    while (knights) 
    {
        int from = count_tz(knights);
        
        bb attacks = knight_attacks[from];
        bb filtered_attacks = attacks & targets;

        while (filtered_attacks) 
        {
            int to = count_tz(filtered_attacks);
            
            if (board->board.occupancies[them] & BIT(to))
            {
                for (int piece = (them ? p : P); piece <= (them ? k : K); piece++)
                {
                    if (GET_BIT(board->board.pieces[piece], to)) 
                    {
                        moves capture_move = GET_MOVE(from, to, FLAG_NORMAL, 0);
                        set_move_capture(&capture_move, piece % 6);
                        *current++ = capture_move;
                        break;
                    }
                }
            }
            else
            {
                *current++ = GET_MOVE(from, to, FLAG_NORMAL, 0);
            }
            
            filtered_attacks &= filtered_attacks - 1;
        }
        
        knights &= knights - 1;
    }

    return current;
}

static inline moves* generate_bishop_moves(const ChessBoard* board, moves* movelist, bb targets) 
{
    const int us = board->side_to_move;
    const int them = !us;
    bb bishops = board->board.pieces[!us ? B : b];  
    moves* current = movelist;

    if (!bishops) 
    {
        return current;
    }
   
    while (bishops) 
    {
        int from = count_tz(bishops);
        
        bb attacks = hashed_bishop_attacks(from, board->board.occupancies[ALL]);
        bb filtered_attacks = attacks & targets;
        
        while (filtered_attacks) 
        {
            int to = count_tz(filtered_attacks);
            
            if (board->board.occupancies[them] & BIT(to))
            {
                for (int piece = (them ? p : P); piece <= (them ? k : K); piece++)
                {
                    if (GET_BIT(board->board.pieces[piece], to)) 
                    {
                        moves capture_move = GET_MOVE(from, to, FLAG_NORMAL, 0);
                        set_move_capture(&capture_move, piece % 6);
                        *current++ = capture_move;
                        break;
                    }
                }
            }
            else
            {
                *current++ = GET_MOVE(from, to, FLAG_NORMAL, 0);
            }
            
            filtered_attacks &= filtered_attacks - 1;
        }
        
        bishops &= bishops - 1;
    }

    return current;
}

static inline moves* generate_rook_moves(const ChessBoard* board, moves* movelist, bb targets) 
{
   const int us = board->side_to_move;
   const int them = !us;
   bb rooks = board->board.pieces[!us ? R : r];
   moves* current = movelist;
   
   if (!rooks) 
   {
       return current;
   }

   while (rooks) 
   {
       int from = count_tz(rooks);
       
       bb attacks = hashed_rook_attacks(from, board->board.occupancies[ALL]);
       bb filtered_attacks = attacks & targets;
       
       while (filtered_attacks) 
       {
           int to = count_tz(filtered_attacks);
           
           if (board->board.occupancies[them] & BIT(to))
           {
               for (int piece = (them ? p : P); piece <= (them ? k : K); piece++)
               {
                   if (GET_BIT(board->board.pieces[piece], to)) 
                   {
                       moves capture_move = GET_MOVE(from, to, FLAG_NORMAL, 0);
                       set_move_capture(&capture_move, piece % 6);
                       *current++ = capture_move;
                       break;
                   }
               }
           }
           else
           {
               *current++ = GET_MOVE(from, to, FLAG_NORMAL, 0);
           }
           
           filtered_attacks &= filtered_attacks - 1;
       }
       
       rooks &= rooks - 1;
   }

   return current;
}

static inline moves* generate_queen_moves(const ChessBoard* board, moves* movelist, bb targets) 
{
   const int us = board->side_to_move;
   const int them = !us;
   bb queens = board->board.pieces[!us ? Q : q];  
   moves* current = movelist;
   
   while (queens) 
   {
       int from = count_tz(queens);
       bb attacks = hashed_queen_attacks(from, board->board.occupancies[ALL]) & targets;
       
       while (attacks) 
       {
           int to = count_tz(attacks);
           
           if (board->board.occupancies[them] & BIT(to))
           {
               for (int piece = (them ? p : P); piece <= (them ? k : K); piece++)
               {
                   if (GET_BIT(board->board.pieces[piece], to)) 
                   {
                       moves capture_move = GET_MOVE(from, to, FLAG_NORMAL, 0);
                       set_move_capture(&capture_move, piece % 6);
                       *current++ = capture_move;
                       break;
                   }
               }
           }
           else
           {
               *current++ = GET_MOVE(from, to, FLAG_NORMAL, 0);
           }
           
           attacks &= attacks - 1;
       }
       
       queens &= queens - 1;
   }
   
   return current;
}

static inline moves* generate_king_moves(const ChessBoard* board, moves* movelist, const BoardState* state, unsigned move_type) 
{
    const int us = board->side_to_move;
    const int them = !us;
    bb kings = board->board.pieces[!us ? K : k];
    moves* current = movelist;
    
    if (!kings) 
    {
        return current;
    }

    int from = count_tz(kings);

    bb attacks = king_attacks[from];

    bb targets = 0ULL;

    if (move_type & QUIET_MOVES) 
    {
        targets |= ~board->board.occupancies[ALL];
    }
    if (move_type & CAPTURE_MOVES) 
    {
        targets |= board->board.occupancies[them];
    }

    targets &= attacks;
    targets &= ~state->attacked;

    if (move_type & ESCAPE_MOVES)
    {
        targets &= ((~state->checkray) ^ state->checkers);
    }

    while (targets) 
    {
        int to = count_tz(targets);
        
        if (board->board.occupancies[them] & BIT(to))
        {
            for (int piece = (them ? p : P); piece <= (them ? k : K); piece++)
            {
                if (GET_BIT(board->board.pieces[piece], to)) 
                {
                    moves capture_move = GET_MOVE(from, to, FLAG_NORMAL, 0);
                    set_move_capture(&capture_move, piece % 6);
                    *current++ = capture_move;
                    break;
                }
            }
        }
        else
        {
            *current++ = GET_MOVE(from, to, FLAG_NORMAL, 0);
        }
        
        CLEAR_LS1B(targets);
    }

    if (!(move_type & ESCAPE_MOVES) & !(move_type & CAPTURE_MOVES)) 
    {
        if (!us) 
        {
            if ((board->castling_rights & W_KING_SIDE) &&
                !(board->board.occupancies[ALL] & 0x6000000000000000) &&
                !(state->attacked & 0x6000000000000000)) 
            {
                *current++ = GET_MOVE(e1, g1, FLAG_CASTLE, 0);
            } 

            if ((board->castling_rights & W_QUEEN_SIDE) &&
                !(board->board.occupancies[ALL] & 0xE00000000000000) &&
                !(state->attacked & 0xC00000000000000)) 
            {
                *current++ = GET_MOVE(e1, c1, FLAG_CASTLE, 0);
            }
        }
        else 
        {
            if ((board->castling_rights & B_KING_SIDE) &&
                !(board->board.occupancies[ALL] & 0x60) &&
                !(state->attacked & 0x60)) 
            {
                *current++ = GET_MOVE(e8, g8, FLAG_CASTLE, 0);
            }

            if ((board->castling_rights & B_QUEEN_SIDE) &&
                !(board->board.occupancies[ALL] & 0xE) &&
                !(state->attacked & 0xC)) 
            {
                *current++ = GET_MOVE(e8, c8, FLAG_CASTLE, 0);
            }
        }
    }
    return current;
}

static inline moves* generate_pawn_moves(const ChessBoard* board, moves* movelist, const BoardState* state, unsigned move_type) 
{
    const int us = board->side_to_move;
    const int them = !us;
    const bb pawns = board->board.pieces[us == WHITE ? P : p];
    const bb all_occupancies = board->board.occupancies[ALL];
    const bb opponent_occupancies = board->board.occupancies[them];
    moves* current = movelist;

    bb single_push_targets = (!us) ? (pawns >> 8) : (pawns << 8);
    single_push_targets &= ~all_occupancies;

    bb promotion_rank = (!us) ? 0xFFULL : 0xFF00000000000000ULL;
    bb promotion_pushes = single_push_targets & promotion_rank;
    single_push_targets &= ~promotion_rank;

    if (move_type & ESCAPE_MOVES)
    {
        single_push_targets &= state->checkray;
    }

    if (move_type & QUIET_MOVES) 
    {
        while (single_push_targets) 
        {
            int to = count_tz(single_push_targets);
            int from = (!us) ? (to + 8) : (to - 8);
            *current++ = GET_MOVE(from, to, FLAG_NORMAL, 0);

            POP_BIT(single_push_targets, to);
        }

        while (promotion_pushes) 
        {
            int to = count_tz(promotion_pushes);
            int from = (!us) ? (to + 8) : (to - 8);
            for (int promotion_piece = PROMOTE_KNIGHT; promotion_piece <= PROMOTE_QUEEN; promotion_piece++) 
            {
                *current++ = GET_MOVE(from, to, FLAG_PROMOTION, promotion_piece);
            }
            POP_BIT(promotion_pushes, to);
        }
    }

    bb start_rank = (!us) ? 0xFF000000000000ULL : 0xFF00ULL;
    bb potential_double_push_pawns = pawns & start_rank;
    
    bb intermediate_squares = (!us) ? (potential_double_push_pawns >> 8) : (potential_double_push_pawns << 8);
    bb double_push_targets = (!us) ? (potential_double_push_pawns >> 16) : (potential_double_push_pawns << 16);
    
    double_push_targets &= ~all_occupancies;
    if (!us) 
    {
        double_push_targets &= ~((intermediate_squares & all_occupancies) >> 8);
    } else 
    {
        double_push_targets &= ~((intermediate_squares & all_occupancies) << 8);
    }

    if (move_type & ESCAPE_MOVES)
    {
        double_push_targets &= state->checkray;
    }

    if (move_type & QUIET_MOVES) 
    {
        while (double_push_targets) 
        {
            int to = count_tz(double_push_targets);
            int from = (!us) ? (to + 16) : (to - 16);
            *current++ = GET_MOVE(from, to, FLAG_NORMAL, 0);

            POP_BIT(double_push_targets, to);
        }
    }

    if (move_type & CAPTURE_MOVES) 
    {
        bb target = opponent_occupancies;
        if (move_type & ESCAPE_MOVES)
        {
            target &= state->checkray | state->checkers;
        }
        bb pawns_for_capture = pawns;
        while (pawns_for_capture) 
        {
            int from = count_tz(pawns_for_capture);
            bb attacks = pawn_attacks[us][from] & opponent_occupancies;
            
            // Handle regular captures
            bb non_promotion_attacks = attacks & ~promotion_rank;
            while (non_promotion_attacks) 
            {
                int to = count_tz(non_promotion_attacks);
                // MODIFIED: Add capture detection
                for (int piece = (them ? p : P); piece <= (them ? k : K); piece++)
                {
                    if (GET_BIT(board->board.pieces[piece], to)) 
                    {
                        moves capture_move = GET_MOVE(from, to, FLAG_NORMAL, 0);
                        set_move_capture(&capture_move, piece % 6);
                        *current++ = capture_move;
                        break;
                    }
                }
                POP_BIT(non_promotion_attacks, to);
            }
            
            // Handle promotion captures
            bb promotion_attacks = attacks & promotion_rank;
            while (promotion_attacks) 
            {
                int to = count_tz(promotion_attacks);
                // MODIFIED: Add capture detection for promotions
                for (int piece = (them ? p : P); piece <= (them ? k : K); piece++)
                {
                    if (GET_BIT(board->board.pieces[piece], to)) 
                    {
                        for (int promotion_piece = PROMOTE_KNIGHT; promotion_piece <= PROMOTE_QUEEN; promotion_piece++) 
                        {
                            moves capture_move = GET_MOVE(from, to, FLAG_PROMOTION, promotion_piece);
                            set_move_capture(&capture_move, piece % 6);
                            *current++ = capture_move;
                        }
                        break;
                    }
                }
                POP_BIT(promotion_attacks, to);
            }
            
            POP_BIT(pawns_for_capture, from);
        }
    }

    // Handle en passant captures
    if (board->enpassant_square != -1 && (move_type & CAPTURE_MOVES)) 
    {   
        bb enpassant_target = BIT(board->enpassant_square);
        bb potential_attackers = pawns & pawn_attacks[!us][board->enpassant_square];

        if (potential_attackers)
        {
            int from = count_tz(potential_attackers);
            // MODIFIED: Add capture flag for en passant
            moves capture_move = GET_MOVE(from, board->enpassant_square, FLAG_ENPASSANT, 0);
            set_move_capture(&capture_move, P);  // En passant always captures a pawn
            *current++ = capture_move;
            
            POP_BIT(potential_attackers, from);
            if (potential_attackers)
            {
                from = count_tz(potential_attackers);
                // MODIFIED: Add capture flag for second en passant attacker
                capture_move = GET_MOVE(from, board->enpassant_square, FLAG_ENPASSANT, 0);
                set_move_capture(&capture_move, P);
                *current++ = capture_move;
            }
        }
    }

    return current;
}


static inline moves* generate_pseudo_legal_moves(const ChessBoard* board, BoardState* state, moves* movelist, unsigned move_type) 
{
    bb targets = 0ULL; 
    const int us = board->side_to_move;
    const int them = !us;

    if (move_type & QUIET_MOVES)
    {
        targets |= ~(board->board.occupancies[ALL]); 
    }
    
    if (move_type & CAPTURE_MOVES)
    {
        targets |= board->board.occupancies[them];  
    }    

    // Descrese the space and speeds up - not necessary for pseudo-legality
    if (state->checkers)   
    {
        move_type |= ESCAPE_MOVES;       
        targets &= state->checkray | state->checkers;  //MRFIX
    }

    moves* current = movelist;

    current = generate_pawn_moves(board, current, state, move_type);
    current = generate_knight_moves(board, current, targets);
    current = generate_bishop_moves(board, current, targets);
    current = generate_rook_moves(board, current, targets);
    current = generate_queen_moves(board, current, targets);
    current = generate_king_moves(board, current, state, move_type);

    *current = 0;
    return current;
}

static inline int legality_check(const ChessBoard* board, BoardState* state, const moves* move) 
{
   if(!*move) return 0;

   const int us = board->side_to_move;
   const int them = !us;
   const int king_square = count_tz(board->board.pieces[!us ? K : k]);
   if (king_square < 0 || king_square >= 64) 
    {
        printf("\nFATAL ERROR: King not found or invalid position detected!\n");
        printf("This indicates a serious board corruption.\n");
        printf("Program will terminate to prevent undefined behavior.\n");
        exit(1);  
    }
   int from = get_move_from_square(move);
   int to = get_move_target_square(move);

   if(get_move_flags(move) == FLAG_ENPASSANT) 
   {
        bb temp_occupancy = board->board.occupancies[ALL];
        temp_occupancy ^= BIT(from);
        //I dont even know if this is actually possible but better safe than sorry
        temp_occupancy ^= BIT(to); 
        temp_occupancy ^= BIT(to + (!us ? 8 : -8));
        
        bb rook_attacks = hashed_rook_attacks(king_square, temp_occupancy);
        bb bishop_attacks = hashed_bishop_attacks(king_square, temp_occupancy);
        
        if((rook_attacks & (board->board.pieces[them ? r : R] | board->board.pieces[them ? q : Q])) ||
            (bishop_attacks & (board->board.pieces[them ? b : B] | board->board.pieces[them ? q : Q])))
        {
            return 0;
        }    
   }

   if(GET_BIT(state->pinned, from)) 
   {
        bb pin_ray = ray(king_square, from);
        if(!( GET_BIT(pin_ray, to)))
        {
            return 0;
        }
   }

   int checker_count = count_ones(state->checkers);

   if (checker_count > 1) 
   {
        if (from != king_square) 
        {
            return 0;
        }
   } 
   else if (checker_count == 1) 
   {
        int checker_square = count_tz(state->checkers);

        if ((GET_BIT(board->board.pieces[them ? n : N], checker_square)) ||
            (GET_BIT(board->board.pieces[them ? p : P], checker_square))) 
        {
            if (from != king_square && to != checker_square) 
            {
                return 0;
            }
        }
        else 
        {
           bb ray = between(king_square, checker_square) | BIT(checker_square);
           if (!(GET_BIT(ray, to)) && from != king_square) 
           {
               return 0;
           }
        }
   }
   return 1;
}

static inline moves* generate_legal_moves(const ChessBoard* board, moves* movelist) 
{
    BoardState state;
    init_board_state(board, &state);
    
    moves* end = generate_pseudo_legal_moves(board, &state, movelist, ALL_MOVES);
    moves* current = movelist;
    
    while (*current) 
    {
        if(!legality_check(board, &state, current)) 
        {
            *current = *(--end);
            *end = 0;
        } 
        else 
        {
            current++;
        }
    }
    return end;
}

/*
// **************** MAKE MOVES ************* \\
*/  

static inline int update_castling_rights(const ChessBoard *board, const int from, const int to, const int old_rights) 
{
    int new_rights = old_rights;
    
    if (from == e1 && GET_BIT(board->board.pieces[K], from))
    { 
        new_rights &= ~(W_KING_SIDE | W_QUEEN_SIDE);
    }   
    if (from == e8 && GET_BIT(board->board.pieces[k], from))
    { 
        new_rights &= ~(B_KING_SIDE | B_QUEEN_SIDE); 
    }

    if ((from == a1 && GET_BIT(board->board.pieces[R], from)) || (to == a1 && GET_BIT(board->board.pieces[R], a1)))
    { 
        new_rights &= ~W_QUEEN_SIDE; 
    } 
    if ((from == h1 && GET_BIT(board->board.pieces[R], from)) || (to == h1 && GET_BIT(board->board.pieces[R], h1))) 
    {
        new_rights &= ~W_KING_SIDE;  
    }    
    if ((from == a8 && GET_BIT(board->board.pieces[r], from)) || (to == a8 && GET_BIT(board->board.pieces[r], a8))) 
    {
        new_rights &= ~B_QUEEN_SIDE;  
    }   
    if ((from == h8 && GET_BIT(board->board.pieces[r], from)) || (to == h8 && GET_BIT(board->board.pieces[r], h8))) 
    {
        new_rights &= ~B_KING_SIDE;   
    }
    
    return new_rights;
}

static inline void make_move(ChessBoard *board, moves *move) 
{
    set_move_enpassant(move, board->enpassant_square);
    set_move_castling(move, board->castling_rights);
    set_move_halfmove(move, board->half_moves);
    
    const int from = get_move_from_square(move);
    const int to = get_move_target_square(move);
    const int flag = get_move_flags(move);
    const int us = board->side_to_move;
    const int them = !us;
    
    board->enpassant_square = -1;
    board->half_moves++;
    board->castling_rights = update_castling_rights(board, from, to, board->castling_rights);
    
    if (GET_BIT(board->board.pieces[!us ? P : p], from))
    {
        board->half_moves = 0;
    }
    
    switch(flag) 
    {
        case FLAG_NORMAL: 
        {
            if (GET_BIT(board->board.occupancies[them], to)) 
            {
                for (int piece = (!us ? p : P); piece <= (!us ? k : K); piece++) 
                {
                    if (GET_BIT(board->board.pieces[piece], to)) 
                    {
                        set_move_capture(move, (piece % 6));
                        board->board.pieces[piece] ^= BIT(to);  
                        board->board.occupancies[them] ^= BIT(to);  
                        break;
                    }
                }
                board->half_moves = 0;  
                board->board.occupancies[ALL] ^= BIT(from);
            }
            else
            {
                board->board.occupancies[ALL] ^= (BIT(from) | BIT(to));
            }
            
            for (int piece = (!us ? P : p); piece <= (!us ? K : k); piece++) 
            {
                if (GET_BIT(board->board.pieces[piece], from)) 
                {
                    board->board.pieces[piece] ^= (BIT(from) | BIT(to)); 
                    break;
                }
            }

            board->board.occupancies[us] ^= (BIT(from) | BIT(to));
            
            if (!us && GET_BIT(board->board.pieces[P], to) && from / 8 == 6 && to / 8 == 4) 
            {
                board->enpassant_square = to + 8;
            }
            else if (us && GET_BIT(board->board.pieces[p], to) && from / 8 == 1 && to / 8 == 3) 
            {
                board->enpassant_square = to - 8;
            }
            break;
        }
        
        case FLAG_CASTLE: 
        {
            const int king = !us ? K : k;
            const int rook = !us ? R : r;
            
            board->board.pieces[king] ^= (BIT(from) | BIT(to));
            
            int rook_from, rook_to;
            if (to == (!us ? g1 : g8))  
            {
                rook_from = !us ? h1 : h8;
                rook_to = !us ? f1 : f8;
            }
            else  
            {
                rook_from = !us ? a1 : a8;
                rook_to = !us ? d1 : d8;
            }
            
            board->board.pieces[rook] ^= (BIT(rook_from) | BIT(rook_to));
            board->board.occupancies[us] ^= (BIT(from) | BIT(to) | BIT(rook_from) | BIT(rook_to));
            board->board.occupancies[ALL] ^= (BIT(from) | BIT(to) | BIT(rook_from) | BIT(rook_to));
            break;
        }
        
        case FLAG_ENPASSANT: 
        {
            const int pawn = !us ? P : p;
            const int captured_pawn = !us ? p : P;
            const int captured_square = to + (!us ? 8 : -8);
            
            board->board.pieces[pawn] ^= (BIT(from) | BIT(to));
            board->board.pieces[captured_pawn] ^= BIT(captured_square);
            board->board.occupancies[us] ^= (BIT(from) | BIT(to));
            board->board.occupancies[them] ^= BIT(captured_square);
            board->board.occupancies[ALL] ^= (BIT(from) | BIT(to) | BIT(captured_square));
            
            set_move_capture(move, P);
            break;
        }
        
        case FLAG_PROMOTION: 
        {
            const int pawn = !us ? P : p;
            const int promotion_type = get_move_promotion(move);
            const int promoted_piece = !us ? (P + 1 + promotion_type) : (p + 1 + promotion_type);
            
            if (GET_BIT(board->board.occupancies[them], to)) 
            {
                for (int piece = (!us ? p : P); piece <= (!us ? k : K); piece++) 
                {
                    if (GET_BIT(board->board.pieces[piece], to)) 
                    {
                        set_move_capture(move, (piece % 6));
                        board->board.pieces[piece] ^= BIT(to);  
                        board->board.occupancies[them] ^= BIT(to);  
                        break;
                    }
                }
                board->board.occupancies[ALL] ^= BIT(from);
                board->half_moves = 0;  
            }
            else
            {
                board->board.occupancies[ALL] ^= (BIT(from) | BIT(to));
            }

            board->board.pieces[pawn] ^= BIT(from);
            board->board.pieces[promoted_piece] ^= BIT(to);
            board->board.occupancies[us] ^= (BIT(from) | BIT(to));
            break;
        }
    }

    board->side_to_move = them;
    if (us) board->full_moves++;
}

static inline void unmake_move(ChessBoard *board, moves *move) 
{
    const int from = get_move_from_square(move);
    const int to = get_move_target_square(move);
    const int flag = get_move_flags(move);
    const int us = board->side_to_move;
    const int them = !us;
    
    board->enpassant_square = get_move_enpassant(move);
    board->half_moves = get_move_halfmove(move);
    board->castling_rights = get_move_castling(move);

    switch(flag) 
    {
        case FLAG_NORMAL: 
        {
            for (int piece = (!us ? p : P); piece <= (!us ? k : K); piece++) 
            {
                if (GET_BIT(board->board.pieces[piece], to)) 
                {
                    board->board.pieces[piece] ^= (BIT(from) | BIT(to));
                    break;
                }
            }
    
            const int captured = get_move_capture(move);
            if (captured) 
            {
                const int captured_piece = !us ? captured - 1: (captured + 5);
                board->board.pieces[captured_piece] ^= BIT(to);
                board->board.occupancies[them] ^= (BIT(from) | BIT(to));
                board->board.occupancies[us] ^= BIT(to);
                board->board.occupancies[ALL] ^= BIT(from);
            }
            else
            {
                board->board.occupancies[them] ^= (BIT(from) | BIT(to));
                board->board.occupancies[ALL] ^= (BIT(from) | BIT(to));
            }
            break;
        }
        
        case FLAG_CASTLE: 
        {
            const int king = !us ? k : K;
            const int rook = !us ? r : R;
            
            board->board.pieces[king] ^= (BIT(from) | BIT(to));
            
            if (to == (!us ? g8 : g1)) 
            {
                const int rook_from = !us ? h8 : h1;
                const int rook_to = !us ? f8 : f1;
                board->board.pieces[rook] ^= (BIT(rook_from) | BIT(rook_to));
                board->board.occupancies[them] ^= (BIT(from) | BIT(to) | BIT(rook_from) | BIT(rook_to));
                board->board.occupancies[ALL] ^= (BIT(from) | BIT(to) | BIT(rook_from) | BIT(rook_to));
            }
            else  
            {
                const int rook_from = !us ? a8 : a1;
                const int rook_to = !us ? d8 : d1;
                board->board.pieces[rook] ^= (BIT(rook_from) | BIT(rook_to));
                board->board.occupancies[them] ^= (BIT(from) | BIT(to) | BIT(rook_from) | BIT(rook_to));
                board->board.occupancies[ALL] ^= (BIT(from) | BIT(to) | BIT(rook_from) | BIT(rook_to));
            }
            break;
        }
        
        case FLAG_ENPASSANT: 
        {
            const int pawn = !us ? p : P;
            const int captured_pawn = !us ? P : p;
            const int captured_square = to + (!us ? -8 : 8);

            board->board.pieces[pawn] ^= (BIT(from) | BIT(to));
            board->board.pieces[captured_pawn] ^= BIT(captured_square);

            board->board.occupancies[them] ^= (BIT(from) | BIT(to));
            board->board.occupancies[us] ^= BIT(captured_square);
            board->board.occupancies[ALL] ^= (BIT(from) | BIT(to) | BIT(captured_square));
            break;
        }
        
        case FLAG_PROMOTION: 
        {
            const int pawn = !us ? p : P;
            const int promotion_type = get_move_promotion(move);
            const int promoted_piece = !us ? (p + 1 + promotion_type) : (P + 1 + promotion_type);
            
            board->board.pieces[promoted_piece] ^= BIT(to);

            board->board.pieces[pawn] ^= BIT(from);
            
            const int captured = get_move_capture(move);
            if (captured) 
            {
                const int captured_piece = !us ? captured - 1 : (captured + 5);
                board->board.pieces[captured_piece] ^= BIT(to);
                board->board.occupancies[us] ^= BIT(to);
                board->board.occupancies[them] ^= (BIT(from) | BIT(to));
                board->board.occupancies[ALL] ^= BIT(from);
            }
            else
            {
                board->board.occupancies[them] ^= (BIT(from) | BIT(to));
                board->board.occupancies[ALL] ^= (BIT(from) | BIT(to));
            }
            break;
        }
    }

    board->side_to_move = !board->side_to_move;
    if (!us) board->full_moves--;
}

/*
// **************** EVALUATION ************* \\
*/

const int relative_value[12] =
{
     100,  305,  320,  500,  1000,  10000, //MOCK, 
    -100, -305, -320, -500, -1000, -10000, //MOCK, 
};

const int pawn_eval_table_w[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 10, 10, 10, 10, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5, 
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5, 
     0,  0,  0,  0,  0,  0,  0,  0,
};

const int pawn_eval_table_b[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0, 
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 10, 10, 10, 10, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0,
};

const int knight_eval_table_w[64] = 
{
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30, 
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

const int knight_eval_table_b[64] = 
{
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

const int bishop_eval_table_w[64] = 
{
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};

const int bishop_eval_table_b[64] = 
{
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};

const int rook_eval_table_w[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0,
};

const int rook_eval_table_b[64] = 
{
     0,  0,  0,  5,  5,  0,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0,
};

const int queen_eval_table_w[64] = 
{
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10, 
    -20,-10,-10, -5, -5,-10,-10,-20,
};

const int queen_eval_table_b[64] = 
{
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
      0,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
};

const int king_eval_table_w[64] = 
{
     0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  5,  5,  5,  5,  0,  0,
     0,  5,  5, 10, 10,  5,  5,  0,
     0,  5, 10, 20, 20, 10,  5,  0,
     0,  5, 10, 20, 20, 10,  5,  0,
     0,  0,  5, 10, 10,  5,  0,  0,
     0,  5,  5, -5, -5,  0,  5,  0,
     0,  0,  5,  0,-15,  0, 10,  0,
};

const int king_eval_table_b[64] = 
{
     0,  0,  5,  0,-15,  0, 10,  0,
     0,  5,  5, -5, -5,  0,  5,  0,
     0,  5,  5, 10, 10,  5,  5,  0,
     0,  5, 10, 20, 20, 10,  5,  0,
     0,  5, 10, 20, 20, 10,  5,  0,
     0,  5,  5, 10, 10,  5,  0,  0,
     0,  0,  5,  5,  5,  5,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,
};

/* FOR THE FUTURE
const int king_eval_table_opening_w[64] = 
{
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
     20, 20,  0,-10,-10,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20,
};

const int king_eval_table_opening_b[64] = 
{
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,-10,-10,  0, 20, 20,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
};

const int king_eval_table_endgame_w[64] = 
{
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50,
};

const int king_eval_table_endgame_b[64] = 
{
    -50,-30,-30,-30,-30,-30,-30,-50,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -50,-40,-30,-20,-20,-30,-40,-50,
};
*/
/*
void flip_table(const int* original, int* flipped) 
{
    for (int i = 0; i < 8; i++) 
    {
        for (int j = 0; j < 8; j++) 
        {
            flipped[(7-i)*8 + j] = original[i * 8 + j];
        }
    }
}
*/

static inline int eval_position(const ChessBoard *board) 
{
    int score = 0;
    const int us = board->side_to_move;
    
    for (int piece = P; piece <= k; piece++) 
    {
        bb pieces = board->board.pieces[piece];
        while (pieces) 
        {
            int square = count_tz(pieces);
            score += relative_value[piece];
            
            switch(piece) 
            {
                case P:
                    score += pawn_eval_table_w[square];
                    break;
                case N:
                    score += knight_eval_table_w[square];
                    break;
                case B:
                    score += bishop_eval_table_w[square];
                    break;
                case R:
                    score += rook_eval_table_w[square];
                    break;
                case Q:
                    score += queen_eval_table_w[square];
                    break;
                case K:
                    score += king_eval_table_w[square];
                    break;
                case p:
                    score -= pawn_eval_table_b[square];
                    break;
                case n:
                    score -= knight_eval_table_b[square];
                    break;
                case b:
                    score -= bishop_eval_table_b[square];
                    break;
                case r:
                    score -= rook_eval_table_b[square];
                    break;
                case q:
                    score -= queen_eval_table_b[square];
                    break;
                case k:
                    score -= king_eval_table_b[square];
                    break;
            }
            CLEAR_LS1B(pieces);
        }
    }
    
    return !us ? score : -score;
}

/*
// **************** MOVE ORDERING - MVV LVA ************* \\
*/

static const int mvv_lva[12][12] = 
{
    // Victims              P    N    B    R    Q    K    p    n    b    r    q    k   <- (attackers)
    /* P victim */       { 105, 104, 103, 102, 101, 100, 105, 104, 103, 102, 101, 100 },
    /* N victim */       { 205, 204, 203, 202, 201, 200, 205, 204, 203, 202, 201, 200 },
    /* B victim */       { 305, 304, 303, 302, 301, 300, 305, 304, 303, 302, 301, 300 },
    /* R victim */       { 405, 404, 403, 402, 401, 400, 405, 404, 403, 402, 401, 400 },
    /* Q victim */       { 505, 504, 503, 502, 501, 500, 505, 504, 503, 502, 501, 500 },
    /* K victim */       { 605, 604, 603, 602, 601, 600, 605, 604, 603, 602, 601, 600 },
    /* p victim */       { 105, 104, 103, 102, 101, 100, 105, 104, 103, 102, 101, 100 },
    /* n victim */       { 205, 204, 203, 202, 201, 200, 205, 204, 203, 202, 201, 200 },
    /* b victim */       { 305, 304, 303, 302, 301, 300, 305, 304, 303, 302, 301, 300 },
    /* r victim */       { 405, 404, 403, 402, 401, 400, 405, 404, 403, 402, 401, 400 },
    /* q victim */       { 505, 504, 503, 502, 501, 500, 505, 504, 503, 502, 501, 500 },
    /* k victim */       { 605, 604, 603, 602, 601, 600, 605, 604, 603, 602, 601, 600 },
};

static inline int move_value(const ChessBoard* board, const moves* move) 
{
    int capture = get_move_capture(move);
    if (capture)  
    {
        int from_square = get_move_from_square(move);
        int attacker = -1;
        
        for (int piece = P; piece <= k; piece++) 
        {
            if (GET_BIT(board->board.pieces[piece], from_square)) 
            {
                attacker = piece;
                break;
            }
        }
    
        int victim = capture - 1; // capture stored as piece + 1
        
        return mvv_lva[victim][attacker];
    }
    
    return 0;
} 

static inline void swap_moves(moves* a, moves* b) 
{
    moves temp = *a;
    *a = *b;
    *b = temp;
}

static inline int partition(const ChessBoard* board, moves* movelist, int low, int high) 
{
    moves pivot = movelist[high];
    int pivot_score = move_value(board, &pivot);
    int i = low - 1;

    for (int j = low; j < high; j++) 
    {
        if (move_value(board, &movelist[j]) > pivot_score) 
        {
            i++;
            swap_moves(&movelist[i], &movelist[j]);
        }
    }
    swap_moves(&movelist[i + 1], &movelist[high]);
    return i + 1;
}

static inline void quicksort_moves(const ChessBoard* board, moves* movelist, int low, int high) 
{
    if (low < high) 
    {
        int pivot = partition(board, movelist, low, high);
        quicksort_moves(board, movelist, low, pivot - 1);
        quicksort_moves(board, movelist, pivot + 1, high);
    }
}

/*
// **************** ALPHA-BETA NEGAMAX ************* \\
*/

static int ply = 0;
static moves best_move = 0;
static long nodes = 0;

// quiescence search
static inline int quiescence(const ChessBoard* board, int alpha, int beta, int qdepth) 
{
    nodes++;

    if (qdepth <= 0)
        return eval_position(board);
    
    int current_eval = eval_position(board);
    
    if (current_eval >= beta)
    {
        return beta;
    }    
    if (current_eval > alpha)
    {
        alpha = current_eval;
    }    

    moves movelist[MAX_MOVES];
    moves* end = generate_legal_moves(board, movelist); 

    // Separate captures
    moves* capture_end = movelist;
    for (moves* move = movelist; move < end; move++) 
    {
        if (get_move_capture(move)) 
        {
            if (move != capture_end)  // Only swap if needed
            {
                moves temp = *capture_end;
                *capture_end = *move;
                *move = temp;
            }
            capture_end++;
        }
    }
    
    if (capture_end > movelist + 1) 
    {
        quicksort_moves(board, movelist, 0, (capture_end - movelist) - 1);
    }

    for (moves* move = movelist; move < capture_end; move++) 
    {  
        ply++;
        make_move((ChessBoard*)board, move);
        
        int score = -quiescence(board, -beta, -alpha, qdepth - 1);
        
        unmake_move((ChessBoard*)board, move);
        ply--;
        
        if (score >= beta)
            return beta;
            
        if (score > alpha)
            alpha = score;
    }
    
    return alpha;
}

// negamax (minmax->switch + alpha beta pruning)
static inline int negamax(const ChessBoard* board, int alpha, int beta, int depth, int qdepth) 
{
    if (depth == 0) 
    {
        return quiescence(board, alpha, beta, qdepth);
    }

    const int us = board->side_to_move;
    int king_square = count_tz(board->board.pieces[!us ? K : k]);
    bb attacked = get_attacked_squares(board, !us);
    
    if (GET_BIT(attacked, king_square))
    {
        depth++; 
    }

    nodes++;
    
    moves movelist[MAX_MOVES];
    moves* end = generate_legal_moves(board, movelist);

    if (!*movelist) 
    {
        if (GET_BIT(attacked, king_square)) 
        {
            return -50000 + ply;
        }
        return 0;
    }

    if (end > movelist + 1) 
    {
        quicksort_moves(board, movelist, 0, (end - movelist) - 1);
    }

    moves best_current = 0;
    const int old_alpha = alpha;
    
    for (moves* move = movelist; move < end; move++) 
    {
        ply++;
        make_move((ChessBoard*)board, move);
        
        int score = -negamax(board, -beta, -alpha, depth - 1, qdepth);
        
        unmake_move((ChessBoard*)board, move);  
        ply--;
        
        if (score >= beta) 
        {
            return beta;
        }
        
        if (score > alpha) 
        {
            alpha = score;
            best_current = *move;
            
            if (ply == 0) 
            {
                best_move = *move;
            }
        }
    }
    
    return alpha;
}

/*
// **************** UNIVERSAL CHESS INTERFACE, long algebraic notation ************* \\
*/ 

void search_position(const ChessBoard* board, int depth) 
{
    ply = 0;
    nodes = 0;
    best_move = 0;
    
    long long start_time = get_current_time_ms();
    int score = negamax(board, -50000, 50000, depth, 10);
    long long end_time = get_current_time_ms(); 
     
    const char* from_sq = SQUARE_TO_COORDINATES[get_move_from_square(&best_move)];
    const char* to_sq = SQUARE_TO_COORDINATES[get_move_target_square(&best_move)];
    
    if (get_move_flags(&best_move) == FLAG_PROMOTION) {
        char promotion_piece = 'q';
        switch(get_move_promotion(&best_move)) {
            case PROMOTE_KNIGHT: promotion_piece = 'n'; break;
            case PROMOTE_BISHOP: promotion_piece = 'b'; break;
            case PROMOTE_ROOK: promotion_piece = 'r'; break;
            default: break;
        }
        printf("bestmove %s%s%c\n", from_sq, to_sq, promotion_piece);
    } 
    else 
    {
        printf("bestmove %s%s\n", from_sq, to_sq);
    }
}

moves parse_move(const ChessBoard *board, const char* move_str) 
{   
    if (!isalpha(move_str[0]) || !isdigit(move_str[1]) || 
        !isalpha(move_str[2]) || !isdigit(move_str[3])) 
    {
        printf("\ninfo string Invalid move format");
        return 0;
    }
    
    int source_square = (move_str[0] - 'a') + ((8 - (move_str[1] - '0')) * 8);
    int target_square = (move_str[2] - 'a') + ((8 - (move_str[3] - '0')) * 8);
    
    if (source_square < 0 || source_square > 63 || target_square < 0 || target_square > 63) 
    {
        printf("\ninfo string Square coordinates out of bounds");
        return 0;
    }
    
    moves movelist[MAX_MOVES];
    moves* end = generate_legal_moves(board, movelist);
    
    char promotion_char = move_str[4];
    
    for (moves* move = movelist; move < end; move++) 
    {
        int move_from = get_move_from_square(move);
        int move_to = get_move_target_square(move);
        
        if (move_from == source_square && move_to == target_square) 
        {
            if (get_move_flags(move) == FLAG_PROMOTION) 
            {
                if (!promotion_char) continue;
                
                int promotion_type;
                switch(promotion_char) 
                {
                    case 'q': promotion_type = PROMOTE_QUEEN; break;
                    case 'r': promotion_type = PROMOTE_ROOK; break;
                    case 'b': promotion_type = PROMOTE_BISHOP; break;
                    case 'n': promotion_type = PROMOTE_KNIGHT; break;
                    default: continue;
                }
                
                if (get_move_promotion(move) == promotion_type) 
                {
                    return *move;
                }
            } 
            else  
            {
                return *move;
            }
        }
    }
    
    printf("\ninfo string No valid move found");
    return 0;
}

void parse_position(ChessBoard *board, char *command) 
{   
    int error;
    command += 9; 
    
    if (strncmp(command, "startpos", 8) == 0) 
    {
        char* start_pos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        parse_fen(board, start_pos);
        command += 8;
    }
    else 
    {
        char *fen_location = strstr(command, "fen");
        if (fen_location == NULL) 
        {
            printf("\ninfo string No FEN found, using start position");
            char* start_pos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
            parse_fen(board, start_pos); 
        }
        else 
        {   
            fen_location += 4;
            error = parse_fen(board, fen_location);
            if (error != FEN_OKAY)
            {
                printf("info string Invalid FEN: %s\nSetting up startpos...\n", get_error(error));
                char* start_pos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
                parse_fen(board, start_pos); 
            }
        }
    }
    
    if (error != 0)
    {
        get_error(error);
    }

    char *moves_section = strstr(command, "moves");
    if (moves_section != NULL) 
    {
        moves_section += 6;
        
        while (*moves_section) 
        {
            while (*moves_section == ' ') moves_section++;
            if (!*moves_section) break;
            
            moves move = parse_move(board, moves_section);
            
            if (move != 0) 
            {
                make_move(board, &move);
            }
            else 
            {
                break;
            }
            
            while (*moves_section && *moves_section != ' ') moves_section++;
        }
    }
    printf("\n");
    print_chessboard(board);
}

void parse_go(const ChessBoard *board, char *command) 
{
    int depth = 4; 
    const int MAX_DEPTH = 8;
    char *depth_cmd = strstr(command, "depth");
    
    if (depth_cmd) 
    {
        char *num_start = depth_cmd + 6;
        while (*num_start == ' ') num_start++;  
        
        if (isdigit(*num_start)) 
        {
            int parsed_depth = atoi(num_start);
            if (parsed_depth > MAX_DEPTH) 
            {
                printf("info string Maximum depth is 8. Using depth 8.\n");
                depth = MAX_DEPTH;
            }
            else if (parsed_depth <= 0) 
            {
                printf("info string Invalid depth. Using default depth 4.\n");
                depth = 4;
            }
            else 
            {
                depth = parsed_depth;
            }
        }
        else 
        {
            printf("info string No valid depth specified. Using default depth 4.\n");
        }
    }
    
    search_position(board, depth);
}

void engine_loop() 
{
    ChessBoard board;
    char input[2048];
    
    setbuf(stdin, NULL);
    setbuf(stdout, NULL);
    
    printf("info string Initializing engine\n");
    init_all();
    
    while (true) 
    {
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) continue;
        if (input[0] == '\n') continue;
        
        if (!strncmp(input, "uci", 3)) 
        {
            printf("funnyEngine\n");
            printf("Alessandro Bellini\n");
            printf("uciok\n");
        }
        else if (!strncmp(input, "isready", 7)) 
        {
            printf("readyok\n");
        }
        else if (!strncmp(input, "ucinewgame", 9)) 
        {
            char* start_pos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
            parse_fen(&board, start_pos);
        }
        else if (!strncmp(input, "position", 8)) 
        {
            parse_position(&board, input);
        }
        else if (!strncmp(input, "go", 2)) 
        {
            parse_go(&board, input);
        }
        else if (!strncmp(input, "stop", 4))
        {
            printf("bestmove e2e4\n");  // Always respond to stop with a move
        }
        else if (!strncmp(input, "quit", 4)) 
        {
            printf("info string Quitting engine\n");
            break;
        }
        else if (!strncmp(input, "d", 1)) 
        {
            print_chessboard(&board);
        }
        
        fflush(stdout);
    }
}

/*
// **************** MAIN ************* \\
*/ 

int main()
{
    engine_loop();
}

