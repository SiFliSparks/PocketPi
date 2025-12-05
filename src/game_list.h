#ifndef GAME_LIST_H
#define GAME_LIST_H

#include <stdint.h>
#include "img_decode.h"

typedef struct GameInfo
{
    char* name;
    char *path;
    char* icon_path;
    img_decode_result_t icon;
} GameInfo_t;

typedef struct GameList
{
    char* base_path;
    GameInfo_t* games;
    uint32_t game_count;
} GameList_t;

GameList_t* scan_game_list(const char* directory_path);
void free_game_list(GameList_t* game_list);
GameList_t* update_game_list(GameList_t* game_list, const char* directory_path);
int decode_game_icon(GameInfo_t* game);

#endif // GAME_LIST_H