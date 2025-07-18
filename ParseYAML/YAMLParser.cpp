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
	bool eof() const noexcept { return currentPos >= lines.size(); }
	void next() noexcept { ++currentPos; }
	const std::string& peek() const { return lines[currentPos]; }
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

/// <summary>
/// YAMLの複数行スカラー値を解析し、文字列として返します。
/// </summary>
/// <param name="In_YAMLLines">YAMLの各行を順に提供するYAMLLinesオブジェクト。</param>
/// <param name="In_BaseIndent">解析を開始する基準インデント幅（スペース数）。</param>
/// <returns>解析された複数行スカラー値の内容を含むstd::string。</returns>
std::string ParseMultilineScalar(YAMLLines& In_YAMLLines, size_t In_BaseIndent)
{
	std::vector<std::string> lines;
	size_t minIndent = std::string::npos;

	while (!In_YAMLLines.eof())
	{
		// 現在の行を取得
		const std::string& line = In_YAMLLines.peek();

		// 空行は無視
		if (line.empty())
		{
			lines.push_back("");
			In_YAMLLines.next();
			continue;
		}

		size_t indent = IndentCounter(line);
		if (indent < In_BaseIndent) break;

		lines.push_back(line);
		// インデントの最小値を更新
		if (line.find_first_not_of(" \t") != std::string::npos)
			minIndent = std::min(minIndent, indent);

		In_YAMLLines.next();
	}

	// 最小インデントが見つからない場合は、基準インデントを使用
	if (minIndent == std::string::npos) minIndent = In_BaseIndent;

	// 最小インデントを基準に各行の先頭の空白を削除します。
	for (auto& lineContent : lines)
	{
		// 行の先頭から最小インデント分の空白を削除
		if (lineContent.size() >= minIndent)
			lineContent = lineContent.substr(minIndent);

		// 行末の\rを除去
		while (!lineContent.empty() && (lineContent.back() == '\r'))
			lineContent.pop_back();
	}

	// 行の内容を結合して1つの文字列にします。
	std::string result;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		result += lines[i];
		if (i + 1 < lines.size()) result += "\n";
	}

	return result;
}

