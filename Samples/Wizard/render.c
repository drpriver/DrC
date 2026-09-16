#pragma once
SDL_Renderer* canvas;
SDL_Texture* letters;

void
box(int x, int y, int w, int h, unsigned color){
    SDL_SetRenderDrawColor(canvas, (color >> 16) & 255, (color >> 8) & 255, color & 255, 255);
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(canvas, &r);
}

void
outline(int x, int y, int w, int h, unsigned color){
    box(x, y, w, 2, color);
    box(x, y + h - 2, w, 2, color);
    box(x, y, 2, h, color);
    box(x + w - 2, y, 2, h, color);
}

_Bool
make_letters(void){
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, FONT_COUNT * FONT_W, FONT_H, 32, SDL_PIXELFORMAT_RGBA32);
    if(!surface) return 0;
    SDL_FillRect(surface, NULL, SDL_MapRGBA(surface.format, 0, 0, 0, 0));
    unsigned white = SDL_MapRGBA(surface.format, 255, 255, 255, 255);
    for(int c = 0; c < FONT_COUNT; c++)
        for(int y = 0; y < FONT_H; y++)
            for(int x = 0; x < FONT_W; x++)
                if(font8x16[c][y] & (128 >> x)){
                    SDL_Rect pixel = {c * FONT_W + x, y, 1, 1};
                    SDL_FillRect(surface, &pixel, white);
                }
    letters = SDL_CreateTextureFromSurface(canvas, surface);
    SDL_FreeSurface(surface);
    if(letters) SDL_SetTextureBlendMode(letters, SDL_BLENDMODE_BLEND);
    return letters != NULL;
}
__attribute__((format(printf, 5, 6)))
void
label(int x, int y, unsigned color, int scale, const char* fmt, ...){
    static char buff[512];
    __builtin_va_list va;
    __builtin_va_start(va, fmt);
    SDL_vsnprintf(buff, sizeof buff, fmt, va);
    __builtin_va_end(va);
    SDL_SetTextureColorMod(letters, (color >> 16)&255, (color >> 8)&255, color&255);
    char* text = buff;
    for(; *text; text++, x += FONT_W * scale){
        unsigned c = (unsigned char)*text;
        if(c < FONT_FIRST || c >= FONT_FIRST + FONT_COUNT) c = '?';
        SDL_Rect src = {(int)(c - FONT_FIRST)*FONT_W, 0, FONT_W, FONT_H};
        SDL_Rect dst = {x, y, FONT_W * scale, FONT_H * scale};
        SDL_RenderCopy(canvas, letters, &src, &dst);
    }
}

void
wrapped(int x, int y, int columns, unsigned color, const char* text){
    if(!text) return;
    while(*text){
        size_t n = SDL_strlen(text);
        if(n > (size_t)columns){
            n = columns;
            size_t word = n;
            while(word && text[word] != ' ') word--;
            if(word) n = word;
        }
        label(x, y, color, 1, "%.*s", (int)n, text);
        text += n;
        while(*text == ' ') text++;
        y += layout.line_height;
    }
}
void
sprite(int x, int y, const SpriteRect parts[:]){
    for(size_t i = 0; i < _Countof parts; i++){
        const SpriteRect* part = &parts[i];
        box(x + part.x, y + part.y, part.w, part.h, part.color);
    }
}
void
button(const ButtonAsset* b){
    SDL_Rect r = b.bounds;
    box(r.x, r.y, r.w, r.h, palette.button_fill);
    outline(r.x, r.y, r.w, r.h, palette.button_border);
    label(r.x + 12, r.y + 12, palette.button_text, 1, "%s", b.text);
}

