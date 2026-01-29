// RenSeq.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <regex>
#include <algorithm>

namespace fs = std::filesystem;

class Seq
{
private:
    struct NumberInfo
    {
        std::string prefix;
        std::string number;
        bool hasNumber;
    };

    // 末尾の数字を抽出
    static NumberInfo ExtractTrailingNumber(const std::string& name)
    {
        NumberInfo info{ name, "", false };

        if (name.empty())
            return info;

        size_t i = name.length();
        
        // 末尾から数字を探す
        while (i > 0 && std::isdigit(name[i - 1]))
        {
            i--;
        }

        // 数字が見つからなかった場合
        if (i == name.length())
        {
            return info;
        }

        // 全体が数字の場合
        if (i == 0)
        {
            info.prefix = "";
            info.number = name;
            info.hasNumber = true;
            return info;
        }

        // プレフィックスと数字部分に分割
        info.prefix = name.substr(0, i);
        info.number = name.substr(i);
        info.hasNumber = true;

        return info;
    }

public:
    /// <summary>
    /// ファイル名末尾の数字を保持したまま、それ以外の部分をリネーム
    /// </summary>
    /// <param name="filePath">リネームするファイルのフルパス</param>
    /// <param name="newPrefix">新しいプレフィックス（数字以外の文字列）</param>
    /// <returns>成功時は新しいファイルパス、失敗時は空文字列</returns>
    static std::string RenameWithNumberPreserve(const std::string& filePath, const std::string& newPrefix)
    {
        fs::path path(filePath);

        if (!fs::exists(path))
        {
            std::cerr << "Error: File not found - " << filePath << std::endl;
            return "";
        }

        fs::path directory = path.parent_path();
        std::string fileName = path.stem().string();
        std::string extension = path.extension().string();

        // 末尾の数字を抽出
        auto info = ExtractTrailingNumber(fileName);

        if (!info.hasNumber)
        {
            std::cerr << "Error: No trailing number found in - " << fileName << std::endl;
            return "";
        }

        // 新しいファイル名を構築
        std::string newFileName = newPrefix + info.number + extension;
        fs::path newFilePath = directory / newFileName;

        // 同名ファイルが存在する場合の処理
        if (fs::exists(newFilePath))
        {
            std::cerr << "Error: File already exists - " << newFileName << std::endl;
            return "";
        }

        try
        {
            fs::rename(path, newFilePath);
            std::cout << "Renamed: " << path.filename().string() << " -> " << newFileName << std::endl;
            return newFilePath.string();
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Error renaming file: " << ex.what() << std::endl;
            return "";
        }
    }

    /// <summary>
    /// 複数のファイルを一括リネーム
    /// </summary>
    /// <param name="filePaths">リネームするファイルのパス配列</param>
    /// <param name="newPrefix">新しいプレフィックス</param>
    /// <returns>リネームに成功したファイル数</returns>
    static int RenameMultipleFiles(const std::vector<std::string>& filePaths, const std::string& newPrefix)
    {
        int successCount = 0;
        for (const auto& filePath : filePaths)
        {
            if (!RenameWithNumberPreserve(filePath, newPrefix).empty())
            {
                successCount++;
            }
        }
        return successCount;
    }

    /// <summary>
    /// ディレクトリ内のファイルを一括リネーム（自然順ソート）
    /// </summary>
    /// <param name="directoryPath">対象ディレクトリ</param>
    /// <param name="pattern">検索パターン（例: ".tga"）</param>
    /// <param name="newPrefix">新しいプレフィックス</param>
    /// <returns>リネームに成功したファイル数</returns>
    static int RenameFilesInDirectory(const std::string& directoryPath, const std::string& pattern, const std::string& newPrefix)
    {
        fs::path dirPath(directoryPath);

        if (!fs::exists(dirPath) || !fs::is_directory(dirPath))
        {
            std::cerr << "Error: Directory not found - " << directoryPath << std::endl;
            return 0;
        }

        std::vector<std::string> files;

        // ファイルを収集
        for (const auto& entry : fs::directory_iterator(dirPath))
        {
            if (entry.is_regular_file())
            {
                std::string ext = entry.path().extension().string();
                if (pattern == "*" || ext == pattern)
                {
                    files.push_back(entry.path().string());
                }
            }
        }

        // 自然順ソート
        std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
            return NaturalCompare(a, b) < 0;
        });

        return RenameMultipleFiles(files, newPrefix);
    }

private:
    // 自然順比較（数字を数値として比較）
    static int NaturalCompare(const std::string& a, const std::string& b)
    {
        size_t posA = 0, posB = 0;

        while (posA < a.length() && posB < b.length())
        {
            bool isDigitA = std::isdigit(a[posA]);
            bool isDigitB = std::isdigit(b[posB]);

            if (isDigitA && !isDigitB) return -1;
            if (!isDigitA && isDigitB) return 1;

            if (isDigitA && isDigitB)
            {
                // 数字として比較
                long long numA = 0, numB = 0;
                while (posA < a.length() && std::isdigit(a[posA]))
                {
                    numA = numA * 10 + (a[posA] - '0');
                    posA++;
                }
                while (posB < b.length() && std::isdigit(b[posB]))
                {
                    numB = numB * 10 + (b[posB] - '0');
                    posB++;
                }

                if (numA != numB)
                    return (numA < numB) ? -1 : 1;
            }
            else
            {
                // 文字として比較
                if (a[posA] != b[posB])
                    return (a[posA] < b[posB]) ? -1 : 1;
                posA++;
                posB++;
            }
        }

        return (a.length() < b.length()) ? -1 : (a.length() > b.length()) ? 1 : 0;
    }
};

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "RenSeq - ファイル名の数字以外をリネーム\n";
        std::cout << "使い方:\n";
        std::cout << "  単一ファイル: renSeq <ファイルパス> <新しいプレフィックス>\n";
        std::cout << "  ディレクトリ: renSeq -d <ディレクトリパス> <拡張子> <新しいプレフィックス>\n";
        std::cout << "\n例:\n";
        std::cout << "  renSeq AAA_0001.tga BBB_\n";
        std::cout << "  renSeq -d C:\\temp .tga NewName_\n";
        return 1;
    }

    std::string arg1 = argv[1];

    if (arg1 == "-d" && argc >= 5)
    {
        // ディレクトリモード
        std::string dirPath = argv[2];
        std::string pattern = argv[3];
        std::string newPrefix = argv[4];

        int count = Seq::RenameFilesInDirectory(dirPath, pattern, newPrefix);
        std::cout << "\n" << count << " ファイルをリネームしました。" << std::endl;
    }
    else if (argc >= 3)
    {
        // 単一ファイルモード
        std::string filePath = argv[1];
        std::string newPrefix = argv[2];

        std::string result = Seq::RenameWithNumberPreserve(filePath, newPrefix);
        if (!result.empty())
        {
            std::cout << "成功: " << result << std::endl;
        }
    }

    return 0;
}
