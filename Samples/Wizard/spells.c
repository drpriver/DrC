SpellDesc definitions[] = {
    {
        .name="Spark", .help="Zap a visible target within 5 tiles for 3 damage.", .mana_cost = 2,
        .cast=_Bool(World* w){
            int i = spell_target(w);
            if(i < 0 || SDL_abs(w->monsters[i].x - w->x) + SDL_abs(w->monsters[i].y - w->y) > 5){
                say(w, "No target within spark range (5 tiles).");
                return 0;
            }
            if(!line_of_sight(w, w->x, w->y, w->monsters[i].x, w->monsters[i].y)){
                say(w, "A wall blocks the shot.");
                return 0;
            }
            int damage = 3 + w->power;
            w->monsters[i].hp -= damage;
            say(w, "Spark hits for %d damage!", damage);
            return 1;
        },
    },
    {
        .name="Mend", .help="Recover 4 health, up to 12.", .mana_cost = 2,
        .cast=_Bool(World* w){
            if(w->hp >= MAX_HEALTH){
                say(w, "Already at full health.");
                return 0;
            }
            w->hp += 4;
            if(w->hp > MAX_HEALTH) w->hp = MAX_HEALTH;
            say(w, "You patch your robe and recover up to 4 HP.");
            return 1;
        },
    },
    {
        .name="Confetti", .help="Blast every visible foe within 3 tiles for 2.", .mana_cost = 3,
        .cast=_Bool(World* w){
            _Bool hit = 0;
            for(int i = 0;i < w->monster_count;i++){
                Monster* m = &w->monsters[i];
                if(m->hp > 0 && SDL_abs(m->x - w->x)+SDL_abs(m->y - w->y)<=3 && line_of_sight(w, w->x, w->y, m->x, m->y)){
                    m->hp -= 2 + w->power;
                    hit = 1;
                }
            }
            say(w, hit?"POP! Weaponized confetti!":"No visible foes within 3 tiles.");
            return hit;
        },
    },
    {
        .name="Gust", .help="Deal 1 damage, knock the target away and interrupt its attack. Range 6.", .mana_cost = 2,
        .cast=_Bool(World* w){
            int i = spell_target(w);
            if(i < 0){
                say(w,"No target to push.");
                return 0;
            }
            Monster* m=&w->monsters[i];
            int dx = m->x - w->x, dy = m->y - w->y;
            if(SDL_abs(dx) + SDL_abs(dy)>6){
                say(w, "Push range is 6 tiles.");
                return 0;
            }
            if(!line_of_sight(w, w->x, w->y, m->x, m->y)){
                say(w, "A wall blocks the push.");
                return 0;
            }
            if(SDL_abs(dx) >= SDL_abs(dy)){
                dx = (dx > 0)-(dx < 0);
                dy = 0;
            }
            else {
                dy = (dy > 0)-(dy < 0);
                dx = 0;
            }
            m->hp--;
            m->charging = 0;
            for(int i = 0; i < 2; i++){
                int x = m->x + dx, y = m->y + dy;
                if(floor_at(w, x, y) && monster_at(w, x, y) < 0){
                    m->x = x;
                    m->y = y;
                }
            }
            say(w, "Gust! 1 damage, knockback, and the attack is interrupted.");
            return 1;
        },
    },
    {
        .name="Ether", .help="Recover 1 mana when < 2.", .mana_cost = 0,
        .cast=_Bool(World* w){
            if(w->mana >= 2){
                say(w, "Ether only works when mana is low.");
                return 0;
            }
            w->mana++;
            say(w, "You drink a sip of ether.");
            return 1;
        },
    },
    {
        .name = "Teleport", .help = "Teleport! to a random location.", .mana_cost = 1,
        .cast = _Bool(World* w){
            unsigned valid_cells[MAP_W * MAP_H];
            unsigned count = 0;
            for(int y = 1; y < MAP_H - 1; y++){
                for(int x = 1; x < MAP_W - 1; x++){
                    if(floor_at(w, x, y) &&
                       monster_at(w, x, y) < 0 &&
                       w->tiles[y][x] == '.' &&
                       !(x == w->x && y == w->y)){
                        valid_cells[count++] = y * MAP_W + x;
                    }
                }
            }
            if(count == 0){
                say(w, "No valid destination found.");
                return 0;
            }
            unsigned choice = valid_cells[w.rng.random_u32() % count];
            w->x = choice % MAP_W;
            w->y = choice / MAP_W;
            collect_pickup(w);
            say(w, "Blink! You materialize in a new spot.");
            return 1;
        },
    },
    {
        .name = "Bomb", .help = "Lay a bomb.", .mana_cost = 1,
        .cast = _Bool(World* w){
            say(w, "Sorry, unimplemented");
            return 0;
        },
    },
    {
        .name = "Sudoku", .help = "Play sudoku", .mana_cost = 0,
        .cast = _Bool(World*w){
            system(__if(_WIN32, "start \"Sudoku\" ", "") "drc " __DIR__ "/../SDL2/sudoku.c" __if(!_WIN32, " &", ""));
            return 1;
        }
    },
};
SpellDesc spells[:] = definitions;
