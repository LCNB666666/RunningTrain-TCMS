/* TCMS 状态屏演示程序
 * 用法：
 *   启动后每 100ms 自动更新 网压/网流/级位/速度/日期时间/BC压力/电制动 等，
 *   演示接口调用方式。接入实际数据源时替换 timer 回调即可。
 *   按键：F2 保存 tcms_screen.bmp，Esc 退出
 */
#include <windows.h>
#include <math.h>
#include "tcms_screen.h"

/* 界面放大倍数：普通版=1.0（默认）；HD版编译时加 -DSCALE=1.25f */
#ifndef SCALE
#define SCALE 1.0f
#endif
#define TW (int)(TCMS_W * SCALE)          /* 目标宽 */
#define TH (int)(TCMS_H * SCALE)          /* 目标高 */

static TCMS_State g_state;
static TCMS_State g_last;
static HWND g_hwnd;
static int g_frozen = 1;   /* 默认冻结：静态画面，空格开始动画 */

static void update_demo(void) {
    static DWORD t0 = 0;
    DWORD t = GetTickCount();
    SYSTEMTIME st;
    char date[32], time[32];
    if (!t0) t0 = t;
    if (g_frozen) return;
    {
        double x = (t - t0) / 1000.0;
        tcms_set_net_voltage(&g_state, 650 + (int)(500 * (0.5 + 0.5 * sin(x * 0.12))));
        tcms_set_net_current(&g_state, (int)(80 + 80 * sin(x * 0.4)));
        tcms_set_speed(&g_state, (int)fabs(55 * sin(x * 0.15)));
        tcms_set_traction_level(&g_state, (int)((sin(x * 0.3) + 1) * 2.5));
        tcms_set_brake_level(&g_state, (int)((sin(x * 0.2) + 1) * 4));
        tcms_set_bc_pressure(&g_state, 0, 200 + (int)(25 * sin(x * 0.5)));
        tcms_set_bc_pressure(&g_state, 1, 210 + (int)(20 * sin(x * 0.6)));
        tcms_set_bc_pressure(&g_state, 2, 190 + (int)(18 * sin(x * 0.4)));
        tcms_set_bc_pressure(&g_state, 3, 220 + (int)(18 * sin(x * 0.7)));
        tcms_set_bc_pressure(&g_state, 4, 215 + (int)(15 * sin(x * 0.3)));
        tcms_set_bc_pressure(&g_state, 5, 205 + (int)(18 * sin(x * 0.8)));
        tcms_set_traction_eb(&g_state, 1, 1 + ((int)(x / 4)) % 3);
        tcms_set_traction_eb(&g_state, 3, 1 + ((int)(x / 5)) % 3);
        tcms_set_traction_eb(&g_state, 4, 1 + ((int)(x / 6)) % 3);
        tcms_set_emergency_brake(&g_state, ((int)(x / 9)) % 2);
        tcms_set_siv(&g_state, 0, ((int)(x / 7)) % 2);
    }
    GetLocalTime(&st);
    {
        wchar_t wdate[32], wtime[32];
        wsprintfW(wdate, L"%d月%d日", st.wMonth, st.wDay);
        wsprintfW(wtime, L"%02d:%02d", st.wHour, st.wMinute);
        WideCharToMultiByte(CP_ACP, 0, wdate, -1, date, 32, NULL, NULL);
        WideCharToMultiByte(CP_ACP, 0, wtime, -1, time, 32, NULL, NULL);
    }
    tcms_set_datetime(&g_state, date, time);
    /* 数值有变化才重绘，避免无谓闪烁 */
    if (memcmp(&g_state, &g_last, sizeof(g_state)) != 0) {
        g_last = g_state;
        InvalidateRect(g_hwnd, NULL, FALSE);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, 1, 1000, NULL);
        return 0;
    case WM_TIMER:
        update_demo();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        {
            /* 客户区先全黑（含边缘10px余量），再贴 24位DIB 画面 */
            RECT cr;
            HBRUSH bb;
            GetClientRect(hwnd, &cr);
            bb = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(dc, &cr, bb);
            DeleteObject(bb);
            {
                HDC mdc = CreateCompatibleDC(dc);
                BITMAPINFO bi;
                void* bits = NULL;
                HBITMAP bmp;
                memset(&bi, 0, sizeof(bi));
                bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bi.bmiHeader.biWidth = TW;
                bi.bmiHeader.biHeight = TH;   /* 24bpp bottom-up */
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 24;
                bi.bmiHeader.biCompression = BI_RGB;
                bmp = CreateDIBSection(mdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
                SelectObject(mdc, bmp);
                tcms_screen_draw(&g_state, mdc, TW, TH);
                BitBlt(dc, 0, 0, TW, TH, mdc, 0, 0, SRCCOPY);
                SelectObject(mdc, bmp);
                DeleteObject(bmp);
                DeleteDC(mdc);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;   /* 不擦背景，避免白闪 */
    case WM_PRINT:
        /* 支持 PrintWindow 抓图：画到传入的DC */
        {
            HDC mdc = CreateCompatibleDC((HDC)w);
            BITMAPINFO bi;
            void* bits = NULL;
            HBITMAP bmp;
            memset(&bi, 0, sizeof(bi));
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = TCMS_W;
            bi.bmiHeader.biHeight = TCMS_H;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 24;
            bi.bmiHeader.biCompression = BI_RGB;
            memset(&bi, 0, sizeof(bi));
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = TW;
            bi.bmiHeader.biHeight = TH;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 24;
            bi.bmiHeader.biCompression = BI_RGB;
            bmp = CreateDIBSection(mdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
            SelectObject(mdc, bmp);
            tcms_screen_draw(&g_state, mdc, TW, TH);
            BitBlt((HDC)w, 0, 0, TW, TH, mdc, 0, 0, SRCCOPY);
            SelectObject(mdc, bmp);
            DeleteObject(bmp);
            DeleteDC(mdc);
            return 0;
        }
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) { DestroyWindow(hwnd); return 0; }
        if (w == VK_F2) {
            if (tcms_screen_save_bmp(&g_state, "tcms_screen.bmp"))
                MessageBoxA(hwnd, "已保存 tcms_screen.bmp", "TCMS", MB_OK);
        }
        if (w == VK_SPACE) g_frozen = !g_frozen;
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, w, l);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int nShow) {
    WNDCLASSW wc;
    HWND hwnd;
    MSG msg;
    tcms_init(&g_state);
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"TcmsScreenDemo";
    RegisterClassW(&wc);
    /* 固定尺寸窗口，禁止拉伸（避免白色区域） */
    hwnd = CreateWindowW(L"TcmsScreenDemo", L"TCMS 车辆状态屏 (C语言接口演示)",
                         WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         TW + 16, TH + 39, NULL, NULL, hInst, NULL);
    g_hwnd = hwnd;
    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);
    tcms_screen_save_bmp(&g_state, "tcms_screen.bmp");
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
