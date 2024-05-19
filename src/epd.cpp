#include "epd.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string_view>
#include <string>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>

void errorUnexpectedReply(const std::string_view& buf) {
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

bool Epd::expectOkReply(struct timeval timeout) {
    bool ok = false;
    bool error = false;
    size_t num_bytes = 0;
    while (1) {
        if (ok || error) {
            timeout = { 0, 0 };
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        //NOTE Timeout is modified to contain the remaining time - which is what we want.
        //NOTE This is only true on Linux.
        if (!select(fd+1, &fds, nullptr, nullptr, &timeout)) {
            if (ok)
                return true;
            else if (error)
                return false;
            else {
                std::cerr << "epd: ERROR: timeout in read\n";
                return false;
            }
        }
        auto n = read(fd, buf+num_bytes, sizeof(buf)-num_bytes-1);
        if (n < 0 && errno == EAGAIN) {
            continue;
        } else if (n <= 0) {
            std::cerr << "ERROR: Couldn't read from $EPD_TTY\n";
            perror("ERROR");
            return false;
        }
        //std::cerr << "DEBUG: reply with " << n << " bytes, total in buffer is " << (num_bytes+n) << "\n";
        num_bytes += n;
        buf[num_bytes] = 0;

        char* line_start = buf;
        char* line_end;
        while ((line_end = (char*)memchr(line_start, '\n', buf+sizeof(buf)-line_start))) {
            *line_end = 0;
            auto line_end2 = line_end;
            if (line_end2[-1] == '\r') {
                line_end2[-1] = 0;
                line_end2--;
            }
            std::string_view line(line_start, line_end2-line_start);

            //std::cerr << "DEBUG: reply with line of length " << line.length() << "\n";
            if (line == "OK") {
                ok = true;
            } else if (line == "") {
                // ignore
            } else if (line.length() >=2 && line[1] == ':') {
                switch (line[0]) {
                    case 'E':
                        std::cerr << "epd: received ERROR: " << line << "\n";
                        error = true;
                        break;
                    case 'W':
                        std::cerr << "epd: received WARN: " << line << "\n";
                        break;
                    case 'I':
                        std::cerr << "epd: received INFO: " << line << "\n";
                        break;
                    default:
                        errorUnexpectedReply(line);
                        error = true;
                        break;
                }
            } else {
                errorUnexpectedReply(line);
                error = true;
                break;
            }

            line_start = line_end+1;
        }

        if (line_start != buf) {
            num_bytes -= line_start-buf;
            memmove(buf, line_start, num_bytes);
        }
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

    if (strcmp(path, "NONE") == 0) {
        dryRun = true;
        fd = 0;
        return;
    } else {
        dryRun = false;
    }

    fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << "ERROR: Couldn't open $EPD_TTY: " << path << "\n";
        perror("ERROR");
        exit(1);
    }

    // change settings
    struct termios t;
    if (tcgetattr(fd, &t) < 0) {
        perror("ERROR in tcgetattr");
        exit(1);
    }
    t.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
                | INLCR | IGNCR | ICRNL | IXON);
    t.c_oflag &= ~OPOST;
    t.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    t.c_cflag &= ~(CSIZE | PARENB);
    t.c_cflag |= CS8;
    cfsetospeed(&t, B115200);
    if (tcsetattr(fd, TCSANOW, &t) < 0) {
        perror("ERROR in tcsetattr");
        exit(1);
    }

    for (int i=1; ; i++) {
        // abort partial commands
        write(fd, "\x03\r\n\r\n", 3);

        // discard previous data without blocking
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval timeout = { 0, 10000 };  // 10 ms
        if (select(fd+1, &fds, nullptr, nullptr, &timeout)) {
            read(fd, buf, sizeof(buf));
        }

        std::cerr << "epd: send init command to display on path " << path << "\n";
        write(fd, "I\n", 2);
        struct timeval timeout2 = { 1, 0 };
        if (expectOkReply(timeout2)) {
            break;
        } else if (i < 3) {
            continue;
        } else {
            std::cerr << "ERROR: We give up!\n";
            exit(1);
        }
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
    //image.write("current3.miff");

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

    if (!dryRun) {
        buf2.str("");
        buf2.clear();

        buf2 << "\x03\r\nD\r\n";
        auto t1 = palette.pixelColor(1, 0).quantumRed();
        auto t2 = palette.pixelColor(2, 0).quantumRed();
        auto t3 = palette.pixelColor(3, 0).quantumRed();
        for (int y=0; y<EPD_HEIGHT; y++) {
            for (int x=0; x<EPD_WIDTH; x++) {
                auto c = image.pixelColor(x, y).quantumRed();
                char ch;
                if (c < t1)
                    ch = ' ';
                else if (c < t2)
                    ch = '-';
                else if (c < t3)
                    ch = '+';
                else
                    ch = '#';
                buf2 << ch;
            }
            buf2 << "\n";
        }

        size_t written = 0;
        while (written < buf2.str().length()) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(fd, &fds);
            struct timeval timeout = { 20, 0 };
            if (!select(fd+1, nullptr, &fds, nullptr, &timeout)) {
                std::cerr << "epd: ERROR: timeout in write\n";
                (void)expectOkReply({ 1, 0 });
                exit(1);
                return;
            }

            size_t bytes_to_write = buf2.str().length() - written;
            if (bytes_to_write > 1024)
                bytes_to_write = 1024;
            //std::cerr << "DEBUG: writing " << bytes_to_write << " bytes\n";
            auto x = write(fd, buf2.str().c_str() + written, bytes_to_write);
            //std::cerr << "DEBUG: written: " << written << "+" << x << " of " << buf2.str().length() << "\n";
            if (x < 0 && errno == EAGAIN) {
                continue;
            } else if (x < 0) {
                std::cerr << "ERROR: Couldn't write to $EPD_TTY, x=" << x << ", errno=" << errno << "\n";
                perror("ERROR");
                exit(1);
                return;
            } else if (x == 0) {
                std::cerr << "ERROR: $EPD_TTY has been closed\n";
                exit(1);
                return;
            } else {
                written += x;
            }
        }

        struct timeval timeout = { 20, 0 };
        (void)expectOkReply(timeout);
    }
}
