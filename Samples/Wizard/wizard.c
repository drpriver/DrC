//
// Small rogue-like game (but you're a wizard). The main program
// acts as an engine that dynamically loads the C source files,
// grabs specific symbols or reflects over them to populate
// gameplay code. All of them are hot reloadable, edit a spell
// and hit 'r' while playing to change how the spell works
// without restarting the game.
//
// Extended language features showcased:
//  - __compile()
//  - _Module reflection
//  - slices
//  - function literals
//  - methods / _Self
//  - #pragma pkg_config / lib
//
#ifdef __linux__
#pragma pkg_config "sdl2"
#endif
#pragma lib "SDL2"
#include <SDL2/SDL.h> <SDL.h>
#include "../SDL2/bitmap_font.h"

#pragma typedef on

enum { MAP_W = 19, MAP_H = 9, MAX_SPELLS = 32, PLAYER_MAX_SPELLS = 6, MAX_GENERATORS = 32, RUN_LEVELS = 3 };
enum { INITIAL_MONSTERS = 3, MONSTERS_PER_LEVEL = 2, MAX_MONSTERS = INITIAL_MONSTERS + (RUN_LEVELS - 1) * MONSTERS_PER_LEVEL };
enum { MAX_HEALTH = 12, MAX_MANA = 6, STAFF_DAMAGE = 2, MONSTER_SPAWN_DISTANCE = 6 };
#include "assets.h"

struct World;
struct MonsterDesc {
    const char* name;
    int base_health,
        damage,
        attack_range,
        notice_range,
        move_turns;
    const SpriteRect art[:];
};
struct Monster {
    int x, y, hp, max_hp;
    int tx, ty;
    _Bool charging;
    const MonsterDesc* definition;
};
typedef void ItemFn(World*);
struct ItemDesc {
    const char* name;
    const SpriteRect art[:];
    ItemFn* collect;
};
struct Rng {
    unsigned state;
    unsigned random_u32(_Self* r){
        unsigned x=r.state ? r.state : 0x9e3779b9u;
        x ^= x<<13;
        x ^= x>>17;
        x ^= x<<5;
        return r.state = x;
    }
};
struct Level {
    char name[48];
    _Bool outdoors;
    char tiles[MAP_H][MAP_W];
    int spawn_x, spawn_y;
    const ItemDesc* pickups[MAP_H][MAP_W];
    int monster_count;
    Monster monsters[MAX_MONSTERS];
};
typedef void LevelGenerator(World*, Level*, Rng*, int);
struct Generator {
    LevelGenerator* generate;
    const char name[:];
};
struct LevelGenerators {
    size_t count;
    Generator items[MAX_GENERATORS];
};

typedef _Bool SpellFn(World*);
struct SpellDesc {
    const char* name;
    const char* help;
    int mana_cost;
    SpellFn* cast;
};

struct ActionSnapshot {
    int turn, depth, hp;
    Monster monsters[MAX_MONSTERS];
};
struct World {
    Level route[RUN_LEVELS];
    const char chosen[RUN_LEVELS][:];
    Rng rng;
    int depth;
    char tiles[MAP_H][MAP_W];
    LevelGenerators generators;
    MonsterDesc all_monsters[:];
    ItemDesc all_items[:];
    SpellDesc all_spells[:];
    SpellDesc player_spells[PLAYER_MAX_SPELLS];
    size_t selected_spell;
    int hover_x, hover_y;
    uint32_t effect_until;
    ActionSnapshot previous;
    _Bool running;
    unsigned seed;
    int x, y, hp, mana, turn;
    _Bool won;
    int target, power;
    const ItemDesc* pickups[MAP_H][MAP_W];
    int monster_count;
    Monster monsters[MAX_MONSTERS];
    char message[160];
};

struct ModuleSource {
    const char* path;
    int (*load)(World*, _Module);
    char* text; // Text of the last successful compile
    size_t size;
};

int load_spells(World*, _Module),
    load_monsters(World*, _Module),
    load_items(World*, _Module),
    load_levels(World*, _Module);
