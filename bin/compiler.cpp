// compiler.cpp - Exam-oriented Mini Compiler in pure C++ (NO Flex/Bison/LLVM)
// Demonstrates phases: Lexer -> Parser(AST) -> Semantic Analysis(Symbol Table) -> TAC Generation

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <memory>
#include <cctype>
#include <stdexcept>

using namespace std;

// =========================================================
// 1) LEXICAL ANALYSIS (LEXER)
// =========================================================
enum class TokenType {
    KW_INT, KW_PRINT,
    IDENT, NUMBER,

    PLUS, MINUS, MUL, DIV,
    ASSIGN,

    SEMI, LPAREN, RPAREN,
    END
};

struct Token {
    TokenType type;
    string lexeme;
    int line, col;
};

static string tokenCategory(TokenType tt) {
    switch (tt) {
        case TokenType::KW_INT:
        case TokenType::KW_PRINT: return "KEYWORD";
        case TokenType::IDENT:    return "IDENTIFIER";
        case TokenType::NUMBER:   return "NUMBER";
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::MUL:
        case TokenType::DIV:
        case TokenType::ASSIGN:   return "OPERATOR";
        case TokenType::SEMI:
        case TokenType::LPAREN:
        case TokenType::RPAREN:   return "SYMBOL";
        case TokenType::END:      return "EOF";
    }
    return "UNKNOWN";
}

class Lexer {
    string src;
    size_t i = 0;
    int line = 1, col = 1;

    char peek(int k = 0) const {
        size_t p = i + (size_t)k;
        if (p >= src.size()) return '\0';
        return src[p];
    }

    char get() {
        char c = peek();
        if (c == '\0') return c;
        i++;
        if (c == '\n') { line++; col = 1; }
        else col++;
        return c;
    }

    void skipWSAndComments() {
        while (true) {
            while (isspace((unsigned char)peek())) get();

            // single-line comment //
            if (peek() == '/' && peek(1) == '/') {
                while (peek() != '\n' && peek() != '\0') get();
                continue;
            }
            break;
        }
    }

    [[noreturn]] void lexError(char c, int l, int c0) const {
        ostringstream oss;
        oss << "Lexical error at " << l << ":" << c0
            << " -> Unexpected character '" << c << "'";
        throw runtime_error(oss.str());
    }

public:
    explicit Lexer(string s) : src(std::move(s)) {}

    vector<Token> tokenize() {
        vector<Token> tokens;

        while (true) {
            skipWSAndComments();
            int startLine = line, startCol = col;
            char c = peek();

            if (c == '\0') {
                tokens.push_back({TokenType::END, "EOF", startLine, startCol});
                break;
            }

            // identifier / keyword
            if (isalpha((unsigned char)c) || c == '_') {
                string lex;
                while (isalnum((unsigned char)peek()) || peek() == '_')
                    lex.push_back(get());

                if (lex == "int")   tokens.push_back({TokenType::KW_INT, lex, startLine, startCol});
                else if (lex == "print") tokens.push_back({TokenType::KW_PRINT, lex, startLine, startCol});
                else tokens.push_back({TokenType::IDENT, lex, startLine, startCol});
                continue;
            }

            // number
            if (isdigit((unsigned char)c)) {
                string lex;
                while (isdigit((unsigned char)peek()))
                    lex.push_back(get());
                tokens.push_back({TokenType::NUMBER, lex, startLine, startCol});
                continue;
            }

            // operators / symbols
            switch (c) {
                case '+': get(); tokens.push_back({TokenType::PLUS, "+", startLine, startCol}); continue;
                case '-': get(); tokens.push_back({TokenType::MINUS, "-", startLine, startCol}); continue;
                case '*': get(); tokens.push_back({TokenType::MUL, "*", startLine, startCol}); continue;
                case '/': get(); tokens.push_back({TokenType::DIV, "/", startLine, startCol}); continue;
                case '=': get(); tokens.push_back({TokenType::ASSIGN, "=", startLine, startCol}); continue;
                case ';': get(); tokens.push_back({TokenType::SEMI, ";", startLine, startCol}); continue;
                case '(': get(); tokens.push_back({TokenType::LPAREN, "(", startLine, startCol}); continue;
                case ')': get(); tokens.push_back({TokenType::RPAREN, ")", startLine, startCol}); continue;
                default:  lexError(c, startLine, startCol);
            }
        }

        return tokens;
    }
};

