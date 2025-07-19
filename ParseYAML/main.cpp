#include "YAMLParser.hpp"
#include <crtdbg.h>
#include <iostream>

int main()
{
	// メモリリーク検出
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);

	// ファイル名を指定してYAMLを解析
	auto root = YAMLParser::ParseYAML("ComplicatedTestData.yaml");
	YAMLParser::Print_YAML(root);
	std::cout << "YAMLの解析が完了しました。" << std::endl;
	root.reset();

	return 0;
}
