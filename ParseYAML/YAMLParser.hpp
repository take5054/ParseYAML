#pragma once

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

class YAMLParser
{
public:
	class YAMLNode;
	using YAMLMap = std::unordered_map<std::string, std::shared_ptr<YAMLNode>>;
	using YAMLSeq = std::vector<std::shared_ptr<YAMLNode>>;
	using YAMLScalar = std::string;

	class YAMLNode
	{
	public:
		enum class Type
		{
			Scalar,
			Sequence,
			Map
		};

		YAMLNode(YAMLScalar val) : type(Type::Scalar), value(val) {}
		YAMLNode(YAMLSeq val) : type(Type::Sequence), value(val) {}
		YAMLNode(YAMLMap val) : type(Type::Map), value(val) {}

		Type type;
		std::variant<YAMLScalar, YAMLSeq, YAMLMap> value;
	};

	static inline std::shared_ptr<YAMLNode> ParseYAML(_In_ const std::string& In_FilePath)
	{
		if (In_FilePath.empty()) return std::make_shared<YAMLNode>(YAMLMap{});

		std::ifstream ifs(In_FilePath, std::ios::binary);
		if (!ifs)
		{
			std::cerr << "ファイルを開けません: " << In_FilePath << std::endl;
			return std::make_shared<YAMLNode>(YAMLMap{});
		}

		// データの読み込み
		std::ostringstream buffer;
		buffer << ifs.rdbuf();

		// Shift_JISからUTF-8への変換
		std::istringstream iss(Convert_UTF8_To_ShiftJIS(buffer.str()));

		// 文字列を行ごとに分割
		std::string currentLine;
		YAMLLines yamlLines;
		while (std::getline(iss, currentLine))
			yamlLines.lines.push_back(currentLine);

		YAMLMap topMap;

		while (!yamlLines.eof())
		{
			const std::string& line = yamlLines.peek();
			const std::string trimmed = TrimLeftWhitespace(line);

			if (trimmed.empty() || trimmed[0] == '#')
			{
				yamlLines.next();
				continue;
			}
			const size_t indent = IndentCounter(line);

			if (indent != 0) break;

			const size_t colon_pos = trimmed.find(':');
			if (colon_pos == std::string::npos) break;
			const std::string key = trimmed.substr(0, colon_pos);
			yamlLines.next();

			if (!yamlLines.eof())
			{
				const std::string& next_line = yamlLines.peek();
				const size_t next_indent = IndentCounter(next_line);

				if (next_indent > indent)
				{
					topMap[key] = ParseNode(yamlLines, next_indent);
					continue;
				}
			}
			topMap[key] = std::make_shared<YAMLNode>(TrimLeftWhitespace(trimmed.substr(colon_pos + 1)));
		}
		return std::make_shared<YAMLNode>(topMap);
	}

	static inline void Print_YAML(_In_ const std::shared_ptr<YAMLNode>& In_Node, _In_ const int& In_IndentDepth = 0)
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

private:
	struct YAMLLines
	{
		std::vector<std::string> lines;
		size_t currentPos = 0;
		inline bool eof() const noexcept { return currentPos >= lines.size(); }
		inline void next() noexcept { ++currentPos; }
		inline const std::string& peek() const { return lines[currentPos]; }
	};

	static inline size_t IndentCounter(_In_ const std::string& In_Line) noexcept
	{
		size_t IndentCnt = 0;
		while (IndentCnt < In_Line.size() &&
			(In_Line[IndentCnt] == ' ' || In_Line[IndentCnt] == '\t'))
			++IndentCnt;
		return IndentCnt;
	}

	static inline std::string TrimLeftWhitespace(_In_ const std::string& In_str) noexcept
	{
		if (In_str.empty()) return In_str;
		const size_t idxCnt = In_str.find_first_not_of(" \t\n\r\f\v");
		return (idxCnt == std::string::npos) ? "" : In_str.substr(idxCnt);
	}