// =========================================================
// AST NODES (Parser output)
// =========================================================
struct Expr { virtual ~Expr() = default; };

struct NumExpr : Expr {
    Token tok;
    explicit NumExpr(Token t) : tok(std::move(t)) {}
};

struct VarExpr : Expr {
    Token tok;
    explicit VarExpr(Token t) : tok(std::move(t)) {}
};

struct UnaryExpr : Expr {
    Token op;
    unique_ptr<Expr> rhs;
    UnaryExpr(Token oper, unique_ptr<Expr> r) : op(std::move(oper)), rhs(std::move(r)) {}
};

struct BinaryExpr : Expr {
    Token op;
    unique_ptr<Expr> lhs, rhs;
    BinaryExpr(unique_ptr<Expr> l, Token oper, unique_ptr<Expr> r)
        : op(std::move(oper)), lhs(std::move(l)), rhs(std::move(r)) {}
};

struct Stmt { virtual ~Stmt() = default; };

struct DeclStmt : Stmt {
    Token name;
    explicit DeclStmt(Token n) : name(std::move(n)) {}
};

struct AssignStmt : Stmt {
    Token name;
    unique_ptr<Expr> rhs;
    AssignStmt(Token n, unique_ptr<Expr> e) : name(std::move(n)), rhs(std::move(e)) {}
};

struct PrintStmt : Stmt {
    Token kw; // 'print'
    unique_ptr<Expr> expr;
    PrintStmt(Token k, unique_ptr<Expr> e) : kw(std::move(k)), expr(std::move(e)) {}
};

struct Program {
    vector<unique_ptr<Stmt>> stmts;
};

// =========================================================
// 2) SYNTAX ANALYSIS (PARSER - builds AST only)
// =========================================================
class Parser {
    const vector<Token>& t;
    size_t p = 0;

    const Token& cur() const { return t[p]; }
    bool at(TokenType tt) const { return cur().type == tt; }

    [[noreturn]] void syntaxError(const string& msg) const {
        ostringstream oss;
        oss << "Syntax error at " << cur().line << ":" << cur().col
            << " near '" << cur().lexeme << "': " << msg;
        throw runtime_error(oss.str());
    }

    Token expect(TokenType tt, const string& msgIfFail) {
        if (!at(tt)) syntaxError(msgIfFail);
        return t[p++];
    }

    bool isStartDecl() const { return at(TokenType::KW_INT); }
    bool isStartStmt() const { return at(TokenType::IDENT) || at(TokenType::KW_PRINT); }

    // Program -> {Decl | Stmt} EOF
    Program parseProgram() {
        Program prog;
        while (!at(TokenType::END)) {
            if (isStartDecl()) prog.stmts.push_back(parseDecl());
            else if (isStartStmt()) prog.stmts.push_back(parseStmt());
            else syntaxError("Expected 'int' declaration or a statement (assignment/print).");
        }
        expect(TokenType::END, "Expected EOF.");
        return prog;
    }

    // Decl -> "int" IDENT ";"
    unique_ptr<Stmt> parseDecl() {
        expect(TokenType::KW_INT, "Expected 'int'.");
        Token id = expect(TokenType::IDENT, "Expected identifier after 'int'.");
        expect(TokenType::SEMI, "Expected ';' after declaration.");
        return make_unique<DeclStmt>(id);
    }

    // Stmt -> Assign ";" | Print ";"
    unique_ptr<Stmt> parseStmt() {
        if (at(TokenType::IDENT)) {
            auto s = parseAssign();
            expect(TokenType::SEMI, "Expected ';' after assignment.");
            return s;
        }
        if (at(TokenType::KW_PRINT)) {
            auto s = parsePrint();
            expect(TokenType::SEMI, "Expected ';' after print.");
            return s;
        }
        syntaxError("Expected statement.");
        return nullptr;
    }

    // Assign -> IDENT "=" Expr
    unique_ptr<Stmt> parseAssign() {
        Token id = expect(TokenType::IDENT, "Expected identifier.");
        expect(TokenType::ASSIGN, "Expected '=' in assignment.");
        auto e = parseExpr();
        return make_unique<AssignStmt>(id, std::move(e));
    }

