#include "colormap_generator.hpp"

#include <iostream>

#include <opencv4/opencv2/core/mat.hpp>
#include <opencv4/opencv2/imgproc.hpp>

bool hex2BGR(const std::string& hex, cv::Scalar& dst) {
    std::string s = hex;

    if (s.size() >= 1 && s[0] == '#') s = s.substr(1);
    else if (s.size() >= 2 && s.substr(0, 2) == "0x") s = s.substr(2);

    if(s.size() != 6) {
        dst = cv::Scalar(0, 0, 0);
        return false;
    }

    auto hexToInt = [](const std::string& h) {
        return static_cast<uchar>(std::stoi(h, nullptr, 16));
    };

    uchar r = hexToInt(s.substr(0, 2));
    uchar g = hexToInt(s.substr(2, 2));
    uchar b = hexToInt(s.substr(4, 2));

    dst = cv::Scalar(b, g, r);
    return true;
}

bool convertHexToBGR(const std::string& hex, int &b, int &g, int &r) {
    cv::Scalar color;
    if(!hex2BGR(hex, color)) {
        b = g = r = 0;
        return false;
    }
    b = static_cast<int>(color[0]);
    g = static_cast<int>(color[1]);
    r = static_cast<int>(color[2]);
    return true;
}

cv::Scalar BGR2Lab(const cv::Scalar& bgr) {
    cv::Mat bgrMat(1, 1, CV_32FC3);
    bgrMat.at<cv::Vec3f>(0, 0) = cv::Vec3f(bgr[0] / 255.0, bgr[1] / 255.0, bgr[2] / 255.0);
    cv::Mat labMat;
    cv::cvtColor(bgrMat, labMat, cv::COLOR_BGR2Lab);
    CV_Assert(labMat.type() == CV_32FC3);
    cv::Vec3f labPixel = labMat.at<cv::Vec3f>(0, 0);
    return cv::Scalar(labPixel[0], labPixel[1], labPixel[2]);
}

void generateBinaryColorMap(const cv::Mat& src, int threshold, ColorMap& colorMap, cv::Mat& viewMap) {
    if (src.empty() || src.channels() != 3) {
        std::cerr << "Input image is empty or not a 3-channel BGR image." << std::endl;
        return;
    }

    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);

    cv::Mat binary;
    cv::threshold(gray, binary, threshold, 255, cv::THRESH_BINARY);

    colorMap.colorMap = binary;
    colorMap.MapOfColor.clear();
    colorMap.MapOfColor[0] = (binary == 255);
    colorMap.MapOfColorName[0] = "white";
    colorMap.MapOfColorValueBGR[0] = cv::Scalar(255, 255, 255);
    colorMap.MapOfColor[1] = (binary == 0);
    colorMap.MapOfColorName[1] = "black";
    colorMap.MapOfColorValueBGR[1] = cv::Scalar(0, 0, 0);

    // Create view map for visualization
    cv::cvtColor(binary, viewMap, cv::COLOR_GRAY2BGR);
}

