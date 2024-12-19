#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <string>
#include <cstdint>

using std::queue;
using std::mutex;
using std::unique_lock;
using std::lock_guard;
using std::condition_variable;
using std::thread;
using std::vector;
using std::string;
using std::min;
using std::ref;
using std::cref;
using std::cout;
using std::cerr;
using std::ifstream;
using std::ofstream;
using std::endl;

struct Pixel {
  uint8_t r, g, b;
};

struct Tile {
  int startX, startY;
  int endX, endY;
};

queue<Tile> inversionTaskQueue;
queue<Tile> blurTaskQueue;
mutex inversionQueueMutex, blurQueueMutex;
condition_variable inversionQueueCV, blurQueueCV;
bool inversionDone = false;
bool blurDone = false;

bool readImage(const string& filename, vector<Pixel>& image, int& width, int& height) {
  ifstream file(filename);
  if (!file.is_open()) {
    cerr << "Error: Could not open file " << filename << endl;
    return false;
  }

  string format;
  file >> format;
  if (format != "P3") {
    cerr << "Error: Unsupported file format (not P3 PPM)" << endl;
    return false;
  }

  file >> width >> height;
  int maxVal;
  file >> maxVal;

  image.resize(width * height);
  for (auto& pixel : image) {
    int r, g, b;
    file >> r >> g >> b;
    pixel.r = static_cast<uint8_t>(r);
    pixel.g = static_cast<uint8_t>(g);
    pixel.b = static_cast<uint8_t>(b);
  }

  return true;
}

bool writeImage(const string& filename, const vector<Pixel>& image, int width, int height) {
  ofstream file(filename);
  if (!file.is_open()) {
    cerr << "Error: Could not open file " << filename << endl;
    return false;
  }

  file << "P3\n" << width << " " << height << "\n255\n";
  for (const auto& pixel : image) {
    file << static_cast<int>(pixel.r) << " " << static_cast<int>(pixel.g) << " " << static_cast<int>(pixel.b) << "\n";
  }

  return true;
}

void invertWorker(const vector<Pixel>& inputImage, vector<Pixel>& outputImage, int width, int height) {
  while (true) {
    Tile tile;
    {
      unique_lock<mutex> lock(inversionQueueMutex);
      inversionQueueCV.wait(lock, [] { return !inversionTaskQueue.empty() || inversionDone; });

      if (inversionDone && inversionTaskQueue.empty()) {
        break;
      }

      tile = inversionTaskQueue.front();
      inversionTaskQueue.pop();
    }

    for (int y = tile.startY; y < tile.endY; ++y) {
      for (int x = tile.startX; x < tile.endX; ++x) {
        const Pixel& pixel = inputImage[y * width + x];
        Pixel& invertedPixel = outputImage[y * width + x];

        invertedPixel.r = 255 - pixel.r;
        invertedPixel.g = 255 - pixel.g;
        invertedPixel.b = 255 - pixel.b;
      }
    }
  }
}

void blurWorker(const vector<Pixel>& inputImage, vector<Pixel>& outputImage, 
    int width, int height, int kernelSize) {
  int halfKernel = kernelSize / 2;

  while (true) {
    Tile tile;
    {
      unique_lock<mutex> lock(blurQueueMutex);
      blurQueueCV.wait(lock, [] { return !blurTaskQueue.empty() || blurDone; });

      if (blurDone && blurTaskQueue.empty()) {
        break;
      }

      tile = blurTaskQueue.front();
      blurTaskQueue.pop();
    }

    for (int y = tile.startY; y < tile.endY; ++y) {
      for (int x = tile.startX; x < tile.endX; ++x) {
        int rSum = 0, gSum = 0, bSum = 0;
        int count = 0;
        
        for (int ky = -halfKernel; ky <= halfKernel; ++ky) {
          for (int kx = -halfKernel; kx <= halfKernel; ++kx) {
            int nx = x + kx;
            int ny = y + ky;
            
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
              const Pixel& pixel = inputImage[ny * width + nx];
              rSum += pixel.r;
              gSum += pixel.g;
              bSum += pixel.b;
              count++;
            }
          }
        }
        
        Pixel& outPixel = outputImage[y * width + x];
        outPixel.r = rSum / count;
        outPixel.g = gSum / count;
        outPixel.b = bSum / count;
      }
    }

  
    {
      lock_guard<mutex> lock(inversionQueueMutex);
      inversionTaskQueue.push(tile);
    }
    inversionQueueCV.notify_one();
  }
}

void process(const vector<Pixel>& inputImage, vector<Pixel>& outputImage, 
    int width, int height, int kernelSize, int tileSize) {
  vector<Pixel> blurredImage(width * height);

  int numThreads = thread::hardware_concurrency();
  vector<thread> inversionWorkers;
  vector<thread> blurWorkers;
  
  for (int i = 0; i < numThreads; ++i) {
    blurWorkers.emplace_back(blurWorker, cref(inputImage), ref(blurredImage), width, height, kernelSize);
  }

  for (int i = 0; i < numThreads; ++i) {
    inversionWorkers.emplace_back(invertWorker, cref(blurredImage), ref(outputImage), width, height);
  }

  for (int startY = 0; startY < height; startY += tileSize) {
    for (int startX = 0; startX < width; startX += tileSize) {
      Tile tile;
      tile.startX = startX;
      tile.startY = startY;
      tile.endX = min(startX + tileSize, width);
      tile.endY = min(startY + tileSize, height);

      {
        lock_guard<mutex> lock(blurQueueMutex);
        blurTaskQueue.push(tile);
      }
      blurQueueCV.notify_one();
    }
  }
  
  {
    lock_guard<mutex> lock(blurQueueMutex);
    blurDone = true;
  }
  blurQueueCV.notify_all();

  for (auto& worker : blurWorkers) {
    worker.join();
  }
  
  {
    lock_guard<mutex> lock(inversionQueueMutex);
    inversionDone = true;
  }
  inversionQueueCV.notify_all();
  
  for (auto& worker : inversionWorkers) {
    worker.join();
  }
}

int main() {
  string inputFile = "input.ppm";
  string outputFile = "output.ppm";
  int width, height;
  int kernelSize = 20;
  int tileSize = 64;

  vector<Pixel> inputImage;
  if (!readImage(inputFile, inputImage, width, height)) {
    return 1;
  }

  vector<Pixel> outputImage(width * height);

  process(inputImage, outputImage, width, height, kernelSize, tileSize);

  if (!writeImage(outputFile, outputImage, width, height)) {
    return 1;
  }

  cout << "Output saved to " << outputFile << endl;

  return 0;
}
