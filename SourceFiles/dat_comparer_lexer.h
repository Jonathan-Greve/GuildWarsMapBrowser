#pragma once
#include <format>

enum class GWMBTokenType : uint8_t {
    AND,
    OR,
    XOR,
    NOT,
    DAT,
    OPEN_PAREN,
    CLOSE_PAREN,
    END
};

struct Token {
    GWMBTokenType token_type;
    std::string value;
};

class Lexer {
public:
    Lexer(const std::string& input) : input(input), position(0) {}

    Token getNextToken() {
        while (position < input.length() && isspace(input[position])) {
            position++;
        }

        if (position == input.length()) {
            return { GWMBTokenType::END, "" };
        }

        char current = input[position];

        if (current == '(') {
            position++;
            return { GWMBTokenType::OPEN_PAREN, "(" };
        }
        else if (current == ')') {
            position++;
            return { GWMBTokenType::CLOSE_PAREN, ")" };
        }
        else if (isalpha(current)) {
            std::string value;
            while (position < input.length() && isalnum(input[position])) {
                value += input[position++];
            }

            if (value == "AND") return { GWMBTokenType::AND, "" };
            if (value == "OR") return { GWMBTokenType::OR, "" };
            if (value == "XOR") return { GWMBTokenType::XOR, "" };
            if (value == "NOT") return { GWMBTokenType::NOT, "" };

            if (value.substr(0, 3) == "DAT") {
                std::string number_part = value.substr(3);
                if (std::all_of(number_part.begin(), number_part.end(), ::isdigit)) {
                    return { GWMBTokenType::DAT, number_part };
                }
                else {
                    throw std::runtime_error(std::format("Invalid DAT token: non-integer characters found after \"DAT\": {}", number_part));
                }
            }
        }

        throw std::runtime_error("Invalid character in input");
    }

private:
    std::string input;
    size_t position;
};