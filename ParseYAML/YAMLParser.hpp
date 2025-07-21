#pragma once

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <charconv>
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

	// YAML ドキュメント内のノード(スカラー値、シーケンス、マップ)を表します。
	class YAMLNode
	{
	public:
		// YAMLノードのタイプを定義します。
		enum class Type
		{
			Scalar,
			Sequence,
			Map
		};

		// マルチラインスカラーのタイプを定義します。
		enum class MultilineType
		{
			None,
			Literal,   // |
			Folded     // >
		};

		YAMLNode(YAMLScalar val, MultilineType mtype = MultilineType::None)
			: type(Type::Scalar), value(val), multilineType(mtype) {}
		YAMLNode(YAMLSeq val) : type(Type::Sequence), value(val), multilineType(MultilineType::None) {}
		YAMLNode(YAMLMap val) : type(Type::Map), value(val), multilineType(MultilineType::None) {}

		Type type;	// ノードのタイプ
		std::variant<YAMLScalar, YAMLSeq, YAMLMap> value;	// ノードの値
		MultilineType multilineType = MultilineType::None;	// マルチラインスカラーのタイプ
	};

	/// <summary>
	/// 指定されたファイルパスからYAMLファイルを読み込み、パースします。
	/// </summary>
	/// <param name="In_FilePath">読み込むYAMLファイルのパス。</param>
	void ParseYAML(_In_ const std::string& In_FilePath)
	{
		if (In_FilePath.empty()) return;

		std::ifstream ifs(In_FilePath, std::ios::binary);
		if (!ifs)
		{
			std::cerr << "ファイルを開けません: " << In_FilePath << std::endl;
			return;
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
		m_YAMLData = YAMLNode(topMap);
	}

	/// <summary>
	/// 指定されたファイルパスにYAMLデータを保存します。
	/// </summary>
	/// <param name="In_FilePath">保存先のファイルパスを指定します。</param>
	/// <returns>保存に成功した場合は true、失敗した場合は false を返します。</returns>
	inline bool SaveYAML(_In_ const std::string& In_FilePath) const
	{
		if (In_FilePath.empty()) return false;
		std::ofstream ofs(In_FilePath, std::ios::binary);
		if (!ofs)
		{
			std::cerr << "ファイルを保存できません: " << In_FilePath << std::endl;
			return false;
		}

		WriteYAMLNode(ofs, m_YAMLData, 0);

		if (!ofs)
		{
			std::cerr << "ファイル書き込みエラー: " << In_FilePath << std::endl;
			return false;
		}
		return true;
	}

	/// <summary>
	/// YAMLデータを取得します。
	/// </summary>
	/// <returns>メンバー変数m_YAMLDataの値を返します。</returns>
	inline YAMLNode GetYAMLData() const noexcept { return m_YAMLData; }

	/// <summary>
	/// キーパスで文字列値を取得
	/// </summary>
	/// <param name="In_keyPath">YAML内のキーのパス（ドット区切り）。例: "users.0.name"</param>
	/// <param name="In_IncludeQuotes">文字列の前後に引用符を含めるかどうか。デフォルトはfalse。</param>
	/// <returns>指定されたキーの値を文字列として返します。キーが存在しない場合は空文字列を返します。</returns>
	inline std::string GetString(_In_ const std::string& In_keyPath, _In_ const bool& In_IncludeQuotes = false) const
	{
		const YAMLScalar* val = FindScalarByPath(In_keyPath);
		if (!val) return "";
		if (In_IncludeQuotes) return *val;

		std::string result = *val;
		// 前後の空白・改行を削除
		result.erase(0, result.find_first_not_of(" \t\n\r"));
		result.erase(result.find_last_not_of(" \t\n\r") + 1);
		// 文字列の整形
		if (!result.empty() && result.front() == '"' && result.back() == '"')
			result = result.substr(1, result.size() - 2);
		else if (!result.empty() && result.front() == '\'' && result.back() == '\'')
			result = result.substr(1, result.size() - 2);

		return result;
	}

	/// <summary>
	/// キーパスでboolを取得
	/// </summary>
	/// <param name="In_keyPath">YAML内のキーのパス（ドット区切り）。例: "users.0.name"</param>
	/// <returns>指定されたキーの値をboolとして返します。キーが存在しない場合はfalseを返します。</returns>
	inline bool GetBool(_In_ const std::string& In_keyPath) const
	{
		const YAMLScalar* val = FindScalarByPath(In_keyPath);
		if (!val) return false;
		return (*val == "true" || *val == "True" || *val == "1");
	}

	/// <summary>
	/// キーパスでintを取得
	/// </summary>
	/// <param name="In_keyPath">YAML内のキーのパス（ドット区切り）。例: "users.0.age"</param>
	/// <returns>指定されたキーの値をintとして返します。変換に失敗した場合は0を返します。</returns>
	inline int GetInt(_In_ const std::string& In_keyPath) const
	{
		const YAMLScalar* val = FindScalarByPath(In_keyPath);
		if (!val) return 0;
		int value = 0;
		auto [ptr, ec] = std::from_chars(val->data(), val->data() + val->size(), value, 10);
		if (ec == std::errc()) return value;
		else
		{
			std::cerr << "整数変換失敗: " << *val << std::endl;
			return 0;
		}
	}

	/// <summary>
	/// キーパスでfloatを取得
	/// </summary>
	/// <param name="In_keyPath">YAML内のキーのパス（ドット区切り）。例: "users.0.height"</param>
	/// <returns>指定されたキーの値をfloatとして返します。変換に失敗した場合は0.0fを返します。</returns>
	inline float GetFloat(_In_ const std::string& In_keyPath) const
	{
		const YAMLScalar* val = FindScalarByPath(In_keyPath);
		if (!val) return 0.0f;
		try
		{
			return std::stof(*val);
		}
		catch (const std::invalid_argument& e)
		{
			std::cerr << "浮動小数点数変換失敗: " << e.what() << std::endl;
		}
		catch (const std::out_of_range& e)
		{
			std::cerr << "浮動小数点数範囲外: " << e.what() << std::endl;
		}
		return 0.0f;
	}

	/// <summary>
	/// キーパスでdoubleを取得
	/// </summary>
	/// <param name="In_keyPath">YAML内のキーのパス（ドット区切り）。例: "users.0.weight"</param>
	/// <returns>指定されたキーの値をdoubleとして返します。変換に失敗した場合は0.0を返します。</returns>
	inline double GetDouble(_In_ const std::string& In_keyPath) const
	{
		const YAMLScalar* val = FindScalarByPath(In_keyPath);
		if (!val) return 0.0;
		try
		{
			return std::stod(*val);
		}
		catch (const std::invalid_argument& e)
		{
			std::cerr << "浮動小数点数変換失敗: " << e.what() << std::endl;
		}
		catch (const std::out_of_range& e)
		{
			std::cerr << "浮動小数点数範囲外: " << e.what() << std::endl;
		}
		return 0.0;
	}

	/// <summary>
	/// YAMLノードの内容をインデント付きで標準出力に表示します。
	/// </summary>
	/// <param name="In_IndentDepth">出力時のインデント幅（スペース数）。デフォルトは0です。</param>
	inline void Print_YAML(_In_ const int& In_IndentDepth = 0) const noexcept
	{
		const std::string strIndent(In_IndentDepth, ' ');

		switch (m_YAMLData.type)
		{
		case YAMLNode::Type::Scalar:
			std::cout << strIndent << std::get<YAMLScalar>(m_YAMLData.value) << "\n";
			break;
		case YAMLNode::Type::Sequence:
			for (const auto& seqNode : std::get<YAMLSeq>(m_YAMLData.value))
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
			for (const auto& keyValue : std::get<YAMLMap>(m_YAMLData.value))
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

	// YAMLデータを保持するメンバー変数
	YAMLNode m_YAMLData = YAMLNode(YAMLMap{});

	/// <summary>
	/// YAMLノードを標準出力にインデント付きで再帰的に表示します。
	/// </summary>
	/// <param name="In_Node">表示するYAMLノードへの共有ポインタ。</param>
	/// <param name="In_IndentDepth">現在のインデント幅（スペース数）。デフォルトは0。</param>
	static void Print_YAML(_In_ const std::shared_ptr<YAMLNode>& In_Node, _In_ const int& In_IndentDepth = 0)
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

	static void WriteYAMLNode(std::ostream& Out_OStream, _In_ const YAMLNode& In_YAMLNode, _In_ const int& In_IndentDepth)
	{
		const std::string strIndent(In_IndentDepth, ' ');
		switch (In_YAMLNode.type)
		{
		case YAMLNode::Type::Scalar:
			WriteScalar(Out_OStream, std::get<YAMLScalar>(In_YAMLNode.value),
				In_YAMLNode.multilineType, In_IndentDepth);
			break;
		case YAMLNode::Type::Sequence:
			for (const auto& seqNode : std::get<YAMLSeq>(In_YAMLNode.value))
			{
				Out_OStream << strIndent << "-";
				if (seqNode->type == YAMLNode::Type::Scalar)
				{
					WriteScalar(Out_OStream, std::get<YAMLScalar>(seqNode->value),
						seqNode->multilineType, In_IndentDepth + 2);
				}
				else
				{
					Out_OStream << "\n";
					WriteYAMLNode(Out_OStream, *seqNode, In_IndentDepth + 2);
				}
			}
			break;
		case YAMLNode::Type::Map:
			for (const auto& keyValue : std::get<YAMLMap>(In_YAMLNode.value))
			{
				Out_OStream << strIndent << keyValue.first << ":";
				if (keyValue.second->type == YAMLNode::Type::Scalar)
				{
					WriteScalar(Out_OStream, std::get<YAMLScalar>(keyValue.second->value),
						keyValue.second->multilineType, In_IndentDepth + 2);
				}
				else
				{
					Out_OStream << "\n";
					WriteYAMLNode(Out_OStream, *keyValue.second, In_IndentDepth + 2);
				}
			}
			break;
		default:
			break;
		}

		// インデント深度が2以下の場合(一番大きな項目が分かれる)
		if (In_IndentDepth <= 2) Out_OStream << "\n"; // 最後に改行を追加
	}

	static inline void WriteScalar(std::ostream& Out_OStream, _In_ const std::string& In_Scalar,
		_In_ const YAMLNode::MultilineType& In_MultilineType, _In_ const int& In_IndentDepth)
	{
		const std::string strIndent(In_IndentDepth, ' ');
		if (In_MultilineType == YAMLNode::MultilineType::Literal)
		{
			Out_OStream << " |\n";
			std::istringstream iss(In_Scalar);
			std::string line;
			while (std::getline(iss, line))
				Out_OStream << strIndent << line << "\n";
		}
		else if (In_MultilineType == YAMLNode::MultilineType::Folded)
		{
			Out_OStream << " >\n";
			std::istringstream iss(In_Scalar);
			std::string line;
			while (std::getline(iss, line))
				Out_OStream << strIndent << line << "\n";
		}
		else
		{
			Out_OStream << " " << In_Scalar << "\n";
		}
	}

	inline const YAMLScalar* FindScalarByPath(_In_ const std::string& In_KeyPath) const
	{
		const YAMLNode* node = &m_YAMLData;
		size_t pos = 0, next;
		while (pos < In_KeyPath.size())
		{
			next = In_KeyPath.find('.', pos);
			const std::string token = In_KeyPath.substr(pos, next == std::string::npos ? std::string::npos : next - pos);

			if (node->type == YAMLNode::Type::Map)
			{
				const auto& map = std::get<YAMLMap>(node->value);
				auto itr = map.find(token);
				if (itr == map.end()) return nullptr;
				node = itr->second.get();
			}
			else if (node->type == YAMLNode::Type::Sequence)
			{
				if (!std::all_of(token.begin(), token.end(), ::isdigit)) return nullptr;
				const size_t idx = std::stoul(token);
				const auto& seq = std::get<YAMLSeq>(node->value);
				if (idx >= seq.size()) return nullptr;
				node = seq[idx].get();
			}
			else
			{
				return nullptr;
			}
			if (next == std::string::npos) break;
			pos = next + 1;
		}
		if (node->type == YAMLNode::Type::Scalar)
		{
			return &std::get<YAMLScalar>(node->value);
		}
		return nullptr;
	}

	/// YAMLファイルの行を管理し、現在の位置を追跡します。
	struct YAMLLines
	{
		std::vector<std::string> lines;	// YAMLファイルの行を格納するベクター
		size_t currentPos = 0;			// 現在の行位置を追跡するインデックス

		// 現在の位置が行数を超えているかどうかを確認します。
		inline bool eof() const noexcept { return currentPos >= lines.size(); }
		// 現在の行位置を取得します。
		inline void next() noexcept { ++currentPos; }
		// 現在の行を取得します。
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

	static std::shared_ptr<YAMLNode> ParseMap(_Inout_ YAMLLines& In_YAMLLines, _In_ const size_t& In_CurrentIndent)
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
				YAMLNode::MultilineType mtype = (val == "|") ? YAMLNode::MultilineType::Literal : YAMLNode::MultilineType::Folded;
				map[key] = std::make_shared<YAMLNode>(multi, mtype);
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

	static std::shared_ptr<YAMLNode> ParseSeq(_Inout_ YAMLLines& In_YAMLLines, _In_ const size_t& In_CurrentIndent)
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
