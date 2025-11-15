#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <iostream>

using namespace cv;
using namespace std;

/// save changes to new file
int save_picture(Mat& img, const string& file_path, const ostringstream& new_path) {
    try {
        const string extension = file_path.substr(file_path.find_last_of('.') + 1);
        const string final_path = file_path.substr(0, file_path.find_last_of('.'))
            + new_path.str() + '.' + extension;

        imwrite(final_path, img);
        cout << "Picture saved to: " << final_path << endl;
        return 0;
    } catch (exception &e) {
        cerr << "\nError: " << e.what() << endl;
        return 1;
    }
}

/// -n create negative image
/// opencv easy way -> cv::LUT()
void negative(Mat& img, ostringstream& new_path) {
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

    new_path << "_negative";
}

/// -y mirror image vertically
/// opencv easy way
void flip_vertical(Mat& img, ostringstream& new_path) {
    // 1. take n number of rows of image
    // 2. for each row y, get its pointer
    // 3. save the new (x, y) pixel with the new y value
    const int cols = img.cols;
    const int rows = img.rows;
    const int chnl = img.channels();

    for (int i = 0; i < rows / 2; i ++) {
        uchar* top_row = img.ptr<uchar>(i);
        uchar* bot_row = img.ptr<uchar>(rows - 1 - i);

        for (int j = 0; j < cols * chnl; j++) {
            const uchar buff = top_row[j];
            top_row[j] = bot_row[j];
            bot_row[j] = buff;
        }
    }

    new_path << "_flipvert";
}

/// -x mirror image horizontally
/// opencv easy way
void flip_horizontal(Mat& img, ostringstream& new_path) {
    // 1. take n number of columns of image
    // 2. for each (x, y) pixel, subtract n by the x-axis value
    // 3. save the new (x, y) pixel with the new x value
    const int cols = img.cols;
    const int rows = img.rows;
    const int chnl = img.channels();

    for (int i = 0; i < rows; i++) {
        uchar* row = img.ptr<uchar>(i);
        for (int j = 0; j < cols / 2; j++) {
            for (int k = 0; k < chnl; k++) {
                const uchar buff = row[j * chnl + k];
                row[j * chnl + k] = row[(cols - 1 - j) * chnl + k];
                row[(cols - 1 - j) * chnl + k] = buff;
            }
        }
    }

    new_path << "_fliphoriz";
}

/// Without using possible existing functions on OpenCV, but
/// without possibly specifying what are the limits of usage of OpenCV,
/// without possibly jeorpadizing the entire purpose of learning codification,
/// without some do-it-all functions of OpenCV, implement the operations!!
/// (Open Comando Vermelho Rogério Lemgruber)
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

    // const char* OPTIONS = argv[1];
    const string file_path = argv[2];
    ostringstream new_path;
    Mat img = imread(file_path, IMREAD_COLOR);

    if (img.empty())
    {
        cout << "Could not read the image: " << file_path << endl;
        return 1;
    }

    // -n create negative image
    // negative(img, file_path);

    // -y mirror image vertically
    // flip_vertical(img, new_path);

    // -x mirror image horizontally
    flip_horizontal(img, new_path);

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

    return save_picture(img, file_path, new_path);
}