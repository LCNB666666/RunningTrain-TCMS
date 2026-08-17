#ifndef TCMS_SCREEN_H
#define TCMS_SCREEN_H
/*
 * TCMS 车辆状态屏 — 接口定义
 * 界面尺寸 602x452，黑色背景，白色网格
 * 所有动态数据通过 tcms_set_xxx() 写入，之后调用 tcms_screen_draw() 重绘
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>

#define TCMS_CARS   6    /* 6节车厢: 0=TC1 1=MP1 2=M1 3=M2 4=MP2 5=TC2 */
#define TCMS_W      602
#define TCMS_H      452

typedef struct {
    /* ---- 顶部运行参数 ---- */
    int  net_voltage;        /* 网压 (V)      */
    int  net_current;        /* 网流 (A)      */
    int  traction_level;     /* 牵引级位      */
    int  brake_level;        /* 制动级位      */
    int  speed;              /* 速度 (km/h)   */
    wchar_t date[32];        /* 日期文本 如 L"12月24日" */
    wchar_t time[32];        /* 时间文本 如 L"09:22"   */

    /* ---- 每节车厢 ---- */
    int  emergency_brake;         /* 紧急制动 0/1 */
    int  eb_mode[TCMS_CARS];      /* 牵引电制动: 0=无 1=牵引(绿) 2=制动(黄) 3=紧急(红) */
    int  bc_pressure[TCMS_CARS];  /* BC压力 (kPa)      */
    int  siv[TCMS_CARS];          /* SIV 0/1            */
    int  air_compressor[TCMS_CARS]; /* 空压机运转 0/1   */
    int  fire_alarm[TCMS_CARS];     /* 火灾报警 0/1     */
    int  bhb[2];                    /* BHB 指示灯       */
    int  blb[2];                    /* BLB 指示灯       */
} TCMS_State;

/* ---- 初始化(载入默认界面值) ---- */
void tcms_init(TCMS_State* s);

/* ---- 接口：顶部参数 ---- */
void tcms_set_net_voltage   (TCMS_State* s, int v);
void tcms_set_net_current   (TCMS_State* s, int a);
void tcms_set_traction_level(TCMS_State* s, int lv);
void tcms_set_brake_level   (TCMS_State* s, int lv);
void tcms_set_speed         (TCMS_State* s, int kmh);
void tcms_set_datetime      (TCMS_State* s, const char* date, const char* time);

/* ---- 接口：每节车厢 car = 0..5 (TC1..TC2) ---- */
void tcms_set_traction_eb   (TCMS_State* s, int car, int mode); /* 0无 1牵引绿 2制动黄 3紧急红 */
void tcms_set_emergency_brake(TCMS_State* s, int on);
void tcms_set_bc_pressure   (TCMS_State* s, int car, int kpa);
void tcms_set_siv           (TCMS_State* s, int car, int on);
void tcms_set_air_compressor(TCMS_State* s, int car, int on);
void tcms_set_fire_alarm    (TCMS_State* s, int car, int on);
void tcms_set_bhb           (TCMS_State* s, int idx, int on);   /* idx 0..1 */
void tcms_set_blb           (TCMS_State* s, int idx, int on);   /* idx 0..1 */

/* ---- 渲染 ---- */
/* 把整屏绘制到目标DC。目标尺寸 target_w x target_h（如 602x452 或放大 1.25 倍的 752x565） */
void tcms_screen_draw(const TCMS_State* s, HDC dc, int target_w, int target_h);
/* 保存当前画面为 24 位 BMP（602x452 原始分辨率），成功返回 1 */
int  tcms_screen_save_bmp(const TCMS_State* s, const char* path);

#ifdef __cplusplus
}
#endif
#endif
