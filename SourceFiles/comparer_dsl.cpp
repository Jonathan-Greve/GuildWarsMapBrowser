#include "pch.h"
#include "comparer_dsl.h"
#include <format>

using namespace peg;

struct FileVersion {
    int hash;
    int size;
    int fname0;
    int fname1;
};

ComparerDSL::ComparerDSL()
{
    m_parser.enable_packrat_parsing();
    m_parser.set_logger([&](size_t line, size_t col, const std::string& msg, const std::string& rule) {
        m_log_messages.emplace_back(std::format("{}:{}: {} in rule: {}", line, col, msg, rule));
        });

    auto ok = m_parser.load_grammar(grammar);
    assert(ok);

    define_semantic_actions();
}

bool ComparerDSL::parse(const std::string& input_expression, std::unordered_map<int, DatCompareFileInfo>& file_infos)
{
    m_log_messages.clear();
    m_file_infos = file_infos;

    int val = 0;
    bool success = m_parser.parse(input_expression, val);

    if (success && val == 1) {
        return true;
    }

    return false;
}

std::vector<std::string>& ComparerDSL::get_log_messages()
{
    return m_log_messages;
}

void ComparerDSL::define_semantic_actions()
{
    m_parser["COMPARE_TYPE"] = [&](const SemanticValues& sv) {
        auto num = any_cast<int>(sv[0]);
        if (m_file_infos.contains(num)) {
            switch (sv.choice())
            {
            case 0:
                return m_file_infos[num].hash;
            case 1:
                return m_file_infos[num].size;
            case 2:
                return m_file_infos[num].fname0;
            case 3:
                return m_file_infos[num].fname1;
            case 4:
                return ((m_file_infos[num].fname0 & 0xFFFF) << 16) | m_file_infos[num].fname1 & 0xFFFF;
            default:
                break;
            }
        }

        return num;
        };

    m_parser["NOT_OP"] = [&](const SemanticValues& sv) {
        if (sv.choice() == 0) {
            return any_cast<int>(sv[0]);
        }

        return static_cast<int>(!static_cast<bool>(any_cast<int>(sv[0])));
        };

    m_parser["OR_OP"] = [&](const SemanticValues& sv) {
        if (sv.size() == 1) {
            return any_cast<int>(sv[0]);
        }

        bool result = false;
        for (const auto& value : sv) {
            int val = any_cast<int>(value);
            result = result || (val != 0);
            if (result) break; // Short-circuit evaluation
        }
        return static_cast<int>(result);
        };

    m_parser["AND_OP"] = [&](const SemanticValues& sv) {
        if (sv.size() == 1) {
            return any_cast<int>(sv[0]);
        }

        bool result = true;
        for (const auto& value : sv) {
            int val = any_cast<int>(value);
            result = result && (val != 0);
            if (!result) break; // Short-circuit evaluation
        }
        return static_cast<int>(result);
        };

    m_parser["EXISTS"] = [&](const SemanticValues& sv) {
        bool allExist = true;

        for (const auto& value : sv) {
            int num = any_cast<int>(value);
            if (!m_file_infos.contains(num)) {
                allExist = false;
                break;
            }
        }

        return static_cast<int>(allExist);
        };

    m_parser["COMP"] = [&](const SemanticValues& sv) {
        if (sv.size() == 1) {
            return any_cast<int>(sv[0]);
        }

        auto left = any_cast<int>(sv[0]);
        auto right = any_cast<int>(sv[2]);
        auto op_choice = any_cast<int>(sv[1]);

        switch (op_choice) {
        case 0: // '=='
            return static_cast<int>(left == right);
        case 1: // '!='
            return static_cast<int>(left != right);
        case 2: // '>='
            return static_cast<int>(left >= right);
        case 3: // '<='
            return static_cast<int>(left <= right);
        case 4: // '>'
            return static_cast<int>(left > right);
        case 5: // '<'
            return static_cast<int>(left < right);
        default:
            break;
        }
        return 0;
        };

    m_parser["ARITHMETIC"] = [&](const SemanticValues& sv) {
        if (sv.size() == 1) {
            return any_cast<int>(sv[0]);
        }

        int result = any_cast<int>(sv[0]);
        for (int i = 1; i < sv.size(); i += 2) {
            auto op_choice = any_cast<int>(sv[i]);
            auto val = any_cast<int>(sv[i + 1]);

            switch (op_choice) {
            case 0: // '+'
                result += val;
                break;
            case 1: // '-'
                result -= val;
                break;
            default:
                break;
            }
        }

        return result;
        };

    m_parser["TERM"] = [&](const SemanticValues& sv) {
        if (sv.size() == 1) {
            return any_cast<int>(sv[0]);
        }

        int result = any_cast<int>(sv[0]);
        for (int i = 1; i < sv.size(); i += 2) {
            auto op_choice = any_cast<int>(sv[i]);
            auto val = any_cast<int>(sv[i + 1]);

            switch (op_choice) {
            case 0: // '*'
                result *= val;
                break;
            case 1: // '/'
                if (val != 0)
                    result /= val;
                else
                    throw std::runtime_error("Division by zero");
                break;
            case 2: // '%'
                if (val != 0)
                    result %= val;
                else
                    throw std::runtime_error("Modulo by zero");
                break;
            default:
                break;
            }
        }

        return result;
        };

    m_parser["FACTOR"] = [&](const SemanticValues& sv) {
        return any_cast<int>(sv[0]);
        };

    m_parser["COMP_OP"] = [&](const SemanticValues& sv) {
        return static_cast<int>(sv.choice());
        };

    m_parser["ADD_SUB_OP"] = [&](const SemanticValues& sv) {
        return static_cast<int>(sv.choice());
        };

    m_parser["MUL_DIV_OP"] = [&](const SemanticValues& sv) {
        return static_cast<int>(sv.choice());
        };

    m_parser["PRIMARY"] = [&](const SemanticValues& sv) {
        const auto out = any_cast<int>(sv[0]);
        return out;
        };

    m_parser["NUMBER"] = [](const SemanticValues& sv) {
        int number = any_cast<int>(sv[0]);
        return number;
        };

    m_parser["HEX_NUMBER"] = [](const SemanticValues& sv) {
        const auto number = std::stoi(sv.token_to_string(), nullptr, 16);
        return number;
        };

    m_parser["DEC_NUMBER"] = [](const SemanticValues& sv) {
        const auto number = sv.token_to_number<int>();
        return number;
        };
}
