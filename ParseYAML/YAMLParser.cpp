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

	// �s�̐擪����X�y�[�X�ƃ^�u���J�E���g
	while (IndentCnt < In_Line.size() &&
		(In_Line[IndentCnt] == ' ' || In_Line[IndentCnt] == '\t'))
		++IndentCnt;

	return IndentCnt;
}

static std::string TrimLeftWhitespace(const std::string& In_str)
{
	if (In_str.empty()) return In_str;

	// YAML�̍s�̐擪����󔒕�������菜��
	size_t idxCnt = In_str.find_first_not_of(" \t\n\r\f\v");
	return (idxCnt == std::string::npos) ? "" : In_str.substr(idxCnt);
}

/// <summary>
/// YAMLLines�\���̂́AYAML�t�@�C���̊e�s���Ǘ����A���݂̈ʒu��I�[����Ȃǂ̑����񋟂��܂��B
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
/// YAML�̍s�f�[�^����m�[�h����͂��A���L�|�C���^�Ƃ��ĕԂ��܂��B
/// </summary>
/// <param name="In_YAMLLines">��͑ΏۂƂȂ�YAML�̍s�f�[�^�B</param>
/// <param name="In_CurrentIndent">���݂̃C���f���g���x���B�m�[�h�̊K�w�\���𔻒f���邽�߂Ɏg�p����܂��B</param>
/// <returns>��͂��ꂽYAML�m�[�h�ւ�std::shared_ptr�B</returns>
std::shared_ptr<YAMLNode> ParseNode(YAMLLines& In_YAMLLines, size_t In_CurrentIndent);

/// <summary>
/// YAML�̃}�b�v�i�����j�m�[�h����͂��Đ������܂��B
/// </summary>
/// <param name="In_YAMLLines">��͑ΏۂƂȂ�YAML�̍s�f�[�^�ւ̎Q�ƁB</param>
/// <param name="In_CurrentIndent">���݂̃C���f���g���x���B�}�b�v�̊K�w�𔻒肷�邽�߂Ɏg�p����܂��B</param>
/// <returns>��͂��ꂽYAML�}�b�v�m�[�h�ւ�std::shared_ptr�B</returns>
std::shared_ptr<YAMLNode> ParseMap(YAMLLines& In_YAMLLines, size_t In_CurrentIndent);

/// <summary>
/// YAML�̃V�[�P���X�m�[�h����͂��Đ������܂��B
/// </summary>
/// <param name="In_YAMLLines">��͑ΏۂƂȂ�YAML�̍s�f�[�^�ւ̎Q�ƁB</param>
/// <param name="In_CurrentIndent">���݂̃C���f���g���x���B�V�[�P���X�̊J�n�ʒu�������܂��B</param>
/// <returns>��͂��ꂽYAML�V�[�P���X�m�[�h�ւ�std::shared_ptr�B</returns>
std::shared_ptr<YAMLNode> ParseSeq(YAMLLines& In_YAMLLines, size_t In_CurrentIndent);

