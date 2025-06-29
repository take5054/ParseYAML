#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

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

std::shared_ptr<YAMLNode> ParseYAML(const std::string&);
void Print_YAML(const std::shared_ptr<YAMLNode>&, int);
