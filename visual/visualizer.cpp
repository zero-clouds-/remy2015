#include "ppm.h"

#include <iostream>

using namespace std;

struct pt {
    double x, y;
};

int main() {
    pt points[64];
    double x, y;
    int count = 0;
    while (cin >> x >> y) {
        points[count].x = x;
        points[count].y = y;
        count++;
    }
    double minx, miny, maxx, maxy;
    maxx = maxy = -9999;
    minx = miny = 9999;
    for (int i = 0; i < count; ++i) {
        if (points[i].x > maxx) maxx = points[i].x; 
        if (points[i].y > maxy) maxy = points[i].y; 
        if (points[i].x < minx) minx = points[i].x; 
        if (points[i].y < miny) miny = points[i].y; 
    }
    maxx += 0.5;
    maxy += 0.5;
    minx -= 0.5;
    miny -= 0.5;
    ppm img((int)((maxx - minx) * 100), (int)((maxy - miny) * 100));
    img.draw_circle((int)((points[0].x - minx) * 100),
                    (int)((points[0].y - miny) * 100), 5, 0, 0xFF, 0);
    for (int i = 1; i < count; ++i) {
        img.draw_line((int)((points[i - 1].x - minx) * 100), 
                      (int)((points[i - 1].y - miny) * 100),
                      (int)((points[i].x - minx) * 100), 
                      (int)((points[i].y - miny) * 100), 0, 0, 0xFF);
        img.draw_circle((int)((points[i].x - minx) * 100),
                        (int)((points[i].y - miny) * 100), 5, 0xFF, 0, 0);
    }
    img.save_to("image.ppm");
    return 0;
}
