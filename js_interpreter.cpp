/*
 * ============================================================
 *  THUNDER HACKATHON 2.0 — JavaScript Interpreter in C++
 *  Build: g++ -std=c++17 -O2 -o js_interpreter js_interpreter.cpp
 *  Run:   ./js_interpreter <file.js>
 *         ./js_interpreter             (reads from stdin)
 * ============================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <memory>
#include <variant>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <stdexcept>
#include <optional>
#include <set>
#include <ctime>
#include <random>

// ================================================================
//  FORWARD DECLARATIONS
// ================================================================
struct Value;
struct Environment;
struct ASTNode;
using ValuePtr = std::shared_ptr<Value>;
using EnvPtr   = std::shared_ptr<Environment>;
using NodePtr  = std::shared_ptr<ASTNode>;

// ================================================================
//  LEXER
// ================================================================
enum class TokenType {
    // Literals
    NUMBER, STRING, BOOLEAN, NULL_T, UNDEFINED,
    // Identifiers / keywords
    IDENTIFIER,
    LET, CONST, VAR, FUNCTION, RETURN, IF, ELSE, WHILE, FOR, DO,
    BREAK, CONTINUE, NEW, THIS, TYPEOF, INSTANCEOF, IN, OF,
    SWITCH, CASE, DEFAULT_KW, THROW, TRY, CATCH, FINALLY,
    // Operators
    PLUS, MINUS, STAR, SLASH, PERCENT, POWER,
    EQ, NEQ, STRICT_EQ, STRICT_NEQ,
    LT, GT, LE, GE,
    AND, OR, NOT,
    BIT_AND, BIT_OR, BIT_XOR, BIT_NOT, LSHIFT, RSHIFT,
    ASSIGN, PLUS_ASSIGN, MINUS_ASSIGN, STAR_ASSIGN, SLASH_ASSIGN, MOD_ASSIGN,
    INC, DEC,
    TERNARY, COLON,
    ARROW,
    SPREAD,
    // Delimiters
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    SEMICOLON, COMMA, DOT, OPTIONAL_CHAIN,
    // Special
    EOF_T, NEWLINE
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};

class Lexer {
public:
    std::string src;
    size_t pos = 0;
    int line = 1;

    Lexer(const std::string& source) : src(source) {}

    char peek(int off = 0) {
        size_t p = pos + off;
        return p < src.size() ? src[p] : '\0';
    }
    char advance() {
        char c = src[pos++];
        if (c == '\n') line++;
        return c;
    }
    void skipWhitespaceAndComments() {
        while (pos < src.size()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
            } else if (c == '/' && peek(1) == '/') {
                while (pos < src.size() && peek() != '\n') advance();
            } else if (c == '/' && peek(1) == '*') {
                advance(); advance();
                while (pos < src.size() && !(peek() == '*' && peek(1) == '/'))
                    advance();
                if (pos < src.size()) { advance(); advance(); }
            } else break;
        }
    }

    Token makeToken(TokenType t, const std::string& v) {
        return {t, v, line};
    }

    Token nextToken() {
        skipWhitespaceAndComments();
        if (pos >= src.size()) return makeToken(TokenType::EOF_T, "");

        char c = peek();

        // Number
        if (isdigit(c) || (c == '.' && isdigit(peek(1)))) {
            std::string num;
            if (c == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
                num += advance(); num += advance();
                while (isxdigit(peek())) num += advance();
                return makeToken(TokenType::NUMBER, num);
            }
            while (isdigit(peek())) num += advance();
            if (peek() == '.' && isdigit(peek(1))) {
                num += advance();
                while (isdigit(peek())) num += advance();
            }
            if (peek() == 'e' || peek() == 'E') {
                num += advance();
                if (peek() == '+' || peek() == '-') num += advance();
                while (isdigit(peek())) num += advance();
            }
            return makeToken(TokenType::NUMBER, num);
        }

        // String
        if (c == '"' || c == '\'' || c == '`') {
            char delim = advance();
            std::string s;
            bool isTemplate = (delim == '`');
            while (pos < src.size() && peek() != delim) {
                if (peek() == '\\') {
                    advance();
                    char esc = advance();
                    switch (esc) {
                        case 'n': s += '\n'; break;
                        case 't': s += '\t'; break;
                        case 'r': s += '\r'; break;
                        case '\\': s += '\\'; break;
                        case '\'': s += '\''; break;
                        case '"': s += '"'; break;
                        case '`': s += '`'; break;
                        default: s += esc; break;
                    }
                } else {
                    s += advance();
                }
            }
            if (pos < src.size()) advance(); // closing
            return makeToken(TokenType::STRING, s);
        }

        // Identifier / keyword
        if (isalpha(c) || c == '_' || c == '$') {
            std::string id;
            while (isalnum(peek()) || peek() == '_' || peek() == '$')
                id += advance();
            static std::map<std::string, TokenType> kws = {
                {"let",TokenType::LET},{"const",TokenType::CONST},{"var",TokenType::VAR},
                {"function",TokenType::FUNCTION},{"return",TokenType::RETURN},
                {"if",TokenType::IF},{"else",TokenType::ELSE},
                {"while",TokenType::WHILE},{"for",TokenType::FOR},{"do",TokenType::DO},
                {"break",TokenType::BREAK},{"continue",TokenType::CONTINUE},
                {"new",TokenType::NEW},{"this",TokenType::THIS},
                {"typeof",TokenType::TYPEOF},{"instanceof",TokenType::INSTANCEOF},
                {"in",TokenType::IN},{"of",TokenType::OF},
                {"true",TokenType::BOOLEAN},{"false",TokenType::BOOLEAN},
                {"null",TokenType::NULL_T},{"undefined",TokenType::UNDEFINED},
                {"switch",TokenType::SWITCH},{"case",TokenType::CASE},
                {"default",TokenType::DEFAULT_KW},
                {"throw",TokenType::THROW},{"try",TokenType::TRY},
                {"catch",TokenType::CATCH},{"finally",TokenType::FINALLY},
            };
            auto it = kws.find(id);
            if (it != kws.end()) return makeToken(it->second, id);
            return makeToken(TokenType::IDENTIFIER, id);
        }

        advance(); // consume the char
        switch (c) {
            case '+':
                if (peek() == '+') { advance(); return makeToken(TokenType::INC, "++"); }
                if (peek() == '=') { advance(); return makeToken(TokenType::PLUS_ASSIGN, "+="); }
                return makeToken(TokenType::PLUS, "+");
            case '-':
                if (peek() == '-') { advance(); return makeToken(TokenType::DEC, "--"); }
                if (peek() == '=') { advance(); return makeToken(TokenType::MINUS_ASSIGN, "-="); }
                return makeToken(TokenType::MINUS, "-");
            case '*':
                if (peek() == '*') { advance(); return makeToken(TokenType::POWER, "**"); }
                if (peek() == '=') { advance(); return makeToken(TokenType::STAR_ASSIGN, "*="); }
                return makeToken(TokenType::STAR, "*");
            case '/':
                if (peek() == '=') { advance(); return makeToken(TokenType::SLASH_ASSIGN, "/="); }
                return makeToken(TokenType::SLASH, "/");
            case '%':
                if (peek() == '=') { advance(); return makeToken(TokenType::MOD_ASSIGN, "%="); }
                return makeToken(TokenType::PERCENT, "%");
            case '=':
                if (peek() == '=') {
                    advance();
                    if (peek() == '=') { advance(); return makeToken(TokenType::STRICT_EQ, "==="); }
                    return makeToken(TokenType::EQ, "==");
                }
                if (peek() == '>') { advance(); return makeToken(TokenType::ARROW, "=>"); }
                return makeToken(TokenType::ASSIGN, "=");
            case '!':
                if (peek() == '=') {
                    advance();
                    if (peek() == '=') { advance(); return makeToken(TokenType::STRICT_NEQ, "!=="); }
                    return makeToken(TokenType::NEQ, "!=");
                }
                return makeToken(TokenType::NOT, "!");
            case '<':
                if (peek() == '=') { advance(); return makeToken(TokenType::LE, "<="); }
                if (peek() == '<') { advance(); return makeToken(TokenType::LSHIFT, "<<"); }
                return makeToken(TokenType::LT, "<");
            case '>':
                if (peek() == '=') { advance(); return makeToken(TokenType::GE, ">="); }
                if (peek() == '>') { advance(); return makeToken(TokenType::RSHIFT, ">>"); }
                return makeToken(TokenType::GT, ">");
            case '&':
                if (peek() == '&') { advance(); return makeToken(TokenType::AND, "&&"); }
                return makeToken(TokenType::BIT_AND, "&");
            case '|':
                if (peek() == '|') { advance(); return makeToken(TokenType::OR, "||"); }
                return makeToken(TokenType::BIT_OR, "|");
            case '^': return makeToken(TokenType::BIT_XOR, "^");
            case '~': return makeToken(TokenType::BIT_NOT, "~");
            case '?':
                if (peek() == '.') { advance(); return makeToken(TokenType::OPTIONAL_CHAIN, "?."); }
                return makeToken(TokenType::TERNARY, "?");
            case ':': return makeToken(TokenType::COLON, ":");
            case '.':
                if (peek() == '.' && peek(1) == '.') { advance(); advance(); return makeToken(TokenType::SPREAD, "..."); }
                return makeToken(TokenType::DOT, ".");
            case '(': return makeToken(TokenType::LPAREN, "(");
            case ')': return makeToken(TokenType::RPAREN, ")");
            case '{': return makeToken(TokenType::LBRACE, "{");
            case '}': return makeToken(TokenType::RBRACE, "}");
            case '[': return makeToken(TokenType::LBRACKET, "[");
            case ']': return makeToken(TokenType::RBRACKET, "]");
            case ';': return makeToken(TokenType::SEMICOLON, ";");
            case ',': return makeToken(TokenType::COMMA, ",");
            default: return makeToken(TokenType::EOF_T, std::string(1,c));
        }
    }

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (true) {
            Token t = nextToken();
            tokens.push_back(t);
            if (t.type == TokenType::EOF_T) break;
        }
        return tokens;
    }
};

// ================================================================
//  AST NODE TYPES
// ================================================================
enum class NodeType {
    Program, Block,
    VarDecl, FuncDecl, ReturnStmt,
    IfStmt, WhileStmt, ForStmt, DoWhileStmt,
    BreakStmt, ContinueStmt,
    SwitchStmt, CaseClause,
    ThrowStmt, TryStmt,
    ExprStmt,
    // Expressions
    NumLiteral, StrLiteral, BoolLiteral, NullLiteral, UndefinedLiteral,
    Identifier, BinOp, UnaryOp, AssignExpr,
    CallExpr, MemberExpr, IndexExpr,
    FuncExpr, ArrowFunc,
    ArrayLiteral, ObjectLiteral,
    Ternary, NewExpr,
    SpreadExpr, TypeofExpr,
    SequenceExpr,
    UpdateExpr,         // ++ / --
    ForInStmt, ForOfStmt,
    DestructureDecl,
};

struct ASTNode {
    NodeType type;
    // Generic fields used by different node types
    std::string name;                    // identifier name, op
    double numVal = 0;
    std::string strVal;
    bool boolVal = false;
    bool prefix = false;                // for unary/update
    std::vector<NodePtr> children;      // body, args, params, ...
    std::vector<std::string> params;    // function param names
    // For object literals: pairs of key->value
    std::vector<std::pair<std::string, NodePtr>> props;
    bool isConst = false;
    bool isSpread = false;
    NodePtr extra;                      // catch id, etc.
};

NodePtr makeNode(NodeType t) {
    auto n = std::make_shared<ASTNode>();
    n->type = t;
    return n;
}

// ================================================================
//  PARSER
// ================================================================
class Parser {
public:
    std::vector<Token> tokens;
    size_t pos = 0;

    Parser(std::vector<Token> toks) : tokens(std::move(toks)) {}

    Token& peek(int off = 0) { 
        size_t p = pos + off;
        if (p >= tokens.size()) return tokens.back();
        return tokens[p];
    }
    Token consume() { return tokens[pos++]; }
    Token expect(TokenType t) {
        if (peek().type != t) {
            throw std::runtime_error("Parse error: unexpected token '" 
                + peek().value + "' at line " + std::to_string(peek().line));
        }
        return consume();
    }
    bool check(TokenType t) { return peek().type == t; }
    bool match(TokenType t) {
        if (check(t)) { consume(); return true; }
        return false;
    }
    void skipSemicolons() {
        while (match(TokenType::SEMICOLON)) {}
    }

    // ---- Top level ----
    NodePtr parseProgram() {
        auto n = makeNode(NodeType::Program);
        while (!check(TokenType::EOF_T)) {
            n->children.push_back(parseStatement());
        }
        return n;
    }

    NodePtr parseBlock() {
        expect(TokenType::LBRACE);
        auto n = makeNode(NodeType::Block);
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_T)) {
            n->children.push_back(parseStatement());
        }
        expect(TokenType::RBRACE);
        return n;
    }

    NodePtr parseStatement() {
        skipSemicolons();
        auto& t = peek();
        if (t.type == TokenType::LET || t.type == TokenType::CONST || t.type == TokenType::VAR)
            return parseVarDecl();
        if (t.type == TokenType::FUNCTION)
            return parseFuncDecl();
        if (t.type == TokenType::RETURN)
            return parseReturn();
        if (t.type == TokenType::IF)
            return parseIf();
        if (t.type == TokenType::WHILE)
            return parseWhile();
        if (t.type == TokenType::DO)
            return parseDoWhile();
        if (t.type == TokenType::FOR)
            return parseFor();
        if (t.type == TokenType::BREAK) {
            consume(); skipSemicolons();
            return makeNode(NodeType::BreakStmt);
        }
        if (t.type == TokenType::CONTINUE) {
            consume(); skipSemicolons();
            return makeNode(NodeType::ContinueStmt);
        }
        if (t.type == TokenType::SWITCH)
            return parseSwitch();
        if (t.type == TokenType::THROW)
            return parseThrow();
        if (t.type == TokenType::TRY)
            return parseTry();
        if (t.type == TokenType::LBRACE)
            return parseBlock();
        // expression statement
        auto expr = makeNode(NodeType::ExprStmt);
        expr->children.push_back(parseExpression());
        skipSemicolons();
        return expr;
    }

    NodePtr parseVarDecl(bool consumeSemi = true) {
        bool isConst = (peek().type == TokenType::CONST);
        consume(); // let / const / var

        auto decl = makeNode(NodeType::VarDecl);
        decl->isConst = isConst;

        // Destructuring: let [a, b] = ... or let {x, y} = ...
        if (check(TokenType::LBRACKET) || check(TokenType::LBRACE)) {
            bool isArray = check(TokenType::LBRACKET);
            consume();
            std::vector<std::string> ids;
            while (!(isArray ? check(TokenType::RBRACKET) : check(TokenType::RBRACE)) && !check(TokenType::EOF_T)) {
                if (check(TokenType::IDENTIFIER)) {
                    // handle rename: { x: y }
                    std::string id = consume().value;
                    if (!isArray && check(TokenType::COLON)) {
                        consume();
                        id = consume().value;
                    }
                    ids.push_back(id);
                }
                match(TokenType::COMMA);
            }
            isArray ? expect(TokenType::RBRACKET) : expect(TokenType::RBRACE);
            expect(TokenType::ASSIGN);
            auto rhs = parseAssignment();
            auto dd = makeNode(NodeType::DestructureDecl);
            dd->params = ids;
            dd->boolVal = isArray;
            dd->children.push_back(rhs);
            return dd;
        }

        // Normal: let a = ..., b = ...
        do {
            std::string id = expect(TokenType::IDENTIFIER).value;
            NodePtr init;
            if (match(TokenType::ASSIGN)) {
                init = parseAssignment();
            }
            auto child = makeNode(NodeType::Identifier);
            child->name = id;
            if (init) child->children.push_back(init);
            decl->children.push_back(child);
        } while (match(TokenType::COMMA));

        if (consumeSemi) skipSemicolons();
        return decl;
    }

    NodePtr parseFuncDecl() {
        expect(TokenType::FUNCTION);
        auto n = makeNode(NodeType::FuncDecl);
        if (check(TokenType::IDENTIFIER)) n->name = consume().value;
        n->params = parseParamList();
        n->children.push_back(parseBlock());
        return n;
    }

    std::vector<std::string> parseParamList() {
        expect(TokenType::LPAREN);
        std::vector<std::string> params;
        while (!check(TokenType::RPAREN) && !check(TokenType::EOF_T)) {
            bool spread = match(TokenType::SPREAD);
            std::string p = expect(TokenType::IDENTIFIER).value;
            if (spread) p = "..." + p;
            params.push_back(p);
            // default value - skip for now
            if (check(TokenType::ASSIGN)) {
                consume();
                parseAssignment(); // discard
            }
            match(TokenType::COMMA);
        }
        expect(TokenType::RPAREN);
        return params;
    }

    NodePtr parseReturn() {
        expect(TokenType::RETURN);
        auto n = makeNode(NodeType::ReturnStmt);
        if (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE) && !check(TokenType::EOF_T))
            n->children.push_back(parseExpression());
        skipSemicolons();
        return n;
    }

    NodePtr parseIf() {
        expect(TokenType::IF);
        expect(TokenType::LPAREN);
        auto n = makeNode(NodeType::IfStmt);
        n->children.push_back(parseExpression()); // condition
        expect(TokenType::RPAREN);
        n->children.push_back(parseStatement());   // then
        if (match(TokenType::ELSE))
            n->children.push_back(parseStatement()); // else
        return n;
    }

    NodePtr parseWhile() {
        expect(TokenType::WHILE);
        expect(TokenType::LPAREN);
        auto n = makeNode(NodeType::WhileStmt);
        n->children.push_back(parseExpression());
        expect(TokenType::RPAREN);
        n->children.push_back(parseStatement());
        return n;
    }

    NodePtr parseDoWhile() {
        expect(TokenType::DO);
        auto n = makeNode(NodeType::DoWhileStmt);
        n->children.push_back(parseStatement());
        expect(TokenType::WHILE);
        expect(TokenType::LPAREN);
        n->children.push_back(parseExpression());
        expect(TokenType::RPAREN);
        skipSemicolons();
        return n;
    }

    NodePtr parseFor() {
        expect(TokenType::FOR);
        expect(TokenType::LPAREN);

        // Lookahead for for...of and for...in
        size_t savedPos = pos;
        bool isForOf = false, isForIn = false;
        {
            // Check for for(let/const/var id of/in ...)
            if (check(TokenType::LET) || check(TokenType::CONST) || check(TokenType::VAR)) {
                size_t p0 = pos;
                consume(); // consume let/const/var
                if (check(TokenType::IDENTIFIER)) {
                    consume(); // consume identifier
                    if (check(TokenType::OF)) { isForOf = true; }
                    else if (check(TokenType::IN)) { isForIn = true; }
                }
                pos = savedPos; // always restore
            }
        }

        if (isForOf || isForIn) {
            auto kw = consume();
            std::string varName = consume().value;
            consume(); // of or in
            auto n = makeNode(isForOf ? NodeType::ForOfStmt : NodeType::ForInStmt);
            n->name = varName;
            n->children.push_back(parseExpression()); // iterable
            expect(TokenType::RPAREN);
            n->children.push_back(parseStatement());
            return n;
        }

        // Normal for loop
        auto n = makeNode(NodeType::ForStmt);
        // init
        if (!check(TokenType::SEMICOLON)) {
            if (check(TokenType::LET) || check(TokenType::CONST) || check(TokenType::VAR))
                n->children.push_back(parseVarDecl(false));
            else {
                auto e = makeNode(NodeType::ExprStmt);
                e->children.push_back(parseExpression());
                n->children.push_back(e);
            }
        } else {
            n->children.push_back(nullptr);
        }
        expect(TokenType::SEMICOLON);
        // condition
        if (!check(TokenType::SEMICOLON))
            n->children.push_back(parseExpression());
        else
            n->children.push_back(nullptr);
        expect(TokenType::SEMICOLON);
        // update
        if (!check(TokenType::RPAREN))
            n->children.push_back(parseExpression());
        else
            n->children.push_back(nullptr);
        expect(TokenType::RPAREN);
        n->children.push_back(parseStatement());
        return n;
    }

    NodePtr parseSwitch() {
        expect(TokenType::SWITCH);
        expect(TokenType::LPAREN);
        auto n = makeNode(NodeType::SwitchStmt);
        n->children.push_back(parseExpression());
        expect(TokenType::RPAREN);
        expect(TokenType::LBRACE);
        while (!check(TokenType::RBRACE) && !check(TokenType::EOF_T)) {
            auto clause = makeNode(NodeType::CaseClause);
            if (match(TokenType::CASE)) {
                clause->children.push_back(parseExpression());
                expect(TokenType::COLON);
            } else {
                expect(TokenType::DEFAULT_KW);
                clause->name = "default";
                expect(TokenType::COLON);
            }
            while (!check(TokenType::CASE) && !check(TokenType::DEFAULT_KW) 
                   && !check(TokenType::RBRACE) && !check(TokenType::EOF_T)) {
                clause->children.push_back(parseStatement());
            }
            n->children.push_back(clause);
        }
        expect(TokenType::RBRACE);
        return n;
    }

    NodePtr parseThrow() {
        expect(TokenType::THROW);
        auto n = makeNode(NodeType::ThrowStmt);
        n->children.push_back(parseExpression());
        skipSemicolons();
        return n;
    }

    NodePtr parseTry() {
        expect(TokenType::TRY);
        auto n = makeNode(NodeType::TryStmt);
        n->children.push_back(parseBlock()); // try body
        NodePtr catchBlock, finallyBlock;
        if (match(TokenType::CATCH)) {
            if (match(TokenType::LPAREN)) {
                n->name = expect(TokenType::IDENTIFIER).value;
                expect(TokenType::RPAREN);
            }
            n->children.push_back(parseBlock());
        } else {
            n->children.push_back(nullptr);
        }
        if (match(TokenType::FINALLY)) {
            n->children.push_back(parseBlock());
        } else {
            n->children.push_back(nullptr);
        }
        return n;
    }

    // ---- Expressions ----
    NodePtr parseExpression() {
        auto left = parseAssignment();
        if (check(TokenType::COMMA)) {
            auto seq = makeNode(NodeType::SequenceExpr);
            seq->children.push_back(left);
            while (match(TokenType::COMMA)) {
                seq->children.push_back(parseAssignment());
            }
            return seq;
        }
        return left;
    }

    NodePtr parseAssignment() {
        auto left = parseTernary();
        static std::set<TokenType> assignOps = {
            TokenType::ASSIGN, TokenType::PLUS_ASSIGN, TokenType::MINUS_ASSIGN,
            TokenType::STAR_ASSIGN, TokenType::SLASH_ASSIGN, TokenType::MOD_ASSIGN
        };
        if (assignOps.count(peek().type)) {
            std::string op = consume().value;
            auto right = parseAssignment();
            auto n = makeNode(NodeType::AssignExpr);
            n->name = op;
            n->children.push_back(left);
            n->children.push_back(right);
            return n;
        }
        return left;
    }

    NodePtr parseTernary() {
        auto cond = parseOr();
        if (match(TokenType::TERNARY)) {
            auto n = makeNode(NodeType::Ternary);
            n->children.push_back(cond);
            n->children.push_back(parseAssignment());
            expect(TokenType::COLON);
            n->children.push_back(parseAssignment());
            return n;
        }
        return cond;
    }

    NodePtr parseOr() {
        auto left = parseAnd();
        while (check(TokenType::OR)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::BinOp);
            n->name = op;
            n->children.push_back(left);
            n->children.push_back(parseAnd());
            left = n;
        }
        return left;
    }

    NodePtr parseAnd() {
        auto left = parseBitOr();
        while (check(TokenType::AND)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::BinOp);
            n->name = op;
            n->children.push_back(left);
            n->children.push_back(parseBitOr());
            left = n;
        }
        return left;
    }

    NodePtr parseBitOr() {
        auto left = parseBitXor();
        while (check(TokenType::BIT_OR)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::BinOp);
            n->name = op; n->children.push_back(left);
            n->children.push_back(parseBitXor()); left = n;
        }
        return left;
    }
    NodePtr parseBitXor() {
        auto left = parseBitAnd();
        while (check(TokenType::BIT_XOR)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::BinOp);
            n->name = op; n->children.push_back(left);
            n->children.push_back(parseBitAnd()); left = n;
        }
        return left;
    }
    NodePtr parseBitAnd() {
        auto left = parseEquality();
        while (check(TokenType::BIT_AND)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::BinOp);
            n->name = op; n->children.push_back(left);
            n->children.push_back(parseEquality()); left = n;
        }
        return left;
    }

    NodePtr parseEquality() {
        auto left = parseRelational();
        while (check(TokenType::EQ) || check(TokenType::NEQ)
            || check(TokenType::STRICT_EQ) || check(TokenType::STRICT_NEQ)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::BinOp);
            n->name = op; n->children.push_back(left);
            n->children.push_back(parseRelational()); left = n;
        }
        return left;
    }

    NodePtr parseRelational() {
        auto left = parseShift();
        while (check(TokenType::LT) || check(TokenType::GT)
            || check(TokenType::LE) || check(TokenType::GE)
            || check(TokenType::INSTANCEOF) || check(TokenType::IN)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::BinOp);
            n->name = op; n->children.push_back(left);
            n->children.push_back(parseShift()); left = n;
        }
        return left;
    }

    NodePtr parseShift() {
        auto left = parseAdditive();
        while (check(TokenType::LSHIFT) || check(TokenType::RSHIFT)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::BinOp);
            n->name = op; n->children.push_back(left);
            n->children.push_back(parseAdditive()); left = n;
        }
        return left;
    }

    NodePtr parseAdditive() {
        auto left = parseMultiplicative();
        while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::BinOp);
            n->name = op; n->children.push_back(left);
            n->children.push_back(parseMultiplicative()); left = n;
        }
        return left;
    }

    NodePtr parseMultiplicative() {
        auto left = parsePower();
        while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::BinOp);
            n->name = op; n->children.push_back(left);
            n->children.push_back(parsePower()); left = n;
        }
        return left;
    }

    NodePtr parsePower() {
        auto left = parseUnary();
        if (check(TokenType::POWER)) {
            consume();
            auto n = makeNode(NodeType::BinOp);
            n->name = "**"; n->children.push_back(left);
            n->children.push_back(parsePower());
            return n;
        }
        return left;
    }

    NodePtr parseUnary() {
        if (check(TokenType::NOT)) {
            consume();
            auto n = makeNode(NodeType::UnaryOp); n->name = "!";
            n->children.push_back(parseUnary()); return n;
        }
        if (check(TokenType::MINUS)) {
            consume();
            auto n = makeNode(NodeType::UnaryOp); n->name = "-";
            n->children.push_back(parseUnary()); return n;
        }
        if (check(TokenType::PLUS)) {
            consume();
            auto n = makeNode(NodeType::UnaryOp); n->name = "+";
            n->children.push_back(parseUnary()); return n;
        }
        if (check(TokenType::BIT_NOT)) {
            consume();
            auto n = makeNode(NodeType::UnaryOp); n->name = "~";
            n->children.push_back(parseUnary()); return n;
        }
        if (check(TokenType::TYPEOF)) {
            consume();
            auto n = makeNode(NodeType::TypeofExpr);
            n->children.push_back(parseUnary()); return n;
        }
        // prefix ++ --
        if (check(TokenType::INC) || check(TokenType::DEC)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::UpdateExpr);
            n->name = op; n->prefix = true;
            n->children.push_back(parseUnary()); return n;
        }
        return parsePostfix();
    }

    NodePtr parsePostfix() {
        auto left = parseCall();
        // postfix ++ --
        if (check(TokenType::INC) || check(TokenType::DEC)) {
            std::string op = consume().value;
            auto n = makeNode(NodeType::UpdateExpr);
            n->name = op; n->prefix = false;
            n->children.push_back(left); return n;
        }
        return left;
    }

    NodePtr parseCall() {
        auto left = parsePrimary();
        while (true) {
            if (check(TokenType::LPAREN)) {
                // function call
                consume();
                auto n = makeNode(NodeType::CallExpr);
                n->children.push_back(left);
                while (!check(TokenType::RPAREN) && !check(TokenType::EOF_T)) {
                    if (check(TokenType::SPREAD)) {
                        consume();
                        auto sp = makeNode(NodeType::SpreadExpr);
                        sp->children.push_back(parseAssignment());
                        n->children.push_back(sp);
                    } else {
                        n->children.push_back(parseAssignment());
                    }
                    match(TokenType::COMMA);
                }
                expect(TokenType::RPAREN);
                left = n;
            } else if (check(TokenType::DOT) || check(TokenType::OPTIONAL_CHAIN)) {
                bool optional = check(TokenType::OPTIONAL_CHAIN);
                consume();
                std::string prop = expect(TokenType::IDENTIFIER).value;
                auto n = makeNode(NodeType::MemberExpr);
                n->name = prop;
                n->boolVal = optional;
                n->children.push_back(left);
                left = n;
            } else if (check(TokenType::LBRACKET)) {
                consume();
                auto n = makeNode(NodeType::IndexExpr);
                n->children.push_back(left);
                n->children.push_back(parseExpression());
                expect(TokenType::RBRACKET);
                left = n;
            } else break;
        }
        return left;
    }

    NodePtr parsePrimary() {
        auto& t = peek();
        // Number
        if (t.type == TokenType::NUMBER) {
            auto n = makeNode(NodeType::NumLiteral);
            std::string v = consume().value;
            if (v.size() > 2 && v[0]=='0' && (v[1]=='x'||v[1]=='X'))
                n->numVal = (double)std::stoul(v, nullptr, 16);
            else
                n->numVal = std::stod(v);
            return n;
        }
        // String
        if (t.type == TokenType::STRING) {
            auto n = makeNode(NodeType::StrLiteral);
            n->strVal = consume().value;
            return n;
        }
        // Boolean
        if (t.type == TokenType::BOOLEAN) {
            auto n = makeNode(NodeType::BoolLiteral);
            n->boolVal = (consume().value == "true");
            return n;
        }
        // null
        if (t.type == TokenType::NULL_T) {
            consume();
            return makeNode(NodeType::NullLiteral);
        }
        // undefined
        if (t.type == TokenType::UNDEFINED) {
            consume();
            return makeNode(NodeType::UndefinedLiteral);
        }
        // Identifier (or single-param arrow: x => ...)
        if (t.type == TokenType::IDENTIFIER) {
            if (peek(1).type == TokenType::ARROW) {
                auto n = makeNode(NodeType::ArrowFunc);
                n->params = {consume().value};
                consume(); // =>
                if (check(TokenType::LBRACE))
                    n->children.push_back(parseBlock());
                else {
                    auto ret = makeNode(NodeType::ReturnStmt);
                    ret->children.push_back(parseAssignment());
                    n->children.push_back(ret);
                }
                return n;
            }
            auto n = makeNode(NodeType::Identifier);
            n->name = consume().value;
            return n;
        }
        // this
        if (t.type == TokenType::THIS) {
            consume();
            auto n = makeNode(NodeType::Identifier);
            n->name = "this";
            return n;
        }
        // Grouped expression or arrow function: (a, b) => ...
        if (t.type == TokenType::LPAREN) {
            // Save pos BEFORE consuming (
            size_t savedBefore = pos;
            consume(); // consume (
            
            // Try to parse as arrow function params
            size_t saved = pos;
            bool isArrow = false;
            std::vector<std::string> ps;
            bool validParams = true;
            
            try {
                while (!check(TokenType::RPAREN) && !check(TokenType::EOF_T)) {
                    bool spread = match(TokenType::SPREAD);
                    if (!check(TokenType::IDENTIFIER)) { validParams = false; break; }
                    std::string p = consume().value;
                    if (spread) p = "..." + p;
                    ps.push_back(p);
                    // skip default values
                    if (check(TokenType::ASSIGN)) { validParams = false; break; }
                    if (!check(TokenType::RPAREN)) match(TokenType::COMMA);
                }
                if (validParams && check(TokenType::RPAREN)) {
                    consume(); // consume )
                    if (check(TokenType::ARROW)) {
                        consume(); // consume =>
                        isArrow = true;
                        auto n = makeNode(NodeType::ArrowFunc);
                        n->params = ps;
                        if (check(TokenType::LBRACE))
                            n->children.push_back(parseBlock());
                        else {
                            auto ret = makeNode(NodeType::ReturnStmt);
                            ret->children.push_back(parseAssignment());
                            n->children.push_back(ret);
                        }
                        return n;
                    }
                }
            } catch (...) {}
            
            if (!isArrow) {
                // Not an arrow function, re-parse as grouped expression
                pos = saved; // reset to after (
                auto expr = parseExpression();
                expect(TokenType::RPAREN);
                return expr;
            }
            // Should not reach here
            return makeNode(NodeType::UndefinedLiteral); // dummy
        }
        // Array literal
        if (t.type == TokenType::LBRACKET) {
            consume();
            auto n = makeNode(NodeType::ArrayLiteral);
            while (!check(TokenType::RBRACKET) && !check(TokenType::EOF_T)) {
                if (check(TokenType::SPREAD)) {
                    consume();
                    auto sp = makeNode(NodeType::SpreadExpr);
                    sp->children.push_back(parseAssignment());
                    n->children.push_back(sp);
                } else if (check(TokenType::COMMA)) {
                    n->children.push_back(makeNode(NodeType::UndefinedLiteral));
                } else {
                    n->children.push_back(parseAssignment());
                }
                match(TokenType::COMMA);
            }
            expect(TokenType::RBRACKET);
            return n;
        }
        // Object literal
        if (t.type == TokenType::LBRACE) {
            consume();
            auto n = makeNode(NodeType::ObjectLiteral);
            while (!check(TokenType::RBRACE) && !check(TokenType::EOF_T)) {
                // Object spread: { ...obj }
                if (check(TokenType::SPREAD)) {
                    consume();
                    auto sp = makeNode(NodeType::SpreadExpr);
                    sp->children.push_back(parseAssignment());
                    n->props.push_back({"__spread__", sp});
                    match(TokenType::COMMA);
                    continue;
                }
                std::string key;
                if (check(TokenType::STRING)) {
                    key = consume().value;
                } else if (check(TokenType::NUMBER)) {
                    key = consume().value;
                } else if (check(TokenType::LBRACKET)) {
                    // computed key
                    consume(); auto expr = parseExpression(); expect(TokenType::RBRACKET);
                    key = "__computed__";
                } else {
                    key = consume().value; // identifier or keyword as key
                }
                NodePtr val;
                if (check(TokenType::COLON)) {
                    consume();
                    val = parseAssignment();
                } else if (check(TokenType::LPAREN)) {
                    // method shorthand
                    auto fn = makeNode(NodeType::FuncExpr);
                    fn->params = parseParamList();
                    fn->children.push_back(parseBlock());
                    val = fn;
                } else {
                    // shorthand { x } = { x: x }
                    auto id = makeNode(NodeType::Identifier);
                    id->name = key;
                    val = id;
                }
                n->props.push_back({key, val});
                match(TokenType::COMMA);
            }
            expect(TokenType::RBRACE);
            return n;
        }
        // Function expression
        if (t.type == TokenType::FUNCTION) {
            consume();
            auto n = makeNode(NodeType::FuncExpr);
            if (check(TokenType::IDENTIFIER)) n->name = consume().value;
            n->params = parseParamList();
            n->children.push_back(parseBlock());
            return n;
        }

        // new
        if (t.type == TokenType::NEW) {
            consume();
            auto n = makeNode(NodeType::NewExpr);
            n->children.push_back(parseCall());
            return n;
        }
        // Spread
        if (t.type == TokenType::SPREAD) {
            consume();
            auto n = makeNode(NodeType::SpreadExpr);
            n->children.push_back(parsePrimary());
            return n;
        }

        throw std::runtime_error("Unexpected token: '" + t.value + "' at line " + std::to_string(t.line));
    }
};

// ================================================================
//  VALUE SYSTEM
// ================================================================
enum class ValueType { Undefined, Null, Boolean, Number, String, Array, Object, Function };

struct JSArray;
struct JSObject;
struct JSFunction;

using JSArrayPtr  = std::shared_ptr<JSArray>;
using JSObjectPtr = std::shared_ptr<JSObject>;
using JSFuncPtr   = std::shared_ptr<JSFunction>;

struct Value {
    ValueType type = ValueType::Undefined;
    bool b = false;
    double num = 0;
    std::string str;
    JSArrayPtr  arr;
    JSObjectPtr obj;
    JSFuncPtr   func;

    static ValuePtr makeUndefined() {
        auto v = std::make_shared<Value>(); v->type = ValueType::Undefined; return v; }
    static ValuePtr makeNull() {
        auto v = std::make_shared<Value>(); v->type = ValueType::Null; return v; }
    static ValuePtr makeBool(bool b) {
        auto v = std::make_shared<Value>(); v->type = ValueType::Boolean; v->b = b; return v; }
    static ValuePtr makeNumber(double n) {
        auto v = std::make_shared<Value>(); v->type = ValueType::Number; v->num = n; return v; }
    static ValuePtr makeString(const std::string& s) {
        auto v = std::make_shared<Value>(); v->type = ValueType::String; v->str = s; return v; }
    static ValuePtr makeArray(JSArrayPtr a) {
        auto v = std::make_shared<Value>(); v->type = ValueType::Array; v->arr = a; return v; }
    static ValuePtr makeObject(JSObjectPtr o) {
        auto v = std::make_shared<Value>(); v->type = ValueType::Object; v->obj = o; return v; }
    static ValuePtr makeFunction(JSFuncPtr f) {
        auto v = std::make_shared<Value>(); v->type = ValueType::Function; v->func = f; return v; }
};

struct JSArray {
    std::vector<ValuePtr> elements;
    std::unordered_map<std::string, ValuePtr> props; // extra props
    JSArrayPtr clone() const {
        auto a = std::make_shared<JSArray>();
        a->elements = elements;
        a->props = props;
        return a;
    }
};

struct JSObject {
    std::unordered_map<std::string, ValuePtr> props;
    JSObjectPtr proto; // prototype chain
    JSFuncPtr constructor;
    std::string className;
};

struct Environment;
using EnvPtr = std::shared_ptr<Environment>;

struct JSFunction {
    std::string name;
    std::vector<std::string> params;
    NodePtr body;
    EnvPtr closure;
    // Native function
    std::function<ValuePtr(std::vector<ValuePtr>, ValuePtr)> native;
    bool isNative = false;
    JSObjectPtr proto; // .prototype
};

// ================================================================
//  ENVIRONMENT
// ================================================================
struct Environment : std::enable_shared_from_this<Environment> {
    std::unordered_map<std::string, ValuePtr> vars;
    std::shared_ptr<Environment> parent;

    Environment(std::shared_ptr<Environment> p = nullptr) : parent(p) {}

    ValuePtr get(const std::string& name) {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second;
        if (parent) return parent->get(name);
        return Value::makeUndefined();
    }

    void set(const std::string& name, ValuePtr val) {
        auto it = vars.find(name);
        if (it != vars.end()) { it->second = val; return; }
        if (parent) {
            auto* env = parent.get();
            while (env) {
                auto it2 = env->vars.find(name);
                if (it2 != env->vars.end()) { it2->second = val; return; }
                env = env->parent.get();
            }
        }
        vars[name] = val; // declare in current scope if not found
    }

    void define(const std::string& name, ValuePtr val) {
        vars[name] = val;
    }

    bool has(const std::string& name) const {
        if (vars.count(name)) return true;
        if (parent) return parent->has(name);
        return false;
    }
};

// ================================================================
//  CONTROL FLOW SIGNALS
// ================================================================
struct ReturnSignal { ValuePtr value; };
struct BreakSignal {};
struct ContinueSignal {};
struct ThrowSignal { ValuePtr value; };

// ================================================================
//  INTERPRETER
// ================================================================
class Interpreter {
public:
    EnvPtr globalEnv;

    Interpreter() {
        globalEnv = std::make_shared<Environment>();
        setupGlobals();
    }

    // ---- Helpers ----
    std::string toStr(ValuePtr v) {
        if (!v) return "undefined";
        switch (v->type) {
            case ValueType::Undefined: return "undefined";
            case ValueType::Null: return "null";
            case ValueType::Boolean: return v->b ? "true" : "false";
            case ValueType::Number: {
                if (std::isnan(v->num)) return "NaN";
                if (std::isinf(v->num)) return v->num > 0 ? "Infinity" : "-Infinity";
                // Remove trailing zeros but keep integers clean
                double intpart;
                if (std::modf(v->num, &intpart) == 0.0 && std::abs(intpart) < 1e15)
                    return std::to_string((long long)intpart);
                // General float
                std::ostringstream oss;
                oss << v->num;
                return oss.str();
            }
            case ValueType::String: return v->str;
            case ValueType::Array: {
                std::string s;
                auto& elems = v->arr->elements;
                for (size_t i = 0; i < elems.size(); i++) {
                    if (i > 0) s += ",";
                    auto& e = elems[i];
                    if (e && e->type != ValueType::Undefined && e->type != ValueType::Null)
                        s += toStr(e);
                }
                return s;
            }
            case ValueType::Object: return "[object Object]";
            case ValueType::Function: return "function " + (v->func ? v->func->name : "") + "() { [native code] }";
        }
        return "";
    }

    double toNum(ValuePtr v) {
        if (!v) return std::numeric_limits<double>::quiet_NaN();
        switch (v->type) {
            case ValueType::Undefined: return std::numeric_limits<double>::quiet_NaN();
            case ValueType::Null: return 0;
            case ValueType::Boolean: return v->b ? 1 : 0;
            case ValueType::Number: return v->num;
            case ValueType::String: {
                std::string s = v->str;
                // trim
                size_t l = s.find_first_not_of(" \t\n\r");
                size_t r = s.find_last_not_of(" \t\n\r");
                if (l == std::string::npos) return 0;
                s = s.substr(l, r-l+1);
                if (s.empty()) return 0;
                try { return std::stod(s); }
                catch (...) { return std::numeric_limits<double>::quiet_NaN(); }
            }
            case ValueType::Array: {
                auto& elems = v->arr->elements;
                if (elems.empty()) return 0;
                if (elems.size() == 1) return toNum(elems[0]);
                return std::numeric_limits<double>::quiet_NaN();
            }
            default: return std::numeric_limits<double>::quiet_NaN();
        }
    }

    bool toBool(ValuePtr v) {
        if (!v) return false;
        switch (v->type) {
            case ValueType::Undefined: return false;
            case ValueType::Null: return false;
            case ValueType::Boolean: return v->b;
            case ValueType::Number: return v->num != 0 && !std::isnan(v->num);
            case ValueType::String: return !v->str.empty();
            default: return true;
        }
    }

    bool strictEq(ValuePtr a, ValuePtr b) {
        if (a->type != b->type) return false;
        switch (a->type) {
            case ValueType::Undefined: return true;
            case ValueType::Null: return true;
            case ValueType::Boolean: return a->b == b->b;
            case ValueType::Number: return a->num == b->num;
            case ValueType::String: return a->str == b->str;
            case ValueType::Array: return a->arr == b->arr;
            case ValueType::Object: return a->obj == b->obj;
            case ValueType::Function: return a->func == b->func;
        }
        return false;
    }

    bool looseEq(ValuePtr a, ValuePtr b) {
        if (a->type == b->type) return strictEq(a, b);
        // null == undefined
        if ((a->type==ValueType::Null||a->type==ValueType::Undefined) &&
            (b->type==ValueType::Null||b->type==ValueType::Undefined)) return true;
        // number and string
        if (a->type==ValueType::Number && b->type==ValueType::String)
            return a->num == toNum(b);
        if (b->type==ValueType::Number && a->type==ValueType::String)
            return toNum(a) == b->num;
        // bool to number
        if (a->type==ValueType::Boolean) return looseEq(Value::makeNumber(toNum(a)), b);
        if (b->type==ValueType::Boolean) return looseEq(a, Value::makeNumber(toNum(b)));
        return false;
    }

    // Array join helper
    std::string arrayJoin(JSArrayPtr arr, const std::string& sep) {
        std::string result;
        for (size_t i = 0; i < arr->elements.size(); i++) {
            if (i > 0) result += sep;
            auto& e = arr->elements[i];
            if (e && e->type != ValueType::Undefined && e->type != ValueType::Null)
                result += toStr(e);
        }
        return result;
    }

    // ---- Native setup ----
    void setupGlobals() {
        // console.log
        auto consoleObj = std::make_shared<JSObject>();
        auto logFn = makeFn([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) std::cout << " ";
                std::cout << toStr(args[i]);
            }
            std::cout << "\n";
            return Value::makeUndefined();
        }, "log");
        consoleObj->props["log"] = Value::makeFunction(logFn);

        auto errorFn = makeFn([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) std::cerr << " ";
                std::cerr << toStr(args[i]);
            }
            std::cerr << "\n";
            return Value::makeUndefined();
        }, "error");
        consoleObj->props["error"] = Value::makeFunction(errorFn);
        globalEnv->define("console", Value::makeObject(consoleObj));

        // Math object
        auto mathObj = std::make_shared<JSObject>();
        mathObj->props["PI"]  = Value::makeNumber(M_PI);
        mathObj->props["E"]   = Value::makeNumber(M_E);
        mathObj->props["abs"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::abs(toNum(a[0]))); }, "abs"));
        mathObj->props["ceil"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::ceil(toNum(a[0]))); }, "ceil"));
        mathObj->props["floor"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::floor(toNum(a[0]))); }, "floor"));
        mathObj->props["round"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::round(toNum(a[0]))); }, "round"));
        mathObj->props["sqrt"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::sqrt(toNum(a[0]))); }, "sqrt"));
        mathObj->props["pow"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::pow(toNum(a[0]), toNum(a[1]))); }, "pow"));
        mathObj->props["max"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            double m = -std::numeric_limits<double>::infinity();
            for (auto& v : a) m = std::max(m, toNum(v));
            return Value::makeNumber(m); }, "max"));
        mathObj->props["min"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            double m = std::numeric_limits<double>::infinity();
            for (auto& v : a) m = std::min(m, toNum(v));
            return Value::makeNumber(m); }, "min"));
        mathObj->props["log"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::log(toNum(a[0]))); }, "log"));
        mathObj->props["log2"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::log2(toNum(a[0]))); }, "log2"));
        mathObj->props["log10"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::log10(toNum(a[0]))); }, "log10"));
        mathObj->props["sin"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::sin(toNum(a[0]))); }, "sin"));
        mathObj->props["cos"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::cos(toNum(a[0]))); }, "cos"));
        mathObj->props["tan"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::tan(toNum(a[0]))); }, "tan"));
        mathObj->props["trunc"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeNumber(std::trunc(toNum(a[0]))); }, "trunc"));
        mathObj->props["sign"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            double n = toNum(a[0]);
            return Value::makeNumber(n > 0 ? 1 : n < 0 ? -1 : 0); }, "sign"));
        mathObj->props["random"] = Value::makeFunction(makeFn([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            static std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            return Value::makeNumber(dist(rng)); }, "random"));
        mathObj->props["hypot"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            double sum = 0;
            for (auto& v : a) { double n = toNum(v); sum += n*n; }
            return Value::makeNumber(std::sqrt(sum)); }, "hypot"));
        globalEnv->define("Math", Value::makeObject(mathObj));

        // Number
        auto numFn = makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            if (a.empty()) return Value::makeNumber(0);
            return Value::makeNumber(toNum(a[0]));
        }, "Number");
        numFn->proto = std::make_shared<JSObject>();
        numFn->proto->props["isNaN"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            if (a.empty() || a[0]->type != ValueType::Number) return Value::makeBool(false);
            return Value::makeBool(std::isnan(a[0]->num)); }, "isNaN"));
        numFn->proto->props["isFinite"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            if (a.empty() || a[0]->type != ValueType::Number) return Value::makeBool(false);
            return Value::makeBool(std::isfinite(a[0]->num)); }, "isFinite"));
        numFn->proto->props["isInteger"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            if (a.empty() || a[0]->type != ValueType::Number) return Value::makeBool(false);
            return Value::makeBool(std::floor(a[0]->num) == a[0]->num); }, "isInteger"));
        numFn->proto->props["parseInt"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            if (a.empty()) return Value::makeNumber(std::numeric_limits<double>::quiet_NaN());
            std::string s = toStr(a[0]);
            int base = (a.size() > 1) ? (int)toNum(a[1]) : 10;
            try { return Value::makeNumber((double)std::stol(s, nullptr, base)); }
            catch (...) { return Value::makeNumber(std::numeric_limits<double>::quiet_NaN()); }
        }, "parseInt"));
        numFn->proto->props["parseFloat"] = Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            if (a.empty()) return Value::makeNumber(std::numeric_limits<double>::quiet_NaN());
            try { return Value::makeNumber(std::stod(toStr(a[0]))); }
            catch (...) { return Value::makeNumber(std::numeric_limits<double>::quiet_NaN()); }
        }, "parseFloat"));
        auto numObj = Value::makeFunction(numFn);
        // Also expose static methods on the function object
        globalEnv->define("Number", numObj);

        // parseInt / parseFloat global
        globalEnv->define("parseInt", numFn->proto->props["parseInt"]);
        globalEnv->define("parseFloat", numFn->proto->props["parseFloat"]);

        // isNaN / isFinite global
        globalEnv->define("isNaN", Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeBool(std::isnan(toNum(a.empty() ? Value::makeUndefined() : a[0]))); }, "isNaN")));
        globalEnv->define("isFinite", Value::makeFunction(makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            return Value::makeBool(std::isfinite(toNum(a.empty() ? Value::makeUndefined() : a[0]))); }, "isFinite")));

        // String
        auto strFn = makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            if (a.empty()) return Value::makeString("");
            return Value::makeString(toStr(a[0]));
        }, "String");
        globalEnv->define("String", Value::makeFunction(strFn));

        // Boolean
        auto boolFn = makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            if (a.empty()) return Value::makeBool(false);
            return Value::makeBool(toBool(a[0]));
        }, "Boolean");
        globalEnv->define("Boolean", Value::makeFunction(boolFn));

        // Array
        auto arrFn = makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            auto arr = std::make_shared<JSArray>();
            if (a.size() == 1 && a[0]->type == ValueType::Number) {
                arr->elements.resize((size_t)a[0]->num, Value::makeUndefined());
            } else {
                arr->elements = a;
            }
            return Value::makeArray(arr);
        }, "Array");
        globalEnv->define("Array", Value::makeFunction(arrFn));

        // Object
        auto objFn = makeFn([](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            if (!a.empty() && (a[0]->type == ValueType::Object || a[0]->type == ValueType::Array))
                return a[0];
            auto o = std::make_shared<JSObject>();
            return Value::makeObject(o);
        }, "Object");
        auto objFnV = Value::makeFunction(objFn);
        // Object.keys, Object.values, Object.entries, Object.assign
        objFn->proto = std::make_shared<JSObject>();
        objFn->proto->props["keys"] = Value::makeFunction(makeFn([](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            auto arr = std::make_shared<JSArray>();
            if (!a.empty()) {
                if (a[0]->type == ValueType::Object && a[0]->obj) {
                    for (auto& kv : a[0]->obj->props)
                        arr->elements.push_back(Value::makeString(kv.first));
                } else if (a[0]->type == ValueType::Array && a[0]->arr) {
                    for (size_t i = 0; i < a[0]->arr->elements.size(); i++)
                        arr->elements.push_back(Value::makeString(std::to_string(i)));
                }
            }
            return Value::makeArray(arr);
        }, "keys"));
        objFn->proto->props["values"] = Value::makeFunction(makeFn([](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            auto arr = std::make_shared<JSArray>();
            if (!a.empty() && a[0]->type == ValueType::Object && a[0]->obj)
                for (auto& kv : a[0]->obj->props)
                    arr->elements.push_back(kv.second);
            return Value::makeArray(arr);
        }, "values"));
        objFn->proto->props["entries"] = Value::makeFunction(makeFn([](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            auto arr = std::make_shared<JSArray>();
            if (!a.empty() && a[0]->type == ValueType::Object && a[0]->obj) {
                for (auto& kv : a[0]->obj->props) {
                    auto pair = std::make_shared<JSArray>();
                    pair->elements.push_back(Value::makeString(kv.first));
                    pair->elements.push_back(kv.second);
                    arr->elements.push_back(Value::makeArray(pair));
                }
            }
            return Value::makeArray(arr);
        }, "entries"));
        objFn->proto->props["assign"] = Value::makeFunction(makeFn([](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            if (a.empty()) return Value::makeUndefined();
            auto target = a[0];
            for (size_t i = 1; i < a.size(); i++) {
                if (a[i]->type == ValueType::Object && a[i]->obj) {
                    for (auto& kv : a[i]->obj->props) {
                        if (target->type == ValueType::Object && target->obj)
                            target->obj->props[kv.first] = kv.second;
                    }
                }
            }
            return target;
        }, "assign"));
        globalEnv->define("Object", objFnV);

        // Date
        auto dateFn = makeFn([this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
            auto obj = std::make_shared<JSObject>();
            obj->className = "Date";
            time_t now = time(nullptr);
            double ms = (double)now * 1000;
            obj->props["__time__"] = Value::makeNumber(ms);
            auto self = Value::makeObject(obj);
            // methods
            obj->props["getTime"] = Value::makeFunction(makeFn([ms](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                return Value::makeNumber(ms); }, "getTime"));
            obj->props["getFullYear"] = Value::makeFunction(makeFn([now](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                struct tm* t = localtime(&now);
                return Value::makeNumber(t->tm_year + 1900); }, "getFullYear"));
            obj->props["getMonth"] = Value::makeFunction(makeFn([now](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                struct tm* t = localtime(&now);
                return Value::makeNumber(t->tm_mon); }, "getMonth"));
            obj->props["getDate"] = Value::makeFunction(makeFn([now](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                struct tm* t = localtime(&now);
                return Value::makeNumber(t->tm_mday); }, "getDate"));
            obj->props["toString"] = Value::makeFunction(makeFn([now](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                char buf[64];
                struct tm* t = localtime(&now);
                strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", t);
                return Value::makeString(buf); }, "toString"));
            return self;
        }, "Date");
        globalEnv->define("Date", Value::makeFunction(dateFn));

        // Error constructor
        auto errFn = makeFn([this](std::vector<ValuePtr> a, ValuePtr thisVal) -> ValuePtr {
            std::string msg = a.empty() ? "" : toStr(a[0]);
            if (thisVal && thisVal->type == ValueType::Object) {
                thisVal->obj->props["message"] = Value::makeString(msg);
                thisVal->obj->props["name"] = Value::makeString("Error");
                thisVal->obj->props["toString"] = Value::makeFunction(makeFn([msg](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                    return Value::makeString("Error: " + msg);
                }, "toString"));
                return thisVal;
            }
            auto obj = std::make_shared<JSObject>();
            obj->className = "Error";
            obj->props["message"] = Value::makeString(msg);
            obj->props["name"] = Value::makeString("Error");
            return Value::makeObject(obj);
        }, "Error");
        globalEnv->define("Error", Value::makeFunction(errFn));

        // undefined / NaN / Infinity
        globalEnv->define("undefined", Value::makeUndefined());
        globalEnv->define("NaN", Value::makeNumber(std::numeric_limits<double>::quiet_NaN()));
        globalEnv->define("Infinity", Value::makeNumber(std::numeric_limits<double>::infinity()));
    }

    JSFuncPtr makeFn(std::function<ValuePtr(std::vector<ValuePtr>, ValuePtr)> f, const std::string& name) {
        auto fn = std::make_shared<JSFunction>();
        fn->name = name;
        fn->native = f;
        fn->isNative = true;
        return fn;
    }

    // ---- Property access ----
    ValuePtr getProp(ValuePtr obj, const std::string& key) {
        if (!obj) return Value::makeUndefined();
        if (obj->type == ValueType::Array) {
            auto& arr = obj->arr;
            if (key == "length") return Value::makeNumber((double)arr->elements.size());
            if (key == "push") return Value::makeFunction(makeFn([arr](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                for (auto& v : a) arr->elements.push_back(v);
                return Value::makeNumber((double)arr->elements.size());
            }, "push"));
            if (key == "pop") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                if (arr->elements.empty()) return Value::makeUndefined();
                auto v = arr->elements.back(); arr->elements.pop_back(); return v;
            }, "pop"));
            if (key == "shift") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                if (arr->elements.empty()) return Value::makeUndefined();
                auto v = arr->elements.front(); arr->elements.erase(arr->elements.begin()); return v;
            }, "shift"));
            if (key == "unshift") return Value::makeFunction(makeFn([arr](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                for (int i = (int)a.size()-1; i >= 0; i--)
                    arr->elements.insert(arr->elements.begin(), a[i]);
                return Value::makeNumber((double)arr->elements.size());
            }, "unshift"));
            if (key == "reverse") return Value::makeFunction(makeFn([arr,obj](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                std::reverse(arr->elements.begin(), arr->elements.end());
                return obj;
            }, "reverse"));
            if (key == "join") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                std::string sep = a.empty() ? "," : toStr(a[0]);
                return Value::makeString(arrayJoin(arr, sep));
            }, "join"));
            if (key == "slice") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                int len = (int)arr->elements.size();
                int start = a.empty() ? 0 : (int)toNum(a[0]);
                int end   = a.size() < 2 ? len : (int)toNum(a[1]);
                if (start < 0) start = std::max(0, len + start);
                if (end   < 0) end   = std::max(0, len + end);
                start = std::min(start, len);
                end   = std::min(end,   len);
                auto r = std::make_shared<JSArray>();
                for (int i = start; i < end; i++) r->elements.push_back(arr->elements[i]);
                return Value::makeArray(r);
            }, "slice"));
            if (key == "splice") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                int len = (int)arr->elements.size();
                int start = a.empty() ? 0 : (int)toNum(a[0]);
                if (start < 0) start = std::max(0, len + start);
                start = std::min(start, len);
                int delCount = a.size() < 2 ? (len - start) : (int)toNum(a[1]);
                delCount = std::max(0, std::min(delCount, len - start));
                auto removed = std::make_shared<JSArray>();
                for (int i = 0; i < delCount; i++) {
                    removed->elements.push_back(arr->elements[start]);
                    arr->elements.erase(arr->elements.begin() + start);
                }
                for (int i = (int)a.size()-1; i >= 2; i--)
                    arr->elements.insert(arr->elements.begin() + start, a[i]);
                return Value::makeArray(removed);
            }, "splice"));
            if (key == "concat") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                auto r = arr->clone();
                for (auto& v : a) {
                    if (v->type == ValueType::Array)
                        for (auto& e : v->arr->elements) r->elements.push_back(e);
                    else r->elements.push_back(v);
                }
                return Value::makeArray(r);
            }, "concat"));
            if (key == "indexOf") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeNumber(-1);
                for (size_t i = 0; i < arr->elements.size(); i++)
                    if (strictEq(arr->elements[i], a[0])) return Value::makeNumber((double)i);
                return Value::makeNumber(-1);
            }, "indexOf"));
            if (key == "includes") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeBool(false);
                for (auto& e : arr->elements)
                    if (strictEq(e, a[0])) return Value::makeBool(true);
                return Value::makeBool(false);
            }, "includes"));
            if (key == "sort") return Value::makeFunction(makeFn([arr,obj,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (!a.empty() && a[0]->type == ValueType::Function) {
                    auto cmpFn = a[0]->func;
                    std::stable_sort(arr->elements.begin(), arr->elements.end(), [&](ValuePtr x, ValuePtr y) {
                        auto r = callFunction(cmpFn, {x, y}, Value::makeUndefined());
                        return toNum(r) < 0;
                    });
                } else {
                    std::stable_sort(arr->elements.begin(), arr->elements.end(), [this](ValuePtr x, ValuePtr y) {
                        return toStr(x) < toStr(y);
                    });
                }
                return obj;
            }, "sort"));
            if (key == "forEach") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty() || a[0]->type != ValueType::Function) return Value::makeUndefined();
                for (size_t i = 0; i < arr->elements.size(); i++)
                    callFunction(a[0]->func, {arr->elements[i], Value::makeNumber((double)i), Value::makeArray(arr)}, Value::makeUndefined());
                return Value::makeUndefined();
            }, "forEach"));
            if (key == "map") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty() || a[0]->type != ValueType::Function) return Value::makeUndefined();
                auto r = std::make_shared<JSArray>();
                for (size_t i = 0; i < arr->elements.size(); i++)
                    r->elements.push_back(callFunction(a[0]->func, {arr->elements[i], Value::makeNumber((double)i)}, Value::makeUndefined()));
                return Value::makeArray(r);
            }, "map"));
            if (key == "filter") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty() || a[0]->type != ValueType::Function) return Value::makeUndefined();
                auto r = std::make_shared<JSArray>();
                for (size_t i = 0; i < arr->elements.size(); i++)
                    if (toBool(callFunction(a[0]->func, {arr->elements[i], Value::makeNumber((double)i)}, Value::makeUndefined())))
                        r->elements.push_back(arr->elements[i]);
                return Value::makeArray(r);
            }, "filter"));
            if (key == "reduce") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty() || a[0]->type != ValueType::Function) return Value::makeUndefined();
                size_t start = 0;
                ValuePtr acc;
                if (a.size() > 1) { acc = a[1]; }
                else { if (arr->elements.empty()) return Value::makeUndefined(); acc = arr->elements[0]; start = 1; }
                for (size_t i = start; i < arr->elements.size(); i++)
                    acc = callFunction(a[0]->func, {acc, arr->elements[i], Value::makeNumber((double)i)}, Value::makeUndefined());
                return acc;
            }, "reduce"));
            if (key == "find") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty() || a[0]->type != ValueType::Function) return Value::makeUndefined();
                for (size_t i = 0; i < arr->elements.size(); i++)
                    if (toBool(callFunction(a[0]->func, {arr->elements[i], Value::makeNumber((double)i)}, Value::makeUndefined())))
                        return arr->elements[i];
                return Value::makeUndefined();
            }, "find"));
            if (key == "some") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty() || a[0]->type != ValueType::Function) return Value::makeBool(false);
                for (size_t i = 0; i < arr->elements.size(); i++)
                    if (toBool(callFunction(a[0]->func, {arr->elements[i], Value::makeNumber((double)i)}, Value::makeUndefined())))
                        return Value::makeBool(true);
                return Value::makeBool(false);
            }, "some"));
            if (key == "every") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty() || a[0]->type != ValueType::Function) return Value::makeBool(true);
                for (size_t i = 0; i < arr->elements.size(); i++)
                    if (!toBool(callFunction(a[0]->func, {arr->elements[i], Value::makeNumber((double)i)}, Value::makeUndefined())))
                        return Value::makeBool(false);
                return Value::makeBool(true);
            }, "every"));
            if (key == "flat") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                auto r = std::make_shared<JSArray>();
                for (auto& e : arr->elements) {
                    if (e->type == ValueType::Array)
                        for (auto& ee : e->arr->elements) r->elements.push_back(ee);
                    else r->elements.push_back(e);
                }
                return Value::makeArray(r);
            }, "flat"));
            if (key == "fill") return Value::makeFunction(makeFn([arr,obj,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                ValuePtr val = a.empty() ? Value::makeUndefined() : a[0];
                for (auto& e : arr->elements) e = val;
                return obj;
            }, "fill"));
            if (key == "toString") return Value::makeFunction(makeFn([arr,this](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                return Value::makeString(arrayJoin(arr, ","));
            }, "toString"));
            // Numeric index
            try {
                size_t idx = std::stoul(key);
                if (idx < arr->elements.size()) return arr->elements[idx];
            } catch (...) {}
            // check extra props
            auto it = arr->props.find(key);
            if (it != arr->props.end()) return it->second;
            return Value::makeUndefined();
        }
        if (obj->type == ValueType::String) {
            const std::string& s = obj->str;
            if (key == "length") return Value::makeNumber((double)s.size());
            if (key == "split") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                auto arr = std::make_shared<JSArray>();
                if (a.empty() || (a[0]->type == ValueType::Undefined)) {
                    arr->elements.push_back(Value::makeString(s));
                    return Value::makeArray(arr);
                }
                std::string sep = toStr(a[0]);
                if (sep.empty()) {
                    for (char c : s) arr->elements.push_back(Value::makeString(std::string(1,c)));
                } else {
                    size_t start = 0, pos2;
                    while ((pos2 = s.find(sep, start)) != std::string::npos) {
                        arr->elements.push_back(Value::makeString(s.substr(start, pos2-start)));
                        start = pos2 + sep.size();
                    }
                    arr->elements.push_back(Value::makeString(s.substr(start)));
                }
                return Value::makeArray(arr);
            }, "split"));
            if (key == "join") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                return Value::makeString(s);
            }, "join"));
            if (key == "indexOf") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeNumber(-1);
                auto pos2 = s.find(toStr(a[0]));
                return Value::makeNumber(pos2 == std::string::npos ? -1 : (double)pos2);
            }, "indexOf"));
            if (key == "lastIndexOf") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeNumber(-1);
                auto pos2 = s.rfind(toStr(a[0]));
                return Value::makeNumber(pos2 == std::string::npos ? -1 : (double)pos2);
            }, "lastIndexOf"));
            if (key == "includes") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeBool(false);
                return Value::makeBool(s.find(toStr(a[0])) != std::string::npos);
            }, "includes"));
            if (key == "startsWith") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeBool(false);
                std::string sub = toStr(a[0]);
                return Value::makeBool(s.size() >= sub.size() && s.substr(0,sub.size()) == sub);
            }, "startsWith"));
            if (key == "endsWith") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeBool(false);
                std::string sub = toStr(a[0]);
                return Value::makeBool(s.size() >= sub.size() && s.substr(s.size()-sub.size()) == sub);
            }, "endsWith"));
            if (key == "slice") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                int len = (int)s.size();
                int start = a.empty() ? 0 : (int)toNum(a[0]);
                int end   = a.size()<2 ? len : (int)toNum(a[1]);
                if (start < 0) start = std::max(0, len+start);
                if (end   < 0) end   = std::max(0, len+end);
                start = std::min(start,len); end = std::min(end,len);
                if (end <= start) return Value::makeString("");
                return Value::makeString(s.substr(start, end-start));
            }, "slice"));
            if (key == "substring") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                int len = (int)s.size();
                int start = a.empty() ? 0 : std::max(0,(int)toNum(a[0]));
                int end   = a.size()<2 ? len : std::max(0,(int)toNum(a[1]));
                if (start > end) std::swap(start, end);
                start = std::min(start,len); end = std::min(end,len);
                return Value::makeString(s.substr(start, end-start));
            }, "substring"));
            if (key == "substr") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                int len = (int)s.size();
                int start = a.empty() ? 0 : (int)toNum(a[0]);
                if (start < 0) start = std::max(0, len+start);
                int count = a.size()<2 ? len-start : (int)toNum(a[1]);
                if (count < 0) count = 0;
                start = std::min(start,len);
                return Value::makeString(s.substr(start, count));
            }, "substr"));
            if (key == "toUpperCase" || key == "toLocaleUpperCase") return Value::makeFunction(makeFn([s](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                std::string r = s;
                std::transform(r.begin(),r.end(),r.begin(),::toupper);
                return Value::makeString(r);
            }, "toUpperCase"));
            if (key == "toLowerCase" || key == "toLocaleLowerCase") return Value::makeFunction(makeFn([s](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                std::string r = s;
                std::transform(r.begin(),r.end(),r.begin(),::tolower);
                return Value::makeString(r);
            }, "toLowerCase"));
            if (key == "trim") return Value::makeFunction(makeFn([s](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                size_t l = s.find_first_not_of(" \t\n\r");
                size_t r2 = s.find_last_not_of(" \t\n\r");
                if (l == std::string::npos) return Value::makeString("");
                return Value::makeString(s.substr(l, r2-l+1));
            }, "trim"));
            if (key == "trimStart") return Value::makeFunction(makeFn([s](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                size_t l = s.find_first_not_of(" \t\n\r");
                return Value::makeString(l == std::string::npos ? "" : s.substr(l));
            }, "trimStart"));
            if (key == "trimEnd") return Value::makeFunction(makeFn([s](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                size_t r2 = s.find_last_not_of(" \t\n\r");
                return Value::makeString(r2 == std::string::npos ? "" : s.substr(0, r2+1));
            }, "trimEnd"));
            if (key == "replace") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.size() < 2) return Value::makeString(s);
                std::string search = toStr(a[0]);
                std::string repl = toStr(a[1]);
                std::string r = s;
                auto pos2 = r.find(search);
                if (pos2 != std::string::npos) r.replace(pos2, search.size(), repl);
                return Value::makeString(r);
            }, "replace"));
            if (key == "replaceAll") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.size() < 2) return Value::makeString(s);
                std::string search = toStr(a[0]);
                std::string repl = toStr(a[1]);
                std::string r = s;
                size_t pos2 = 0;
                while ((pos2 = r.find(search, pos2)) != std::string::npos) {
                    r.replace(pos2, search.size(), repl);
                    pos2 += repl.size();
                }
                return Value::makeString(r);
            }, "replaceAll"));
            if (key == "charAt") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeString("");
                int idx = (int)toNum(a[0]);
                if (idx < 0 || idx >= (int)s.size()) return Value::makeString("");
                return Value::makeString(std::string(1,s[idx]));
            }, "charAt"));
            if (key == "charCodeAt") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeNumber(std::numeric_limits<double>::quiet_NaN());
                int idx = (int)toNum(a[0]);
                if (idx < 0 || idx >= (int)s.size()) return Value::makeNumber(std::numeric_limits<double>::quiet_NaN());
                return Value::makeNumber((double)(unsigned char)s[idx]);
            }, "charCodeAt"));
            if (key == "repeat") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeString("");
                int n = (int)toNum(a[0]);
                std::string r;
                for (int i = 0; i < n; i++) r += s;
                return Value::makeString(r);
            }, "repeat"));
            if (key == "padStart") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeString(s);
                int targetLen = (int)toNum(a[0]);
                std::string pad = a.size() > 1 ? toStr(a[1]) : " ";
                std::string r = s;
                while ((int)r.size() < targetLen) r = pad + r;
                return Value::makeString(r.substr(r.size() - std::max(0, targetLen)));
            }, "padStart"));
            if (key == "padEnd") return Value::makeFunction(makeFn([s,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                if (a.empty()) return Value::makeString(s);
                int targetLen = (int)toNum(a[0]);
                std::string pad = a.size() > 1 ? toStr(a[1]) : " ";
                std::string r = s;
                while ((int)r.size() < targetLen) r += pad;
                return Value::makeString(r.substr(0, targetLen));
            }, "padEnd"));
            if (key == "toString" || key == "valueOf") return Value::makeFunction(makeFn([s](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                return Value::makeString(s);
            }, key));
            // Numeric index
            try {
                size_t idx = std::stoul(key);
                if (idx < s.size()) return Value::makeString(std::string(1, s[idx]));
            } catch (...) {}
            return Value::makeUndefined();
        }
        if (obj->type == ValueType::Object) {
            auto& o = obj->obj;
            auto it = o->props.find(key);
            if (it != o->props.end()) return it->second;
            // Check proto
            if (o->proto) {
                auto it2 = o->proto->props.find(key);
                if (it2 != o->proto->props.end()) return it2->second;
            }
            return Value::makeUndefined();
        }
        if (obj->type == ValueType::Function) {
            auto& f = obj->func;
            // Check proto props (like Number.isNaN etc)
            if (f->proto) {
                auto it = f->proto->props.find(key);
                if (it != f->proto->props.end()) return it->second;
            }
            if (key == "name") return Value::makeString(f->name);
            if (key == "length") return Value::makeNumber((double)f->params.size());
            if (key == "call") return Value::makeFunction(makeFn([this, f](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                ValuePtr thisVal = a.empty() ? Value::makeUndefined() : a[0];
                std::vector<ValuePtr> args(a.begin() + (a.empty() ? 0 : 1), a.end());
                return callFunction(f, args, thisVal);
            }, "call"));
            if (key == "apply") return Value::makeFunction(makeFn([this, f](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                ValuePtr thisVal = a.empty() ? Value::makeUndefined() : a[0];
                std::vector<ValuePtr> args;
                if (a.size() > 1 && a[1]->type == ValueType::Array)
                    args = a[1]->arr->elements;
                return callFunction(f, args, thisVal);
            }, "apply"));
            if (key == "bind") return Value::makeFunction(makeFn([this, f](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                ValuePtr thisVal = a.empty() ? Value::makeUndefined() : a[0];
                std::vector<ValuePtr> boundArgs(a.begin() + (a.empty() ? 0 : 1), a.end());
                auto bound = makeFn([this, f, thisVal, boundArgs](std::vector<ValuePtr> callArgs, ValuePtr) -> ValuePtr {
                    std::vector<ValuePtr> args = boundArgs;
                    for (auto& v : callArgs) args.push_back(v);
                    return callFunction(f, args, thisVal);
                }, f->name);
                return Value::makeFunction(bound);
            }, "bind"));
            return Value::makeUndefined();
        }
        if (obj->type == ValueType::Number) {
            if (key == "toString") return Value::makeFunction(makeFn([obj,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                int base = a.empty() ? 10 : (int)toNum(a[0]);
                if (base == 10) return Value::makeString(toStr(obj));
                // Convert to base
                long long n = (long long)obj->num;
                if (n == 0) return Value::makeString("0");
                std::string result;
                bool neg = n < 0; if (neg) n = -n;
                while (n > 0) {
                    int d = n % base;
                    result = (char)(d < 10 ? '0'+d : 'a'+d-10) + result;
                    n /= base;
                }
                if (neg) result = "-" + result;
                return Value::makeString(result);
            }, "toString"));
            if (key == "toFixed") return Value::makeFunction(makeFn([obj,this](std::vector<ValuePtr> a, ValuePtr) -> ValuePtr {
                int digits = a.empty() ? 0 : (int)toNum(a[0]);
                std::ostringstream oss;
                oss << std::fixed;
                oss.precision(digits);
                oss << obj->num;
                return Value::makeString(oss.str());
            }, "toFixed"));
        }
        return Value::makeUndefined();
    }

    void setProp(ValuePtr obj, const std::string& key, ValuePtr val) {
        if (obj->type == ValueType::Array) {
            try {
                size_t idx = std::stoul(key);
                auto& arr = obj->arr;
                if (idx >= arr->elements.size())
                    arr->elements.resize(idx+1, Value::makeUndefined());
                arr->elements[idx] = val;
                return;
            } catch (...) {}
            if (key == "length") {
                size_t newLen = (size_t)toNum(val);
                obj->arr->elements.resize(newLen, Value::makeUndefined());
                return;
            }
            obj->arr->props[key] = val;
            return;
        }
        if (obj->type == ValueType::Object) {
            obj->obj->props[key] = val;
            return;
        }
    }

    // ---- Call a function ----
    ValuePtr callFunction(JSFuncPtr fn, std::vector<ValuePtr> args, ValuePtr thisVal) {
        if (!fn) return Value::makeUndefined();
        if (fn->isNative) return fn->native(args, thisVal);

        auto env = std::make_shared<Environment>(fn->closure);
        env->define("this", thisVal ? thisVal : Value::makeUndefined());

        // Bind params
        for (size_t i = 0; i < fn->params.size(); i++) {
            const std::string& p = fn->params[i];
            if (p.substr(0,3) == "...") {
                // rest param
                auto rest = std::make_shared<JSArray>();
                for (size_t j = i; j < args.size(); j++) rest->elements.push_back(args[j]);
                env->define(p.substr(3), Value::makeArray(rest));
                break;
            }
            env->define(p, i < args.size() ? args[i] : Value::makeUndefined());
        }

        try {
            execNode(fn->body, env);
        } catch (ReturnSignal& r) {
            return r.value ? r.value : Value::makeUndefined();
        }
        return Value::makeUndefined();
    }

    // ---- Execute a node ----
    void execNode(NodePtr node, EnvPtr env) {
        if (!node) return;
        switch (node->type) {
            case NodeType::Program:
            case NodeType::Block: {
                auto blockEnv = std::make_shared<Environment>(env);
                // Hoist function declarations - closure must be blockEnv for mutual recursion
                for (auto& child : node->children) {
                    if (child && child->type == NodeType::FuncDecl) {
                        auto fn = makeUserFunc(child, blockEnv);
                        blockEnv->define(child->name, Value::makeFunction(fn));
                    }
                }
                for (auto& child : node->children)
                    execNode(child, blockEnv);
                return;
            }
            case NodeType::FuncDecl: return; // handled by hoisting
            case NodeType::VarDecl: {
                for (auto& child : node->children) {
                    ValuePtr val = Value::makeUndefined();
                    if (!child->children.empty())
                        val = evalExpr(child->children[0], env);
                    env->define(child->name, val);
                }
                return;
            }
            case NodeType::DestructureDecl: {
                auto rhs = evalExpr(node->children[0], env);
                if (node->boolVal) { // array destructure
                    for (size_t i = 0; i < node->params.size(); i++) {
                        ValuePtr v = Value::makeUndefined();
                        if (rhs->type == ValueType::Array && i < rhs->arr->elements.size())
                            v = rhs->arr->elements[i];
                        env->define(node->params[i], v);
                    }
                } else { // object destructure
                    for (auto& p : node->params) {
                        ValuePtr v = getProp(rhs, p);
                        env->define(p, v);
                    }
                }
                return;
            }
            case NodeType::ExprStmt:
                evalExpr(node->children[0], env);
                return;
            case NodeType::ReturnStmt: {
                ValuePtr val = Value::makeUndefined();
                if (!node->children.empty())
                    val = evalExpr(node->children[0], env);
                throw ReturnSignal{val};
            }
            case NodeType::IfStmt: {
                bool cond = toBool(evalExpr(node->children[0], env));
                if (cond) execNode(node->children[1], env);
                else if (node->children.size() > 2) execNode(node->children[2], env);
                return;
            }
            case NodeType::WhileStmt: {
                while (toBool(evalExpr(node->children[0], env))) {
                    try {
                        execNode(node->children[1], env);
                    } catch (BreakSignal&) { return; }
                      catch (ContinueSignal&) { continue; }
                }
                return;
            }
            case NodeType::DoWhileStmt: {
                do {
                    try {
                        execNode(node->children[0], env);
                    } catch (BreakSignal&) { return; }
                      catch (ContinueSignal&) {}
                } while (toBool(evalExpr(node->children[1], env)));
                return;
            }
            case NodeType::ForStmt: {
                auto forEnv = std::make_shared<Environment>(env);
                if (node->children[0]) execNode(node->children[0], forEnv);
                while (true) {
                    if (node->children[1]) {
                        if (!toBool(evalExpr(node->children[1], forEnv))) break;
                    }
                    try {
                        execNode(node->children[3], forEnv);
                    } catch (BreakSignal&) { return; }
                      catch (ContinueSignal&) {}
                    if (node->children[2]) evalExpr(node->children[2], forEnv);
                }
                return;
            }
            case NodeType::ForOfStmt: {
                auto iterable = evalExpr(node->children[0], env);
                auto loopEnv = std::make_shared<Environment>(env);
                std::vector<ValuePtr> items;
                if (iterable->type == ValueType::Array)
                    items = iterable->arr->elements;
                else if (iterable->type == ValueType::String)
                    for (char c : iterable->str)
                        items.push_back(Value::makeString(std::string(1,c)));
                for (auto& item : items) {
                    loopEnv->define(node->name, item);
                    try { execNode(node->children[1], loopEnv); }
                    catch (BreakSignal&) { return; }
                    catch (ContinueSignal&) { continue; }
                }
                return;
            }
            case NodeType::ForInStmt: {
                auto obj = evalExpr(node->children[0], env);
                auto loopEnv = std::make_shared<Environment>(env);
                std::vector<std::string> keys;
                if (obj->type == ValueType::Object && obj->obj)
                    for (auto& kv : obj->obj->props) keys.push_back(kv.first);
                else if (obj->type == ValueType::Array)
                    for (size_t i = 0; i < obj->arr->elements.size(); i++) keys.push_back(std::to_string(i));
                for (auto& k : keys) {
                    loopEnv->define(node->name, Value::makeString(k));
                    try { execNode(node->children[1], loopEnv); }
                    catch (BreakSignal&) { return; }
                    catch (ContinueSignal&) { continue; }
                }
                return;
            }
            case NodeType::BreakStmt: throw BreakSignal{};
            case NodeType::ContinueStmt: throw ContinueSignal{};
            case NodeType::SwitchStmt: {
                auto disc = evalExpr(node->children[0], env);
                bool matched = false;
                int defaultIdx = -1;
                auto switchEnv = std::make_shared<Environment>(env);
                try {
                    for (size_t i = 1; i < node->children.size(); i++) {
                        auto& clause = node->children[i];
                        if (clause->name == "default") { defaultIdx = (int)i; if (!matched) continue; }
                        if (!matched) {
                            // first child of clause is the test value
                            auto testVal = evalExpr(clause->children[0], switchEnv);
                            if (!strictEq(disc, testVal)) continue;
                            matched = true;
                            // Execute from child[1] onward
                            for (size_t j = 1; j < clause->children.size(); j++)
                                execNode(clause->children[j], switchEnv);
                        } else {
                            // fall-through: execute all children
                            size_t start = (clause->name == "default") ? 0 : 1;
                            for (size_t j = start; j < clause->children.size(); j++)
                                execNode(clause->children[j], switchEnv);
                        }
                    }
                    // default
                    if (!matched && defaultIdx >= 0) {
                        auto& clause = node->children[defaultIdx];
                        for (size_t j = 0; j < clause->children.size(); j++)
                            execNode(clause->children[j], switchEnv);
                    }
                } catch (BreakSignal&) {}
                return;
            }
            case NodeType::ThrowStmt: {
                auto val = evalExpr(node->children[0], env);
                throw ThrowSignal{val};
            }
            case NodeType::TryStmt: {
                try {
                    execNode(node->children[0], env);
                } catch (ThrowSignal& ts) {
                    if (node->children[1]) {
                        auto catchEnv = std::make_shared<Environment>(env);
                        if (!node->name.empty()) catchEnv->define(node->name, ts.value);
                        execNode(node->children[1], catchEnv);
                    }
                } catch (std::exception& e) {
                    if (node->children[1]) {
                        auto catchEnv = std::make_shared<Environment>(env);
                        if (!node->name.empty()) catchEnv->define(node->name, Value::makeString(e.what()));
                        execNode(node->children[1], catchEnv);
                    }
                }
                if (node->children[2]) execNode(node->children[2], env);
                return;
            }
            default:
                evalExpr(node, env);
        }
    }

    JSFuncPtr makeUserFunc(NodePtr node, EnvPtr env) {
        auto fn = std::make_shared<JSFunction>();
        fn->name = node->name;
        fn->params = node->params;
        fn->body = node->children[0]; // block
        fn->closure = env;
        fn->proto = std::make_shared<JSObject>();
        return fn;
    }

    // ---- Evaluate expression ----
    ValuePtr evalExpr(NodePtr node, EnvPtr env) {
        if (!node) return Value::makeUndefined();
        switch (node->type) {
            case NodeType::NumLiteral: return Value::makeNumber(node->numVal);
            case NodeType::StrLiteral: return Value::makeString(node->strVal);
            case NodeType::BoolLiteral: return Value::makeBool(node->boolVal);
            case NodeType::NullLiteral: return Value::makeNull();
            case NodeType::UndefinedLiteral: return Value::makeUndefined();
            case NodeType::Identifier: return env->get(node->name);
            case NodeType::TypeofExpr: {
                ValuePtr v;
                try { v = evalExpr(node->children[0], env); }
                catch (...) { return Value::makeString("undefined"); }
                switch (v->type) {
                    case ValueType::Undefined: return Value::makeString("undefined");
                    case ValueType::Null: return Value::makeString("object");
                    case ValueType::Boolean: return Value::makeString("boolean");
                    case ValueType::Number: return Value::makeString("number");
                    case ValueType::String: return Value::makeString("string");
                    case ValueType::Function: return Value::makeString("function");
                    default: return Value::makeString("object");
                }
            }
            case NodeType::ArrayLiteral: {
                auto arr = std::make_shared<JSArray>();
                for (auto& child : node->children) {
                    if (child->type == NodeType::SpreadExpr) {
                        auto spread = evalExpr(child->children[0], env);
                        if (spread->type == ValueType::Array)
                            for (auto& e : spread->arr->elements) arr->elements.push_back(e);
                    } else {
                        arr->elements.push_back(evalExpr(child, env));
                    }
                }
                return Value::makeArray(arr);
            }
            case NodeType::ObjectLiteral: {
                auto obj = std::make_shared<JSObject>();
                for (auto& [k, v] : node->props) {
                    if (k == "__spread__") {
                        // spread another object's props
                        auto spreadVal = evalExpr(v->children[0], env);
                        if (spreadVal->type == ValueType::Object && spreadVal->obj) {
                            for (auto& kv : spreadVal->obj->props)
                                obj->props[kv.first] = kv.second;
                        }
                    } else {
                        obj->props[k] = evalExpr(v, env);
                    }
                }
                return Value::makeObject(obj);
            }
            case NodeType::FuncExpr:
            case NodeType::ArrowFunc: {
                auto fn = makeUserFunc(node, env);
                auto fnVal = Value::makeFunction(fn);
                if (!node->name.empty()) env->define(node->name, fnVal);
                return fnVal;
            }
            case NodeType::UnaryOp: {
                if (node->name == "-") return Value::makeNumber(-toNum(evalExpr(node->children[0], env)));
                if (node->name == "+") return Value::makeNumber(toNum(evalExpr(node->children[0], env)));
                if (node->name == "!") return Value::makeBool(!toBool(evalExpr(node->children[0], env)));
                if (node->name == "~") return Value::makeNumber((double)(~(int)toNum(evalExpr(node->children[0], env))));
                return Value::makeUndefined();
            }
            case NodeType::UpdateExpr: {
                auto& target = node->children[0];
                ValuePtr oldVal, newVal;
                auto doUpdate = [&](ValuePtr v) {
                    oldVal = v;
                    double n = toNum(v);
                    newVal = Value::makeNumber(node->name == "++" ? n+1 : n-1);
                };
                if (target->type == NodeType::Identifier) {
                    doUpdate(env->get(target->name));
                    env->set(target->name, newVal);
                } else if (target->type == NodeType::MemberExpr) {
                    auto obj = evalExpr(target->children[0], env);
                    doUpdate(getProp(obj, target->name));
                    setProp(obj, target->name, newVal);
                } else if (target->type == NodeType::IndexExpr) {
                    auto obj = evalExpr(target->children[0], env);
                    auto key = toStr(evalExpr(target->children[1], env));
                    doUpdate(getProp(obj, key));
                    setProp(obj, key, newVal);
                }
                return node->prefix ? newVal : oldVal;
            }
            case NodeType::BinOp: {
                const std::string& op = node->name;
                // Short-circuit
                if (op == "&&") {
                    auto l = evalExpr(node->children[0], env);
                    return toBool(l) ? evalExpr(node->children[1], env) : l;
                }
                if (op == "||") {
                    auto l = evalExpr(node->children[0], env);
                    return toBool(l) ? l : evalExpr(node->children[1], env);
                }
                auto left  = evalExpr(node->children[0], env);
                auto right = evalExpr(node->children[1], env);
                // Arithmetic
                if (op == "+") {
                    if (left->type == ValueType::String || right->type == ValueType::String)
                        return Value::makeString(toStr(left) + toStr(right));
                    if (left->type == ValueType::Array || right->type == ValueType::Array)
                        return Value::makeString(toStr(left) + toStr(right));
                    return Value::makeNumber(toNum(left) + toNum(right));
                }
                if (op == "-")  return Value::makeNumber(toNum(left) - toNum(right));
                if (op == "*")  return Value::makeNumber(toNum(left) * toNum(right));
                if (op == "/")  return Value::makeNumber(toNum(left) / toNum(right));
                if (op == "%")  return Value::makeNumber(std::fmod(toNum(left), toNum(right)));
                if (op == "**") return Value::makeNumber(std::pow(toNum(left), toNum(right)));
                if (op == "<<") return Value::makeNumber((double)((int)toNum(left) << (int)toNum(right)));
                if (op == ">>") return Value::makeNumber((double)((int)toNum(left) >> (int)toNum(right)));
                if (op == "&")  return Value::makeNumber((double)((int)toNum(left) & (int)toNum(right)));
                if (op == "|")  return Value::makeNumber((double)((int)toNum(left) | (int)toNum(right)));
                if (op == "^")  return Value::makeNumber((double)((int)toNum(left) ^ (int)toNum(right)));
                // Comparison
                if (op == "===") return Value::makeBool(strictEq(left, right));
                if (op == "!==") return Value::makeBool(!strictEq(left, right));
                if (op == "==")  return Value::makeBool(looseEq(left, right));
                if (op == "!=")  return Value::makeBool(!looseEq(left, right));
                if (op == "<") {
                    if (left->type==ValueType::String && right->type==ValueType::String)
                        return Value::makeBool(left->str < right->str);
                    return Value::makeBool(toNum(left) < toNum(right));
                }
                if (op == ">") {
                    if (left->type==ValueType::String && right->type==ValueType::String)
                        return Value::makeBool(left->str > right->str);
                    return Value::makeBool(toNum(left) > toNum(right));
                }
                if (op == "<=") {
                    if (left->type==ValueType::String && right->type==ValueType::String)
                        return Value::makeBool(left->str <= right->str);
                    return Value::makeBool(toNum(left) <= toNum(right));
                }
                if (op == ">=") {
                    if (left->type==ValueType::String && right->type==ValueType::String)
                        return Value::makeBool(left->str >= right->str);
                    return Value::makeBool(toNum(left) >= toNum(right));
                }
                if (op == "instanceof") return Value::makeBool(false); // basic stub
                if (op == "in") {
                    std::string k = toStr(left);
                    if (right->type == ValueType::Object) return Value::makeBool(right->obj->props.count(k) > 0);
                    if (right->type == ValueType::Array) {
                        try { return Value::makeBool(std::stoul(k) < right->arr->elements.size()); } catch (...) {}
                    }
                    return Value::makeBool(false);
                }
                return Value::makeUndefined();
            }
            case NodeType::AssignExpr: {
                const std::string& op = node->name;
                auto& target = node->children[0];
                auto rhs = evalExpr(node->children[1], env);

                auto applyOp = [&](ValuePtr old) -> ValuePtr {
                    if (op == "=")  return rhs;
                    if (op == "+=") {
                        if (old->type == ValueType::String || rhs->type == ValueType::String)
                            return Value::makeString(toStr(old) + toStr(rhs));
                        return Value::makeNumber(toNum(old) + toNum(rhs));
                    }
                    if (op == "-=") return Value::makeNumber(toNum(old) - toNum(rhs));
                    if (op == "*=") return Value::makeNumber(toNum(old) * toNum(rhs));
                    if (op == "/=") return Value::makeNumber(toNum(old) / toNum(rhs));
                    if (op == "%=") return Value::makeNumber(std::fmod(toNum(old), toNum(rhs)));
                    return rhs;
                };

                if (target->type == NodeType::Identifier) {
                    auto newVal = applyOp(env->get(target->name));
                    env->set(target->name, newVal);
                    return newVal;
                }
                if (target->type == NodeType::MemberExpr) {
                    auto obj = evalExpr(target->children[0], env);
                    auto newVal = applyOp(getProp(obj, target->name));
                    setProp(obj, target->name, newVal);
                    return newVal;
                }
                if (target->type == NodeType::IndexExpr) {
                    auto obj = evalExpr(target->children[0], env);
                    auto key = toStr(evalExpr(target->children[1], env));
                    auto newVal = applyOp(getProp(obj, key));
                    setProp(obj, key, newVal);
                    return newVal;
                }
                return rhs;
            }
            case NodeType::MemberExpr: {
                auto obj = evalExpr(node->children[0], env);
                if (node->boolVal && (obj->type == ValueType::Null || obj->type == ValueType::Undefined))
                    return Value::makeUndefined();
                return getProp(obj, node->name);
            }
            case NodeType::IndexExpr: {
                auto obj = evalExpr(node->children[0], env);
                auto key = toStr(evalExpr(node->children[1], env));
                return getProp(obj, key);
            }
            case NodeType::CallExpr: {
                // Evaluate callee
                auto& calleeNode = node->children[0];
                ValuePtr thisVal = Value::makeUndefined();
                ValuePtr calleeVal;

                if (calleeNode->type == NodeType::MemberExpr) {
                    thisVal = evalExpr(calleeNode->children[0], env);
                    calleeVal = getProp(thisVal, calleeNode->name);
                } else if (calleeNode->type == NodeType::IndexExpr) {
                    thisVal = evalExpr(calleeNode->children[0], env);
                    auto key = toStr(evalExpr(calleeNode->children[1], env));
                    calleeVal = getProp(thisVal, key);
                } else {
                    calleeVal = evalExpr(calleeNode, env);
                }

                // Evaluate args
                std::vector<ValuePtr> args;
                for (size_t i = 1; i < node->children.size(); i++) {
                    auto& arg = node->children[i];
                    if (arg->type == NodeType::SpreadExpr) {
                        auto spread = evalExpr(arg->children[0], env);
                        if (spread->type == ValueType::Array)
                            for (auto& e : spread->arr->elements) args.push_back(e);
                    } else {
                        args.push_back(evalExpr(arg, env));
                    }
                }

                if (!calleeVal || calleeVal->type != ValueType::Function) {
                    std::string name = (calleeNode->type == NodeType::Identifier) ? calleeNode->name : 
                                       (calleeNode->type == NodeType::MemberExpr) ? calleeNode->name : "?";
                    throw ThrowSignal{Value::makeString(name + " is not a function")};
                }
                return callFunction(calleeVal->func, args, thisVal);
            }
            case NodeType::NewExpr: {
                auto& calleeNode = node->children[0];
                ValuePtr calleeVal;
                std::vector<ValuePtr> args;
                
                if (calleeNode->type == NodeType::CallExpr) {
                    calleeVal = evalExpr(calleeNode->children[0], env);
                    for (size_t i = 1; i < calleeNode->children.size(); i++)
                        args.push_back(evalExpr(calleeNode->children[i], env));
                } else {
                    calleeVal = evalExpr(calleeNode, env);
                }

                if (!calleeVal || calleeVal->type != ValueType::Function)
                    return Value::makeUndefined();

                auto fn = calleeVal->func;
                auto obj = std::make_shared<JSObject>();
                obj->className = fn->name;
                if (fn->proto) obj->proto = fn->proto;
                auto objVal = Value::makeObject(obj);

                if (fn->isNative) {
                    // For native constructors, just call them
                    return fn->native(args, objVal);
                }

                try {
                    auto result = callFunction(fn, args, objVal);
                    if (result && result->type == ValueType::Object) return result;
                } catch (ReturnSignal& r) {
                    if (r.value && r.value->type == ValueType::Object) return r.value;
                }
                return objVal;
            }
            case NodeType::Ternary: {
                bool cond = toBool(evalExpr(node->children[0], env));
                return cond ? evalExpr(node->children[1], env) : evalExpr(node->children[2], env);
            }
            case NodeType::SequenceExpr: {
                ValuePtr last = Value::makeUndefined();
                for (auto& child : node->children) last = evalExpr(child, env);
                return last;
            }
            case NodeType::SpreadExpr:
                return evalExpr(node->children[0], env);
            default:
                return Value::makeUndefined();
        }
    }

    void run(const std::string& source) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        auto ast = parser.parseProgram();
        execNode(ast, globalEnv);
    }
};

// ================================================================
//  MAIN
// ================================================================
int main(int argc, char* argv[]) {
    std::string source;

    if (argc >= 2) {
        std::ifstream file(argv[1]);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file: " << argv[1] << "\n";
            return 1;
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        source = ss.str();
    } else {
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        source = ss.str();
    }

    try {
        Interpreter interp;
        interp.run(source);
    } catch (ThrowSignal& ts) {
        Interpreter tmp;
        std::cerr << "Uncaught: " << tmp.toStr(ts.value) << "\n";
        return 1;
    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
