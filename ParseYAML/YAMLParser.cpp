#include "YAMLParser.hpp"
#include <sstream>
#include <iostream>
#include <cctype>
#include <memory>
#include <string>
#include <variant>
#include <vector>

static size_t IndentCounter(const std::string& In_Line)
{
	size_t IndentCnt = 0;

	// 行の先頭からスペースとタブをカウント
	while (IndentCnt < In_Line.size() &&
		(In_Line[IndentCnt] == ' ' || In_Line[IndentCnt] == '\t'))
		++IndentCnt;

	return IndentCnt;
}

static std::string TrimLeftWhitespace(const std::string& In_str)
{
	if (In_str.empty()) return In_str;

	// YAMLの行の先頭から空白文字を取り除く
	size_t idxCnt = In_str.find_first_not_of(" \t\n\r\f\v");
	return (idxCnt == std::string::npos) ? "" : In_str.substr(idxCnt);
}

/// <summary>
/// YAMLLines構造体は、YAMLファイルの各行を管理し、現在の位置や終端判定などの操作を提供します。
/// </summary>
struct YAMLLines
{
	std::vector<std::string> lines;
	size_t currentPos = 0;
	bool eof() const { return currentPos >= lines.size(); }
	const std::string& peek() const { return lines[currentPos]; }
	void next() { ++currentPos; }
};

/// <summary>
/// YAMLの行データからノードを解析し、共有ポインタとして返します。
/// </summary>
/// <param name="In_YAMLLines">解析対象となるYAMLの行データ。</param>
/// <param name="In_CurrentIndent">現在のインデントレベル。ノードの階層構造を判断するために使用されます。</param>
/// <returns>解析されたYAMLノードへのstd::shared_ptr。</returns>
std::shared_ptr<YAMLNode> ParseNode(YAMLLines& In_YAMLLines, size_t In_CurrentIndent);

/// <summary>
/// YAMLのマップ（辞書）ノードを解析して生成します。
/// </summary>
/// <param name="In_YAMLLines">解析対象となるYAMLの行データへの参照。</param>
/// <param name="In_CurrentIndent">現在のインデントレベル。マップの階層を判定するために使用されます。</param>
/// <returns>解析されたYAMLマップノードへのstd::shared_ptr。</returns>
std::shared_ptr<YAMLNode> ParseMap(YAMLLines& In_YAMLLines, size_t In_CurrentIndent);

/// <summary>
/// YAMLのシーケンスノードを解析して生成します。
/// </summary>
/// <param name="In_YAMLLines">解析対象となるYAMLの行データへの参照。</param>
/// <param name="In_CurrentIndent">現在のインデントレベル。シーケンスの開始位置を示します。</param>
/// <returns>解析されたYAMLシーケンスノードへのstd::shared_ptr。</returns>
std::shared_ptr<YAMLNode> ParseSeq(YAMLLines& In_YAMLLines, size_t In_CurrentIndent);

std::shared_ptr<YAMLNode> ParseNode(YAMLLines& In_YAMLLines, size_t In_CurrentIndent)
{
	if (In_YAMLLines.eof()) return std::make_shared<YAMLNode>(std::string{});

	// 現在の行を取得し、インデントのカウントとトリミングを行います。
	const std::string& line = In_YAMLLines.peek();
	size_t indent = IndentCounter(line);
	std::string trimmed = TrimLeftWhitespace(line);

	if (trimmed.empty() || trimmed[0] == '#')
	{
		In_YAMLLines.next();
		return ParseNode(In_YAMLLines, In_CurrentIndent);
	}
	if (trimmed[0] == '-')
	{
		return ParseSeq(In_YAMLLines, indent);
	}
	if (trimmed.find(':') != std::string::npos)
	{
		return ParseMap(In_YAMLLines, indent);
	}

	In_YAMLLines.next();
	return std::make_shared<YAMLNode>(trimmed);
}

std::shared_ptr<YAMLNode> ParseMap(YAMLLines& In_YAMLLines, size_t In_CurrentIndent)
{
	YAMLMap map;
	while (!In_YAMLLines.eof())
	{
		const std::string& line = In_YAMLLines.peek();
		if (line.empty() || line.find_first_not_of(" \t") == std::string::npos || line[0] == '#')
		{
			In_YAMLLines.next();
			continue;
		}
		size_t indent = IndentCounter(line);

		// インデントが現在のインデントと一致しない場合は終了
		if (indent != In_CurrentIndent) break;

		std::string trimmed = TrimLeftWhitespace(line);
		auto colon_pos = trimmed.find(':');
		if (colon_pos == std::string::npos) break;

		std::string key = trimmed.substr(0, colon_pos);
		std::string val = TrimLeftWhitespace(trimmed.substr(colon_pos + 1));
		In_YAMLLines.next();

		// 子ノードあり（値が空 or 値の右が何もなければ）
		if (val.empty() && !In_YAMLLines.eof())
		{
			size_t next_indent = In_YAMLLines.eof() ? 0 : IndentCounter(In_YAMLLines.peek());
			if (next_indent > indent)
			{
				auto child = ParseNode(In_YAMLLines, next_indent);
				map[key] = child;
				continue;
			}
		}
		// 値のみ
		map[key] = std::make_shared<YAMLNode>(val);
	}

	return std::make_shared<YAMLNode>(map);
}

