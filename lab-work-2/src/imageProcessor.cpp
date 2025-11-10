#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include <iostream>

using namespace cv;
using namespace std;

// Without using existing specific functions on OpenCv
// -n create negative image
// -y mirror image vertically
// -x mirror image horizontally
// -l <degrees> rotate image by multiples of 90º
// -b <intensity> increase brightness
// -d <intensity> decreases brightness
int main(int argc, char *argv[])
{
    if (argc < 1)
    {
        std::cerr << "Usage: bin2text image_file text_file\n";
        return 1;
    }

    std::string image_path = samples::findFile("starry_night.jpg");
    Mat img = imread(image_path, IMREAD_COLOR);

    if (img.empty())
    {
        std::cout << "Could not read the image: " << image_path << std::endl;
        return 1;
    }

    imshow("Display window", img);
    int k = waitKey(0); // Wait for a keystroke in the window

    if (k == 's')
    {
        imwrite("starry_night.png", img);
    }

    return 0;
}