#include "code_engine.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>
#include <chrono>
#include <dirent.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cstring>
#include <array>

namespace elberr {

CodeEngine::CodeEngine(const std::string& sourceDir)
    : sourceDir_(sourceDir)
{
    // Build directory is next to source
    buildDir_ = sourceDir_ + "/../build";
}

// === Phase 1: READ ===

void CodeEngine::scanSourceFiles() {
    sourceFiles_.clear();

    auto scanDir = [&](const std::string& dir) {
        DIR* d = opendir(dir.c_str());
        if (!d) return;
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            std::string fullPath = dir + "/" + name;

            struct stat st;
            if (stat(fullPath.c_str(), &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) continue; // don't recurse for now

            // Only C++ files
            if (name.size() > 4 &&
                (name.substr(name.size() - 4) == ".cpp" ||
                 name.substr(name.size() - 4) == ".hpp")) {
                sourceFiles_.push_back(fullPath);
            }
        }
        closedir(d);
    };

    scanDir(sourceDir_ + "/src");
    scanDir(sourceDir_ + "/include");

    std::sort(sourceFiles_.begin(), sourceFiles_.end());
    std::cout << "[CodeEngine] Scanned " << sourceFiles_.size() << " source files\n";
}

std::string CodeEngine::readFile(const std::string& path) const {
    std::ifstream ifs(path);
    if (!ifs) return "";
    return std::string(std::istreambuf_iterator<char>(ifs),
                       std::istreambuf_iterator<char>());
}

// === Phase 2: PARSE ===

void CodeEngine::parseAll() {
    constructs_.clear();
    for (auto& file : sourceFiles_) {
        parseFile(file);
    }
    std::cout << "[CodeEngine] Parsed " << constructs_.size() << " constructs from "
              << sourceFiles_.size() << " files\n";
}

void CodeEngine::parseFile(const std::string& path) {
    std::string content = readFile(path);
    if (content.empty()) return;

    // Extract short filename for display
    std::string file = path;
    size_t lastSlash = path.rfind('/');
    if (lastSlash != std::string::npos) {
        size_t prevSlash = path.rfind('/', lastSlash - 1);
        if (prevSlash != std::string::npos)
            file = path.substr(prevSlash + 1);
    }

    extractIncludes(content, file);
    extractClasses(content, file);
    extractFunctions(content, file);
}

void CodeEngine::extractIncludes(const std::string& content, const std::string& file) {
    std::istringstream iss(content);
    std::string line;
    int lineNum = 0;
    while (std::getline(iss, line)) {
        ++lineNum;
        // #include "..." or #include <...>
        size_t pos = line.find("#include");
        if (pos != std::string::npos) {
            CodeConstruct c;
            c.kind = CodeConstruct::INCLUDE;
            c.file = file;
            c.line = lineNum;
            // Extract include name
            size_t start = line.find_first_of("\"<", pos + 8);
            size_t end = line.find_first_of("\">", start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                c.name = line.substr(start + 1, end - start - 1);
            }
            c.signature = line;
            constructs_.push_back(c);
        }
    }
}

void CodeEngine::extractClasses(const std::string& content, const std::string& file) {
    // Find "class ClassName" or "struct ClassName"
    auto lines = splitLines(content);
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        // Simple pattern: "class Name {" or "class Name :"
        size_t classPos = std::string::npos;
        bool isStruct = false;

        size_t cp = line.find("class ");
        size_t sp = line.find("struct ");
        if (cp != std::string::npos && (cp == 0 || line[cp-1] == ' ' || line[cp-1] == '\t')) {
            classPos = cp + 6;
        } else if (sp != std::string::npos && (sp == 0 || line[sp-1] == ' ' || line[sp-1] == '\t')) {
            classPos = sp + 7;
            isStruct = true;
        }

        if (classPos == std::string::npos) continue;
        // Skip forward declarations (no { on this or next line)
        if (line.find(';') != std::string::npos && line.find('{') == std::string::npos) continue;

        // Extract class name
        std::string name;
        size_t p = classPos;
        while (p < line.size() && (std::isalnum(line[p]) || line[p] == '_')) {
            name += line[p++];
        }
        if (name.empty()) continue;

        // Find class body (match braces)
        std::string body;
        int braceDepth = 0;
        bool started = false;
        size_t bodyStart = i;
        size_t bodyEnd = i;
        for (size_t j = i; j < lines.size(); ++j) {
            for (char ch : lines[j]) {
                if (ch == '{') { braceDepth++; started = true; }
                if (ch == '}') braceDepth--;
            }
            body += lines[j] + "\n";
            if (started && braceDepth <= 0) {
                bodyEnd = j;
                break;
            }
        }

        CodeConstruct c;
        c.kind = CodeConstruct::CLASS;
        c.name = name;
        c.file = file;
        c.line = static_cast<int>(i + 1);
        c.signature = (isStruct ? "struct " : "class ") + name;
        c.body = body;
        c.lineCount = static_cast<int>(bodyEnd - bodyStart + 1);
        constructs_.push_back(c);
    }
}

