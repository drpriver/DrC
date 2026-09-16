static const SpriteRect goblin_art[] = {
    {7, 31, 27, 5, 0x171b2c},
    {9, 18, 23, 15, 0x47716b},
    {10, 7, 21, 17, 0x8ac98b},
    {5, 8, 6, 8, 0x8ac98b},
    {31, 8, 5, 8, 0x8ac98b},
    {10, 31, 7, 5, 0x384451},
    {25, 31, 7, 5, 0x384451},
    {15, 14, 4, 4, 0x202638},
    {24, 14, 4, 4, 0x202638},
};

static const SpriteRect caster_art[] = {
    {7, 31, 27, 5, 0x171b2c},
    {10, 10, 21, 24, 0x764182},
    {14, 5, 13, 10, 0xb15db5},
    {16, 15, 13, 10, 0xd888e0},
    {30, 16, 4, 18, 0xd7aac0},
    {15, 14, 4, 4, 0x202638},
    {24, 14, 4, 4, 0x202638},
};

static const SpriteRect brute_art[] = {
    {7, 31, 27, 5, 0x171b2c},
    {9, 18, 23, 15, 0xa66c4b},
    {10, 7, 21, 17, 0xefb46b},
    {5, 8, 6, 8, 0xefb46b},
    {31, 8, 5, 8, 0xefb46b},
    {10, 31, 7, 5, 0x384451},
    {25, 31, 7, 5, 0x384451},
    {3, 21, 9, 13, 0xa3b4c4},
    {5, 20, 5, 3, 0xe7ded0},
    {15, 14, 4, 4, 0x202638},
    {24, 14, 4, 4, 0x202638},
};

MonsterDesc monster_definitions[] = {
    {.name = "Goblin", .base_health = 3, .damage = 2, .attack_range = 1,
     .notice_range = 8, .move_turns = 1, .art = goblin_art},
    {.name = "Caster", .base_health = 3, .damage = 2, .attack_range = 5,
     .notice_range = 8, .move_turns = 1, .art = caster_art},
    {.name = "Brute", .base_health = 4, .damage = 3, .attack_range = 1,
     .notice_range = 8, .move_turns = 2, .art = brute_art},
};
MonsterDesc monsters[:] = monster_definitions;
