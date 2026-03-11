#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

namespace elberr {

enum class FormulaType {
    ATOM,       // "rain", "power_on"
    NOT,        // ~φ
    AND,        // φ & ψ
    OR,         // φ | ψ
    IMPLIES,    // φ -> ψ
    BICONDI,    // φ <-> ψ
    KNOWS,      // K(φ) — agent knows φ
    BELIEVES    // B(φ) — agent believes φ
};

struct Formula {
    FormulaType type;
    std::string atom;                        // only for ATOM
    std::shared_ptr<Formula> left;           // unary: child; binary: left
    std::shared_ptr<Formula> right;          // binary: right

    // convenience constructors
    static std::shared_ptr<Formula> makeAtom(const std::string& name);
    static std::shared_ptr<Formula> makeNot(std::shared_ptr<Formula> f);
    static std::shared_ptr<Formula> makeAnd(std::shared_ptr<Formula> l, std::shared_ptr<Formula> r);
    static std::shared_ptr<Formula> makeOr(std::shared_ptr<Formula> l, std::shared_ptr<Formula> r);
    static std::shared_ptr<Formula> makeImplies(std::shared_ptr<Formula> l, std::shared_ptr<Formula> r);
    static std::shared_ptr<Formula> makeBicondi(std::shared_ptr<Formula> l, std::shared_ptr<Formula> r);
    static std::shared_ptr<Formula> makeKnows(std::shared_ptr<Formula> f);
    static std::shared_ptr<Formula> makeBelieves(std::shared_ptr<Formula> f);

    std::string toString() const;
    void collectAtoms(std::vector<std::string>& out) const;
};

using FormulaPtr = std::shared_ptr<Formula>;

// Recursive descent parser for formula strings
// Grammar:
//   expr     = bicondi
//   bicondi  = implies ("<->" implies)*
//   implies  = disjunct ("->" disjunct)*
//   disjunct = conjunct ("|" conjunct)*
//   conjunct = unary ("&" unary)*
//   unary    = "~" unary | "K(" expr ")" | "B(" expr ")" | atom | "(" expr ")"
//   atom     = [a-zA-Z_][a-zA-Z0-9_]*
class FormulaParser {
public:
    FormulaPtr parse(const std::string& input);

private:
    std::string src_;
    size_t pos_ = 0;

    void skipSpaces();
    char peek();
    char advance();
    bool match(const std::string& s);
    FormulaPtr parseExpr();
    FormulaPtr parseBicondi();
    FormulaPtr parseImplies();
    FormulaPtr parseDisjunct();
    FormulaPtr parseConjunct();
    FormulaPtr parseUnary();
    FormulaPtr parseAtom();
};

} // namespace elberr
