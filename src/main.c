// TODO:规范头文件包含写法
#include "rtthread.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include "stdio.h"
#include "string.h"
#include "mem_section.h"
#include "littlevgl2rtt.h"

#if RT_USING_DFS
    #include "dfs_file.h"
    #include "dfs_posix.h"
#endif
#include "drv_flash.h"
#include <drivers/audio.h>
#include "input.h"
#include "audio.h"
#include "tjpgd.h"
#include "game_list.h"
#include "img_decode.h"

// TODO:集中声明
img_decode_result_t no_image_result = {0};  // 默认图标（用于无图标的游戏）
img_decode_result_t bg_result = {0};        // 背景图片

lv_img_dsc_t no_image_dsc;
lv_img_dsc_t bg_dsc;
lv_obj_t * bg_img;

LV_FONT_DECLARE(nmdws_16);

// TODO:分离文件过滤到单独文件
const char supported_extensions[][4] = {
    "nes",
    "NES"
};

#ifndef FS_REGION_START_ADDR
    #error "Need to define file system start address!"
#endif

#define FS_ROOT "root"

/**
 * @brief Mount fs.
 */
int mnt_init(void)
{
    register_mtd_device(FS_REGION_START_ADDR, FS_REGION_SIZE, FS_ROOT);
    if (dfs_mount(FS_ROOT, "/", "elm", 0, 0) == 0) // fs exist
    {
        rt_kprintf("mount fs on flash to root success\n");
    }
    else
    {
        // auto mkfs, remove it if you want to mkfs manual
        rt_kprintf("mount fs on flash to root fail\n");
        if (dfs_mkfs("elm", FS_ROOT) == 0)//Format file system
        {
            rt_kprintf("make elm fs on flash sucess, mount again\n");
            if (dfs_mount(FS_ROOT, "/", "elm", 0, 0) == 0)
                rt_kprintf("mount fs on flash success\n");
            else
                rt_kprintf("mount to fs on flash fail\n");
        }
        else
            rt_kprintf("dfs_mkfs elm flash fail\n");
    }
    return RT_EOK;
}
INIT_ENV_EXPORT(mnt_init);

// TODO:分离SD卡初始化到单独文件,实现热插拔检测
void sdcard_init(void)
{
    rt_device_t msd = rt_device_find("sd0");
    if (msd == NULL)
    {
        rt_kprintf("sd card not found\n");
        return;
    }
    // mkdir("/sdcard",0x0777);
    if (dfs_mount("sd0", "/sdcard", "elm", 0, 0) != 0) // fs exist
    {
        rt_kprintf("mount fs on tf card to /sdcard fail\n");
        rt_kprintf("sd card might not be formatted or is corrupted.\n");
        return;
    }

    rt_kprintf("mount fs on tf card to /sdcard success\n");
}

// TODO:分离PSRAM内存管理到单独文件,重命名
static uint8_t opus_heap_pool[4096 * 1024] L2_RET_BSS_SECT(opus_heap_pool);
static struct rt_memheap opus_memheap;
int opus_heap_init(void)
{
    rt_memheap_init(&opus_memheap, "opus_memheap", (void *)opus_heap_pool,
                    sizeof(opus_heap_pool));
    return 0;
}

void *opus_heap_malloc(uint32_t size)
{
    return rt_memheap_alloc(&opus_memheap, size);
}

void opus_heap_free(void *p)
{
    rt_memheap_free(p);
}

extern int redrawNesFlag;
extern lv_img_dsc_t nes_img_dsc; // LVGL 图像描述符
extern lv_obj_t * nes_img_obj; // LVGL 图像对象
int nofrendo_main(int argc, char *argv[]);

lv_group_t * g;
lv_indev_t * indev;

void keyboard_read(lv_indev_t * indev, lv_indev_data_t * data) {
    if(input_get_key_state(INPUT_KEY_A))
    {
        data->key = LV_KEY_ENTER;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }
    if(input_get_key_state(INPUT_KEY_DOWN))
    {
        data->key = LV_KEY_NEXT;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }    
    if(input_get_key_state(INPUT_KEY_UP))
    {
        data->key = LV_KEY_PREV;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }
        if(input_get_key_state(INPUT_KEY_R))
    {
        data->key = LV_KEY_NEXT;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }    
    if(input_get_key_state(INPUT_KEY_L))
    {
        data->key = LV_KEY_PREV;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }
}

