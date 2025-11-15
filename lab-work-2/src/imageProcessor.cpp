#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <unistd.h>

#include <iostream>
#include <filesystem>

using namespace cv;
using namespace std;

/// overengineered way to save changes to a new file
int save_picture(const Mat & img, const string& file_path, const ostringstream& new_path)
{
    if (new_path.str().length() == 0) {
        cout << "No changes made to picture." << endl;
        return 0;
    }

    try {
        const string output = "out/";
        const size_t last_slh = file_path.find_last_of("/");
        const size_t last_dot = file_path.find_last_of(".");

        string folder;
        if (last_slh == string::npos) {
            // No slash - file in current directory
            folder = output;
        } else {
            folder = file_path.substr(0, last_slh + 1) + output;
        }

        std::filesystem::create_directories(folder);

        const size_t name_start = last_slh == string::npos ? 0 : last_slh + 1;
        const size_t name_length = last_dot - name_start;
        const string name = file_path.substr(name_start, name_length);

        const string extension = file_path.substr(last_dot + 1);
        const string final_path = folder + name + new_path.str() + '.' + extension;

        imwrite(final_path, img);
        cout << "Picture saved to: " << final_path << endl;
        return 0;
    } catch (exception &e) {
        cerr << "\nError: " << e.what() << endl;
        return 1;
    }
}

/// -n create negative image
/// opencv easy way -> cv::bitwise_not()
void negative(Mat& img, ostringstream& new_path)
{
    // Update: this is to handle images with more than 8bits of depth.
    // I've already implemented the hard way, now I'll go the easy way.
    if (img.depth() != CV_8U) {
        bitwise_not(img, img);
        new_path << "_negtv";
        return;
    }

    const int cols = img.cols;
    const int rows = img.rows;
    const int chnl = img.channels();
    const int pchn = chnl == 4 ? 3 : chnl;

    // looking into documentation, I found an example that the best
    // performance can be taken by checking if we can iterate by an
    // image as if it was a single row:
    if (img.isContinuous()) {
        uchar* data = img.data;
        for (int i = 0; i < rows * cols; i++) {
            for (int j = 0; j < pchn; j++) {
                const int curr = i * chnl + j;
                data[curr] = 255 - data[curr];
            }
        }
    } else {
        for (int i = 0; i < rows; i++) {
            uchar* row = img.ptr<uchar>(i);
            for (int j = 0; j < cols; j++) {
                for (int k = 0; k < pchn; k++) {
                    const int curr = j * chnl + k;
                    row[curr] = 255 - row[curr];
                }
            }
        }
    }

    new_path << "_negtv";
}

/// -y mirror image vertically;
/// opencv easy way -> flip()
void flip_vertical(Mat& img, ostringstream& new_path)
{
    // update: adding support to images with depth larger than 8bit.
    // original code is down below.
    if (img.depth() != CV_8U) {
        flip(img, img, 0);
        new_path << "_flipy";
        return;
    }

    // o.g. code for 8-bit images
    const int rows = img.rows;
    const int row_size = img.cols * img.channels();

    for (int i = 0; i < rows / 2; i++) {
        uchar* top_row = img.ptr<uchar>(i);
        uchar* bot_row = img.ptr<uchar>(rows - 1 - i);
        std::swap_ranges(top_row, top_row + row_size, bot_row);
    }

    new_path << "_flipy";
}

/// -x mirror image horizontally;
/// opencv easy way -> flip()
void flip_horizontal(Mat& img, ostringstream& new_path)
{
    // update: adding support to images with depth larger than 8bit.
    // original code is down below.
    if (img.depth() != CV_8U) {
        flip(img, img, 1);
        new_path << "_flipx";
        return;
    }

    // o.g. code for 8-bit images
    const int cols = img.cols;
    const int rows = img.rows;
    const int chnl = img.channels();

    for (int i = 0; i < rows; i++) {
        uchar* row = img.ptr<uchar>(i);
        for (int j = 0; j < cols / 2; j++) {
            for (int k = 0; k < chnl; k++) {
                swap(row[j * chnl + k], row[(cols - j - 1) * chnl + k]);
            }
        }
    }

    new_path << "_flipx";
}