/// <summary>
/// YAML�̕����s�X�J���[�l����͂��A������Ƃ��ĕԂ��܂��B
/// </summary>
/// <param name="In_YAMLLines">YAML�̊e�s�����ɒ񋟂���YAMLLines�I�u�W�F�N�g�B</param>
/// <param name="In_BaseIndent">��͂��J�n�����C���f���g���i�X�y�[�X���j�B</param>
/// <returns>��͂��ꂽ�����s�X�J���[�l�̓��e���܂�std::string�B</returns>
std::string ParseMultilineScalar(YAMLLines& In_YAMLLines, size_t In_BaseIndent)
{
	std::vector<std::string> lines;
	size_t minIndent = std::string::npos;

	while (!In_YAMLLines.eof())
	{
		// ���݂̍s���擾
		const std::string& line = In_YAMLLines.peek();

		// ��s�͖���
		if (line.empty())
		{
			lines.push_back("");
			In_YAMLLines.next();
			continue;
		}

		size_t indent = IndentCounter(line);
		if (indent < In_BaseIndent) break;

		lines.push_back(line);
		// �C���f���g�̍ŏ��l���X�V
		if (line.find_first_not_of(" \t") != std::string::npos)
			minIndent = std::min(minIndent, indent);

		In_YAMLLines.next();
	}

	// �ŏ��C���f���g��������Ȃ��ꍇ�́A��C���f���g���g�p
	if (minIndent == std::string::npos) minIndent = In_BaseIndent;

	// �ŏ��C���f���g����Ɋe�s�̐擪�̋󔒂��폜���܂��B
	for (auto& lineContent : lines)
	{
		// �s�̐擪����ŏ��C���f���g���̋󔒂��폜
		if (lineContent.size() >= minIndent)
			lineContent = lineContent.substr(minIndent);

		// �s����\r������
		while (!lineContent.empty() && (lineContent.back() == '\r'))
			lineContent.pop_back();
	}

	// �s�̓��e����������1�̕�����ɂ��܂��B
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
	// EOF�`�F�b�N
	if (In_YAMLLines.eof()) return std::make_shared<YAMLNode>(std::string{});

	// ���݂̍s���擾���A�C���f���g�̃J�E���g�ƃg���~���O���s���܂��B
	const std::string& line = In_YAMLLines.peek();
	size_t indent = IndentCounter(line);
	std::string trimmed = TrimLeftWhitespace(line);

	// �C���f���g�����݂̃C���f���g��菬�����ꍇ�́A��͂��I�����܂��B
	if (trimmed.empty() || trimmed[0] == '#')
	{
		In_YAMLLines.next();
		return ParseNode(In_YAMLLines, In_CurrentIndent);
	}
	// �V�[�P���X�̊J�n������ '-' ������ꍇ
	if (trimmed[0] == '-')
	{
		return ParseSeq(In_YAMLLines, indent);
	}
	// �}�b�v�̃L�[�ƒl�̃y�A������ ':' ������ꍇ
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

		// ��s��R�����g�s�̓X�L�b�v
		if (line.empty() || line.find_first_not_of(" \t") == std::string::npos || line[0] == '#')
		{
			In_YAMLLines.next();
			continue;
		}
		size_t indent = IndentCounter(line);

		// ���݂̃C���f���g��菬�����ꍇ�͏I��
		if (indent < In_CurrentIndent) break;
		// ���݂̃C���f���g���傫���ꍇ�̓X�L�b�v
		if (indent > In_CurrentIndent) continue;

		std::string trimmed = TrimLeftWhitespace(line);
		auto colon_pos = trimmed.find(':');

		// �L�[�ƒl�̃y�A���Ȃ��ꍇ�̓X�L�b�v
		if (colon_pos == std::string::npos)
		{
			In_YAMLLines.next();
			continue;
		}

		std::string key = trimmed.substr(0, colon_pos);
		std::string val = TrimLeftWhitespace(trimmed.substr(colon_pos + 1));
		In_YAMLLines.next();

		// ����if���ׂ̈ɁA�L�[�ƒl�̗������g����
		val.erase(std::find_if(val.rbegin(), val.rend(), [](unsigned char ch)
			{ return !std::isspace(ch); }).base(), val.end());

		// �}���`���C���X�J���[�̏���
		if (val == "|" || val == ">")
		{
			std::string multi = ParseMultilineScalar(In_YAMLLines, In_CurrentIndent + 2);
			map[key] = std::make_shared<YAMLNode>(multi);
			continue;
		}

		// ���̍s�������C���f���g�� "key: value" �Ȃ�A�l�X�g���ꂽ�}�b�v�Ƃ��ĉ��
		if (!In_YAMLLines.eof())
		{
			const std::string& next_line = In_YAMLLines.peek();
			size_t next_indent = IndentCounter(next_line);
			std::string next_trimmed = TrimLeftWhitespace(next_line);

			// ���̍s�������C���f���g�ŁA���L�[�ƒl�̃y�A������ꍇ
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

		// ��s��R�����g�s�̓X�L�b�v
		if (line.empty() || line.find_first_not_of(" \t") == std::string::npos || line[0] == '#')
		{
			In_YAMLLines.next();
			continue;
		}
		size_t indent = IndentCounter(line);

		// ���݂̃C���f���g��菬�����ꍇ�͏I��
		if (indent < In_CurrentIndent) break;
		// ���݂̃C���f���g���傫���ꍇ�̓X�L�b�v
		if (indent > In_CurrentIndent)
		{
			In_YAMLLines.next();
			continue;
		}

		std::string trimmed = TrimLeftWhitespace(line);
		if (trimmed[0] != '-') break;

		std::string after_dash = TrimLeftWhitespace(trimmed.substr(1));
		In_YAMLLines.next();

		// 1�s�X�J���[
		if (!after_dash.empty() && after_dash.find(':') == std::string::npos)
		{
			seq.push_back(std::make_shared<YAMLNode>(after_dash));
			continue;
		}

		// �}�b�v�v�f
		YAMLMap one_map;
		// 1�s�ڂ� "key: value" �Ȃ�
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

		// ���������C���f���g�� "key: value" �s�����ׂă}�b�v�ɒǉ�
		while (!In_YAMLLines.eof())
		{
			const std::string& next_line = In_YAMLLines.peek();
			size_t next_indent = IndentCounter(next_line);
			std::string next_trimmed = TrimLeftWhitespace(next_line);

			// ���̗v�f�������C���f���g�� "key: value" �Ȃ�}�b�v�ɒǉ�
			if (next_indent == In_CurrentIndent + 2 && next_trimmed.find(':') != std::string::npos && next_trimmed[0] != '-')
			{
				auto colon_pos = next_trimmed.find(':');
				std::string key = next_trimmed.substr(0, colon_pos);
				std::string val = TrimLeftWhitespace(next_trimmed.substr(colon_pos + 1));
				In_YAMLLines.next();

				// �l�X�g������ꍇ
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
	// ���͂���̏ꍇ�͋�̃}�b�v��Ԃ�
	if (In_YAML.empty()) return std::make_shared<YAMLNode>(YAMLMap{});

	// ��͂��s�����߂ɍs���Ƃɕ���
	std::istringstream iss(In_YAML);
	std::string currentLine;
	YAMLLines yamlLines;
	while (std::getline(iss, currentLine))
		yamlLines.lines.push_back(currentLine);

	YAMLMap topMap;

	// EOF�ɒB����܂�YAML�̍s�����
	while (!yamlLines.eof())
	{
		// ��s��R�����g�̓X�L�b�v
		const std::string& line = yamlLines.peek();
		std::string trimmed = TrimLeftWhitespace(line);

		// ��s��R�����g�s�͖���
		if (trimmed.empty() || trimmed[0] == '#')
		{
			yamlLines.next();
			continue;
		}
		size_t indent = IndentCounter(line);

		// �g�b�v���x���ȊO�͖���
		if (indent != 0) break;

		// �L�[���擾
		auto colon_pos = trimmed.find(':');
		if (colon_pos == std::string::npos) break;
		std::string key = trimmed.substr(0, colon_pos);
		std::string val = TrimLeftWhitespace(trimmed.substr(colon_pos + 1));
		yamlLines.next();

		// �q�m�[�h������ꍇ
		if (!yamlLines.eof())
		{
			const std::string& next_line = yamlLines.peek();
			size_t next_indent = IndentCounter(next_line);
			std::string next_trimmed = TrimLeftWhitespace(next_line);

			// ���̍s�������C���f���g��
			if (next_indent > indent)
			{
				topMap[key] = ParseNode(yamlLines, next_indent);
				continue;
			}
		}
		// �l�̂�
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
