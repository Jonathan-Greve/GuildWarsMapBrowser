#pragma once
#include <dat_comparer_lexer.h>
#include <string>
#include <memory>
#include <vector>
#include <map>


struct ASTNode {
    GWMBTokenType type;
    int value;
    std::shared_ptr<ASTNode> left;
    std::shared_ptr<ASTNode> right;

    ASTNode(GWMBTokenType type, std::string stringValue)
        : type(type), left(nullptr), right(nullptr) {
        if (type == GWMBTokenType::DAT)
            value = (convertToInt(stringValue));
    }

    ASTNode(GWMBTokenType type, std::string stringValue, std::shared_ptr<ASTNode> left, std::shared_ptr<ASTNode> right)
        : type(type), left(left), right(right) {
        if (type == GWMBTokenType::DAT)
            value = (convertToInt(stringValue));
    }
private:
    static int convertToInt(const std::string& str) {
        try {
            return std::stoi(str);
        }
        catch (const std::invalid_argument& e) {
            throw std::runtime_error("Invalid argument: The string \"" + str + "\" is not a valid integer.");
        }
        catch (const std::out_of_range& e) {
            throw std::runtime_error("Out of range: The string \"" + str + "\" is too large for an int.");
        }
    }
};

class Parser {
public:
    Parser(Lexer& lexer) : lexer(lexer) {
        current_token = lexer.getNextToken();
    }

    std::shared_ptr<ASTNode> parse() {
        return expression();
    }

private:
    Lexer& lexer;
    Token current_token;

    std::shared_ptr<ASTNode> expression() {
        auto node = term();

        while (current_token.token_type == GWMBTokenType::OR) {
            Token token = current_token;
            eat(GWMBTokenType::OR);
            node = std::make_shared<ASTNode>(token.token_type, token.value, node, term());
        }

        return node;
    }

    std::shared_ptr<ASTNode> term() {
        auto node = factor();

        while (current_token.token_type == GWMBTokenType::AND || current_token.token_type == GWMBTokenType::XOR) {
            Token token = current_token;
            if (token.token_type == GWMBTokenType::AND) {
                eat(GWMBTokenType::AND);
            }
            else if (token.token_type == GWMBTokenType::XOR) {
                eat(GWMBTokenType::XOR);
            }
            node = std::make_shared<ASTNode>(token.token_type, token.value, node, factor());
        }

        return node;
    }

    std::shared_ptr<ASTNode> factor() {
        Token token = current_token;

        if (token.token_type == GWMBTokenType::NOT) {
            eat(GWMBTokenType::NOT);
            auto node = std::make_shared<ASTNode>(token.token_type, token.value, factor(), nullptr);
            return node;
        }
        else if (token.token_type == GWMBTokenType::OPEN_PAREN) {
            eat(GWMBTokenType::OPEN_PAREN);
            auto node = expression();
            eat(GWMBTokenType::CLOSE_PAREN);
            return node;
        }
        else if (token.token_type == GWMBTokenType::DAT) {
            eat(GWMBTokenType::DAT);
            return std::make_shared<ASTNode>(token.token_type, token.value, nullptr, nullptr);
        }

        throw std::runtime_error("Invalid syntax");
    }

    void eat(GWMBTokenType type) {
        if (current_token.token_type == type) {
            current_token = lexer.getNextToken();
        }
        else {
            throw std::runtime_error("Invalid syntax");
        }
    }
};

inline std::vector<uint32_t> evaluate(const std::shared_ptr<ASTNode>& node, const std::map<int, uint32_t>& DATs_hashes) {
    if (node->type == GWMBTokenType::DAT) {
        auto it = DATs_hashes.find(node->value);
        if (it != DATs_hashes.end()) {
            return { it->second };
        }
        throw std::runtime_error("Undefined data value: " + std::to_string(node->value));
    }

    std::set<uint32_t> result_set;
    std::vector<uint32_t> left_eval;
    std::vector<uint32_t> right_eval;

    if (node->left)
        left_eval = evaluate(node->left, DATs_hashes);
    if (node->right)
        right_eval = evaluate(node->right, DATs_hashes);

    switch (node->type) {
    case GWMBTokenType::AND:
        for (const auto& left_hash : left_eval) {
            for (const auto& right_hash : right_eval) {
                if (left_hash == right_hash) {
                    result_set.insert(left_hash);
                }
            }
        }
        break;
    case GWMBTokenType::OR:
        result_set.insert(left_eval.begin(), left_eval.end());
        result_set.insert(right_eval.begin(), right_eval.end());
        break;
    case GWMBTokenType::XOR:
        for (const auto& hash : left_eval) {
            if (std::find(right_eval.begin(), right_eval.end(), hash) == right_eval.end()) {
                result_set.insert(hash);
            }
        }
        for (const auto& hash : right_eval) {
            if (std::find(left_eval.begin(), left_eval.end(), hash) == left_eval.end()) {
                result_set.insert(hash);
            }
        }
        break;
    case GWMBTokenType::NOT:
        for (const auto& [key, hash] : DATs_hashes) {
            if (std::find(left_eval.begin(), left_eval.end(), hash) == left_eval.end()) {
                result_set.insert(hash);
            }
        }
        break;
    default:
        throw std::runtime_error("Invalid node type");
    }

    return std::vector<uint32_t>(result_set.begin(), result_set.end());
}