/// -l <degrees> rotate image by multiples of 90º;
/// opencv easy way -> rotate();
void rotate(Mat& img, int rotation, ostringstream& new_path)
{
    rotation = (rotation % 360 + 360) % 360;
    if (rotation == 0) return;

    // update: added this to handle images with depth higher than 8bit.
    // this time using cv::rotate() because I'm lazy.
    if (img.depth() != CV_8U) {
        int rotateCode = 0;
        bool needs_rotation = true;

        switch(rotation) {
            case 90:  rotateCode = ROTATE_90_COUNTERCLOCKWISE; break;
            case 180: rotateCode = ROTATE_180; break;
            case 270: rotateCode = ROTATE_90_CLOCKWISE; break;
            default:  needs_rotation = false; break;
        }

        if (needs_rotation) {
            cv::rotate(img, img, rotateCode);
            new_path << "_r" << rotation;
        }
        return;
    }

    // update: og functions down below
    // this one is the hardest and most cumbersome of my implementations,
    // since there are many cases and advantages to be taken of. I could've
    // just applied the 90 degrees rotation up to 3 times to achieve the others,
    // but the 90 degrees and 270 degrees need to create a new Mat, while
    // the 180 degrees rotation can actually swap pixel in the same Mat.
    // Also, the 180 can take advantage of Mat::isContinous(), for O(n)
    // while 90/270 cannot (or would be very hard to implement).
    const int cols = img.cols;
    const int rows = img.rows;
    const int chnl = img.channels();

    // counterclockwise 90 / clockwise 270 degrees rotation
    if (rotation == 90) {
        Mat rot_img = Mat(cols, rows, img.type());

        for (int i = 0; i < rows; i++) {
            const uchar* src_row = img.ptr<uchar>(i);
            for (int j = 0; j < cols; j++) {
                uchar* dst_row = rot_img.ptr<uchar>(cols - j - 1);
                for (int k = 0; k < chnl; k++) {
                    dst_row[i * chnl + k] = src_row[j * chnl + k];
                }
            }
        }
        img = rot_img;
        new_path << "_r90";
        return;
    }

    // 180 degrees rotation
    if (rotation == 180) {
        // curiously, if img memory is continuous, this algo is the same
        // for horizontal flip. But without restraining the rows, it is
        // actually both horizontal AND vertical flips
        if (img.isContinuous()) {
            uchar* data = img.ptr<uchar>(0);
            const int elements = rows * cols;

            for (int i = 0; i < elements / 2; i++) {
                for (int k = 0; k < chnl; k++) {
                    swap(data[i * chnl + k],
                        data[(elements - i - 1) * chnl + k]);
                }
            }
        } else {
            for (int i = 0; i < rows / 2; i++) {
                uchar* top_row = img.ptr<uchar>(i);
                uchar* bot_row = img.ptr<uchar>(rows - i - 1);

                for (int j = 0; j < cols; j++) {
                    for (int k = 0; k < chnl; k++) {
                        swap(top_row[j * chnl + k],
                            bot_row[(cols - j - 1) * chnl + k]);
                    }
                }
            }
            if (rows % 2 == 1) {
                uchar* mid_row = img.ptr<uchar>(rows / 2);

                for (int j = 0; j < cols / 2; j++) {
                    for (int k = 0; k < chnl; k++) {
                        swap(mid_row[j * chnl + k],
                            mid_row[(cols - 1 - j) * chnl + k]);
                    }
                }
            }
        }
        new_path << "_r180";
        return;
    }

    // clockwise 90 / counterclockwise 270 degrees rotation
    if (rotation == 270) {
        Mat rot_img = Mat(cols, rows, img.type());

        for (int i = 0; i < rows; i++) {
            const uchar* src_row = img.ptr<uchar>(i);
            for (int j = 0; j < cols; j++) {
                uchar* dst_row = rot_img.ptr<uchar>(j);
                for (int k = 0; k < chnl; k++) {
                    dst_row[(rows - i - 1) * chnl + k] = src_row[j * chnl + k];
                }
            }
        }
        img = rot_img;
        new_path << "_r270";
    }
}

/// -b <intensity> increase contrast and brightness;
/// opencv easy way -> convertTo()
void increase_contrbright(Mat& img, const double& gain, const int& bias, ostringstream& new_path)
{
    // this is to handle images with alpha channel
    if (img.channels() == 4) {
        vector<Mat> channels;

        split(img, channels);
        for (int i = 0; i < 3; i++) channels[i].convertTo(channels[i], -1, gain, bias);
        merge(channels, img);

        new_path << "_icb";
        return;
    }

    // Update: again, handling non-8bit images using OpenCV
    if (img.depth() != CV_8U) {
        img.convertTo(img, -1, gain, bias);
        new_path << "_icb";
        return;
    }

    // this algo would be just 3 nested iterators, to apply some changes
    // to each channel of each column of each row, which is already done
    // in the negative() function. So for this one I took the liberty of
    // using Mat::forEach(), that actually can take a lambda and also
    // parallelizes execution under the hood.
    // update: I'm changing this to support more channels than bgr
    img.forEach<uchar>([&](uchar& pixel, const int[]) {
            pixel = saturate_cast<uchar>(pixel * gain + bias);
        });

    new_path << "_icb";
}

