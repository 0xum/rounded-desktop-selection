#define MVP_RENDER_TEST
#include "rounded-desktop-selection.wh.cpp"
#include <cstdio>
#include <cstdlib>

static int failures = 0;
static void Check(bool condition, const char* message) {
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
    if (!condition) ++failures;
}
static int PngEncoder(CLSID& clsid) {
    UINT count = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&count, &size);
    auto encoders = static_cast<Gdiplus::ImageCodecInfo*>(std::malloc(size));
    if (!encoders) return -1;
    Gdiplus::GetImageEncoders(count, size, encoders);
    int found = -1;
    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(encoders[i].MimeType, L"image/png") == 0) {
            clsid = encoders[i].Clsid; found = static_cast<int>(i); break;
        }
    }
    std::free(encoders);
    return found;
}

int main() {
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput startup;
    if (Gdiplus::GdiplusStartup(&token, &startup, nullptr) != Gdiplus::Ok) return 2;
    {
        DWORD rgb = 0;
        Check(ParseRgb(L"#E81123", rgb) && rgb == 0xE81123, "RGB parser");
        Check(!ParseRgb(L"#GG1123", rgb) && !ParseRgb(L"E8112300", rgb), "Reject invalid color");
        Gdiplus::Bitmap image(400, 240, PixelFormat32bppARGB);
        Gdiplus::Graphics graphics(&image);
        graphics.Clear(Gdiplus::Color(255, 255, 255, 255));
        RECT rect{40, 40, 340, 200};
        Appearance style;
        Check(DrawMarquee(graphics, rect, style, 1.0f), "Render default marquee");
        Gdiplus::Color pixel;
        image.GetPixel(150, 100, &pixel);
        Check(std::abs(static_cast<int>(pixel.GetR()) - 250) <= 1 &&
              std::abs(static_cast<int>(pixel.GetG()) - 207) <= 1 &&
              std::abs(static_cast<int>(pixel.GetB()) - 211) <= 1,
              "20 percent fill correctly blends over white");
        image.GetPixel(40, 40, &pixel);
        Check(pixel.GetR() == 255 && pixel.GetG() == 255 && pixel.GetB() == 255,
              "Rounded corner leaves background untouched");
        image.GetPixel(39, 100, &pixel);
        Check(pixel.GetR() == 255 && pixel.GetG() == 255 && pixel.GetB() == 255,
              "Drawing remains within rectangle");
        graphics.Clear(Gdiplus::Color(255, 255, 255, 255));
        RECT dirtyStrip{150, 80, 160, 160};
        Check(DrawClippedMarquee(graphics, rect, dirtyStrip, style, 1),
              "Paint narrow dirty strip using full selection geometry");
        image.GetPixel(150, 100, &pixel);
        Check(std::abs(static_cast<int>(pixel.GetG()) - 207) <= 1,
              "Dirty strip is fill, without an artificial border");
        image.GetPixel(149, 100, &pixel);
        Check(pixel.GetG() == 255, "Dirty paint cannot alter outside pixels");
        graphics.Clear(Gdiplus::Color(255, 255, 255, 255));
        DrawMarquee(graphics, rect, style, 1);
        CLSID encoder;
        if (PngEncoder(encoder) >= 0)
            image.Save(L"render-preview.png", &encoder, nullptr);
        graphics.Clear(Gdiplus::Color(255, 255, 255, 255));
        style.fillOpacity = 0; style.borderOpacity = 0;
        Check(DrawMarquee(graphics, rect, style, 1), "Zero opacity supported");
        image.GetPixel(150, 100, &pixel);
        Check(pixel.GetR() == 255 && pixel.GetG() == 255 && pixel.GetB() == 255,
              "Zero opacity leaves background untouched");
        style = Appearance{}; style.radius = 64; style.borderWidth = 8;
        Check(DrawMarquee(graphics, RECT{10, 10, 11, 100}, style, 2),
              "One pixel width and high DPI");
        Check(DrawMarquee(graphics, RECT{10, 10, 100, 11}, style, 2),
              "One pixel height and high DPI");
        Check(DrawMarquee(graphics, RECT{10, 10, 10, 10}, style, 1),
              "Empty rectangle");
        style.radius = 0;
        Check(DrawMarquee(graphics, rect, style, 1.5f), "Square corners and 150 percent DPI");
        graphics.Clear(Gdiplus::Color(255, 16, 32, 64));
        style = Appearance{};
        DrawMarquee(graphics, rect, style, 1);
        image.GetPixel(150, 100, &pixel);
        Check(std::abs(static_cast<int>(pixel.GetR()) - 59) <= 1 &&
              std::abs(static_cast<int>(pixel.GetG()) - 29) <= 1 &&
              std::abs(static_cast<int>(pixel.GetB()) - 58) <= 1,
              "Fill blends over dark background");
    }
    Gdiplus::GdiplusShutdown(token);
    return failures ? 1 : 0;
}