// TODO:删除无用函数
void key_deinit();
rt_thread_t nes_thread = RT_NULL;
lv_obj_t * cont;
extern uint16_t *lcdfb;
void emu_thread_entry(void *parameter)
{
    char* filename = parameter;//"mario.nes";

    char* args[1] = { filename };

    rt_thread_mdelay(100); // wait for lvgl ready

    nofrendo_main(1,args);

    rt_kprintf("Emu thread exit.\n");
    nes_thread = RT_NULL;
    key_deinit();
}

static void btn_event_cb(lv_event_t * e)
{
    rt_kprintf("Button clicked\n");
    if(nes_thread != RT_NULL) return;

    lv_event_code_t code = lv_event_get_code(e);
    const char *game_path = NULL;
    if (code == LV_EVENT_RELEASED) {
        lv_obj_t *btn = lv_event_get_target(e);
        // 从按钮的用户数据中获取游戏路径
        game_path = (const char*)lv_obj_get_user_data(btn);
        if (game_path) {
            rt_kprintf("Clicked game: %s\n", game_path);
        } else {
            rt_kprintf("Error: No game path found!\n");
            return;
        }
    }

    nes_thread = rt_thread_create("nes_emu",
        (void(*)(void*))emu_thread_entry, (void*)game_path,
        16384, 21, 100);
    if (nes_thread != NULL) {
        rt_thread_startup(nes_thread);
    }
    lv_obj_add_flag(bg_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_HIDDEN);
    lv_obj_delete_delayed(cont,500);
}

/* 当按钮获得焦点时，滚动容器使该按钮可见（结合 scroll snap center 可居中） */
static void btn_focused_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    /* 将按钮滚动到可见区域，配合 lv_obj_set_scroll_snap_x 设置可以使其居中 */
    lv_obj_scroll_to_view(btn, LV_ANIM_ON);
}

static void scroll_event_cb(lv_event_t * e)
{
    lv_obj_t * cont = lv_event_get_target(e);
    int scroll_y = lv_obj_get_scroll_y(cont);
    uint32_t child_cnt = lv_obj_get_child_cnt(cont);
    uint32_t i;
    for(i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(cont, i);
        int child_y = lv_obj_get_y(child);
        int rel_y = child_y - scroll_y;
        int diff_to_center = rel_y - (lv_obj_get_height(cont) / 2 - lv_obj_get_height(child) / 2);
        diff_to_center = diff_to_center > 0 ? diff_to_center : -diff_to_center;
        // rt_kprintf("diff to center: %d\n", diff_to_center);
        float ratio = 1.0f;
        if(diff_to_center < 50)
        {
            ratio = 1.0f;
        }
        else if(diff_to_center < 60)
        {
            ratio = 1.0f - ((float)(diff_to_center - 50) / 10.0f) * 0.1f;
        }
        else
        {
            ratio = 0.9f;
        }
        lv_obj_set_style_transform_pivot_x(child, lv_obj_get_width(child) / 2, 0);
        lv_obj_set_style_transform_pivot_y(child, lv_obj_get_height(child) / 2, 0);
        lv_obj_set_style_transform_zoom(child, (int)(256 * ratio), 0);
    }
}

int change_extension(const char *filename, char *new_filename, int new_size, const char *new_ext)
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

int get_extension(const char *filename, char *ext, int ext_size)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return -1;
    strncpy(ext, dot + 1, ext_size - 1);
    ext[ext_size - 1] = '\0'; // Ensure null-termination
    return 0;
}

int extension_filter(const char *ext)
{
    for(size_t i=0;i<sizeof(supported_extensions)/sizeof(supported_extensions[0]);i++)
    {
        if(strcmp(ext, supported_extensions[i])==0) return 1;
    }
    return 0;
}

// 全局游戏列表（用于管理游戏数据）
static GameList_t* current_game_list = NULL;

/**
 * @brief 合并多个游戏列表为一个
 * @param lists 游戏列表数组
 * @param count 列表数量
 * @return 合并后的游戏列表，失败返回 NULL
 */