    // Print -> "print" Expr
    unique_ptr<Stmt> parsePrint() {
        Token kw = expect(TokenType::KW_PRINT, "Expected 'print'.");
        auto e = parseExpr();
        return make_unique<PrintStmt>(kw, std::move(e));
    }

    // Expr -> Term {(+|-) Term}
    unique_ptr<Expr> parseExpr() {
        auto left = parseTerm();
        while (at(TokenType::PLUS) || at(TokenType::MINUS)) {
            Token op = cur(); p++;
            auto right = parseTerm();
            left = make_unique<BinaryExpr>(std::move(left), op, std::move(right));
        }
        return left;
    }

    // Term -> Unary {(*|/) Unary}
    unique_ptr<Expr> parseTerm() {
        auto left = parseUnary();
        while (at(TokenType::MUL) || at(TokenType::DIV)) {
            Token op = cur(); p++;
            auto right = parseUnary();
            left = make_unique<BinaryExpr>(std::move(left), op, std::move(right));
        }
        return left;
    }

    // Unary -> (+|-) Unary | Primary
    unique_ptr<Expr> parseUnary() {
        if (at(TokenType::PLUS) || at(TokenType::MINUS)) {
            Token op = cur(); p++;
            auto rhs = parseUnary();
            return make_unique<UnaryExpr>(op, std::move(rhs));
        }
        return parsePrimary();
    }

    // Primary -> NUMBER | IDENT | "(" Expr ")"
    unique_ptr<Expr> parsePrimary() {
        if (at(TokenType::NUMBER)) {
            Token n = cur(); p++;
            return make_unique<NumExpr>(n);
        }
        if (at(TokenType::IDENT)) {
            Token id = cur(); p++;
            return make_unique<VarExpr>(id);
        }
        if (at(TokenType::LPAREN)) {
            p++;
            auto e = parseExpr();
            expect(TokenType::RPAREN, "Expected ')' to close '('.");
            return e;
        }
        syntaxError("Expected NUMBER, IDENTIFIER, or '(' expression ')'.");
        return nullptr;
    }

public:
    explicit Parser(const vector<Token>& tokens) : t(tokens) {}
    Program parse() { return parseProgram(); }
};

// =========================================================
// 3) SEMANTIC ANALYSIS (Symbol Table + checks)
// =========================================================
struct Symbol {
    string name;
    string type;   // only "int"
};

class SemanticAnalyzer {
    unordered_map<string, Symbol> table;
    vector<string> order;

    [[noreturn]] void semError(const Token& where, const string& msg) const {
        ostringstream oss;
        oss << "Semantic error at " << where.line << ":" << where.col
            << " near '" << where.lexeme << "': " << msg;
        throw runtime_error(oss.str());
    }

    void checkExpr(const Expr* e) {
        if (auto n = dynamic_cast<const NumExpr*>(e)) {
            (void)n; // ok
            return;
        }
        if (auto v = dynamic_cast<const VarExpr*>(e)) {
            if (table.find(v->tok.lexeme) == table.end())
                semError(v->tok, "Variable '" + v->tok.lexeme + "' used before declaration.");
            return;
        }
        if (auto u = dynamic_cast<const UnaryExpr*>(e)) {
            checkExpr(u->rhs.get());
            return;
        }
        if (auto b = dynamic_cast<const BinaryExpr*>(e)) {
            checkExpr(b->lhs.get());
            checkExpr(b->rhs.get());
            return;
        }
        throw runtime_error("Internal error: Unknown Expr node in semantic analysis.");
    }

public:
    void analyze(const Program& prog) {
        for (const auto& st : prog.stmts) {
            if (auto d = dynamic_cast<const DeclStmt*>(st.get())) {
                const string& name = d->name.lexeme;
                if (table.find(name) != table.end())
                    semError(d->name, "Duplicate declaration of '" + name + "'.");
                table[name] = Symbol{name, "int"};
                order.push_back(name);
                continue;
            }
            if (auto a = dynamic_cast<const AssignStmt*>(st.get())) {
                const string& name = a->name.lexeme;
                if (table.find(name) == table.end())
                    semError(a->name, "Assignment to undeclared variable '" + name + "'.");
                checkExpr(a->rhs.get());
                continue;
            }
            if (auto pr = dynamic_cast<const PrintStmt*>(st.get())) {
                checkExpr(pr->expr.get());
                continue;
            }
            throw runtime_error("Internal error: Unknown Stmt node in semantic analysis.");
        }
    }