void CodeEngine::extractFunctions(const std::string& content, const std::string& file) {
    auto lines = splitLines(content);

    // Pattern: "ReturnType Name::Method(" or "ReturnType function("
    // Simplified: look for lines with "(" that are followed by "{"
    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];

        // Skip preprocessor, includes, comments
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        if (line[first] == '#' || line[first] == '/') continue;
        if (line.find("//") == first) continue;

        // Must have '(' to be a function
        size_t parenPos = line.find('(');
        if (parenPos == std::string::npos) continue;

        // Must not be just a call inside a function body (check indentation)
        if (first > 4) continue; // indented too much — likely inside a function

        // Skip "if(", "while(", "for(", "switch(", "return(", "catch("
        std::string beforeParen = line.substr(0, parenPos);
        size_t lastSpace = beforeParen.find_last_of(" \t");
        std::string lastWord;
        if (lastSpace != std::string::npos) {
            lastWord = beforeParen.substr(lastSpace + 1);
        } else {
            lastWord = beforeParen;
        }
        // Trim
        while (!lastWord.empty() && (lastWord.front() == ' ' || lastWord.front() == '\t'))
            lastWord.erase(lastWord.begin());

        if (lastWord == "if" || lastWord == "while" || lastWord == "for" ||
            lastWord == "switch" || lastWord == "return" || lastWord == "catch" ||
            lastWord == "class" || lastWord == "struct" || lastWord == "enum" ||
            lastWord == "namespace" || lastWord == "using" || lastWord == "typedef") continue;

        // Find the opening brace — could be on same line or next
        bool hasBrace = false;
        size_t braceStart = i;
        if (line.find('{') != std::string::npos) {
            hasBrace = true;
        } else if (i + 1 < lines.size() && lines[i + 1].find('{') != std::string::npos) {
            hasBrace = true;
            braceStart = i + 1;
        } else if (i + 2 < lines.size() && lines[i + 2].find('{') != std::string::npos) {
            hasBrace = true;
            braceStart = i + 2;
        }

        if (!hasBrace) continue;
        // Skip pure declarations (with ; before {)
        if (line.find(';') != std::string::npos && line.find('{') == std::string::npos) continue;

        // Extract function body
        std::string body;
        int braceDepth = 0;
        bool started = false;
        size_t bodyEnd = braceStart;
        for (size_t j = braceStart; j < lines.size(); ++j) {
            for (char ch : lines[j]) {
                if (ch == '{') { braceDepth++; started = true; }
                if (ch == '}') braceDepth--;
            }
            body += lines[j] + "\n";
            if (started && braceDepth <= 0) {
                bodyEnd = j;
                break;
            }
        }

        // Extract function name (handle Class::Method)
        std::string funcName = lastWord;
        std::string className;
        size_t colonPos = funcName.find("::");
        if (colonPos != std::string::npos) {
            className = funcName.substr(0, colonPos);
            funcName = funcName.substr(colonPos + 2);
        }

        CodeConstruct c;
        c.kind = className.empty() ? CodeConstruct::FUNCTION : CodeConstruct::METHOD;
        c.name = funcName;
        c.parentClass = className;
        c.file = file;
        c.line = static_cast<int>(i + 1);
        c.signature = line;
        // Trim signature
        while (!c.signature.empty() && (c.signature.back() == '{' || c.signature.back() == ' '))
            c.signature.pop_back();
        c.body = body;
        c.lineCount = static_cast<int>(bodyEnd - braceStart + 1);
        c.complexity = estimateComplexity(body);
        c.dependencies = extractCalls(body);
        constructs_.push_back(c);
    }
}

