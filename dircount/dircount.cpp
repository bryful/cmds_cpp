#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <windows.h>

namespace fs = std::filesystem;

struct DirInfo
{
	fs::path path;
	std::uintmax_t fileCount;
};

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
	// コンソールをUTF-8にする
	SetConsoleOutputCP(CP_UTF8);

	fs::path rootPath;

	// 引数があればそのパス、なければカレントディレクトリ
	if (argc >= 2)
	{
		rootPath = fs::path(argv[1]);
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

	bool scanFailed = false;
	std::vector<DirInfo> dirs;
	std::vector<fs::path> pending;

	std::cerr
		<< "Scan root: "
		<< PathToUtf8(rootPath)
		<< std::endl;

	//
	// まずルート直下のディレクトリを取得
	//
	{
		std::error_code iterEc;

		fs::directory_iterator it(
			rootPath,
			fs::directory_options::none,
			iterEc
		);

		fs::directory_iterator end;

		if (iterEc)
		{
			std::cerr
				<< "ERROR: Cannot open root directory: "
				<< iterEc.message()
				<< std::endl;

			return 1;
		}

		std::size_t rootCount = 0;

		while (it != end)
		{
			++rootCount;

			const fs::directory_entry& entry = *it;

			std::error_code typeEc;

			bool isSymlink = entry.is_symlink(typeEc);

			if (!typeEc && !isSymlink)
			{
				typeEc.clear();

				if (entry.is_directory(typeEc) && !typeEc)
				{
					pending.push_back(entry.path());
				}
			}

			std::error_code nextEc;

			it.increment(nextEc);

			if (nextEc)
			{
			scanFailed = true;
				std::cerr
					<< "ERROR: Root enumeration at entry "
					<< rootCount
					<< ": "
					<< nextEc.message()
					<< std::endl;

				break;
			}
		}
	}

	std::cerr
		<< "Found "
		<< pending.size()
		<< " directories."
		<< std::endl;

	//
	// ディレクトリを1個ずつ処理
	//
	std::size_t scanCount = 0;

	while (!pending.empty())
	{
		fs::path dirPath = pending.back();
		pending.pop_back();

		++scanCount;

		//
		// 重要:
		// path.string() は使わない
		//
		std::cerr
			<< "["
			<< scanCount
			<< "/"
			<< (scanCount + pending.size())
			<< "] "
			<< PathToUtf8(dirPath)
			<< std::endl;

		std::uintmax_t fileCount = 0;

		std::error_code iterEc;

		fs::directory_iterator it(
			dirPath,
			fs::directory_options::none,
			iterEc
		);

		fs::directory_iterator end;

		if (iterEc)
		{
			std::cerr
				<< "  ERROR: Cannot open: "
				<< iterEc.message()
				<< std::endl;

			scanFailed = true;
			dirs.push_back({ dirPath, 0 });
			continue;
		}

		while (it != end)
		{
			const fs::directory_entry& entry = *it;

			std::error_code typeEc;

			//
			// 通常ファイルを数える
			//
			if (entry.is_regular_file(typeEc) && !typeEc)
			{
				++fileCount;
			}

			//
			// サブディレクトリがあれば後で走査
			//
			typeEc.clear();

			bool isSymlink = entry.is_symlink(typeEc);

			if (!typeEc && !isSymlink)
			{
				typeEc.clear();

				if (entry.is_directory(typeEc) && !typeEc)
				{
					pending.push_back(entry.path());
				}
			}

			std::error_code nextEc;

			it.increment(nextEc);

			if (nextEc)
			{
			scanFailed = true;
				std::cerr
					<< "  ERROR: Enumeration: "
					<< nextEc.message()
					<< std::endl;

				break;
			}
		}

		dirs.push_back(
			{
				dirPath,
				fileCount
			});
	}

	std::cerr
		<< "Scan complete: "
		<< dirs.size()
		<< " directories"
		<< std::endl;

	//
	// ファイル数の多い順
	//
	std::sort(
		dirs.begin(),
		dirs.end(),
		[](const DirInfo& a, const DirInfo& b)
		{
			if (a.fileCount != b.fileCount)
			{
				return a.fileCount > b.fileCount;
			}

			return a.path.native() < b.path.native();
		}
	);

	//
	// 結果表示
	//
	for (const auto& dir : dirs)
	{
		std::error_code relEc;

		fs::path relativePath =
			fs::relative(dir.path, rootPath, relEc);

		if (relEc)
		{
			relativePath = dir.path;
		}

		std::cout
			<< std::setw(4)
			<< std::setfill('0')
			<< dir.fileCount
			<< "  /"
			<< PathToUtf8(relativePath)
			<< '\n';
	}

	return scanFailed ? 1 : 0;
}