    const unordered_map<string, Symbol>& symbols() const { return table; }
    const vector<string>& symbolOrder() const { return order; }
};

// =========================================================
// 4) INTERMEDIATE CODE GENERATION (TAC)
// =========================================================
class TACGenerator {
    vector<string> code;
    int tempCounter = 0;

    string newTemp() { return "t" + to_string(++tempCounter); }

    string genExpr(const Expr* e) {
        if (auto n = dynamic_cast<const NumExpr*>(e)) {
            return n->tok.lexeme; // immediate constant is fine in TAC
        }
        if (auto v = dynamic_cast<const VarExpr*>(e)) {
            return v->tok.lexeme;
        }
        if (auto u = dynamic_cast<const UnaryExpr*>(e)) {
            string r = genExpr(u->rhs.get());
            // Keep TAC simple & canonical: t = 0 - r  (for unary minus)
            if (u->op.type == TokenType::MINUS) {
                string t = newTemp();
                code.push_back(t + " = 0 - " + r);
                return t;
            }
            // unary plus: just return rhs
            return r;
        }
        if (auto b = dynamic_cast<const BinaryExpr*>(e)) {
            string l = genExpr(b->lhs.get());
            string r = genExpr(b->rhs.get());
            string t = newTemp();
            code.push_back(t + " = " + l + " " + b->op.lexeme + " " + r);
            return t;
        }
        throw runtime_error("Internal error: Unknown Expr node in TAC generation.");
    }

public:
    vector<string> generate(const Program& prog) {
        code.clear();
        tempCounter = 0;

        for (const auto& st : prog.stmts) {
            if (dynamic_cast<const DeclStmt*>(st.get())) {
                // For this lab compiler, declarations do not generate TAC.
                continue;
            }
            if (auto a = dynamic_cast<const AssignStmt*>(st.get())) {
                string rhs = genExpr(a->rhs.get());
                code.push_back(a->name.lexeme + " = " + rhs);
                continue;
            }
            if (auto pr = dynamic_cast<const PrintStmt*>(st.get())) {
                string x = genExpr(pr->expr.get());
                code.push_back("print " + x);
                continue;
            }
            throw runtime_error("Internal error: Unknown Stmt node in TAC generation.");
        }

        return code;
    }
};

// =========================================================
// OUTPUT HELPERS (Exam format)
// =========================================================
static void printTokens(const vector<Token>& toks) {
    cout << "TOKENS:\n";
    for (const auto& tk : toks) {
        if (tk.type == TokenType::END) break; // keep output clean
        cout << left << setw(10) << tk.lexeme << " " << tokenCategory(tk.type) << "\n";
    }
    cout << "\n";
}

static void printSymbolTable(const SemanticAnalyzer& sem) {
    cout << "SYMBOL TABLE:\n";
    cout << left << setw(10) << "Name" << "Type\n";
    for (const auto& name : sem.symbolOrder()) {
        cout << left << setw(10) << name << "int\n";
    }
    cout << "\n";
}

static void printTAC(const vector<string>& tac) {
    cout << "INTERMEDIATE CODE (TAC):\n";
    for (const auto& line : tac) cout << line << "\n";
    cout << "\n";
}

int main() {
    try {
        // Read entire source program from stdin
        ostringstream oss;
        oss << cin.rdbuf();
        string src = oss.str();

        // Phase 1: Lexer
        Lexer lexer(src);
        auto tokens = lexer.tokenize();
        printTokens(tokens);

        // Phase 2: Parser -> AST
        Parser parser(tokens);
        Program ast = parser.parse();

        // Phase 3: Semantic analysis -> Symbol table checks
        SemanticAnalyzer sem;
        sem.analyze(ast);
        printSymbolTable(sem);

        // Phase 4: TAC generation
        TACGenerator gen;
        auto tac = gen.generate(ast);
        printTAC(tac);

        return 0;
    } catch (const exception& ex) {
        cerr << ex.what() << "\n";
        return 1;
    }
}
