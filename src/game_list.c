#include "game_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rtthread.h"

#if RT_USING_DFS
    #include "dfs_file.h"
    #include "dfs_posix.h"
#endif

// PSRAM 内存分配函数（在 main.c 中定义）
extern void *opus_heap_malloc(uint32_t size);
extern void opus_heap_free(void *p);

// 支持的游戏文件扩展名
static const char supported_extensions[][4] = {
    "nes",
    "NES"
};

/**
 * @brief 获取文件扩展名
 * @param filename 文件名
 * @param ext 输出扩展名的缓冲区
 * @param ext_size 缓冲区大小
 * @return 0 成功, -1 失败
 */
static int get_extension(const char *filename, char *ext, int ext_size)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return -1;
    strncpy(ext, dot + 1, ext_size - 1);
    ext[ext_size - 1] = '\0'; // 确保字符串结束
    return 0;
}

/**
 * @brief 检查扩展名是否在支持列表中
 * @param ext 扩展名
 * @return 1 支持, 0 不支持
 */
static int extension_filter(const char *ext)
{
    for(size_t i = 0; i < sizeof(supported_extensions) / sizeof(supported_extensions[0]); i++)
    {
        if(strcmp(ext, supported_extensions[i]) == 0) return 1;
    }
    return 0;
}

/**
 * @brief 更改文件扩展名
 * @param filename 原始文件名
 * @param new_filename 新文件名缓冲区
 * @param new_size 缓冲区大小
 * @param new_ext 新扩展名
 * @return 0 成功, -1 失败
 */
static int change_extension(const char *filename, char *new_filename, int new_size, const char *new_ext)
{
    const char *dot = strrchr(filename, '.');
    int base_len;
    if (!dot || dot == filename)
        base_len = strlen(filename);
    else
        base_len = dot - filename;
    if (base_len + 1 + strlen(new_ext) + 1 > new_size)
        return -1;
    strncpy(new_filename, filename, base_len);
    new_filename[base_len] = '.';
    strcpy(new_filename + base_len + 1, new_ext);
    return 0;
}

/**
 * @brief 扫描指定目录，创建游戏列表
 * @param directory_path 要扫描的目录路径
 * @return 指向 GameList_t 的指针，失败返回 NULL
 */
GameList_t* scan_game_list(const char* directory_path)
{
    if (!directory_path) return NULL;

    GameList_t* game_list = (GameList_t*)opus_heap_malloc(sizeof(GameList_t));
    if (!game_list) return NULL;

    // 初始化游戏列表
    game_list->base_path = (char*)opus_heap_malloc(strlen(directory_path) + 1);
    if (!game_list->base_path)
    {
        opus_heap_free(game_list);
        return NULL;
    }
    strcpy(game_list->base_path, directory_path);
    game_list->games = NULL;
    game_list->game_count = 0;

    // 打开目录
    DIR *dirp = opendir(directory_path);
    if (dirp == RT_NULL)
    {
        rt_kprintf("Failed to open directory: %s\n", directory_path);
        opus_heap_free(game_list->base_path);
        opus_heap_free(game_list);
        return NULL;
    }

    // 第一遍扫描：计算游戏文件数量
    struct dirent *d;
    uint32_t count = 0;
    while ((d = readdir(dirp)) != RT_NULL)
    {
        char ext[16];
        if (get_extension(d->d_name, ext, sizeof(ext)) == 0)
        {
            if (extension_filter(ext))
            {
                count++;
            }
        }
    }

    if (count == 0)
    {
        rt_kprintf("No game files found in directory: %s\n", directory_path);
        closedir(dirp);
        return game_list; // 返回空列表
    }

    // 分配游戏信息数组
    game_list->games = (GameInfo_t*)opus_heap_malloc(sizeof(GameInfo_t) * count);
    if (!game_list->games)
    {
        closedir(dirp);
        opus_heap_free(game_list->base_path);
        opus_heap_free(game_list);
        return NULL;
    }

    // 第二遍扫描：填充游戏信息
    rewinddir(dirp);
    uint32_t index = 0;
    while ((d = readdir(dirp)) != RT_NULL && index < count)
    {
        char ext[16];
        if (get_extension(d->d_name, ext, sizeof(ext)) == 0)
        {
            if (extension_filter(ext))
            {
                GameInfo_t *game = &game_list->games[index];
                
                // 分配并复制游戏名称
                game->name = (char*)opus_heap_malloc(strlen(d->d_name) + 1);
                if (game->name)
                {
                    strcpy(game->name, d->d_name);
                }

                // 构建游戏完整路径
                size_t path_len = strlen(directory_path) + strlen(d->d_name) + 2;
                game->path = (char*)opus_heap_malloc(path_len);
                if (game->path)
                {
                    if (directory_path[strlen(directory_path) - 1] == '/')
                        snprintf(game->path, path_len, "%s%s", directory_path, d->d_name);
                    else
                        snprintf(game->path, path_len, "%s/%s", directory_path, d->d_name);
                }

                // 构建图标路径（将扩展名改为 .jpg）
                char icon_filename[256];
                snprintf(icon_filename, sizeof(icon_filename), "%s/%s", directory_path, d->d_name);
                char icon_path_buf[256];
                if (change_extension(icon_filename, icon_path_buf, sizeof(icon_path_buf), "jpg") == 0)
                {
                    // 检查图标文件是否存在
                    if (access(icon_path_buf, 0) == 0)
                    {
                        game->icon_path = (char*)opus_heap_malloc(strlen(icon_path_buf) + 1);
                        if (game->icon_path)
                        {
                            strcpy(game->icon_path, icon_path_buf);
                        }
                    }
                    else
                    {
                        game->icon_path = NULL; // 图标不存在
                    }
                }
                else
                {
                    game->icon_path = NULL;
                }

                // 初始化图标解码结果（尚未解码）
                game->icon.data = NULL;
                game->icon.width = 0;
                game->icon.height = 0;

                index++;
            }
        }
    }

    game_list->game_count = index;
    closedir(dirp);

    rt_kprintf("Scanned %u game(s) from %s\n", game_list->game_count, directory_path);
    return game_list;
}

