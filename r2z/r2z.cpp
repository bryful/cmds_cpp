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
        enabled = false;
        if (line.length() < 53) return false;

        try
        {
            wchar_t attrD = line[20];
            wchar_t attrA = line[24];

            if ((attrD != L'D') && (attrD != L'.')) return false;
            if ((attrA != L'A') && (attrA != L'.')) return false;

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

bool RarToZip(const std::wstring& filePath, const std::wstring& sevenZipPath)
{
    try
    {
        fs::path rarFile(filePath);
        
        if (!fs::exists(rarFile) || !fs::is_regular_file(rarFile))
        {
            std::wcerr << L"Archive file not found: " << filePath << std::endl;
            return false;
        }

        std::wstring ext = rarFile.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext != L".rar")
        {
            std::wcerr << L"Not a RAR file: " << ext << std::endl;
            return false;
        }

        // アーカイブ内にscrファイルがあるかチェック
        std::vector<ArcInfo> list = Listup(filePath, sevenZipPath);
        bool hasScrFile = HasScrFileInArchive(list);

        // 一時展開フォルダを作成
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        fs::path tempExtractFolder = fs::path(tempPath) / rarFile.stem();
        
        fs::create_directories(tempExtractFolder);

        // rarファイルを一時フォルダに展開
        std::wstring arguments = L"\"" + sevenZipPath + L"\" x \"" + filePath + L"\" -o\"" + tempExtractFolder.wstring() + L"\" -y";
        
        // scrファイルがある場合は除外
        if (hasScrFile)
        {
            arguments += L" -x!*.scr";
            std::wcout << L"Warning: *.scr files will be excluded" << std::endl;
        }

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
            std::wcerr << L"Failed to extract RAR file" << std::endl;
            fs::remove_all(tempExtractFolder);
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode != 0)
        {
            std::wcerr << L"Extraction failed with exit code: " << exitCode << std::endl;
            fs::remove_all(tempExtractFolder);
            return false;
        }

        // 展開先のサブディレクトリとファイルをチェック
        int dirCount = 0;
        int fileCount = 0;
        fs::path targetFolder = tempExtractFolder;

        for (const auto& entry : fs::directory_iterator(tempExtractFolder))
        {
            if (entry.is_directory())
                dirCount++;
            else if (entry.is_regular_file())
                fileCount++;
        }

        // サブディレクトリが1つだけでファイルが0の場合、そのサブディレクトリを圧縮対象にする
        if (dirCount == 1 && fileCount == 0)
        {
            for (const auto& entry : fs::directory_iterator(tempExtractFolder))
            {
                if (entry.is_directory())
                {
                    targetFolder = entry.path();
                    break;
                }
            }
        }

        // zipファイル名を作成
        std::wstring outputZipFile = rarFile.wstring();
        size_t pos = outputZipFile.rfind(L".rar");
        if (pos != std::wstring::npos)
        {
            outputZipFile.replace(pos, 4, L".zip");
        }
        else
        {
            outputZipFile += L".zip";
        }

        // zipに圧縮
        std::wstring targetPath = targetFolder.wstring() + L"\\*";
        arguments = L"\"" + sevenZipPath + L"\" a -tzip \"" + outputZipFile + L"\" \"" + targetPath + L"\"";

        si = { sizeof(si) };
        pi = {};

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
            std::wcerr << L"Failed to create ZIP file" << std::endl;
            fs::remove_all(tempExtractFolder);
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // 一時フォルダを削除
        fs::remove_all(tempExtractFolder);

        if (exitCode == 0)
        {
            std::wcout << L"Created: " << outputZipFile << std::endl;
            return true;
        }
        else
        {
            std::wcerr << L"Compression failed with exit code: " << exitCode << std::endl;
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
        RarToZip(argv[i], sevenZipPath);
    }

    return 0;
}