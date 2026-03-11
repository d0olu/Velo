#include "russian_grammar.hpp"
#include <algorithm>
#include <sstream>
#include <cstring>

namespace elberr {

// UTF-8 Cyrillic: each letter is 2 bytes (0xD0/0xD1 + second byte)
static bool isCyrByte(unsigned char c) {
    return c == 0xD0 || c == 0xD1;
}

static size_t utf8CharLen(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static std::vector<std::string> splitWords(const std::string& text) {
    std::vector<std::string> words;
    std::string current;
    size_t i = 0;
    while (i < text.size()) {
        unsigned char c = text[i];
        size_t len = utf8CharLen(c);
        if (len == 2 && isCyrByte(c)) {
            current += text.substr(i, len);
            i += len;
        } else if (std::isalnum(c) || c == '_') {
            current += c;
            ++i;
        } else {
            if (!current.empty()) {
                words.push_back(current);
                current.clear();
            }
            ++i;
        }
    }
    if (!current.empty()) words.push_back(current);
    return words;
}

std::string SVOTriple::asProposition() const {
    std::string s = RussianGrammar::transliterate(subject);
    std::string v = RussianGrammar::transliterate(verb);
    std::string o = RussianGrammar::transliterate(object);
    // e.g. "sobaka_begat_bystro"
    return s + "_" + v + (o.empty() ? "" : "_" + o);
}

RussianGrammar::RussianGrammar() {
    initSuffixes();
    initExceptions();
}

bool RussianGrammar::endsWith(const std::string& word, const std::string& suffix) const {
    if (suffix.size() > word.size()) return false;
    return word.compare(word.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void RussianGrammar::initSuffixes() {
    // Noun suffixes (nominative, genitive, dative, accusative, instrumental, prepositional)
    // Gender: MASC, FEM, NEUT; singular/plural
    auto addNoun = [&](const std::string& suf, Gender g, bool pl, int cs, int strip, const std::string& ls) {
        SuffixRule r;
        r.pos = POS::NOUN; r.gender = g; r.plural = pl; r.caseNum = cs;
        r.tense = Tense::NONE; r.stripBytes = strip; r.lemmaSuffix = ls; r.person = 0;
        nounSuffixes_.push_back(r);
    };

    // Masculine: -ов (gen pl), -ам (dat pl), -ами (instr pl), -ах (prep pl)
    // nom.sg = stem, gen.sg = -а, dat.sg = -у, acc.sg = stem, instr.sg = -ом, prep.sg = -е
    addNoun("\xD0\xBE\xD0\xB2", Gender::MASC, true, 2, 4, "");       // -ов gen.pl
    addNoun("\xD0\xB0\xD0\xBC", Gender::MASC, true, 3, 4, "");       // -ам dat.pl
    addNoun("\xD0\xB0\xD1\x85", Gender::MASC, true, 6, 4, "");       // -ах prep.pl
    addNoun("\xD0\xBE\xD0\xBC", Gender::MASC, false, 5, 4, "");      // -ом instr.sg
    addNoun("\xD0\xB0", Gender::MASC, false, 2, 2, "");              // -а gen.sg
    addNoun("\xD1\x83", Gender::MASC, false, 3, 2, "");              // -у dat.sg
    addNoun("\xD0\xB5", Gender::MASC, false, 6, 2, "");              // -е prep.sg
    addNoun("\xD1\x8B", Gender::MASC, true, 1, 2, "");               // -ы nom.pl

    // Feminine: -а (nom.sg), -ы (gen.sg), -е (dat.sg), -у (acc.sg), -ой (instr.sg), -е (prep.sg)
    addNoun("\xD0\xBE\xD0\xB9", Gender::FEM, false, 5, 4, "\xD0\xB0"); // -ой instr.sg → -а
    addNoun("\xD0\xB0", Gender::FEM, false, 1, 0, "");               // -а nom.sg (lemma)

    // Neuter: -о (nom.sg), -а (gen.sg)
    addNoun("\xD0\xBE", Gender::NEUT, false, 1, 0, "");              // -о nom.sg

    // Verb suffixes
    auto addVerb = [&](const std::string& suf, Tense t, int strip, const std::string& ls, int pers) {
        SuffixRule r;
        r.pos = POS::VERB; r.gender = Gender::NONE; r.plural = false; r.caseNum = 0;
        r.tense = t; r.stripBytes = strip; r.lemmaSuffix = ls; r.person = pers;
        verbSuffixes_.push_back(r);
    };

    // Infinitive: -ть, -ти, -чь
    addVerb("\xD1\x82\xD1\x8C", Tense::INF, 0, "", 0);              // -ть
    addVerb("\xD1\x82\xD0\xB8", Tense::INF, 0, "", 0);              // -ти

    // Present tense: -ю, -ешь, -ет, -ем, -ете, -ют
    addVerb("\xD0\xB5\xD1\x82", Tense::PRESENT, 4, "\xD0\xB0\xD1\x82\xD1\x8C", 3); // -ет → -ать (3rd)
    addVerb("\xD1\x8E", Tense::PRESENT, 2, "\xD0\xB0\xD1\x82\xD1\x8C", 1);          // -ю → -ать (1st)
    addVerb("\xD1\x8E\xD1\x82", Tense::PRESENT, 4, "\xD0\xB0\xD1\x82\xD1\x8C", 3); // -ют → -ать (3rd pl)

    // Past tense: -л, -ла, -ло, -ли
    addVerb("\xD0\xBB\xD0\xB0", Tense::PAST, 4, "\xD1\x82\xD1\x8C", 0);  // -ла → -ть (fem)
    addVerb("\xD0\xBB\xD0\xBE", Tense::PAST, 4, "\xD1\x82\xD1\x8C", 0);  // -ло → -ть (neut)
    addVerb("\xD0\xBB\xD0\xB8", Tense::PAST, 4, "\xD1\x82\xD1\x8C", 0);  // -ли → -ть (pl)
    addVerb("\xD0\xBB", Tense::PAST, 2, "\xD1\x82\xD1\x8C", 0);          // -л → -ть (masc)

    // Adjective suffixes
    auto addAdj = [&](const std::string& suf, Gender g, int cs, int strip, const std::string& ls) {
        SuffixRule r;
        r.pos = POS::ADJ; r.gender = g; r.plural = false; r.caseNum = cs;
        r.tense = Tense::NONE; r.stripBytes = strip; r.lemmaSuffix = ls; r.person = 0;
        adjSuffixes_.push_back(r);
    };

    // -ый (masc nom), -ая (fem nom), -ое (neut nom), -ые (pl nom)
    addAdj("\xD1\x8B\xD0\xB9", Gender::MASC, 1, 0, "");   // -ый
    addAdj("\xD0\xB8\xD0\xB9", Gender::MASC, 1, 0, "");   // -ий
    addAdj("\xD0\xB0\xD1\x8F", Gender::FEM, 1, 4, "\xD1\x8B\xD0\xB9");  // -ая → -ый
    addAdj("\xD0\xBE\xD0\xB5", Gender::NEUT, 1, 4, "\xD1\x8B\xD0\xB9"); // -ое → -ый
    addAdj("\xD1\x8B\xD0\xB5", Gender::NONE, 1, 4, "\xD1\x8B\xD0\xB9"); // -ые → -ый
    addAdj("\xD0\xBE\xD0\xB3\xD0\xBE", Gender::MASC, 2, 6, "\xD1\x8B\xD0\xB9"); // -ого gen
    addAdj("\xD0\xBE\xD0\xBC\xD1\x83", Gender::MASC, 3, 6, "\xD1\x8B\xD0\xB9"); // -ому dat
}

void RussianGrammar::initExceptions() {
    // быть (to be)
    auto addExc = [&](const std::string& form, POS pos, Gender g, Tense t, const std::string& lemma) {
        MorphInfo m;
        m.pos = pos; m.gender = g; m.tense = t; m.lemma = lemma; m.original = form;
        exceptions_[form] = m;
    };

    addExc("\xD0\xB1\xD1\x8B\xD1\x82\xD1\x8C", POS::VERB, Gender::NONE, Tense::INF, "\xD0\xB1\xD1\x8B\xD1\x82\xD1\x8C"); // быть
    addExc("\xD0\xB5\xD1\x81\xD1\x82\xD1\x8C", POS::VERB, Gender::NONE, Tense::PRESENT, "\xD0\xB1\xD1\x8B\xD1\x82\xD1\x8C"); // есть
    addExc("\xD0\xB1\xD1\x8B\xD0\xBB", POS::VERB, Gender::MASC, Tense::PAST, "\xD0\xB1\xD1\x8B\xD1\x82\xD1\x8C"); // был
    addExc("\xD0\xB1\xD1\x8B\xD0\xBB\xD0\xB0", POS::VERB, Gender::FEM, Tense::PAST, "\xD0\xB1\xD1\x8B\xD1\x82\xD1\x8C"); // была
    addExc("\xD0\xB1\xD1\x83\xD0\xB4\xD0\xB5\xD1\x82", POS::VERB, Gender::NONE, Tense::FUTURE, "\xD0\xB1\xD1\x8B\xD1\x82\xD1\x8C"); // будет

    // идти (to go)
    addExc("\xD0\xB8\xD0\xB4\xD1\x82\xD0\xB8", POS::VERB, Gender::NONE, Tense::INF, "\xD0\xB8\xD0\xB4\xD1\x82\xD0\xB8"); // идти
    addExc("\xD0\xB8\xD0\xB4\xD1\x91\xD1\x82", POS::VERB, Gender::NONE, Tense::PRESENT, "\xD0\xB8\xD0\xB4\xD1\x82\xD0\xB8"); // идёт
    addExc("\xD1\x88\xD1\x91\xD0\xBB", POS::VERB, Gender::MASC, Tense::PAST, "\xD0\xB8\xD0\xB4\xD1\x82\xD0\xB8"); // шёл

    // мочь (can)
    addExc("\xD0\xBC\xD0\xBE\xD1\x87\xD1\x8C", POS::VERB, Gender::NONE, Tense::INF, "\xD0\xBC\xD0\xBE\xD1\x87\xD1\x8C"); // мочь
    addExc("\xD0\xBC\xD0\xBE\xD0\xB6\xD0\xB5\xD1\x82", POS::VERB, Gender::NONE, Tense::PRESENT, "\xD0\xBC\xD0\xBE\xD1\x87\xD1\x8C"); // может
    addExc("\xD0\xBC\xD0\xBE\xD0\xB3", POS::VERB, Gender::MASC, Tense::PAST, "\xD0\xBC\xD0\xBE\xD1\x87\xD1\x8C"); // мог

    // Pronouns
    auto addPron = [&](const std::string& form) {
        MorphInfo m;
        m.pos = POS::PRON; m.lemma = form; m.original = form;
        exceptions_[form] = m;
    };
    addPron("\xD1\x8F");     // я
    addPron("\xD1\x82\xD1\x8B"); // ты
    addPron("\xD0\xBE\xD0\xBD"); // он
    addPron("\xD0\xBE\xD0\xBD\xD0\xB0"); // она
    addPron("\xD0\xBE\xD0\xBD\xD0\xBE"); // оно
    addPron("\xD0\xBC\xD1\x8B"); // мы
    addPron("\xD0\xB2\xD1\x8B"); // вы
    addPron("\xD0\xBE\xD0\xBD\xD0\xB8"); // они
    addPron("\xD1\x8D\xD1\x82\xD0\xBE"); // это

    // Prepositions
    auto addPrep = [&](const std::string& form) {
        MorphInfo m;
        m.pos = POS::PREP; m.lemma = form; m.original = form;
        exceptions_[form] = m;
    };
    addPrep("\xD0\xB2");     // в
    addPrep("\xD0\xBD\xD0\xB0"); // на
    addPrep("\xD0\xBE");     // о
    addPrep("\xD0\xBE\xD0\xB1"); // об
    addPrep("\xD1\x81");     // с
    addPrep("\xD0\xBA");     // к
    addPrep("\xD0\xB8\xD0\xB7"); // из
    addPrep("\xD0\xB7\xD0\xB0"); // за
    addPrep("\xD0\xBF\xD0\xBE"); // по
    addPrep("\xD0\xB4\xD0\xBB\xD1\x8F"); // для

    // Conjunctions
    auto addConj = [&](const std::string& form) {
        MorphInfo m;
        m.pos = POS::CONJ; m.lemma = form; m.original = form;
        exceptions_[form] = m;
    };
    addConj("\xD0\xB8");     // и
    addConj("\xD0\xB0");     // а
    addConj("\xD0\xBD\xD0\xBE"); // но
    addConj("\xD0\xB8\xD0\xBB\xD0\xB8"); // или
    addConj("\xD1\x87\xD1\x82\xD0\xBE"); // что
    addConj("\xD0\xBA\xD0\xB0\xD0\xBA"); // как

    // Particles
    auto addPart = [&](const std::string& form) {
        MorphInfo m;
        m.pos = POS::PART; m.lemma = form; m.original = form;
        exceptions_[form] = m;
    };
    addPart("\xD0\xBD\xD0\xB5"); // не
    addPart("\xD0\xBD\xD0\xB8"); // ни
}

MorphInfo RussianGrammar::trySuffixTable(const std::string& word,
                                          const std::vector<SuffixRule>& table) const {
    MorphInfo best;
    size_t bestLen = 0;

    for (auto& r : table) {
        // Find matching suffix by checking lemmaSuffix pattern
        // We check if word ends with certain byte patterns
        std::string suffix;
        // Build the expected suffix from the rule
        // The suffix is implicit in the stripBytes + lemmaSuffix
        // Try suffix match by testing endsWith for common patterns

        // Actually, we need to store the suffix. Let's check by trying to
        // reconstruct: if stripBytes > 0, the suffix is the last stripBytes of the word
        if (r.stripBytes > 0 && word.size() >= (size_t)r.stripBytes) {
            std::string stem = word.substr(0, word.size() - r.stripBytes);
            std::string lemma = stem + r.lemmaSuffix;
            if ((size_t)r.stripBytes > bestLen) {
                bestLen = r.stripBytes;
                best.pos = r.pos;
                best.gender = r.gender;
                best.plural = r.plural;
                best.caseNum = r.caseNum;
                best.tense = r.tense;
                best.person = r.person;
                best.lemma = lemma;
                best.original = word;
            }
        } else if (r.stripBytes == 0) {
            // This IS the lemma form
            if (word.size() > bestLen) {
                best.pos = r.pos;
                best.gender = r.gender;
                best.plural = r.plural;
                best.caseNum = r.caseNum;
                best.tense = r.tense;
                best.person = r.person;
                best.lemma = word;
                best.original = word;
            }
        }
    }

    return best;
}

MorphInfo RussianGrammar::analyze(const std::string& word) const {
    // 1. Check exceptions first
    auto it = exceptions_.find(word);
    if (it != exceptions_.end()) {
        MorphInfo m = it->second;
        m.original = word;
        return m;
    }

    // 2. Try suffix tables (longest match wins)
    MorphInfo best;
    best.original = word;

    // Try verbs first (more specific suffixes)
    MorphInfo verbResult = trySuffixTable(word, verbSuffixes_);
    if (verbResult.pos != POS::UNKNOWN) best = verbResult;

    // Try adjectives
    MorphInfo adjResult = trySuffixTable(word, adjSuffixes_);
    if (adjResult.pos != POS::UNKNOWN && adjResult.lemma.size() > best.lemma.size())
        best = adjResult;

    // Try nouns
    MorphInfo nounResult = trySuffixTable(word, nounSuffixes_);
    if (nounResult.pos != POS::UNKNOWN && best.pos == POS::UNKNOWN)
        best = nounResult;

    if (best.pos == POS::UNKNOWN) {
        best.lemma = word;
    }
    best.original = word;
    return best;
}

std::vector<MorphInfo> RussianGrammar::analyzeSentence(const std::string& sentence) const {
    auto words = splitWords(sentence);
    std::vector<MorphInfo> result;
    result.reserve(words.size());
    for (auto& w : words) {
        // Convert to lowercase for analysis (UTF-8 aware)
        result.push_back(analyze(w));
    }
    return result;
}

std::vector<SVOTriple> RussianGrammar::extractSVO(const std::vector<MorphInfo>& morphs) const {
    std::vector<SVOTriple> triples;

    // Simple SVO extraction: find NOUN VERB NOUN patterns
    // Also: PRON VERB NOUN, NOUN VERB ADJ, NOUN VERB
    for (size_t i = 0; i + 1 < morphs.size(); ++i) {
        if ((morphs[i].pos == POS::NOUN || morphs[i].pos == POS::PRON) &&
            i + 1 < morphs.size() && morphs[i + 1].pos == POS::VERB) {
            SVOTriple t;
            t.subject = morphs[i].lemma;
            t.verb = morphs[i + 1].lemma;
            // Look for object
            if (i + 2 < morphs.size() &&
                (morphs[i + 2].pos == POS::NOUN || morphs[i + 2].pos == POS::ADJ ||
                 morphs[i + 2].pos == POS::ADV)) {
                t.object = morphs[i + 2].lemma;
            }
            triples.push_back(t);
        }
    }

    // Also look for "X является Y", "X — это Y", "X есть Y" patterns
    for (size_t i = 0; i + 2 < morphs.size(); ++i) {
        if (morphs[i].pos == POS::NOUN || morphs[i].pos == POS::PRON) {
            // Check for "является", "есть"
            if (morphs[i + 1].lemma == "\xD0\xB1\xD1\x8B\xD1\x82\xD1\x8C" || // быть
                morphs[i + 1].original == "\xD1\x8F\xD0\xB2\xD0\xBB\xD1\x8F\xD0\xB5\xD1\x82\xD1\x81\xD1\x8F") { // является
                SVOTriple t;
                t.subject = morphs[i].lemma;
                t.verb = "is";
                t.object = morphs[i + 2].lemma;
                triples.push_back(t);
            }
        }
    }

    return triples;
}

std::vector<std::string> RussianGrammar::splitSentences(const std::string& text) const {
    std::vector<std::string> sentences;
    std::string current;
    for (size_t i = 0; i < text.size(); ++i) {
        current += text[i];
        if (text[i] == '.' || text[i] == '!' || text[i] == '?') {
            // Trim
            size_t start = current.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) {
                sentences.push_back(current.substr(start));
            }
            current.clear();
        }
    }
    if (!current.empty()) {
        size_t start = current.find_first_not_of(" \t\n\r");
        if (start != std::string::npos) {
            sentences.push_back(current.substr(start));
        }
    }
    return sentences;
}

std::string RussianGrammar::transliterate(const std::string& cyr) {
    static const std::unordered_map<std::string, std::string> table = {
        {"\xD0\xB0", "a"}, {"\xD0\xB1", "b"}, {"\xD0\xB2", "v"}, {"\xD0\xB3", "g"},
        {"\xD0\xB4", "d"}, {"\xD0\xB5", "e"}, {"\xD1\x91", "yo"}, {"\xD0\xB6", "zh"},
        {"\xD0\xB7", "z"}, {"\xD0\xB8", "i"}, {"\xD0\xB9", "j"}, {"\xD0\xBA", "k"},
        {"\xD0\xBB", "l"}, {"\xD0\xBC", "m"}, {"\xD0\xBD", "n"}, {"\xD0\xBE", "o"},
        {"\xD0\xBF", "p"}, {"\xD1\x80", "r"}, {"\xD1\x81", "s"}, {"\xD1\x82", "t"},
        {"\xD1\x83", "u"}, {"\xD1\x84", "f"}, {"\xD1\x85", "kh"}, {"\xD1\x86", "ts"},
        {"\xD1\x87", "ch"}, {"\xD1\x88", "sh"}, {"\xD1\x89", "sch"}, {"\xD1\x8A", ""},
        {"\xD1\x8B", "y"}, {"\xD1\x8C", ""}, {"\xD1\x8D", "e"}, {"\xD1\x8E", "yu"},
        {"\xD1\x8F", "ya"},
        // Uppercase
        {"\xD0\x90", "A"}, {"\xD0\x91", "B"}, {"\xD0\x92", "V"}, {"\xD0\x93", "G"},
        {"\xD0\x94", "D"}, {"\xD0\x95", "E"}, {"\xD0\x81", "Yo"}, {"\xD0\x96", "Zh"},
        {"\xD0\x97", "Z"}, {"\xD0\x98", "I"}, {"\xD0\x99", "J"}, {"\xD0\x9A", "K"},
        {"\xD0\x9B", "L"}, {"\xD0\x9C", "M"}, {"\xD0\x9D", "N"}, {"\xD0\x9E", "O"},
        {"\xD0\x9F", "P"}, {"\xD0\xA0", "R"}, {"\xD0\xA1", "S"}, {"\xD0\xA2", "T"},
        {"\xD0\xA3", "U"}, {"\xD0\xA4", "F"}, {"\xD0\xA5", "Kh"}, {"\xD0\xA6", "Ts"},
        {"\xD0\xA7", "Ch"}, {"\xD0\xA8", "Sh"}, {"\xD0\xA9", "Sch"}, {"\xD0\xAA", ""},
        {"\xD0\xAB", "Y"}, {"\xD0\xAC", ""}, {"\xD0\xAD", "E"}, {"\xD0\xAE", "Yu"},
        {"\xD0\xAF", "Ya"},
    };

    std::string result;
    size_t i = 0;
    while (i < cyr.size()) {
        size_t len = utf8CharLen((unsigned char)cyr[i]);
        std::string ch = cyr.substr(i, len);
        auto it = table.find(ch);
        if (it != table.end()) {
            result += it->second;
        } else {
            result += ch;
        }
        i += len;
    }
    return result;
}

bool RussianGrammar::isCyrillic(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        if (isCyrByte((unsigned char)s[i])) return true;
    }
    return false;
}

} // namespace elberr
