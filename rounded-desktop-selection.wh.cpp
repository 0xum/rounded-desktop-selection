// ==WindhawkMod==
// @id              rounded-desktop-selection
// @name            Rounded Desktop & Explorer Selection
// @name:pt         Selecao arredondada do desktop e Explorador
// @description     Rounded translucent drag selection, Explorer item and header highlights
// @version         0.2.6
// @author          delta
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lgdiplus -lcomctl32 -lgdi32 -luxtheme -lmsimg32
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Rounded Desktop Selection — MVP

Styles desktop and File Explorer drag selection, item and column-header highlights.
Windows retains its native rectangular hit testing and selection behavior.
The navigation tree keeps its existing styling.

Defaults: #E81123, 20% fill opacity, 80% border opacity, 8 DIP radius,
1 DIP border. Settings work with Windhawk 1.7.3.

Disable the mod to restore native drawing. Each feature keeps native drawing
when its required Windows symbols or exports cannot be resolved.
Do not combine with other mods that replace desktop marquee painting.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- Color: "#E81123"
  $name: Color (#RRGGBB)
- FillOpacity: 20
  $name: Fill opacity (0-100%)
- BorderOpacity: 80
  $name: Border opacity (0-100%)
- CornerRadius: 8
  $name: Corner radius (DIP, 0-64)
- BorderWidth: 1
  $name: Border width (DIP, 0-8)
- ExplorerHighlights: true
  $name: Explorer item and header highlights
- ExplorerMarquee: true
  $name: Explorer drag-selection rectangle
*/
// ==/WindhawkModSettings==

#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <gdiplus.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cwchar>
#ifndef MVP_RENDER_TEST
#include <windhawk_utils.h>
#endif

struct Appearance {
    DWORD rgb = 0xE81123;
    int fillOpacity = 20;
    int borderOpacity = 80;
    int radius = 8;
    int borderWidth = 1;
};

static bool ParseRgb(const wchar_t* text, DWORD& rgb) {
    if (!text) return false;
    if (*text == L'#') ++text;
    if (wcslen(text) != 6) return false;
    DWORD result = 0;
    for (int i = 0; i < 6; ++i) {
        wchar_t c = text[i];
        int digit = c >= L'0' && c <= L'9' ? c - L'0' :
                    c >= L'a' && c <= L'f' ? c - L'a' + 10 :
                    c >= L'A' && c <= L'F' ? c - L'A' + 10 : -1;
        if (digit < 0) return false;
        result = (result << 4) | digit;
    }
    rgb = result;
    return true;
}

static Gdiplus::Color MakeColor(DWORD rgb, int opacity) {
    BYTE alpha = static_cast<BYTE>((std::clamp(opacity, 0, 100) * 255 + 50) / 100);
    return Gdiplus::Color(alpha, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

// Both the live hook and the rendering tests use this exact drawing function.
static bool DrawMarquee(Gdiplus::Graphics& graphics, const RECT& rect,
                        const Appearance& style, float dpiScale) {
    float width = static_cast<float>(rect.right - rect.left);
    float height = static_cast<float>(rect.bottom - rect.top);
    if (width <= 0 || height <= 0) return true;
    float penWidth = std::min(std::clamp(style.borderWidth, 0, 8) * dpiScale,
                              std::min(width, height));
    float inset = penWidth / 2.0f;
    float x = rect.left + inset, y = rect.top + inset;
    float w = width - penWidth, h = height - penWidth;
    // Very thin drag rectangles must remain valid even with a wide border.
    if (w <= 0 || h <= 0) {
        Gdiplus::SolidBrush brush(MakeColor(style.rgb,
            std::max(style.fillOpacity, style.borderOpacity)));
        return graphics.FillRectangle(&brush, static_cast<float>(rect.left),
            static_cast<float>(rect.top), width, height) == Gdiplus::Ok;
    }
    float radius = std::min(std::clamp(style.radius, 0, 64) * dpiScale,
                            std::min(w, h) / 2.0f);
    Gdiplus::GraphicsPath path;
    if (radius > 0) {
        float diameter = radius * 2;
        path.AddArc(x, y, diameter, diameter, 180, 90);
        path.AddArc(x + w - diameter, y, diameter, diameter, 270, 90);
        path.AddArc(x + w - diameter, y + h - diameter, diameter, diameter, 0, 90);
        path.AddArc(x, y + h - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();
    } else {
        path.AddRectangle(Gdiplus::RectF(x, y, w, h));
    }
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    Gdiplus::SolidBrush fill(MakeColor(style.rgb, style.fillOpacity));
    if (graphics.FillPath(&fill, &path) != Gdiplus::Ok) return false;
    if (penWidth > 0 && style.borderOpacity > 0) {
        Gdiplus::Pen border(MakeColor(style.rgb, style.borderOpacity), penWidth);
        if (graphics.DrawPath(&border, &path) != Gdiplus::Ok) return false;
    }
    return true;
}

static bool DrawClippedMarquee(Gdiplus::Graphics& graphics, const RECT& selection,
                               const RECT& repaint, const Appearance& style,
                               float dpiScale) {
    auto state = graphics.Save();
    graphics.SetClip(Gdiplus::Rect(repaint.left, repaint.top,
        std::max(0L, repaint.right - repaint.left),
        std::max(0L, repaint.bottom - repaint.top)), Gdiplus::CombineModeIntersect);
    bool result = DrawMarquee(graphics, selection, style, dpiScale);
    graphics.Restore(state);
    return result;
}

enum class Highlight { None, Item, Header, ViewButton, NativeFocus };
static Highlight ClassifyHighlight(const wchar_t* theme, int part, int state) {
    // ItemsView has a separate themed rectangular focus overlay. The item
    // background already draws our rounded outline; preserve all other parts.
    if (_wcsicmp(theme, L"ItemsView") == 0 && part == 3 && state == 1)
        return Highlight::NativeFocus;
    // Explorer's pending selection while holding the mouse is a separate
    // ItemsView overlay, rather than a ListView selected-item background.
    if (_wcsicmp(theme, L"ItemsView") == 0 && part == 3 && state == 2)
        return Highlight::Item;
    if (_wcsicmp(theme, L"ListView") == 0 && part == LVP_LISTITEM &&
        (state == LISS_HOT || state == LISS_SELECTED ||
         state == LISS_SELECTEDNOTFOCUS || state == LISS_HOTSELECTED))
        return Highlight::Item;
    if (_wcsicmp(theme, L"Header") == 0 &&
        (part == HP_HEADERITEM || part == HP_HEADERITEMLEFT || part == HP_HEADERITEMRIGHT) &&
        state >= HIS_NORMAL && state <= HIS_ICONSORTEDPRESSED && state % 3 != 1)
        return Highlight::Header;
    if (_wcsicmp(theme, L"Toolbar") == 0 && part == TP_BUTTON &&
        (state == TS_HOT || state == TS_PRESSED || state == TS_CHECKED || state == TS_HOTCHECKED))
        return Highlight::ViewButton;
    if (_wcsicmp(theme, L"Button") == 0 && part == BP_PUSHBUTTON &&
        (state == PBS_HOT || state == PBS_PRESSED || state == PBS_DEFAULTED))
        return Highlight::ViewButton;
    return Highlight::None;
}

// Render into a premultiplied DIB, then source-over both color AND alpha.
// Direct drawing with GDI+ on a composited HDC can flatten transparency.
static bool BlendMarquee(HDC destination, const RECT& shape, const RECT* clip,
                          const Appearance& style, float scale) {
    RECT visible = shape;
    if (clip && !IntersectRect(&visible, &visible, clip)) return true;
    RECT dcClip{};
    int clipType = GetClipBox(destination, &dcClip);
    if (clipType == NULLREGION) return true;
    if (clipType != ERROR && !IntersectRect(&visible, &visible, &dcClip)) return true;
    int width = visible.right - visible.left, height = visible.bottom - visible.top;
    if (width <= 0 || height <= 0) return true;
    if (width > 32768 || height > 32768 || static_cast<int64_t>(width)*height > 33554432)
        return false;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HDC memory = CreateCompatibleDC(destination);
    if (!memory) return false;
    HBITMAP dib = CreateDIBSection(destination, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!dib) { DeleteDC(memory); return false; }
    HGDIOBJ previous = SelectObject(memory, dib);
    bool success = false;
    {
        Gdiplus::Bitmap image(width, height, width*4, PixelFormat32bppPARGB,
                              static_cast<BYTE*>(pixels));
        Gdiplus::Graphics graphics(&image);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        graphics.TranslateTransform(static_cast<float>(-visible.left),
                                    static_cast<float>(-visible.top));
        success = DrawMarquee(graphics, shape, style, scale);
        graphics.Flush(Gdiplus::FlushIntentionSync);
        if (success) {
            BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
            success = AlphaBlend(destination, visible.left, visible.top, width, height,
                                 memory, 0, 0, width, height, blend) != FALSE;
        }
    }
    SelectObject(memory, previous);
    DeleteObject(dib);
    DeleteDC(memory);
    return success;
}

#ifndef MVP_RENDER_TEST
static std::atomic<DWORD> g_rgb{0xE81123};
static std::atomic<int> g_fill{20}, g_border{80}, g_radius{8}, g_width{1};
static std::atomic<bool> g_explorerHighlights{true}, g_explorerMarquee{true};
static bool g_highlightsReady = false, g_marqueeReady = false;
static ULONG_PTR g_gdiplusToken = 0;
static HMODULE g_controls = nullptr;
static thread_local HWND g_currentList = nullptr;
static size_t g_ownerOffset = 0, g_managerOffset = 0, g_rectOffset = 0;
static std::atomic<bool> g_recordedPaint{false};
static std::atomic<bool> g_recordedItem{false}, g_recordedHeader{false}, g_recordedExplorerMarquee{false};
static std::atomic<bool> g_recordedFocus{false}, g_recordedView{false}, g_recordedBorder{false};
static HMODULE g_dui = nullptr, g_frame = nullptr;
static thread_local HWND g_paintWindows[16]{};
static thread_local unsigned g_paintDepth = 0;
static thread_local void* g_explorerSelector = nullptr;
static thread_local void* g_borderElement = nullptr;
using GetThemeClass_t = HRESULT(WINAPI*)(HTHEME, LPWSTR, int);
static GetThemeClass_t g_themeClass;
static decltype(&BeginPaint) BeginPaint_Original;
static decltype(&EndPaint) EndPaint_Original;
static decltype(&DrawThemeBackground) ThemeBackground_Original;
static decltype(&DrawThemeBackgroundEx) ThemeBackgroundEx_Original;
static decltype(&DrawFocusRect) DrawFocusRect_Original;
using StartSelector_t = void(__cdecl*)(void*, POINT, void*, int, int, int, void*);
using EndSelector_t = void(__cdecl*)(void*);
using ElementBackground_t = void(__cdecl*)(void*, HDC, void*, const RECT*, const RECT*, const RECT*, const RECT*);
using ElementBorder_t = void(__cdecl*)(void*, HDC, void*, RECT*, const RECT*);
using ElementFill_t = void(__cdecl*)(void*, HDC, ULONG, int, int, int, int, bool);
static StartSelector_t StartSelector_Original;
static EndSelector_t EndSelector_Original;
static ElementBackground_t ElementBackground_Original;
static ElementBorder_t ElementBorder_Original;
static ElementFill_t ElementFill_Original;
using ElementBool_t = bool(__cdecl*)(void*);
static ElementBool_t ElementSelected, ElementMouseWithin, ElementMouseFocused;

static Appearance CurrentAppearance() {
    return {g_rgb.load(), g_fill.load(), g_border.load(), g_radius.load(), g_width.load()};
}

static HWND PaintWindow(HDC dc) {
    HWND window = WindowFromDC(dc);
    if (!window && g_paintDepth) window = g_paintWindows[std::min(g_paintDepth, 16u)-1];
    return window;
}

static bool IsExplorerWindow(HWND window) {
    wchar_t name[64]{};
    if (!window || !GetClassNameW(GetAncestor(window, GA_ROOT), name, ARRAYSIZE(name))) return false;
    return wcscmp(name, L"CabinetWClass") == 0 || wcscmp(name, L"ExploreWClass") == 0;
}

static bool IsViewButtonArea(HWND window, const RECT& bounds) {
    for (HWND parent = window; parent; parent = GetParent(parent)) {
        wchar_t name[64]{};
        GetClassNameW(parent, name, ARRAYSIZE(name));
        if (wcscmp(name, L"msctls_statusbar32") == 0) return true;
    }
    // Some Explorer builds host these buttons directly in the frame.
    HWND root = GetAncestor(window, GA_ROOT);
    RECT client{};
    if (!root || !GetClientRect(root, &client)) return false;
    POINT point{(bounds.left+bounds.right)/2, (bounds.top+bounds.bottom)/2};
    MapWindowPoints(window, root, &point, 1);
    UINT dpi = GetDpiForWindow(window);
    int scale = dpi ? dpi : 96;
    return point.x >= client.right - MulDiv(128, scale, 96) &&
           point.y >= client.bottom - MulDiv(48, scale, 96);
}

static BOOL WINAPI DrawFocusRect_Hook(HDC dc, const RECT* bounds) {
    HWND window = PaintWindow(dc);
    if (g_explorerHighlights && bounds && IsExplorerWindow(window)) {
        wchar_t name[64]{};
        GetClassNameW(window, name, ARRAYSIZE(name));
        if (wcscmp(name, L"DirectUIHWND") == 0 || wcscmp(name, L"SysListView32") == 0 ||
            IsViewButtonArea(window, *bounds)) {
            // The styled item/button already supplies a rounded focus outline.
            if (!g_recordedFocus.exchange(true)) Wh_SetIntValue(L"ExplorerFocusObserved", 1);
            return TRUE;
        }
    }
    return DrawFocusRect_Original(dc, bounds);
}

static HDC WINAPI BeginPaint_Hook(HWND window, LPPAINTSTRUCT paint) {
    HDC dc = BeginPaint_Original(window, paint);
    if (dc) {
        if (g_paintDepth < ARRAYSIZE(g_paintWindows)) g_paintWindows[g_paintDepth] = window;
        ++g_paintDepth;
    }
    return dc;
}

static BOOL WINAPI EndPaint_Hook(HWND window, const PAINTSTRUCT* paint) {
    BOOL result = EndPaint_Original(window, paint);
    if (g_paintDepth) --g_paintDepth;
    return result;
}

static bool PaintHighlight(HTHEME theme, HDC dc, int part, int state,
                            const RECT* bounds, const RECT* clip, DWORD flags = 0) {
    if (!g_explorerHighlights || !g_themeClass || !bounds || !dc ||
        !IsExplorerWindow(PaintWindow(dc))) return false;
    wchar_t name[64]{};
    if (FAILED(g_themeClass(theme, name, ARRAYSIZE(name)))) return false;
    Highlight kind = ClassifyHighlight(name, part, state);
    if (kind == Highlight::None) return false;
    if (kind == Highlight::NativeFocus) {
        if (!g_recordedFocus.exchange(true)) Wh_SetIntValue(L"ExplorerThemeFocusObserved", 1);
        return true;
    }
    if (kind == Highlight::ViewButton && !IsViewButtonArea(PaintWindow(dc), *bounds)) return false;
    // File rows already have the pane background. Painting LISS_NORMAL here
    // can add another mod's fill/outline beneath our translucent rounded shape.
    // Header and button surfaces still need their neutral control background.
    if (kind == Highlight::Header || kind == Highlight::ViewButton) {
        HRESULT base = ThemeBackground_Original(theme, dc, part, 1, bounds, clip);
        if (FAILED(base)) return false;
    }
    Appearance style = CurrentAppearance();
    if (kind == Highlight::Header || kind == Highlight::ViewButton) style.radius = 4;
    if (flags & DTBG_OMITBORDER) style.borderWidth = 0;
    if (flags & DTBG_OMITCONTENT) style.fillOpacity = 0;
    UINT dpi = GetDpiForWindow(PaintWindow(dc));
    if (!BlendMarquee(dc, *bounds, clip, style, (dpi ? dpi : 96)/96.0f)) return false;
    if (kind == Highlight::ViewButton) {
        if (!g_recordedView.exchange(true)) Wh_SetIntValue(L"ExplorerViewButtonObserved", 1);
    } else {
        auto& recorded = kind == Highlight::Item ? g_recordedItem : g_recordedHeader;
        if (!recorded.exchange(true)) Wh_SetIntValue(kind == Highlight::Item ? L"ExplorerItemObserved" : L"ExplorerHeaderObserved", 1);
    }
    return true;
}

static HRESULT WINAPI ThemeBackground_Hook(HTHEME theme, HDC dc, int part, int state,
                                           const RECT* bounds, const RECT* clip) {
    if (PaintHighlight(theme, dc, part, state, bounds, clip)) return S_OK;
    return ThemeBackground_Original(theme, dc, part, state, bounds, clip);
}

static HRESULT WINAPI ThemeBackgroundEx_Hook(HTHEME theme, HDC dc, int part, int state,
                                             const RECT* bounds, const DTBGOPTS* options) {
    DWORD flags = options ? options->dwFlags : 0;
    const RECT* clip = flags & DTBG_CLIPRECT ? &options->rcClip : nullptr;
    if (!(flags & ~(DTBG_CLIPRECT|DTBG_OMITBORDER|DTBG_OMITCONTENT)) &&
        PaintHighlight(theme, dc, part, state, bounds, clip, flags)) return S_OK;
    return ThemeBackgroundEx_Original(theme, dc, part, state, bounds, options);
}

static void __cdecl StartSelector_Hook(void* self, POINT point, void* element,
                                       int selection, int button, int direction, void* item) {
    g_explorerSelector = self;
    StartSelector_Original(self, point, element, selection, button, direction, item);
}

static void __cdecl EndSelector_Hook(void* self) {
    EndSelector_Original(self);
    if (g_explorerSelector == self) g_explorerSelector = nullptr;
}

static void __cdecl ElementBackground_Hook(void* self, HDC dc, void* value,
    const RECT* bounds, const RECT* clip, const RECT* excluded, const RECT* target) {
    if (g_explorerMarquee && self == g_explorerSelector && bounds && IsExplorerWindow(PaintWindow(dc))) {
        UINT dpi = GetDpiForWindow(PaintWindow(dc));
        if (BlendMarquee(dc, *bounds, clip, CurrentAppearance(), (dpi ? dpi : 96)/96.0f)) {
            if (!g_recordedExplorerMarquee.exchange(true)) Wh_SetIntValue(L"ExplorerMarqueeObserved", 1);
            return;
        }
    }
    if (g_explorerHighlights && bounds && IsExplorerWindow(PaintWindow(dc)) &&
        IsViewButtonArea(PaintWindow(dc), *bounds) && ElementSelected && ElementMouseWithin) {
        UINT dpi = GetDpiForWindow(PaintWindow(dc));
        float scale = (dpi ? dpi : 96)/96.0f;
        int width = bounds->right-bounds->left, height = bounds->bottom-bounds->top;
        if (width > 16*scale && width <= 40*scale && height > 16*scale && height <= 40*scale &&
            (ElementSelected(self) || ElementMouseWithin(self))) {
            Appearance style = CurrentAppearance();
            style.radius = 4;
            if (BlendMarquee(dc, *bounds, clip, style, scale)) {
                if (!g_recordedView.exchange(true)) Wh_SetIntValue(L"ExplorerViewButtonObserved", 1);
                return;
            }
        }
    }
    ElementBackground_Original(self, dc, value, bounds, clip, excluded, target);
}

static void __cdecl ElementBorder_Hook(void* self, HDC dc, void* value, RECT* bounds, const RECT* clip) {
    void* previous = g_borderElement;
    if (g_explorerMarquee && self == g_explorerSelector && IsExplorerWindow(PaintWindow(dc))) g_borderElement = self;
    if (g_explorerHighlights && self != g_explorerSelector && bounds && IsExplorerWindow(PaintWindow(dc)) &&
        IsViewButtonArea(PaintWindow(dc), *bounds) &&
        ElementSelected && ElementMouseFocused &&
        (ElementSelected(self) || ElementMouseFocused(self))) {
        g_borderElement = self;
        if (!g_recordedBorder.exchange(true)) Wh_SetIntValue(L"ExplorerDuiBorderObserved", 1);
    }
    // Keep native border layout adjustments; suppress only its rasterized fill.
    ElementBorder_Original(self, dc, value, bounds, clip);
    g_borderElement = previous;
}

static void __cdecl ElementFill_Hook(void* self, HDC dc, ULONG color,
                                     int left, int top, int right, int bottom, bool alpha) {
    if (self != g_borderElement) ElementFill_Original(self, dc, color, left, top, right, bottom, alpha);
}

using PaintMarquee_t = void(__cdecl*)(void*, HDC, void*, const RECT*);
using ListWndProc_t = LRESULT(__cdecl*)(void*, HWND, UINT, WPARAM, LPARAM);
static PaintMarquee_t PaintMarquee_Original;
static PaintMarquee_t PaintMarquee_Target;
static ListWndProc_t ListWndProc_Original;

static bool IsDesktopList(HWND hwnd) {
    if (!hwnd) return false;
    wchar_t className[64]{};
    if (!GetClassNameW(hwnd, className, ARRAYSIZE(className)) ||
        wcscmp(className, L"SysListView32") != 0) return false;
    HWND parent = GetParent(hwnd);
    if (!GetClassNameW(parent, className, ARRAYSIZE(className)) ||
        wcscmp(className, L"SHELLDLL_DefView") != 0) return false;
    if (!GetClassNameW(GetAncestor(hwnd, GA_ROOT), className, ARRAYSIZE(className)))
        return false;
    return wcscmp(className, L"Progman") == 0 || wcscmp(className, L"WorkerW") == 0;
}

static LRESULT __cdecl ListWndProc_Hook(void* self, HWND hwnd, UINT message,
                                       WPARAM wParam, LPARAM lParam) {
    HWND previous = g_currentList;
    g_currentList = hwnd;
    LRESULT result = ListWndProc_Original(self, hwnd, message, wParam, lParam);
    g_currentList = previous;
    return result;
}

// Derive the exact member offsets that this Windows binary itself uses to
// intersect the dirty rect with its native marquee rect. Never hardcode a
// private layout or follow pointers when this instruction pattern is absent.
static bool ResolveGeometryLayout() {
    const BYTE* code = reinterpret_cast<const BYTE*>(PaintMarquee_Target);
    int ownerAt = -1, constantAt = -1, managerAt = -1, addAt = -1;
    for (int i = 0; i < 112; ++i) {
        if (code[i] == 0x48 && code[i+1] == 0x8B && code[i+2] == 0x41) {
            ownerAt = i; g_ownerOffset = code[i+3];
        }
        if (code[i] == 0x41 && code[i+1] == 0xBE) {
            DWORD offset; memcpy(&offset, code+i+2, sizeof(offset));
            constantAt = i; g_rectOffset = offset;
        }
        if (code[i] == 0x4C && code[i+1] == 0x8B && code[i+2] == 0x80) {
            DWORD offset; memcpy(&offset, code+i+3, sizeof(offset));
            managerAt = i; g_managerOffset = offset;
        }
        if (code[i] == 0x4D && code[i+1] == 0x03 && code[i+2] == 0xC6) addAt = i;
    }
    return ownerAt >= 0 && ownerAt < constantAt && constantAt < managerAt &&
        managerAt < addAt && addAt - managerAt < 24 &&
        g_ownerOffset > 0 && g_ownerOffset <= 128 && g_ownerOffset % 8 == 0 &&
        g_managerOffset > 0 && g_managerOffset < 4096 && g_managerOffset % 8 == 0 &&
        g_rectOffset > 0 && g_rectOffset < 1024 && g_rectOffset % 4 == 0;
}

static bool GetNativeSelectionRect(void* self, RECT& rect) {
    void* owner = nullptr;
    void* manager = nullptr;
    SIZE_T read = 0;
    HANDLE process = GetCurrentProcess();
    if (!ReadProcessMemory(process, static_cast<BYTE*>(self)+g_ownerOffset,
                           &owner, sizeof(owner), &read) || !owner) return false;
    if (!ReadProcessMemory(process, static_cast<BYTE*>(owner)+g_managerOffset,
                           &manager, sizeof(manager), &read) || !manager) return false;
    if (!ReadProcessMemory(process, static_cast<BYTE*>(manager)+g_rectOffset,
                           &rect, sizeof(rect), &read)) return false;
    return rect.right >= rect.left && rect.bottom >= rect.top &&
        static_cast<int64_t>(rect.right) - rect.left < 100000 &&
        static_cast<int64_t>(rect.bottom) - rect.top < 100000;
}

static void __cdecl PaintMarquee_Hook(void* self, HDC dc, void* theme,
                                     const RECT* rect) {
    RECT selection{};
    if (!dc || !rect || !IsDesktopList(g_currentList) ||
        !GetNativeSelectionRect(self, selection)) {
        PaintMarquee_Original(self, dc, theme, rect);
        return;
    }
    Appearance style{g_rgb.load(), g_fill.load(), g_border.load(),
                     g_radius.load(), g_width.load()};
    UINT dpi = GetDpiForWindow(g_currentList);
    Gdiplus::Graphics graphics(dc);
    if (graphics.GetLastStatus() != Gdiplus::Ok ||
        !DrawClippedMarquee(graphics, selection, *rect, style, (dpi ? dpi : 96) / 96.0f)) {
        PaintMarquee_Original(self, dc, theme, rect);
        return;
    }
    if (!g_recordedPaint.exchange(true)) {
        Wh_SetIntValue(L"DesktopPaintObserved", 1);
        Wh_Log(L"Custom desktop marquee painted: %ld,%ld,%ld,%ld, DPI=%u",
            rect->left, rect->top, rect->right, rect->bottom, dpi);
    }
}

static void LoadSettings() {
    WindhawkUtils::StringSetting color(Wh_GetStringSetting(L"Color"));
    DWORD rgb = 0xE81123;
    ParseRgb(color, rgb);
    g_rgb = rgb;
    g_fill = std::clamp(Wh_GetIntSetting(L"FillOpacity"), 0, 100);
    g_border = std::clamp(Wh_GetIntSetting(L"BorderOpacity"), 0, 100);
    g_radius = std::clamp(Wh_GetIntSetting(L"CornerRadius"), 0, 64);
    g_width = std::clamp(Wh_GetIntSetting(L"BorderWidth"), 0, 8);
    g_explorerHighlights = g_highlightsReady && Wh_GetIntSetting(L"ExplorerHighlights") != 0;
    g_explorerMarquee = g_marqueeReady && Wh_GetIntSetting(L"ExplorerMarquee") != 0;
}

static HMODULE LoadThemedControls() {
    HMODULE shell = LoadLibraryExW(L"shell32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!shell) return nullptr;
    ACTCTXW context{};
    context.cbSize = sizeof(context);
    context.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
    context.hModule = shell;
    context.lpResourceName = MAKEINTRESOURCEW(124);
    HANDLE activation = CreateActCtxW(&context);
    ULONG_PTR cookie = 0;
    HMODULE controls = nullptr;
    if (activation != INVALID_HANDLE_VALUE && ActivateActCtx(activation, &cookie)) {
        controls = LoadLibraryExW(L"comctl32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        DeactivateActCtx(0, cookie);
    }
    if (activation != INVALID_HANDLE_VALUE) ReleaseActCtx(activation);
    FreeLibrary(shell);
    return controls;
}

template<typename T> static bool HookExport(HMODULE module, const char* name, T hook, T* original) {
    T function = reinterpret_cast<T>(module ? GetProcAddress(module, name) : nullptr);
    return function && WindhawkUtils::SetFunctionHook(function, hook, original);
}

BOOL Wh_ModInit() {
    Wh_SetIntValue(L"DesktopPaintObserved", 0);
    Wh_SetIntValue(L"Initialized", 0);
    Wh_SetIntValue(L"ExplorerItemObserved", 0);
    Wh_SetIntValue(L"ExplorerHeaderObserved", 0);
    Wh_SetIntValue(L"ExplorerMarqueeObserved", 0);
    LoadSettings();
    Gdiplus::GdiplusStartupInput input;
    if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr) != Gdiplus::Ok)
        return FALSE;
    g_controls = LoadThemedControls();
    const WindhawkUtils::SYMBOL_HOOK resolve[] = {
        {{L"private: void __cdecl CLVDrawManager::_PaintMarquee(struct HDC__ *,void *,struct tagRECT const *)"},
         &PaintMarquee_Target, nullptr},
    };
    const WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {{L"private: void __cdecl CLVDrawManager::_PaintMarquee(struct HDC__ *,void *,struct tagRECT const *)"},
         &PaintMarquee_Original, PaintMarquee_Hook},
        {{L"public: __int64 __cdecl CListView::WndProc(struct HWND__ *,unsigned int,unsigned __int64,__int64)"},
         &ListWndProc_Original, ListWndProc_Hook},
    };
    // Desktop and Explorer capabilities fail independently.
    bool desktopReady = g_controls &&
        WindhawkUtils::HookSymbols(g_controls, resolve, ARRAYSIZE(resolve)) &&
        ResolveGeometryLayout() &&
        WindhawkUtils::HookSymbols(g_controls, hooks, ARRAYSIZE(hooks));
    Wh_SetIntValue(L"DesktopReady", desktopReady);
    HMODULE theme = GetModuleHandleW(L"uxtheme.dll");
    g_themeClass = reinterpret_cast<GetThemeClass_t>(theme ? GetProcAddress(theme, MAKEINTRESOURCEA(74)) : nullptr);
    bool highlightsReady = g_themeClass &&
        WindhawkUtils::SetFunctionHook(BeginPaint, BeginPaint_Hook, &BeginPaint_Original) &&
        WindhawkUtils::SetFunctionHook(EndPaint, EndPaint_Hook, &EndPaint_Original) &&
        WindhawkUtils::SetFunctionHook(DrawThemeBackground, ThemeBackground_Hook, &ThemeBackground_Original) &&
        WindhawkUtils::SetFunctionHook(DrawThemeBackgroundEx, ThemeBackgroundEx_Hook, &ThemeBackgroundEx_Original) &&
        WindhawkUtils::SetFunctionHook(DrawFocusRect, DrawFocusRect_Hook, &DrawFocusRect_Original);
    Wh_SetIntValue(L"ExplorerHighlightsReady", highlightsReady);
    g_highlightsReady = highlightsReady;
    g_dui = LoadLibraryExW(L"dui70.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (g_dui) {
        ElementSelected = reinterpret_cast<ElementBool_t>(GetProcAddress(g_dui, "?GetSelected@Element@DirectUI@@QEAA_NXZ"));
        ElementMouseWithin = reinterpret_cast<ElementBool_t>(GetProcAddress(g_dui, "?GetMouseWithin@Element@DirectUI@@QEAA_NXZ"));
        ElementMouseFocused = reinterpret_cast<ElementBool_t>(GetProcAddress(g_dui, "?GetMouseFocused@Element@DirectUI@@QEAA_NXZ"));
    }
    g_frame = LoadLibraryExW(L"ExplorerFrame.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    const WindhawkUtils::SYMBOL_HOOK explorerHooks[] = {
        {{L"public: void __cdecl UIMarqueeSelector::StartMarquee(struct tagPOINT,class DirectUI::Element *,enum INITIALSELECTION,enum CLICKBUTTON,enum __MIDL___MIDL_itf_itemsview_0000_0004_0001,class UIItem *)"},
         &StartSelector_Original, StartSelector_Hook},
        {{L"public: void __cdecl UIMarqueeSelector::EndMarquee(void)"},
         &EndSelector_Original, EndSelector_Hook},
    };
    bool marqueeReady = highlightsReady && g_dui && g_frame &&
        WindhawkUtils::HookSymbols(g_frame, explorerHooks, ARRAYSIZE(explorerHooks)) &&
        HookExport(g_dui, "?PaintBackground@Element@DirectUI@@QEAAXPEAUHDC__@@PEAVValue@2@AEBUtagRECT@@222@Z", ElementBackground_Hook, &ElementBackground_Original) &&
        HookExport(g_dui, "?PaintBorder@Element@DirectUI@@QEAAXPEAUHDC__@@PEAVValue@2@PEAUtagRECT@@AEBU5@@Z", ElementBorder_Hook, &ElementBorder_Original) &&
        HookExport(g_dui, "?_Fill@Element@DirectUI@@IEAAXPEAUHDC__@@KHHHH_N@Z", ElementFill_Hook, &ElementFill_Original);
    Wh_SetIntValue(L"ExplorerMarqueeReady", marqueeReady);
    g_marqueeReady = marqueeReady;
    LoadSettings();
    Wh_SetIntValue(L"Initialized", 1);
    Wh_Log(L"Selection styles initialized: desktop=%d highlights=%d marquee=%d", desktopReady, highlightsReady, marqueeReady);
    return TRUE;
}

void Wh_ModSettingsChanged() { LoadSettings(); }

void Wh_ModUninit() {
    Wh_SetIntValue(L"Initialized", 0);
    if (g_controls) FreeLibrary(g_controls);
    if (g_dui) FreeLibrary(g_dui);
    if (g_frame) FreeLibrary(g_frame);
    if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
}
#endif