/// -d <intensity> decreases contrast and brightness;
/// opencv easy way -> convertTo()
void decrease_contrbright(Mat& img, const double& gain, const int& bias, ostringstream& new_path)
{
    // c'mon, I ran out of hard ways to implement, I'm using the easy way now.
    img.convertTo(img, -1, 1.0/gain, -bias);
    new_path << "_dcb";
}

/// never thought I would live to see the day I would write
/// something like this myself, as a programmer.
void print_usage()
{
    cerr << "Usage: imageProcessor [OPTIONS] FILE\n";

    cerr << "\nProcess images with various transformations and adjustments.\n";

    cerr << "\nTransformations:\n";
    cerr << "  -y              mirror image vertically\n";
    cerr << "  -x              mirror image horizontally\n";

    cerr << "\nColor Adjustments:\n";
    cerr << "  -n              create negative image\n";
    cerr << "  -l DEGREES      rotate by multiples of 90°\n";
    cerr << "  -b GAIN BIAS    increase contrast/brightness (gain: 1.0-10.0, bias: 0-100)\n";
    cerr << "  -d GAIN BIAS    decrease contrast/brightness (gain: 1.0-10.0, bias: 0-100)\n";

    cerr << "\nGeneral:\n";
    cerr << "  -i              show image processing stats\n";
    cerr << "  -h, --help      display this help and exit\n";

    cerr << "Examples:\n";
    cerr << "  imageProcessor -n image.jpg                      # Create negative\n";
    cerr << "  imageProcessor -l 90 -x image.png                # Rotate and flip\n";
    cerr << "  imageProcessor -nxy -l 180 -b 2.0 10 image.bmp   # Chain operations\n\n";

    cerr << "Supported formats: JPG, PNG, BMP, TIFF, WEBP\n";
    cerr << "Output location: ./out/image_transform.jpg, same level as image.jpg\n";
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
            if (b_gain < 1 || b_bias < 0) {
                cerr << "Error: Invalid gain/bias values\n";
                return 1;
            }
            break;
        case 'd':
            d_gain = stod(optarg);
            d_bias = stoi(argv[optind++]);
            if (d_gain < 1 || d_bias < 0) {
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

    Mat img = imread(file_path, IMREAD_UNCHANGED);
    if (img.empty())
    {
        cerr << "Could not read the image: " << file_path << endl;
        cerr << "Supported formats: JPG, PNG, BMP, TIFF\n";
        return 1;
    }

    if (proc_i) {
        cout << "\n====== IMAGE INFO ====== " << endl;
        cout << "Dimensions: " << img.rows << " x " << img.cols << endl;
        cout << "Channels: " << img.channels();
        if (img.channels() == 4) cout << " (with alpha)";
        cout << "." << endl;
        cout << "Image data size: " << img.total() * img.elemSize() / (1024 * 1024) << " MB" << endl;
    }

    ostringstream new_path;
    ostringstream proc_info;

    TickMeter tm_total;
    tm_total.start();

    // actual image processors
    if (ngtv) {
        TickMeter tm;
        tm.start();
        negative(img, new_path);
        tm.stop();
        proc_info << "Negative inversion: " << tm.getTimeMilli() << "ms." << endl;
    }
    if (flip_y) {
        TickMeter tm;
        tm.start();
        flip_vertical(img, new_path);
        tm.stop();
        proc_info << "Flip Vertical: " << tm.getTimeMilli() << "ms." << endl;
    }
    if (flip_x) {
        TickMeter tm;
        tm.start();
        flip_horizontal(img, new_path);
        tm.stop();
        proc_info << "Flip Horizontal: " << tm.getTimeMilli() << "ms." << endl;
    }
    if (rotation != 0) {
        TickMeter tm;
        tm.start();
        rotate(img, rotation, new_path);
        tm.stop();
        proc_info << "Rotate by " << rotation << " degrees: " << tm.getTimeMilli() << "ms." << endl;
    }
    if (b_gain != 1.0 || b_bias != 0) {
        TickMeter tm;
        tm.start();
        increase_contrbright(img, b_gain, b_bias, new_path);
        tm.stop();
        proc_info << "C/B increase: " << tm.getTimeMilli() << "ms." << endl;
    };
    if (d_gain != 1.0 || d_bias != 0) {
        TickMeter tm;
        tm.start();
        decrease_contrbright(img, d_gain , d_bias, new_path);
        tm.stop();
        proc_info << "C/B decrease (parallel): " << tm.getTimeMilli() << "ms." << endl;
    }

    tm_total.stop();

    if (proc_i) {
        cout << "\n====== PERFORMANCE ====== " << endl;
        cout << proc_info.str();
        cout << "Total processing time: " << tm_total.getAvgTimeMilli() << "ms.\n" << endl;
    }

    return save_picture(img, file_path, new_path);
}
