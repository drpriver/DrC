#ifdef __linux__
#pragma pkg_config "sdl2"
#else
#pragma lib "SDL2"
#endif
#include <SDL2/SDL.h> <SDL.h>

#if !defined NO_SDL_MIXER
#ifdef __linux__
#pragma pkg_config "SDL2_mixer"
#else
#pragma lib "SDL2_mixer"
#endif
#include <SDL2_mixer/SDL_mixer.h> <SDL2/SDL_mixer.h> <SDL_mixer.h>
#endif

#include "bitmap_font.h"

enum { GAME_W = 600, SIDE_W = 200, WIDTH = GAME_W + SIDE_W, HEIGHT = 520 };
enum { MAX_CARDS = 12, MAX_HANDS = 4 };
enum { ANIM_MS = 250, ANIM_STAGGER = 150 };
enum { MIN_BET = 10, BET_STEP = 10 };
enum { NUM_CHECKS = 13 };
enum { CHIP_ANIM_MS = 300, CHIP_STAGGER = 60, MAX_CHIP_ANIMS = 40 };

typedef struct Hand Hand;
struct Hand {
    int cards[MAX_CARDS];
    unsigned deal_time[MAX_CARDS];
    int count;
    int bet;
    _Bool done;
    _Bool surrendered;
    _Bool doubled;
    _Bool from_split;
};

typedef struct ChipAnim ChipAnim;
struct ChipAnim {
    int src_x, src_y, dst_x, dst_y;
    int color;
    unsigned start_time;
};

typedef struct Button Button;
struct Button {
    const char* label;
    int x, y, w;
    _Bool active;
    _Bool visible;
};

typedef struct Check Check;
struct Check {
    const char* label;
    _Bool* flag;
};

int num_decks = 1; // 0 = infinite
int deck_size = 52;
int deck[6 * 52]; // max 6 decks
int deck_pos;
_Bool need_shuffle;
Hand hands[MAX_HANDS];
int num_hands;
int active_hand;

int dealer_cards[MAX_CARDS], dealer_count;
unsigned dealer_deal_time[MAX_CARDS];
_Bool dealer_reveal;
unsigned deal_timer;
unsigned anim_done_time;

ChipAnim chip_anims[MAX_CHIP_ANIMS];
int num_chip_anims;
unsigned chip_anim_done;
int deal_sounds_played; // how many deal sounds have been triggered this round
int chip_sounds_played; // how many chip sounds have been triggered

enum { PHASE_PLAY, PHASE_INSURANCE, PHASE_EVEN_MONEY, PHASE_DONE } phase;
int insurance_bet;

int bankroll;
int next_bet; // persists between rounds

enum { R_NONE, R_WIN, R_LOSE, R_PUSH, R_BJ, R_SURRENDER, R_CHARLIE } hand_results[MAX_HANDS];
int hand_payouts[MAX_HANDS];

int total_won, total_lost, rounds_played;
_Bool bet_modified; // sticky: set when bet buttons pressed, cleared on start_round
SDL_Texture *suit_tex,
            *chip_tex,
            *font_tex;
SDL_Renderer *ren;

enum { NUM_CARD_SNDS = 8, NUM_CHIP_SNDS = 3 };
#ifdef NO_SDL_MIXER
typedef void Mix_Chunk;
#endif
Mix_Chunk *snd_card_slide[NUM_CARD_SNDS];
Mix_Chunk *snd_chip_lay[NUM_CHIP_SNDS];
int volume = 32; // 0..128
_Bool muted;

_Bool rule_split       = 1; // allow splitting pairs
_Bool rule_resplit     = 1; // re-split up to 4 hands
_Bool rule_das         = 1; // double after split
_Bool rule_hit_aces    = 1; // hit split aces
_Bool rule_charlie     = 1; // 5-card charlie wins
_Bool rule_surrender   = 1; // late surrender
_Bool rule_s17         = 1; // dealer stands on soft 17 (off = hits soft 17)
_Bool rule_bj_3to2     = 1; // blackjack pays 3:2 (off = pays 1:1)
_Bool rule_insurance   = 1; // offer insurance vs ace
_Bool rule_split_tens  = 1; // split any 10-value (off = must match rank)
_Bool rule_split_bj    = 0; // BJ after split pays 3:2 (off = counts as 21)
_Bool rule_dbl_any     = 0; // double on any number of cards (off = first two only)
_Bool rule_21_wins     = 0; // player 21 is automatic winner

enum {
    B_HIT, B_STAND, B_DOUBLE, B_SPLIT, B_SURRENDER, B_DEAL,
    B_INSURE, B_DECLINE,
    B_BET_DN, B_BET_UP, B_BET_HALF, B_BET_DBL,
    B_DECKS,
    B_COUNT
};
Button buttons[B_COUNT];

Check checks[NUM_CHECKS] = {
    {"SPLIT", &rule_split},
    {"RESPLIT", &rule_resplit},
    {"SPLIT ANY 10", &rule_split_tens},
    {"DOUBLE AFTER SPLIT", &rule_das},
    {"HIT ACES AFTER SPLIT", &rule_hit_aces},
    {"5-CARD CHARLIE", &rule_charlie},
    {"SURRENDER", &rule_surrender},
    {"INSURANCE", &rule_insurance},
    {"STAND 17", &rule_s17},
    {"BJ 3-TO-2", &rule_bj_3to2},
    {"BJ AFTER SPLIT", &rule_split_bj},
    {"DOUBLE ANY NUMBER", &rule_dbl_any},
    {"21 AUTO-WIN", &rule_21_wins},
};


unsigned rng_state;

unsigned rng(void);
void format_money(char* buf, int size, int val);
void shuffle(void);
void cycle_decks(void);
unsigned next_card_time(void);
int deal_card(void);
int hand_value(int* cards, int count);
_Bool hand_soft(int* cards, int count);
_Bool can_split(void);
_Bool can_double(void);
_Bool can_surrender(void);
_Bool is_blackjack(Hand* h);
int bj_payout(int bet);
void start_round(void);
void check_bj(void);
void do_insurance(_Bool accept);
void do_even_money(_Bool accept);
void do_hit(void);
void do_stand(void);
void do_double(void);
void do_split(void);
void do_surrender(void);
void advance_hand(void);
void dealer_play(void);
void settle(void);
void queue_settle_chips(void);
int hand_cx(int i);
void draw(void);
void init_textures(void),
     init_font(void),
     init_suits(void),
     init_chips(void);
void init_sounds(void);
void cleanup_sounds(void);
void apply_volume(void);
void play_sound(Mix_Chunk* snd);
int button_hit(int mx, int my);
int check_hit(int mx, int my);
_Bool click_volume(int mx, int my);

