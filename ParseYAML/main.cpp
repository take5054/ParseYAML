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

	//yaml->GenerateNode("new.node.test", YAMLParser::YAMLNode::Type::Scalar);
	yaml->SetBool("new.node.test", true);
	yaml->SetString("new.node.test2", "新しいノードの追加");

	std::cout << "新しいノードの追加: " << yaml->GetBool("new.node.test") << std::endl;

	yaml->GenerateNode("admins", YAMLParser::YAMLNode::Type::Sequence);
	yaml->GenerateNode("admins.0", YAMLParser::YAMLNode::Type::Map);
	yaml->GenerateNode("admins.1", YAMLParser::YAMLNode::Type::Map);
	yaml->SetString("admins.0.name", "admin1");
	yaml->SetInt("admins.0.age", 30);
	yaml->SetString("admins.0.role", "superuser");
	yaml->SetString("admins.1.name", "admin2");
	yaml->SetInt("admins.1.age", 28);
	yaml->SetString("admins.1.role", "moderator");

	std::cout << "admins.0.name: " << yaml->GetString("admins.0.name") << std::endl;

	yaml->SaveYAML("OutputTestData.yaml");

	yaml.reset();

	return 0;
}
