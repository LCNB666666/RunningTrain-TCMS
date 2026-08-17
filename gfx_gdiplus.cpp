/* GDI+ 抗锯齿渲染桥接层：文字/多边形/圆 平滑，供 C 代码调用 */
#include <windows.h>
#include <gdiplus.h>
using namespace Gdiplus;

static ULONG_PTR g_token = 0;
static int g_ready = 0;

static void ensure_init(void) {
    if (g_ready) return;
    GdiplusStartupInput si;
    si.GdiplusVersion = 1;
    si.DebugEventCallback = NULL;
    si.SuppressBackgroundThread = FALSE;
    si.SuppressExternalCodecs = FALSE;
    if (GdiplusStartup(&g_token, &si, NULL) == Ok) g_ready = 1;
}

static const wchar_t* pick_family(void) {
    static const wchar_t* fams[] = { L"MS Gothic", L"SimHei", L"SimSun", L"Microsoft YaHei" };
    int i;
    for (i = 0; i < 4; i++) {
        FontFamily ff(fams[i]);
        if (ff.GetLastStatus() == Ok) return fams[i];
    }
    return L"";
}

static Color mkcolor(COLORREF c) {
    return Color(255, GetRValue(c), GetGValue(c), GetBValue(c));
}

extern "C" {

void gfx_poly(HDC dc, const POINT* pts, int n, COLORREF c) {
    ensure_init();
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias8x8);
    SolidBrush br(mkcolor(c));
    g.FillPolygon(&br, (const Point*)pts, n);
}

void gfx_ellipse(HDC dc, int x, int y, int w, int h, COLORREF c, int fill) {
    ensure_init();
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias8x8);
    if (fill) {
        SolidBrush br(mkcolor(c));
        g.FillEllipse(&br, x, y, w, h);
    } else {
        Pen pn(mkcolor(c), 1.6f);   /* 空心圆稍粗，白边线更明显 */
        REAL ins = 1.0f;            /* 向内缩，使外缘与实心圆视觉一致 */
        g.DrawEllipse(&pn, (REAL)x + ins, (REAL)y + ins, (REAL)w - 2 * ins, (REAL)h - 2 * ins);
    }
}

void gfx_text_extent(HDC dc, const wchar_t* s, int size, int bold, int* pw, int* ph) {
    ensure_init();
    Graphics g(dc);
    FontFamily fam(pick_family());
    Font font(&fam, (REAL)size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    RectF bound;
    g.MeasureString(s, -1, &font, PointF(0, 0), &bound);
    *pw = (int)(bound.Width + 0.5f);
    *ph = (int)(bound.Height + 0.5f);
}

/* left: 0=水平居中 1=左对齐；垂直均居中 */
void gfx_text(HDC dc, int x, int y, int w, int h, const wchar_t* s, int size, COLORREF c, int bold, int left) {
    ensure_init();
    Graphics g(dc);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    FontFamily fam(pick_family());
    Font font(&fam, (REAL)size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
    SolidBrush br(mkcolor(c));
    StringFormat fmt;
    fmt.SetAlignment(left ? StringAlignmentNear : StringAlignmentCenter);
    fmt.SetLineAlignment(StringAlignmentCenter);
    fmt.SetFormatFlags(StringFormatFlagsNoWrap | StringFormatFlagsNoClip);
    RectF rc((REAL)x, (REAL)y, (REAL)w, (REAL)h);
    g.DrawString(s, -1, &font, rc, &fmt, &br);
}

} /* extern "C" */
