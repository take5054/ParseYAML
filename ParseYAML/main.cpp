#include "YAMLParser.hpp"
#include <crtdbg.h>
#include <iostream>

int main()
{
	// メモリリーク検出
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);

	// ファイル名を指定してYAMLを解析
	auto node = ParseYAML("ComplicatedTestData.yaml");
	Print_YAML(node, 0);
	std::cout << "YAMLの解析が完了しました。" << std::endl;
	node.reset();

	return 0;
}