std::shared_ptr<YAMLNode> ParseNode(YAMLLines& In_YAMLLines, size_t In_CurrentIndent)
{
	// EOFチェック
	if (In_YAMLLines.eof()) return std::make_shared<YAMLNode>(std::string{});

	// 現在の行を取得し、インデントのカウントとトリミングを行います。
	const std::string& line = In_YAMLLines.peek();
	size_t indent = IndentCounter(line);
	std::string trimmed = TrimLeftWhitespace(line);

	// インデントが現在のインデントより小さい場合は、解析を終了します。
	if (trimmed.empty() || trimmed[0] == '#')
	{
		In_YAMLLines.next();
		return ParseNode(In_YAMLLines, In_CurrentIndent);
	}
	// シーケンスの開始を示す '-' がある場合
	if (trimmed[0] == '-')
	{
		return ParseSeq(In_YAMLLines, indent);
	}
	// マップのキーと値のペアを示す ':' がある場合
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

		// 空行やコメント行はスキップ
		if (line.empty() || line.find_first_not_of(" \t") == std::string::npos || line[0] == '#')
		{
			In_YAMLLines.next();
			continue;
		}
		size_t indent = IndentCounter(line);

		// 現在のインデントより小さい場合は終了
		if (indent < In_CurrentIndent) break;
		// 現在のインデントより大きい場合はスキップ
		if (indent > In_CurrentIndent) continue;

		std::string trimmed = TrimLeftWhitespace(line);
		auto colon_pos = trimmed.find(':');

		// キーと値のペアがない場合はスキップ
		if (colon_pos == std::string::npos)
		{
			In_YAMLLines.next();
			continue;
		}

		std::string key = trimmed.substr(0, colon_pos);
		std::string val = TrimLeftWhitespace(trimmed.substr(colon_pos + 1));
		In_YAMLLines.next();

		// 次のif文の為に、キーと値の両方をトリム
		val.erase(std::find_if(val.rbegin(), val.rend(), [](unsigned char ch)
			{ return !std::isspace(ch); }).base(), val.end());

		// マルチラインスカラーの処理
		if (val == "|" || val == ">")
		{
			std::string multi = ParseMultilineScalar(In_YAMLLines, In_CurrentIndent + 2);
			map[key] = std::make_shared<YAMLNode>(multi);
			continue;
		}

		// 次の行が同じインデントで "key: value" なら、ネストされたマップとして解析
		if (!In_YAMLLines.eof())
		{
			const std::string& next_line = In_YAMLLines.peek();
			size_t next_indent = IndentCounter(next_line);
			std::string next_trimmed = TrimLeftWhitespace(next_line);

			// 次の行が同じインデントで、かつキーと値のペアがある場合
			if ((val.empty() && next_indent > indent && !next_trimmed.empty()) ||
				(next_indent > indent && !next_trimmed.empty() && (next_trimmed[0] == '-' || next_trimmed.find(':') != std::string::npos)))
			{
				map[key] = ParseNode(In_YAMLLines, next_indent);
				continue;
			}
		}

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

		// 空行やコメント行はスキップ
		if (line.empty() || line.find_first_not_of(" \t") == std::string::npos || line[0] == '#')
		{
			In_YAMLLines.next();
			continue;
		}
		size_t indent = IndentCounter(line);

		// 現在のインデントより小さい場合は終了
		if (indent < In_CurrentIndent) break;
		// 現在のインデントより大きい場合はスキップ
		if (indent > In_CurrentIndent)
		{
			In_YAMLLines.next();
			continue;
		}

		std::string trimmed = TrimLeftWhitespace(line);
		if (trimmed[0] != '-') break;

		std::string after_dash = TrimLeftWhitespace(trimmed.substr(1));
		In_YAMLLines.next();

		// 1行スカラー
		if (!after_dash.empty() && after_dash.find(':') == std::string::npos)
		{
			seq.push_back(std::make_shared<YAMLNode>(after_dash));
			continue;
		}

		// マップ要素
		YAMLMap one_map;
		// 1行目が "key: value" なら
		if (!after_dash.empty())
		{
			auto colon_pos = after_dash.find(':');
			if (colon_pos != std::string::npos)
			{
				std::string key = after_dash.substr(0, colon_pos);
				std::string val = TrimLeftWhitespace(after_dash.substr(colon_pos + 1));
				one_map[key] = std::make_shared<YAMLNode>(val);
			}
		}

		// 続く同じインデントの "key: value" 行をすべてマップに追加
		while (!In_YAMLLines.eof())
		{
			const std::string& next_line = In_YAMLLines.peek();
			size_t next_indent = IndentCounter(next_line);
			std::string next_trimmed = TrimLeftWhitespace(next_line);

			// 次の要素が同じインデントで "key: value" ならマップに追加
			if (next_indent == In_CurrentIndent + 2 && next_trimmed.find(':') != std::string::npos && next_trimmed[0] != '-')
			{
				auto colon_pos = next_trimmed.find(':');
				std::string key = next_trimmed.substr(0, colon_pos);
				std::string val = TrimLeftWhitespace(next_trimmed.substr(colon_pos + 1));
				In_YAMLLines.next();

				// ネストがある場合
				if (!In_YAMLLines.eof())
				{
					const std::string& peek_line = In_YAMLLines.peek();
					size_t peek_indent = IndentCounter(peek_line);
					if (peek_indent > next_indent)
					{
						one_map[key] = ParseNode(In_YAMLLines, peek_indent);
						continue;
					}
				}
				one_map[key] = std::make_shared<YAMLNode>(val);
			}
			else
			{
				break;
			}
		}
		seq.push_back(std::make_shared<YAMLNode>(one_map));
	}
	return std::make_shared<YAMLNode>(seq);
}

std::shared_ptr<YAMLNode> ParseYAML(const std::string& In_YAML)
{
	// 入力が空の場合は空のマップを返す
	if (In_YAML.empty()) return std::make_shared<YAMLNode>(YAMLMap{});

	// 解析を行うために行ごとに分割
	std::istringstream iss(In_YAML);
	std::string currentLine;
	YAMLLines yamlLines;
	while (std::getline(iss, currentLine))
		yamlLines.lines.push_back(currentLine);

	YAMLMap topMap;

	// EOFに達するまでYAMLの行を解析
	while (!yamlLines.eof())
	{
		// 空行やコメントはスキップ
		const std::string& line = yamlLines.peek();
		std::string trimmed = TrimLeftWhitespace(line);

		// 空行やコメント行は無視
		if (trimmed.empty() || trimmed[0] == '#')
		{
			yamlLines.next();
			continue;
		}
		size_t indent = IndentCounter(line);

		// トップレベル以外は無視
		if (indent != 0) break;

		// キー名取得
		auto colon_pos = trimmed.find(':');
		if (colon_pos == std::string::npos) break;
		std::string key = trimmed.substr(0, colon_pos);
		std::string val = TrimLeftWhitespace(trimmed.substr(colon_pos + 1));
		yamlLines.next();

		// 子ノードがある場合
		if (!yamlLines.eof())
		{
			const std::string& next_line = yamlLines.peek();
			size_t next_indent = IndentCounter(next_line);
			std::string next_trimmed = TrimLeftWhitespace(next_line);

			// 次の行が同じインデントの
			if (next_indent > indent)
			{
				topMap[key] = ParseNode(yamlLines, next_indent);
				continue;
			}
		}
		// 値のみ
		topMap[key] = std::make_shared<YAMLNode>(val);
	}
	return std::make_shared<YAMLNode>(topMap);
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
