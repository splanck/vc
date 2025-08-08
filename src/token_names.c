/*
 * Token name lookup table and helper function.
 *
 * Part of vc under the BSD 2-Clause license.
 * See LICENSE for details.
 */

#include "token_names.h"

/* Table mapping token_type_t values to the textual names used in diagnostics.
 * The index of each entry corresponds directly to its token_type_t value.
 * When adding new tokens, update both the token_type_t enum in token.h and
 * this table to keep them in sync. */
static const char *token_names[] = {
    [TOK_EOF] = "end of file",
    [TOK_IDENT] = "identifier",
    [TOK_NUMBER] = "number",
    [TOK_FLOAT] = "float number",
    [TOK_IMAG_NUMBER] = "imaginary number",
    [TOK_STRING] = "string",
    [TOK_CHAR] = "character",
    [TOK_WIDE_STRING] = "L\"string\"",
    [TOK_WIDE_CHAR] = "L'char'",
    [TOK_KW_INT] = "\"int\"",
    [TOK_KW_CHAR] = "\"char\"",
    [TOK_KW_FLOAT] = "\"float\"",
    [TOK_KW_DOUBLE] = "\"double\"",
    [TOK_KW_SHORT] = "\"short\"",
    [TOK_KW_LONG] = "\"long\"",
    [TOK_KW_BOOL] = "\"bool\"",
    [TOK_KW_UNSIGNED] = "\"unsigned\"",
    [TOK_KW_VOID] = "\"void\"",
    [TOK_KW_ENUM] = "\"enum\"",
    [TOK_KW_STRUCT] = "\"struct\"",
    [TOK_KW_UNION] = "\"union\"",
    [TOK_KW_TYPEDEF] = "\"typedef\"",
    [TOK_KW_STATIC] = "\"static\"",
    [TOK_KW_EXTERN] = "\"extern\"",
    [TOK_KW_CONST] = "\"const\"",
    [TOK_KW_VOLATILE] = "\"volatile\"",
    [TOK_KW_RESTRICT] = "\"restrict\"",
    [TOK_KW_REGISTER] = "\"register\"",
    [TOK_KW_INLINE] = "\"inline\"",
    [TOK_KW_NORETURN] = "\"_Noreturn\"",
    [TOK_KW_STATIC_ASSERT] = "\"_Static_assert\"",
    [TOK_KW_RETURN] = "\"return\"",
    [TOK_KW_IF] = "\"if\"",
    [TOK_KW_ELSE] = "\"else\"",
    [TOK_KW_DO] = "\"do\"",
    [TOK_KW_WHILE] = "\"while\"",
    [TOK_KW_FOR] = "\"for\"",
    [TOK_KW_BREAK] = "\"break\"",
    [TOK_KW_CONTINUE] = "\"continue\"",
    [TOK_KW_GOTO] = "\"goto\"",
    [TOK_KW_SWITCH] = "\"switch\"",
    [TOK_KW_CASE] = "\"case\"",
    [TOK_KW_DEFAULT] = "\"default\"",
    [TOK_KW_SIZEOF] = "\"sizeof\"",
    [TOK_KW_COMPLEX] = "\"_Complex\"",
    [TOK_KW_ALIGNAS] = "\"alignas\"",
    [TOK_KW_ALIGNOF] = "\"_Alignof\"",
    [TOK_LPAREN] = "'('",
    [TOK_RPAREN] = ")",
    [TOK_LBRACE] = "'{'",
    [TOK_RBRACE] = "'}'",
    [TOK_SEMI] = ";",
    [TOK_COMMA] = ",",
    [TOK_PLUS] = "+",
    [TOK_MINUS] = "-",
    [TOK_DOT] = ".",
    [TOK_ARROW] = "'->'",
    [TOK_AMP] = "&",
    [TOK_STAR] = "*",
    [TOK_SLASH] = "/",
    [TOK_PERCENT] = "%",
    [TOK_PIPE] = "|",
    [TOK_CARET] = "^",
    [TOK_SHL] = "'<<'",
    [TOK_SHR] = "'>>'",
    [TOK_PLUSEQ] = "+=",
    [TOK_MINUSEQ] = "-=",
    [TOK_STAREQ] = "*=",
    [TOK_SLASHEQ] = "/=",
    [TOK_PERCENTEQ] = "%=",
    [TOK_AMPEQ] = "&=",
    [TOK_PIPEEQ] = "|=",
    [TOK_CARETEQ] = "^=",
    [TOK_SHLEQ] = "<<=",
    [TOK_SHREQ] = ">>=",
    [TOK_INC] = "++",
    [TOK_DEC] = "--",
    [TOK_ASSIGN] = "=",
    [TOK_EQ] = "==",
    [TOK_NEQ] = "!=",
    [TOK_LOGAND] = "&&",
    [TOK_LOGOR] = "||",
    [TOK_NOT] = "!",
    [TOK_LT] = "<",
    [TOK_GT] = ">",
    [TOK_LE] = "<=",
    [TOK_GE] = ">=",
    [TOK_LBRACKET] = "[",
    [TOK_RBRACKET] = "]",
    [TOK_QMARK] = "?",
    [TOK_COLON] = ":",
    [TOK_LABEL] = "label",
    [TOK_ELLIPSIS] = "'...'",
    [TOK_UNKNOWN] = "unknown"
};

const char *token_name(token_type_t type)
{
    size_t n = sizeof(token_names) / sizeof(token_names[0]);
    if (type >= 0 && (size_t)type < n && token_names[type])
        return token_names[type];
    return "unknown";
}