int CodeEngine::estimateComplexity(const std::string& body) const {
    int complexity = 1; // base
    // Count decision points
    std::vector<std::string> keywords = {"if ", "if(", "else if", "while ", "while(",
                                          "for ", "for(", "case ", "catch ", "&&", "||", "?"};
    for (auto& kw : keywords) {
        size_t pos = 0;
        while ((pos = body.find(kw, pos)) != std::string::npos) {
            ++complexity;
            pos += kw.size();
        }
    }
    return complexity;
}

std::vector<std::string> CodeEngine::extractCalls(const std::string& body) const {
    std::vector<std::string> calls;
    // Simple: find "identifier(" patterns
    size_t i = 0;
    while (i < body.size()) {
        // Skip strings
        if (body[i] == '"') {
            ++i;
            while (i < body.size() && body[i] != '"') {
                if (body[i] == '\\') ++i;
                ++i;
            }
            if (i < body.size()) ++i;
            continue;
        }
        // Skip comments
        if (i + 1 < body.size() && body[i] == '/' && body[i+1] == '/') {
            while (i < body.size() && body[i] != '\n') ++i;
            continue;
        }

        if (body[i] == '(' && i > 0) {
            // Walk back to get identifier
            size_t end = i;
            size_t start = i - 1;
            while (start > 0 && (std::isalnum(body[start]) || body[start] == '_' ||
                                  body[start] == ':')) {
                --start;
            }
            if (!std::isalnum(body[start]) && body[start] != '_') ++start;
            if (start < end) {
                std::string call = body.substr(start, end - start);
                // Filter out keywords
                if (call != "if" && call != "while" && call != "for" &&
                    call != "switch" && call != "return" && call != "sizeof" &&
                    call != "static_cast" && call != "reinterpret_cast" &&
                    call != "dynamic_cast" && call != "const_cast" &&
                    !call.empty()) {
                    calls.push_back(call);
                }
            }
        }
        ++i;
    }

    // Deduplicate
    std::sort(calls.begin(), calls.end());
    calls.erase(std::unique(calls.begin(), calls.end()), calls.end());
    return calls;
}

std::vector<CodeConstruct> CodeEngine::findByName(const std::string& name) const {
    std::vector<CodeConstruct> result;
    for (auto& c : constructs_) {
        if (c.name == name || c.name.find(name) != std::string::npos) {
            result.push_back(c);
        }
    }
    return result;
}

std::vector<CodeConstruct> CodeEngine::findInFile(const std::string& file) const {
    std::vector<CodeConstruct> result;
    for (auto& c : constructs_) {
        if (c.file == file || c.file.find(file) != std::string::npos) {
            result.push_back(c);
        }
    }
    return result;
}

std::unordered_map<std::string, std::vector<std::string>> CodeEngine::dependencyGraph() const {
    std::unordered_map<std::string, std::vector<std::string>> graph;
    for (auto& c : constructs_) {
        if (c.kind == CodeConstruct::FUNCTION || c.kind == CodeConstruct::METHOD) {
            std::string key = c.parentClass.empty() ? c.name : c.parentClass + "::" + c.name;
            graph[key] = c.dependencies;
        }
    }
    return graph;
}

// === Phase 3: REASON ===

bool CodeEngine::hasNullCheck(const std::string& body, const std::string& ptr) const {
    return body.find("if (" + ptr) != std::string::npos ||
           body.find("if (" + ptr + " ==") != std::string::npos ||
           body.find("if (!" + ptr) != std::string::npos ||
           body.find(ptr + " != nullptr") != std::string::npos ||
           body.find(ptr + " == nullptr") != std::string::npos;
}

