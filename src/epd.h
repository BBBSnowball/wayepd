#include <ImageMagick/Magick++.h>
extern "C" {
    #include <pixman.h>
}

class Epd {
    Magick::Image palette;
public:
    Epd();
    ~Epd();
    void processFrame(Magick::Image& image, struct pixman_region16* damage);
};
