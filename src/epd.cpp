#include "epd.h"

Epd::Epd()
    : palette(Magick::Geometry(4, 1), Magick::Color(0, 0, 0))
{
    palette.colorSpace(Magick::sRGBColorspace);
    palette.pixelColor(0, 0, Magick::ColorGray(0.0));
    palette.pixelColor(1, 0, Magick::ColorGray(0x55/255.0));
    palette.pixelColor(2, 0, Magick::ColorGray(0xab/255.0));
    palette.pixelColor(3, 0, Magick::ColorGray(1.0));
}

Epd::~Epd() {
}

void Epd::processFrame(Magick::Image& image, struct pixman_region16* damage) {
    // We are using the lite variant of the ImageMagick package, so we cannot use PNG here.
    image.write("current2.ppm");

    if (false) {
        image.quantizeDither(true);
        image.quantizeDitherMethod(Magick::FloydSteinbergDitherMethod);
        //image.quantizeColorSpace(Magick::HSLColorspace);
        //image.quantizeColorSpace(Magick::RGBColorspace);

        // This would reset the dither method:
        // see see https://github.com/ImageMagick/ImageMagick/discussions/6017
        // Why? It calls quantizeDither(), which resets the dither method, as well.
        image.map(palette, true);
    } else {
        // We have to do it the manual way (see above). Thus, we do most of what image.map() would do:
        image.modifyImage();
        GetPPException;
        //options()->quantizeDither(true);  // That's the call that we have to skip.
        //auto quantizeInfo = image.options()->quantizeInfo();  // we cannot call that :-/
        MagickCore::QuantizeInfo quantizeInfo;
        GetQuantizeInfo(&quantizeInfo);
        quantizeInfo.number_colors = 4;
        quantizeInfo.dither_method = Magick::FloydSteinbergDitherMethod;
        RemapImage(&quantizeInfo, image.image(), palette.constImage(), exceptionInfo);
    }
    image.write("current3.ppm");

    if (true) {
        printf("processFrame\n");
        printf("  damage:\n    extents: x1=%d, y1=%d, x2=%d, y2=%d\n",
            damage->extents.x1, damage->extents.y1, damage->extents.x2, damage->extents.y2);
        int num = 0;
        pixman_box16_t* rects = pixman_region_rectangles(damage, &num);
        for (int i=0; i<num; i++) {
            printf("    rect: x1=%d, y1=%d, x2=%d, y2=%d\n",
                rects[i].x1, rects[i].y1, rects[i].x2, rects[i].y2);
        }
    }
}
