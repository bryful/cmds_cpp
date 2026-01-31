#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include "HeaderRename.h"

namespace fs = std::filesystem;

int wmain(int argc, wchar_t* argv[]) {
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    HeaderRename hr;

    fs::path exePath = argv[0];
    fs::path lstPath = exePath.parent_path() / exePath.stem().concat(L".lst");

    if (!hr.LoadWords(lstPath.wstring())) {
        hr.SaveWords(lstPath.wstring());
    }

    fs::path targetPath = (argc < 2) ? fs::current_path() : fs::path(argv[1]);

    if (!fs::exists(targetPath)) {
        std::wcout << L"Error: Path not found." << std::endl;
        return 1;
    }

    std::vector<fs::path> items;
    for (const auto& entry : fs::directory_iterator(targetPath)) {
        items.push_back(entry.path());
    }

    for (const auto& p : items) {
        if (fs::is_regular_file(p)) {
            std::wstring ext = toLowwer(p.extension().wstring());
            if (ext == L".zip" || ext == L".rar" || ext == L".7z") {
                hr.Rename(p);
            }
        }
        else if (fs::is_directory(p)) {
            hr.Rename(p);
        }
    }

    return 0;
}