#ifndef PPM
#define PPM

#include <cmath>
#include <fstream>
#include <cstring>
// Extra code to handle ppm image functions
struct ppm {
  unsigned char *image;
  int w, h;
  unsigned char *font;
  int fw, fh, cw, base;
  ppm(int width, int height) {
    w = width;
    h = height;
    image = new unsigned char[width * height * 3];
    memset(image, 0xFF, width * height * 3);
    font = NULL;
  }
  ~ppm() {
    delete[] image;
    if (font) delete[] font;
  }
  void set_pixel(int x, int y,
                 unsigned char r, unsigned char g, unsigned char b) {
    int p = (w * y + x) * 3;
    image[p + 0] = r;
    image[p + 1] = g;
    image[p + 2] = b;
  }
  int get_pixel(int x, int y) {
    int p = (w * y + x) * 3;
    int r = image[p + 0];
    int g = image[p + 1];
    int b = image[p + 2];
    return (r << 16) + (g << 8) + b;
  }
  void draw_line(int x_0, int y_0, int x_1, int y_1,
                 unsigned char r, unsigned char g, unsigned char b) {
    int d_x = x_1 - x_0;
    int d_y = y_1 - y_0;
    double err = 0;
    double d_err = d_x ? std::abs(d_y * 1.0 / d_x) : 0;
    int x_s = 1, y_s = 1;
    bool steep = false;
    int octant = 0;
    if (!d_x || d_err > 1) octant = 1;
    if (d_x < 0) octant = 3 - octant;
    if (d_y < 0) octant = 7 - octant;
    switch (octant) {
      case 0: break;
      case 1: steep = true; break;
      case 2: steep = true; x_s = -1; break;
      case 3: x_s = -1; break;
      case 4: x_s = -1; y_s = -1; break;
      case 5: steep = true; x_s = -1; y_s = -1; break;
      case 6: steep = true; y_s = -1; break;
      case 7: y_s = -1; break;
    }
    x_0 *= x_s;
    x_1 *= x_s;
    y_0 *= y_s;
    y_1 *= y_s;
    if (steep) {
      int temp = x_0;
      x_0 = y_0;
      y_0 = temp;
      temp = x_1;
      x_1 = y_1;
      y_1 = temp;
      d_err = 1 / d_err;
      if (d_x == 0) d_err = 0;
    }
    int y = y_0;
    for (int x = x_0; x <= x_1; ++x) {
      if (steep) set_pixel(y * x_s, x * y_s, r, g, b);
      else set_pixel(x * x_s, y * y_s, r, g, b);
      err += d_err;
      if (err >= 0.5) {
        y++;
        err -= 1;
      }
    }
  }
  void draw_circle(int x_c, int y_c, int radius,
                   unsigned char r, unsigned char g, unsigned char b) {
    int x = radius;
    int y = 0;
    int r_err = 1 - x;
    while (x >= y) {
      set_pixel(x + x_c, y + y_c, r, g, b);
      set_pixel(y + x_c, x + y_c, r, g, b);
      set_pixel(-x + x_c, y + y_c, r, g, b);
      set_pixel(-y + x_c, x + y_c, r, g, b);
      set_pixel(-x + x_c, -y + y_c, r, g, b);
      set_pixel(-y + x_c, -x + y_c, r, g, b);
      set_pixel(x + x_c, -y + y_c, r, g, b);
      set_pixel(y + x_c, -x + y_c, r, g, b);
      y++;
      if (r_err < 0)
        r_err += 2 * y + 1;
      else {
        x--;
        r_err += 2 * (y - x + 1);
      }
    }
  }
  void draw_string(int x_0, int y_0, const char *text,
                   unsigned char r, unsigned char g, unsigned char b) {
    if (!font) return;
    for (int i = 0; text[i]; ++i) {
      int x_s = x_0 + i * cw;
      int y_s = y_0 - base;
      for (int y = 0; y < fh; ++y)
        for (int x = 0; x < cw; ++x) {
          int p = (fw * y + x + (text[i] - ' ') * cw) * 3;
          if (font[p]) set_pixel(x_s + x, y_s + y, r, g, b);
        } 
    }
  }
  void load_font(const char *filename) {
    std::ifstream ifile;
    ifile.open(filename, std::ios_base::binary | std::ios_base::in);
    unsigned char c;
    int temp;
    ifile >> c >> c >> fw >> fh >> temp >> c;
    font = new unsigned char[fw * fh * 3];
    for (int i = 0; i < fw * fh * 3; ++i)
      ifile >> font[i];
    ifile.close();
    cw = fw / 95;
    base = fh - 1;
    int p = (cw * ('L' - ' ') + (fh - 2) * fw) * 3;
    int r = 0;
    while(!font[p] && r++ < base)
      p -= fw * 3;
    base -= r;
  }
  void save_to(const char *filename) {
    std::ofstream ofile;
    ofile.open(filename, std::ios_base::binary | std::ios_base::out);
    ofile << "P6\n" << w << " " << h << " 255\n";
    for (int i = 0; i < w * h * 3; ++i)
      ofile << image[i];
    ofile.close();
  }
};

#endif
