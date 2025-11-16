#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include <iostream>
#include <string>

using namespace std;
using namespace cv;

// Using OpenCv
// extract a single color channel from image
// args:
// file_name
// channel_number (0 = R, 1 = G, 2 = B)
int main(const int argc, char *argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: colorChannel file_path channel_number{ 0 | 1 | 2 }\n";
        cerr << "    channel_number (0 = B, 1 = G, 2 = R)\n";
        return 1;
    }

    const string file_path = argv[2];
    const int channel_number = atoi(argv[1]);
    const Mat img = imread(file_path, IMREAD_COLOR);

    if (img.empty())
    {
        cout << "Could not read the image: " << file_path << endl;
        return 1;
    }

    Mat channel;
    const string extension = file_path.substr(file_path.find_last_of('.') + 1);
    const string new_path = file_path + "_" + to_string(channel_number) + "." + extension;

    TickMeter tm;
    tm.start();
    extractChannel(img, channel, channel_number);
    tm.stop();

    imshow("Display window", channel);
    imwrite(new_path, channel);

    cout << tm.getTimeMilli() << " ms" << endl;

    return 0;
}
