void courtyard(World* w,Level* l,Rng* rng,int depth){
    level_begin(l,"Courtyard",'.');
    for(int y=2;y<MAP_H-1;y+=2) for(int x=4;x<MAP_W-2;x+=2)
        if(rng.random_u32()%3==0) l->tiles[y][x]='#';
    level_populate(w,l,rng,depth);
}
void catacombs(World* w,Level* l,Rng* rng,int depth){
    level_begin(l,"Catacombs",'.');
    for(int x=5;x<=11;x+=6){
        int door=1+rng.random_u32()%(MAP_H-2);
        for(int y=1;y<MAP_H-1;y++) if(y!=door) l->tiles[y][x]='#';
    }
    level_populate(w,l,rng,depth);
}
void crossroads(World* w,Level* l,Rng* rng,int depth){
    level_begin(l,"Crossroads",'#');
    for(int y=1;y<MAP_H-1;y++) for(int x=1;x<MAP_W-1;x++)
        if(y==1 || y==4 || y==7 || x==1 || x==9 || x==17) l->tiles[y][x]='.';
    int room=2+rng.random_u32()%4;
    for(int y=2;y<=3;y++) for(int x=room;x<room+3;x++) l->tiles[y][x]='.';
    l->tiles[MAP_H-2][MAP_W-2]='>';
    level_populate(w,l,rng,depth);
}
void vault(World* w,Level* l,Rng* rng,int depth){
    level_begin(l,"The Inner Vault",'.');
    for(int y=2;y<=6;y++) for(int x=6;x<=12;x++)
        if(y==2 || y==6 || x==6 || x==12) l->tiles[y][x]='#';
    int door=3+rng.random_u32()%3;
    l->tiles[door][6]='.';l->tiles[door][12]='.';
    level_populate(w,l,rng,depth);
}
void islands(World* w,Level* l,Rng* rng,int depth){
    level_begin(l,"Moonstone Islands",'~');
    l->outdoors=1;
    // Ocean surrounds all six land platforms, including the edge of the map.
    for(int y=0;y<MAP_H;y++) for(int x=0;x<MAP_W;x++) l->tiles[y][x]='~';
    for(int y=1;y<MAP_H-1;y++) for(int x=1;x<MAP_W-1;x++)
        if(y!=4 && x!=6 && x!=12) l->tiles[y][x]='.';
    for(int x=3;x<=15;x+=6) l->tiles[4][x]='=';
    for(int x=6;x<=12;x+=6){
        l->tiles[1+rng.random_u32()%3][x]='=';
        l->tiles[5+rng.random_u32()%3][x]='=';
    }
    l->tiles[MAP_H-2][MAP_W-2]='>';
    level_populate(w,l,rng,depth);
}
