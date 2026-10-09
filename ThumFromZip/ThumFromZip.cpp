
#define NOMINMAX
#include <windows.h>
#include "SevenZipExtract.h"

#include <algorithm>

#include <cctype>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

// 処理結果
enum class Result {
	Success,
	NoImage,
	Exists,
	Error
};

// 処理件数
struct Statistics {
	int total = 0;
	int success = 0;
	int noImage = 0;
	int exists = 0;
	int errors = 0;
};
void PrintConsole(const std::wstring& message, DWORD destination = STD_OUTPUT_HANDLE)
{
	HANDLE h = GetStdHandle(destination);
	DWORD written = 0;

	if (h != INVALID_HANDLE_VALUE && h != nullptr) {
		if (GetConsoleMode(h, &written)) {
			WriteConsoleW(
				h,
				message.c_str(),
				static_cast<DWORD>(message.size()),
				&written,
				nullptr
			);
			return;
		}
	}

	// パイプ・ログファイルにはUTF-8で出力する。
	const int size = WideCharToMultiByte(CP_UTF8, 0, message.data(),
		static_cast<int>(message.size()), nullptr, 0, nullptr, nullptr);
	if (size <= 0) return;
	std::string bytes(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, message.data(), static_cast<int>(message.size()),
		bytes.data(), size, nullptr, nullptr);
	size_t offset = 0;
	while (offset < bytes.size()) {
		if (!WriteFile(h, bytes.data() + offset, static_cast<DWORD>(bytes.size() - offset),
			&written, nullptr) || !written) break;
		offset += written;
	}
}

class WideLog : public std::wostringstream {
	DWORD destination_;
public:
	explicit WideLog(DWORD destination = STD_OUTPUT_HANDLE) : destination_(destination) {}
	~WideLog() { PrintConsole(str(), destination_); }
};
// ZIP内の画像の拡張子を判定する
std::string GetImageExtension(const std::string& name)
{
	// ZIP内のディレクトリは対象外
	if (name.empty() ||
		name.back() == '/' ||
		name.back() == '\\') {
		return "";
	}

	const size_t slash = name.find_last_of("/\\");
	const size_t dot = name.find_last_of('.');

	if (dot == std::string::npos ||
		(slash != std::string::npos && dot < slash)) {
		return "";
	}

	std::string ext = name.substr(dot);

	std::transform(ext.begin(), ext.end(), ext.begin(),
		[](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});

	if (ext == ".jpg" || ext == ".jpeg") {
		return ".jpeg";
	}

	if (ext == ".png") {
		return ".png";
	}

	return "";
}

