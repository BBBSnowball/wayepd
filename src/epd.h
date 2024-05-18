#include <ImageMagick/Magick++.h>
#include <iostream>
#include <fstream>
#include <cstdint>
extern "C" {
    #include <pixman.h>
}

#define EPD_WIDTH 280
#define EPD_HEIGHT 480

class Epd {
    Magick::Image palette;
    std::fstream fd;
    std::string buf;

    bool expectOkReply();
public:
    Epd();
    ~Epd();
    void processFrame(Magick::Image& image, struct pixman_region16* damage);
};