void
render(World* w){
    box(0, 0, layout.window.w, layout.window.h, palette.background);
    label(24, 22, palette.level_title, 2, "%s", w.route[w.depth].name);
    label(layout.stats.health_x, layout.stats.stat_y, palette.health_text, 1, "HEALTH %d / %d", w.hp > 0? w.hp : 0, MAX_HEALTH);
    box(layout.stats.health_x, layout.stats.bar_y, layout.stats.health_bar_w, layout.stats.bar_h, palette.health_empty);
    box(layout.stats.health_x, layout.stats.bar_y, (w.hp > 0? w.hp : 0)*layout.stats.health_bar_w/MAX_HEALTH, layout.stats.bar_h, palette.health_fill);
    label(layout.stats.mana_x, layout.stats.stat_y, palette.mana_text, 1, "MANA %d / %d", w.mana, MAX_MANA);
    for(int i = 0; i < MAX_MANA; i++)
        box(layout.stats.mana_x + i * layout.stats.mana_pip_step, layout.stats.bar_y, layout.stats.mana_pip_w, layout.stats.bar_h, i < w.mana? palette.mana_fill : palette.mana_empty);
    label(layout.spellbook.x, layout.stats.stat_y, palette.turn_text, 1, "TURN %d   POWER +%d", w.turn, w.power);
    label(layout.spellbook.x, 52, palette.muted_text, 1, "LEVEL %d / %d   GUARDIANS %d / %d", w.depth + 1, RUN_LEVELS, w.monster_count - living_monsters(w), w.monster_count);
    box(layout.board.x, 90, layout.board.w, 26, palette.controls_background);
    _Bool effect = SDL_GetTicks() < w.effect_until && w.depth == w.previous.depth;
    for(int y = 0; y < MAP_H; y++) for(int x = 0; x < MAP_W; x++){
        int px = layout.board.x + x * layout.board.tile, py = layout.board.y + y * layout.board.tile;
        char tile = w.tiles[y][x];
        _Bool wall = tile == '#', island = w.route[w.depth].outdoors;
        box(px, py, layout.board.tile, layout.board.tile, wall? palette.wall_background:((x + y)%2? palette.floor_odd : palette.floor_even));
        if(tile == '~' || tile == '='){
            box(px, py, layout.board.tile, layout.board.tile, (x + y)%2? palette.water_odd : palette.water_even);
            int wave = (int)(SDL_GetTicks()/animation.wave_step_ms + x * 3 + y * 5)%8;
            box(px + 5 + wave, py + 11, 13, 2, palette.wave_light);
            box(px + 19 - wave, py + 27, 12, 2, palette.wave_dark);
            if(tile == '='){
                _Bool vertical = y > 0 && floor_at(w, x, y - 1);
                if(vertical){
                    box(px + 7, py, 26, layout.board.tile, palette.bridge_base);
                    for(int n = 1; n < layout.board.tile; n += 8){
                        box(px + 8, py + n, 24, 6, palette.bridge_plank);
                        box(px + 10, py + n, 20, 1, palette.bridge_highlight);
                    }
                    box(px + 5, py, 3, layout.board.tile, palette.bridge_rail);
                    box(px + 32, py, 3, layout.board.tile, palette.bridge_rail);
                }
                else {
                    box(px, py + 7, layout.board.tile, 26, palette.bridge_base);
                    for(int n = 1; n < layout.board.tile; n+=8){
                        box(px + n, py + 8, 6, 24, palette.bridge_plank);
                        box(px + n, py + 10, 1, 20, palette.bridge_highlight);
                    }
                    box(px, py + 5, layout.board.tile, 3, palette.bridge_rail);
                    box(px, py + 32, layout.board.tile, 3, palette.bridge_rail);
                }
            }
        }
        else if(wall){
            box(px + 2, py + 2, layout.board.tile - 4, layout.board.tile - 8, palette.wall_face);
            box(px + 3, py + 3, layout.board.tile - 6, 3, palette.wall_highlight);
            box(px + 18, py + 7, 2, 17, palette.wall_mortar);
            box(px + 2, py + 25, layout.board.tile - 4, 2, palette.wall_mortar);
        }
        else if(island){
            box(px, py, layout.board.tile, layout.board.tile, (x + y)%2? palette.grass_odd : palette.grass_even);
            if(x > 0 && w.tiles[y][x - 1] == '~') box(px, py, 5, layout.board.tile, palette.sand);
            if(x < MAP_W - 1 && w.tiles[y][x + 1] == '~') box(px + layout.board.tile - 5, py, 5, layout.board.tile, palette.sand);
            if(y > 0 && w.tiles[y - 1][x] == '~') box(px, py, layout.board.tile, 5, palette.sand);
            if(y < MAP_H - 1 && w.tiles[y + 1][x] == '~') box(px, py + layout.board.tile - 5, layout.board.tile, 5, palette.sand);
            if((x * 7 + y * 13)%3==0){
                box(px + 13, py + 17, 3, 6, palette.grass_detail);
                box(px + 17, py + 19, 3, 4, palette.grass_detail);
            }
        }
        else {
            box(px + 3, py + layout.board.tile - 3, layout.board.tile - 6, 1, palette.floor_edge);
            if((x * 7 + y * 13)%5==0) box(px + 9, py + 11, 3, 2, palette.floor_detail);
        }
        if(w.tiles[y][x] == '>'){
            unsigned portal = living_monsters(w)? palette.portal_closed : palette.portal_open;
            outline(px + 7, py + 4, 26, 32, portal);
            outline(px + 12, py + 9, 16, 22, portal);
        }
        if(w.pickups[y][x]) sprite(px, py, w.pickups[y][x].art);
        for(int i = 0; i < w.monster_count; i++){
            Monster* m = &w.monsters[i];
            if(m.hp > 0 && m.charging && m.tx == x && m.ty == y){
                box(px + 3, py + 3, 34, 34, palette.danger_fill);
                outline(px + 2, py + 2, 36, 36, palette.danger_border);
            }
        }
        if(inside(w.hover_x, w.hover_y, px, py, layout.board.tile, layout.board.tile) && floor_at(w, x, y)) outline(px + 1, py + 1, 38, 38, palette.hover_border);
    }
    for(int i = 0; i < w.monster_count; i++){
        Monster* m = &w.monsters[i];
        int px = layout.board.x + m.x * layout.board.tile, py = layout.board.y + m.y * layout.board.tile;
        if(m.hp > 0){
            sprite(px, py, m.definition.art);
            if(m.charging) label(px + 15, py - 12, palette.charging_text, 1, "!");
            box(px + 5, py + 37, 30, 3, palette.wall_background);
            box(px + 5, py + 37, 30 * SDL_min(m.hp, m.max_hp) / m.max_hp, 3, palette.monster_health);
            if(spell_target(w)==i) outline(px, py, layout.board.tile, layout.board.tile, palette.target_border);
        }
        if(effect && m.hp < w.previous.monsters[i].hp){
            label(px + 9, py - 9, palette.damage_text, 1, "-%d", w.previous.monsters[i].hp - m.hp);
        }
    }
    sprite(layout.board.x + w.x * layout.board.tile, layout.board.y + w.y * layout.board.tile, wizard_art);
    if(effect && w.hp < w.previous.hp) outline(layout.board.x + w.x * layout.board.tile, layout.board.y + w.y * layout.board.tile, layout.board.tile, layout.board.tile, palette.player_damage);
    box(layout.board.x, 567, layout.board.w, 77, palette.message_background);
    wrapped(38, 580, 89, palette.message_text, w.message);
    for(size_t i = 0; i < _Countof w.player_spells; i++){
        SpellDesc* spell = &w.player_spells[i];
        int y = layout.spellbook.card_y+(int)i*layout.spellbook.card_h;
        _Bool active = w.selected_spell == i;
        box(layout.spellbook.x, y, layout.spellbook.w, layout.spellbook.card_h - layout.spellbook.card_gap, active? palette.card_active_fill : palette.card_fill);
        outline(layout.spellbook.x, y, layout.spellbook.w, layout.spellbook.card_h - layout.spellbook.card_gap, active? palette.card_active_border : palette.card_border);
        unsigned color = w.mana < spell.mana_cost? palette.spell_unavailable : palette.spell_name;
        label(layout.spellbook.x + 12, y + 9, color, 1, "[%zu] %s", i + 1, spell.name);
        label(layout.spellbook.x + 12, y + 34, palette.spell_summary, 1, "%d mana | %.22s", spell.mana_cost, spell.help);
    }
    SpellDesc* spell = &w.player_spells[w.selected_spell];
    wrapped(layout.spellbook.x, 611, 35, palette.spell_help, spell.help);
    for(size_t i = 0; i < _Countof buttons; i++) button(&buttons[i]);
    for(size_t i = 0; i < _Countof captions; i++){
        const CaptionAsset* c = &captions[i];
        label(c.x, c.y, c.color, c.scale, "%s", c.text);
    }
    if(w.won || w.hp <= 0){
        box(154, 232, 500, 154, palette.outcome_background);
        outline(154, 232, 500, 154, palette.outcome_border);
        label(182, 254, palette.outcome_title, 2, "%s", w.won? "THE VAULT IS YOURS":"EXPEDITION ENDED", );
        label(182, 305, palette.outcome_text, 1, "%s", w.won? "You recovered the Grand Semicolon.":"LMAO YOU DIED.");
        label(182, 341, palette.outcome_text, 1, "Click New run below, or press N.");
    }
    SDL_RenderPresent(canvas);
}