static GameList_t* merge_game_lists(GameList_t** lists, uint32_t count)
{
    if (!lists || count == 0) return NULL;
    
    // 计算总游戏数量
    uint32_t total_games = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (lists[i]) {
            total_games += lists[i]->game_count;
        }
    }
    
    if (total_games == 0) return NULL;
    
    // 创建合并后的列表
    GameList_t* merged = (GameList_t*)opus_heap_malloc(sizeof(GameList_t));
    if (!merged) return NULL;
    
    merged->base_path = (char*)opus_heap_malloc(16);
    if (merged->base_path) {
        strcpy(merged->base_path, "[Multiple]");
    }
    
    merged->games = (GameInfo_t*)opus_heap_malloc(sizeof(GameInfo_t) * total_games);
    if (!merged->games) {
        if (merged->base_path) opus_heap_free(merged->base_path);
        opus_heap_free(merged);
        return NULL;
    }
    
    // 复制所有游戏信息
    uint32_t index = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (lists[i]) {
            for (uint32_t j = 0; j < lists[i]->game_count; j++) {
                merged->games[index++] = lists[i]->games[j];
            }
            // 清空原列表的 games 指针，防止重复释放
            lists[i]->games = NULL;
            lists[i]->game_count = 0;
        }
    }
    
    merged->game_count = total_games;
    rt_kprintf("Merged %u games from %u directories\n", total_games, count);
    
    return merged;
}

void list_cleanup(void)
{
    if (current_game_list)
    {
        rt_kprintf("Cleaning up game list...\n");
        free_game_list(current_game_list);
        current_game_list = NULL;
    }
}

