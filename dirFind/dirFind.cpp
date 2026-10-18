#include <iostream>
#include <filesystem>
#include <string>
#include <windows.h>

namespace fs = std::filesystem;

// UTF-16 -> UTF-8
std::string WideToUtf8(const std::wstring& src)
{
	if (src.empty())
		return std::string();

	int size = WideCharToMultiByte(
		CP_UTF8,
		0,
		src.c_str(),
		static_cast<int>(src.size()),
		nullptr,
		0,
		nullptr,
		nullptr
	);

	if (size <= 0)
		return std::string();

	std::string result(size, '\0');

	WideCharToMultiByte(
		CP_UTF8,
		0,
		src.c_str(),
		static_cast<int>(src.size()),
		result.data(),
		size,
		nullptr,
		nullptr
	);

	return result;
}

std::string PathToUtf8(const fs::path& path)
{
	return WideToUtf8(path.wstring());
}

int wmain(int argc, wchar_t* argv[])
{
	SetConsoleOutputCP(CP_UTF8);

	//
	// 第1引数: 検索文字列（必須）
	//
	if (argc < 2)
	{
		std::cerr
			<< "Usage: dirFind.exe \"search string\" [path]"
			<< std::endl;

		return 1;
	}

	std::wstring searchText = argv[1];

	if (searchText.empty())
	{
		std::cerr
			<< "ERROR: Search string is empty."
			<< std::endl;

		return 1;
	}

	//
	// 第2引数: 検索パス
	// 省略時はカレントディレクトリ
	//
	fs::path rootPath;

	if (argc >= 3)
	{
		rootPath = fs::path(argv[2]);
	}
	else
	{
		rootPath = fs::current_path();
	}

	std::error_code ec;

	rootPath = fs::absolute(rootPath, ec);

	if (ec)
	{
		std::cerr
			<< "ERROR: Cannot get absolute path: "
			<< ec.message()
			<< std::endl;

		return 1;
	}

	if (!fs::exists(rootPath, ec))
	{
		std::cerr
			<< "ERROR: Path does not exist: "
			<< PathToUtf8(rootPath)
			<< std::endl;

		return 1;
	}

	if (!fs::is_directory(rootPath, ec))
	{
		std::cerr
			<< "ERROR: Not a directory: "
			<< PathToUtf8(rootPath)
			<< std::endl;

		return 1;
	}

	std::cerr
		<< "Search: "
		<< WideToUtf8(searchText)
		<< std::endl;

	std::cerr
		<< "Scan root: "
		<< PathToUtf8(rootPath)
		<< std::endl;

	std::size_t dirCount = 0;
	std::size_t fileCount = 0;
	std::size_t foundCount = 0;
	bool scanFailed = false;

	std::error_code iterEc;

	fs::recursive_directory_iterator it(
		rootPath,
		fs::directory_options::none,
		iterEc
	);

	fs::recursive_directory_iterator end;

	if (iterEc)
	{
		std::cerr
			<< "ERROR: Cannot open directory: "
			<< iterEc.message()
			<< std::endl;

		return 1;
	}

	while (it != end)
	{
		const fs::directory_entry& entry = *it;

		std::error_code typeEc;

		//
		// シンボリックリンク・ジャンクション等には入らない
		//
		bool isSymlink = entry.is_symlink(typeEc);

		if (!typeEc && isSymlink)
		{
			typeEc.clear();

			if (entry.is_directory(typeEc) && !typeEc)
			{
				it.disable_recursion_pending();
			}
		}

		typeEc.clear();

		if (entry.is_directory(typeEc) && !typeEc)
		{
			++dirCount;

			//
			// 進捗は標準エラー出力
			//
			std::cerr
				<< "["
				<< dirCount
				<< "] "
				<< PathToUtf8(entry.path())
				<< std::endl;
		}
		else
		{
			typeEc.clear();

			if (entry.is_regular_file(typeEc) && !typeEc)
			{
				++fileCount;

				//
				// ファイル名だけを検索対象にする
				//
				const std::wstring fileName =
					entry.path().filename().wstring();

				//
				// 大文字・小文字を無視して部分一致
				//
				if (FindStringOrdinal(FIND_FROMSTART,
					fileName.c_str(), static_cast<int>(fileName.size()),
					searchText.c_str(), static_cast<int>(searchText.size()), TRUE) >= 0)
				{
					++foundCount;

					std::error_code relEc;

					fs::path relativePath =
						fs::relative(
							entry.path(),
							rootPath,
							relEc
						);

					if (relEc)
					{
						relativePath = entry.path();
					}

					//
					// 結果は標準出力
					//
					std::cout
						<< "/"
						<< PathToUtf8(relativePath.generic_wstring())
						<< '\n';
				}
			}
		}

		std::error_code nextEc;

		it.increment(nextEc);

		if (nextEc)
		{
			scanFailed = true;
			std::cerr
				<< "ERROR: Enumeration: "
				<< nextEc.message()
				<< std::endl;
		}
	}

	std::cerr
		<< "Scan complete: "
		<< dirCount
		<< " directories, "
		<< fileCount
		<< " files, "
		<< foundCount
		<< " found"
		<< std::endl;

	return scanFailed ? 1 : 0;
}