bool CodeEngine::hasMutexGuard(const std::string& body) const {
    return body.find("lock_guard") != std::string::npos ||
           body.find("unique_lock") != std::string::npos ||
           body.find("scoped_lock") != std::string::npos ||
           body.find(".lock()") != std::string::npos;
}

bool CodeEngine::hasErrorHandling(const std::string& body) const {
    return body.find("try") != std::string::npos ||
           body.find("catch") != std::string::npos ||
           body.find("if (!") != std::string::npos ||
           body.find("if (err") != std::string::npos ||
           body.find("if (res !=") != std::string::npos;
}

std::vector<CodePatch> CodeEngine::findPotentialBugs() const {
    std::vector<CodePatch> patches;

    for (auto& c : constructs_) {
        if (c.kind != CodeConstruct::FUNCTION && c.kind != CodeConstruct::METHOD) continue;

        // Bug: shared_ptr dereferenced without null check
        if (c.body.find("->") != std::string::npos) {
            // Check for patterns like "f->left->toString()" without null check
            // Look for member access on shared_ptr without guard
            if (c.body.find("left->") != std::string::npos && !hasNullCheck(c.body, "left")) {
                CodePatch p;
                p.file = c.file;
                p.lineStart = c.line;
                p.reason = "Potential null dereference: 'left' accessed without null check in " +
                           c.parentClass + "::" + c.name;
                p.category = "bugfix";
                p.confidence = 0.6;
                patches.push_back(p);
            }
            if (c.body.find("right->") != std::string::npos && !hasNullCheck(c.body, "right")) {
                CodePatch p;
                p.file = c.file;
                p.lineStart = c.line;
                p.reason = "Potential null dereference: 'right' accessed without null check in " +
                           c.parentClass + "::" + c.name;
                p.category = "bugfix";
                p.confidence = 0.5;
                patches.push_back(p);
            }
        }

        // Bug: mutex-protected data accessed in multi-thread context without lock
        if ((c.body.find("entries_") != std::string::npos ||
             c.body.find("rules_") != std::string::npos ||
             c.body.find("vocab_") != std::string::npos) &&
            !hasMutexGuard(c.body) &&
            c.body.find("thread") == std::string::npos) {
            // Only flag if function seems to be called from callbacks
            if (c.name.find("onPage") != std::string::npos ||
                c.name.find("onChat") != std::string::npos ||
                c.name.find("callback") != std::string::npos) {
                CodePatch p;
                p.file = c.file;
                p.lineStart = c.line;
                p.reason = "Shared data accessed in callback without mutex in " + c.name;
                p.category = "bugfix";
                p.confidence = 0.7;
                patches.push_back(p);
            }
        }

        // Bug: high cyclomatic complexity
        if (c.complexity > 15) {
            CodePatch p;
            p.file = c.file;
            p.lineStart = c.line;
            p.reason = "High complexity (" + std::to_string(c.complexity) +
                       ") in " + c.parentClass + "::" + c.name + " — consider splitting";
            p.category = "refactor";
            p.confidence = 0.4;
            patches.push_back(p);
        }
    }

    return patches;
}

std::vector<CodePatch> CodeEngine::findOptimizations() const {
    std::vector<CodePatch> patches;

    for (auto& c : constructs_) {
        if (c.kind != CodeConstruct::FUNCTION && c.kind != CodeConstruct::METHOD) continue;

        // String concatenation in loop — suggest reserve
        if (c.body.find("for") != std::string::npos &&
            c.body.find("+=") != std::string::npos &&
            c.body.find("reserve") == std::string::npos) {
            // Check if it's string concatenation
            if (c.body.find("std::string") != std::string::npos ||
                c.body.find("result +=") != std::string::npos ||
                c.body.find("text +=") != std::string::npos) {
                CodePatch p;
                p.file = c.file;
                p.lineStart = c.line;
                p.reason = "String concatenation in loop without reserve() in " + c.name;
                p.category = "optimization";
                p.confidence = 0.5;
                patches.push_back(p);
            }
        }

        // Linear search in addPattern — suggest unordered_set
        if (c.name == "addPattern" && c.body.find("for (auto&") != std::string::npos) {
            CodePatch p;
            p.file = c.file;
            p.lineStart = c.line;
            p.reason = "O(n) dedup in addPattern — should use unordered_set for O(1) lookup";
            p.category = "optimization";
            p.confidence = 0.8;
            patches.push_back(p);
        }
    }

    return patches;
}

