#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <chrono>

const int ditherMatrix[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};

void applyOrderedDithering(cv::Mat& grayImage) {
    int rows = grayImage.rows;
    int cols = grayImage.cols;
    int matrixSize = 4;
    int scale = 256 / (matrixSize * matrixSize + 1);
    
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            int threshold = ditherMatrix[i % matrixSize][j % matrixSize] * scale;
            grayImage.at<uchar>(i, j) = (grayImage.at<uchar>(i, j) > threshold) ? 255 : 0;
        }
    }
}

void applyErrorDiffusionDithering(cv::Mat& grayImage) {
    int rows = grayImage.rows;
    int cols = grayImage.cols;
    cv::Mat errorImage;
    grayImage.convertTo(errorImage, CV_32F);
    
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            float oldPixel = errorImage.at<float>(i, j);
            float newPixel = (oldPixel > 127) ? 255.0f : 0.0f;
            errorImage.at<float>(i, j) = newPixel;
            float quantError = oldPixel - newPixel;
            
            if (j + 1 < cols) errorImage.at<float>(i, j + 1) += quantError * 7.0f / 16.0f;
            if (i + 1 < rows) {
                if (j > 0) errorImage.at<float>(i + 1, j - 1) += quantError * 3.0f / 16.0f;
                errorImage.at<float>(i + 1, j) += quantError * 5.0f / 16.0f;
                if (j + 1 < cols) errorImage.at<float>(i + 1, j + 1) += quantError * 1.0f / 16.0f;
            }
        }
    }
    errorImage.convertTo(grayImage, CV_8U);
}

void processFrame(const cv::Mat& frame, cv::Mat& outputFrame, int blockSize, int threshhold, const cv::Mat& blackImg, const cv::Mat& whiteImg, int ditherMode) {
    int rows = frame.rows / blockSize;
    int cols = frame.cols / blockSize;
    
    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(cols, rows));
    cv::cvtColor(resized, resized, cv::COLOR_BGR2GRAY);
    
    // New Image Dithering
    if (ditherMode == 1) {
        applyOrderedDithering(resized);
    } else if (ditherMode == 2) {
        applyErrorDiffusionDithering(resized);
    }
    
    outputFrame = cv::Mat::zeros(frame.size(), frame.type());
    
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            uchar pixel = resized.at<uchar>(i, j);
            cv::Mat roi = outputFrame(cv::Rect(j * blockSize, i * blockSize, blockSize, blockSize));
            if (pixel < threshhold) {
                blackImg.copyTo(roi);
            } else {
                whiteImg.copyTo(roi);
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 7) {
        std::cout << "Usage: " << argv[0] << " <video_file> <block_size> <threshold> <black_pixel_image> <white_pixel_image> <output_video>" << std::endl;
        return -1;
    }
    
    std::string videoPath = argv[1];
    int blockSize = std::stoi(argv[2]);
	int threshhold = std::stoi(argv[3]);
    std::string blackPixelImage = argv[4];
    std::string whitePixelImage = argv[5];
    std::string outputVideo = argv[6];
    
    int ditherMode = 0;
    std::cout << "Choose dithering mode: \n";
    std::cout << "0: No dithering\n";
    std::cout << "1: Ordered dithering\n";
    std::cout << "2: Error-diffusion dithering\n";
    std::cout << "Enter choice: ";
    std::cin >> ditherMode;
    
    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened()) {
        std::cerr << "Error opening video file." << std::endl;
        return -1;
    }
    
    cv::Mat blackImg = cv::imread(blackPixelImage);
    cv::Mat whiteImg = cv::imread(whitePixelImage);
    
    if (blackImg.empty() || whiteImg.empty()) {
        std::cerr << "Error loading replacement images." << std::endl;
        return -1;
    }
    
    cv::Mat frame, outputFrame;
    
    // Use avc1 codec and output as MP4
    int fourcc = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
    cv::Size frameSize(cap.get(cv::CAP_PROP_FRAME_WIDTH), cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    double fps = cap.get(cv::CAP_PROP_FPS);
    cv::VideoWriter videoWriter(outputVideo, fourcc, fps, frameSize);
    
    if (!videoWriter.isOpened()) {
        std::cerr << "Error: Could not open the output video file for writing." << std::endl;
        return -1;
    }
    
    cv::setNumThreads(4); // Enable multi-threading for performance boost
    int totalFrames = cap.get(cv::CAP_PROP_FRAME_COUNT);
    int currentFrame = 0;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (cap.read(frame)) {
        processFrame(frame, outputFrame, blockSize, threshhold, blackImg, whiteImg, ditherMode);
        videoWriter.write(outputFrame);
        //cv::imshow("Output", outputFrame);
        //if (cv::waitKey(33) == 27) break; // Press ESC to exit
	currentFrame++;
        std::cout << "\rProgress: Frame " << currentFrame << " / " << totalFrames << std::flush;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    std::cout << std::endl;
    
    std::chrono::duration<double> elapsed_time = end_time - start_time;
    std::cout << "Processing completed in " << elapsed_time.count() << " seconds." << std::endl;
    
    double avgfps = totalFrames/elapsed_time.count();
    double timeframe = elapsed_time.count()/totalFrames;
    std::cout << "Average Time per Frame: " << timeframe << " seconds. (" << avgfps << " fps)" << std::endl;
    
    return 0;
}