Result ExtractFirstImage(const fs::path& zipPath, bool overwrite)
{
    try {
        sevenzip::Archive archive(zipPath, {0x01});
        UInt32 count = 0;
        sevenzip::Check(archive.Get()->GetNumberOfItems(&count), "Cannot list ZIP");
        UInt32 selectedIndex = 0;
        std::wstring selectedDisplay;
        std::string selectedName, selectedSort, selectedExtension;
        bool hasImage = false;
        for (UInt32 i = 0; i < count; ++i) {
            sevenzip::Property directory, path;
            sevenzip::Check(archive.Get()->GetProperty(i, kpidIsDir, &directory.value), "Cannot read ZIP entry");
            if (directory.value.vt == VT_BOOL && directory.value.boolVal != VARIANT_FALSE) continue;
            sevenzip::Check(archive.Get()->GetProperty(i, kpidPath, &path.value), "Cannot read ZIP filename");
            if (path.value.vt != VT_BSTR || !path.value.bstrVal) continue;
            std::wstring wide(path.value.bstrVal, SysStringLen(path.value.bstrVal));
            // 7-Zip uses Windows separators; retain the former ZIP path comparison.
            std::replace(wide.begin(), wide.end(), L'\\', L'/');
            int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
            if (length <= 0) continue;
            std::string name(length, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), name.data(), length, nullptr, nullptr);
            std::string extension = GetImageExtension(name);
            if (extension.empty()) continue;
            size_t slash = name.find_last_of("/\\");
            std::string sortName = slash == std::string::npos ? name : name.substr(slash + 1);
            std::transform(sortName.begin(), sortName.end(), sortName.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (!hasImage || sortName < selectedSort || (sortName == selectedSort && name < selectedName)) {
                selectedIndex = i;
                selectedName = name;
                selectedSort = sortName;
                selectedExtension = extension;
                selectedDisplay = wide;
                hasImage = true;
            }
        }
        if (!hasImage) {
            PrintConsole(L"  SKIP: JPG/PNGがありません。\n");
            return Result::NoImage;
        }
        fs::path outputPath = zipPath;
        outputPath.replace_extension(selectedExtension);
        PrintConsole(L"  SELECT: " + selectedDisplay + L"\n");
        fs::path writePath = outputPath;
        if (overwrite) {
            GUID id{};
            sevenzip::Check(CoCreateGuid(&id), "Cannot create temporary filename");
            wchar_t suffix[40]{};
            if (!StringFromGUID2(id, suffix, 40)) throw std::runtime_error("Cannot format temporary filename");
            writePath += std::wstring(L".") + suffix + L".tmp";
        }
        HANDLE output = CreateFileW(writePath.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (output == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) return Result::Exists;
            PrintConsole(L"  ERROR: 出力ファイル作成失敗\n");
            return Result::Error;
        }
        // Remove partial output on any extraction or allocation failure.
        bool success = false;
        try {
            sevenzip::ComPtr<sevenzip::ExtractCallback> callback(new sevenzip::ExtractCallback(output, selectedIndex));
            HRESULT hr = archive.Get()->Extract(&selectedIndex, 1, false, callback.get());
            success = hr == S_OK && callback->result == NArchive::NExtract::NOperationResult::kOK;
        }
        catch (...) {
            CloseHandle(output);
            DeleteFileW(writePath.c_str());
            throw;
        }
        if (!CloseHandle(output)) success = false;
        if (!success) {
            DeleteFileW(writePath.c_str());
            PrintConsole(L"  ERROR: 展開失敗（破損・暗号化など）\n");
            return Result::Error;
        }
        // Replace only after extraction succeeds, preserving an existing image on failure.
        if (overwrite && !MoveFileExW(writePath.c_str(), outputPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            DeleteFileW(writePath.c_str());
            PrintConsole(L"  ERROR: 既存ファイルの上書き失敗\n");
            return Result::Error;
        }
        PrintConsole(L"  OK: " + outputPath.wstring() + L"\n");
        return Result::Success;
    }
    catch (const std::exception& e) {
        PrintConsole(L"  ERROR: ZIPの処理に失敗しました。\n");
        std::cerr << e.what() << '\n';
        return Result::Error;
    }
}

// 1つのZIPを処理して統計を更新




void ProcessZip(
	const fs::path& path,
	Statistics& stats,
	bool overwrite)
{
	++stats.total;

	// 必ず最初にZIP名を表示する
	PrintConsole(
		L"\n----------------------------------------\n"
		L"ZIP: " + path.wstring() + L"\n"
	);

	// 既存サムネイルを確認
	const wchar_t* extensions[] = {
		L".jpg", L".jpeg", L".png"
	};

	for (const auto* ext : extensions) {
		fs::path thumbnail = path;
		thumbnail.replace_extension(ext);

		std::error_code ec;

		if (!overwrite && fs::exists(thumbnail, ec) && !ec) {
			++stats.exists;

			PrintConsole(
				L"SKIP: サムネイルあり\n"
				L"      " + thumbnail.wstring() +
				L"\n処理終了: " + path.filename().wstring() +
				L" [スキップ]\n"
			);

			return;
		}
	}

	// 上書き指定時は既存サムネイルがあってもZIPを開く
	PrintConsole(L"処理開始: " + path.filename().wstring() + L"\n");

	const Result result = ExtractFirstImage(path, overwrite);

	// 処理結果を集計
	std::wstring status;

	switch (result) {
	case Result::Success:
		++stats.success;
		status = L"正常終了";
		break;

	case Result::NoImage:
		++stats.noImage;
		status = L"画像なし";
		break;

	case Result::Exists:
		++stats.exists;
		status = L"既存ファイル";
		break;

	case Result::Error:
		++stats.errors;
		status = L"エラー";
		break;
	}

	// 必ず終了メッセージを表示
	PrintConsole(
		L"処理終了: " + path.filename().wstring() +
		L" [" + status + L"]\n"
	);
}


// ZIPファイルか判定
bool IsZipFile(const fs::path& path)
{
	std::wstring ext = path.extension().wstring();

	std::transform(ext.begin(), ext.end(), ext.begin(),
		[](wchar_t c) {
			return static_cast<wchar_t>(towlower(c));
		});

	return ext == L".zip";
}

// メイン
int wmain(int argc, wchar_t* argv[])
{
	// 引数なしならカレントディレクトリ
	fs::path root;
	bool overwrite = false;

	try {
		bool endOptions = false;
		for (int i = 1; i < argc; ++i) {
			const std::wstring arg = argv[i];
			if (!endOptions && arg == L"--") {
				endOptions = true;
			}
			else if (!endOptions && (arg == L"--overwrite" || arg == L"-f")) {
				overwrite = true;
			}
			else if (!endOptions && (arg == L"--help" || arg == L"-h")) {
				PrintConsole(L"Usage: ThumFromZip.exe [--overwrite|-f] [folder]\n"
					L"  --overwrite, -f : 既存画像があっても抽出して上書き\n");
				return 0;
			}
			else if ((!endOptions && !arg.empty() && arg.front() == L'-') || !root.empty()) {
				PrintConsole(L"ERROR: 不明なオプション、または引数が多すぎます。--help を参照してください。\n");
				return 1;
			}
			else {
				root = fs::absolute(arg);
			}
		}
		if (root.empty()) root = fs::current_path();

		if (!fs::exists(root) ||
			!fs::is_directory(root)) {

			WideLog(STD_ERROR_HANDLE)
				<< L"ERROR: ディレクトリが存在しません。\n"
				<< root.wstring() << L'\n';

			return 1;
		}

		WideLog()
			<< L"ZIP画像抽出\n"
			<< L"検索対象: " << root.wstring()
			<< L"\n";

		Statistics stats;

		// サブディレクトリを再帰的に検索
		const auto options =
			fs::directory_options::skip_permission_denied;

		std::error_code ec;

		fs::recursive_directory_iterator it(
			root, options, ec
		);

		const fs::recursive_directory_iterator end;

		if (ec) {
			WideLog(STD_ERROR_HANDLE)
				<< L"ERROR: ディレクトリを開けません。\n";

			return 1;
		}

		while (it != end) {
			std::error_code fileError;

			const bool regular =
				it->is_regular_file(fileError);

			if (!fileError &&
				regular &&
				IsZipFile(it->path())) {

				ProcessZip(it->path(), stats, overwrite);
			}

			it.increment(ec);

			if (ec) {
				WideLog(STD_ERROR_HANDLE)
					<< L"WARNING: ディレクトリの検索エラー\n";

				++stats.errors;
				ec.clear();
			}
		}

		// 結果表示
		WideLog()
			<< L"\n"
			<< L"============================\n"
			<< L"処理結果\n"
			<< L"============================\n"
			<< L"ZIP総数       : " << stats.total << L'\n'
			<< L"抽出成功      : " << stats.success << L'\n'
			<< L"画像なし      : " << stats.noImage << L'\n'
			<< L"既存ファイル  : " << stats.exists << L'\n'
			<< L"エラー        : " << stats.errors << L'\n';

		return stats.errors == 0 ? 0 : 1;
	}
	catch (const fs::filesystem_error& e) {
		std::cerr
			<< "Filesystem ERROR: "
			<< e.what() << '\n';

		return 1;
	}
	catch (const std::exception& e) {
		std::cerr
			<< "ERROR: "
			<< e.what() << '\n';

		return 1;
	}
}