	static inline std::string Convert_UTF8_To_ShiftJIS(_In_ const std::string_view& In_Source)
	{
		const char* text = In_Source.data();
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(text), -1, NULL, 0);
		std::wstring wideText(size_needed, 0);
		MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(text), -1, &wideText[0], size_needed);

		size_needed = WideCharToMultiByte(932, 0, wideText.c_str(), -1, NULL, 0, NULL, NULL);
		std::string shiftJISText(size_needed, 0);
		WideCharToMultiByte(932, 0, wideText.c_str(), -1, &shiftJISText[0], size_needed, NULL, NULL);

		return shiftJISText;
	}

	static inline std::string ParseMultilineScalar(_Inout_ YAMLLines& In_YAMLLines, _In_ const size_t& In_BaseIndent)
	{
		std::vector<std::string> lines;
		size_t minIndent = std::string::npos;

		while (!In_YAMLLines.eof())
		{
			const std::string& line = In_YAMLLines.peek();
			if (line.empty())
			{
				lines.push_back("");
				In_YAMLLines.next();
				continue;
			}
			const size_t indent = IndentCounter(line);
			if (indent < In_BaseIndent) break;

			lines.push_back(line);
			if (line.find_first_not_of(" \t") != std::string::npos)
#undef min
				minIndent = std::min(minIndent, indent);

			In_YAMLLines.next();
		}

		if (minIndent == std::string::npos) minIndent = In_BaseIndent;

		for (auto& lineContent : lines)
		{
			if (lineContent.size() >= minIndent)
				lineContent = lineContent.substr(minIndent);
			while (!lineContent.empty() && (lineContent.back() == '\r'))
				lineContent.pop_back();
		}

		std::string result;
		for (size_t i = 0; i < lines.size(); ++i)
		{
			result += lines[i];
			if (i + 1 < lines.size()) result += "\n";
		}

		return result;
	}

	static inline std::shared_ptr<YAMLNode> ParseNode(_Inout_ YAMLLines& In_YAMLLines, _In_ const size_t& In_CurrentIndent)
	{
		if (In_YAMLLines.eof()) return std::make_shared<YAMLNode>(std::string{});

		const std::string& line = In_YAMLLines.peek();
		const size_t indent = IndentCounter(line);
		const std::string trimmed = TrimLeftWhitespace(line);

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

	static inline std::shared_ptr<YAMLNode> ParseMap(_Inout_ YAMLLines& In_YAMLLines, _In_ const size_t& In_CurrentIndent)
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
			const size_t indent = IndentCounter(line);
			if (indent < In_CurrentIndent) break;
			if (indent > In_CurrentIndent) continue;

			const std::string trimmed = TrimLeftWhitespace(line);
			const size_t colon_pos = trimmed.find(':');
			if (colon_pos == std::string::npos)
			{
				In_YAMLLines.next();
				continue;
			}

			const std::string key = trimmed.substr(0, colon_pos);
			std::string val = TrimLeftWhitespace(trimmed.substr(colon_pos + 1));
			In_YAMLLines.next();

			val.erase(std::find_if(val.rbegin(), val.rend(), [](unsigned char ch)
				{ return !std::isspace(ch); }).base(), val.end());

			if (val == "|" || val == ">")
			{
				std::string multi = ParseMultilineScalar(In_YAMLLines, In_CurrentIndent + 2);
				map[key] = std::make_shared<YAMLNode>(multi);
				continue;
			}

			if (!In_YAMLLines.eof())
			{
				const std::string& next_line = In_YAMLLines.peek();
				const size_t next_indent = IndentCounter(next_line);
				const std::string next_trimmed = TrimLeftWhitespace(next_line);

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

	static inline std::shared_ptr<YAMLNode> ParseSeq(_Inout_ YAMLLines& In_YAMLLines, _In_ const size_t& In_CurrentIndent)
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
			const size_t indent = IndentCounter(line);
			if (indent < In_CurrentIndent) break;
			if (indent > In_CurrentIndent)
			{
				In_YAMLLines.next();
				continue;
			}

			const std::string trimmed = TrimLeftWhitespace(line);
			if (trimmed[0] != '-') break;

			const std::string after_dash = TrimLeftWhitespace(trimmed.substr(1));
			In_YAMLLines.next();

			if (!after_dash.empty() && after_dash.find(':') == std::string::npos)
			{
				seq.push_back(std::make_shared<YAMLNode>(after_dash));
				continue;
			}

			YAMLMap one_map;
			if (!after_dash.empty())
			{
				const size_t colon_pos = after_dash.find(':');
				if (colon_pos != std::string::npos)
				{
					one_map[after_dash.substr(0, colon_pos)] =
						std::make_shared<YAMLNode>(
							TrimLeftWhitespace(after_dash.substr(colon_pos + 1))
						);
				}
			}

			while (!In_YAMLLines.eof())
			{
				const std::string& next_line = In_YAMLLines.peek();
				const size_t next_indent = IndentCounter(next_line);
				const std::string next_trimmed = TrimLeftWhitespace(next_line);

				if (next_indent == In_CurrentIndent + 2 && next_trimmed.find(':') != std::string::npos && next_trimmed[0] != '-')
				{
					const size_t colon_pos = next_trimmed.find(':');
					const std::string key = next_trimmed.substr(0, colon_pos);
					In_YAMLLines.next();

					if (!In_YAMLLines.eof())
					{
						const std::string& peek_line = In_YAMLLines.peek();
						const size_t peek_indent = IndentCounter(peek_line);
						if (peek_indent > next_indent)
						{
							one_map[key] = ParseNode(In_YAMLLines, peek_indent);
							continue;
						}
					}
					one_map[key] = std::make_shared<YAMLNode>(TrimLeftWhitespace(next_trimmed.substr(colon_pos + 1)));
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

};
