#include "include/CImg.h"

using namespace cimg_library;

int main()
{
    CImg<float> img(640, 480, 1, 3);
    cimg_forXY(img, x, y)
    {
        img(x, y, 0, 0) = (float) x / (float) img.width();
        img(x, y, 0, 1) = (float) y / (float) img.height();
        img(x, y, 0, 2) = (float) (x + y) / (float) (img.width() + img.height());
    }
    img.display("crt");
    img *= 255.f;
    img.save("crt.png");

    return 0;
}
