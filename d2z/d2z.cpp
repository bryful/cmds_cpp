// d2z.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <io.h>
#include <fstream>

namespace fs = std::filesystem;

std::wstring LoadSevenZipPath()
{
    std::wstring defaultPath = L"C:\\Program Files\\7-Zip\\7z.exe";
    
    // 実行ファイルのフルパスを取得
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    
    // 拡張子を.prefに変更
    fs::path prefPath(exePath);
    prefPath.replace_extension(L".pref");
    
    // .prefファイルが存在するかチェック
    if (!fs::exists(prefPath))
    {
        return defaultPath;
    }
    
    // ファイルを読み込む
    std::wifstream prefFile(prefPath);
    if (!prefFile.is_open())
    {
        return defaultPath;
    }
    
    std::wstring sevenZipPath;
    std::getline(prefFile, sevenZipPath);
    prefFile.close();
    
    // 読み込んだパスが空でないかチェック
    if (sevenZipPath.empty())
    {
        return defaultPath;
    }
    
    // 前後の空白を削除
    sevenZipPath.erase(0, sevenZipPath.find_first_not_of(L" \t\r\n"));
    sevenZipPath.erase(sevenZipPath.find_last_not_of(L" \t\r\n") + 1);
    
    // 読み込んだパスが有効な7z.exeのパスかチェック
    if (fs::exists(sevenZipPath) && fs::is_regular_file(sevenZipPath))
    {
        return sevenZipPath;
    }
    
    return defaultPath;
}

bool DirToZip(const std::wstring& dirPath, const std::wstring& sevenZipPath)
{
    try
    {
        fs::path dir(dirPath);
        
        // ディレクトリが存在するかチェック
        if (!fs::exists(dir) || !fs::is_directory(dir))
        {
            std::wcerr << L"Directory not found: " << dirPath << std::endl;
            return false;
        }

        // サブディレクトリとファイルをカウント
        int dirCount = 0;
        int fileCount = 0;
        fs::path targetFolder = dir;

        for (const auto& entry : fs::directory_iterator(dir))
        {
            if (entry.is_directory())
                dirCount++;
            else if (entry.is_regular_file())
                fileCount++;
        }

        // サブディレクトリが1つだけでファイルが0の場合、そのサブディレクトリをターゲットにする
        if (dirCount == 1 && fileCount == 0)
        {
            for (const auto& entry : fs::directory_iterator(dir))
            {
                if (entry.is_directory())
                {
                    targetFolder = entry.path();
                    break;
                }
            }
        }

        // Zipファイル名とターゲットパスを作成
        std::wstring zipFileName = dir.wstring() + L".zip";
        std::wstring targetPath = targetFolder.wstring() + L"\\*";
        
        // 7z.exeのコマンドライン引数を作成
        std::wstring arguments = L"\"" + sevenZipPath + L"\" a \"" + zipFileName + L"\" \"" + targetPath + L"\"";

        // プロセスを起動
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

        // プロセスの終了を待つ
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode == 0)
        {
            std::wcout << L"Created: " << zipFileName << std::endl;
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
    // 7z.exeのパスを読み込む
    std::wstring sevenZipPath = LoadSevenZipPath();
    
    // 7z.exeの存在チェック
    if (!fs::exists(sevenZipPath))
    {
        std::wcout << L"7-Zip not found." << std::endl;
        return 1;
    }

    // 引数チェック
    if (argc <= 1)
    {
        std::wcout << L"no params" << std::endl;
        return 1;
    }

    // 各引数に対してDirToZipを実行
    for (int i = 1; i < argc; i++)
    {
        DirToZip(argv[i], sevenZipPath);
    }

    return 0;
}