void list_init(void)
{
    // 创建容器
    cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cont, 320, 240);
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, 0);
    /* Remove rounded corners and border for the container */
    lv_obj_set_style_radius(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_border_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(cont, scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_set_style_clip_corner(cont, true, 0);
    lv_obj_set_scroll_dir(cont, LV_DIR_VER);
    /* 垂直方向启用 snap 到中心，这样在调用 scroll_to_view 时会把子对象居中显示 */
    lv_obj_set_scroll_snap_y(cont, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(cont, LV_SCROLLBAR_MODE_ON);
    lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);

    // 扫描多个目录的游戏列表
    rt_kprintf("Scanning game lists from multiple directories...\n");
    
    const char* scan_paths[] = {
        "/",                    // 根目录
        "/sdcard/roms/nes"      // SD 卡 NES 游戏目录
    };
    const uint32_t path_count = sizeof(scan_paths) / sizeof(scan_paths[0]);
    
    GameList_t* temp_lists[path_count];
    uint32_t valid_lists = 0;
    
    // 扫描每个目录
    for (uint32_t i = 0; i < path_count; i++) {
        rt_kprintf("Scanning: %s\n", scan_paths[i]);
        temp_lists[i] = scan_game_list(scan_paths[i]);
        if (temp_lists[i] && temp_lists[i]->game_count > 0) {
            rt_kprintf("  Found %u game(s)\n", temp_lists[i]->game_count);
            valid_lists++;
        } else {
            rt_kprintf("  No games found\n");
        }
    }
    
    // 合并所有游戏列表
    if (valid_lists > 0) {
        current_game_list = merge_game_lists(temp_lists, path_count);
    }
    
    // 释放临时列表（已经合并，只需释放结构体）
    for (uint32_t i = 0; i < path_count; i++) {
        if (temp_lists[i]) {
            if (temp_lists[i]->base_path) opus_heap_free(temp_lists[i]->base_path);
            opus_heap_free(temp_lists[i]);
        }
    }
    
    if (!current_game_list || current_game_list->game_count == 0)
    {
        rt_kprintf("No games found in any directory!\n");
        return;
    }

    rt_kprintf("Total: %u game(s), creating UI...\n", current_game_list->game_count);

    // 为每个游戏创建按钮
    for (uint32_t i = 0; i < current_game_list->game_count; i++)
    {
        GameInfo_t *game = &current_game_list->games[i];
        
        // 创建图像按钮
        lv_obj_t * btn = lv_imagebutton_create(cont);
        
        // 尝试解码游戏图标
        if (game->icon_path && decode_game_icon(game) == 0)
        {
            // 成功解码图标，创建 LVGL 图像描述符
            static lv_img_dsc_t game_icon_dsc[32]; // 假设最多32个游戏
            if (i < 32)
            {
                game_icon_dsc[i].header.always_zero = 0;
                game_icon_dsc[i].header.w = game->icon.width;
                game_icon_dsc[i].header.h = game->icon.height;
                game_icon_dsc[i].header.cf = LV_COLOR_FORMAT_RGB888;
                game_icon_dsc[i].data_size = game->icon.width * game->icon.height * 3;
                game_icon_dsc[i].data = game->icon.data;
                
                lv_imagebutton_set_src(btn, LV_IMGBTN_STATE_RELEASED, NULL, &game_icon_dsc[i], NULL);
            }
            else
            {
                // 超过数组大小，使用默认图标
                lv_imagebutton_set_src(btn, LV_IMGBTN_STATE_RELEASED, NULL, &no_image_dsc, NULL);
            }
        }
        else
        {
            // 没有图标或解码失败，使用默认图标
            lv_imagebutton_set_src(btn, LV_IMGBTN_STATE_RELEASED, NULL, &no_image_dsc, NULL);
        }
        
        // 设置按钮属性
        lv_group_add_obj(g, btn);
        lv_obj_set_width(btn, 120);
        lv_obj_set_height(btn, 90);
        lv_obj_set_align(btn, LV_ALIGN_CENTER);
        
        // // 设置圆角样式
        // lv_obj_set_style_radius(btn, 24, 0);
        // lv_obj_set_style_clip_corner(btn, true, 0);
        
        // 设置阴影样式
        lv_obj_set_style_shadow_width(btn, 8, 0);
        lv_obj_set_style_shadow_spread(btn, 2, 0);
        lv_obj_set_style_shadow_ofs_x(btn, 0, 0);
        lv_obj_set_style_shadow_ofs_y(btn, 4, 0);
        lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_30, 0);
        
        // 聚焦时增强阴影效果
        lv_obj_set_style_shadow_width(btn, 12, LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_spread(btn, 3, LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_ofs_y(btn, 6, LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_50, LV_STATE_FOCUSED);

        // 将游戏路径存储在按钮的用户数据中
        lv_obj_set_user_data(btn, (void*)game->path);
        
        // 创建标签显示游戏名称
        lv_obj_t * label = lv_label_create(btn);
        lv_label_set_text(label, game->name);
        lv_obj_set_style_text_font(label, &nmdws_16, 0);
        /* Make the label narrower than the button and enable circular scrolling
         * so long filenames will scroll (marquee). */
        lv_obj_set_width(label, lv_pct(100));
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        
        // 添加事件回调
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_RELEASED, NULL);
        /* 当按钮被键盘/方向键选中（获得焦点）时，让容器滚动并居中该按钮 */
        lv_obj_add_event_cb(btn, btn_focused_cb, LV_EVENT_FOCUSED, NULL);
    }

    /*Update the buttons position manually for first*/
    lv_obj_send_event(cont, LV_EVENT_SCROLL, NULL);

    /*Be sure the first button is in the middle*/
    if (lv_obj_get_child_cnt(cont) > 0)
    {
        lv_obj_scroll_to_view(lv_obj_get_child(cont, 0), LV_ANIM_OFF);
    }
    
    rt_kprintf("Game list UI created successfully.\n");
}

// TODO:删除无用函数
void delete_nes_thread(void)
{
    if(nes_thread != RT_NULL) rt_thread_delete(nes_thread);
}
MSH_CMD_EXPORT(delete_nes_thread, delete nes thread);

/**
 * @brief 更换背景图片
 * @param filename 新的背景图片文件路径
 * @return 0: 成功, -1: 失败
 */
int change_background(int argc, char *argv[])
{
    if (argc < 2)
    {
        rt_kprintf("Usage: change_background <image_file>\n");
        return -1;
    }

    const char* filename = argv[1];

    if (!filename)
    {
        rt_kprintf("Error: filename is NULL\n");
        return -1;
    }

    rt_kprintf("Changing background to: %s\n", filename);

    // 临时解码结果
    img_decode_result_t new_bg_result = {0};
    
    // 解码新的背景图片
    if (img_decode_from_file(filename, &new_bg_result) != 0)
    {
        rt_kprintf("Failed to decode background image: %s\n", filename);
        return -1;
    }

    // 调整图片大小到屏幕尺寸
    if (img_decode_resize_image(&new_bg_result, 320, 240) != 0)
    {
        rt_kprintf("Failed to resize background image\n");
        img_free_decode_result(&new_bg_result);
        return -1;
    }

    // 释放旧的背景图片数据
    if (bg_result.data)
    {
        img_free_decode_result(&bg_result);
    }

    // 更新全局背景数据
    bg_result = new_bg_result;
    
    // 更新 LVGL 图像描述符
    bg_dsc.header.always_zero = 0;
    bg_dsc.header.w = bg_result.width;
    bg_dsc.header.h = bg_result.height;
    bg_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
    bg_dsc.data_size = bg_result.width * bg_result.height * 3;
    bg_dsc.data = bg_result.data;

    // 如果背景图像对象已存在，更新其图像源
    if (bg_img)
    {
        lv_img_set_src(bg_img, &bg_dsc);
        lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);
        
        // 重新计算缩放比例
        float ratio_x = (float)320 / (float)bg_dsc.header.w;
        float ratio_y = (float)240 / (float)bg_dsc.header.h;
        float ratio = ratio_x > ratio_y ? ratio_x : ratio_y;
        lv_img_set_zoom(bg_img, (int)(256 * ratio));
        
        rt_kprintf("Background changed successfully\n");
    }
    else
    {
        rt_kprintf("Warning: bg_img object not initialized yet\n");
    }

    return 0;
}
MSH_CMD_EXPORT(change_background, change background image);


