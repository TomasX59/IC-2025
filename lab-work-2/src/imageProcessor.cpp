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
int main(const int argc, const char *argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: imageProcessor [OPTION] FILE\n\n";
        cerr << "Process images with various transformations.\n\n";
        cerr << "Options:\n";
        cerr << "  -n                    create negative image\n";
        cerr << "  -y                    mirror image vertically\n";
        cerr << "  -x                    mirror image horizontally\n";
        cerr << "  -l DEGREES            rotate image by multiples of 90°\n";
        cerr << "  -b INTENSITY          increase brightness\n";
        cerr << "  -d INTENSITY          decrease brightness\n";
        cerr << "  -h, --help            display this help and exit\n";
        return 1;
    }

    const string file_path = argv[2];
    Mat img = imread(file_path, IMREAD_COLOR);

    if (img.empty())
    {
        cout << "Could not read the image: " << file_path << endl;
        return 1;
    }

    // -n create negative image
    // 1. take each pixel value RGB
    // 2. for each channel of a pixel, subtract value from 255
    // 3. save each pixel in new image
    // opencv easy way -> cv::LUT()
    try {
        const int cols = img.cols;
        const int rows = img.rows;
        const int chnl = img.channels();

        // looking into documentation, I found an example that the best
        // performance can be taken by checking if we can iterate by an
        // image as if it was a single row:
        if (img.isContinuous()) {
            uchar* data = img.data;
            const int element_count = rows * cols * chnl;
            for (int i = 0; i < element_count; i++) {
                data[i] = 255 - data[i];
            }
        } else {
            for (int i = 0; i < rows; i++) {
                uchar* row = img.ptr<uchar>(i);
                for (int j = 0; j < cols * chnl; j++) {
                    row[j] = 255 - row[j];
                }
            }
        }

        const string extension = file_path.substr(file_path.find_last_of('.') + 1);
        const string new_path = file_path.substr(0, file_path.find_last_of('.')) + "_negatv." + extension;

        imwrite(new_path, img);
        cout << "Negative saved to: " << new_path << endl;

    } catch (exception &e) {
        cerr << "\nError: " << e.what() << endl;
    }

    // -y mirror image vertically
    // 1. take n number of rows of image
    // 2. for each (x, y) pixel, subtract n by the y axis value
    // 3. save the new (x, y) pixel with the new y value
    // opencv easy way

    // -x mirror image horizontally
    // 1. take n number of columns of image
    // 2. for each (x, y) pixel, subtract n by the x axis value
    // 3. save the new (x, y) pixel with the new x value
    // opencv easy way

    // -l <degrees> rotate image by multiples of 90º
    // opencv easy way

    // -b <intensity> increase brightness
    // 1. take g and b, gain and bias from user input
    // 2. for each row, for each col, for each channel, multiply by g and add b
    // 3. save new image
    // opencv easy way -> convertTo();

    // -d <intensity> decreases brightness
    // 1. take g and b, gain and bias from user input
    // 2. for each row, for each col, for each channel, divide by g and subtract b
    // 3. save new image
    // opencv easy way -> convertTo();

    return 0;
}