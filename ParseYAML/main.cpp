#include "YAMLParser.hpp"
#include <crtdbg.h>
#include <iostream>

int main()
{
	// メモリリーク検出
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF);

	// ファイル名を指定してYAMLを解析
	std::unique_ptr<YAMLParser> yaml = std::make_unique<YAMLParser>();
	yaml->ParseYAML("ComplicatedTestData.yaml");
	yaml->Print_YAML();

	std::cout << "YAMLの解析が完了しました。" << std::endl;

	std::cout << "users.0.name: " << yaml->GetString("users.0.name") << std::endl;
	std::cout << "users.1.age: " << yaml->GetInt("users.1.profile.age") << std::endl;
	std::cout << "ここまで深くなる: " << yaml->GetString("deep_nest.level1.level2.level3.level4") << std::endl;

	yaml.reset();

	return 0;
}