void generateMultiColorMap(const cv::Mat& src, Colors& colors, ColorMap& colorMap, cv::Mat& viewMap) {
    if (src.empty() || src.channels() != 3) {
        std::cerr << "Input image is empty or not a 3-channel BGR image." << std::endl;
        return;
    }
    if (colors.size() < 2) {
        std::cerr << "At least two colors are required to generate a multi-color map." << std::endl;
        return;
    }

    // 正規化
    cv::Mat floatMat;
    src.convertTo(floatMat, CV_32FC3, 1.0 / 255.0);

    // Lab 変換
    cv::Mat labMat;
    cv::cvtColor(floatMat, labMat, cv::COLOR_BGR2Lab);
    CV_Assert(labMat.type() == CV_32FC3);

    // HLS 変換
    cv::Mat hlsMat;
    cv::cvtColor(floatMat, hlsMat, cv::COLOR_BGR2HLS);
    CV_Assert(hlsMat.type() == CV_32FC3);

    // colors4print の準備
    std::vector<std::string> colorNames;
    std::vector<cv::Scalar> colorValuesBGR;
    std::vector<cv::Scalar> colorValuesLab;
    for (const auto& [name, hex] : colors) {
        cv::Scalar bgrColor;
        hex2BGR(hex, bgrColor);
        cv::Scalar labColor = BGR2Lab(bgrColor);
        colorNames.push_back(name);
        colorValuesBGR.push_back(bgrColor);
        colorValuesLab.push_back(labColor);
    }

    // 出力マップ準備
    cv::Mat indexMap(src.size(), CV_8UC1, cv::Scalar(255)); // 255 = 未分類
    cv::Mat minDist(src.size(), CV_32F, cv::Scalar(FLT_MAX));
    //cv::Mat achroMask; // achroの時のみ使用

    cv::Mat dist(labMat.size(), CV_32F);

    for (size_t i = 0; i < colorValuesLab.size(); ++i) {

        cv::Vec3f c((float)colorValuesLab[i][0],
                    (float)colorValuesLab[i][1],
                    (float)colorValuesLab[i][2]);

        cv::parallel_for_(cv::Range(0, labMat.rows), [&](const cv::Range& range) {
            for (int y = range.start; y < range.end; y++) {
                const cv::Vec3f* src = labMat.ptr<cv::Vec3f>(y);
                float* dptr = dist.ptr<float>(y);
                for (int x = 0; x < labMat.cols; x++) {
                    cv::Vec3f v = src[x] - c;
                    dptr[x] = v.dot(v);
                }
            }
        });

        cv::Mat mask = dist < minDist;
        dist.copyTo(minDist, mask);
        indexMap.setTo((uchar)i, mask);
    }

    viewMap = cv::Mat::zeros(src.size(), CV_8UC3);
    viewMap.setTo(cv::Scalar(255, 255, 255));

    colorMap.colorMap = indexMap;
    colorMap.MapOfColor.clear();
    colorMap.MapOfColorName.clear();

    for (size_t i = 0; i < colorValuesBGR.size(); ++i) {
        cv::Mat mask = (indexMap == (uchar)i);
        if(cv::countNonZero(mask) > 0) {
            viewMap.setTo(colorValuesBGR[i], mask);
            colorMap.MapOfColor[i] = mask;
            colorMap.MapOfColorName[i] = colorNames[i];
        }
    }
}

