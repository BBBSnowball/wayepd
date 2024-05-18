#include <ImageMagick/Magick++.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdint>
extern "C" {
    #include <pixman.h>
}

#define EPD_WIDTH 280
#define EPD_HEIGHT 480

class Epd {
    Magick::Image palette;
    int fd;
    char buf[1024];
    std::ostringstream buf2;

    bool expectOkReply(struct timeval timeout);
public:
    Epd();
    ~Epd();
    void processFrame(Magick::Image& image, struct pixman_region16* damage);
};
