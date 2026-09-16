#ifndef WIZARD_ASSETS_H
#define WIZARD_ASSETS_H

struct SpriteRect {
    int x, y, w, h;
    unsigned color;
};

static const SpriteRect wizard_art[] = {
    {8, 31, 25, 5, 0x171b2c},
    {12, 19, 18, 15, 0x7651d4},
    {16, 21, 9, 12, 0xb895f5},
    {15, 12, 12, 11, 0xf4cba0},
    {22, 15, 3, 3, 0x232439},
    {9, 10, 23, 5, 0x9871ee},
    {14, 5, 14, 7, 0x9871ee},
    {18, 1, 7, 6, 0xb8a0ff},
    {12, 31, 6, 5, 0x433553},
    {25, 31, 6, 5, 0x433553},
    {33, 13, 3, 22, 0xc99c69},
    {30, 8, 8, 8, 0x70e3ef},
    {32, 9, 3, 3, 0xffffff},
};


// Artwork uses 40-pixel tiles.
static const struct {
    struct { int w, h; } window;
    struct { int x, y, w, tile; } board;
    struct { int x, w, card_y, card_h, card_gap; } spellbook;
    struct { int health_x, mana_x, stat_y, bar_y, health_bar_w, bar_h, mana_pip_w, mana_pip_step; } stats;
    int line_height;
} layout = {
    .window = {
        .w = 1120,
        .h = 740,
    },
    .board = {
        .x = 24,
        .y = 128,
        .w = MAP_W * 40,
        .tile = 40,
    },
    .spellbook = {
        .x = 48 + MAP_W * 40,
        .w = 1120 - (48 + MAP_W * 40) - 24,
        .card_y = 150,
        .card_h = 66,
        .card_gap = 6,
    },
    .stats = {
        .health_x = 440,
        .mana_x = 616,
        .stat_y = 26,
        .bar_y = 51,
        .health_bar_w = 144,
        .bar_h = 10,
        .mana_pip_w = 17,
        .mana_pip_step = 23,
    },
    .line_height = 21,
};

static const struct {
    int damage_flash_ms;
    int wave_step_ms;
    int event_wait_ms;
} animation = {
    .damage_flash_ms = 550,
    .wave_step_ms = 280,
    .event_wait_ms = 50,
};

static const struct {
    unsigned background;
    unsigned level_title;
    unsigned health_text;
    unsigned health_empty;
    unsigned health_fill;
    unsigned mana_text;
    unsigned mana_fill;
    unsigned mana_empty;
    unsigned turn_text;
    unsigned muted_text;
    unsigned controls_background;
    unsigned controls_text;
    unsigned wall_background;
    unsigned floor_odd;
    unsigned floor_even;
    unsigned water_odd;
    unsigned water_even;
    unsigned wave_light;
    unsigned wave_dark;
    unsigned bridge_base;
    unsigned bridge_plank;
    unsigned bridge_highlight;
    unsigned bridge_rail;
    unsigned wall_face;
    unsigned wall_highlight;
    unsigned wall_mortar;
    unsigned grass_odd;
    unsigned grass_even;
    unsigned sand;
    unsigned grass_detail;
    unsigned floor_edge;
    unsigned floor_detail;
    unsigned portal_closed;
    unsigned portal_open;
    unsigned danger_fill;
    unsigned danger_border;
    unsigned hover_border;
    unsigned monster_health;
    unsigned target_border;
    unsigned damage_text;
    unsigned player_damage;
    unsigned dodge_title;
    unsigned dodge_help;
    unsigned message_background;
    unsigned message_text;
    unsigned spellbook_title;
    unsigned card_active_fill;
    unsigned card_fill;
    unsigned card_active_border;
    unsigned card_border;
    unsigned spell_unavailable;
    unsigned spell_name;
    unsigned spell_summary;
    unsigned spell_help;
    unsigned outcome_background;
    unsigned outcome_border;
    unsigned outcome_title;
    unsigned outcome_text;
    unsigned button_fill;
    unsigned button_border;
    unsigned button_text;
    unsigned charging_text;
} palette = {
    .background = 0x111827,
    .level_title = 0xf5e6cc,
    .health_text = 0xffb3bc,
    .health_empty = 0x3b293e,
    .health_fill = 0xe87990,
    .mana_text = 0x88d7ee,
    .mana_fill = 0x62c6e0,
    .mana_empty = 0x26394b,
    .turn_text = 0xf1ce83,
    .muted_text = 0x91a4bf,
    .controls_background = 0x243144,
    .controls_text = 0xc6d5e5,
    .wall_background = 0x151c2c,
    .floor_odd = 0x29384b,
    .floor_even = 0x2d3d50,
    .water_odd = 0x164968,
    .water_even = 0x194f70,
    .wave_light = 0x317995,
    .wave_dark = 0x286b88,
    .bridge_base = 0x715442,
    .bridge_plank = 0xc79b65,
    .bridge_highlight = 0xe1bb7b,
    .bridge_rail = 0x594736,
    .wall_face = 0x46536b,
    .wall_highlight = 0x62708a,
    .wall_mortar = 0x354158,
    .grass_odd = 0x537c69,
    .grass_even = 0x58846e,
    .sand = 0xd2c39a,
    .grass_detail = 0x87ab83,
    .floor_edge = 0x34465a,
    .floor_detail = 0x3c4b5e,
    .portal_closed = 0x4a6575,
    .portal_open = 0x79eed6,
    .danger_fill = 0x783a47,
    .danger_border = 0xff9f81,
    .hover_border = 0x738eab,
    .monster_health = 0xe68492,
    .target_border = 0xeacf89,
    .damage_text = 0xffe2a8,
    .player_damage = 0xff8099,
    .dodge_title = 0xffbf8c,
    .dodge_help = 0x95a9bf,
    .message_background = 0x1c2739,
    .message_text = 0xe3dfd7,
    .spellbook_title = 0xdecaf9,
    .card_active_fill = 0x3e365a,
    .card_fill = 0x202e43,
    .card_active_border = 0xb79cdd,
    .card_border = 0x35465e,
    .spell_unavailable = 0x738195,
    .spell_name = 0xe8ddf4,
    .spell_summary = 0x98acc4,
    .spell_help = 0xd3bce9,
    .outcome_background = 0x182233,
    .outcome_border = 0xcfb47d,
    .outcome_title = 0xf4d39d,
    .outcome_text = 0xc1cede,
    .button_fill = 0x303c53,
    .button_border = 0x4b6079,
    .button_text = 0xd7e2ed,
    .charging_text = 0xffbf79,
};

struct ButtonAsset {
    SDL_Rect bounds;
    const char* text;
    int key;
};

static const ButtonAsset buttons[] = {
    {{24, 668, 174, 40}, "Wait  [Space]", ' '},
    {{210, 668, 174, 40}, "Reload  [R]", 'r'},
    {{396, 668, 174, 40}, "New run  [N]", 'n'},
};

struct CaptionAsset {
    int x, y;
    unsigned color;
    int scale;
    const char* text;
};

static const CaptionAsset captions[] = {
    {34, 95, palette.controls_text, 1, "Move: arrows / WASD / click floor   Cast: click card or 1 - 6"},
    {24, 510, palette.dodge_title, 1, "DODGE THE ORANGE TILES"},
    {24, 533, palette.dodge_help, 1, "Marked attacks land after your next action. Push interrupts them."},
    {594, 680, palette.muted_text, 1, "Esc: clear target"},
    {layout.spellbook.x, 98, palette.spellbook_title, 2, "YOUR SPELLBOOK"},
};

#endif
