#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include "BracketRename.h"

namespace fs = std::filesystem;

int wmain(int argc, wchar_t* argv[]) {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    fs::path targetPath = (argc < 2) ? fs::current_path() : fs::path(argv[1]);

    if (!fs::exists(targetPath)) {
        std::wcout << L"Error: Path not found." << std::endl;
        return 1;
    }

    fs::path targetDir = fs::is_directory(targetPath) ? targetPath : targetPath.parent_path();

    BracketRename renovator;
    renovator.InitLog(targetDir);

    std::vector<fs::path> items;
    if (fs::is_directory(targetPath)) {
        for (const auto& entry : fs::directory_iterator(targetPath)) {
            items.push_back(entry.path());
        }
    } else {
        items.push_back(targetPath);
    }

    for (const auto& p : items) {
        if (fs::is_regular_file(p)) {
            std::wstring ext = toLower(p.extension().wstring());
            if (ext == L".zip" || ext == L".rar" || ext == L".7z" || ext == L".mp4" || ext == L".mov" || ext == L".mpg" || ext == L".mpeg") {
                renovator.Rename(p);
            }
        }
        else if (fs::is_directory(p)) {
            renovator.Rename(p);
        }
    }

    return 0;
}