std::vector<CodePatch> CodeEngine::suggestImprovements() const {
    std::vector<CodePatch> patches;

    // Check for JSON building without escaping
    for (auto& c : constructs_) {
        if (c.body.find("\"type\":\"") != std::string::npos &&
            c.body.find("\\\"") != std::string::npos) {
            // Building JSON by string concat
            if (c.body.find("escapeJson") == std::string::npos &&
                c.body.find("json_escape") == std::string::npos) {
                CodePatch p;
                p.file = c.file;
                p.lineStart = c.line;
                p.reason = "JSON built by concatenation without escaping — XSS/parse risk in " + c.name;
                p.category = "bugfix";
                p.confidence = 0.9;
                patches.push_back(p);
            }
        }
    }

    return patches;
}

// === Phase 4: MODIFY ===

bool CodeEngine::applyPatch(const CodePatch& patch) {
    if (patch.oldCode.empty() || patch.newCode.empty()) return false;

    // Find the actual file path
    std::string fullPath;
    for (auto& f : sourceFiles_) {
        if (f.find(patch.file) != std::string::npos) {
            fullPath = f;
            break;
        }
    }
    if (fullPath.empty()) return false;

    std::string content = readFile(fullPath);
    if (content.empty()) return false;

    // Backup
    lastBackup_ = content;
    lastPatchFile_ = fullPath;

    // Find and replace
    size_t pos = content.find(patch.oldCode);
    if (pos == std::string::npos) {
        std::cerr << "[CodeEngine] Patch target not found in " << patch.file << "\n";
        return false;
    }

    content.replace(pos, patch.oldCode.size(), patch.newCode);

    if (!writeFile(fullPath, content)) return false;

    appliedPatches_.push_back(patch);
    std::cout << "[CodeEngine] Applied patch: " << patch.reason << "\n";
    return true;
}

bool CodeEngine::revertLast() {
    if (lastBackup_.empty() || lastPatchFile_.empty()) return false;
    bool ok = writeFile(lastPatchFile_, lastBackup_);
    if (ok) {
        std::cout << "[CodeEngine] Reverted last patch\n";
        if (!appliedPatches_.empty()) appliedPatches_.pop_back();
    }
    lastBackup_.clear();
    lastPatchFile_.clear();
    return ok;
}

CompileResult CodeEngine::compile() {
    CompileResult result;

    auto start = std::chrono::steady_clock::now();

    // Ensure build directory exists
    std::string mkdirCmd = "mkdir -p " + buildDir_;
    int ret = system(mkdirCmd.c_str());
    (void)ret;

    // Run cmake + make
    std::string cmd = "cd " + buildDir_ +
                      " && cmake " + sourceDir_ + " -DCMAKE_CXX_STANDARD=17 2>&1" +
                      " && make -j$(nproc) 2>&1";

    // Capture output
    std::array<char, 4096> buffer;
    std::string output;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        result.errors.push_back("Failed to run compiler");
        return result;
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }
    int exitCode = pclose(pipe);

    auto end = std::chrono::steady_clock::now();
    result.durationSec = std::chrono::duration<double>(end - start).count();
    result.output = output;
    result.success = (exitCode == 0);

    // Parse errors and warnings
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find("error:") != std::string::npos) {
            result.errors.push_back(line);
        } else if (line.find("warning:") != std::string::npos) {
            result.warnings.push_back(line);
        }
    }

    std::cout << "[CodeEngine] Compile " << (result.success ? "OK" : "FAILED")
              << " (" << result.durationSec << "s, "
              << result.errors.size() << " errors, "
              << result.warnings.size() << " warnings)\n";

    return result;
}

bool CodeEngine::tryPatch(const CodePatch& patch, CompileResult& result) {
    if (!applyPatch(patch)) return false;

    result = compile();
    if (!result.success) {
        std::cout << "[CodeEngine] Patch failed compilation — reverting\n";
        revertLast();
        ++failedPatches_;
        return false;
    }

    ++successfulPatches_;
    std::cout << "[CodeEngine] Patch compiled successfully!\n";
    return true;
}