int
main(void){
    rng_state = (unsigned)SDL_GetPerformanceCounter();
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    #ifndef NO_SDL_MIXER
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    #endif
    init_sounds();
    SDL_Window* win = SDL_CreateWindow("Blackjack",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        2*WIDTH, 2*HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(ren, WIDTH, HEIGHT);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    init_textures();
    bankroll = 2000;
    next_bet = 50;
    shuffle();
    phase = PHASE_DONE;
    for(;;){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            switch(ev.type){
            case SDL_QUIT:
                goto finally;
                break;
            case SDL_KEYDOWN:
                switch(ev.key.keysym.sym){
                case SDLK_ESCAPE: case SDLK_q:
                    goto finally;
                    break;
                case SDLK_SLASH:
                case SDLK_h:
                    if(phase == PHASE_PLAY) do_hit();
                    break;
                case SDLK_s:
                    hit_s:
                    if(phase == PHASE_PLAY) do_stand();
                    break;
                case SDLK_PERIOD:
                case SDLK_d:
                    if(phase == PHASE_PLAY) do_double();
                    break;
                case SDLK_p:
                    if(phase == PHASE_PLAY) do_split();
                    break;
                case SDLK_r:
                    if(phase == PHASE_PLAY) do_surrender();
                    break;
                case SDLK_y: case SDLK_i:
                    if(phase == PHASE_EVEN_MONEY) do_even_money(1);
                    else if(phase == PHASE_INSURANCE) do_insurance(1);
                    break;
                case SDLK_SPACE:
                case SDLK_n: case SDLK_RETURN:
                    if(phase == PHASE_EVEN_MONEY) do_even_money(0);
                    else if(phase == PHASE_INSURANCE) do_insurance(0);
                    else if(phase == PHASE_DONE) start_round();
                    else goto hit_s;
                    break;
                case SDLK_UP:
                    next_bet *= 2;
                    next_bet -= next_bet % BET_STEP;
                    if(next_bet > bankroll) next_bet = bankroll;
                    bet_modified = 1;
                    break;
                case SDLK_DOWN:
                    next_bet /= 2;
                    next_bet -= next_bet % BET_STEP;
                    if(next_bet < MIN_BET) next_bet = MIN_BET;
                    bet_modified = 1;
                    break;
                case SDLK_m:
                    muted = !muted;
                    apply_volume();
                    break;
                }
                break;
            case SDL_MOUSEBUTTONDOWN: {
                int mx = ev.button.x, my = ev.button.y;
                // Check buttons
                int btn = button_hit(mx, my);
                switch(btn){
                case B_HIT:       do_hit(); break;
                case B_STAND:     do_stand(); break;
                case B_DOUBLE:    do_double(); break;
                case B_SPLIT:     do_split(); break;
                case B_SURRENDER: do_surrender(); break;
                case B_DEAL:      if(phase == PHASE_DONE) start_round(); break;
                case B_INSURE:
                    if(phase == PHASE_EVEN_MONEY) do_even_money(1);
                    else do_insurance(1);
                    break;
                case B_DECLINE:
                    if(phase == PHASE_EVEN_MONEY) do_even_money(0);
                    else do_insurance(0);
                    break;
                case B_BET_DN:
                    next_bet -= BET_STEP;
                    if(next_bet < MIN_BET) next_bet = MIN_BET;
                    bet_modified = 1;
                    break;
                case B_BET_UP:
                    next_bet += BET_STEP;
                    if(next_bet > bankroll) next_bet = bankroll;
                    bet_modified = 1;
                    break;
                case B_BET_HALF:
                    next_bet /= 2;
                    next_bet -= next_bet % BET_STEP;
                    if(next_bet < MIN_BET) next_bet = MIN_BET;
                    bet_modified = 1;
                    break;
                case B_BET_DBL:
                    next_bet *= 2;
                    next_bet -= next_bet % BET_STEP;
                    if(next_bet > bankroll) next_bet = bankroll;
                    bet_modified = 1;
                    break;
                case B_DECKS: cycle_decks(); break;
                }
                // Check checkboxes
                int cb = check_hit(mx, my);
                if(cb >= 0) *checks[cb].flag = !*checks[cb].flag;
                // Volume bar
                if(click_volume(mx, my)){
                }
                break;
            }
            }
        }
        draw();
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }
    finally:
    cleanup_sounds();
    #ifndef NO_SDL_MIXER
    Mix_CloseAudio();
    #endif
    SDL_DestroyTexture(chip_tex);
    SDL_DestroyTexture(suit_tex);
    SDL_DestroyTexture(font_tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

void
format_money(char* buf, int size, int val){
    int neg = val < 0;
    int a = neg ? -val : val;
    if(a & 1)
        SDL_snprintf(buf, size, "%s$%d.5", neg ? "-" : "", a / 2);
    else
        SDL_snprintf(buf, size, "%s$%d", neg ? "-" : "", a / 2);
}

void
shuffle(void){
    for(int i = 0; i < deck_size; i++) deck[i] = i % 52;
    for(int i = deck_size - 1; i > 0; i--){
        int j = rng() % (i + 1);
        int t = deck[i]; deck[i] = deck[j]; deck[j] = t;
    }
    deck_pos = 0;
}

void
cycle_decks(void){
    switch(num_decks){
    case 1: num_decks = 2; break;
    case 2: num_decks = 4; break;
    case 4: num_decks = 6; break;
    case 6: num_decks = 0; break;
    default: num_decks = 1; break;
    }
    deck_size = num_decks ? num_decks * 52 : 52;
    shuffle();
    need_shuffle = 0;
}

unsigned
next_card_time(void){
    unsigned now = SDL_GetTicks();
    if(deal_timer < now) deal_timer = now;
    unsigned t = deal_timer;
    deal_timer += ANIM_STAGGER;
    anim_done_time = t + ANIM_MS;
    return t;
}

int
deal_card(void){
    if(num_decks == 0) return rng() % 52; // infinite deck
    if(deck_pos >= deck_size / 2) need_shuffle = 1; // cut card at 50%
    return deck[deck_pos++];
}

int
hand_value(int* cards, int count){
    int total = 0, aces = 0;
    for(int i = 0; i < count; i++){
        int rank = cards[i] % 13;
        if(rank == 0){ total += 11; aces++; }
        else if(rank >= 10) total += 10;
        else total += rank + 1;
    }
    while(total > 21 && aces > 0){ total -= 10; aces--; }
    return total;
}

_Bool
hand_soft(int* cards, int count){
    int total = 0, aces = 0;
    for(int i = 0; i < count; i++){
        int rank = cards[i] % 13;
        if(rank == 0){ total += 11; aces++; }
        else if(rank >= 10) total += 10;
        else total += rank + 1;
    }
    while(total > 21 && aces > 0){ total -= 10; aces--; }
    return aces > 0;
}

_Bool
is_blackjack(Hand* h){
    return h->count == 2 && !h->from_split && hand_value(h->cards, h->count) == 21;
}

int
bj_payout(int bet){
    return rule_bj_3to2 ? bet + bet * 3 / 2 : bet * 2;
}

_Bool
can_split(void){
    if(phase != PHASE_PLAY) return 0;
    if(!rule_split) return 0;
    if(!rule_resplit && num_hands > 1) return 0;
    if(num_hands >= MAX_HANDS) return 0;
    Hand* h = &hands[active_hand];
    if(h->count != 2) return 0;
    if(bankroll < h->bet) return 0;
    int r0 = h->cards[0] % 13;
    int r1 = h->cards[1] % 13;
    if(r0 == r1) return 1;
    // Mismatched 10-value cards (e.g. J+Q) only if rule allows
    if(rule_split_tens && r0 >= 9 && r1 >= 9) return 1;
    return 0;
}

_Bool
can_double(void){
    if(phase != PHASE_PLAY) return 0;
    Hand* h = &hands[active_hand];
    if(!rule_dbl_any && h->count != 2) return 0;
    if(h->from_split && !rule_das) return 0;
    if(bankroll <= 0) return 0;
    return 1;
}

_Bool
can_surrender(void){
    if(phase != PHASE_PLAY) return 0;
    if(!rule_surrender) return 0;
    Hand* h = &hands[active_hand];
    if(h->count != 2) return 0;
    if(h->from_split) return 0;
    if(num_hands > 1) return 0;
    return 1;
}

void
start_round(void){
    int bet = next_bet;
    if(bet > bankroll) bet = bankroll;
    if(bet <= 0) return;
    if(need_shuffle){ shuffle(); need_shuffle = 0; }
    num_hands = 1;
    active_hand = 0;
    dealer_reveal = 0;
    dealer_count = 0;
    insurance_bet = 0;
    rounds_played++;
    SDL_memset(hand_results, 0, sizeof hand_results);
    SDL_memset(hand_payouts, 0, sizeof hand_payouts);
    num_chip_anims = 0;
    chip_anim_done = 0;
    deal_sounds_played = 0;
    chip_sounds_played = 0;
    bet_modified = 0;

    Hand* h = &hands[0];
    SDL_memset(h, 0, sizeof *h);
    h->bet = bet;
    bankroll -= bet;

    h->deal_time[h->count] = next_card_time();
    h->cards[h->count++] = deal_card();
    dealer_deal_time[dealer_count] = next_card_time();
    dealer_cards[dealer_count++] = deal_card();
    h->deal_time[h->count] = next_card_time();
    h->cards[h->count++] = deal_card();
    dealer_deal_time[dealer_count] = next_card_time();
    dealer_cards[dealer_count++] = deal_card();

    // Offer even money if player has BJ and dealer shows ace
    int uprank = dealer_cards[0] % 13;
    if(rule_insurance && uprank == 0 && is_blackjack(h)){
        phase = PHASE_EVEN_MONEY;
        return;
    }
    // Offer insurance if dealer shows ace
    if(rule_insurance && uprank == 0 && bankroll >= h->bet / 2){
        phase = PHASE_INSURANCE;
        return;
    }
    check_bj();
}

void
check_bj(void){
    Hand* h = &hands[0];
    _Bool player_bj = is_blackjack(h);
    // Dealer peek: if upcard is ace or 10-value, check for dealer blackjack
    int uprank = dealer_cards[0] % 13;
    _Bool peek_card = (uprank == 0 || uprank >= 10);
    _Bool dealer_bj = peek_card && (hand_value(dealer_cards, dealer_count) == 21);
    // Settle insurance side bet
    if(insurance_bet > 0){
        if(dealer_bj){
            bankroll += insurance_bet * 3;
            total_won += insurance_bet * 2;
        }
        else {
            total_lost += insurance_bet;
        }
    }
    if(player_bj || dealer_bj){
        h->done = 1;
        dealer_reveal = 1;
        phase = PHASE_DONE;
        if(player_bj && dealer_bj){
            hand_results[0] = R_PUSH;
            bankroll += h->bet;
            hand_payouts[0] = 0;
        }
        else if(player_bj){
            int payout = bj_payout(h->bet);
            bankroll += payout;
            hand_payouts[0] = payout - h->bet;
            hand_results[0] = R_BJ;
            total_won += hand_payouts[0];
        }
        else {
            hand_results[0] = R_LOSE;
            hand_payouts[0] = -h->bet;
            total_lost += h->bet;
        }
        queue_settle_chips();
        return;
    }
    phase = PHASE_PLAY;
}

void
do_even_money(_Bool accept){
    if(phase != PHASE_EVEN_MONEY) return;
    Hand* h = &hands[0];
    if(accept){
        // Pay 1:1 immediately, done
        h->done = 1;
        dealer_reveal = 1;
        phase = PHASE_DONE;
        bankroll += h->bet * 2;
        hand_payouts[0] = h->bet;
        hand_results[0] = R_BJ;
        total_won += h->bet;
        queue_settle_chips();
    }
    else {
        // Decline even money, proceed to normal check_bj
        check_bj();
    }
}

void
do_insurance(_Bool accept){
    if(phase != PHASE_INSURANCE) return;
    if(accept){
        insurance_bet = hands[0].bet / 2;
        bankroll -= insurance_bet;
    }
    check_bj();
}

void
advance_hand(void){
    for(;;){
        active_hand++;
        if(active_hand >= num_hands){
            dealer_play();
            return;
        }
        if(!hands[active_hand].done) return;
    }
}

void
do_hit(void){
    if(phase != PHASE_PLAY) return;
    Hand* h = &hands[active_hand];
    if(h->done) return;
    h->deal_time[h->count] = next_card_time();
    h->cards[h->count++] = deal_card();
    int val = hand_value(h->cards, h->count);
    if(val >= 21){
        h->done = 1;
        advance_hand();
    }
    else if(rule_charlie && h->count >= 5){
        h->done = 1;
        advance_hand();
    }
}

void
do_stand(void){
    if(phase != PHASE_PLAY) return;
    Hand* h = &hands[active_hand];
    if(h->done) return;
    h->done = 1;
    advance_hand();
}

void
do_double(void){
    if(!can_double()) return;
    Hand* h = &hands[active_hand];
    int extra = h->bet;
    if(extra > bankroll) extra = bankroll;
    bankroll -= extra;
    h->bet += extra;
    h->doubled = 1;
    h->deal_time[h->count] = next_card_time();
    h->cards[h->count++] = deal_card();
    h->done = 1;
    advance_hand();
}

void
do_split(void){
    if(!can_split()) return;
    Hand* h = &hands[active_hand];
    Hand* nh = &hands[num_hands];
    SDL_memset(nh, 0, sizeof *nh);
    nh->bet = h->bet;
    nh->from_split = 1;
    bankroll -= nh->bet;
    nh->deal_time[0] = h->deal_time[1];
    nh->cards[nh->count++] = h->cards[1];

    h->count = 1;
    h->from_split = 1;
    h->deal_time[h->count] = next_card_time();
    h->cards[h->count++] = deal_card();
    nh->deal_time[nh->count] = next_card_time();
    nh->cards[nh->count++] = deal_card();

    num_hands++;
    // If split aces and can't hit them, auto-stand
    if(!rule_hit_aces && (h->cards[0] % 13) == 0){
        h->done = 1;
        nh->done = 1;
        advance_hand();
        return;
    }
    // 21 after split auto-stands
    if(hand_value(nh->cards, nh->count) == 21) nh->done = 1;
    if(hand_value(h->cards, h->count) == 21){
        h->done = 1;
        advance_hand();
    }
}

void
do_surrender(void){
    if(!can_surrender()) return;
    Hand* h = &hands[active_hand];
    h->done = 1;
    h->surrendered = 1;
    int returned = h->bet / 2;
    bankroll += returned;
    hand_results[active_hand] = R_SURRENDER;
    hand_payouts[active_hand] = -(h->bet - returned);
    total_lost += h->bet - returned;
    advance_hand();
}

void
dealer_play(void){
    dealer_reveal = 1;
    phase = PHASE_DONE;
    _Bool all_resolved = 1;
    for(int i = 0; i < num_hands; i++){
        if(hands[i].surrendered) continue;
        if(hand_value(hands[i].cards, hands[i].count) > 21) continue;
        all_resolved = 0;
    }
    if(!all_resolved){
        for(;;){
            int dv = hand_value(dealer_cards, dealer_count);
            if(dv > 17) break;
            if(dv == 17){
                if(rule_s17) break; // stand on soft 17
                if(!hand_soft(dealer_cards, dealer_count)) break; // hard 17 always stands
            }
            if(deck_pos >= deck_size) break;
            dealer_deal_time[dealer_count] = next_card_time();
            dealer_cards[dealer_count++] = deal_card();
        }
    }
    settle();
    queue_settle_chips();
}

void
settle(void){
    int dv = hand_value(dealer_cards, dealer_count);
    _Bool dealer_bust = dv > 21;

    for(int i = 0; i < num_hands; i++){
        if(hand_results[i] != R_NONE) continue;
        Hand* h = &hands[i];
        int pv = hand_value(h->cards, h->count);

        if(pv > 21){
            hand_results[i] = R_LOSE;
            hand_payouts[i] = -h->bet;
            total_lost += h->bet;
        }
        else if(rule_split_bj && h->from_split && h->count == 2 && pv == 21){
            int payout = bj_payout(h->bet);
            bankroll += payout;
            hand_payouts[i] = payout - h->bet;
            hand_results[i] = R_BJ;
            total_won += hand_payouts[i];
        }
        else if(rule_charlie && h->count >= 5){
            hand_results[i] = R_CHARLIE;
            bankroll += h->bet * 2;
            hand_payouts[i] = h->bet;
            total_won += h->bet;
        }
        else if((rule_21_wins && pv == 21) || dealer_bust || pv > dv){
            hand_results[i] = R_WIN;
            bankroll += h->bet * 2;
            hand_payouts[i] = h->bet;
            total_won += h->bet;
        }
        else if(dv > pv){
            hand_results[i] = R_LOSE;
            hand_payouts[i] = -h->bet;
            total_lost += h->bet;
        }
        else {
            hand_results[i] = R_PUSH;
            bankroll += h->bet;
            hand_payouts[i] = 0;
        }
    }
}

// --- Layout constants ---
enum { CARD_W = 64, CARD_H = 90, CARD_OVERLAP = 22 };
enum { CARD_RANK_SZ = 1, CARD_SUIT_SZ = 2, CARD_RANK_PAD = 4 };
enum { SIDE_PAD = 10, SIDE_X = GAME_W + SIDE_PAD, SIDE_INNER_W = SIDE_W - 2 * SIDE_PAD };
enum { CB_SIZE = 16, CB_PAD = 4, CB_ROW_EXTRA = 4, CB_LABEL_GAP = 6, CB_CHECK_INSET = 2 };
enum { CB_START_Y = 30, RULES_LABEL_GAP = 18 };
enum { VOL_LABEL_Y = 358, VOL_BAR_Y = 376, VOL_BAR_H = 10, MUTE_Y = 396 };
enum { DECK_LABEL_Y = 456 };
enum { SHOE_Y = 474, SHOE_BAR_DY = 12, SHOE_BAR_H = 10 };
enum { STATS_BOT = 40, STATS_LINE_H = 18 };
enum { BTN_H = 30, BTN_PAD = 8, BTN_BOT = 44 };
enum { PLAY_BTN_W = 80, DEAL_BTN_W = 140 };
enum { DEALER_LABEL_Y = 18, DEALER_CARDS_Y = 38 };
enum { PLAYER_CARDS_Y = 190 };
enum { CARDS_VAL_GAP = 18 };
enum { VAL_BET_GAP = 26, VAL_RESULT_GAP = 52, RESULT_PAY_GAP = 28 };
enum { ACTIVE_IND_PAD = 8, ACTIVE_IND_EXTRA_H = 56 };
enum { SHOE_PILE_X = GAME_W - CARD_W - 20, SHOE_MAX_LAYERS = 8 };
enum { CHIP_R = 21, CHIP_W = 48, CHIP_H = 48, CHIP_STACK_DY = 6 };
enum { HOUSE_PILE_X = 40 };

void
init_font(void){
    int atlas_w = FONT_COUNT * FONT_W;
    SDL_Surface* surf = SDL_CreateRGBSurface(0, atlas_w, FONT_H, 32,
        0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    SDL_LockSurface(surf);
    unsigned* pixels = (unsigned*)surf->pixels;
    for(int ch = 0; ch < FONT_COUNT; ch++){
        for(int row = 0; row < FONT_H; row++){
            unsigned char bits = font8x16[ch][row];
            for(int col = 0; col < FONT_W; col++){
                int px = ch * FONT_W + col;
                if(bits & (0x80 >> col))
                    pixels[row * atlas_w + px] = 0xffffffff;
                else
                    pixels[row * atlas_w + px] = 0x00000000;
            }
        }
    }
    SDL_UnlockSurface(surf);
    font_tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    SDL_SetTextureBlendMode(font_tex, SDL_BLENDMODE_BLEND);
}

// Suit bitmaps 11x11. Top 11 bits of each short are the pixels.
enum { SUIT_W = 11, SUIT_H = 11 };
static const unsigned short suit_bm[4][SUIT_H] = {
    { // spade
        0b00000100000 << 5,
        0b00001110000 << 5,
        0b00011111000 << 5,
        0b00111111100 << 5,
        0b01111111110 << 5,
        0b11111111111 << 5,
        0b11111111111 << 5,
        0b11111011111 << 5,
        0b01110001110 << 5,
        0b00000100000 << 5,
        0b00011111000 << 5,
    },
    { // heart
        0b00000000000 << 5,
        0b01110001110 << 5,
        0b11111011111 << 5,
        0b11111111111 << 5,
        0b11111111111 << 5,
        0b01111111110 << 5,
        0b00111111100 << 5,
        0b00011111000 << 5,
        0b00001110000 << 5,
        0b00000100000 << 5,
        0b00000000000 << 5,
    },
    { // diamond
        0b00000100000 << 5,
        0b00001110000 << 5,
        0b00011111000 << 5,
        0b00111111100 << 5,
        0b01111111110 << 5,
        0b11111111111 << 5,
        0b01111111110 << 5,
        0b00111111100 << 5,
        0b00011111000 << 5,
        0b00001110000 << 5,
        0b00000100000 << 5,
    },
    { // club
        0b00001110000 << 5,
        0b00011111000 << 5,
        0b00011111000 << 5,
        0b01101110110 << 5,
        0b11110101111 << 5,
        0b11111111111 << 5,
        0b11111111111 << 5,
        0b01110101110 << 5,
        0b00000100000 << 5,
        0b00001110000 << 5,
        0b00011111000 << 5,
    },
};

// Suit texture: 4 suits side by side, 11x11 each, white pixels.
void
init_suits(void){
    int atlas_w = 4 * SUIT_W;
    SDL_Surface* surf = SDL_CreateRGBSurface(0, atlas_w, SUIT_H, 32,
        0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    SDL_LockSurface(surf);
    unsigned* pixels = (unsigned*)surf->pixels;
    for(int s = 0; s < 4; s++){
        for(int row = 0; row < SUIT_H; row++){
            unsigned short bits = suit_bm[s][row];
            for(int col = 0; col < SUIT_W; col++){
                int px = s * SUIT_W + col;
                if(bits & (0x8000 >> col))
                    pixels[row * atlas_w + px] = 0xffffffff;
                else
                    pixels[row * atlas_w + px] = 0x00000000;
            }
        }
    }
    SDL_UnlockSurface(surf);
    suit_tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    SDL_SetTextureBlendMode(suit_tex, SDL_BLENDMODE_BLEND);
}

// Chip texture: white circle with smooth antialiased edges.
void
init_chips(void){
    SDL_Surface* surf = SDL_CreateRGBSurface(0, CHIP_W, CHIP_H, 32,
        0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    SDL_LockSurface(surf);
    unsigned* pixels = (unsigned*)surf->pixels;
    float cx = (CHIP_W - 1) * 0.5f;
    float cy = (CHIP_H - 1) * 0.5f;
    float r_outer = CHIP_R;
    float r_inner = CHIP_R - 2.0f;
    for(int y = 0; y < CHIP_H; y++){
        for(int x = 0; x < CHIP_W; x++){
            float dx = x - cx;
            float dy = y - cy;
            float dist = SDL_sqrtf(dx*dx + dy*dy);
            // Smooth outer edge: 1px feather
            float a = r_outer + 0.5f - dist;
            if(a > 1.0f) a = 1.0f;
            if(a <= 0.0f){ pixels[y * CHIP_W + x] = 0x00000000; continue; }
            // Rim blend: white inside, gray at edge
            int gray;
            if(dist <= r_inner - 0.5f)
                gray = 255;
            else if(dist >= r_outer - 0.5f)
                gray = 0x88;
            else {
                float t = (dist - (r_inner - 0.5f)) / (r_outer - r_inner);
                if(t < 0.0f) t = 0.0f;
                if(t > 1.0f) t = 1.0f;
                gray = 255 - (int)(t * (255 - 0x88));
            }
            unsigned ai = (unsigned)(a * 255.0f);
            pixels[y * CHIP_W + x] = (ai << 24)
                | ((unsigned)gray << 16)
                | ((unsigned)gray << 8)
                | (unsigned)gray;
        }
    }
    SDL_UnlockSurface(surf);
    chip_tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    SDL_SetTextureBlendMode(chip_tex, SDL_BLENDMODE_BLEND);
}

void
draw_chip(int x, int y, int color){
    unsigned char r, g, b;
    switch(color){
        case 0: r=0xcc; g=0x22; b=0x22; break; // red $5
        case 1: r=0x22; g=0x88; b=0x22; break; // green $25
        case 2: r=0x33; g=0x33; b=0x33; break; // black $100
        case 3: r=0xdd; g=0xdd; b=0xdd; break; // white $1
        default: r=0x88; g=0x22; b=0xcc; break; // purple $500
    }
    SDL_SetTextureColorMod(chip_tex, r, g, b);
    SDL_Rect dst = {x - CHIP_W/2, y - CHIP_H/2, CHIP_W, CHIP_H};
    SDL_RenderCopy(ren, chip_tex, 0, &dst);
}

enum { NUM_DENOMS = 5 };
const int denom_values[NUM_DENOMS] = {500, 100, 25, 5, 1};
const int denom_colors[NUM_DENOMS] = {4, 2, 1, 0, 3};

void
draw_chip_stack(int x, int y, int amount){
    amount /= 2; // half-dollars to dollars
    int n = 0;
    for(int d = 0; d < NUM_DENOMS; d++){
        int count = amount / denom_values[d];
        amount %= denom_values[d];
        for(int i = 0; i < count && n < 15; i++, n++)
            draw_chip(x, y - n * CHIP_STACK_DY, denom_colors[d]);
    }
}

int
hand_cx(int i){
    if(num_hands == 1) return GAME_W / 2;
    if(num_hands == 2) return GAME_W / 4 + i * GAME_W / 2;
    if(num_hands == 3) return GAME_W / 6 + i * GAME_W / 3;
    return GAME_W / 8 + i * GAME_W / 4;
}

void
queue_chip_flow(int amount, int from_x, int from_y, int to_x, int to_y){
    if(amount <= 0) return;
    int a = amount / 2; // half-dollars to dollars
    unsigned now = SDL_GetTicks();
    unsigned base = anim_done_time > now ? anim_done_time : now;
    for(int d = 0; d < NUM_DENOMS; d++){
        int count = a / denom_values[d];
        a %= denom_values[d];
        for(int i = 0; i < count && num_chip_anims < MAX_CHIP_ANIMS; i++){
            chip_anims[num_chip_anims++] = (ChipAnim){from_x, from_y, to_x, to_y, denom_colors[d], base};
            base += CHIP_STAGGER;
        }
    }
    if(num_chip_anims > 0)
        chip_anim_done = base - CHIP_STAGGER + CHIP_ANIM_MS;
}

void
queue_settle_chips(void){
    num_chip_anims = 0;
    chip_anim_done = 0;
    int house_y = DEALER_CARDS_Y + CARD_H / 2;
    int bet_y = PLAYER_CARDS_Y + CARD_H + CARDS_VAL_GAP + VAL_BET_GAP;
    for(int i = 0; i < num_hands; i++){
        int hx = hand_cx(i);
        int payout = hand_payouts[i];
        if(payout > 0)
            queue_chip_flow(payout, HOUSE_PILE_X, house_y, hx, bet_y);
        else if(payout < 0)
            queue_chip_flow(-payout, hx, bet_y, HOUSE_PILE_X, house_y);
    }
}

void
draw_suit(int suit, int x, int y, int sz){
    SDL_Rect src = {suit * SUIT_W, 0, SUIT_W, SUIT_H};
    SDL_Rect dst = {x, y, SUIT_W * sz, SUIT_H * sz};
    unsigned char r, g, b, a;
    SDL_GetRenderDrawColor(ren, &r, &g, &b, &a);
    SDL_SetTextureColorMod(suit_tex, r, g, b);
    SDL_SetTextureAlphaMod(suit_tex, a);
    SDL_RenderCopy(ren, suit_tex, &src, &dst);
}

// Draw a single character using the font texture. sz = scale factor.
void
draw_char(unsigned char ch, int x, int y, int sz){
    if(ch < FONT_FIRST || ch >= FONT_FIRST + FONT_COUNT) return;
    int idx = ch - FONT_FIRST;
    SDL_Rect src = {idx * FONT_W, 0, FONT_W, FONT_H};
    SDL_Rect dst = {x, y, FONT_W * sz, FONT_H * sz};
    // Use current draw color as color mod
    unsigned char r, g, b, a;
    SDL_GetRenderDrawColor(ren, &r, &g, &b, &a);
    SDL_SetTextureColorMod(font_tex, r, g, b);
    SDL_SetTextureAlphaMod(font_tex, a);
    SDL_RenderCopy(ren, font_tex, &src, &dst);
}

void
draw_text(const char* text, int cx, int cy, int sz){
    int len = (int)SDL_strlen(text);
    int charw = FONT_W * sz;
    int charh = FONT_H * sz;
    int totalw = len * charw;
    int x = cx - totalw / 2;
    int y = cy - charh / 2;
    for(int i = 0; i < len; i++)
        draw_char(text[i], x + i * charw, y, sz);
}

void
draw_text_left(const char* text, int lx, int cy, int sz){
    int charw = FONT_W * sz;
    int charh = FONT_H * sz;
    int y = cy - charh / 2;
    for(int i = 0; text[i]; i++)
        draw_char(text[i], lx + i * charw, y, sz);
}

void
draw_number(int num, int cx, int cy, int sz){
    char buf[16];
    SDL_snprintf(buf, sizeof buf, "%d", num);
    draw_text(buf, cx, cy, sz);
}

const char* rank_labels[13] = { "A","2","3","4","5","6","7","8","9","10","J","Q","K" };

void
draw_card(int card, int x, int y, _Bool facedown){
    SDL_SetRenderDrawColor(ren, 0x20, 0x20, 0x20, 0x40);
    SDL_Rect shadow = {x + 2, y + 2, CARD_W, CARD_H};
    SDL_RenderFillRect(ren, &shadow);
    if(facedown){
        SDL_SetRenderDrawColor(ren, 0x22, 0x44, 0xaa, 0xff);
        SDL_Rect bg = {x, y, CARD_W, CARD_H};
        SDL_RenderFillRect(ren, &bg);
        SDL_SetRenderDrawColor(ren, 0x33, 0x55, 0xcc, 0xff);
        for(int i = 4; i < CARD_W - 4; i += 6)
            for(int j = 4; j < CARD_H - 4; j += 6){
                SDL_Rect dot = {x + i, y + j, 3, 3};
                SDL_RenderFillRect(ren, &dot);
            }
        SDL_SetRenderDrawColor(ren, 0x11, 0x22, 0x66, 0xff);
        SDL_Rect b = {x, y, CARD_W, 2}; SDL_RenderFillRect(ren, &b);
        b = (SDL_Rect){x, y + CARD_H - 2, CARD_W, 2}; SDL_RenderFillRect(ren, &b);
        b = (SDL_Rect){x, y, 2, CARD_H}; SDL_RenderFillRect(ren, &b);
        b = (SDL_Rect){x + CARD_W - 2, y, 2, CARD_H}; SDL_RenderFillRect(ren, &b);
        return;
    }
    int suit = card / 13;
    int rank = card % 13;
    _Bool red = (suit == 1 || suit == 2);
    SDL_SetRenderDrawColor(ren, 0xff, 0xff, 0xff, 0xff);
    SDL_Rect bg = {x, y, CARD_W, CARD_H};
    SDL_RenderFillRect(ren, &bg);
    SDL_SetRenderDrawColor(ren, 0xaa, 0xaa, 0xaa, 0xff);
    SDL_Rect b = {x, y, CARD_W, 1}; SDL_RenderFillRect(ren, &b);
    b = (SDL_Rect){x, y + CARD_H - 1, CARD_W, 1}; SDL_RenderFillRect(ren, &b);
    b = (SDL_Rect){x, y, 1, CARD_H}; SDL_RenderFillRect(ren, &b);
    b = (SDL_Rect){x + CARD_W - 1, y, 1, CARD_H}; SDL_RenderFillRect(ren, &b);
    if(red) SDL_SetRenderDrawColor(ren, 0xcc, 0x00, 0x00, 0xff);
    else    SDL_SetRenderDrawColor(ren, 0x11, 0x11, 0x11, 0xff);
    const char* label = rank_labels[rank];
    int cw = FONT_W * CARD_RANK_SZ, ch = FONT_H * CARD_RANK_SZ;
    for(int i = 0; label[i]; i++)
        draw_char(label[i], x + CARD_RANK_PAD + i * cw, y + CARD_RANK_PAD, CARD_RANK_SZ);
    draw_suit(suit, x + CARD_W / 2 - (SUIT_W * CARD_SUIT_SZ) / 2, y + CARD_H / 2 - (SUIT_H * CARD_SUIT_SZ) / 2, CARD_SUIT_SZ);
    int llen = (int)SDL_strlen(label);
    for(int i = 0; label[i]; i++)
        draw_char(label[i], x + CARD_W - CARD_RANK_PAD - cw - (llen - 1 - i) * cw, y + CARD_H - CARD_RANK_PAD - ch, CARD_RANK_SZ);
}

void
draw_hand_cards(int* cards, unsigned* deal_times, int count, int cx, int y, _Bool hide_second){
    unsigned now = SDL_GetTicks();
    int total_w = CARD_W + (count > 1 ? (count - 1) * CARD_OVERLAP : 0);
    int base_x = cx - total_w / 2;
    for(int i = 0; i < count; i++){
        int dst_x = base_x + i * CARD_OVERLAP;
        unsigned dt = deal_times[i];
        if(dt != 0 && now < dt)
            continue; // hasn't started moving yet
        if(dt != 0 && now < dt + ANIM_MS){
            float p = (float)(now - dt) / (float)ANIM_MS;
            p = 1.0f - (1.0f - p) * (1.0f - p); // ease-out
            int ax = SHOE_PILE_X + (int)((dst_x - SHOE_PILE_X) * p);
            int ay = DEALER_CARDS_Y + (int)((y - DEALER_CARDS_Y) * p);
            draw_card(cards[i], ax, ay, 1); // facedown in transit
        }
        else {
            draw_card(cards[i], dst_x, y, hide_second && i == 1);
        }
    }
}

int check_y(int i){ return CB_START_Y + i * (CB_SIZE + CB_PAD + CB_ROW_EXTRA); }

int
check_hit(int mx, int my){
    if(mx < SIDE_X || mx >= SIDE_X + SIDE_INNER_W) return -1;
    for(int i = 0; i < NUM_CHECKS; i++){
        int y = check_y(i);
        if(my >= y && my < y + CB_SIZE + CB_PAD) return i;
    }
    return -1;
}

void
draw_checkbox(int i){
    int x = SIDE_X;
    int y = check_y(i);
    // Box
    SDL_SetRenderDrawColor(ren, 0xcc, 0xcc, 0xcc, 0xff);
    SDL_Rect box = {x, y, CB_SIZE, CB_SIZE};
    SDL_RenderFillRect(ren, &box);
    if(*checks[i].flag){
        SDL_SetRenderDrawColor(ren, 0x44, 0xcc, 0x44, 0xff);
        SDL_Rect inner = {x + CB_CHECK_INSET, y + CB_CHECK_INSET, CB_SIZE - 2*CB_CHECK_INSET, CB_SIZE - 2*CB_CHECK_INSET};
        SDL_RenderFillRect(ren, &inner);
    }
    // Label
    SDL_SetRenderDrawColor(ren, 0xcc, 0xcc, 0xcc, 0xff);
    draw_text_left(checks[i].label, x + CB_SIZE + CB_LABEL_GAP, y + CB_SIZE / 2, 1);
}


void
layout_buttons(void){
    SDL_memset(buttons, 0, sizeof buttons);
    int btn_y = HEIGHT - BTN_BOT;
    if(phase == PHASE_PLAY){
        int bw = PLAY_BTN_W;
        int n = 5;
        int total = bw * n + BTN_PAD * (n - 1);
        int sx = (GAME_W - total) / 2;

        buttons[B_DOUBLE] = (Button){"DOUBLE", sx, btn_y, bw, can_double(), 1};
        sx += bw + BTN_PAD;
        buttons[B_HIT] = (Button){"HIT", sx, btn_y, bw, 1, 1};
        sx += bw + BTN_PAD;
        buttons[B_STAND] = (Button){"STAND", sx, btn_y, bw, 1, 1};
        sx += bw + BTN_PAD;
        buttons[B_SPLIT] = (Button){"SPLIT", sx, btn_y, bw, can_split(), 1};
        sx += bw + BTN_PAD;
        buttons[B_SURRENDER] = (Button){"SURRENDER", sx, btn_y, bw, can_surrender(), 1};
    }
    else if(phase == PHASE_INSURANCE || phase == PHASE_EVEN_MONEY){
        int bw = DEAL_BTN_W;
        int total = bw * 2 + BTN_PAD;
        int sx = (GAME_W - total) / 2;
        const char* yes_label = phase == PHASE_EVEN_MONEY ? "EVEN MONEY" : "INSURE";
        buttons[B_INSURE] = (Button){yes_label, sx, btn_y, bw, 1, 1};
        sx += bw + BTN_PAD;
        buttons[B_DECLINE] = (Button){"DECLINE", sx, btn_y, bw, 1, 1};
    }
    else {
        int bbw = 50;
        int dbw = DEAL_BTN_W;
        int n_bet = 4;
        int total = n_bet * bbw + BTN_PAD * n_bet + dbw;
        int sx = (GAME_W - total) / 2;
        buttons[B_BET_DN]   = (Button){"-5",  sx, btn_y, bbw, next_bet > MIN_BET, 1};
        sx += bbw + BTN_PAD;
        buttons[B_BET_HALF] = (Button){"1/2", sx, btn_y, bbw, next_bet > MIN_BET, 1};
        sx += bbw + BTN_PAD;
        buttons[B_DEAL] = (Button){"DEAL", sx, btn_y, dbw, bankroll > 0 && next_bet <= bankroll, 1};
        sx += dbw + BTN_PAD;
        buttons[B_BET_DBL]  = (Button){"2X",  sx, btn_y, bbw, 1, 1};
        sx += bbw + BTN_PAD;
        buttons[B_BET_UP]   = (Button){"+5",  sx, btn_y, bbw, 1, 1};
    }
    // Deck selector: always visible in sidebar
    buttons[B_DECKS] = (Button){NULL, SIDE_X, DECK_LABEL_Y - 8, SIDE_INNER_W, 1, 1};
}

int
button_hit(int mx, int my){
    for(int i = 0; i < B_COUNT; i++){
        if(!buttons[i].visible || !buttons[i].active) continue;
        if(mx >= buttons[i].x && mx < buttons[i].x + buttons[i].w &&
           my >= buttons[i].y && my < buttons[i].y + BTN_H)
            return i;
    }
    return -1;
}

void
draw_buttons(void){
    for(int i = 0; i < B_COUNT; i++){
        if(!buttons[i].visible) continue;
        Button* b = &buttons[i];
        if(!b->label || !b->label[0]) continue; // invisible hit region
        if(b->active)
            SDL_SetRenderDrawColor(ren, 0xcc, 0xaa, 0x44, 0xff);
        else
            SDL_SetRenderDrawColor(ren, 0x33, 0x44, 0x33, 0xff);
        SDL_Rect br = {b->x, b->y, b->w, BTN_H};
        SDL_RenderFillRect(ren, &br);
        if(b->active)
            SDL_SetRenderDrawColor(ren, 0xff, 0xff, 0xff, 0xff);
        else
            SDL_SetRenderDrawColor(ren, 0x55, 0x66, 0x55, 0xff);
        draw_text(b->label, b->x + b->w / 2, b->y + BTN_H / 2, 1);
    }
}

void
draw(void){
    // Clear letterbox area
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 0xff);
    SDL_RenderClear(ren);
    // Game area background
    SDL_SetRenderDrawColor(ren, 0x1a, 0x6b, 0x3c, 0xff);
    SDL_Rect game_bg = {0, 0, GAME_W, HEIGHT};
    SDL_RenderFillRect(ren, &game_bg);
    // Sidebar background
    SDL_SetRenderDrawColor(ren, 0x14, 0x3d, 0x26, 0xff);
    SDL_Rect side_bg = {GAME_W, 0, SIDE_W, HEIGHT};
    SDL_RenderFillRect(ren, &side_bg);
    // --- Sidebar ---
    char buf[32];
    // Next bet preview (above action buttons when in PHASE_DONE)
    if(phase == PHASE_DONE){
        _Bool all_lost = 1;
        for(int i = 0; i < num_hands; i++)
            if(hand_results[i] != R_LOSE && hand_results[i] != R_SURRENDER){ all_lost = 0; break; }
        if(num_hands == 0) all_lost = 1;
        if((bet_modified || all_lost) && SDL_GetTicks() >= chip_anim_done){
            int bet_y = HEIGHT - BTN_BOT - BTN_H - 12;
            SDL_SetTextureAlphaMod(chip_tex, 0x88);
            draw_chip_stack(GAME_W / 2, bet_y - FONT_H - 4, next_bet);
            SDL_SetTextureAlphaMod(chip_tex, 0xff);
            SDL_SetRenderDrawColor(ren, 0xff, 0xdd, 0x44, 0x88);
            format_money(buf, sizeof buf, next_bet);
            draw_text(buf, GAME_W / 2, bet_y, 2);
        }
    }
    // Rules label
    SDL_SetRenderDrawColor(ren, 0x99, 0x99, 0x99, 0xff);
    draw_text_left("RULES", SIDE_X, CB_START_Y - RULES_LABEL_GAP, 1);
    // Checkboxes
    for(int i = 0; i < NUM_CHECKS; i++)
        draw_checkbox(i);
    // Volume
    SDL_SetRenderDrawColor(ren, 0x99, 0x99, 0x99, 0xff);
    draw_text_left(muted ? "VOLUME (MUTED)" : "VOLUME", SIDE_X, VOL_LABEL_Y, 1);
    // Bar background
    SDL_SetRenderDrawColor(ren, 0x0a, 0x2a, 0x1a, 0xff);
    SDL_Rect vol_bg = {SIDE_X, VOL_BAR_Y, SIDE_INNER_W, VOL_BAR_H};
    SDL_RenderFillRect(ren, &vol_bg);
    // Bar fill
    int vol_w = SIDE_INNER_W * volume / 128;
    SDL_SetRenderDrawColor(ren, muted ? 0x66 : 0x44, muted ? 0x66 : 0xaa, muted ? 0x66 : 0x66, 0xff);
    SDL_Rect vol_fill = {SIDE_X, VOL_BAR_Y, vol_w, VOL_BAR_H};
    SDL_RenderFillRect(ren, &vol_fill);
    // Mute checkbox
    SDL_SetRenderDrawColor(ren, 0xcc, 0xcc, 0xcc, 0xff);
    SDL_Rect mute_box = {SIDE_X, MUTE_Y, CB_SIZE, CB_SIZE};
    SDL_RenderFillRect(ren, &mute_box);
    if(muted){
        SDL_SetRenderDrawColor(ren, 0xcc, 0x44, 0x44, 0xff);
        SDL_Rect mute_inner = {SIDE_X + CB_CHECK_INSET, MUTE_Y + CB_CHECK_INSET, CB_SIZE - 2*CB_CHECK_INSET, CB_SIZE - 2*CB_CHECK_INSET};
        SDL_RenderFillRect(ren, &mute_inner);
    }
    SDL_SetRenderDrawColor(ren, 0xcc, 0xcc, 0xcc, 0xff);
    draw_text_left("MUTE", SIDE_X + CB_SIZE + CB_LABEL_GAP, MUTE_Y + CB_SIZE / 2, 1);
    // Deck selector (clickable)
    SDL_SetRenderDrawColor(ren, 0xcc, 0xcc, 0xcc, 0xff);
    if(num_decks == 0)
        draw_text_left("DECKS: INF", SIDE_X, DECK_LABEL_Y, 1);
    else {
        SDL_snprintf(buf, sizeof buf, "DECKS: %d", num_decks);
        draw_text_left(buf, SIDE_X, DECK_LABEL_Y, 1);
    }
    // Shoe
    int remaining;
    int total_cards;
    if(num_decks == 0){
        remaining = 52;
        total_cards = 52;
    }
    else {
        remaining = deck_size - deck_pos;
        total_cards = deck_size;
    }
    SDL_SetRenderDrawColor(ren, 0x99, 0x99, 0x99, 0xff);
    if(num_decks == 0)
        draw_text_left("SHOE INF", SIDE_X, SHOE_Y, 1);
    else {
        SDL_snprintf(buf, sizeof buf, "SHOE %d/%d", remaining, total_cards);
        draw_text_left(buf, SIDE_X, SHOE_Y, 1);
    }
    // Bar background
    int bar_y = SHOE_Y + SHOE_BAR_DY;
    SDL_SetRenderDrawColor(ren, 0x0a, 0x2a, 0x1a, 0xff);
    SDL_Rect shoe_bg = {SIDE_X, bar_y, SIDE_INNER_W, SHOE_BAR_H};
    SDL_RenderFillRect(ren, &shoe_bg);
    // Bar fill
    int fill_w = (num_decks == 0) ? SIDE_INNER_W : SIDE_INNER_W * remaining / total_cards;
    SDL_SetRenderDrawColor(ren, 0x44, 0xaa, 0x66, 0xff);
    SDL_Rect shoe_fill = {SIDE_X, bar_y, fill_w, SHOE_BAR_H};
    SDL_RenderFillRect(ren, &shoe_fill);
    // Cut card marker at 50%
    if(num_decks != 0){
        SDL_SetRenderDrawColor(ren, 0xff, 0x44, 0x44, 0xff);
        SDL_Rect shoe_cut = {SIDE_X + SIDE_INNER_W / 2, bar_y - 1, 2, SHOE_BAR_H + 2};
        SDL_RenderFillRect(ren, &shoe_cut);
    }
    // --- Game area ---
    // Dealer
    SDL_SetRenderDrawColor(ren, 0xcc, 0xcc, 0xcc, 0xff);
    draw_text("DEALER", GAME_W / 2, DEALER_LABEL_Y, 2);
    unsigned now = SDL_GetTicks();
    // Trigger card deal sounds
    {
        int visible = 0;
        for(int i = 0; i < dealer_count; i++)
            if(dealer_deal_time[i] != 0 && now >= dealer_deal_time[i]) visible++;
        for(int i = 0; i < num_hands; i++)
            for(int j = 0; j < hands[i].count; j++)
                if(hands[i].deal_time[j] != 0 && now >= hands[i].deal_time[j]) visible++;
        while(deal_sounds_played < visible){
            play_sound(snd_card_slide[deal_sounds_played % NUM_CARD_SNDS]);
            deal_sounds_played++;
        }
    }
    // Trigger chip sounds
    {
        int arrived = 0;
        for(int i = 0; i < num_chip_anims; i++)
            if(now >= chip_anims[i].start_time) arrived++;
        while(chip_sounds_played < arrived){
            play_sound(snd_chip_lay[chip_sounds_played % NUM_CHIP_SNDS]);
            chip_sounds_played++;
        }
    }
    draw_hand_cards(dealer_cards, dealer_deal_time, dealer_count, GAME_W / 2, DEALER_CARDS_Y, !dealer_reveal);
    if(dealer_reveal && dealer_count > 0 && now >= anim_done_time){
        int dv = hand_value(dealer_cards, dealer_count);
        SDL_SetRenderDrawColor(ren, 0xff, 0xff, 0xaa, 0xff);
        draw_number(dv, GAME_W / 2, DEALER_CARDS_Y + CARD_H + CARDS_VAL_GAP, 2);
    }
    // Shoe pile
    {
        int layers = (num_decks == 0) ? SHOE_MAX_LAYERS : remaining * SHOE_MAX_LAYERS / total_cards;
        // Stack edges behind the top card
        for(int i = layers; i > 0; i--){
            int sx = SHOE_PILE_X + i;
            int sy = DEALER_CARDS_Y + i;
            SDL_SetRenderDrawColor(ren, 0x1a, 0x3a, 0x90, 0xff);
            SDL_Rect edge = {sx, sy, CARD_W, CARD_H};
            SDL_RenderFillRect(ren, &edge);
            SDL_SetRenderDrawColor(ren, 0x11, 0x22, 0x66, 0xff);
            SDL_Rect b = {sx, sy, CARD_W, 1};
            SDL_RenderFillRect(ren, &b);
            b = (SDL_Rect){sx, sy + CARD_H - 1, CARD_W, 1};
            SDL_RenderFillRect(ren, &b);
            b = (SDL_Rect){sx, sy, 1, CARD_H};
            SDL_RenderFillRect(ren, &b);
            b = (SDL_Rect){sx + CARD_W - 1, sy, 1, CARD_H};
            SDL_RenderFillRect(ren, &b);
        }
        // Top card
        if(remaining > 0)
            draw_card(0, SHOE_PILE_X, DEALER_CARDS_Y, 1);
    }
    // House chip rack
    {
        int ry = DEALER_CARDS_Y + CARD_H - CHIP_H/2;
        int rx = HOUSE_PILE_X;
        int spacing = CHIP_W - 12;
        // Five overlapping columns, 4 high
        for(int j = 3; j >= 0; j--) draw_chip(rx,             ry - j * CHIP_STACK_DY, 4);
        for(int j = 3; j >= 0; j--) draw_chip(rx + spacing,   ry - j * CHIP_STACK_DY, 2);
        for(int j = 3; j >= 0; j--) draw_chip(rx + spacing*2, ry - j * CHIP_STACK_DY, 1);
        for(int j = 3; j >= 0; j--) draw_chip(rx + spacing*3, ry - j * CHIP_STACK_DY, 0);
        for(int j = 3; j >= 0; j--) draw_chip(rx + spacing*4, ry - j * CHIP_STACK_DY, 3);
    }
    // Player hands
    for(int i = 0; i < num_hands; i++){
        Hand* h = &hands[i];
        int hcx = hand_cx(i);
        // Active indicator
        if(phase == PHASE_PLAY && i == active_hand){
            SDL_SetRenderDrawColor(ren, 0xff, 0xdd, 0x44, 0x30);
            int iw = GAME_W / (num_hands > 1 ? num_hands : 1);
            SDL_Rect ind = {hcx - iw / 2, PLAYER_CARDS_Y - ACTIVE_IND_PAD, iw, CARD_H + ACTIVE_IND_EXTRA_H};
            SDL_RenderFillRect(ren, &ind);
        }
        draw_hand_cards(h->cards, h->deal_time, h->count, hcx, PLAYER_CARDS_Y, 0);
        // Check if all cards in this hand have arrived
        _Bool hand_arrived = 1;
        for(int j = 0; j < h->count; j++)
            if(h->deal_time[j] != 0 && now < h->deal_time[j] + ANIM_MS){ hand_arrived = 0; break; }
        int val_y = PLAYER_CARDS_Y + CARD_H + CARDS_VAL_GAP;
        if(hand_arrived){
            if(h->surrendered){
                SDL_SetRenderDrawColor(ren, 0x99, 0x99, 0x99, 0xff);
                draw_text("FOLD", hcx, val_y, 1);
            }
            else {
                int pv = hand_value(h->cards, h->count);
                SDL_SetRenderDrawColor(ren, 0xff, 0xff, 0xaa, 0xff);
                if(pv <= 21 && hand_soft(h->cards, h->count) && pv != 21){
                    SDL_snprintf(buf, sizeof buf, "%d/%d", pv - 10, pv);
                    draw_text(buf, hcx, val_y, 2);
                }
                else
                    draw_number(pv, hcx, val_y, 2);
            }
        }
        // Bet chip stack: hide for lost hands once chip animation starts moving, hide all if bet changed
        if(!bet_modified && (hand_results[i] != R_LOSE && hand_results[i] != R_SURRENDER
           || (num_chip_anims > 0 && now < chip_anims[0].start_time))){
            int chip_y = val_y + VAL_BET_GAP + CHIP_H/2;
            draw_chip_stack(hcx, chip_y, h->bet);
            SDL_SetRenderDrawColor(ren, 0xff, 0xdd, 0x44, 0xcc);
            format_money(buf, sizeof buf, h->bet);
            draw_text(buf, hcx, val_y + VAL_BET_GAP - 6, 1);
        }
        // Result (wait for both card and chip animations, hide if bet changed)
        if(phase == PHASE_DONE && !bet_modified && hand_results[i] != R_NONE && now >= anim_done_time && now >= chip_anim_done){
            const char* msg = "";
            int mr = 0, mg = 0, mb = 0;
            switch(hand_results[i]){
                case R_WIN:       msg = "WIN";       mr=0xff; mg=0xdd; mb=0x44; break;
                case R_BJ:        msg = "BLACKJACK"; mr=0xff; mg=0xdd; mb=0x44; break;
                case R_CHARLIE:   msg = "CHARLIE";   mr=0xff; mg=0xdd; mb=0x44; break;
                case R_LOSE:      msg = "LOSE";      mr=0xff; mg=0x44; mb=0x44; break;
                case R_PUSH:      msg = "PUSH";      mr=0xcc; mg=0xcc; mb=0xcc; break;
                case R_SURRENDER: msg = "SURRENDER"; mr=0x99; mg=0x99; mb=0x99; break;
                default: break;
            }
            int ry = val_y + VAL_RESULT_GAP;
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 0xff);
            draw_text(msg, hcx + 1, ry + 1, 2);
            SDL_SetRenderDrawColor(ren, (unsigned char)mr, (unsigned char)mg, (unsigned char)mb, 0xff);
            draw_text(msg, hcx, ry, 2);

            if(hand_payouts[i] != 0){
                char pay[16];
                if(hand_payouts[i] > 0){
                    pay[0] = '+';
                    format_money(pay+1, sizeof pay -1, hand_payouts[i]);
                }
                else
                    format_money(pay, sizeof pay, hand_payouts[i]);
                draw_text(pay, hcx, ry + RESULT_PAY_GAP, 1);
            }
        }
    }
    // In-flight chip animations
    for(int i = 0; i < num_chip_anims; i++){
        ChipAnim* ca = &chip_anims[i];
        if(now < ca->start_time) continue;
        unsigned elapsed = now - ca->start_time;
        if(elapsed >= (unsigned)CHIP_ANIM_MS) continue; // done, now part of stack
        float p = (float)elapsed / (float)CHIP_ANIM_MS;
        p = 1.0f - (1.0f - p) * (1.0f - p); // ease-out
        int cx = ca->src_x + (int)((ca->dst_x - ca->src_x) * p);
        int cy = ca->src_y + (int)((ca->dst_y - ca->src_y) * p);
        draw_chip(cx, cy, ca->color);
    }
    // Insurance prompt
    if(phase == PHASE_EVEN_MONEY){
        SDL_SetRenderDrawColor(ren, 0xff, 0xdd, 0x44, 0xff);
        draw_text("EVEN MONEY?", GAME_W / 2, HEIGHT / 2, 3);
        SDL_SetRenderDrawColor(ren, 0xcc, 0xcc, 0xcc, 0xff);
        draw_text("TAKE 1:1 PAYOUT", GAME_W / 2, HEIGHT / 2 + 32, 1);
    }
    else if(phase == PHASE_INSURANCE){
        SDL_SetRenderDrawColor(ren, 0xff, 0xdd, 0x44, 0xff);
        draw_text("INSURANCE?", GAME_W / 2, HEIGHT / 2, 3);
        SDL_SetRenderDrawColor(ren, 0xcc, 0xcc, 0xcc, 0xff);
        SDL_memcpy(buf, "COST: ", 6);
        format_money(buf + 6, sizeof buf - 6, hands[0].bet / 2);
        draw_text(buf, GAME_W / 2, HEIGHT / 2 + 32, 1);
    }
    // Busted / waiting message
    if(phase == PHASE_DONE && bankroll <= 0 && now >= chip_anim_done){
        SDL_SetRenderDrawColor(ren, 0x00, 0x00, 0x00, 0xcc);
        SDL_Rect overlay = {0, 0, GAME_W, HEIGHT};
        SDL_RenderFillRect(ren, &overlay);
        SDL_SetRenderDrawColor(ren, 0xff, 0x44, 0x44, 0xff);
        draw_text("BUSTED", GAME_W / 2, HEIGHT / 2 - 30, 3);
        SDL_SetRenderDrawColor(ren, 0xcc, 0xcc, 0xcc, 0xff);
        const char* taunts[] = {
            "MAYBE TRY SLOTS INSTEAD",
            "YOUR STRATEGY NEEDS WORK",
            "BASIC STRATEGY EXISTS, YOU KNOW",
            "YOU SHOULD HAVE WALKED AWAY",
            "THAT WAS EMBARRASSING",
            "THE ATM IS BY THE RESTROOMS",
            "HAVE YOU CONSIDERED POKER?",
            "COUNTING CARDS ISN'T YOUR THING",
        };
        draw_text(taunts[rounds_played % 8], GAME_W / 2, HEIGHT / 2 + 10, 1);
        SDL_SetRenderDrawColor(ren, 0xaa, 0xaa, 0xaa, 0xff);
        draw_text("YOU LOSE", GAME_W / 2, HEIGHT / 2 + 40, 1);
    }
    else if(phase == PHASE_DONE && num_hands == 0){
        SDL_SetRenderDrawColor(ren, 0xff, 0xff, 0xff, 0xff);
        draw_text("PRESS DEAL", GAME_W / 2, HEIGHT / 2, 3);
    }
    // Player bankroll chips (bottom left, fixed columns per denomination)
    {
        int dollars = bankroll / 2;
        int bx = 20;
        int by = HEIGHT - BTN_BOT - BTN_H - 10 - CHIP_H/2;
        int spacing = CHIP_W - 12;
        int rem = dollars;
        for(int d = 0; d < NUM_DENOMS; d++){
            int count = rem / denom_values[d];
            rem %= denom_values[d];
            int n = count > 10 ? 10 : count;
            for(int j = n - 1; j >= 0; j--)
                draw_chip(bx + d * spacing, by - j * CHIP_STACK_DY, denom_colors[d]);
        }
        // Bankroll amount below chips
        SDL_SetRenderDrawColor(ren, 0xff, 0xff, 0xff, 0xff);
        format_money(buf, sizeof buf, bankroll);
        draw_text_left(buf, bx, by + CHIP_H/2 + 4, 2);
    }
    // W/L stats (bottom right of game area)
    {
        int stats_x = GAME_W - 10;
        int stats_y = HEIGHT - STATS_BOT;
        SDL_SetRenderDrawColor(ren, 0x88, 0x88, 0x88, 0xff);
        buf[0] = 'W'; buf[1] = ' ';
        format_money(buf + 2, sizeof buf - 2, total_won);
        draw_text(buf, stats_x - (int)SDL_strlen(buf) * FONT_W / 2, stats_y, 1);
        buf[0] = 'L'; buf[1] = ' ';
        format_money(buf + 2, sizeof buf - 2, total_lost);
        draw_text(buf, stats_x - (int)SDL_strlen(buf) * FONT_W / 2, stats_y + STATS_LINE_H, 1);
    }
    // Buttons
    layout_buttons();
    draw_buttons();
}

void
init_textures(void){
    init_font();
    init_suits();
    init_chips();
}

#ifdef __DIR__
#define SND_DIR __DIR__ "/BjAudio/"
#else
#define SND_DIR "Samples/BjAudio/"
#endif


void
play_sound(Mix_Chunk* snd){
    #ifndef NO_SDL_MIXER
    if(snd) Mix_PlayChannel(-1, snd, 0);
    #endif
}

void
apply_volume(void){
    #ifndef NO_SDL_MIXER
    int v = muted ? 0 : volume;
    for(int i = 0; i < NUM_CARD_SNDS; i++)
        if(snd_card_slide[i]) Mix_VolumeChunk(snd_card_slide[i], v);
    for(int i = 0; i < NUM_CHIP_SNDS; i++)
        if(snd_chip_lay[i]) Mix_VolumeChunk(snd_chip_lay[i], v);
    #endif
}

void
init_sounds(void){
    #ifndef NO_SDL_MIXER
    char path[128];
    for(int i = 0; i < NUM_CARD_SNDS; i++){
        SDL_snprintf(path, sizeof path, SND_DIR "card-slide-%d.ogg", i + 1);
        snd_card_slide[i] = Mix_LoadWAV(path);
    }
    for(int i = 0; i < NUM_CHIP_SNDS; i++){
        SDL_snprintf(path, sizeof path, SND_DIR "chip-lay-%d.ogg", i + 1);
        snd_chip_lay[i] = Mix_LoadWAV(path);
    }
    apply_volume();
    #endif
}

void
cleanup_sounds(void){
    #ifndef NO_SDL_MIXER
    for(int i = 0; i < NUM_CARD_SNDS; i++)
        Mix_FreeChunk(snd_card_slide[i]);
    for(int i = 0; i < NUM_CHIP_SNDS; i++)
        Mix_FreeChunk(snd_chip_lay[i]);
    #endif
}

unsigned
rng(void){
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
_Bool
click_volume(int mx, int my){
    if(mx >= SIDE_X && mx < SIDE_X + SIDE_INNER_W && my >= VOL_BAR_Y && my < VOL_BAR_Y + VOL_BAR_H){
        volume = (mx - SIDE_X) * 128 / SIDE_INNER_W;
        if(volume < 0) volume = 0;
        if(volume > 128) volume = 128;
        apply_volume();
        return 1;
    }
    // Mute checkbox
    if(mx >= SIDE_X && mx < SIDE_X + SIDE_INNER_W
    && my >= MUTE_Y && my < MUTE_Y + CB_SIZE + CB_PAD){
        muted = !muted;
        apply_volume();
        return 1;
    }
    return 0;
}
