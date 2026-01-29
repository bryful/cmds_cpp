#include <iostream>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>
#include <cstdio>
#include <algorithm>
#include <cctype>
#include <webp/decode.h>
#include <fstream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <Windows.h>

namespace fs = std::filesystem;

std::wstring NormalizePathUnicode(const std::wstring& path) {
    int len = NormalizeString(NormalizationC, path.c_str(), -1, NULL, 0);
    if (len <= 0) return path;
    std::wstring normalized(len, 0);
    NormalizeString(NormalizationC, path.c_str(), -1, &normalized[0], len);
    // 末尾のnull文字を削除
    if (!normalized.empty() && normalized.back() == L'\0') {
        normalized.pop_back();
    }
    return normalized;
}

bool ConvertWebpToJpeg_Stb(const fs::path & webpPath, const fs::path& jpegPath, int quality = 75) {
    // WebPファイルをバイナリで読み込む
    std::ifstream file(webpPath, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "WebPファイルが開けません: " << webpPath.filename().string() << std::endl;
        return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "WebPファイルの読み込み失敗: " << webpPath.filename().string() << std::endl;
        return false;
    }
    file.close();

    // WebPデコード（RGBで取得）
    int width = 0, height = 0;
    uint8_t* rgb = WebPDecodeRGB(buffer.data(), buffer.size(), &width, &height);
    if (!rgb) {
        std::cerr << "WebPデコード失敗: " << webpPath.filename().string() << std::endl;
        return false;
    }

    // std::ofstream を使用して jpegPath に直接書き込む
    std::ofstream outFile(jpegPath, std::ios::binary);
    if (!outFile) {
        std::cerr << "JPEG書き込み失敗: ファイルを開けません" << std::endl;
        WebPFree(rgb);
        return false;
    }

    // stbi_write_jpg_to_func を使用してメモリに書き込み、その後ファイルに保存
    auto write_func = [](void* context, void* data, int size) {
        std::ofstream* stream = static_cast<std::ofstream*>(context);
        stream->write(static_cast<const char*>(data), size);
    };

    int ok = stbi_write_jpg_to_func(write_func, &outFile, width, height, 3, rgb, quality);
    WebPFree(rgb);
    outFile.close();

    if (!ok) {
        std::cerr << "JPEG書き込み失敗" << std::endl;
        return false;
    }
    else {
        try {
            if (std::filesystem::remove(webpPath)) {
                // 削除成功
            }
            else {
                return false;
            }
            std::cout << "変換完了: " << webpPath.filename().string() << " -> " << jpegPath.filename().string() << std::endl;
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "削除エラー: " << webpPath.filename().string() <<" - " << e.what() << std::endl;
            return false;
        }
    }
    return true;
}

void Usage() {
    std::cout << "Usage: wp <input.webp> " << std::endl;
    std::cout << "Example: wp image.webp" << std::endl;
}

fs::path ConvertImgiToImgJpeg(const fs::path& webpName) {
    std::string fn = webpName.filename().string();
    
    // null文字を削除
    fn.erase(std::remove(fn.begin(), fn.end(), '\0'), fn.end());
    
    std::regex re(R"(imgi_(\d+)_(\d+)\.webp)", std::regex::icase);
    std::smatch m;
    
    if (std::regex_match(fn, m, re) && m[1] == m[2]) {
        int n = std::stoi(m[1]);
        char buf[32];
        std::snprintf(buf, sizeof(buf), "img%03d.jpeg", n);
        
        fs::path ret = webpName;
        return ret.replace_filename(buf);
    }
    
    // マッチしない場合は拡張子だけjpegに（コピーを作成）
    fs::path result = webpName;
    return result.replace_extension(".jpeg");
}

bool ConvertWebpToJpeg_Stb(const fs::path& path) {
    fs::path jpegPath = ConvertImgiToImgJpeg(path);
    return ConvertWebpToJpeg_Stb(path, jpegPath);
}

// ファイルパスを末尾の数字でソートする関数
void SortByTrailingNumber(std::vector<fs::path>& files) {
    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
        std::string sa = a.filename().string();
        std::string sb = b.filename().string();
        
        // ★null文字を削除
        sa.erase(std::remove(sa.begin(), sa.end(), '\0'), sa.end());
        sb.erase(std::remove(sb.begin(), sb.end(), '\0'), sb.end());
        
        // ファイル名から末尾の数字を抽出（拡張子の前）
        std::regex re(R"((\d+)(?:\.\w+)?$)");
        std::smatch ma, mb;

        int na = 0, nb = 0;
        if (std::regex_search(sa, ma, re)) {
            na = std::stoi(ma[1]);
        }
        if (std::regex_search(sb, mb, re)) {
            nb = std::stoi(mb[1]);
        }
        
        // デバッグ出力（確認後削除可能）
        // std::cout << "比較: [" << sa << "] → " << na << " vs [" << sb << "] → " << nb << std::endl;
        
        return na < nb; // 昇順
    });
}

std::vector<fs::path> GetFilesWithExtensions(const fs::path& dir, const std::vector<std::string>& exts) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;

        std::wstring normalized = NormalizePathUnicode(entry.path().wstring());
        fs::path normalizedPath(normalized);

        std::string ext = normalizedPath.extension().string();
        
        // ★null文字を全て削除
        ext.erase(std::remove(ext.begin(), ext.end(), '\0'), ext.end());
        
        // 小文字変換
        for (char& c : ext) {
            c = (char)::tolower((unsigned char)c);
        }
        
        for (const auto& e : exts) {
            if (ext == e) {
                files.push_back(normalizedPath);
                break;
            }
        }
    }
    SortByTrailingNumber(files);
    return files;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        Usage();
        return 1;
    }
    fs::path webpPath = fs::path(argv[1]);
    if (fs::is_regular_file(webpPath)) {
        ConvertWebpToJpeg_Stb(webpPath);
    }
    else if (fs::is_directory(webpPath)) {
        std::vector<std::string> exts = { ".webp"};
        std::vector<fs::path> lst = GetFilesWithExtensions(webpPath, exts);
        for (const auto& f : lst) {
            ConvertWebpToJpeg_Stb(f);
        }
    }
    else {
        Usage();
        return 1;
    }
    return 0;
}
