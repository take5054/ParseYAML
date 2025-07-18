#include "YAMLParser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <Windows.h>
#include <crtdbg.h>

std::string Convert_UTF8_To_ShiftJIS(const std::string_view& In_Source)
{
    const char* text = In_Source.data();
    // UTF-8 から Shift-JIS への変換
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(text), -1, NULL, 0);
    std::wstring wideText(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(text), -1, &wideText[0], size_needed);

    size_needed = WideCharToMultiByte(932, 0, wideText.c_str(), -1, NULL, 0, NULL, NULL);
    std::string shiftJISText(size_needed, 0);
    WideCharToMultiByte(932, 0, wideText.c_str(), -1, &shiftJISText[0], size_needed, NULL, NULL);

    return shiftJISText;
}

int main()
{
    // メモリリーク検出
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);

    // ファイル名を指定
    const std::string filename = "ComplicatedTestData.yaml";

    // UTF-8ファイルを読み込む
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs)
    {
        std::cerr << "ファイルを開けません: " << filename << std::endl;
        return 1;
    }

    // ファイル全体を文字列に読み込む
    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    std::string utf8Content = buffer.str();

    std::string yamldata = Convert_UTF8_To_ShiftJIS(utf8Content);

    auto node = ParseYAML(yamldata);
    Print_YAML(node, 0);
	std::cout << "YAMLの解析が完了しました。" << std::endl;
	node.reset();

    return 0;
}
