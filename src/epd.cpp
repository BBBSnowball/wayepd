#include "epd.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <string.h>

void errorUnexpectedReply(const std::string& buf) {
    std::cerr << "epd: ERROR: unexpected reply\n";
    std::cerr << "  reply: \"";
    size_t i;
    for (i=0; i<16 && i < buf.length(); i++) {
        if (buf[i] >= ' ' && buf[i] < 'z')
            std::cerr << (char)buf[i];
        else
            std::cerr << std::hex << std::setw(2) << (uint8_t)buf[i];
    }
    std::cerr << "\"";
    if (i > 16)
        std::cerr << "...";
    std::cerr << "\n";
}

bool Epd::expectOkReply() {
    fd.flush();

    bool ok = false;
    bool error = false;
    while (1) {
        if (ok || error) {
            // read more data but only if more data is available
            char c;
            if (!fd.readsome(&c, 1))
                break;
            fd.unget();
        } else {
            char c;
            fd.read(&c, 1);
            fd.unget();
        }
        if (!std::getline(fd, buf))
            break;
        if (!fd) {
            std::cerr << "epd: ERROR: error in read\n";
            return false;
        }
        if (buf[buf.length()-1] == '\r')
            buf.erase(buf.end()-1);
        std::cerr << "DEBUG: reply of length " << buf.length() << "\n";
        if (buf == "OK") {
            ok = true;
            continue;
        } else if (buf == "") {
            continue;
        } else if (buf.length() >=2 && buf[1] == ':') {
            switch (buf[0]) {
                case 'E':
                    std::cerr << "epd: received ERROR: " << buf << "\n";
                    error = true;
                    continue;
                case 'W':
                    std::cerr << "epd: received WARN: " << buf << "\n";
                    continue;
                case 'I':
                    std::cerr << "epd: received INFO: " << buf << "\n";
                    continue;
            }
        }

        errorUnexpectedReply(buf);
        error = true;
    }

    if (!ok && !error)
        std::cerr << "epd: ERROR: no reply\n";

    return ok;
}

Epd::Epd()
    : palette(Magick::Geometry(4, 1), Magick::Color(0, 0, 0))
{
    palette.colorSpace(Magick::sRGBColorspace);
    palette.pixelColor(0, 0, Magick::ColorGray(0.0));
    palette.pixelColor(1, 0, Magick::ColorGray(0x55/255.0));
    palette.pixelColor(2, 0, Magick::ColorGray(0xab/255.0));
    palette.pixelColor(3, 0, Magick::ColorGray(1.0));

    const char* path = getenv("EPD_TTY");
    if (!path) {
        std::cerr << "ERROR: The environment variable EPD_TTY must be set, e.g. \"/dev/ttyACM0\".\n";
        exit(1);
    }

    std::cerr << "epd: send init command to display on path " << path << "\n";
    fd.open(path);

    // not using ignore() because that has a different semantic (it will block)
    buf.resize(1024);
    fd.readsome(buf.data(), sizeof(buf));

    fd << "\x03\n\nI\n";
    fd.flush();
    if (!expectOkReply()) {
        exit(1);
    }
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
    image.write("current3.miff");

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

    if (true) {
        fd << "\x03D\n";
        auto t1 = palette.pixelColor(1, 0);
        auto t2 = palette.pixelColor(1, 0);
        auto t3 = palette.pixelColor(1, 0);
        for (int y=0; y<EPD_HEIGHT; y++) {
            for (int x=0; x<EPD_WIDTH; x++) {
                auto c = image.pixelColor(x, y);
                if (c.quantumRed() < t1.quantumRed())
                    fd << ' ';
                else if (c.quantumRed() < t2.quantumRed())
                    fd << '-';
                else if (c.quantumRed() < t3.quantumRed())
                    fd << '+';
                else
                    fd << '#';
            }
            fd << "\n";
        }
        fd.flush();

        (void)expectOkReply();
    }
}
