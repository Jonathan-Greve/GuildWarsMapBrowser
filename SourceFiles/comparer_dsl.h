#pragma once
#include "peglib.h"

struct DatCompareFileInfo {
    int hash;
    int size;
    int fname0;
    int fname1;
};

constexpr  auto grammar = R"(
    EXPR          <- OR_OP
    OR_OP         <- AND_OP ('or'i AND_OP)*
    AND_OP        <- COMP ('and'i COMP)*
    COMP          <- NOT_OP (COMP_OP NOT_OP)?
    NOT_OP        <- ARITHMETIC / 'not'i COMP
    ARITHMETIC    <- TERM (ADD_SUB_OP TERM)*
    TERM          <- FACTOR (MUL_DIV_OP FACTOR)*
    FACTOR        <- PRIMARY / NUMBER
    PRIMARY       <- (EXISTS  / COMPARE_TYPE / '(' EXPR ')' ) WHITESPACE
    ADD_SUB_OP    <- '+' / '-'
    MUL_DIV_OP    <- '*' / '/' / '%'
    EXISTS        <- 'exists'i '(' HASH NUMBER (',' HASH NUMBER)* ')'
    COMP_OP       <- '==' / '!=' / '>=' / '<=' / '>' / '<'
    COMPARE_TYPE  <- HASH NUMBER / SIZE NUMBER / FNAME0 NUMBER / FNAME1 NUMBER / FNAME NUMBER
    ~HASH         <- 'hash'i
    ~SIZE         <- 'size'i
    ~FNAME        <- 'fname'i
    ~FNAME0       <- 'fname0'i
    ~FNAME1       <- 'fname1'i
    NUMBER        <- HEX_NUMBER / DEC_NUMBER
    HEX_NUMBER    <- < '0x'i [a-fA-F0-9]+ >
    DEC_NUMBER    <- < [0-9]+ >
    ~WHITESPACE   <- SPACE
    ~SPACE        <- (' ' / '\t')*
    %whitespace   <- [ \t]*
)";

class ComparerDSL {
public:
    ComparerDSL();

    // Returns true if the file_id should be included in the filtered items.
    // Returns false otherwise (either incorrect expression entered by the user or the file_infos do not match the expression)
    bool parse(const std::string& input_expression, std::unordered_map<int, DatCompareFileInfo>& file_infos);

    // Get the log messages that are populated by the peglib parser when it errors.
    std::vector<std::string>& get_log_messages();


private:
    void define_semantic_actions();

    // This is updated for every file_id to be compared. If there are 2 dat files added in the compare window
    // then this might contain 2 entries; one per dat file. Note a file_id does not have to exist in the dat.
    std::unordered_map<int, DatCompareFileInfo> m_file_infos;

    std::vector<std::string> m_log_messages;

    // the peglib parser
    peg::parser m_parser;
};