#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace elberr {

enum class POS { NOUN, VERB, ADJ, ADV, PREP, CONJ, PRON, PART, UNKNOWN };
enum class Gender { MASC, FEM, NEUT, NONE };
enum class Tense { PAST, PRESENT, FUTURE, INF, NONE };

struct MorphInfo {
    POS pos = POS::UNKNOWN;
    Gender gender = Gender::NONE;
    bool plural = false;
    int caseNum = 0;       // 1-6 for nouns (nominative..prepositional)
    Tense tense = Tense::NONE;
    int person = 0;        // 1,2,3 for verbs
    std::string lemma;     // dictionary form
    std::string original;  // as seen in text
};

struct SVOTriple {
    std::string subject;
    std::string verb;
    std::string object;
    std::string asProposition() const;
};

class RussianGrammar {
public:
    RussianGrammar();

    // Morphological analysis of a single word
    MorphInfo analyze(const std::string& word) const;

    // Analyze a sentence, return morphed words
    std::vector<MorphInfo> analyzeSentence(const std::string& sentence) const;

    // Extract SVO triples from analyzed sentence
    std::vector<SVOTriple> extractSVO(const std::vector<MorphInfo>& morphs) const;

    // Split Russian text into sentences
    std::vector<std::string> splitSentences(const std::string& text) const;

    // Transliterate Cyrillic to Latin for atom names
    static std::string transliterate(const std::string& cyr);

    // Check if string contains Cyrillic
    static bool isCyrillic(const std::string& s);

private:
    struct SuffixRule {
        POS pos;
        Gender gender;
        bool plural;
        int caseNum;
        Tense tense;
        int stripBytes;
        std::string lemmaSuffix;
        int person;
    };

    std::vector<SuffixRule> nounSuffixes_;
    std::vector<SuffixRule> verbSuffixes_;
    std::vector<SuffixRule> adjSuffixes_;
    std::unordered_map<std::string, MorphInfo> exceptions_;

    void initSuffixes();
    void initExceptions();

    bool endsWith(const std::string& word, const std::string& suffix) const;
    MorphInfo trySuffixTable(const std::string& word,
                             const std::vector<SuffixRule>& table) const;
};

} // namespace elberr
