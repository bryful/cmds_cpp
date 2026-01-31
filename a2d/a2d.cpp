#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

struct ArcInfo
{
    std::wstring name;
    bool isDir;
    bool isRootDir;
    bool enabled;

    ArcInfo() : isDir(false), isRootDir(false), enabled(false) {}

    bool Parse(const std::wstring& line)
    {
        // 7z.exeの出力形式:
        //   Date      Time    Attr         Size   Compressed  Name
        // 2024-05-18 09:35:57 D...A            0            0  フォルダ名
        enabled = false;
        if (line.length() < 53) return false;

        try
        {
            // 属性チェック (位置20-24)
            if (line.length() < 53) return false;
            
            wchar_t attrD = line[20];
            wchar_t attrA = line[24];

            if ((attrD != L'D') && (attrD != L'.')) return false;
            if ((attrA != L'A') && (attrA != L'.')) return false;

            // ファイル名取得 (位置53以降)
            name = line.substr(53);
            isDir = (attrD == L'D');
            isRootDir = (name.find(L'\\') == std::wstring::npos && name.find(L'/') == std::wstring::npos);
            enabled = true;
            return true;
        }
        catch (...)
        {
            enabled = false;
            return false;
        }
    }
};

std::wstring LoadSevenZipPath()
{
    std::wstring defaultPath = L"C:\\Program Files\\7-Zip\\7z.exe";
    
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    
    fs::path prefPath(exePath);
    prefPath.replace_extension(L".pref");
    
    if (!fs::exists(prefPath))
    {
        return defaultPath;
    }
    
    std::wifstream prefFile(prefPath);
    if (!prefFile.is_open())
    {
        return defaultPath;
    }
    
    std::wstring sevenZipPath;
    std::getline(prefFile, sevenZipPath);
    prefFile.close();
    
    if (sevenZipPath.empty())
    {
        return defaultPath;
    }
    
    sevenZipPath.erase(0, sevenZipPath.find_first_not_of(L" \t\r\n"));
    sevenZipPath.erase(sevenZipPath.find_last_not_of(L" \t\r\n") + 1);
    
    if (fs::exists(sevenZipPath) && fs::is_regular_file(sevenZipPath))
    {
        return sevenZipPath;
    }
    
    return defaultPath;
}

std::vector<ArcInfo> Listup(const std::wstring& archiveFile, const std::wstring& sevenZipPath)
{
    std::vector<ArcInfo> fileList;
    
    if (!fs::exists(archiveFile))
    {
        return fileList;
    }

    std::wstring arguments = L"\"" + sevenZipPath + L"\" l \"" + archiveFile + L"\"";

    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
    {
        return fileList;
    }

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    PROCESS_INFORMATION pi = {};

    if (CreateProcessW(
        nullptr,
        const_cast<LPWSTR>(arguments.c_str()),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi))
    {
        CloseHandle(hWritePipe);

        char buffer[4096];
        DWORD bytesRead;
        std::string output;

        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            output += buffer;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);

        // Shift-JISからワイド文字に変換
        int wideSize = MultiByteToWideChar(CP_OEMCP, 0, output.c_str(), -1, nullptr, 0);
        if (wideSize > 0)
        {
            std::vector<wchar_t> wideBuffer(wideSize);
            MultiByteToWideChar(CP_OEMCP, 0, output.c_str(), -1, wideBuffer.data(), wideSize);

            std::wstring wideOutput(wideBuffer.data());
            std::wistringstream stream(wideOutput);
            std::wstring line;

            while (std::getline(stream, line))
            {
                ArcInfo ai;
                if (ai.Parse(line))
                {
                    fileList.push_back(ai);
                }
            }
        }
    }
    else
    {
        CloseHandle(hWritePipe);
        CloseHandle(hReadPipe);
    }

    return fileList;
}

bool HasScrFileInArchive(const std::vector<ArcInfo>& fileList)
{
    for (const auto& file : fileList)
    {
        if (!file.isDir)
        {
            fs::path filePath(file.name);
            std::wstring ext = filePath.extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == L".scr")
            {
                return true;
            }
        }
    }
    return false;
}

bool ArcToDir(const std::wstring& filePath, const std::wstring& sevenZipPath)
{
    try
    {
        fs::path archiveFile(filePath);
        
        if (!fs::exists(archiveFile) || !fs::is_regular_file(archiveFile))
        {
            std::wcerr << L"Archive file not found: " << filePath << std::endl;
            return false;
        }

        std::wstring ext = archiveFile.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext != L".rar" && ext != L".zip")
        {
            std::wcerr << L"Unsupported archive format: " << ext << std::endl;
            return false;
        }

        // アーカイブの内容をリストアップ
        std::vector<ArcInfo> list = Listup(filePath, sevenZipPath);

        int rc = 0; // ルートディレクトリ数
        int fc = 0; // ファイル数

        for (const auto& file : list)
        {
            if (file.isRootDir) rc++;
            if (!file.isDir) fc++;
        }

        // 展開先ディレクトリを決定
        fs::path tmpDir = archiveFile.parent_path() / archiveFile.stem();

        if (((rc == 1) && (fc == 0)) || ((rc == 0) && (fc > 0)))
        {
            tmpDir = archiveFile.parent_path();
        }

        std::wstring tmpDirStr = tmpDir.wstring();
        // 引数を一気に組み立てる（スペースの入れ忘れを防止）
        std::wstringstream ss;
        ss << L"\"" << sevenZipPath << L"\"";
        ss << L" x";                               // 展開コマンド
        ss << L" \"-x!***.scr\"";                   // 除外設定（先に持ってくるか、クォーテーションで囲む）
        ss << L" \"" << filePath << L"\"";         // アーカイブファイルパス
        ss << L" -o\"" << tmpDirStr << L"\"";      // 展開先
        ss << L" -y";                              // 全て「はい」

        std::wstring arguments = ss.str();

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        if (!CreateProcessW(
            nullptr,
            const_cast<LPWSTR>(arguments.c_str()),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi))
        {
            std::wcerr << L"Failed to start 7z.exe" << std::endl;
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode == 0)
        {
            std::wcout << L"Extracted to: " << tmpDirStr << std::endl;
            return true;
        }
        else
        {
            std::wcerr << L"7z.exe failed with exit code: " << exitCode << std::endl;
            return false;
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return false;
    }
}

int wmain(int argc, wchar_t* argv[])
{
    std::wstring sevenZipPath = LoadSevenZipPath();
    
    if (!fs::exists(sevenZipPath))
    {
        std::wcout << L"7-Zip not found." << std::endl;
        return 1;
    }

    if (argc <= 1)
    {
        std::wcout << L"no params" << std::endl;
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        ArcToDir(argv[i], sevenZipPath);
    }

    return 0;
}