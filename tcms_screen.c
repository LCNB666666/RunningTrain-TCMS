/* TCMS 车辆状态屏 — 实现（Win32 GDI 渲染，602x452） */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "tcms_screen.h"

/* GDI+ 抗锯齿渲染桥接层（gfx_gdiplus.cpp） */
extern void gfx_poly(HDC dc, const POINT* pts, int n, COLORREF c);
extern void gfx_ellipse(HDC dc, int x, int y, int w, int h, COLORREF c, int fill);
extern void gfx_text(HDC dc, int x, int y, int w, int h, const wchar_t* s, int size, COLORREF c, int bold, int left);
extern void gfx_text_extent(HDC dc, const wchar_t* s, int size, int bold, int* pw, int* ph);

/* ================= 工具函数（全部显式按 g_sc 缩放坐标） ================= */
static float g_sc = 1.0f;
static int SX(int v) { return (int)(v * g_sc + 0.5f); }

static void fill_rect(HDC dc, int x, int y, int w, int h, COLORREF c) {
    RECT r = { SX(x), SX(y), SX(x + w), SX(y + h) };
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &r, b);
    DeleteObject(b);
}
static void frame_rect(HDC dc, int x, int y, int w, int h, COLORREF c, int wid) {
    HPEN p = CreatePen(PS_SOLID, SX(wid) > 0 ? SX(wid) : 1, c);
    HGDIOBJ op = SelectObject(dc, p);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, SX(x), SX(y), SX(x + w + 1), SX(y + h + 1));  /* 右/下边 +1 闭合 */
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(p);
}
static void line(HDC dc, int x1, int y1, int x2, int y2, COLORREF c, int wid) {
    HPEN p = CreatePen(PS_SOLID, SX(wid) > 0 ? SX(wid) : 1, c);
    HGDIOBJ op = SelectObject(dc, p);
    MoveToEx(dc, SX(x1), SX(y1), NULL);
    LineTo(dc, SX(x2), SX(y2));
    SelectObject(dc, op);
    DeleteObject(p);
}
static void poly_fill(HDC dc, const POINT* pts, int n, COLORREF c) {
    int i;
    if (g_sc == 1.0f) { gfx_poly(dc, pts, n, c); return; }
    {
        POINT* p2 = (POINT*)malloc((size_t)n * sizeof(POINT));
        for (i = 0; i < n; i++) { p2[i].x = SX(pts[i].x); p2[i].y = SX(pts[i].y); }
        gfx_poly(dc, p2, n, c);
        free(p2);
    }
}
static void ellipse_d(HDC dc, int x, int y, int w, int h, COLORREF c, int fill) {
    int dx = SX(x), dy = SX(y);
    /* 2026-08-17 修复: 直径只由逻辑尺寸决定 (SX(w)/SX(h)) — 原 SX(x+w)-SX(x)
       两端点独立取整, 同一尺寸的圆在不同 x 得到 8 或 9px 宽度 -> 椭圆/正圆
       交替 (用户实测: 一正圆配一椭圆重复). 统一后所有轮都是 9x9 正圆. */
    int dw = SX(w), dh = SX(h);
    if (!fill) {
        /* 2026-08-17 空心轮重画 (用户反馈: 细笔环糊块 / 1px 锯齿都不行):
           白实心圆 + 黑心挖孔 = 两个 AA 实心圆叠加, 小尺寸下圆滑规整 */
        int in_w = dw - 4, in_h = dh - 4;   /* 内孔小 4px -> ~2px 环宽 */
        if (in_w < 2) in_w = 2;
        if (in_h < 2) in_h = 2;
        gfx_ellipse(dc, dx, dy, dw, dh, c, 1);   /* 白盘 */
        gfx_ellipse(dc, dx + (dw - in_w) / 2, dy + (dh - in_h) / 2,
                    in_w, in_h, RGB(0, 0, 0), 1);   /* 黑心 */
        return;
    }
    gfx_ellipse(dc, dx, dy, dw, dh, c, fill);
}
static HFONT make_font(int size, int bold) {
    const wchar_t* names[] = { L"MS Gothic", L"SimHei", L"SimSun", L"Microsoft YaHei" };
    HFONT f = NULL;
    int i;
    for (i = 0; i < 4 && !f; i++) {
        f = CreateFontW(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0,
                        DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, names[i]);
    }
    return f;
}
static void text_center(HDC dc, int x, int y, int w, int h, const wchar_t* s, int size, COLORREF c, int bold) {
    HFONT f = make_font(SX(size), bold);
    HGDIOBJ of = SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    RECT r = { SX(x), SX(y), SX(x + w), SX(y + h) };
    DrawTextW(dc, s, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
    DeleteObject(f);
}
/* 带大圆点的文本：前段 + 大圆点 + 后段，整体居中 */
static void text_with_dot(HDC dc, int x, int y, int w, int h,
                          const wchar_t* pre, const wchar_t* suf, int size, COLORREF c) {
    int szd = SX(size);
    HFONT f = make_font(szd, 0);
    HGDIOBJ of = SelectObject(dc, f);
    SIZE sp, ss;
    int gap, total, cx, cy, dotd, half, plen, slen;
    RECT r1, r2;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    plen = (int)wcslen(pre); slen = (int)wcslen(suf);
    GetTextExtentPoint32W(dc, pre, plen, &sp);
    GetTextExtentPoint32W(dc, suf, slen, &ss);
    dotd = (int)(szd * 0.45);
    gap = (int)(szd * 1.1);                 /* 点+两侧对称间距 */
    total = sp.cx + gap + ss.cx;
    cx = SX(x) + (SX(w) - total) / 2;
    cy = SX(y) + SX(h) / 2;
    half = (gap - dotd) / 2;                /* 点严格居中于两段文字之间 */
    r1.left = cx; r1.top = SX(y); r1.right = cx + sp.cx; r1.bottom = SX(y + h);
    DrawTextW(dc, pre, -1, &r1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    gfx_ellipse(dc, cx + sp.cx + half, cy - dotd / 2, dotd, dotd, c, 1);
    r2.left = cx + sp.cx + gap; r2.top = SX(y); r2.right = cx + sp.cx + gap + ss.cx; r2.bottom = SX(y + h);
    DrawTextW(dc, suf, -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
    DeleteObject(f);
}

/* ================= 状态与接口 ================= */
void tcms_init(TCMS_State* s) {
    memset(s, 0, sizeof(*s));
    s->net_voltage  = 750;
    s->net_current  = 0;
    s->brake_level  = 5;
    s->speed        = 0;
    wcscpy(s->date, L"12月24日");
    wcscpy(s->time, L"09:22");
    s->eb_mode[1] = 2; s->eb_mode[3] = 2; s->eb_mode[4] = 2;  /* C3 C5 C6 制动(黄)显示 0 */
    s->bc_pressure[0]=204; s->bc_pressure[1]=228; s->bc_pressure[2]=193;
    s->bc_pressure[3]=224; s->bc_pressure[4]=228; s->bc_pressure[5]=204;
    s->siv[0] = 1; s->siv[5] = 1;                 /* C2 C7 显示 380V/50HZ */
    s->air_compressor[1] = 1;                     /* C3 绿灯（2车） */
    s->air_compressor[4] = 1;                     /* C6 绿灯 */
    s->fire_alarm[0] = 0; s->fire_alarm[3] = 0;   /* 火灾报警默认不点亮，接口可开 */
    s->bhb[0] = 1; s->bhb[1] = 1;
    s->blb[0] = 1; s->blb[1] = 1;
}
void tcms_set_net_voltage   (TCMS_State* s, int v) { s->net_voltage = v; }
void tcms_set_net_current   (TCMS_State* s, int a) { s->net_current = a; }
void tcms_set_traction_level(TCMS_State* s, int lv){ s->traction_level = lv; }
void tcms_set_brake_level   (TCMS_State* s, int lv){ s->brake_level = lv; }
void tcms_set_speed         (TCMS_State* s, int k) { s->speed = k; }
void tcms_set_datetime      (TCMS_State* s, const char* d, const char* t) {
    if (d) MultiByteToWideChar(CP_ACP, 0, d, -1, s->date, 31);
    if (t) MultiByteToWideChar(CP_ACP, 0, t, -1, s->time, 31);
}
void tcms_set_traction_eb   (TCMS_State* s, int car, int mode) { if (car>=0&&car<6) s->eb_mode[car] = mode; }
void tcms_set_traction_current(TCMS_State* s, int car, int amps){ if (car>=0&&car<6) s->traction_current[car] = amps; }
void tcms_set_wheel_solid(TCMS_State* s, int solid) { s->wheel_solid = solid ? 1 : 0; }
void tcms_set_direction  (TCMS_State* s, int dir)   { s->direction = dir; }
void tcms_set_split_mode (TCMS_State* s, int split) { s->split_mode = split ? 1 : 0; }
void tcms_set_emergency_brake(TCMS_State* s, int on) { s->emergency_brake = on ? 1 : 0; }
void tcms_set_bc_pressure   (TCMS_State* s, int car, int kpa){ if (car>=0&&car<6) s->bc_pressure[car] = kpa; }
void tcms_set_siv           (TCMS_State* s, int car, int on) { if (car>=0&&car<6) s->siv[car] = on ? 1 : 0; }
void tcms_set_air_compressor(TCMS_State* s, int car, int on) { if (car>=0&&car<6) s->air_compressor[car] = on ? 1 : 0; }
void tcms_set_fire_alarm    (TCMS_State* s, int car, int on) { if (car>=0&&car<6) s->fire_alarm[car] = on ? 1 : 0; }
void tcms_set_bhb           (TCMS_State* s, int i, int on) { if (i>=0&&i<2) s->bhb[i] = on ? 1 : 0; }
void tcms_set_blb           (TCMS_State* s, int i, int on) { if (i>=0&&i<2) s->blb[i] = on ? 1 : 0; }

/* ================= 渲染 ================= */
#define RGB_C(r,g,b) RGB(r,g,b)
static const COLORREF BLACK   = RGB(0,0,0);
static const COLORREF WHITE   = RGB(248,248,248);
static const COLORREF DARK    = RGB(104,88,72);
static const COLORREF ORANGE  = RGB(248,168,72);
static const COLORREF GREEN   = RGB(168,248,120);
static const COLORREF GREEN2  = RGB(120,248,120);
static const COLORREF GRAYL   = RGB(120,120,136);

static void draw_train(const TCMS_State* s, HDC dc) {
    POINT p1[] = { {476,103},{525,103},{535,128},{529,134},{525,129},{476,129},{476,121},{528,121},{523,107},{476,107} };
    POINT p2[] = { {178,107},{523,107},{528,121},{173,120} };
    POINT p3[] = { {176,103},{224,103},{224,107},{178,107},{173,121},{224,121},{224,128},{175,128},{172,134},{167,129} };
    POINT p4[] = { {106,101},{91,116},{106,131},{106,124},{144,124},{144,109},{106,109} };
    POINT p5[] = { {174,134},{527,134},{524,130},{175,129},{173,134} };
    poly_fill(dc, p1, 10, WHITE);
    poly_fill(dc, p2, 4,  RGB(248,72,72));
    poly_fill(dc, p3, 10, WHITE);
    /* 方向箭头 p4 (小车左侧, 2026-08-16 换向器联动):
       前=尖指左(当前样子); 后=镜像尖指右; 中立=不显示 */
    if (s->direction == 1) {
        poly_fill(dc, p4, 7, WHITE);
    } else if (s->direction == -1) {
        POINT p4r[7];
        for (int k = 0; k < 7; k++) {
            p4r[k].x = 235 - p4[k].x;   /* 以 x=117.5 镜像 */
            p4r[k].y = p4[k].y;
        }
        poly_fill(dc, p4r, 7, WHITE);
    }
    poly_fill(dc, p5, 5,  RGB(216,88,104));
    /* 速度线 */
    fill_rect(dc, 227,103,60,4, RGB(248,232,232));
    fill_rect(dc, 289,103,60,4, RGB(248,232,232));
    fill_rect(dc, 351,103,60,4, RGB(248,232,232));
    fill_rect(dc, 413,103,60,4, RGB(248,232,232));
    fill_rect(dc, 414,121,59,7, WHITE);
    fill_rect(dc, 352,121,59,7, WHITE);
    fill_rect(dc, 290,121,59,7, WHITE);
    fill_rect(dc, 227,121,59,7, WHITE);
    /* 车轮 (2026-08-16 用户需求修正):
       - 只有 2/4/5 动车车轮 (light1 4个 + light2 8个) 随工况变化:
         牵引/电制动 -> 实心白; 其他 -> 空心白边
       - 1/3/6 拖车车轮 (dark 12个) 恒空心白边
       - 2026-08-17 用户需求: 实心/空心切换**相邻两个为一组**随机错开 —
         同轴左右两轮同刻切换 (6 组, 每组一个随机时刻 0..~500ms),
         0.5s 内全部完成, 不整体同时变 */
    {
        static const int dark[]  = {176,186,205,215,289,299,331,341,478,488,508,518};
        static const int light1[]= {227,237,269,279};
        static const int light2[]= {351,361,393,403,413,423,455,465};
        static int g_wheel_init = 0;
        static int g_wheel_target = 0;      /* 目标状态 */
        static int64_t g_wheel_t0 = 0;      /* 目标状态变化时刻 */
        static int g_wheel_off[12];         /* 每轮随机切换时刻 (0..1199ms) */
        int64_t nowms = GetTickCount64();
        int i, solid;
        if (!g_wheel_init) {
            g_wheel_init = 1;
            srand((unsigned)nowms);
            g_wheel_target = s->wheel_solid;
            g_wheel_t0 = nowms;
        }
        if (s->wheel_solid != g_wheel_target) {
            /* 目标变化: 相邻两轮一组 (同轴) 生成 6 组随机时刻 —
               500ms/6 段, 每段 83ms 内随机一点, 再按组洗牌打乱顺序 */
            g_wheel_target = s->wheel_solid;
            g_wheel_t0 = nowms;
            for (i = 0; i < 6; i++) {
                int t = i * 83 + (rand() % 83);
                g_wheel_off[i * 2] = t;
                g_wheel_off[i * 2 + 1] = t;   /* 组内两轮同刻 */
            }
            for (i = 5; i > 0; i--) {
                int j = rand() % (i + 1);
                int t0 = g_wheel_off[i * 2], t1 = g_wheel_off[i * 2 + 1];
                g_wheel_off[i * 2] = g_wheel_off[j * 2];
                g_wheel_off[i * 2 + 1] = g_wheel_off[j * 2 + 1];
                g_wheel_off[j * 2] = t0;
                g_wheel_off[j * 2 + 1] = t1;
            }
        }
        for (i=0;i<12;i++) ellipse_d(dc, dark[i],132,8,8, WHITE, 0);   /* 拖车: 恒空心 */
        /* 动车 12 轮: 每个轮在自己的随机时刻翻到目标状态, 未到点的保持原状 */
        for (i=0;i<12;i++) {
            int64_t el = nowms - g_wheel_t0;
            solid = (el >= g_wheel_off[i]) ? g_wheel_target : (g_wheel_target ? 0 : 1);
            if (i < 4) ellipse_d(dc, light1[i], 132, 8, 8, WHITE, solid);
            else       ellipse_d(dc, light2[i-4], 132, 8, 8, WHITE, solid);
        }
    }
}

void tcms_screen_draw(const TCMS_State* s, HDC dc, int target_w, int target_h) {
    int i;
    wchar_t buf[64];
    g_sc = (float)target_w / (float)TCMS_W;
    /* 背景 */
    fill_rect(dc, 0, 0, TCMS_W, TCMS_H, BLACK);

    /* ---- 最外边框：一圈完整闭合线（顶y=4 底y=448 左x=3 右x=599，四角闭合） ---- */
    frame_rect(dc, 3, 4, 596, 444, WHITE, 1);
    /* 顶栏三块（上边与外框顶边同线，左右边并入外框） */
    frame_rect(dc, 3, 4, 227, 29, WHITE, 1);
    frame_rect(dc, 230, 4, 147, 29, WHITE, 1);   /* 与左右两块同高对齐 */
    frame_rect(dc, 377, 4, 222, 29, WHITE, 1);
    {
        wcscpy(buf, s->date);
        wcscat(buf, L"  ");
        wcscat(buf, s->time);
        text_center(dc, 377, 4, 221, 29, buf, 13, WHITE, 0);
        text_center(dc, 230, 4, 147, 29, L"车辆状态", 13, WHITE, 0);
    }

    /* ---- 标签：网压/网流/牵引 制动级位/速度 ---- */
    {
        const wchar_t* l1 = L"网压"; const wchar_t* l2 = L"网流";
        const wchar_t* l4 = L"速度";
        /* 四个标签统一一条线（y中心=42，略上移对齐） */
        text_center(dc, 40, 35, 36, 14, l1, 13, WHITE, 0);
        text_center(dc, 147, 35, 35, 14, l2, 13, WHITE, 0);
        text_with_dot(dc, 368, 35, 96, 14, L"牵引", L"制动级位", 13, WHITE);
        text_center(dc, 534, 35, 31, 14, l4, 13, WHITE, 0);
    }

    /* ---- 指示灯：网压/网流/级位/速度 数值框（白粗边框） ---- */
    {
        COLORREF bg;
        /* 网压：正常绿 / +200黄 / 过高红 */
        if (s->net_voltage > 1100) bg = RGB(248,72,72);
        else if (s->net_voltage > 950) bg = RGB(248,200,104);
        else bg = RGB(152,248,120);
        fill_rect(dc, 24, 52, 71, 27, bg);
        frame_rect(dc, 24, 52, 71, 27, WHITE, 2);
        wsprintfW(buf, L"%d", s->net_voltage);
        text_center(dc, 24, 52, 71, 27, buf, 13, RGB(248,248,248), 0);
        /* 网流 0 */
        frame_rect(dc, 113, 52, 105, 29, WHITE, 2);
        wsprintfW(buf, L"%d", s->net_current);
        text_center(dc, 113, 52, 105, 29, buf, 13, WHITE, 0);
        /* 级位 (2026-08-16 用户修正):
           紧急制动 EB 或 制动9级 -> 红底白字"紧急制动"
           制动1-8=黄底 / 牵引1-5=绿底 / 手柄0=无底色只显示 0 */
        if (s->emergency_brake || s->brake_level >= 9) {
            bg = RGB(248,72,72);
            fill_rect(dc, 352, 52, 133, 26, bg);
            frame_rect(dc, 352, 52, 133, 26, WHITE, 2);
            text_center(dc, 352, 52, 133, 26, L"紧急制动", 13, RGB(248,248,248), 0);
        } else if (s->brake_level > 0) {
            bg = RGB(248,200,104);
            fill_rect(dc, 352, 52, 133, 26, bg);
            frame_rect(dc, 352, 52, 133, 26, WHITE, 2);
            wsprintfW(buf, L"制动%d级", s->brake_level);
            text_center(dc, 352, 52, 133, 26, buf, 13, RGB(246,246,245), 0);
        } else if (s->traction_level > 0) {
            bg = RGB(152,248,120);
            fill_rect(dc, 352, 52, 133, 26, bg);
            frame_rect(dc, 352, 52, 133, 26, WHITE, 2);
            wsprintfW(buf, L"牵引%d级", s->traction_level);
            text_center(dc, 352, 52, 133, 26, buf, 13, RGB(246,246,245), 0);
        } else {
            /* 手柄 0: 无底色, 只画框和 0 */
            frame_rect(dc, 352, 52, 133, 26, WHITE, 2);
            text_center(dc, 352, 52, 133, 26, L"0", 13, WHITE, 0);
        }
        /* 速度 0 */
        frame_rect(dc, 505, 52, 89, 26, WHITE, 2);
        wsprintfW(buf, L"%d", s->speed);
        text_center(dc, 505, 52, 89, 26, buf, 13, WHITE, 0);
    }

    /* ---- 小车 ---- */
    draw_train(s, dc);

    /* ---- 表格 ---- */
    {
        static const int cols[] = {68,164,226,288,350,412,475,539};
        static const int rows[] = {160,180,199,220,239,279,300,319,339,359,379};   /* 整体下移16 */
        static const wchar_t* hdr[10] = {
            L"车号", L"牵引 · 电制动", L"B C 压力", L"空压机运转", L"SIV",
            L"B H B", L"B L B", L"紧急短路", L"制动塞门", L"火灾报警" };
        int c, r;
        frame_rect(dc, 68, 160, 471, 219, WHITE, 2);
        for (c = 0; c < 8; c++) line(dc, cols[c], 160, cols[c], 379, WHITE, 1);
        for (r = 0; r < 11; r++) line(dc, 68, rows[r], 539, rows[r], WHITE, 1);
        /* 表头列(白框+白字13居中) */
        for (r = 0; r < 10; r++) {
            int y0 = rows[r], y1 = rows[r+1];
            frame_rect(dc, 68, y0, 96, y1 - y0, WHITE, 1);
            if (wcschr(hdr[r], L'·')) {
                wchar_t pre[32], suf[32];
                const wchar_t* dotp = wcschr(hdr[r], L'·');
                int plen = (int)(dotp - hdr[r]);
                wcsncpy(pre, hdr[r], plen); pre[plen] = 0;
                wcscpy(suf, dotp + 1);
                /* 去掉点两侧空格，点严格居中 */
                if (plen > 0 && pre[plen - 1] == L' ') pre[--plen] = 0;
                if (suf[0] == L' ') wcscpy(suf, suf + 1);
                text_with_dot(dc, 68, y0, 96, y1 - y0, pre, suf, 13, WHITE);
            } else {
                text_center(dc, 68, y0, 96, y1 - y0, hdr[r], 13, WHITE, 0);
            }
        }
        /* 车号 1..6 */
        for (c = 0; c < 6; c++) {
            wsprintfW(buf, L"%d", c + 1);
            text_center(dc, cols[c+1], 160, cols[c+2]-cols[c+1], 20, buf, 13, WHITE, 0);
        }
        /* R2 牵引 电制动 (2026-08-16): 牵引=绿 制动=黄 紧急=红; 格内显示电流值 */
        {
            int car;
            for (car = 0; car < 6; car++) {
                if (s->eb_mode[car] > 0) {
                    COLORREF ec;
                    if (s->eb_mode[car] == 1) ec = RGB(152,248,120);
                    else if (s->eb_mode[car] == 2) ec = RGB(248,200,104);
                    else ec = RGB(248,72,72);
                    fill_rect(dc, cols[car+1], 180, cols[car+2]-cols[car+1], 19, ec);
                    frame_rect(dc, cols[car+1], 180, cols[car+2]-cols[car+1], 19, WHITE, 1);
                    wsprintfW(buf, L"%d", s->traction_current[car]);
                    text_center(dc, cols[car+1], 180, cols[car+2]-cols[car+1], 19, buf, 13,
                                s->eb_mode[car] == 3 ? WHITE : RGB(0,0,0), 0);
                }
            }
        }
        /* R3 BC压力 */
        for (c = 0; c < 6; c++) {
            wsprintfW(buf, L"%d", s->bc_pressure[c]);
            text_center(dc, cols[c+1], 199, cols[c+2]-cols[c+1], 21, buf, 13, WHITE, 0);
        }
        /* R4 空压机运转: 每节车厢绿灯（默认 2车C3 与 C6） */
        for (i = 0; i < 6; i++) {
            if (s->air_compressor[i]) {
                fill_rect(dc, cols[i+1], 220, cols[i+2]-cols[i+1], 19, GREEN);
                frame_rect(dc, cols[i+1], 220, cols[i+2]-cols[i+1], 19, WHITE, 1);
            }
        }
        /* R5 SIV: siv[0]/siv[5] 两行 380V/50HZ */
        for (i = 0; i < 6; i++) {
            if (s->siv[i]) {
                int x = cols[i+1], w = cols[i+2]-cols[i+1];
                text_center(dc, x, 239, w, 20, L"380V", 13, WHITE, 0);
                text_center(dc, x, 259, w, 20, L"50HZ", 13, WHITE, 0);
            }
        }
        /* R6/R7 BHB BLB 小片指示灯(由 bhb/blb 控制) */
        {
            int chips[4][5] = {
                {353,282,24,15, 0}, {385,282,24,15, 0},   /* BHB */
                {353,302,24,15, 1}, {385,302,24,15, 1}    /* BLB */
            };
            int ons[4] = { s->bhb[0], s->bhb[1], s->blb[0], s->blb[1] };
            for (i = 0; i < 4; i++) {
                if (ons[i]) {
                    if (chips[i][4] == 0) {
                        fill_rect(dc, chips[i][0], chips[i][1], chips[i][2], chips[i][3], RGB(136,248,120));
                    } else {
                        frame_rect(dc, chips[i][0], chips[i][1], chips[i][2], chips[i][3], RGB(136,248,120), 1);
                    }
                    wsprintfW(buf, L"%d", (i % 2) + 1);
                    text_center(dc, chips[i][0], chips[i][1], chips[i][2], chips[i][3], buf, 9,
                                chips[i][4] ? WHITE : RGB(0,0,0), 0);
                }
            }
        }
        /* R10 火灾报警: fire_alarm[0]/[3] 灰灯 */
        {
            static const int fa[] = {0, 3};
            for (i = 0; i < 2; i++) {
                int car = fa[i];
                if (s->fire_alarm[car]) {
                    fill_rect(dc, cols[car+1], 359, cols[car+2]-cols[car+1], 21, GRAYL);
                    frame_rect(dc, cols[car+1], 359, cols[car+2]-cols[car+1], 21, WHITE, 1);
                }
            }
        }
    }

    /* ---- 底部：分隔线(外框底边已画) + 按钮 ---- */
    line(dc, 4, 409, 599, 409, WHITE, 1);
    {
        struct { int x, y, w, h; const wchar_t* t; int sz; } btns[6] = {
            { 493, 419, 67, 20, s->split_mode ? L"恢复" : L"分屏", 11 },   /* 2026-08-16: 分屏/恢复按钮 */
            { 404, 418, 67, 21, L"检修",     11 },
            { 316, 419, 65, 20, L"运行",     11 },   /* 2026-08-17: 字号统一 11 */
            { 224, 418, 69, 22, L"故障",     11 },
            { 135, 419, 68, 20, L"空调设备", 11 },
            { 45,  420, 69, 19, L"主菜单",   11 },   /* 2026-08-17: 字号统一 11 */
        };
        for (i = 0; i < 6; i++) {
            fill_rect(dc, btns[i].x, btns[i].y, btns[i].w, btns[i].h, WHITE);
            text_center(dc, btns[i].x, btns[i].y, btns[i].w, btns[i].h, btns[i].t, btns[i].sz, RGB(6,10,14), 0);
        }
    }
    g_sc = 1.0f;
}

/* ================= BMP 保存 ================= */
int tcms_screen_save_bmp(const TCMS_State* s, const char* path) {
    HDC mdc = CreateCompatibleDC(NULL);
    BITMAPINFO bi;
    void* bits = NULL;
    HBITMAP bmp;
    FILE* f;
    BITMAPFILEHEADER bfh;
    BITMAPINFOHEADER* bih;
    int ok = 0;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = TCMS_W;
    bi.bmiHeader.biHeight = TCMS_H;    /* bottom-up 标准BMP */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;
    bmp = CreateDIBSection(mdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (bmp) {
        HGDIOBJ ob = SelectObject(mdc, bmp);
        tcms_screen_draw(s, mdc, TCMS_W, TCMS_H);
        f = fopen(path, "wb");
        if (f) {
            int rowstride = ((TCMS_W * 3 + 3) & ~3);
            bfh.bfType = 0x4D42;
            bfh.bfSize = sizeof(bfh) + sizeof(BITMAPINFOHEADER) + rowstride * TCMS_H;
            bfh.bfReserved1 = bfh.bfReserved2 = 0;
            bfh.bfOffBits = sizeof(bfh) + sizeof(BITMAPINFOHEADER);
            fwrite(&bfh, 1, sizeof(bfh), f);
            bih = &bi.bmiHeader;
            bih->biSizeImage = rowstride * TCMS_H;
            fwrite(bih, 1, sizeof(BITMAPINFOHEADER), f);
            fwrite(bits, 1, rowstride * TCMS_H, f);
            fclose(f);
            ok = 1;
        }
        SelectObject(mdc, ob);
        DeleteObject(bmp);
    }
    DeleteDC(mdc);
    return ok;
}

/* ================= 启动屏 (2026-08-16 用户需求: TCMS 与 DMI 同款开机流程) =================
 * DOS 自检 -> 待机时钟 -> 主界面; 阶段推进由 hud_boot 状态机驱动 (tcms 只负责画).
 * 行数/节拍与 DMI 的 DOS 屏一致 (10 行, 380ms/行), lines_shown 直接复用 DMI 计数. */
#define TCMS_DOS_LINES 10
#define TCMS_DOS_Y0    36
#define TCMS_DOS_DY    30
#define TCMS_DOS_SIZE  15

static void tcms_build_dos_lines(wchar_t lines[TCMS_DOS_LINES][160])
{
    /* 真实 CPU 型号 (与 DMI 相同来源: 注册表) */
    wchar_t cpu[140] = L"Unknown CPU";
    HKEY hk = 0;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                      0, KEY_READ, &hk) == ERROR_SUCCESS) {
        char buf[256];
        DWORD sz = sizeof(buf), type = 0;
        if (RegQueryValueExA(hk, "ProcessorNameString", NULL, &type,
                             (LPBYTE)buf, &sz) == ERROR_SUCCESS && sz > 1) {
            buf[sz - 1] = '\0';
            MultiByteToWideChar(CP_ACP, 0, buf, -1, cpu, 140);
        }
        RegCloseKey(hk);
    }
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    swprintf(lines[0], 160, L"TCMS 车辆状态屏 BIOS v1.2");
    swprintf(lines[1], 160, L"Copyright (C) 2026 OpenETCS");
    swprintf(lines[2], 160, L"CPU  : %ls", cpu);
    swprintf(lines[3], 160, L"MEM  : %.0fM OK", (double)ms.ullTotalPhys / (1024.0 * 1024.0));
    swprintf(lines[4], 160, L"列车总线 ............ OK");
    swprintf(lines[5], 160, L"制动单元 ............ OK");
    swprintf(lines[6], 160, L"辅助电源 ............ OK");
    swprintf(lines[7], 160, L"门控系统 ............ OK");
    swprintf(lines[8], 160, L"");
    swprintf(lines[9], 160, L"Starting system...");
}

void tcms_screen_draw_dos(HDC dc, int lines_shown, int target_w, int target_h)
{
    static wchar_t lines[TCMS_DOS_LINES][160];
    static int built = 0;
    int i;
    if (!built) {
        tcms_build_dos_lines(lines);
        built = 1;
    }
    /* 2026-08-17: 内部坐标系随目标尺寸等比放大 (与主界面同规则, 元素零漂移) */
    g_sc = (float)target_w / (float)TCMS_W;
    fill_rect(dc, 0, 0, TCMS_W, TCMS_H, BLACK);
    if (lines_shown > TCMS_DOS_LINES) lines_shown = TCMS_DOS_LINES;
    for (i = 0; i < lines_shown; i++)
        text_center(dc, 40, TCMS_DOS_Y0 + i * TCMS_DOS_DY,
                    TCMS_W - 80, TCMS_DOS_DY, lines[i], TCMS_DOS_SIZE, WHITE, 0);
    /* 闪烁光标: 末行文字行尾, 500ms 周期 (与 DMI 一致) */
    if (lines_shown > 0 && (GetTickCount64() / 500) % 2 == 0) {
        int tw = 0, th = 0, cxx;
        gfx_text_extent(dc, lines[lines_shown - 1], SX(TCMS_DOS_SIZE), 0, &tw, &th);
        cxx = 40 + tw + 6;
        if (cxx > TCMS_W - 20) cxx = TCMS_W - 20;
        fill_rect(dc, cxx, TCMS_DOS_Y0 + (lines_shown - 1) * TCMS_DOS_DY + 7,
                  10, TCMS_DOS_SIZE, WHITE);
    }
}

void tcms_screen_draw_clock(HDC dc, const wchar_t* hhmmss, const wchar_t* status,
                            int target_w, int target_h)
{
    const wchar_t* s = hhmmss ? hhmmss : L"--:--:--";
    int cw = 0, ch = 0, sw = 0, sh = 0, total, y0;
    /* 2026-08-17: 内部坐标系随目标尺寸等比放大 (与主界面同规则, 元素零漂移) */
    g_sc = (float)target_w / (float)TCMS_W;
    fill_rect(dc, 0, 0, TCMS_W, TCMS_H, BLACK);
    /* 时钟 + 状态字: 字号放大, 整体垂直居中 (2026-08-17 用户需求) */
    gfx_text_extent(dc, s, SX(64), 1, &cw, &ch);
    if (status && status[0])
        gfx_text_extent(dc, status, SX(16), 0, &sw, &sh);
    total = ch + (sh ? 12 + sh : 0);
    y0 = (TCMS_H - total) / 2;
    if (y0 < 16) y0 = 16;
    text_center(dc, 0, y0, TCMS_W, ch, s, 64, WHITE, 1);
    if (status && status[0])
        text_center(dc, 0, y0 + ch + 12, TCMS_W, sh, status, 16, WHITE, 0);
    /* 左下角闪烁小白方块: 500ms 周期, 表示程序未卡死 (与 DMI 一致) */
    if ((GetTickCount64() / 500) % 2 == 0)
        fill_rect(dc, 12, TCMS_H - 26, 12, 12, WHITE);
}
