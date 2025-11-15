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
    // 2. for each top and bottom row, swap their column pixels
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
    // 1. take n number of rows of image
    // 2. for each row, swap the first and last column
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

// -l <degrees> rotate image by multiples of 90º
// opencv easy way -> rotate(); transpose(); and flip();
void rotate(Mat& img, const int rotation, ostringstream& new_path) {
    if (rotation % 360 == 0) {
        new_path << "_rotate360";
        return;
    }

    // counterclockwise 90 degrees rotation
    if (rotation / 90 == 1) {
        Mat rot = Mat(img.cols, img.rows, img.type());

        for (int i = 0; i < img.rows; i++) {
            const Vec3b* src_row = img.ptr<Vec3b>(i);
            for (int j = 0; j < img.cols; j++) {
                rot.at<Vec3b>(img.cols - j - 1, i) = src_row[j];
            }
        }
        img = rot.clone();
        rot.deallocate();
        new_path << "_rotate90";
        return;
    }

    // 180 degrees rotation
    if (rotation / 90 == 2) {
        const int cols = img.cols;
        const int rows = img.rows;

        // curiously, if img memory is continuous, this algo is the same
        // for horizontal flip. But without restraining the rows, it is
        // actually both horizontal AND vertical flips
        if (img.isContinuous()) {
            uchar* data = img.data;
            const int chnl = img.channels();
            const int elements = rows * cols;

            for (int i = 0; i < elements / 2; i++) {
                for (int j = 0; j < chnl; j++) {
                    const int curr = i * chnl + j;
                    const int swap = (elements - i - 1) * chnl + j;

                    const uchar buff = data[curr];
                    data[curr] = data[swap];
                    data[swap] = buff;
                }
            }
        } else {
            for (int i = 0; i < rows / 2; i++) {
                for (int j = 0; j < cols; j++) {
                    const Vec3b buff = img.at<Vec3b>(i, j);
                    img.at<Vec3b>(i, j) = img.at<Vec3b>(cols - i - 1, rows - j - 1);
                    img.at<Vec3b>(cols - i - 1, rows - j - 1) = buff;
                }
            }
            if (rows % 2 == 1) {
                const int mid_row = rows / 2;
                for (int j = 0; j < cols / 2; j++) {
                    const Vec3b buff = img.at<Vec3b>(mid_row, j);
                    img.at<Vec3b>(mid_row, j) = img.at<Vec3b>(mid_row, rows - j - 1);
                    img.at<Vec3b>(mid_row, rows - j - 1) = buff;
                }
            }
        }
        new_path << "_rotate180";
        return;
    }

    // clockwise 90 degrees rotation
    if (rotation / 90 == 3) {
        Mat rot = Mat(img.cols, img.rows, img.type());

        for (int i = 0; i < img.rows; i++) {
            const Vec3b* src_row = img.ptr<Vec3b>(i);
            for (int j = 0; j < img.cols; j++) {
                rot.at<Vec3b>(j, img.rows - i - 1) = src_row[j];
            }
        }
        img = rot.clone();
        new_path << "_rotate270";
        return;
    }
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
        cerr << "  -b GAIN BIAS          increase gamma\n";
        cerr << "  -d GAIN BIAS          decrease gamma\n";
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
    // flip_horizontal(img, new_path);

    // -l <degrees> rotate image by multiples of 90º
    // 2 approaches: rotate 90 x 3 times or find if rotation is 90, 180, 270 or 360
    // 1. multiply matrix by its corresponding rotation
    // opencv easy way
    const int rotation = stoi(argv[3]);

    rotate(img, rotation, new_path);

    // -b <intensity> increase brightness
    // 1. take g and b, gain and bias from user input
    // 2. for each row, for each col, for each channel, multiply by g and add b
    // 3. save new image
    // opencv easy way -> convertTo() or LUT();

    // -d <intensity> decreases brightness
    // 1. take g and b, gain and bias from user input
    // 2. for each row, for each col, for each channel, divide by g and subtract b
    // 3. save new image
    // opencv easy way -> convertTo() or LUT();

    return save_picture(img, file_path, new_path);
}