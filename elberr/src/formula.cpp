#include "formula.hpp"
#include <stdexcept>
#include <sstream>

namespace elberr {

FormulaPtr Formula::makeAtom(const std::string& name) {
    auto f = std::make_shared<Formula>();
    f->type = FormulaType::ATOM;
    f->atom = name;
    return f;
}

FormulaPtr Formula::makeNot(FormulaPtr child) {
    auto f = std::make_shared<Formula>();
    f->type = FormulaType::NOT;
    f->left = std::move(child);
    return f;
}

FormulaPtr Formula::makeAnd(FormulaPtr l, FormulaPtr r) {
    auto f = std::make_shared<Formula>();
    f->type = FormulaType::AND;
    f->left = std::move(l);
    f->right = std::move(r);
    return f;
}

FormulaPtr Formula::makeOr(FormulaPtr l, FormulaPtr r) {
    auto f = std::make_shared<Formula>();
    f->type = FormulaType::OR;
    f->left = std::move(l);
    f->right = std::move(r);
    return f;
}

FormulaPtr Formula::makeImplies(FormulaPtr l, FormulaPtr r) {
    auto f = std::make_shared<Formula>();
    f->type = FormulaType::IMPLIES;
    f->left = std::move(l);
    f->right = std::move(r);
    return f;
}

FormulaPtr Formula::makeBicondi(FormulaPtr l, FormulaPtr r) {
    auto f = std::make_shared<Formula>();
    f->type = FormulaType::BICONDI;
    f->left = std::move(l);
    f->right = std::move(r);
    return f;
}

FormulaPtr Formula::makeKnows(FormulaPtr child) {
    auto f = std::make_shared<Formula>();
    f->type = FormulaType::KNOWS;
    f->left = std::move(child);
    return f;
}

FormulaPtr Formula::makeBelieves(FormulaPtr child) {
    auto f = std::make_shared<Formula>();
    f->type = FormulaType::BELIEVES;
    f->left = std::move(child);
    return f;
}

std::string Formula::toString() const {
    switch (type) {
        case FormulaType::ATOM: return atom;
        case FormulaType::NOT: return "~" + left->toString();
        case FormulaType::AND: return "(" + left->toString() + " & " + right->toString() + ")";
        case FormulaType::OR: return "(" + left->toString() + " | " + right->toString() + ")";
        case FormulaType::IMPLIES: return "(" + left->toString() + " -> " + right->toString() + ")";
        case FormulaType::BICONDI: return "(" + left->toString() + " <-> " + right->toString() + ")";
        case FormulaType::KNOWS: return "K(" + left->toString() + ")";
        case FormulaType::BELIEVES: return "B(" + left->toString() + ")";
    }
    return "?";
}

void Formula::collectAtoms(std::vector<std::string>& out) const {
    if (type == FormulaType::ATOM) {
        out.push_back(atom);
        return;
    }
    if (left) left->collectAtoms(out);
    if (right) right->collectAtoms(out);
}

// --- Parser ---

FormulaPtr FormulaParser::parse(const std::string& input) {
    src_ = input;
    pos_ = 0;
    auto result = parseExpr();
    skipSpaces();
    if (pos_ < src_.size()) {
        throw std::runtime_error("Unexpected character at position " + std::to_string(pos_));
    }
    return result;
}

void FormulaParser::skipSpaces() {
    while (pos_ < src_.size() && (src_[pos_] == ' ' || src_[pos_] == '\t'))
        ++pos_;
}

char FormulaParser::peek() {
    skipSpaces();
    return pos_ < src_.size() ? src_[pos_] : '\0';
}

char FormulaParser::advance() {
    skipSpaces();
    return pos_ < src_.size() ? src_[pos_++] : '\0';
}

bool FormulaParser::match(const std::string& s) {
    skipSpaces();
    if (src_.compare(pos_, s.size(), s) == 0) {
        pos_ += s.size();
        return true;
    }
    return false;
}

FormulaPtr FormulaParser::parseExpr() {
    return parseBicondi();
}

FormulaPtr FormulaParser::parseBicondi() {
    auto left = parseImplies();
    while (match("<->")) {
        auto right = parseImplies();
        left = Formula::makeBicondi(left, right);
    }
    return left;
}

FormulaPtr FormulaParser::parseImplies() {
    auto left = parseDisjunct();
    while (match("->")) {
        auto right = parseDisjunct();
        left = Formula::makeImplies(left, right);
    }
    return left;
}

FormulaPtr FormulaParser::parseDisjunct() {
    auto left = parseConjunct();
    while (peek() == '|') {
        advance();
        auto right = parseConjunct();
        left = Formula::makeOr(left, right);
    }
    return left;
}

FormulaPtr FormulaParser::parseConjunct() {
    auto left = parseUnary();
    while (peek() == '&') {
        advance();
        auto right = parseUnary();
        left = Formula::makeAnd(left, right);
    }
    return left;
}

FormulaPtr FormulaParser::parseUnary() {
    if (peek() == '~') {
        advance();
        return Formula::makeNot(parseUnary());
    }
    if (match("K(")) {
        auto inner = parseExpr();
        if (peek() != ')') throw std::runtime_error("Expected ')' after K(...)");
        advance();
        return Formula::makeKnows(inner);
    }
    if (match("B(")) {
        auto inner = parseExpr();
        if (peek() != ')') throw std::runtime_error("Expected ')' after B(...)");
        advance();
        return Formula::makeBelieves(inner);
    }
    if (peek() == '(') {
        advance();
        auto inner = parseExpr();
        if (peek() != ')') throw std::runtime_error("Expected ')'");
        advance();
        return inner;
    }
    return parseAtom();
}

FormulaPtr FormulaParser::parseAtom() {
    skipSpaces();
    if (pos_ >= src_.size()) throw std::runtime_error("Unexpected end of formula");
    size_t start = pos_;
    while (pos_ < src_.size() && (std::isalnum(src_[pos_]) || src_[pos_] == '_'))
        ++pos_;
    if (pos_ == start) throw std::runtime_error("Expected atom at position " + std::to_string(pos_));
    return Formula::makeAtom(src_.substr(start, pos_ - start));
}

} // namespace elberr