ModuleSource module_sources[] = {
    {__DIR__ "/spells.c", load_spells},
    {__DIR__ "/monsters.c", load_monsters},
    {__DIR__ "/items.c", load_items},
    {__DIR__ "/levels.c", load_levels},
};

int
main(int argc, char** argv){
    World world = { };
    if(load_modules(&world))
        return 1;
    if(!start_run(&world, (unsigned)__RAND__)){
        return 1;
    }
    if(SDL_Init(SDL_INIT_VIDEO)){
        SDL_Log("SDL: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("Wizard", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, layout.window.w, layout.window.h, SDL_WINDOW_SHOWN);
    if(!window){
        SDL_Log("SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    canvas = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if(!canvas) canvas = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if(!canvas || !make_letters()){
        SDL_Log("SDL: %s\n", SDL_GetError());
        if(canvas) SDL_DestroyRenderer(canvas);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    world.running = 1;
    render(&world);
    SDL_Event event;
    while(world.running){
        if(SDL_WaitEventTimeout(&event, animation.event_wait_ms)) handle_event(&world, &event);
        render(&world);
    }
    for(size_t i = 0; i < _Countof module_sources; i++) SDL_free(module_sources[i].text);
    SDL_DestroyTexture(letters);
    SDL_DestroyRenderer(canvas);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

int
load_modules(World* w){
    World next = *w;
    ModuleSource sources[_Countof module_sources];
    SDL_memcpy(sources, module_sources, sizeof sources);
    if(load_modules_into(&next, sources)){
        for(size_t i = 0; i < _Countof sources; i++)
            if(sources[i].text != module_sources[i].text) SDL_free(sources[i].text);
        return 1;
    }
    _Bool changed = 0;
    for(size_t i = 0; i < _Countof sources; i++)
        changed |= sources[i].text != module_sources[i].text;
    if(!changed) return 0;
    for(int i = 0; i < next.monster_count; i++) reload_monster(&next, &next.monsters[i]);
    for(int y = 0; y < MAP_H; y++) for(int x = 0; x < MAP_W; x++)
        reload_item(&next, &next.pickups[y][x]);
    for(int depth = 0; depth < RUN_LEVELS; depth++){
        Level* l = &next.route[depth];
        for(int i = 0; i < l.monster_count; i++){
            reload_monster(&next, &l.monsters[i]);
        }
        for(int y = 0; y < MAP_H; y++) for(int x = 0; x < MAP_W; x++)
            reload_item(&next, &l.pickups[y][x]);
    }
    for(size_t i = 0; i < _Countof sources; i++)
        if(sources[i].text != module_sources[i].text) SDL_free(module_sources[i].text);
    SDL_memcpy(module_sources, sources, sizeof module_sources);
    *w = next;
    return 0;
}

int
load_spells(World* w, _Module module){
    SpellDesc(*exported)[:] = module.symbol("spells", typeof(*exported));
    if(!exported){
        SDL_Log("No symbol named spells exported");
        return 1;
    }
    if(module.run()) {
        SDL_Log("Failed to run '%s'", __DIR__ "/spells.c");
        return 1;
    }
    if(exported.count < PLAYER_MAX_SPELLS || exported.count > MAX_SPELLS || !exported.data) {
        SDL_Log("Invalid spell count: %zu (need %zu to %zu)\n", exported.count, (size_t)PLAYER_MAX_SPELLS, (size_t)MAX_SPELLS);
        return 1;
    }
    for(size_t i = 0; i < _Countof *exported; i++){
        SpellDesc* spell = &(*exported)[i];
        if(!spell.name || !spell.name[0] || !spell.help || spell.mana_cost < 0 || !spell.cast){
            SDL_Log("Spell %zu (%s) is invalid\n", i, spell.name?spell.name:"<no name>");
            return 1;
        }
    }
    w.all_spells = *exported;
    for(size_t i = 0; i < _Countof w.player_spells; i++){
        if(!w.player_spells[i].name) continue;
        for(size_t j = 0; j < _Countof w.all_spells; j++){
            if(!w.all_spells[j].name) continue;
            if(SDL_strcmp(w.player_spells[i].name, w.all_spells[j].name) == 0){
                w.player_spells[i] = w.all_spells[j];
                break;
            }
        }
    }
    return 0;
}

int
load_monsters(World* w, _Module module){
    MonsterDesc(*monsters)[:] = module.symbol("monsters", typeof(*monsters));
    if(!monsters || module.run()) return 1;
    if(!monsters.data || !monsters.count) return 1;
    for(size_t i = 0; i < monsters.count; i++){
        MonsterDesc* d = &(*monsters)[i];
        if(!d.name || !d.name[0] || !d.art.data || !d.art.count || d.base_health <= 0 || d.damage < 0 || d.attack_range < 1 || d.notice_range < 0 || d.move_turns < 1){
            SDL_Log("Invalid monsters definition at %zu", i);
            return 1;
        }
        for(size_t j = 0; j < i; j++)
            if(SDL_strcmp(d.name, (*monsters)[j].name) == 0){
                SDL_Log("Duplicate monsters name: %s", d.name);
                return 1;
            }
    }
    w.all_monsters = *monsters;
    return 0;
}

int
load_items(World* w, _Module module){
    ItemDesc(*items)[:] = module.symbol("items", typeof(*items));
    if(!items || module.run()) return 1;
    if(!items.data || !items.count) return 1;
    for(size_t i = 0; i < items.count; i++){
        ItemDesc* d = &(*items)[i];
        if(!d.name || !d.name[0] || !d.art.data || !d.art.count || !d.collect){
            SDL_Log("Invalid items definition at %zu", i);
            return 1;
        }
        for(size_t j = 0; j < i; j++)
            if(SDL_strcmp(d.name, (*items)[j].name) == 0){
                SDL_Log("Duplicate items name: %s", d.name);
                return 1;
            }
    }
    w.all_items = *items;
    return 0;
}

int
load_levels(World* w, _Module module){
    LevelGenerators next = {0};
    for(size_t i = 0; i < module.func_count; i++){
        _ModuleMember declaration = module.func_decl(i);
        if(declaration.type != LevelGenerator) continue;
        if(next.count == _Countof next.items) {
            SDL_Log("idk why this is an error\n");
            return 1;
        }
        _ModuleMember function = module.func(i);
        if(!function.address) return 1;
        next.items[next.count++] = {(LevelGenerator*)function.address, function.name};
    }
    if(next.count < RUN_LEVELS){
        SDL_Log("Took few level generators: %zu (need at least %zu)", next.count, (size_t)RUN_LEVELS);
        return 1;
    }
    if(module.run()) {
        SDL_Log("Failed to run '%s'\n", __DIR__ "/levels.c");
        return 1;
    }
    SDL_qsort(next.items, next.count, sizeof next.items[0],
        int(const void* a, const void* b){
            const typeof(next.items[0]) *l = a, *r = b;
            return SDL_strcmp(l.name.data, r.name.data);
        }
    );
    w.generators = next;
    return 0;
}


int
load_modules_into(World* w, ModuleSource sources[:]){
    for(size_t i = 0; i < _Countof sources; i++){
        ModuleSource* source = &sources[i];
        size_t size;
        char* text = SDL_LoadFile(source.path, &size);
        if(!text){
            SDL_Log("Unable to read '%s'", source.path);
            return 1;
        }
        if(source.text && size == source.size && SDL_memcmp(text, source.text, size) == 0){
            SDL_free(text);
            SDL_Log("'%s' hasn't changed", source.path);
            continue;
        }
        // Keep the candidate buffer for the caller to commit or discard.
        source.text = text;
        source.size = size;
        _Module module = __compile(text, source.path);
        if(!module || source.load(w, module)){
            SDL_Log("Failed to reload '%s'", source.path);
            return 1;
        }
    }
    return 0;
}

void
reload_monster(World* w, Monster* m){
    if(!m.definition) return;
    for(size_t i = 0; i < w.all_monsters.count; i++)
        if(SDL_strcmp(m.definition.name, w.all_monsters[i].name) == 0){
            m.definition = &w.all_monsters[i];
            return;
        }
}

void
reload_item(World* w, const ItemDesc** item){
    if(!*item) return;
    for(size_t i = 0; i < w.all_items.count; i++)
        if(SDL_strcmp((*item).name, w.all_items[i].name) == 0){
            *item = &w.all_items[i];
            return;
        }
}

_Bool walkable_tile(char c){ return c=='.' || c=='>' || c=='='; }
_Bool
level_floor(Level* l,int x,int y){
    return x>=0 && x<MAP_W && y>=0 && y<MAP_H && walkable_tile(l.tiles[y][x]);
}
void
level_begin(Level* l, const char* name, char fill){
    SDL_memset(l, 0, sizeof *l);
    SDL_snprintf(l.name, sizeof l.name, "%s", name);
    l.spawn_x = 1;
    l.spawn_y = 1;
    for(int y = 0; y < MAP_H; y++)
        for(int x = 0; x < MAP_W; x++)
            l.tiles[y][x] = (x == 0 || y == 0 || x == MAP_W-1 || y == MAP_H - 1) ? '#' : fill;
    l.tiles[1][1] = '.';
    l.tiles[MAP_H-2][MAP_W-2] = '>';
}
void
level_populate(World* w, Level* l, Rng* rng, int depth){
    l.monster_count = INITIAL_MONSTERS + (depth - 1) * MONSTERS_PER_LEVEL;
    if(l.monster_count < 1 || l.monster_count > MAX_MONSTERS || !w.all_monsters.count) return;
    int cells[MAP_W*MAP_H], count = 0;
    for(int y = 1; y < MAP_H-1; y++)
        for(int x = 1; x < MAP_W-1; x++)
            if(l.tiles[y][x] == '.' && !(x==l.spawn_x && y==l.spawn_y))
                cells[count++] = y * MAP_W + x;
    for(int i = count - 1; i > 0; i--){
        int j = rng.random_u32() % (i+1),
            tmp = cells[i];
        cells[i] = cells[j];
        cells[j] = tmp;
    }
    int monsters = 0, items = 0;
    for(int i = 0; i < count; i++){
        int x = cells[i] % MAP_W, y = cells[i] / MAP_W;
        if(monsters < l.monster_count && SDL_abs(x - l.spawn_x) + SDL_abs(y - l.spawn_y) >= MONSTER_SPAWN_DISTANCE){
            const MonsterDesc* definition = &w.all_monsters[monsters % w.all_monsters.count];
            l.monsters[monsters] = {.x = x, .y = y, .hp = depth * definition.base_health,
                                    .max_hp = depth * definition.base_health, .definition = definition};
            monsters++;
        }
        else {
            if((size_t)items == _Countof w.all_items)
                continue;
            l.pickups[y][x] = &w.all_items[items++];
        }
    }
}
_Bool
valid_level(Level* l){
    if(l.monster_count < 1 || l.monster_count > MAX_MONSTERS) return 0;
    if(!l.name[0] || !level_floor(l, l.spawn_x, l.spawn_y)) return 0;
    if(l.tiles[l.spawn_y][l.spawn_x] != '.') return 0;
    int portal = 0;
    for(int y = 0; y < MAP_H; y++)
        for(int x = 0; x < MAP_W; x++){
            char c = l.tiles[y][x];
            if(!walkable_tile(c) && c != '#' && c != '~') return 0;
            if((x==0 || y==0 || x==MAP_W-1 || y==MAP_H-1) && walkable_tile(c))
                return 0;
            portal += c == '>';
            if(l.pickups[y][x] && c !='.')
                return 0;
        }
    if(portal!=1 || l.pickups[l.spawn_y][l.spawn_x]) return 0;
    _Bool seen[MAP_H][MAP_W] = {};
    int queue[MAP_W*MAP_H],
        head = 0,
        tail = 0;
    queue[tail++] = l.spawn_y * MAP_W + l.spawn_x;
    seen[l.spawn_y][l.spawn_x] = 1;
    int dx[4]={1, 0, -1, 0},
        dy[4]={0, 1, 0, -1};
    while(head<tail){
        int pos = queue[head++],
            x = pos % MAP_W,
            y = pos / MAP_W;
        for(int i = 0; i < 4; i++){
            int nx = x + dx[i],
                ny = y + dy[i];
            if(!level_floor(l, nx, ny) || seen[ny][nx])
                continue;
            seen[ny][nx]=1;
            queue[tail++] = ny * MAP_W + nx;
        }
    }
    for(int y = 0; y < MAP_H; y++)
        for(int x = 0; x < MAP_W; x++)
            if(level_floor(l, x, y) && !seen[y][x])
                return 0;
    for(int i = 0; i < l.monster_count; i++){
        Monster* m = &l.monsters[i];
        if(!level_floor(l, m.x, m.y) || m.hp <= 0 || m.max_hp < m.hp || !m.definition || m.charging)
            return 0;
        if(l.tiles[m.y][m.x] != '.' || l.pickups[m.y][m.x] || (m.x == l.spawn_x && m.y == l.spawn_y))
            return 0;
        for(int j=0; j < i; j++)
            if(l.monsters[j].x == m.x && l.monsters[j].y == m.y)
                return 0;
    }
    return 1;
}

__attribute__((format(printf, 2, 3)))
void
say(World* w, const char* text, ...){
    __builtin_va_list va;
    __builtin_va_start(va, text);
    SDL_vsnprintf(w.message, sizeof w.message, text, va);
    __builtin_va_end(va);
}

__attribute__((format(printf, 2, 3)))
void
say_append(World* w, const char* text, ...){
    __builtin_va_list va;
    __builtin_va_start(va, text);
    size_t len = SDL_strlen(w.message);
    SDL_vsnprintf(w.message+len, sizeof w.message-len, text, va);
    __builtin_va_end(va);
}

_Bool
enter_level(World* w, int depth){
    // Choose from the latest generators, excluding names already visited this run.
    int available[MAX_GENERATORS], count = 0;
    for(size_t i = 0; i < w.generators.count; i++){
        _Bool used = 0;
        for(int j = 0; j < depth; j++)
            if(SDL_strcmp(w.chosen[j].data, w.generators.items[i].name.data) == 0) used = 1;
        if(!used) available[count++] = i;
    }
    if(!count){
        say(w, "No unvisited level generators. Edit levels.c and reload.");
        return 0;
    }
    World next = *w;
    int index = available[next.rng.random_u32() % count];
    Generator* generator = &next.generators.items[index];
    Level generated = {};
    generator.generate(&next, &generated, &next.rng, depth + 1);
    if(!valid_level(&generated)){
        say(w, "Generator %s produced an invalid level. Fix it and reload, then retry.", generator.name.data);
        return 0;
    }
    next.route[depth] = generated;
    next.chosen[depth] = generator.name;
    next.depth = depth;
    next.x = generated.spawn_x;
    next.y = generated.spawn_y;
    next.target = -1;
    SDL_memcpy(next.tiles, generated.tiles, sizeof next.tiles);
    SDL_memcpy(next.pickups, generated.pickups, sizeof next.pickups);
    next.monster_count = generated.monster_count;
    SDL_memcpy(next.monsters, generated.monsters, sizeof next.monsters);
    say(&next, "Level %d / %d: %s. Defeat the guardians to open the portal.", depth+1, RUN_LEVELS, generated.name);
    *w = next;
    return 1;
}

_Bool
start_run(World* w, unsigned seed){
    if(w.generators.count < RUN_LEVELS || w.generators.count > MAX_GENERATORS) return 0;
    if(!w.all_spells.data || w.all_spells.count < PLAYER_MAX_SPELLS || w.all_spells.count > MAX_SPELLS) return 0;
    World next = *w;
    next.hp = MAX_HEALTH;
    next.mana = MAX_MANA;
    next.target = -1;
    next.turn = next.power = 0;
    next.won = 0;
    next.effect_until = 0;
    next.seed = seed;
    Rng rng = {seed};
    SDL_memset(next.route, 0, sizeof next.route);
    SDL_memset(next.chosen, 0, sizeof next.chosen);
    size_t idxes[MAX_SPELLS];
    size_t N = _Countof next.all_spells;
    for(size_t i = 0; i < N; i++)
        idxes[i] = i;
    for(size_t i = 0; i < N; i++){
        size_t j = (rng.random_u32() % (N-i)) + i;
        size_t tmp = idxes[i];
        idxes[i] = idxes[j];
        idxes[j] = tmp;
    }
    for(size_t i = 0; i < _Countof next.player_spells; i++){
        size_t s = idxes[i];
        next.player_spells[i] = next.all_spells[s];
    }
    next.rng = rng;
    if(!enter_level(&next, 0)) return 0;
    *w = next;
    return 1;
}

int
monster_at(World* w, int x, int y){
    for(int i = 0; i < w.monster_count; i++){
        Monster* m = &w.monsters[i];
        if(m.hp > 0 && m.x == x && m.y == y) return i;
    }
    return -1;
}

_Bool
floor_at(World* w, int x, int y){
    return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H && walkable_tile(w.tiles[y][x]);
}

_Bool
sight_at(World* w, int x, int y){
    return x>=0 && x<MAP_W && y>=0 && y<MAP_H && (walkable_tile(w.tiles[y][x]) || w.tiles[y][x]=='~');
}

_Bool
line_of_sight(World* w, int x, int y, int tx, int ty){
    if(!floor_at(w, x,y) || !floor_at(w, tx, ty)) return 0;
    int nx = SDL_abs(tx-x), ny = SDL_abs(ty-y);
    int sx = (tx>x)-(tx<x), sy = (ty>y)-(ty<y), ix = 0, iy = 0;
    while(ix<nx || iy<ny){
        int horizontal = (2*ix+1)*ny, vertical = (2*iy+1)*nx;
        if(horizontal == vertical){
            if(!sight_at(w, x+sx, y) || !sight_at(w, x,y+sy)) return 0;
            x+=sx;y+=sy;ix++;iy++;
        }
        else if(horizontal<vertical){x+=sx;ix++;}
        else {y+=sy;iy++;}
        if(!sight_at(w, x,y)) return 0;
    }
    return 1;
}
int
living_monsters(World* w){
    int n = 0;
    for(int i = 0; i < w.monster_count; i++) n += w.monsters[i].hp > 0;
    return n;
}

void
end_turn(World* w){
    w.turn++;
    if(!living_monsters(w) && w.tiles[w.y][w.x] == '>'){
        if(w.depth+1<RUN_LEVELS) enter_level(w, w.depth+1);
        else { w.won = 1; say(w, "Three levels cleared! You escape with the Grand Semicolon."); }
        return;
    }
    int hits = 0;
    for(int i = 0; i < w.monster_count; i++){
        Monster* m = &w.monsters[i];
        if(m.hp <= 0) continue;
        if(m.charging){
            if(w.x == m.tx && w.y == m.ty && line_of_sight(w, m.x, m.y, m.tx, m.ty)){
                int damage = m.definition.damage;
                w.hp -= damage;
                hits += damage;
            }
            m.charging = 0;
            continue;
        }
        int dx = w.x - m.x, dy = w.y - m.y;
        int distance = SDL_abs(dx) + SDL_abs(dy);
        if(distance <= m.definition.attack_range && line_of_sight(w, m.x, m.y, w.x, w.y)){
            m.charging = 1;
            m.tx = w.x; m.ty = w.y;
            continue;
        }
        if(distance > m.definition.notice_range || w.turn % m.definition.move_turns) continue;
        int nx = m.x + (dx > 0) - (dx < 0);
        int ny = m.y;
        if(!dx || !floor_at(w, nx, ny) || monster_at(w, nx, ny) >= 0){
            nx = m.x;
            ny += (dy > 0) - (dy < 0);
        }
        if(floor_at(w, nx, ny) && monster_at(w, nx, ny) < 0 && !(nx == w.x && ny == w.y)){
            m.x = nx; m.y = ny;
        }
    }
    if(hits) say_append(w, " Hit for %d! Dodge the marked tiles.", hits);
    if(w.hp <= 0) say(w, "Your expedition ends here. Press N or click New run.");
}

void
collect_pickup(World* w){
    const ItemDesc* item = w.pickups[w.y][w.x];
    if(!item) return;
    w.pickups[w.y][w.x] = NULL;
    item.collect(w);
}

_Bool
move_player(World* w, int dx, int dy){
    if(w.hp <= 0 || w.won) return 0;
    int x = w.x + dx, y = w.y + dy;
    if(!floor_at(w, x, y)){
        say(w, sight_at(w, x,y)?"Deep water. Cross using a bridge.":"That wall resists your pointer arithmetic.");return 0;
    }
    int m = monster_at(w, x, y);
    if(m >= 0){
        w.monsters[m].hp -= STAFF_DAMAGE;
        say(w, "You bonk %s with your staff (%d damage).", w.monsters[m].definition.name, STAFF_DAMAGE);
    }
    else {
        w.x = x; w.y = y;
        say(w, w.tiles[y][x] == '>' && living_monsters(w)
            ? "The stairs stay sealed until all guardians are defeated." : "Your robe swishes dramatically.");
    }
    collect_pickup(w);
    end_turn(w);
    return 1;
}


_Bool
walk_toward(World* w, int tx, int ty){
    if(!floor_at(w, tx, ty) || (tx == w.x && ty == w.y)) return 0;
    int dist[MAP_H][MAP_W];
    for(int y = 0; y < MAP_H; y++) for(int x = 0; x < MAP_W; x++) dist[y][x] = -1;
    int qx[MAP_W*MAP_H], qy[MAP_W*MAP_H], head = 0, tail = 0;
    qx[tail] = tx; qy[tail++] = ty; dist[ty][tx] = 0;
    int dx[4] = {1, 0,-1, 0}, dy[4] = {0, 1,0, -1};
    while(head < tail){
        int x = qx[head], y = qy[head++];
        for(int d = 0; d < 4; d++){
            int nx = x + dx[d], ny = y + dy[d];
            if(!floor_at(w, nx, ny) || dist[ny][nx] >= 0) continue;
            if(monster_at(w, nx, ny) >= 0) continue;
            dist[ny][nx] = dist[y][x] + 1;
            qx[tail] = nx; qy[tail++] = ny;
        }
    }
    int best = MAP_W*MAP_H, direction = -1;
    for(int d = 0; d < 4; d++){
        int nx = w.x + dx[d], ny = w.y + dy[d];
        if(!floor_at(w, nx, ny)) continue;
        int n = dist[ny][nx];
        if(n >= 0 && n < best){ best = n; direction = d; }
    }
    return direction >= 0 && move_player(w, dx[direction], dy[direction]);
}

int
spell_target(World* w){
    if(w.target >= 0 && w.target < w.monster_count && w.monsters[w.target].hp > 0) return w.target;
    int target = -1, distance = MAP_W + MAP_H;
    for(int i = 0; i < w.monster_count; i++){
        Monster* m = &w.monsters[i];
        int d = SDL_abs(m.x - w.x) + SDL_abs(m.y - w.y);
        if(m.hp > 0 && d < distance && line_of_sight(w, w.x, w.y, m.x, m.y)){ target = i; distance = d; }
    }
    return target;
}

_Bool
cast_spell(World* w, int i){
    if(w.hp <= 0 || w.won || i < 0 || (size_t)i >= _Countof w.player_spells) return 0;
    SpellDesc* spell = &w.player_spells[i];
    if(w.mana < spell.mana_cost){
        say(w, "Not enough mana.");
        return 0;
    }
    if(!spell.cast(w)) return 0;
    w.mana -= spell.mana_cost;
    end_turn(w);
    return 1;
}


_Bool inside(int x, int y, int bx, int by, int bw, int bh){ return x>=bx && y>=by && x < bx + bw && y < by + bh; }

_Bool
handle_key(World* w, int key){
    if(key >= 'A' && key <= 'Z') key += 'a' - 'A';
    if(key == 'q') return 0;
    if(key == 'n'){
        if(!start_run(w, w.seed + 1)){
            say(w, "A generator produced an invalid level. Keeping this run.");
            return 1;
        }
        return 1;
    }
    if(key == 'r'){
        if(load_modules(w)) say(w, "Reload failed.");
        else say(w, "Spells, levels, monsters and items reloaded.");
        return 1;
    }
    if(key == '\t'){
        w.selected_spell = (w.selected_spell + 1) % _Countof w.player_spells;
        return 1;
    }
    if(w.hp <= 0 || w.won){ say(w, "Press N for another adventure, or Q to quit."); return 1; }
    switch(key){
        case 'w': move_player(w, 0, -1); break;
        case 'a': move_player(w, -1, 0); break;
        case 's': move_player(w, 0, 1); break;
        case 'd': move_player(w, 1, 0); break;
        case ' ': say(w, "You contemplate undefined behavior."); end_turn(w); break;
        default: {
            int i = key >= '1' && key < '1' + PLAYER_MAX_SPELLS ? key - '1' :
                (key == '\r' || key == '\n' ? (int)w.selected_spell : -1);
            if(i < 0 || (size_t)i >= _Countof w.player_spells) return 1;
            w.selected_spell = i;
            cast_spell(w, i);
        } break;
    }
    return 1;
}

int
card_at(World* w, int x, int y){
    if(!inside(x, y, layout.spellbook.x, layout.spellbook.card_y, layout.spellbook.w, PLAYER_MAX_SPELLS * layout.spellbook.card_h)) return -1;
    if((y - layout.spellbook.card_y)%layout.spellbook.card_h >= layout.spellbook.card_h - layout.spellbook.card_gap) return -1;
    int i = (y - layout.spellbook.card_y)/layout.spellbook.card_h;
    return (size_t)i < _Countof w.player_spells ? i : -1;
}

ActionSnapshot
snapshot(World* w){
    ActionSnapshot result = {.turn = w.turn, .depth = w.depth, .hp = w.hp};
    SDL_memcpy(result.monsters, w.monsters, sizeof result.monsters);
    return result;
}
void
remember_action(World* w, ActionSnapshot before){
    if(w.turn != before.turn){
        w.previous = before;
        w.effect_until = SDL_GetTicks()+animation.damage_flash_ms;
    }
}
void
click(World* w, int x, int y, int button){
    int card = card_at(w, x, y);
    ActionSnapshot before = snapshot(w);
    if(card >= 0){
        w.selected_spell = card;
        if(button == SDL_BUTTON_LEFT) cast_spell(w, card);
    }
    else if(inside(x, y, layout.board.x, layout.board.y, MAP_W * layout.board.tile, MAP_H * layout.board.tile)){
        int tx = (x - layout.board.x)/layout.board.tile, ty = (y - layout.board.y)/layout.board.tile;
        int m = monster_at(w, tx, ty);
        if(m>=0){
            w.target = m;
            say(w, "Target selected. Click a spell card to cast, or move into it to strike.");
            if(button == SDL_BUTTON_RIGHT) cast_spell(w, w.selected_spell);
        }
        else if(button == SDL_BUTTON_LEFT) walk_toward(w, tx, ty);
    }
    else for(size_t i = 0; i < _Countof buttons; i++){
        const ButtonAsset* b = &buttons[i];
        if(!inside(x, y, b.bounds.x, b.bounds.y, b.bounds.w, b.bounds.h)) continue;
        handle_key(w, b.key);
        break;
    }
    remember_action(w, before);
}

void
handle_event(World* w, SDL_Event* event){
    switch(event.type){
        case SDL_QUIT:
            w.running = 0;
            return;
        case SDL_MOUSEMOTION:
            w.hover_x = event.motion.x;
            w.hover_y = event.motion.y;
            return;
        case SDL_MOUSEBUTTONDOWN:
            click(w, event.button.x, event.button.y, event.button.button);
            return;
        case SDL_KEYDOWN:
            if(event.key.repeat) return;
            break;
        default: return;
    }
    int key = event.key.keysym.sym;
    switch(key){
        case SDLK_UP: key = 'w'; break;
        case SDLK_DOWN: key = 's'; break;
        case SDLK_LEFT: key = 'a'; break;
        case SDLK_RIGHT: key = 'd'; break;
        case SDLK_ESCAPE:
            w.target = -1;
            return;
    }
    ActionSnapshot before = snapshot(w);
    w.running = handle_key(w, key);
    remember_action(w, before);
}

#include "render.c"
