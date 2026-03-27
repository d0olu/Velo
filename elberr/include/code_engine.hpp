#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace elberr {

// Parsed C++ construct from own source code
struct CodeConstruct {
    enum Kind { FUNCTION, CLASS, METHOD, MEMBER, INCLUDE, ENUM, NAMESPACE, COMMENT, UNKNOWN };
    Kind kind = UNKNOWN;
    std::string name;
    std::string file;
    int line = 0;
    std::string signature;     // full function/method signature
    std::string body;          // function body or class body
    std::string parentClass;   // for methods
    std::vector<std::string> dependencies;  // what it calls/uses
    int complexity = 0;        // rough cyclomatic complexity
    int lineCount = 0;
};

// A proposed self-modification
struct CodePatch {
    std::string file;
    int lineStart = 0;
    int lineEnd = 0;
    std::string oldCode;
    std::string newCode;
    std::string reason;        // why this change
    std::string category;      // "bugfix", "optimization", "feature", "refactor"
    double confidence = 0.0;   // how sure the agent is this is correct
};

// Result of a compilation attempt
struct CompileResult {
    bool success = false;
    std::string output;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    double durationSec = 0.0;
};

// CodeEngine: lets ELBERR read, understand, modify, and recompile itself
class CodeEngine {
public:
    explicit CodeEngine(const std::string& sourceDir);

    // === Phase 1: READ own source ===
    // Scan all .hpp/.cpp files in source directory
    void scanSourceFiles();
    // Get all discovered source file paths
    const std::vector<std::string>& sourceFiles() const { return sourceFiles_; }
    // Read a specific file, return contents
    std::string readFile(const std::string& path) const;

    // === Phase 2: PARSE C++ structure ===
    // Parse all scanned files into constructs
    void parseAll();
    // Get parsed constructs
    const std::vector<CodeConstruct>& constructs() const { return constructs_; }
    // Find constructs by name
    std::vector<CodeConstruct> findByName(const std::string& name) const;
    // Find constructs in a file
    std::vector<CodeConstruct> findInFile(const std::string& file) const;
    // Get dependency graph: who calls whom
    std::unordered_map<std::string, std::vector<std::string>> dependencyGraph() const;

    // === Phase 3: REASON about code ===
    // Find potential bugs (null deref, missing checks, etc.)
    std::vector<CodePatch> findPotentialBugs() const;
    // Find optimization opportunities
    std::vector<CodePatch> findOptimizations() const;
    // Suggest improvements based on learned patterns
    std::vector<CodePatch> suggestImprovements() const;

    // === Phase 4: MODIFY self ===
    // Apply a patch to source
    bool applyPatch(const CodePatch& patch);
    // Revert last applied patch
    bool revertLast();
    // Compile the project
    CompileResult compile();
    // Full cycle: apply patch → compile → revert if failed
    bool tryPatch(const CodePatch& patch, CompileResult& result);

    // === Phase 5: LEARN from code ===
    // Extract "lessons" from reading code — returns belief atoms
    // e.g. "pattern_raii_guard", "uses_shared_ptr", "has_mutex"
    std::vector<std::pair<std::string, double>> extractBeliefs() const;

    // Stats
    size_t totalLines() const;
    size_t totalFunctions() const;
    size_t totalClasses() const;
    std::string summary() const;

    // Track what the agent has successfully changed
    const std::vector<CodePatch>& appliedPatches() const { return appliedPatches_; }
    int successfulPatches() const { return successfulPatches_; }
    int failedPatches() const { return failedPatches_; }

private:
    std::string sourceDir_;
    std::string buildDir_;
    std::vector<std::string> sourceFiles_;
    std::vector<CodeConstruct> constructs_;
    std::vector<CodePatch> appliedPatches_;
    std::string lastBackup_;    // content before last patch
    std::string lastPatchFile_; // file of last patch
    int successfulPatches_ = 0;
    int failedPatches_ = 0;

    // Parsing helpers
    void parseFile(const std::string& path);
    void extractFunctions(const std::string& content, const std::string& file);
    void extractClasses(const std::string& content, const std::string& file);
    void extractIncludes(const std::string& content, const std::string& file);
    int estimateComplexity(const std::string& body) const;
    std::vector<std::string> extractCalls(const std::string& body) const;

    // Code pattern detection
    bool hasNullCheck(const std::string& body, const std::string& ptr) const;
    bool hasMutexGuard(const std::string& body) const;
    bool hasErrorHandling(const std::string& body) const;

    // File I/O
    bool writeFile(const std::string& path, const std::string& content);
    std::vector<std::string> splitLines(const std::string& content) const;
    std::string joinLines(const std::vector<std::string>& lines) const;
};

} // namespace elberr
