#include <iostream>
#include <fstream>
#include <ostream>
#include <vector>
#include <cstdlib>
#include <ctime>

using std::string;
using std::vector;
using std::ofstream;
using std::cout;
using std::cerr;
using std::endl;

#define IMAGE_WIDTH_TILES 16
#define IMAGE_HEIGHT_TILES 9
#define SCALE_FACTOR 100

struct Pixel {
  int r, g, b;
};

int main() {
  string filename = "input.ppm";

  int width = IMAGE_WIDTH_TILES * SCALE_FACTOR;
  int height = IMAGE_HEIGHT_TILES * SCALE_FACTOR;
  
  srand(static_cast<unsigned>(time(nullptr)));
  
  vector<Pixel> colors(IMAGE_WIDTH_TILES * IMAGE_HEIGHT_TILES);
  for (auto& color : colors) {
    color.r = rand() % 256;
    color.g = rand() % 256;
    color.b = rand() % 256;
  }
  
  ofstream file(filename);
  if (!file.is_open()) {
    cerr << "Error: Could not open file " << filename << endl;
    return 1;
  }
  
  file << "P3\n";
  file << width << " " << height << "\n255\n";
  
  for (int y = 0; y < height; ++y) {
    int tileRow = (y / SCALE_FACTOR);
    for (int x = 0; x < width; ++x) {
      int tileCol = (x / SCALE_FACTOR);
      const Pixel& color = colors[tileRow * IMAGE_WIDTH_TILES + tileCol];
      file << color.r << " " << color.g << " " << color.b << " ";
    }
    file << "\n";
  }

  file.close();
  cout << "PPM file generated: " << filename << endl;
  return 0;
}
