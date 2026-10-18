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
	std::uintmax_t totalSize;
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

std::string FormatSize(std::uintmax_t size)
{
	const double KB = 1024.0;
	const double MB = 1024.0 * 1024.0;
	const double GB = 1024.0 * 1024.0 * 1024.0;
	const double TB = 1024.0 * 1024.0 * 1024.0 * 1024.0;

	char buffer[64];

	if (size >= static_cast<std::uintmax_t>(TB))
	{
		sprintf_s(
			buffer,
			"%.2f TB",
			static_cast<double>(size) / TB
		);
	}
	else if (size >= static_cast<std::uintmax_t>(GB))
	{
		sprintf_s(
			buffer,
			"%.2f GB",
			static_cast<double>(size) / GB
		);
	}
	else if (size >= static_cast<std::uintmax_t>(MB))
	{
		sprintf_s(
			buffer,
			"%.2f MB",
			static_cast<double>(size) / MB
		);
	}
	else if (size >= static_cast<std::uintmax_t>(KB))
	{
		sprintf_s(
			buffer,
			"%.2f KB",
			static_cast<double>(size) / KB
		);
	}
	else
	{
		sprintf_s(
			buffer,
			"%llu B",
			static_cast<unsigned long long>(size)
		);
	}

	return buffer;
}

int wmain(int argc, wchar_t* argv[])
{
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

		while (it != end)
		{
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
					<< "ERROR: Root enumeration: "
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

	std::size_t scanCount = 0;

	//
	// ディレクトリを1個ずつ処理
	//
	while (!pending.empty())
	{
		fs::path dirPath = pending.back();
		pending.pop_back();

		++scanCount;

		std::cerr
			<< "["
			<< scanCount
			<< "/"
			<< (scanCount + pending.size())
			<< "] "
			<< PathToUtf8(dirPath)
			<< std::endl;

		std::uintmax_t totalSize = 0;

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
			// 通常ファイルならサイズ加算
			//
			if (entry.is_regular_file(typeEc) && !typeEc)
			{
				std::error_code sizeEc;

				std::uintmax_t fileSize =
					entry.file_size(sizeEc);

				if (!sizeEc)
				{
					totalSize += fileSize;
				}
				else
				{
					scanFailed = true;
					std::cerr
						<< "  ERROR: Cannot get file size: "
						<< PathToUtf8(entry.path())
						<< " : "
						<< sizeEc.message()
						<< std::endl;
				}
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
				totalSize
			});
	}

	std::cerr
		<< "Scan complete: "
		<< dirs.size()
		<< " directories"
		<< std::endl;

	//
	// サイズの大きい順
	//
	std::sort(
		dirs.begin(),
		dirs.end(),
		[](const DirInfo& a, const DirInfo& b)
		{
			if (a.totalSize != b.totalSize)
			{
				return a.totalSize > b.totalSize;
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
			<< std::setw(12)
			<< std::right
			<< FormatSize(dir.totalSize)
			<< "  /"
			<< PathToUtf8(relativePath)
			<< '\n';
	}

	return scanFailed ? 1 : 0;
}