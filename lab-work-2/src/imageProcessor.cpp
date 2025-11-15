#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <unistd.h>

using namespace cv;
using namespace std;

/// save changes to new file
int save_picture(const Mat & img, const string& file_path, const ostringstream& new_path)
{
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
void negative(Mat& img, ostringstream& new_path)
{
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

/// -y mirror image vertically;
/// opencv easy way -> flip()
void flip_vertical(Mat& img, ostringstream& new_path)
{
    const int rows = img.rows;
    const int row_size = img.cols * img.channels();

    for (int i = 0; i < rows / 2; i++) {
        uchar* top_row = img.ptr<uchar>(i);
        uchar* bot_row = img.ptr<uchar>(rows - 1 - i);
        std::swap_ranges(top_row, top_row + row_size, bot_row);
    }

    new_path << "_flipvert";
}

/// -x mirror image horizontally;
/// opencv easy way
void flip_horizontal(Mat& img, ostringstream& new_path)
{
    const int cols = img.cols;
    const int rows = img.rows;
    const int cnhl = img.channels();

    for (int i = 0; i < rows; i++) {
        uchar* row = img.ptr<uchar>(i);
        for (int j = 0; j < cols / 2; j++) {
            for (int k = 0; k < cnhl; k++) {
                swap(row[j * cnhl + k], row[(cols - j - 1) * cnhl + k]);
            }
        }
    }

    new_path << "_fliphoriz";
}

// -l <degrees> rotate image by multiples of 90º;
// opencv easy way -> rotate(); transpose(); and flip();
void rotate(Mat& img, const int rotation, ostringstream& new_path)
{
    if (rotation % 360 == 0) {
        new_path << "_rotate360";
        return;
    }

    // counterclockwise 90 degrees rotation
    if (rotation / 90 == 1) {
        Mat rot_img = Mat(img.cols, img.rows, img.type());

        for (int i = 0; i < img.rows; i++) {
            const Vec3b* src_row = img.ptr<Vec3b>(i);
            for (int j = 0; j < img.cols; j++) {
                Vec3b* dst_row = rot_img.ptr<Vec3b>(img.cols - j - 1);
                dst_row[i] = src_row[j];
            }
        }
        img = rot_img;
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
            Vec3b* data = img.ptr<Vec3b>(0);
            const int elements = rows * cols;

            for (int i = 0; i < elements / 2; i++) {
                swap(data[i], data[(elements - i - 1)]);
            }
        } else {
            for (int i = 0; i < rows / 2; i++) {
                Vec3b* top_row = img.ptr<Vec3b>(i);
                Vec3b* bot_row = img.ptr<Vec3b>(rows - i - 1);

                for (int j = 0; j < cols; j++) {
                    swap(top_row[j], bot_row[cols - j - 1]);
                }
            }
            if (rows % 2 == 1) {
                Vec3b* mid_row = img.ptr<Vec3b>(rows / 2);

                for (int j = 0; j < cols / 2; j++) {
                    swap(mid_row[j], mid_row[cols - 1 - j]);
                }
            }
        }
        new_path << "_rotate180";
        return;
    }

    // clockwise 90 degrees rotation
    if (rotation / 90 == 3) {
        Mat rot_img = Mat(img.cols, img.rows, img.type());

        for (int i = 0; i < img.rows; i++) {
            const Vec3b* src_row = img.ptr<Vec3b>(i);
            for (int j = 0; j < img.cols; j++) {
                Vec3b* dst_row = rot_img.ptr<Vec3b>(j);
                dst_row[img.rows - i - 1] = src_row[j];
            }
        }
        img = rot_img;
        new_path << "_rotate270";
    }
}

// -b <intensity> increase contrast and brightness;
// opencv easy way -> convertTo()
void increase_contrbright(Mat& img, const double& gain, const int& bias, ostringstream& new_path)
{
    // this algo would be just 3 nested iterators, to apply some changes
    // to each channel of each column of each row, which is already done
    // in the negative() function. So for this one I took the liberty of
    // using Mat::forEach(), that actually can take a lambda and also
    // parallelizes execution under the hood.
    img.forEach<Vec3b>([&gain, &bias](Vec3b &pixel, const int[]) -> void {
        pixel[0] = saturate_cast<uchar>(pixel[0] * gain + bias); //b
        pixel[1] = saturate_cast<uchar>(pixel[1] * gain + bias); //g
        pixel[2] = saturate_cast<uchar>(pixel[2] * gain + bias); //r
    });

    new_path << "_increased_contrbright";
}

// -d <intensity> decreases contrast and brightness;
// opencv easy way -> convertTo()
void decrease_contrbright(Mat& img, const double& gain, const int& bias, ostringstream& new_path)
{
    img.forEach<Vec3b>([&gain, &bias](Vec3b &pixel, const int[]) -> void {
        pixel[0] = saturate_cast<uchar>(pixel[0] / gain - bias); //b
        pixel[1] = saturate_cast<uchar>(pixel[1] / gain - bias); //g
        pixel[2] = saturate_cast<uchar>(pixel[2] / gain - bias); //r
    });

    new_path << "_decreased_contrbright";
}

void print_usage()
{
    cerr << "Usage: imageProcessor [OPTION] FILE\n\n";
    cerr << "Process images with various transformations.\n";
    cerr << "\nFlags:\n";
    cerr << "  -n                     create negative image\n";
    cerr << "  -y                     mirror image vertically\n";
    cerr << "  -x                     mirror image horizontally\n";
    cerr << "  -i                     show image processing stats\n";
    cerr << "  -h, --help             display this help and exit\n";
    cerr << "\nOptions:\n";
    cerr << "  -l DEGREES             rotate counterclockwise by 90°\n";
    cerr << "  -b [0-10.0] [0-100]    increase contrast and brightness\n";
    cerr << "  -d [0-10.0] [0-100]    decrease contrast and brightness\n";
}

/// Without using possible existing functions on OpenCV, but
/// without possibly specifying what are the limits of usage of OpenCV,
/// without possibly jeorpadizing the learning of image matrix manipulation,
/// without some do-it-all functions of OpenCV, implement the operations!!
/// (Open Comando Vermelho Rogério Lemgruber)
int main(const int argc, char *const *argv) {
    string file_path;
    int rotation = 0;
    double b_gain = 1.0, d_gain = 1.0;
    int b_bias = 0, d_bias = 0;
    bool ngtv = false, flip_y = false, flip_x = false, proc_i = false;

    int opt;
    while ((opt = getopt(argc, argv, "nyxil:b:d:h")) != -1) {
        switch (opt) {
        case 'n': ngtv = true; break;
        case 'y': flip_y = true; break;
        case 'x': flip_x = true; break;
        case 'i': proc_i = true; break;
        case 'l':
            rotation = stoi(optarg);
            if (rotation % 90 != 0) {
                cerr << "Error: rotation must be a multiple of 90\n";
                return 1;
            }
            break;
        case 'b':
            b_gain = stod(optarg);
            b_bias = stoi(argv[optind++]);
            if (b_gain < 0 || b_bias < 0) {
                cerr << "Error: Invalid gain/bias values\n";
                return 1;
            }
            break;
        case 'd':
            d_gain = stod(optarg);
            d_bias = stoi(argv[optind++]);
            if (d_gain < 0 || d_bias < 0) {
                cerr << "Error: Invalid gain/bias values\n";
                return 1;
            }
            break;
        case 'h':
            print_usage();
            return 0;
        default:
            print_usage();
            return 1;
        }
    }

    if (optind < argc) {
        file_path = argv[optind];
    } else {
        cerr << "Error: No file specified\n";
        return 1;
    }

    cout << "Reading image from: " << file_path << endl;

    Mat img = imread(file_path, IMREAD_COLOR);
    if (img.empty())
    {
        cerr << "Could not read the image: " << file_path << endl;
        cerr << "Supported formats: JPG, PNG, BMP, TIFF\n";
        return 1;
    }

    if (proc_i) {
        cout << "\n====== IMAGE INFO ====== " << endl;
        cout << "Dimensions: " << img.rows << " x " << img.cols << endl;
        cout << "Channels: " << img.channels() << ", " << img.type() << endl;
        cout << "Image data size: " << img.total() * img.elemSize() / (1024 * 1024) << " MB" << endl;
    }

    ostringstream new_path;
    ostringstream proc_info;

    TickMeter tm_total;
    tm_total.start();

    // image processors
    if (ngtv) {
        TickMeter tm;
        tm.start();
        negative(img, new_path);
        tm.stop();
        proc_info << "Negative inversion - " << tm.getTimeMilli() << "ms." << endl;
    }
    if (flip_y) {
        TickMeter tm;
        tm.start();
        flip_vertical(img, new_path);
        tm.stop();
        proc_info << "Flip Vertical - " << tm.getTimeMilli() << "ms." << endl;
    }
    if (flip_x) {
        TickMeter tm;
        tm.start();
        flip_horizontal(img, new_path);
        tm.stop();
        proc_info << "Flip Horizontal - " << tm.getTimeMilli() << "ms." << endl;
    }
    if (rotation != 0) {
        TickMeter tm;
        tm.start();
        rotate(img, rotation, new_path);
        tm.stop();
        proc_info << "Rotate by " << rotation << " degrees - " << tm.getTimeMilli() << "ms." << endl;
    }
    if (b_gain != 1.0 || b_bias != 0) {
        TickMeter tm;
        tm.start();
        increase_contrbright(img, b_gain, b_bias, new_path);
        tm.stop();
        proc_info << "C/B increase - " << tm.getTimeMilli() << "ms." << endl;
    };
    if (d_gain != 1.0 || d_bias != 0) {
        TickMeter tm;
        tm.start();
        decrease_contrbright(img, d_gain , d_bias, new_path);
        tm.stop();
        proc_info << "C/B increase - " << tm.getTimeMilli() << "ms." << endl;
    }

    tm_total.stop();

    if (proc_i) {
        cout << "\n====== PERFORMANCE ====== " << endl;
        cout << proc_info.str();
        cout << "Total processing time: " << tm_total.getAvgTimeMilli() << "ms.\n" << endl;
    }

    return save_picture(img, file_path, new_path);
}