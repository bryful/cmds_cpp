#include "fsort.h"

int wmain(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    try {
        bool reverse = false;
        fs::path root;
        if (argc == 1) root = fs::current_path();
        else if (argc == 2 && argv[1][0] != L'-' && argv[1][0] != L'/') root = argv[1];
        else if (argc == 3 && (Equal(argv[1], L"-r") || Equal(argv[1], L"-rev") ||
            Equal(argv[1], L"/r") || Equal(argv[1], L"/rev"))) { reverse = true; root = argv[2]; }
        else { std::cerr << "Usage: fs [<directory>] | fs -r <directory>\n"; return 1; }
        root = fs::absolute(root).lexically_normal();
        while (root.filename().empty() && root != root.root_path()) root = root.parent_path();
        ValidateRoot(root);
        const auto result = Sort(root, reverse);
        std::cout << "Moved: " << result.moved << ", skipped: " << result.skipped << ", failed: " << result.failed << '\n';
        return result.failed ? 1 : 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
