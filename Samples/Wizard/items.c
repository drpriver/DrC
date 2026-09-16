static const SpriteRect healing_art[] = {
    {16, 8, 8, 6, 0xe6d8b7},
    {12, 14, 16, 17, 0xff8195},
    {14, 15, 4, 7, 0xffe6e5},
    {14, 31, 12, 3, 0x31384c},
};

static const SpriteRect moonwater_art[] = {
    {16, 8, 8, 6, 0xe6d8b7},
    {12, 14, 16, 17, 0x65d8ef},
    {14, 15, 4, 7, 0xffe6e5},
    {14, 31, 12, 3, 0x31384c},
};

static const SpriteRect ember_relic_art[] = {
    {17, 7, 6, 24, 0xffd280},
    {10, 14, 20, 9, 0xffd280},
    {17, 14, 6, 8, 0xfff1cc},
};

ItemDesc item_definitions[] = {
    {
        .name = "Ember relic", .art = ember_relic_art,
        .collect = void(World* w){
            w.power++;
            say(w, "Ember relic! Lightning spells +1 damage for this run.");
        },
    },
    {
        .name = "Healing draught", .art = healing_art,
        .collect = void(World* w){
            int healing = 5;
            w.hp += healing;
            if(w.hp > MAX_HEALTH) w.hp = MAX_HEALTH;
            say(w, "Healing draught: restored %d HP.", healing);
        },
    },
    {
        .name = "Moonwater", .art = moonwater_art,
        .collect = void(World* w){
            w.mana = MAX_MANA;
            say(w, "Moonwater: mana fully restored.");
        },
    },
};
ItemDesc items[:] = item_definitions;