std::shared_ptr<YAMLNode> ParseSeq(YAMLLines& In_YAMLLines, size_t In_CurrentIndent)
{
	YAMLSeq seq;
	while (!In_YAMLLines.eof())
	{
		const std::string& line = In_YAMLLines.peek();
		if (line.empty() || line.find_first_not_of(" \t") == std::string::npos || line[0] == '#')
		{
			In_YAMLLines.next();
			continue;
		}
		size_t indent = IndentCounter(line);

		// インデントが現在のインデントと一致しない場合は終了
        if (indent < In_CurrentIndent) break;
        // インデントが大きい場合は親ノードで処理するのでスキップ
        if (indent > In_CurrentIndent) { In_YAMLLines.next(); continue; }

		std::string trimmed = TrimLeftWhitespace(line);
		if (trimmed[0] != '-') break;

		std::string after_dash = TrimLeftWhitespace(trimmed.substr(1));
		In_YAMLLines.next();

		// パターン1: - item3: （リスト要素がマップのとき）
		YAMLMap one_map;
		bool is_map = false;

		// 1行目にコロンがある場合（- key: value）
		auto colon_pos = after_dash.find(':');
		if (colon_pos != std::string::npos)
		{
			std::string map_key = after_dash.substr(0, colon_pos);
			std::string map_val = TrimLeftWhitespace(after_dash.substr(colon_pos + 1));
			one_map[map_key] = std::make_shared<YAMLNode>(map_val);
			is_map = true;
		}
		else if (!after_dash.empty())
		{
			// スカラー要素
			seq.push_back(std::make_shared<YAMLNode>(after_dash));
			continue;
		}

		// 続く同じインデントのマップ要素をまとめて追加
		while (!In_YAMLLines.eof())
		{
			const std::string& next_line = In_YAMLLines.peek();
			size_t next_indent = IndentCounter(next_line);
			if (next_indent != In_CurrentIndent + 2) break; // 2スペース分下がっているか

			std::string next_trimmed = TrimLeftWhitespace(next_line);
			auto next_colon = next_trimmed.find(':');
			if (next_colon == std::string::npos) break;

			std::string key = next_trimmed.substr(0, next_colon);
			std::string val = TrimLeftWhitespace(next_trimmed.substr(next_colon + 1));
			one_map[key] = std::make_shared<YAMLNode>(val);
			is_map = true;
			In_YAMLLines.next();
		}

		if (is_map)
		{
			seq.push_back(std::make_shared<YAMLNode>(one_map));
		}
	}

	return std::make_shared<YAMLNode>(seq);
}

std::shared_ptr<YAMLNode> ParseYAML(const std::string& In_YAML)
{
	std::istringstream iss(In_YAML);
	std::string currentLine;
	YAMLLines yamlLines;

	while (std::getline(iss, currentLine))
		yamlLines.lines.push_back(currentLine);

	return ParseNode(yamlLines, 0);
}

void Print_YAML(const std::shared_ptr<YAMLNode>& In_Node, int In_IndentDepth = 0)
{
	if (!In_Node) return;
	const std::string strIndent(In_IndentDepth, ' ');

	switch (In_Node->type)
	{
	case YAMLNode::Type::Scalar:
		std::cout << strIndent << std::get<YAMLScalar>(In_Node->value) << "\n";
		break;
	case YAMLNode::Type::Sequence:
		for (const auto& seqNode : std::get<YAMLSeq>(In_Node->value))
		{
			switch (seqNode->type)
			{
			case YAMLNode::Type::Scalar:
				std::cout << strIndent << "- ";
				Print_YAML(seqNode, 0);
				break;
			case YAMLNode::Type::Sequence:
				std::cout << strIndent << "- \n";
				Print_YAML(seqNode, In_IndentDepth + 2);
				break;
			case YAMLNode::Type::Map:
				std::cout << strIndent << "- \n";
				for (const auto& kv : std::get<YAMLMap>(seqNode->value))
				{
					std::cout << strIndent << "  " << kv.first << ":";
					if (kv.second->type == YAMLNode::Type::Scalar)
					{
						std::cout << " " << std::get<YAMLScalar>(kv.second->value) << "\n";
					}
					else
					{
						std::cout << "\n";
						Print_YAML(kv.second, In_IndentDepth + 4);
					}
				}
				break;
			default:
				break;
			}
		}
		break;
	case YAMLNode::Type::Map:
		for (const auto& keyValue : std::get<YAMLMap>(In_Node->value))
		{
			std::cout << strIndent << keyValue.first << ":";
			if (keyValue.second->type == YAMLNode::Type::Scalar)
			{
				std::cout << " " << std::get<YAMLScalar>(keyValue.second->value) << "\n";
			}
			else
			{
				std::cout << "\n";
				Print_YAML(keyValue.second, In_IndentDepth + 2);
			}
		}
		break;
	default:
		break;
	}
}
