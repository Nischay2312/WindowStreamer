#include <opencv2/opencv.hpp>
#include <websocketpp/client.hpp>
#include <windows.h>
#include <iostream>

void convert(uint16_t x, uint8_t &c, uint8_t &f) {
    c = (x >> 8) & 0xff; // Coarse
    f = x & 0xff;       // Fine
}

void SendData(const std::vector<uint8_t> &data, websocketpp::client &ws) {
    constexpr size_t chunk_size = 10000;
    ws.send("1");
    for (size_t i = 0; i < data.size(); i += chunk_size) {
        ws.send_binary(data.data() + i, std::min(chunk_size, data.size() - i));
    }
    ws.send("0");
}

int main() {
    RECT bounding_box = {0, 0, 1920, 1080};
    int frame = 0;
    double time_start = cv::getTickCount();
    websocketpp::client ws;
    ws.connect("ws://192.168.1.3:81/");

    while (true) {
        double time_process = cv::getTickCount();
        // Get frame
        cv::Mat sct_img;
        // Capture screen using Windows API (replace with suitable method for your platform)
        HWND desktop = GetDesktopWindow();
        HDC desktop_dc = GetDC(desktop);
        HDC my_dc = CreateCompatibleDC(desktop_dc);
        HBITMAP my_bmp = CreateCompatibleBitmap(desktop_dc, bounding_box.right, bounding_box.bottom);
        SelectObject(my_dc, my_bmp);
        BitBlt(my_dc, 0, 0, bounding_box.right, bounding_box.bottom, desktop_dc, 0, 0, SRCCOPY);
        DeleteDC(my_dc);
        DeleteDC(desktop_dc);
        sct_img = cv::Mat(bounding_box.bottom, bounding_box.right, CV_8UC4, my_bmp);

        // Process frame
        cv::resize(sct_img, sct_img, cv::Size(160, 128), 0, 0, cv::INTER_AREA);
        cv::cvtColor(sct_img, sct_img, cv::COLOR_BGR2RGB);
        std::cout << "Time to Process frame: " << (cv::getTickCount() - time_process) * 1000 / cv::getTickFrequency() << "ms\n";

        // Convert to RGB565
        time_process = cv::getTickCount();
        cv::cvtColor(sct_img, sct_img, cv::COLOR_BGR2BGR565);
        std::vector<uint8_t> Eight_BitData(sct_img.rows * sct_img.cols * 2);
        for (int i = 0; i < sct_img.rows; ++i) {
            for (int j = 0; j < sct_img.cols; ++j) {
                uint16_t pixel = sct_img.at<uint16_t>(i, j);
                uint8_t c, f;
                convert(pixel, c, f);
                Eight_BitData[i * sct_img.cols * 2 + j * 2] = c;
                Eight_BitData[i * sct_img.cols * 2 + j * 2 + 1] = f;
            }
        }

        std::cout << "Time to Convert frame: " << (cv::getTickCount() - time_process) * 1000 / cv::getTickFrequency() << "ms\n";

        // Send Data
        time_process = cv::getTickCount();
        SendData(Eight_BitData, ws);
        std::cout << "Time to Send frame: " << (cv::getTickCount() - time_process) * 1000 / cv::getTickFrequency() << "ms\n";

        cv::imshow("screen", sct_img);
        if (frame > 100) {
            std::cout << "FPS: " << frame / ((cv::getTickCount() - time_start) / cv::getTickFrequency()) << '\n';
            frame = 0;
            time_start = cv::getTickCount();
        }

        if (cv::waitKey(1) == 'q') {
            cv::destroyAllWindows();
            break;
        }
    }

    return 0;
}