/**
 * @brief  Main program
 * @param  None
 * @retval 0 if success, otherwise failure number
 */
int main(void)
{
    /* Output a message on console using printf function */
    rt_kprintf("lchspi-extension.\n");

    opus_heap_init();

    rt_err_t ret = RT_EOK;
    /* init littlevGL */
    ret = littlevgl2rtt_init("lcd");
    HAL_PIN_Set(PAD_PA00 + 41, LCDC1_8080_DIO5, PIN_PULLDOWN, 1);
    HAL_PIN_Set(PAD_PA00 + 37, LCDC1_8080_DIO2, PIN_PULLDOWN, 1);
    if (ret != RT_EOK)
    {
        return ret;
    }
    // 旋转屏幕显示方向
    lv_display_set_rotation(NULL, LV_DISPLAY_ROTATION_270);
    // 设置屏幕背景色为黑色
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    sdcard_init();

    audio_init();

    // 默认图标解码
    if (img_decode_from_file("no_image.jpg", &no_image_result) == 0)
    {
        rt_kprintf("Default game icon decoded successfully.\n");
        img_decode_resize_image(&no_image_result, 120, 90);
        no_image_dsc.header.always_zero = 0;
        no_image_dsc.header.w = no_image_result.width;
        no_image_dsc.header.h = no_image_result.height;
        no_image_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
        no_image_dsc.data_size = no_image_result.width * no_image_result.height * 3;
        no_image_dsc.data = no_image_result.data;
    }
    else
    {
        rt_kprintf("Failed to decode no_image.jpg (default icon)\n");
    }

    // 背景图片解码
    if (img_decode_from_file("bg.jpg", &bg_result) == 0)
    {
        rt_kprintf("Background image decoded successfully.\n");
        img_decode_resize_image(&bg_result, 320, 240);
        bg_dsc.header.always_zero = 0;
        bg_dsc.header.w = bg_result.width;
        bg_dsc.header.h = bg_result.height;
        bg_dsc.header.cf = LV_COLOR_FORMAT_RGB888;
        bg_dsc.data_size = bg_result.width * bg_result.height * 3;
        bg_dsc.data = bg_result.data;
    }
    else
    {
        rt_kprintf("Failed to decode bg.jpg\n");
    }

    input_init();
    g = lv_group_create();
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, keyboard_read);
    lv_indev_set_group(indev, g);
    bg_img = lv_img_create(lv_scr_act());
    lv_img_set_src(bg_img, &bg_dsc);
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);
    float ratio_x = (float)320 / (float)bg_dsc.header.w;
    float ratio_y = (float)240 / (float)bg_dsc.header.h;
    float ratio = ratio_x > ratio_y ? ratio_x : ratio_y;
    lv_img_set_zoom(bg_img, (int)(256 * ratio));
    list_init(); 
    /* Infinite loop */

    while (1)
    {
        if(nes_thread == RT_NULL)
        {
            if(nes_img_obj != NULL)
            {
                lv_obj_delete(nes_img_obj);
                nes_img_obj = NULL;
                free(lcdfb);
                lcdfb = NULL;
                // 清理旧的游戏列表（如果存在）
                list_cleanup();
                // 重新创建游戏列表 UI
                list_init();
                lv_obj_clear_flag(bg_img, LV_OBJ_FLAG_HIDDEN);
            }
            uint32_t ms = lv_task_handler();
            rt_thread_mdelay(ms);
        }
        else
        {
            rt_thread_mdelay(10);
        }
    }
    return 0;
}