void generateAchroColorMap(const cv::Mat& src, float achro_sensitivity, std::vector<float>& achro_thresholds,
    Colors& achro_colors, Colors& colors, ColorMap& colorMap, cv::Mat& viewMap) {

    if (src.empty() || src.channels() != 3) {
        std::cerr << "Input image is empty or not a 3-channel BGR image." << std::endl;
        return;
    }
    if (achro_colors.size() < 2) {
        std::cerr << "At least two achromatic colors are required." << std::endl;
        return;
    }

    // 正規化
    cv::Mat floatMat;
    src.convertTo(floatMat, CV_32FC3, 1.0 / 255.0);

    // Lab 変換
    cv::Mat labMat;
    cv::cvtColor(floatMat, labMat, cv::COLOR_BGR2Lab);
    CV_Assert(labMat.type() == CV_32FC3);

    // HLS 変換
    cv::Mat hlsMat;
    cv::cvtColor(floatMat, hlsMat, cv::COLOR_BGR2HLS);
    CV_Assert(hlsMat.type() == CV_32FC3);

    // colors4print の準備
    std::vector<std::string> colorNames;
    std::vector<cv::Scalar> colorValuesBGR;
    std::vector<cv::Scalar> colorValuesLab;
    for (const auto& [name, hex] : colors) {
        cv::Scalar bgrColor;
        hex2BGR(hex, bgrColor);
        cv::Scalar labColor = BGR2Lab(bgrColor);
        colorNames.push_back(name);
        colorValuesBGR.push_back(bgrColor);
        colorValuesLab.push_back(labColor);
    }

    // 出力マップ準備
    cv::Mat indexMap(src.size(), CV_8UC1, cv::Scalar(255)); // 255 = 未分類
    cv::Mat minDist(src.size(), CV_32F, cv::Scalar(FLT_MAX));
    cv::Mat achroMask;

    // 無彩色検出
    cv::Mat L_hls, S_cyl, L_lab;
    cv::extractChannel(hlsMat, L_hls, 1); // 0:H, 1:L, 2:S
    cv::extractChannel(hlsMat, S_cyl, 2);
    cv::extractChannel(labMat, L_lab, 0);

    // 双円錐型のS
    cv::Mat S_double = S_cyl.mul(1.0f - cv::abs(L_hls * 2.0f - 1.0f));

    // 無彩色マスク
    achroMask = (S_double < achro_sensitivity);

    int achro_offset = achro_colors.size();

    // 黒・灰・白分類
    if(achro_offset == 2) {
        cv::Mat blackMask = achroMask & (L_lab < achro_thresholds[0]);
        cv::Mat whiteMask = achroMask & (L_lab >= achro_thresholds[0]);
        indexMap.setTo((uchar)0, blackMask);
        indexMap.setTo((uchar)1, whiteMask);
        colorMap.MapOfColor[0] = blackMask;
        colorMap.MapOfColorName[0] = achro_colors[0].first;
        colorMap.MapOfColor[1] = whiteMask;
        colorMap.MapOfColorName[1] = achro_colors[1].first;
    } else if(achro_offset == 3) {
        cv::Mat blackMask = achroMask & (L_lab < achro_thresholds[0]);
        cv::Mat grayMask = achroMask & (L_lab >= achro_thresholds[0]) & (L_lab < achro_thresholds[1]);
        cv::Mat whiteMask = achroMask & (L_lab >= achro_thresholds[1]);
        indexMap.setTo((uchar)0, blackMask);
        indexMap.setTo((uchar)1, grayMask);
        indexMap.setTo((uchar)2, whiteMask);
        colorMap.MapOfColor[0] = blackMask;
        colorMap.MapOfColorName[0] = achro_colors[0].first;
        colorMap.MapOfColor[1] = grayMask;
        colorMap.MapOfColorName[1] = achro_colors[1].first;
        colorMap.MapOfColor[2] = whiteMask;
        colorMap.MapOfColorName[2] = achro_colors[2].first;
    } else if(achro_offset == 4) {
        cv::Mat blackMask = achroMask & (L_lab < achro_thresholds[0]);
        cv::Mat darkGrayMask = achroMask & (L_lab >= achro_thresholds[0]) & (L_lab < achro_thresholds[1]);
        cv::Mat lightGrayMask = achroMask & (L_lab >= achro_thresholds[1]) & (L_lab < achro_thresholds[2]);
        cv::Mat whiteMask = achroMask & (L_lab >= achro_thresholds[2]);
        indexMap.setTo((uchar)0, blackMask);
        indexMap.setTo((uchar)1, darkGrayMask);
        indexMap.setTo((uchar)2, lightGrayMask);
        indexMap.setTo((uchar)3, whiteMask);
        colorMap.MapOfColor[0] = blackMask;
        colorMap.MapOfColorName[0] = achro_colors[0].first;
        colorMap.MapOfColor[1] = darkGrayMask;
        colorMap.MapOfColorName[1] = achro_colors[1].first;
        colorMap.MapOfColor[2] = lightGrayMask;
        colorMap.MapOfColorName[2] = achro_colors[2].first;
        colorMap.MapOfColor[3] = whiteMask;
        colorMap.MapOfColorName[3] = achro_colors[3].first;
    } else {
        std::cerr << "Unsupported number of achromatic colors: " << achro_offset << std::endl;
        return;
    }

    cv::Mat dist(labMat.size(), CV_32F);

    for (size_t i = 0; i < colorValuesLab.size(); ++i) {
        cv::Vec3f c((float)colorValuesLab[i][0],
                    (float)colorValuesLab[i][1],
                    (float)colorValuesLab[i][2]);

        cv::parallel_for_(cv::Range(0, labMat.rows), [&](const cv::Range& range) {
            for (int y = range.start; y < range.end; y++) {
                const cv::Vec3f* src = labMat.ptr<cv::Vec3f>(y);
                float* dptr = dist.ptr<float>(y);
                for (int x = 0; x < labMat.cols; x++) {
                    cv::Vec3f v = src[x] - c;
                    dptr[x] = v.dot(v);
                }
            }
        });

        cv::Mat mask = dist < minDist;
        mask &= ~achroMask;
        dist.copyTo(minDist, mask);
        indexMap.setTo((uchar)(i + achro_offset), mask);
    }

    viewMap = cv::Mat::zeros(src.size(), CV_8UC3);
    viewMap.setTo(cv::Scalar(255, 255, 255));

    for(size_t i = 0; i < achro_offset; ++i) {
        cv::Scalar bgrColor;
        hex2BGR(achro_colors[i].second, bgrColor);
        viewMap.setTo(bgrColor, colorMap.MapOfColor[i]);
        colorMap.MapOfColorValueBGR[i] = bgrColor;
    }

    for(size_t i = 0; i < colorValuesBGR.size(); ++i) {
        cv::Mat mask = (indexMap == (uchar)(i + achro_offset));
        viewMap.setTo(colorValuesBGR[i], mask);
        colorMap.MapOfColor[i + achro_offset] = mask;
        colorMap.MapOfColorName[i + achro_offset] = colorNames[i];
        colorMap.MapOfColorValueBGR[i + achro_offset] = colorValuesBGR[i];
    }

    // set ColorMap.colorMap
    colorMap.colorMap = cv::Mat::zeros(src.size(), CV_8UC1);
    for(const auto& [id, mask] : colorMap.MapOfColor) {
        colorMap.colorMap.setTo((uchar)id, mask);
    }
}