/**
 * @brief 释放游戏列表内存
 * @param game_list 要释放的游戏列表
 */
void free_game_list(GameList_t* game_list)
{
    if (!game_list) return;

    // 释放基础路径
    if (game_list->base_path)
    {
        opus_heap_free(game_list->base_path);
    }

    // 释放每个游戏的信息
    if (game_list->games)
    {
        for (uint32_t i = 0; i < game_list->game_count; i++)
        {
            if (game_list->games[i].name)
                opus_heap_free(game_list->games[i].name);
            if (game_list->games[i].path)
                opus_heap_free(game_list->games[i].path);
            if (game_list->games[i].icon_path)
                opus_heap_free(game_list->games[i].icon_path);
            // 释放已解码的图标数据
            if (game_list->games[i].icon.data)
                img_free_decode_result(&game_list->games[i].icon);
        }
        opus_heap_free(game_list->games);
    }

    opus_heap_free(game_list);
}

/**
 * @brief 更新游戏列表（重新扫描目录）
 * @param game_list 原有的游戏列表（如果不为 NULL 将被释放）
 * @param directory_path 要扫描的目录路径
 * @return 新的游戏列表，失败返回 NULL
 */
GameList_t* update_game_list(GameList_t* game_list, const char* directory_path)
{
    // 释放旧列表
    if (game_list)
    {
        free_game_list(game_list);
    }

    // 重新扫描
    return scan_game_list(directory_path);
}

/**
 * @brief 解码游戏图标到 PSRAM（统一尺寸为 120x90）
 * @param game 游戏信息结构体指针
 * @return 0 成功，-1 失败（无图标路径或解码失败）
 */
int decode_game_icon(GameInfo_t* game)
{
    if (!game) {
        rt_kprintf("Invalid game pointer\n");
        return -1;
    }

    // 检查是否有图标路径
    if (!game->icon_path) {
        rt_kprintf("No icon path for game: %s\n", game->name ? game->name : "Unknown");
        return -1;
    }

    // 如果已经解码过，先释放旧数据
    if (game->icon.data) {
        rt_kprintf("Icon already decoded, freeing old data\n");
        img_free_decode_result(&game->icon);
    }

    // 解码图标（使用 RGB888 格式以获得更好的质量）
    int ret = img_decode_from_file(game->icon_path, &game->icon);
    
    if (ret == 0) {
        rt_kprintf("Successfully decoded icon for %s (%ux%u)\n", 
                   game->name ? game->name : "Unknown",
                   game->icon.width, 
                   game->icon.height);
        
        // 统一调整图标大小为 120x90（中心裁剪）
        const uint32_t target_width = 120;
        const uint32_t target_height = 90;
        
        if (game->icon.width != target_width || game->icon.height != target_height) {
            rt_kprintf("Resizing icon from %ux%u to %ux%u...\n", 
                       game->icon.width, game->icon.height,
                       target_width, target_height);
            
            if (img_decode_resize_image(&game->icon, target_width, target_height) == 0) {
                rt_kprintf("Icon resized successfully\n");
            } else {
                rt_kprintf("Failed to resize icon, keeping original size\n");
            }
        }
        
        return 0;
    } else {
        rt_kprintf("Failed to decode icon for %s\n", game->name ? game->name : "Unknown");
        // 确保 icon 结构体清零
        game->icon.data = NULL;
        game->icon.width = 0;
        game->icon.height = 0;
        return -1;
    }
}