// === Phase 5: LEARN ===

std::vector<std::pair<std::string, double>> CodeEngine::extractBeliefs() const {
    std::vector<std::pair<std::string, double>> beliefs;

    // Count patterns across codebase
    int totalFuncs = 0, funcsWithGuard = 0, funcsWithErrorHandling = 0;
    int totalComplexity = 0;

    for (auto& c : constructs_) {
        if (c.kind == CodeConstruct::FUNCTION || c.kind == CodeConstruct::METHOD) {
            ++totalFuncs;
            totalComplexity += c.complexity;
            if (hasMutexGuard(c.body)) ++funcsWithGuard;
            if (hasErrorHandling(c.body)) ++funcsWithErrorHandling;
        }
    }

    if (totalFuncs > 0) {
        beliefs.push_back({"code_total_functions_" + std::to_string(totalFuncs), 1.0});
        beliefs.push_back({"code_total_files_" + std::to_string(sourceFiles_.size()), 1.0});
        beliefs.push_back({"code_avg_complexity_" +
            std::to_string(totalComplexity / totalFuncs), 0.9});
        beliefs.push_back({"code_mutex_coverage_" +
            std::to_string(funcsWithGuard * 100 / totalFuncs) + "pct", 0.8});
        beliefs.push_back({"code_error_handling_" +
            std::to_string(funcsWithErrorHandling * 100 / totalFuncs) + "pct", 0.8});
    }

    // Detect patterns used
    for (auto& c : constructs_) {
        if (c.body.find("shared_ptr") != std::string::npos)
            beliefs.push_back({"pattern_shared_ptr", 0.9});
        if (c.body.find("lock_guard") != std::string::npos)
            beliefs.push_back({"pattern_raii_lock", 0.9});
        if (c.body.find("atomic") != std::string::npos)
            beliefs.push_back({"pattern_atomic", 0.9});
        if (c.body.find("std::thread") != std::string::npos)
            beliefs.push_back({"pattern_multithreading", 0.9});
        if (c.body.find("make_shared") != std::string::npos)
            beliefs.push_back({"pattern_factory", 0.7});
    }

    // Deduplicate
    std::sort(beliefs.begin(), beliefs.end());
    beliefs.erase(std::unique(beliefs.begin(), beliefs.end()), beliefs.end());

    return beliefs;
}

// === Stats ===

size_t CodeEngine::totalLines() const {
    size_t total = 0;
    for (auto& f : sourceFiles_) {
        std::string content = readFile(f);
        total += std::count(content.begin(), content.end(), '\n');
    }
    return total;
}

size_t CodeEngine::totalFunctions() const {
    size_t count = 0;
    for (auto& c : constructs_) {
        if (c.kind == CodeConstruct::FUNCTION || c.kind == CodeConstruct::METHOD) ++count;
    }
    return count;
}

size_t CodeEngine::totalClasses() const {
    size_t count = 0;
    for (auto& c : constructs_) {
        if (c.kind == CodeConstruct::CLASS) ++count;
    }
    return count;
}

std::string CodeEngine::summary() const {
    std::ostringstream ss;
    ss << "CodeEngine: " << sourceFiles_.size() << " files, "
       << constructs_.size() << " constructs ("
       << totalFunctions() << " functions, "
       << totalClasses() << " classes), "
       << successfulPatches_ << " successful patches, "
       << failedPatches_ << " failed patches";
    return ss.str();
}

// === Helpers ===

bool CodeEngine::writeFile(const std::string& path, const std::string& content) {
    std::ofstream ofs(path);
    if (!ofs) {
        std::cerr << "[CodeEngine] Failed to write " << path << "\n";
        return false;
    }
    ofs << content;
    return true;
}

std::vector<std::string> CodeEngine::splitLines(const std::string& content) const {
    std::vector<std::string> lines;
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string CodeEngine::joinLines(const std::vector<std::string>& lines) const {
    std::string result;
    for (auto& line : lines) {
        result += line + "\n";
    }
    return result;
}

} // namespace elberr
