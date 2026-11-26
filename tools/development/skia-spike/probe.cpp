#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"
#include "include/ports/SkTypeface_win.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main(int argc, char** argv) try {
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(480, 320));
    check(bool(surface), "Raster surface allocation");
    auto* canvas = surface->getCanvas();
    canvas->clear(SK_ColorWHITE);
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    canvas->saveLayerAlpha(nullptr, 128);
    canvas->drawRect(SkRect::MakeXYWH(10, 10, 80, 60), paint);
    canvas->drawRect(SkRect::MakeXYWH(50, 10, 80, 60), paint);
    canvas->restore();
    SkPixmap pixels;
    check(surface->peekPixels(&pixels), "Raster pixels");
    check(pixels.getColor(20, 30) == pixels.getColor(60, 30), "Subtree opacity darkens overlap");
    const auto translucent = pixels.getColor(20, 30);
    check(SkColorGetR(translucent) == 255 && SkColorGetG(translucent) >= 126 &&
          SkColorGetG(translucent) <= 128, "Layer alpha result");

    canvas->save();
    canvas->translate(160, 10);
    canvas->clipRect(SkRect::MakeWH(60, 60));
    canvas->scale(2, 2);
    canvas->clipRect(SkRect::MakeXYWH(5, 5, 15, 15));
    paint.setColor(SK_ColorBLUE);
    canvas->drawRect(SkRect::MakeWH(100, 100), paint);
    canvas->restore();
    check(pixels.getColor(180, 30) == SK_ColorBLUE, "Transformed clip interior");
    check(pixels.getColor(165, 30) == SK_ColorWHITE && pixels.getColor(205, 30) == SK_ColorWHITE,
          "Nested clip coordinate space");
    canvas->save();
    canvas->translate(270, 40);
    canvas->rotate(90);
    paint.setColor(SK_ColorGREEN);
    canvas->drawRect(SkRect::MakeXYWH(0, 0, 30, 10), paint);
    canvas->restore();
    check(pixels.getColor(265, 55) == SK_ColorGREEN && pixels.getColor(280, 45) == SK_ColorWHITE,
          "Rotation geometry");

    const SkPoint points[] = {{10, 100}, {130, 100}};
    const SkColor4f colors[] = {SkColors::kRed, SkColors::kBlue};
    paint.setShader(SkShaders::LinearGradient(points,
        SkGradient(SkGradient::Colors(colors, SkTileMode::kClamp), {})));
    canvas->drawRect(SkRect::MakeXYWH(10, 90, 120, 60), paint);
    check(SkColorGetR(pixels.getColor(20, 110)) > SkColorGetR(pixels.getColor(120, 110)),
          "Linear gradient interpolation");
    paint.setShader(nullptr);
    paint.setColor(SK_ColorBLUE);
    paint.setImageFilter(SkImageFilters::DropShadow(8, 8, 3, 3, SK_ColorBLACK, nullptr));
    canvas->drawRect(SkRect::MakeXYWH(165, 95, 40, 40), paint);
    check(pixels.getColor(210, 130) != SK_ColorWHITE, "Drop shadow outside source bounds");
    paint.setImageFilter(SkImageFilters::Blur(3, 3, nullptr));
    canvas->drawRect(SkRect::MakeXYWH(255, 95, 40, 40), paint);
    check(pixels.getColor(252, 110) != SK_ColorWHITE, "Blur outside source bounds");
    paint.setImageFilter(nullptr);

    auto fontManager = SkFontMgr_New_DirectWrite();
    check(bool(fontManager), "DirectWrite font manager");
    auto typeface = fontManager->matchFamilyStyle("Segoe UI", SkFontStyle());
    check(bool(typeface), "System font lookup");
    SkFont font(typeface, 24);
    paint.setColor(SK_ColorBLACK);
    canvas->drawString("Sunfleck / Skia CPU probe", 10, 205, font, paint);
    bool glyphPixel = false;
    for (int y = 180; y < 208; ++y) for (int x = 10; x < 350; ++x)
        glyphPixel |= pixels.getColor(x, y) != SK_ColorWHITE;
    check(glyphPixel, "DirectWrite-backed basic glyph drawing");

    auto snapshot = surface->makeImageSnapshot();
    canvas->drawImage(snapshot, 0, 220);
    check(surface->peekPixels(&pixels), "Pixels after snapshot copy-on-write");
    check(pixels.getColor(20, 250) == translucent, "Image drawing");
    std::ofstream out(argc > 1 ? argv[1] : "probe.ppm", std::ios::binary);
    out << "P6\n480 320\n255\n";
    for (int y = 0; y < 320; ++y) for (int x = 0; x < 480; ++x) {
        auto color = pixels.getColor(x, y);
        const char rgb[] = {char(SkColorGetR(color)), char(SkColorGetG(color)), char(SkColorGetB(color))};
        out.write(rgb, 3);
    }
    check(bool(out), "Write evidence image");
    std::cout << "PASS: layer opacity, nested transforms/clips, rotation, gradient, shadow, blur, image, basic DirectWrite glyphs\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
