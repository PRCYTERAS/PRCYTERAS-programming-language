#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// ben yaptim codex - 2026-11-15 - yalnizca stdlibCall icinde gercek runtime yonlendirmesi olan moduller
static const std::unordered_set<std::string>& stdlibNamespaces() {
    static const std::unordered_set<std::string> names = {
        "Json", "Base64", "Regex", "DateTime", "Toml", "Yaml", "Xml", "Dotenv",
        "Uuid", "Logger", "Stats", "Fuzzy",
        "GoogleGenai", "OpenAi", "Anthropic", "Ollama", "Groq", "HuggingFace", "ElevenLabs", "Whisper", "StabilityAi",
        "VectorDb", "NeuralNet", "NeatAi", "Pathfinding", "NavMesh3D", "BehaviorTree", "Discord", "DiscordRpc", "Telegram",
        "WhatsApp", "Twitch", "YouTubeLive", "Slack", "Smtp", "Webhook", "Irc",
        "Http", "Socket", "WebSocket", "WebServer", "Html", "Downloader", "Ftp",
        "Dns", "OAuth2", "Jwt", "RateLimiter", "Upnp", "Steam", "Sqlite",
        "Firebase", "Supabase", "Redis", "MySql", "BinaryPack",
        "Camera2D", "Tween", "Particle2D", "Lighting2D", "Physics2D", "Ecs", "Gamepad",
        "Localization", "Noise", "QrCode", "Crypto", "Argon2", "ZipArchive",
        "FileWatcher", "Clipboard", "WinDialog", "Registry", "Screenshot", "GifRecorder", "FontLoader", "ImageEdit", "TrayIcon", "CliArgs", "TerminalUi", "LinAlg", "FixedPoint",
        "BigInt", "Decimal", "Profiler", "UnitTest", "Scheduler"
    };
    return names;
}
static bool isStdlibNamespace(const std::string& name) { return stdlibNamespaces().count(name) != 0; }
static std::unordered_set<std::string> baseNamespaceRoots() {
    std::unordered_set<std::string> roots{"Engine", "Engine3D", "Physics3D", "UI", "Color", "File", "Dosya", "Console", "Konsol", "Math", "Matematik", "System", "Sistem", "String", "Metin", "Arena", "Scheduler"};
    roots.insert(stdlibNamespaces().begin(), stdlibNamespaces().end());
    return roots;
}

// ben yaptım opencode - 2026-11-14 - yerel paket yoneticisi ve stdlib modulleri
static fs::path prcyterasDataRoot() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return {};
    return fs::path(std::wstring(buffer.data(), length)) / L"PRCYTERAS";
}

static fs::path installedPackagesRoot() {
    const fs::path root = prcyterasDataRoot();
    return root.empty() ? fs::path{} : root / L"packages";
}

static fs::path bundledPackagesRoot() {
    std::wstring buffer(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0 || size >= buffer.size()) return {};
    buffer.resize(size);
    return fs::path(buffer).parent_path() / L"packages";
}

static std::string trimPackageText(std::string value) {
    std::size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    std::size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
    return value.substr(first, last - first);
}

static std::unordered_map<std::string, std::string> readPackageInfo(const fs::path& directory) {
    std::unordered_map<std::string, std::string> info;
    std::ifstream input(directory / L"package.info", std::ios::binary);
    std::string line;
    while (std::getline(input, line)) {
        line = trimPackageText(line);
        if (line.empty() || line[0] == '#') continue;
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        info[trimPackageText(line.substr(0, equals))] = trimPackageText(line.substr(equals + 1));
    }
    return info;
}

static std::string packageEntryName(const std::unordered_map<std::string, std::string>& info, const std::wstring& name) {
    const auto entry = info.find("entry");
    if (entry != info.end() && !entry->second.empty()) return entry->second;
    return std::string(name.begin(), name.end()) + ".pts";
}

static std::wstring asciiWide(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

static bool validPackageName(const std::wstring& name) {
    if (name.empty() || name == L"." || name == L"..") return false;
    for (const wchar_t c : name) {
        if (!(std::iswalnum(c) || c == L'-' || c == L'_' || c == L'.')) return false;
    }
    return name.find(L"..") == std::wstring::npos;
}

static void installBundledPackage(const std::wstring& name) {
    if (!validPackageName(name))
        throw std::runtime_error("Gecersiz paket adi. Yalnizca harf, rakam, nokta, tire ve alt cizgi kullanin.");
    const fs::path bundled = bundledPackagesRoot() / name;
    if (bundled.empty() || !fs::is_directory(bundled))
        throw std::runtime_error("Paket bulunamadi: " + std::string(name.begin(), name.end()) + ". Mevcut paketler icin: pts cargo search");
    const auto info = readPackageInfo(bundled);
    const std::string entry = packageEntryName(info, name);
    if (!fs::exists(bundled / fs::path(entry)))
        throw std::runtime_error("Paket giris dosyasi bulunamadi: " + entry);
    const fs::path root = installedPackagesRoot();
    if (root.empty()) throw std::runtime_error("Paket dizini icin LOCALAPPDATA bulunamadi.");
    const fs::path target = root / name;
    std::error_code error;
    fs::create_directories(root, error);
    if (error) throw std::runtime_error("Paket dizini olusturulamadi: " + error.message());
    const bool existed = fs::exists(target);
    const std::wstring suffix = L".prcyteras-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    const fs::path staged = root / (name + suffix + L".stage");
    const fs::path backup = root / (name + suffix + L".backup");
    fs::remove_all(staged, error);
    error.clear();
    fs::copy(bundled, staged, fs::copy_options::recursive | fs::copy_options::overwrite_existing, error);
    if (error) {
        const std::string message = error.message();
        std::error_code ignored;
        fs::remove_all(staged, ignored);
        throw std::runtime_error("Paket gecici alana kopyalanamadi: " + message);
    }
    if (existed) {
        fs::rename(target, backup, error);
        if (error) {
            const std::string message = error.message();
            std::error_code ignored;
            fs::remove_all(staged, ignored);
            throw std::runtime_error("Mevcut paket yedeklenemedi; eski kopya korundu: " + message);
        }
    }
    fs::rename(staged, target, error);
    if (error) {
        const std::string message = error.message();
        std::error_code ignored;
        fs::remove_all(staged, ignored);
        if (existed) {
            std::error_code restoreError;
            fs::rename(backup, target, restoreError);
            if (restoreError) throw std::runtime_error("Yeni paket etkinlestirilemedi (" + message + ") ve eski kopya geri yuklenemedi (" + restoreError.message() + "); yedek: " + backup.string());
        }
        throw std::runtime_error("Yeni paket etkinlestirilemedi; eski kopya korundu: " + message);
    }
    if (existed) {
        std::error_code cleanupError;
        fs::remove_all(backup, cleanupError);
        if (cleanupError) std::wcerr << L"Uyari: eski paket yedegi silinemedi: " << backup.wstring() << L"\n";
    }
    std::wcout << (existed ? L"Guncellendi: " : L"Kuruldu: ") << name;
    const auto version = info.find("version");
    if (version != info.end() && !version->second.empty()) std::wcout << L" " << asciiWide(version->second);
    std::wcout << L" -> " << target.wstring() << L"\n";
}

static int installAllBundledPackages() {
    const fs::path root = bundledPackagesRoot();
    if (root.empty() || !fs::is_directory(root)) throw std::runtime_error("Yerel paket katalogu bulunamadi.");
    std::vector<std::wstring> names;
    for (const auto& entry : fs::directory_iterator(root)) {
        if (entry.is_directory()) names.push_back(entry.path().filename().wstring());
    }
    std::sort(names.begin(), names.end());
    if (names.empty()) throw std::runtime_error("Yerel paket katalogu bos.");
    std::size_t installed = 0;
    std::vector<std::pair<std::wstring, std::string>> failures;
    for (const auto& name : names) {
        try {
            installBundledPackage(name);
            ++installed;
        } catch (const std::exception& error) {
            failures.emplace_back(name, error.what());
            std::wcerr << L"HATA: " << name << L" kurulamadi; katalogdaki diger paketlere devam ediliyor.\n";
            std::cerr << "  " << error.what() << "\n";
        }
    }
    std::wcout << L"Sonuc: " << installed << L"/" << names.size() << L" yerel paket kaynagi kuruldu/guncellendi.";
    if (!failures.empty()) std::wcout << L" " << failures.size() << L" paket basarisiz.";
    std::wcout << L"\n";
    return failures.empty() ? 0 : 1;
}

static int listInstalledPackages() {
    const fs::path root = installedPackagesRoot();
    if (root.empty() || !fs::is_directory(root)) {
        std::wcout << L"Kurulu paket yok.\n";
        return 0;
    }
    std::vector<fs::path> packages;
    for (const auto& entry : fs::directory_iterator(root)) if (entry.is_directory()) packages.push_back(entry.path());
    std::sort(packages.begin(), packages.end());
    if (packages.empty()) {
        std::wcout << L"Kurulu paket yok.\n";
        return 0;
    }
    for (const auto& directory : packages) {
        const auto info = readPackageInfo(directory);
        const auto version = info.find("version");
        const auto description = info.find("description");
        std::wcout << directory.filename().wstring();
        if (version != info.end() && !version->second.empty()) std::wcout << L" " << asciiWide(version->second);
        if (description != info.end() && !description->second.empty()) std::wcout << L" - " << asciiWide(description->second);
        std::wcout << L"\n";
    }
    return 0;
}

static int searchBundledPackages() {
    const fs::path root = bundledPackagesRoot();
    if (root.empty() || !fs::is_directory(root)) {
        std::wcout << L"Gomulu paket bulunamadi.\n";
        return 0;
    }
    std::vector<fs::path> packages;
    for (const auto& entry : fs::directory_iterator(root)) if (entry.is_directory()) packages.push_back(entry.path());
    std::sort(packages.begin(), packages.end());
    for (const auto& directory : packages) {
        const auto info = readPackageInfo(directory);
        const auto version = info.find("version");
        const auto description = info.find("description");
        std::wcout << directory.filename().wstring();
        if (version != info.end() && !version->second.empty()) std::wcout << L" " << asciiWide(version->second);
        if (description != info.end() && !description->second.empty()) std::wcout << L" - " << asciiWide(description->second);
        std::wcout << L"\n";
    }
    return 0;
}

static int uninstallPackage(const std::wstring& name) {
    if (!validPackageName(name))
        throw std::runtime_error("Gecersiz paket adi. Yalnizca harf, rakam, nokta, tire ve alt cizgi kullanin.");
    const fs::path root = installedPackagesRoot();
    if (root.empty()) throw std::runtime_error("Paket dizini icin LOCALAPPDATA bulunamadi.");
    const fs::path target = root / name;
    if (!fs::exists(target)) throw std::runtime_error("Paket kurulu degil: " + std::string(name.begin(), name.end()));
    std::error_code error;
    fs::remove_all(target, error);
    if (error) throw std::runtime_error("Paket kaldirilamadi: " + error.message());
    std::wcout << L"Kaldirildi: " << name << L"\n";
    return 0;
}

static fs::path packageModuleEntry(const fs::path& root, const std::string& module) {
    if (root.empty() || module.empty()) return {};
    fs::path relative;
    std::size_t start = 0;
    while (start < module.size()) {
        const std::size_t dot = module.find('.', start);
        relative /= asciiWide(module.substr(start, dot == std::string::npos ? std::string::npos : dot - start));
        if (dot == std::string::npos) break;
        start = dot + 1;
    }
    const fs::path directory = root / relative;
    if (!fs::is_directory(directory)) return {};
    const auto info = readPackageInfo(directory);
    const fs::path entry = directory / asciiWide(packageEntryName(info, directory.filename().wstring()));
    if (!fs::exists(entry)) return {};
    return fs::weakly_canonical(entry);
}

static fs::path installedPackageModule(const std::string& module) {
    fs::path result = packageModuleEntry(installedPackagesRoot(), module);
    if (!result.empty()) return result;
    return packageModuleEntry(bundledPackagesRoot(), module);
}

enum class Kind {
    End, Newline, Indent, Dedent, Identifier, Number, String,
    LParen, RParen, LBracket, RBracket, Comma, Colon, Dot, Semicolon,
    Plus, Minus, Star, Slash, Percent,
    Assign, PlusAssign, MinusAssign, StarAssign, SlashAssign, PercentAssign,
    BitAndAssign, BitOrAssign, BitXorAssign, ShiftLeftAssign, ShiftRightAssign,
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,
    Bang, AndAnd, OrOr, BitAnd, BitOr, BitXor, ShiftLeft, ShiftRight, Tilde, Arrow
};

struct Token {
    Kind kind;
    std::string text;
    int line;
};

class CompileError : public std::runtime_error {
public:
    explicit CompileError(const std::string& message) : std::runtime_error(message) {}
};

static std::string lineMessage(int line, const std::string& message) {
    return "Satir " + std::to_string(line) + ": " + message;
}

static void printDiagnostic(const fs::path& path, const std::string& source, const std::string& message) {
    std::size_t lineNumber = 0;
    const std::size_t marker = message.find("Satir ");
    if (marker != std::string::npos) {
        const std::size_t firstDigit = marker + 6;
        try { lineNumber = std::stoull(message.substr(firstDigit)); } catch (...) {}
    }
    std::cerr << "PRCYTERAS derleme hatasi\n";
    if (!path.empty()) std::cerr << " --> " << path.string() << (lineNumber ? ":" + std::to_string(lineNumber) : "") << "\n";
    if (lineNumber) {
        std::istringstream lines(source);
        std::string line;
        for (std::size_t current = 1; current <= lineNumber && std::getline(lines, line); ++current) {
            if (current == lineNumber) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                const std::string number = std::to_string(current);
                std::cerr << "  |\n" << number << " | " << line << "\n";
                std::cerr << std::string(number.size(), ' ') << " | ^\n";
            }
        }
    }
    std::cerr << " = " << message << "\n";
}

class Lexer {
    const std::string& source;
    size_t pos = 0;
    int line = 1;
    bool lineStart = true;
    std::vector<int> indentation{0};
    std::vector<Token> tokens;

    char peek(size_t ahead = 0) const {
        return pos + ahead < source.size() ? source[pos + ahead] : '\0';
    }
    char take() {
        char c = peek();
        if (c != '\0') {
            ++pos;
            if (c == '\n') ++line;
        }
        return c;
    }
    void add(Kind kind, std::string text = {}, int atLine = -1) {
        tokens.push_back(Token{kind, std::move(text), atLine < 0 ? line : atLine});
    }
    static bool identStart(unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80;
    }
    static bool identPart(unsigned char c) {
        return identStart(c) || (c >= '0' && c <= '9');
    }
    void newline() {
        if (peek() == '\r') take();
        if (peek() == '\n') take();
        add(Kind::Newline, {}, line - 1);
        lineStart = true;
    }
    void skipLineComment() {
        while (peek() != '\n' && peek() != '\0') take();
    }
    void scanString(int atLine) {
        take();
        std::string value;
        while (peek() != '"' && peek() != '\0') {
            char c = take();
            if (c == '\n') throw CompileError(lineMessage(atLine, "Metin ayni satirda bitmeli."));
            if (c == '\\') {
                char escaped = take();
                switch (escaped) {
                    case 'n': value.push_back('\n'); break;
                    case 'r': value.push_back('\r'); break;
                    case 't': value.push_back('\t'); break;
                    case '\\': value.push_back('\\'); break;
                    case '"': value.push_back('"'); break;
                    default: throw CompileError(lineMessage(atLine, "Bilinmeyen metin kacisi."));
                }
            } else {
                value.push_back(c);
            }
        }
        if (peek() != '"') throw CompileError(lineMessage(atLine, "Metin tirnakla kapatilmamis."));
        take();
        add(Kind::String, std::move(value), atLine);
    }
    bool two(char first, char second) const { return peek() == first && peek(1) == second; }
public:
    explicit Lexer(const std::string& text) : source(text) {}

    std::vector<Token> scan() {
        while (peek() != '\0') {
            if (lineStart) {
                int spaces = 0;
                while (peek() == ' ') { take(); ++spaces; }
                if (peek() == '\t') throw CompileError(lineMessage(line, "Girinti icin sekme yerine bosluk kullanin."));
                if (peek() == '\r' || peek() == '\n') { newline(); continue; }
                if (peek() == '#' || two('/', '/')) { skipLineComment(); continue; }
                if (spaces > indentation.back()) {
                    indentation.push_back(spaces);
                    add(Kind::Indent, {}, line);
                } else if (spaces < indentation.back()) {
                    while (indentation.size() > 1 && spaces < indentation.back()) {
                        indentation.pop_back();
                        add(Kind::Dedent, {}, line);
                    }
                    if (spaces != indentation.back()) throw CompileError(lineMessage(line, "Girinti onceki bloklarla uyusmuyor."));
                }
                lineStart = false;
            }

            unsigned char c = static_cast<unsigned char>(peek());
            int atLine = line;
            if (peek() == '\r') { take(); continue; }
            if (peek() == '\n') { newline(); continue; }
            if (peek() == ' ' || peek() == '\t') { take(); continue; }
            if (peek() == '#' || two('/', '/')) { skipLineComment(); continue; }
            if (peek() == '"') { scanString(atLine); continue; }
            if (identStart(c)) {
                size_t start = pos;
                while (identPart(static_cast<unsigned char>(peek()))) take();
                add(Kind::Identifier, source.substr(start, pos - start), atLine);
                continue;
            }
            if (c >= '0' && c <= '9') {
                size_t start = pos;
                while (peek() >= '0' && peek() <= '9') take();
                if (peek() == '.' && peek(1) >= '0' && peek(1) <= '9') {
                    take();
                    while (peek() >= '0' && peek() <= '9') take();
                }
                add(Kind::Number, source.substr(start, pos - start), atLine);
                continue;
            }

            auto singleOrDouble = [&](char second, Kind doubleKind, Kind singleKind) {
                take();
                if (peek() == second) { take(); add(doubleKind, {}, atLine); }
                else add(singleKind, {}, atLine);
            };
            switch (peek()) {
                case '(': take(); add(Kind::LParen, {}, atLine); break;
                case ')': take(); add(Kind::RParen, {}, atLine); break;
                case '[': take(); add(Kind::LBracket, {}, atLine); break;
                case ']': take(); add(Kind::RBracket, {}, atLine); break;
                case ',': take(); add(Kind::Comma, {}, atLine); break;
                case ':': take(); add(Kind::Colon, {}, atLine); break;
                case '.': take(); add(Kind::Dot, {}, atLine); break;
                case ';': take(); add(Kind::Semicolon, {}, atLine); break;
                case '+': singleOrDouble('=', Kind::PlusAssign, Kind::Plus); break;
                case '-':
                    take();
                    if (peek() == '>') { take(); add(Kind::Arrow, {}, atLine); }
                    else if (peek() == '=') { take(); add(Kind::MinusAssign, {}, atLine); }
                    else add(Kind::Minus, {}, atLine);
                    break;
                case '*': singleOrDouble('=', Kind::StarAssign, Kind::Star); break;
                case '/': singleOrDouble('=', Kind::SlashAssign, Kind::Slash); break;
                case '%': singleOrDouble('=', Kind::PercentAssign, Kind::Percent); break;
                case '=': singleOrDouble('=', Kind::Equal, Kind::Assign); break;
                case '!': singleOrDouble('=', Kind::NotEqual, Kind::Bang); break;
                case '>':
                    take();
                    if (peek() == '=') { take(); add(Kind::GreaterEqual, {}, atLine); }
                    else if (peek() == '>') { take(); if (peek() == '=') { take(); add(Kind::ShiftRightAssign, {}, atLine); } else add(Kind::ShiftRight, {}, atLine); }
                    else add(Kind::Greater, {}, atLine);
                    break;
                case '<':
                    take();
                    if (peek() == '=') { take(); add(Kind::LessEqual, {}, atLine); }
                    else if (peek() == '<') { take(); if (peek() == '=') { take(); add(Kind::ShiftLeftAssign, {}, atLine); } else add(Kind::ShiftLeft, {}, atLine); }
                    else add(Kind::Less, {}, atLine);
                    break;
                case '&':
                    if (two('&', '&')) { take(); take(); add(Kind::AndAnd, {}, atLine); }
                    else { take(); if (peek() == '=') { take(); add(Kind::BitAndAssign, {}, atLine); } else add(Kind::BitAnd, {}, atLine); }
                    break;
                case '|':
                    if (two('|', '|')) { take(); take(); add(Kind::OrOr, {}, atLine); }
                    else { take(); if (peek() == '=') { take(); add(Kind::BitOrAssign, {}, atLine); } else add(Kind::BitOr, {}, atLine); }
                    break;
                case '^': take(); if (peek() == '=') { take(); add(Kind::BitXorAssign, {}, atLine); } else add(Kind::BitXor, {}, atLine); break;
                case '~': take(); add(Kind::Tilde, {}, atLine); break;
                default: throw CompileError(lineMessage(atLine, std::string("Taninmayan karakter: '") + static_cast<char>(c) + "'."));
            }
        }
        if (!tokens.empty() && tokens.back().kind != Kind::Newline) add(Kind::Newline, {}, line);
        while (indentation.size() > 1) {
            indentation.pop_back();
            add(Kind::Dedent, {}, line);
        }
        tokens.push_back(Token{Kind::End, {}, line});
        return tokens;
    }
};

static std::string cppIdentifier(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size();) {
        if (i + 1 < text.size()) {
            const unsigned char a = static_cast<unsigned char>(text[i]);
            const unsigned char b = static_cast<unsigned char>(text[i + 1]);
            if (a == 0xC3 || a == 0xC4 || a == 0xC5) {
                char replacement = 0;
                if (a == 0xC3) {
                    if (b == 0xA7) replacement = 'c';       // ç
                    else if (b == 0x87) replacement = 'C';  // Ç
                    else if (b == 0xB6) replacement = 'o';  // ö
                    else if (b == 0x96) replacement = 'O';  // Ö
                    else if (b == 0xBC) replacement = 'u';  // ü
                    else if (b == 0x9C) replacement = 'U';  // Ü
                } else if (a == 0xC4) {
                    if (b == 0x9F) replacement = 'g';       // ğ
                    else if (b == 0x9E) replacement = 'G';  // Ğ
                    else if (b == 0xB1) replacement = 'i';  // ı
                    else if (b == 0xB0) replacement = 'I';  // İ
                } else {
                    if (b == 0x9F) replacement = 's';       // ş
                    else if (b == 0x9E) replacement = 'S';  // Ş
                }
                if (replacement) {
                    out.push_back(replacement);
                    i += 2;
                    continue;
                }
            }
        }
        out.push_back(text[i++]);
    }
    return out;
}

static std::string canonicalWord(const std::string& text) {
    std::string result = cppIdentifier(text);
    for (char& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::unordered_map<std::string, std::string> aliases = {
        {"yapilandir", "configure"}, {"configure", "configure"},
        {"modul", "module"}, {"module", "module"},
        {"surec", "process"}, {"process", "process"},
        {"eylem", "action"}, {"action", "action"},
        {"ana_akis", "main_flow"}, {"main_flow", "main_flow"},
        {"her", "each"}, {"each", "each"},
        {"icin", "in"}, {"in", "in"},
        {"yeni", "new"}, {"new", "new"},
        {"akislistesi", "flowlist"}, {"flowlist", "flowlist"}, {"flow_list", "flowlist"},
        {"list", "list"},
        {"metin", "string"}, {"yazi", "string"}, {"string", "string"},
        {"sayi", "int"}, {"number", "int"}, {"integer", "int"}, {"int", "int"},
        {"mantik", "bool"}, {"boolean", "bool"}, {"bool", "bool"},
        {"dogru", "true"}, {"true", "true"}, {"yanlis", "false"}, {"false", "false"},
        {"eger", "if"}, {"if", "if"}, {"degilse", "else"}, {"else", "else"},
        {"yoksaeger", "elif"}, {"elif", "elif"},
        {"dondur", "return"}, {"return", "return"},
        {"sabit", "const"}, {"const", "const"},
        {"ve", "and"}, {"and", "and"}, {"veya", "or"}, {"or", "or"},
        {"degil", "not"}, {"not", "not"},
        {"sinif", "class"}, {"class", "class"},
        {"islev", "fn"}, {"fonksiyon", "fn"}, {"fn", "fn"},
        {"dongu", "while"}, {"while", "while"},
        {"herkes", "each"},
        {"kes", "break"}, {"break", "break"},
        {"surdur", "continue"}, {"continue", "continue"},
        {"aktar", "import"}, {"import", "import"},
        {"olarak", "as"}, {"as", "as"},
        {"auto", "auto"}, {"float", "float"}, {"double", "double"},
        {"long", "long"}, {"short", "short"},
        {"isaretsiz", "uint"}, {"tamsayi", "int"}
    };
    auto found = aliases.find(result);
    return found == aliases.end() ? result : found->second;
}

// The native backend lowers PRCYTERAS expressions and control flow directly to
// LLVM IR. The fixed runtime is linked as a library; no per-program C++ source is
// emitted by this backend.
enum class NativeType { Dynamic, Int, Float, Bool, Function };

struct NativeFunctionSignature {
    std::vector<NativeType> parameterTypes;
    std::vector<int> parameterBits;
    std::vector<bool> parameterUnsigned;
    std::vector<std::string> callbackParameterSignatures;
    NativeType resultType = NativeType::Dynamic;
    int resultBits = 64;
    bool resultUnsigned = false;
};

struct NativeStructFieldSlot {
    std::string slot;
    NativeType type = NativeType::Dynamic;
    int bits = 64;
    bool isUnsigned = false;
};

struct NativeFixedArray {
    std::string slot;
    NativeType elementType = NativeType::Dynamic;
    int elementBits = 64;
    bool elementUnsigned = false;
    std::size_t length = 0;
};

struct NativeStructInstance {
    std::string typeName;
    std::unordered_map<std::string, NativeStructFieldSlot> fields;
    bool temporary = true;
};

struct NativeExpr {
    enum class Kind { Value, Variable, Namespace, Qualified, ClassType, StructType, StructValue, ListType, Member, Index } kind = Kind::Value;
    std::string value;
    std::string name;
    std::string owner;
    NativeType type = NativeType::Dynamic;
    int bits = 64;
    bool isUnsigned = false;
    int fieldIndex = -1;
    std::string staticClass;
    std::string nativeData;
    std::shared_ptr<NativeStructInstance> structInstance;
    NativeFunctionSignature functionSignature;
    bool hasFunctionSignature = false;
    std::string baseName;
};

struct NativeStructFieldDefinition {
    std::string name;
    NativeType type = NativeType::Dynamic;
    int bits = 64;
    bool isUnsigned = false;
    std::string defaultValue = "0";
};

struct NativeFieldType {
    NativeType type = NativeType::Dynamic;
    int bits = 64;
    bool isUnsigned = false;
};

static std::string nativeTypeName(const std::string& type) {
    const std::string canonical = canonicalWord(type);
    if (canonical == "list") return "List";
    if (canonical == "flowlist") return "FlowList";
    return cppIdentifier(type);
}

struct IntegerTypeInfo {
    bool valid = false;
    bool isUnsigned = false;
    int bits = 64;
};

static IntegerTypeInfo integerTypeInfo(const std::string& type) {
    const std::string name = canonicalWord(type);
    if (name == "int" || name == "long" || name == "sint" || name == "int64" || name == "i64") return {true, false, 64};
    if (name == "short" || name == "int16" || name == "i16") return {true, false, 16};
    if (name == "uint" || name == "uint64" || name == "u64") return {true, true, 64};
    if (name == "byte" || name == "uint8" || name == "u8") return {true, true, 8};
    if (name == "word" || name == "uint16" || name == "u16") return {true, true, 16};
    if (name == "dword" || name == "uint32" || name == "u32") return {true, true, 32};
    if (name == "qword") return {true, true, 64};

    std::size_t prefix = 0;
    bool isUnsigned = false;
    if (name.starts_with("unsigned")) { prefix = 8; isUnsigned = true; }
    else if (name.starts_with("isaretsiz")) { prefix = 9; isUnsigned = true; }
    else if (name.starts_with("uint")) { prefix = 4; isUnsigned = true; }
    else if (name.starts_with("signed")) prefix = 6;
    else if (name.starts_with("tamsayi")) prefix = 7;
    else if (name.starts_with("sint")) prefix = 4;
    else if (name.starts_with("int")) prefix = 3;
    else if (name.starts_with("u")) { prefix = 1; isUnsigned = true; }
    else if (name.starts_with("i")) prefix = 1;
    else return {};
    if (prefix >= name.size()) return {};
    int bits = 0;
    for (std::size_t i = prefix; i < name.size(); ++i) {
        if (name[i] < '0' || name[i] > '9') return {};
        bits = bits * 10 + (name[i] - '0');
        if (bits > 64) return {};
    }
    if (bits < 4 || bits > 64) return {};
    return {true, isUnsigned, bits};
}

static bool integerWidthCandidate(const std::string& type) {
    const std::string name = canonicalWord(type);
    static constexpr std::string_view prefixes[] = {
        "unsigned", "isaretsiz", "signed", "tamsayi", "uint", "sint", "int", "u", "i"
    };
    for (const auto prefix : prefixes) {
        if (!name.starts_with(prefix) || name.size() == prefix.size()) continue;
        bool digits = true;
        for (std::size_t i = prefix.size(); i < name.size(); ++i)
            if (name[i] < '0' || name[i] > '9') { digits = false; break; }
        if (digits) return true;
    }
    return false;
}

class NativeIRParser {
    struct LocalValue {
        std::string slot;
        NativeType type = NativeType::Dynamic;
        int bits = 64;
        bool isUnsigned = false;
        bool pinnedBox = false;
        NativeFunctionSignature functionSignature;
        bool hasFunctionSignature = false;
    };
    struct Parameter {
        std::string name;
        NativeType type = NativeType::Dynamic;
        int bits = 64;
        bool isUnsigned = false;
        NativeFunctionSignature callbackSignature;
        bool hasCallbackSignature = false;
    };
    struct DirectNativeFunction {
        std::string symbol;
        std::vector<Parameter> params;
        NativeType resultType = NativeType::Dynamic;
        int resultBits = 64;
        bool resultUnsigned = false;
        bool needsParentEnvironment = true;
    };
    struct FunctionContext {
        struct LoopTarget {
            std::string breakLabel;
            std::string continueLabel;
            std::string metadata;
        };
        std::ostringstream body;
        std::ostringstream allocas;
        int temporary = 0;
        int label = 0;
        int local = 0;
        bool terminated = false;
        bool directNative = false;
        NativeType returnType = NativeType::Dynamic;
        int returnBits = 64;
        bool returnUnsigned = false;
        std::string env = "%env";
        std::vector<LoopTarget> loops;
        std::vector<std::string> pinnedBoxSlots;
        std::unordered_map<std::string, LocalValue> locals;
        std::unordered_set<std::string> dynamicLocals;
        std::unordered_set<std::string> constBindings;
        std::unordered_map<std::string, std::string> staticObjectTypes;
        std::unordered_map<std::string, std::string> nativeObjectDataLocals;
        std::unordered_map<std::string, std::shared_ptr<NativeStructInstance>> nativeStructLocals;
        std::unordered_map<std::string, NativeFixedArray> fixedArrays;
        std::string temp() { return "%v" + std::to_string(temporary++); }
        std::string newLabel(const char* prefix) { return std::string(prefix) + std::to_string(label++); }
        bool isConstLocal(const std::string& name) const { return constBindings.contains(name); }
        void line(const std::string& text) { body << "  " << text << '\n'; }
        void start(const std::string& name) { body << name << ":\n"; terminated = false; }
        void branch(const std::string& name) {
            if (terminated) return;
            std::string metadata;
            if (!loops.empty() && loops.back().continueLabel == name)
                metadata = loops.back().metadata;
            line("br label %" + name + (metadata.empty() ? "" : ", !llvm.loop " + metadata));
            terminated = true;
        }
        std::string addLocal(const std::string& name, NativeType type, int bits, bool isUnsigned, const std::string& llvmType, bool pinnedBox = false) {
            auto found = locals.find(name);
            if (found != locals.end()) return found->second.slot;
            std::string slot = "%local" + std::to_string(local++);
            allocas << "  " << slot << " = alloca " << llvmType << '\n';
            locals.emplace(name, LocalValue{slot, type, bits, isUnsigned, pinnedBox});
            if (pinnedBox) pinnedBoxSlots.push_back(slot);
            return slot;
        }
        void releasePinnedBoxes() {
            for (const std::string& slot : pinnedBoxSlots) {
                const std::string value = temp();
                line(value + " = load i8*, i8** " + slot);
                line("call void @pts_box_unpin(i8* " + value + ")");
            }
        }
        std::string addPointerDataLocal(const std::string& name) {
            const std::string slot="%local"+std::to_string(local++);
            allocas<<"  "<<slot<<" = alloca i8*\n";
            nativeObjectDataLocals[name]=slot;
            return slot;
        }
        std::string bodyWithAllocas() const {
            std::string text = body.str();
            const size_t firstLine = text.find('\n');
            if (firstLine != std::string::npos) text.insert(firstLine + 1, allocas.str());
            return text;
        }
    };
    std::vector<Token> tokens;
    fs::path sourcePath;
    size_t pos = 0;
    std::ostringstream strings;
    std::ostringstream functionsIR;
    std::ostringstream initBody;
    std::ostringstream registrations;
    std::ostringstream loopMetadata;
    std::unordered_map<std::string, std::string> aliases;
    // Resolved same-module calls go straight to their LLVM function symbol.
    // This avoids the runtime string/hash dispatcher and lets LLVM inline them.
    std::unordered_map<std::string, std::string> directFunctionSymbols;
    std::unordered_map<std::string, NativeFunctionSignature> functionSignatures;
    std::unordered_set<std::string> globalNames;
    std::unordered_set<std::string> globalConstNames;
    // Primitive signatures use a native LLVM ABI at statically-resolved call sites.
    std::unordered_map<std::string, DirectNativeFunction> directNativeFunctions;
    std::unordered_map<std::string, bool> functionNeedsEnvironment;
    std::unordered_map<std::string, NativeExpr> configuredScalars;
    std::vector<std::pair<std::string, std::string>> functionRegistrationCandidates;
    std::unordered_set<std::string> dynamicFunctionCalls;
    std::unordered_set<std::string> usedStdlibModules;
    std::unordered_set<std::string> namespaceRoots = baseNamespaceRoots();
    std::unordered_set<std::string> classNames;
    std::unordered_set<std::string> structNames;
    std::unordered_map<std::string, std::vector<NativeStructFieldDefinition>> structDefinitions;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> classFieldIndices;
    std::unordered_map<std::string, std::unordered_map<std::string, NativeFieldType>> classFieldTypes;
    std::unordered_set<std::wstring> importedModules;
    std::string modulePrefix;
    std::optional<NativeFunctionSignature> parsedFunctionTypeSignature;
    std::string currentClass;
    FunctionContext* current = nullptr;
    FunctionContext initFunction;
    bool releaseOverflowUndefined = false;
    bool releaseOptimizations = false;
    unsigned nextLoopMetadataId = 0;
    int stringIndex = 0;
    int functionIndex = 0;
    std::string mainSymbol;
    bool mainNeedsEnvironment = true;
    std::string errorMessage;

    const Token& token() const { return tokens[pos]; }
    const Token& look(size_t n = 1) const { return tokens[std::min(pos + n, tokens.size() - 1)]; }
    bool is(Kind kind) const { return token().kind == kind; }
    bool word(const char* name) const { return token().kind == Kind::Identifier && canonicalWord(token().text) == name; }
    Token take() { Token out = token(); if (!is(Kind::End)) ++pos; return out; }
    bool match(Kind kind) { if (!is(kind)) return false; take(); return true; }
    bool matchWord(const char* name) { if (!word(name)) return false; take(); return true; }
    Token expect(Kind kind, const std::string& message) {
        if (!is(kind)) throw CompileError(lineMessage(token().line, message));
        return take();
    }
    Token expectName(const std::string& message) { return expect(Kind::Identifier, message); }
    std::optional<std::int64_t> integerConstant(const NativeExpr& expr) const {
        if (expr.kind != NativeExpr::Kind::Value || expr.type != NativeType::Int || expr.value.empty()) return std::nullopt;
        std::int64_t value = 0;
        const auto parsed = std::from_chars(expr.value.data(), expr.value.data() + expr.value.size(), value, 10);
        if (parsed.ec != std::errc{} || parsed.ptr != expr.value.data() + expr.value.size()) return std::nullopt;
        return value;
    }
    // ben yaptım opencode - 2026-11-11 - sabit dizi baslatma ve const korumasi
    std::string fixedArrayIntegerConstant(const std::string& text, bool negative, int bits, bool isUnsigned, int line, std::size_t index) {
        std::uint64_t magnitude = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), magnitude, 10);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
            throw CompileError(lineMessage(line, "Sabit dizi baslaticisinda gecersiz tamsayi (eleman " + std::to_string(index + 1) + ")."));
        if (negative && isUnsigned)
            throw CompileError(lineMessage(line, "Isaretsiz sabit dizi elemanina negatif deger atanamaz (eleman " + std::to_string(index + 1) + ")."));
        const unsigned width = bits > 0 ? static_cast<unsigned>(bits) : 64u;
        if (width < 64) {
            const std::uint64_t limit = 1ULL << width;
            const std::uint64_t maxValue = isUnsigned ? (limit - 1) : (limit / 2 - 1);
            const std::uint64_t maxMagnitude = isUnsigned ? maxValue : (limit / 2);
            if (negative ? magnitude > maxMagnitude : magnitude > maxValue)
                throw CompileError(lineMessage(line, "Sabit dizi elemani " + std::to_string(index + 1) + " " + std::to_string(bits) + " bitlik turun sinirini asiyor."));
        } else {
            const std::uint64_t maxMagnitude = 1ULL << 63;
            const std::uint64_t maxValue = maxMagnitude - 1;
            if (negative ? magnitude > maxMagnitude : (!isUnsigned && magnitude > maxValue))
                throw CompileError(lineMessage(line, "Sabit dizi elemani " + std::to_string(index + 1) + " 64 bitlik turun sinirini asiyor."));
        }
        if (magnitude == 0) return "0";
        return negative ? "-" + std::to_string(magnitude) : std::to_string(magnitude);
    }
    // ben yaptım opencode - 2026-11-11 - sabit dizi baslatma ve const korumasi
    // ben yaptım codex - REJECT: '+' ile yazilan bool baslaticisi reddedilmiyor; isaretli bool degerleri gecersiz olmali.
    std::string fixedArrayInitializerElement(NativeType elementType, int bits, bool isUnsigned, std::size_t index) {
        const int elementLine = token().line;
        bool signedValue = false;
        bool negative = false;
        if (match(Kind::Minus)) { negative = true; signedValue = true; }
        else if (match(Kind::Plus)) signedValue = true;
        if (is(Kind::LBracket))
            throw CompileError(lineMessage(elementLine, "Ici ice dizi baslaticilari bu dilimde desteklenmiyor (eleman " + std::to_string(index + 1) + ")."));
        if (is(Kind::Number)) {
            const Token number = take();
            const bool floatLiteral = number.text.find('.') != std::string::npos;
            if (elementType == NativeType::Int) {
                if (floatLiteral)
                    throw CompileError(lineMessage(number.line, "Tamsayi sabit diziye ondalikli deger atanamaz (eleman " + std::to_string(index + 1) + ")."));
                return fixedArrayIntegerConstant(number.text, negative, bits, isUnsigned, number.line, index);
            }
            if (elementType == NativeType::Float) {
                std::string value = number.text;
                if (value.front() == '.') value = "0" + value;
                if (value.back() == '.') value += "0";
                if (!floatLiteral) value += ".0";
                return negative ? "-" + value : value;
            }
            throw CompileError(lineMessage(number.line, "bool sabit diziye sayisal deger atanamaz (eleman " + std::to_string(index + 1) + ")."));
        }
        if (word("true") || word("false")) {
            if (elementType != NativeType::Bool)
                throw CompileError(lineMessage(token().line, "bool olmayan sabit diziye true/false atanamaz (eleman " + std::to_string(index + 1) + ")."));
            if (signedValue)
                throw CompileError(lineMessage(token().line, "bool sabit dizi elemaninda isaret kullanilamaz (eleman " + std::to_string(index + 1) + ")."));
            const bool value = word("true");
            take();
            return value ? "true" : "false";
        }
        if (is(Kind::Identifier))
            throw CompileError(lineMessage(token().line, "Sabit dizi baslaticisi yalnizca derleme zamaninda sabit scalar degerler icerebilir (eleman " + std::to_string(index + 1) + ")."));
        throw CompileError(lineMessage(token().line, "Sabit dizi baslaticisinda gecersiz eleman (eleman " + std::to_string(index + 1) + ")."));
    }
    void skipLines() { while (is(Kind::Newline) || is(Kind::Semicolon)) take(); }
    void endLine() {
        while (match(Kind::Semicolon)) {}
        if (match(Kind::Newline) || is(Kind::Dedent) || is(Kind::End)) return;
        throw CompileError(lineMessage(token().line, "Komut satir sonunda bitmeli; beklenmeyen ifade: '" + token().text + "' (token " + std::to_string(static_cast<int>(token().kind)) + ")."));
    }
    std::string llvmString(const std::string& text) {
        const std::string label = "@.pts.str." + std::to_string(stringIndex++);
        strings << label << " = private unnamed_addr constant [" << (text.size() + 1) << " x i8] c\"";
        static const char hex[] = "0123456789ABCDEF";
        for (unsigned char c : text) { strings << '\\' << hex[c >> 4] << hex[c & 15]; }
        strings << "\\00\", align 1\n";
        return "getelementptr inbounds ([" + std::to_string(text.size() + 1) + " x i8], [" +
               std::to_string(text.size() + 1) + " x i8]* " + label + ", i64 0, i64 0)";
    }
    std::string call(const std::string& function, const std::string& args) {
        std::string result = current->temp();
        current->line(result + " = call i8* @" + function + "(" + args + ")");
        return result;
    }
    void callVoid(const std::string& function, const std::string& args) {
        current->line("call void @" + function + "(" + args + ")");
    }
    std::string cstrArg(const std::string& text) { return "i8* " + llvmString(text); }
    std::string intValue(std::int64_t value) { return call("pts_int", "i64 " + std::to_string(value)); }
    std::string boolValue(bool value) { return call("pts_bool", std::string("i1 ") + (value ? "true" : "false")); }
    std::string stringValue(const std::string& value) { return call("pts_string", cstrArg(value)); }
    static std::string llvmType(NativeType type, int bits = 64) {
        switch (type) {
            case NativeType::Int: return "i" + std::to_string(bits);
            case NativeType::Float: return "double";
            case NativeType::Bool: return "i1";
            case NativeType::Function: return "i8*";
            default: return "i8*";
        }
    }
    static NativeType valueType(const std::string& type) {
        const std::string canonical = canonicalWord(type);
        if (integerTypeInfo(canonical).valid) return NativeType::Int;
        if (canonical == "float" || canonical == "double") return NativeType::Float;
        if (canonical == "bool") return NativeType::Bool;
        if (canonical == "function") return NativeType::Function;
        return NativeType::Dynamic;
    }
    static std::string functionSignatureKey(const NativeFunctionSignature& signature) {
        std::string key="(";
        for(std::size_t i=0;i<signature.parameterTypes.size();++i) {
            if(i)key.push_back(';');
            const NativeType type=signature.parameterTypes[i];
            key+=std::to_string(static_cast<int>(type));
            key.push_back(':');
            const int bits=type==NativeType::Int
                ? (i<signature.parameterBits.size()?signature.parameterBits[i]:64)
                : type==NativeType::Bool?1:64;
            key+=std::to_string(bits);
            key.push_back(':');
            key+=(type==NativeType::Int&&i<signature.parameterUnsigned.size()&&signature.parameterUnsigned[i])?'u':'s';
            key.push_back(':');
            if(i<signature.callbackParameterSignatures.size())key+=signature.callbackParameterSignatures[i];
        }
        key+=")->";
        key+=std::to_string(static_cast<int>(signature.resultType));
        key.push_back(':');
        const int resultBits=signature.resultType==NativeType::Int?signature.resultBits:
            signature.resultType==NativeType::Bool?1:64;
        key+=std::to_string(resultBits);
        key.push_back(':');
        key+=(signature.resultType==NativeType::Int&&signature.resultUnsigned)?'u':'s';
        return key;
    }
    static NativeFunctionSignature signatureOf(const std::vector<Parameter>& params,
        NativeType resultType=NativeType::Dynamic,int resultBits=64,bool resultUnsigned=false) {
        NativeFunctionSignature signature;
        signature.resultType=resultType;
        signature.resultBits=resultBits;
        signature.resultUnsigned=resultUnsigned;
        for(const auto& parameter:params) {
            signature.parameterTypes.push_back(parameter.type);
            signature.parameterBits.push_back(parameter.bits);
            signature.parameterUnsigned.push_back(parameter.isUnsigned);
            signature.callbackParameterSignatures.push_back(parameter.type==NativeType::Function&&parameter.hasCallbackSignature
                ? functionSignatureKey(parameter.callbackSignature):std::string{});
        }
        return signature;
    }
    std::string scalarCall(const std::string& resultType, const std::string& function, const std::string& args) {
        std::string result = current->temp();
        current->line(result + " = call " + resultType + " @" + function + "(" + args + ")");
        return result;
    }
    NativeExpr materialize(NativeExpr expr) {
        if (expr.kind == NativeExpr::Kind::StructValue ||
            (expr.kind == NativeExpr::Kind::Variable && expr.structInstance)) return expr;
        if (expr.kind == NativeExpr::Kind::Variable) {
            if(current->fixedArrays.contains(expr.name))
                throw CompileError(lineMessage(token().line,"Sabit dizi yalnizca indislenerek kullanilabilir: "+expr.name));
            auto local = current->locals.find(expr.name);
            if (local != current->locals.end()) {
                const std::string value = current->temp();
                const std::string type = llvmType(local->second.type, local->second.bits);
                current->line(value + " = load " + type + ", " + type + "* " + local->second.slot);
                NativeExpr result{NativeExpr::Kind::Value, value, {}, {}, local->second.type, local->second.bits, local->second.isUnsigned};
                if(local->second.type==NativeType::Function&&local->second.hasFunctionSignature) {
                    result.functionSignature=local->second.functionSignature;
                    result.hasFunctionSignature=true;
                }
                return result;
            }
            if(!current->dynamicLocals.contains(expr.name)&&!globalNames.contains(expr.name)) {
                std::string functionName=expr.name;
                if(const auto alias=aliases.find(functionName);alias!=aliases.end())functionName=alias->second;
                if(!modulePrefix.empty()&&functionName.find('.')==std::string::npos)functionName=modulePrefix+"."+functionName;
                const auto function=directFunctionSymbols.find(functionName);
                if(function!=directFunctionSymbols.end()) {
                    NativeExpr result{NativeExpr::Kind::Value,functionPointer(function->second),{}, {},NativeType::Function};
                    if(const auto signature=functionSignatures.find(functionName);signature!=functionSignatures.end()) {
                        result.functionSignature=signature->second;
                        result.hasFunctionSignature=true;
                    }
                    return result;
                }
            }
            NativeExpr value{NativeExpr::Kind::Value, call("pts_env_get", "i8* " + current->env + ", " + cstrArg(expr.name)), {}, {}};value.staticClass=expr.staticClass;return value;
        }
        if(expr.kind==NativeExpr::Kind::Qualified) {
            const auto function=directFunctionSymbols.find(expr.name);
            if(function!=directFunctionSymbols.end()) {
                NativeExpr result{NativeExpr::Kind::Value,functionPointer(function->second),{}, {},NativeType::Function};
                if(const auto signature=functionSignatures.find(expr.name);signature!=functionSignatures.end()) {
                    result.functionSignature=signature->second;
                    result.hasFunctionSignature=true;
                }
                return result;
            }
        }
        if (expr.kind == NativeExpr::Kind::Member) {
            if (expr.fieldIndex == -2 || expr.fieldIndex == -3) {
                const std::string type = llvmType(expr.type, expr.bits);
                const std::string value = current->temp();
                current->line(value + " = load " + type + ", " + type + "* " + expr.owner);
                return NativeExpr{NativeExpr::Kind::Value, value, {}, {}, expr.type, expr.bits, expr.isUnsigned};
            }
            if (expr.fieldIndex >= 0 && expr.type == NativeType::Int) {
                const std::string value = scalarCall("i64", expr.isUnsigned ? "pts_get_member_u64" : "pts_get_member_i64",
                    "i8* " + expr.owner + ", i64 " + std::to_string(expr.fieldIndex));
                if (expr.bits == 64) return NativeExpr{NativeExpr::Kind::Value, value, {}, {}, NativeType::Int, 64, expr.isUnsigned};
                const std::string narrowed = current->temp();
                current->line(narrowed + " = trunc i64 " + value + " to " + llvmType(NativeType::Int, expr.bits));
                return NativeExpr{NativeExpr::Kind::Value, narrowed, {}, {}, NativeType::Int, expr.bits, expr.isUnsigned};
            }
            if (expr.fieldIndex >= 0 && expr.type == NativeType::Float)
                return NativeExpr{NativeExpr::Kind::Value, scalarCall("double", "pts_get_member_f64", "i8* " + expr.owner + ", i64 " + std::to_string(expr.fieldIndex)), {}, {}, NativeType::Float};
            if (expr.fieldIndex >= 0 && expr.type == NativeType::Bool)
                return NativeExpr{NativeExpr::Kind::Value, scalarCall("i1", "pts_get_member_bool", "i8* " + expr.owner + ", i64 " + std::to_string(expr.fieldIndex)), {}, {}, NativeType::Bool};
            const std::string getter = expr.fieldIndex >= 0 ? "pts_get_member_index" : "pts_get_member";
            const std::string arguments = expr.fieldIndex >= 0
                ? "i8* " + expr.owner + ", i64 " + std::to_string(expr.fieldIndex)
                : "i8* " + expr.owner + ", " + cstrArg(expr.name);
            return NativeExpr{NativeExpr::Kind::Value, call(getter, arguments), {}, {}};
        }
        if (expr.kind == NativeExpr::Kind::Index)
            return NativeExpr{NativeExpr::Kind::Value, call("pts_index_get", "i8* " + expr.owner + ", i8* " + expr.name), {}, {}};
        return expr;
    }
    std::string valueOf(NativeExpr expr) {
        expr = materialize(std::move(expr));
        if (expr.kind != NativeExpr::Kind::Value) throw CompileError(lineMessage(token().line, "Bu ad bir deger olarak kullanilamiyor."));
        if (expr.type == NativeType::Int) {
            std::string value = expr.value;
            if (expr.bits < 64) {
                value = current->temp();
                current->line(value + " = " + std::string(expr.isUnsigned ? "zext " : "sext ") + llvmType(NativeType::Int, expr.bits) + " " + expr.value + " to i64");
            }
            return call(expr.isUnsigned ? "pts_uint" : "pts_int", "i64 " + value);
        }
        if (expr.type == NativeType::Float) return call("pts_float", "double " + expr.value);
        if (expr.type == NativeType::Bool) return call("pts_bool", "i1 " + expr.value);
        return expr.value;
    }
    std::string convertValue(NativeExpr expr, NativeType target, int targetBits = 64, bool targetUnsigned = false) {
        expr = materialize(std::move(expr));
        if (expr.kind != NativeExpr::Kind::Value) throw CompileError(lineMessage(token().line, "Tur donusumu icin bir deger bekleniyordu."));
        if (expr.type == NativeType::Dynamic) {
            const std::string boxed = expr.value;
            if (target == NativeType::Int) {
                const std::string value = scalarCall("i64", targetUnsigned ? "pts_as_uint" : "pts_as_int", "i8* " + boxed);
                NativeExpr converted{NativeExpr::Kind::Value, value, {}, {}, NativeType::Int, 64, targetUnsigned};
                return convertValue(converted, NativeType::Int, targetBits, targetUnsigned);
            }
            if (target == NativeType::Float) return scalarCall("double", "pts_as_float", "i8* " + boxed);
            if (target == NativeType::Bool) return scalarCall("i1", "pts_as_bool", "i8* " + boxed);
        }
        if (expr.type == target) {
            if (target != NativeType::Int || (expr.bits == targetBits && expr.isUnsigned == targetUnsigned)) return expr.value;
            const std::string sourceType = llvmType(NativeType::Int, expr.bits);
            const std::string destinationType = llvmType(NativeType::Int, targetBits);
            const std::string result = current->temp();
            if (expr.bits < targetBits)
                current->line(result + " = " + std::string(expr.isUnsigned ? "zext " : "sext ") + sourceType + " " + expr.value + " to " + destinationType);
            else if (expr.bits > targetBits)
                current->line(result + " = trunc " + sourceType + " " + expr.value + " to " + destinationType);
            else return expr.value;
            return result;
        }
        const std::string result = current->temp();
        if (target == NativeType::Float && expr.type == NativeType::Int)
            current->line(result + " = " + std::string(expr.isUnsigned ? "uitofp " : "sitofp ") + llvmType(NativeType::Int, expr.bits) + " " + expr.value + " to double");
        else if (target == NativeType::Float && expr.type == NativeType::Bool)
            current->line(result + " = uitofp i1 " + expr.value + " to double");
        else if (target == NativeType::Int && expr.type == NativeType::Bool)
            current->line(result + " = zext i1 " + expr.value + " to " + llvmType(NativeType::Int, targetBits));
        else if (target == NativeType::Int && expr.type == NativeType::Float)
            current->line(result + " = " + std::string(targetUnsigned ? "fptoui " : "fptosi ") + "double " + expr.value + " to " + llvmType(NativeType::Int, targetBits));
        else if (target == NativeType::Bool && expr.type == NativeType::Int)
            current->line(result + " = icmp ne " + llvmType(NativeType::Int, expr.bits) + " " + expr.value + ", 0");
        else if (target == NativeType::Bool && expr.type == NativeType::Float)
            current->line(result + " = fcmp une double " + expr.value + ", 0.0");
        else return valueOf(expr);
        return result;
    }
    std::string truthValue(NativeExpr expr) {
        expr = materialize(std::move(expr));
        if (expr.kind != NativeExpr::Kind::Value) throw CompileError(lineMessage(token().line, "Kosul icin bir deger bekleniyordu."));
        if (expr.type == NativeType::Bool) return expr.value;
        if (expr.type == NativeType::Int) {
            const std::string result = current->temp();
            current->line(result + " = icmp ne " + llvmType(NativeType::Int, expr.bits) + " " + expr.value + ", 0");
            return result;
        }
        if (expr.type == NativeType::Float) {
            const std::string result = current->temp();
            current->line(result + " = fcmp une double " + expr.value + ", 0.0");
            return result;
        }
        return scalarCall("i1", "pts_truthy", "i8* " + expr.value);
    }
    NativeExpr binaryValue(const std::string& op, NativeExpr left, NativeExpr right) {
        left = materialize(std::move(left));
        right = materialize(std::move(right));
        const bool leftInteger = left.type == NativeType::Int;
        const bool rightInteger = right.type == NativeType::Int;
        const bool leftNumeric = leftInteger || left.type == NativeType::Float;
        const bool rightNumeric = rightInteger || right.type == NativeType::Float;
        if (left.kind == NativeExpr::Kind::Value && right.kind == NativeExpr::Kind::Value && leftNumeric && rightNumeric) {
            const bool floating = left.type == NativeType::Float || right.type == NativeType::Float;
            if (floating) {
                const std::string a = convertValue(left, NativeType::Float), b = convertValue(right, NativeType::Float), result = current->temp();
                if (op == "+" || op == "-" || op == "*" || op == "/") {
                    const char* instruction = op == "+" ? "fadd" : op == "-" ? "fsub" : op == "*" ? "fmul" : "fdiv";
                    const bool allowContraction = releaseOverflowUndefined && op != "/";
                    current->line(result + " = " + instruction +
                        (releaseOptimizations ? " fast" : allowContraction ? " contract" : "") +
                        " double " + a + ", " + b);
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Float};
                }
                if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
                    const char* predicate = op == "==" ? "oeq" : op == "!=" ? "une" : op == "<" ? "olt" : op == "<=" ? "ole" : op == ">" ? "ogt" : "oge";
                    current->line(result + " = fcmp " + std::string(releaseOptimizations ? "fast " : "") +
                        predicate + " double " + a + ", " + b);
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Bool};
                }
            } else if (leftInteger && rightInteger) {
                const int bits = std::max(left.bits, right.bits);
                const bool isUnsigned = left.isUnsigned == right.isUnsigned ? left.isUnsigned :
                    (left.isUnsigned ? left.bits >= right.bits : right.bits >= left.bits);
                const std::string type = llvmType(NativeType::Int, bits);
                const std::string a = convertValue(left, NativeType::Int, bits, isUnsigned);
                const std::string b = convertValue(right, NativeType::Int, bits, isUnsigned);
                const std::string result = current->temp();
                if (op == "+" || op == "-" || op == "*" || op == "&" || op == "|" || op == "^") {
                    std::string instruction = op == "+" ? "add" : op == "-" ? "sub" : op == "*" ? "mul" : op == "&" ? "and" : op == "|" ? "or" : "xor";
                    if (releaseOverflowUndefined && !isUnsigned && (op == "+" || op == "-" || op == "*")) instruction += " nsw";
                    current->line(result + " = " + instruction + " " + type + " " + a + ", " + b);
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Int, bits, isUnsigned};
                }
                if (op == "<<" || op == ">>") {
                    const std::string shiftCount = current->temp();
                    current->line(shiftCount + " = urem " + type + " " + b + ", " + std::to_string(bits));
                    const char* instruction = op == "<<" ? "shl" : isUnsigned ? "lshr" : "ashr";
                    current->line(result + " = " + instruction + " " + type + " " + a + ", " + shiftCount);
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Int, bits, isUnsigned};
                }
                if (op == "/" || op == "%") {
                    const auto constantDivisor = integerConstant(right);
                    if (constantDivisor && *constantDivisor != 0 && (isUnsigned || *constantDivisor != -1)) {
                        const std::string divisor = convertValue(right, NativeType::Int, bits, isUnsigned);
                        current->line(result + " = " + std::string(op == "/" ? (isUnsigned ? "udiv" : "sdiv") : (isUnsigned ? "urem" : "srem")) +
                            " " + type + " " + a + ", " + divisor);
                        return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Int, bits, isUnsigned};
                    }
                    const std::string wideA = convertValue(NativeExpr{NativeExpr::Kind::Value, a, {}, {}, NativeType::Int, bits, isUnsigned}, NativeType::Int, 64, isUnsigned);
                    const std::string wideB = convertValue(NativeExpr{NativeExpr::Kind::Value, b, {}, {}, NativeType::Int, bits, isUnsigned}, NativeType::Int, 64, isUnsigned);
                    const std::string wideResult = scalarCall("i64", isUnsigned ? (op == "/" ? "pts_uint_div" : "pts_uint_mod") : (op == "/" ? "pts_int_div" : "pts_int_mod"), "i64 " + wideA + ", i64 " + wideB);
                    NativeExpr divided{NativeExpr::Kind::Value, wideResult, {}, {}, NativeType::Int, 64, isUnsigned};
                    const std::string narrowed = convertValue(divided, NativeType::Int, bits, isUnsigned);
                    return NativeExpr{NativeExpr::Kind::Value, narrowed, {}, {}, NativeType::Int, bits, isUnsigned};
                }
                if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
                    const char* predicate = op == "==" ? "eq" : op == "!=" ? "ne" : op == "<" ? (isUnsigned ? "ult" : "slt") : op == "<=" ? (isUnsigned ? "ule" : "sle") : op == ">" ? (isUnsigned ? "ugt" : "sgt") : (isUnsigned ? "uge" : "sge");
                    current->line(result + " = icmp " + predicate + " " + type + " " + a + ", " + b);
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Bool};
                }
            }
        }
        if (left.kind == NativeExpr::Kind::Value && right.kind == NativeExpr::Kind::Value && left.type!=NativeType::Dynamic && right.type!=NativeType::Dynamic && (op == "and" || op == "or")) {
            const std::string a=truthValue(left),b=truthValue(right),result = current->temp();
            current->line(result + " = " + (op == "and" ? "and" : "or") + " i1 " + a + ", " + b);
            return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Bool};
        }
        if (left.kind == NativeExpr::Kind::Value && right.kind == NativeExpr::Kind::Value && left.type == NativeType::Bool && right.type == NativeType::Bool && (op == "==" || op == "!=")) {
            const std::string result = current->temp();
            current->line(result + " = icmp " + std::string(op == "==" ? "eq" : "ne") + " i1 " + left.value + ", " + right.value);
            return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Bool};
        }
        const std::string a = valueOf(left), b = valueOf(right);
        return NativeExpr{NativeExpr::Kind::Value, call("pts_binary", cstrArg(op) + ", i8* " + a + ", i8* " + b), {}, {}};
    }
    void validateFunctionArguments(const std::vector<NativeExpr>& expressions,
        const NativeFunctionSignature& expected,int line,const std::string& functionName) {
        if(expressions.size()!=expected.parameterTypes.size())
            throw CompileError(lineMessage(line,"Function/callback arguman sayisi uyusmuyor: "+functionName+" icin "+
                std::to_string(expected.parameterTypes.size())+" bekleniyor, "+std::to_string(expressions.size())+" verildi."));
        for(std::size_t i=0;i<expressions.size();++i) {
            const NativeType expectedType=expected.parameterTypes[i];
            NativeExpr actual=materialize(expressions[i]);
            if(expectedType==NativeType::Function) {
                if(actual.type!=NativeType::Function)
                    throw CompileError(lineMessage(line,"Function parametresine yalnizca duz fonksiyon adresi veya Function yereli verilebilir."));
                const std::string nested=i<expected.callbackParameterSignatures.size()?expected.callbackParameterSignatures[i]:std::string{};
                if(!nested.empty()&&(!actual.hasFunctionSignature||functionSignatureKey(actual.functionSignature)!=nested))
                    throw CompileError(lineMessage(line,"Function callback argumaninin imzasi uyusmuyor."));
                continue;
            }
            if(expectedType==NativeType::Dynamic)continue;
            const int expectedBits=i<expected.parameterBits.size()?expected.parameterBits[i]:64;
            const bool expectedUnsigned=i<expected.parameterUnsigned.size()&&expected.parameterUnsigned[i];
            if(actual.type!=expectedType||(expectedType==NativeType::Int&&
                (actual.bits!=expectedBits||actual.isUnsigned!=expectedUnsigned)))
                throw CompileError(lineMessage(line,"Function/callback argumani tur imzasi uyusmuyor: "+functionName+
                    " parametre "+std::to_string(i+1)+"."));
        }
    }
    void emitFixedArrayBoundsCheck(const std::string& index,std::size_t length,int line) {
        if(current==&initFunction)
            throw CompileError(lineMessage(line,"Sabit diziler yalnizca fonksiyon icinde kullanilabilir."));
        const std::string inBounds=current->temp();
        const std::string valid=current->newLabel("array_index_valid_");
        const std::string invalid=current->newLabel("array_index_invalid_");
        current->line(inBounds+" = icmp ult i64 "+index+", "+std::to_string(length));
        current->line("br i1 "+inBounds+", label %"+valid+", label %"+invalid);
        current->terminated=true;
        current->start(invalid);
        current->line("call void @pts_array_bounds_fail(i64 "+index+", i64 "+std::to_string(length)+")");
        current->line("unreachable");
        current->terminated=true;
        current->start(valid);
    }
    std::string arguments(const std::vector<NativeExpr>& expressions) {
        if (expressions.empty()) return "i8** null";
        std::string array = current->temp();
        current->line(array + " = alloca [" + std::to_string(expressions.size()) + " x i8*]");
        for (size_t i = 0; i < expressions.size(); ++i) {
            NativeExpr copy = expressions[i];
            std::string value = valueOf(copy);
            std::string slot = current->temp();
            current->line(slot + " = getelementptr inbounds [" + std::to_string(expressions.size()) + " x i8*], [" +
                         std::to_string(expressions.size()) + " x i8*]* " + array + ", i64 0, i64 " + std::to_string(i));
            current->line("store i8* " + value + ", i8** " + slot);
        }
        std::string first = current->temp();
        current->line(first + " = getelementptr inbounds [" + std::to_string(expressions.size()) + " x i8*], [" +
                      std::to_string(expressions.size()) + " x i8*]* " + array + ", i64 0, i64 0");
        return "i8** " + first;
    }
    NativeExpr callNamed(const std::string& name, const std::vector<NativeExpr>& args) {
        const auto structure=structDefinitions.find(name);
        if(structure!=structDefinitions.end())return NativeExpr{NativeExpr::Kind::StructType,{},name,{}};
        if (classNames.contains(name)) { NativeExpr instance{NativeExpr::Kind::Value,
            call("pts_make", cstrArg(name) + ", " + arguments(args) + ", i32 " + std::to_string(args.size())), {}, {}};instance.staticClass=name;return instance; }
        std::string functionName = name;
        auto alias = aliases.find(functionName);
        if (alias != aliases.end()) functionName = alias->second;
        if (!modulePrefix.empty() && functionName.find('.') == std::string::npos)
            functionName = modulePrefix + "." + functionName;
        if(const auto signature=functionSignatures.find(functionName);signature!=functionSignatures.end()&&
            std::find(signature->second.parameterTypes.begin(),signature->second.parameterTypes.end(),NativeType::Function)!=signature->second.parameterTypes.end())
            validateFunctionArguments(args,signature->second,token().line,functionName);
        const auto nativeFunction = directNativeFunctions.find(functionName);
        if (nativeFunction != directNativeFunctions.end() && args.size() == nativeFunction->second.params.size()) {
            const auto& signature = nativeFunction->second;
            std::string callArguments = signature.needsParentEnvironment ? "i8* " + current->env : std::string{};
            for (size_t i = 0; i < args.size(); ++i) {
                const auto& parameter = signature.params[i];
                const std::string value = convertValue(args[i], parameter.type, parameter.bits, parameter.isUnsigned);
                if (!callArguments.empty()) callArguments += ", ";
                callArguments += llvmType(parameter.type, parameter.bits) + " " + value;
            }
            const std::string result = current->temp();
            current->line(result + " = call " + std::string(releaseOptimizations ? "fastcc " : "") +
                llvmType(signature.resultType, signature.resultBits) + " @" +
                signature.symbol + "(" + callArguments + ")");
            return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, signature.resultType,
                signature.resultBits, signature.resultUnsigned};
        }
        if (functionName == "random.int_range" && args.size() == 2 &&
            args[0].type == NativeType::Int && args[1].type == NativeType::Int) {
            const std::string packed = arguments(args);
            const std::string boxedResult = call("pts_random_call", cstrArg("int_range") + ", " + packed + ", i32 2");
            const std::string result = current->temp();
            current->line(result + " = call i64 @pts_as_int(i8* " + boxedResult + ")");
            return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Int, 64, false};
        }
        const std::size_t mathDot = functionName.find('.');
        const bool isMathCall = mathDot != std::string::npos &&
            (functionName.substr(0, mathDot) == "Math" || functionName.substr(0, mathDot) == "Matematik");
        std::vector<NativeExpr> evaluatedMathArgs;
        if (isMathCall) {
            evaluatedMathArgs.reserve(args.size());
            for (const auto& argument : args) {
                const bool staticallyStoredLocal = argument.kind == NativeExpr::Kind::Variable && current->locals.contains(argument.name);
                const bool staticallyTypedMember = argument.kind == NativeExpr::Kind::Member && argument.type != NativeType::Dynamic;
                evaluatedMathArgs.push_back(staticallyStoredLocal || staticallyTypedMember ? materialize(argument) : argument);
            }
        }
        const std::vector<NativeExpr>& mathArgs = isMathCall ? evaluatedMathArgs : args;
        if (isMathCall) {
            const std::string operation = functionName.substr(mathDot + 1);
            const auto isStaticNumber = [](const NativeExpr& value) {
                return value.kind == NativeExpr::Kind::Value &&
                    (value.type == NativeType::Int || value.type == NativeType::Float);
            };
            const auto asDouble = [&](const NativeExpr& value) {
                return convertValue(value, NativeType::Float);
            };
            const auto callFloatIntrinsic = [&](const std::string& intrinsic, const NativeExpr& argument) {
                const std::string result = current->temp();
                current->line(result + " = call double @llvm." + intrinsic + ".f64(double " + asDouble(argument) + ")");
                return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Float};
            };
            const auto callFloatFunction = [&](const std::string& function, const NativeExpr& argument) {
                const std::string result = current->temp();
                current->line(result + " = call double @" + function + "(double " + asDouble(argument) + ")");
                return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Float};
            };
            if (mathArgs.empty()) {
                if (operation == "pi" || operation == "PI")
                    return NativeExpr{NativeExpr::Kind::Value, "3.141592653589793238462643383279502884", {}, {}, NativeType::Float};
                if (operation == "e" || operation == "E")
                    return NativeExpr{NativeExpr::Kind::Value, "2.718281828459045235360287471352662498", {}, {}, NativeType::Float};
                if (operation == "tau" || operation == "TAU")
                    return NativeExpr{NativeExpr::Kind::Value, "6.283185307179586476925286766559005768", {}, {}, NativeType::Float};
            }
            if (mathArgs.size() == 1 && isStaticNumber(mathArgs[0])) {
                if (operation == "sqrt" || operation == "karekok") return callFloatIntrinsic("sqrt", mathArgs[0]);
                if (operation == "sin") return callFloatIntrinsic("sin", mathArgs[0]);
                if (operation == "cos") return callFloatIntrinsic("cos", mathArgs[0]);
                if (operation == "tan") return callFloatFunction("tan", mathArgs[0]);
                if (operation == "asin") return callFloatFunction("asin", mathArgs[0]);
                if (operation == "acos") return callFloatFunction("acos", mathArgs[0]);
                if (operation == "atan") return callFloatFunction("atan", mathArgs[0]);
                if (operation == "exp") return callFloatIntrinsic("exp", mathArgs[0]);
                if (operation == "log" || operation == "ln") return callFloatIntrinsic("log", mathArgs[0]);
                if (operation == "log10") return callFloatIntrinsic("log10", mathArgs[0]);
                if (operation == "floor" || operation == "taban") return callFloatIntrinsic("floor", mathArgs[0]);
                if (operation == "ceil" || operation == "tavan") return callFloatIntrinsic("ceil", mathArgs[0]);
                if (operation == "round" || operation == "yuvarla") return callFloatIntrinsic("round", mathArgs[0]);
                if (operation == "abs" || operation == "mutlak") {
                    if (mathArgs[0].type == NativeType::Float) return callFloatIntrinsic("fabs", mathArgs[0]);
                    if (mathArgs[0].isUnsigned) {
                        const std::string result = convertValue(mathArgs[0], NativeType::Int, 64, true);
                        return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Int, 64, true};
                    }
                    const std::string value = convertValue(mathArgs[0], NativeType::Int, 64, false);
                    const std::string negative = current->temp();
                    const std::string negated = current->temp();
                    const std::string isMinimum = current->temp();
                    const std::string saturated = current->temp();
                    const std::string result = current->temp();
                    current->line(negative + " = icmp slt i64 " + value + ", 0");
                    current->line(negated + " = sub i64 0, " + value);
                    current->line(isMinimum + " = icmp eq i64 " + value + ", -9223372036854775808");
                    current->line(saturated + " = select i1 " + isMinimum + ", i64 9223372036854775807, i64 " + negated);
                    current->line(result + " = select i1 " + negative + ", i64 " + saturated + ", i64 " + value);
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Int, 64, false};
                }
                if (operation == "radians" || operation == "radyan" || operation == "degrees" || operation == "derece") {
                    const bool toRadians = operation == "radians" || operation == "radyan";
                    const std::string factor = toRadians ? "0.017453292519943295769236907684886" : "57.295779513082320876798154814105";
                    const std::string result = current->temp();
                    current->line(result + " = fmul double " + asDouble(mathArgs[0]) + ", " + factor);
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Float};
                }
            }
            if (mathArgs.size() == 2 && isStaticNumber(mathArgs[0]) && isStaticNumber(mathArgs[1])) {
                const std::string a = asDouble(mathArgs[0]);
                const std::string b = asDouble(mathArgs[1]);
                if (operation == "pow" || operation == "kuvvet") {
                    const std::string result = current->temp();
                    current->line(result + " = call double @llvm.pow.f64(double " + a + ", double " + b + ")");
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Float};
                }
                if (operation == "min" || operation == "minimum" || operation == "max" || operation == "maksimum") {
                    const bool isMinimum = operation == "min" || operation == "minimum";
                    const std::string compare = current->temp();
                    const std::string result = current->temp();
                    current->line(compare + " = fcmp olt double " + (isMinimum ? b : a) + ", " + (isMinimum ? a : b));
                    current->line(result + " = select i1 " + compare + ", double " + b + ", double " + a);
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Float};
                }
                if (operation == "atan2") {
                    const std::string result = current->temp();
                    current->line(result + " = call double @atan2(double " + a + ", double " + b + ")");
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Float};
                }
            }
            if (mathArgs.size() == 3 && isStaticNumber(mathArgs[0]) && isStaticNumber(mathArgs[1]) && isStaticNumber(mathArgs[2])) {
                const std::string value = asDouble(mathArgs[0]);
                const std::string lower = asDouble(mathArgs[1]);
                const std::string upper = asDouble(mathArgs[2]);
                if (operation == "lerp" || operation == "dogrusal_aradeger") {
                    const std::string factor = asDouble(mathArgs[2]);
                    const std::string endpoint = asDouble(mathArgs[1]);
                    const std::string difference = current->temp();
                    const std::string scaled = current->temp();
                    const std::string result = current->temp();
                    current->line(difference + " = fsub double " + endpoint + ", " + value);
                    current->line(scaled + " = fmul double " + difference + ", " + factor);
                    current->line(result + " = fadd double " + value + ", " + scaled);
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Float};
                }
                if (operation == "clamp" || operation == "sinirla") {
                    const std::string below = current->temp();
                    const std::string lowClamped = current->temp();
                    const std::string above = current->temp();
                    const std::string result = current->temp();
                    current->line(below + " = fcmp olt double " + value + ", " + lower);
                    current->line(lowClamped + " = select i1 " + below + ", double " + lower + ", double " + value);
                    current->line(above + " = fcmp olt double " + upper + ", " + lowClamped);
                    current->line(result + " = select i1 " + above + ", double " + upper + ", double " + lowClamped);
                    return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Float};
                }
            }
        }
        const std::string packed = arguments(args);
        const std::string count = "i32 " + std::to_string(args.size());
        if (name == "Color.count" || name == "Color.palette_count" || name == "Color.generated_count") {
            const std::string result = scalarCall("i64", name == "Color.generated_count" ? "pts_color_generated_count" : "pts_color_count", "");
            return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Int, 64, false};
        }
        if (name.rfind("Color.", 0) == 0 && name.find('.', 6) == std::string::npos &&
            (name == "Color.rgb" || name == "Color.hex" || name == "Color.index4" || name == "Color.rgb565" ||
             name == "Color.rgba64" || name == "Color.rgb_depth" || name == "Color.at" || name == "Color.palette" ||
             name == "Color.generate" || name == "Color.spectrum" || name == "Color.named" || name == "Color.name"))
            return NativeExpr{NativeExpr::Kind::Value, call("pts_color_call", cstrArg(name.substr(6)) + ", " + packed + ", " + count), {}, {}};
        if (name.rfind("Engine.", 0) == 0) return NativeExpr{NativeExpr::Kind::Value,
            call("pts_engine_call", cstrArg(name.substr(7)) + ", " + packed + ", " + count), {}, {}};
        if (name.rfind("Engine3D.", 0) == 0) return NativeExpr{NativeExpr::Kind::Value,
            call("pts_engine3d_call", cstrArg(name.substr(9)) + ", " + packed + ", " + count), {}, {}};
        if (name.rfind("Physics3D.", 0) == 0) return NativeExpr{NativeExpr::Kind::Value,
            call("pts_engine3d_call", cstrArg("physics_" + name.substr(10)) + ", " + packed + ", " + count), {}, {}};
        if (name.rfind("UI.", 0) == 0) return NativeExpr{NativeExpr::Kind::Value,
            call("pts_ui_call", cstrArg(name.substr(3)) + ", " + packed + ", " + count), {}, {}};
        if (name.rfind("File.", 0) == 0 || name.rfind("Dosya.", 0) == 0)
            return NativeExpr{NativeExpr::Kind::Value, call("pts_file_call", cstrArg(name.substr(name.find('.') + 1)) + ", " + packed + ", " + count), {}, {}};
        if (name.rfind("Console.", 0) == 0 || name.rfind("Konsol.", 0) == 0)
            return NativeExpr{NativeExpr::Kind::Value, call("pts_console_call", cstrArg(name.substr(name.find('.') + 1)) + ", " + packed + ", " + count), {}, {}};
        if (name.rfind("Math.", 0) == 0 || name.rfind("Matematik.", 0) == 0)
            return NativeExpr{NativeExpr::Kind::Value, call("pts_math_call", cstrArg(name.substr(name.find('.') + 1)) + ", " + packed + ", " + count), {}, {}};
        if (name.rfind("System.", 0) == 0 || name.rfind("Sistem.", 0) == 0)
            return NativeExpr{NativeExpr::Kind::Value, call("pts_system_call", cstrArg(name.substr(name.find('.') + 1)) + ", " + packed + ", " + count), {}, {}};
        if (name.rfind("String.", 0) == 0 || name.rfind("Metin.", 0) == 0)
            return NativeExpr{NativeExpr::Kind::Value, call("pts_string_call", cstrArg(name.substr(name.find('.') + 1)) + ", " + packed + ", " + count), {}, {}};
        // ben yaptım opencode - 2026-11-14 - stdlib cagri yonlendirmesi
        {
            const std::size_t dot = name.find('.');
            if (dot != std::string::npos && isStdlibNamespace(name.substr(0, dot))) {
                usedStdlibModules.insert(name.substr(0, dot));
                return NativeExpr{NativeExpr::Kind::Value,
                    call("pts_stdlib_call", cstrArg(name.substr(0, dot)) + ", " + cstrArg(name.substr(dot + 1)) + ", " + packed + ", " + count), {}, {}};
            }
        }
        if (name.rfind("Arena.", 0) == 0)
            return NativeExpr{NativeExpr::Kind::Value, call("pts_arena_call", cstrArg(name.substr(6)) + ", " + packed + ", " + count), {}, {}};
        if (name.rfind("Color.", 0) == 0) return NativeExpr{NativeExpr::Kind::Value, stringValue(name.substr(6)), {}, {}};
        if (name.rfind("random.", 0) == 0) return NativeExpr{NativeExpr::Kind::Value,
            call("pts_random_call", cstrArg(name.substr(7)) + ", " + packed + ", " + count), {}, {}};
        if (name == "List" || name == "FlowList") return NativeExpr{NativeExpr::Kind::Value,
            call("pts_list_new", std::string("i1 ") + (name == "FlowList" ? "true" : "false")), {}, {}};
        if (name == "string") return NativeExpr{NativeExpr::Kind::Value,
            call("pts_to_string", "i8* " + (args.empty() ? std::string("null") : valueOf(args[0]))), {}, {}};
        if (name == "print" || name == "yaz" || name == "write") {
            callVoid("pts_print", packed + ", " + count);
            return NativeExpr{NativeExpr::Kind::Value, "null", {}, {}};
        }
        const auto directFunction = directFunctionSymbols.find(functionName);
        if (directFunction != directFunctionSymbols.end()) {
            const auto needsEnv = functionNeedsEnvironment.find(functionName);
            const std::string parent = needsEnv != functionNeedsEnvironment.end() && !needsEnv->second ? "null" : current->env;
            return NativeExpr{NativeExpr::Kind::Value,
                call(directFunction->second, "i8* " + parent + ", " + packed + ", " + count), {}, {}};
        }
        const std::string dispatcher = functionName.find('.') == std::string::npos ? "pts_call_global" : "pts_call_namespace";
        dynamicFunctionCalls.insert(functionName);
        return NativeExpr{NativeExpr::Kind::Value,
            call(dispatcher, cstrArg(functionName) + ", i8* " + current->env + ", " + packed + ", " + count), {}, {}};
    }
    NativeExpr invokeFunctionPointer(const std::string& name,const LocalValue& local,
        const std::vector<NativeExpr>& args,int line) {
        if(local.hasFunctionSignature)
            validateFunctionArguments(args,local.functionSignature,line,name);
        const std::string pointer=current->temp();
        current->line(pointer+" = load i8*, i8** "+local.slot);
        const std::string callable=current->temp();
        current->line(callable+" = bitcast i8* "+pointer+" to i8* (i8*, i8**, i32)*");
        const std::string packed=arguments(args);
        const std::string result=current->temp();
        current->line(result+" = call i8* "+callable+"(i8* "+current->env+", "+packed+", i32 "+std::to_string(args.size())+")");
        NativeExpr boxed{NativeExpr::Kind::Value,result,{}, {}};
        if(local.hasFunctionSignature&&local.functionSignature.resultType!=NativeType::Dynamic) {
            const auto& signature=local.functionSignature;
            const std::string value=convertValue(boxed,signature.resultType,signature.resultBits,signature.resultUnsigned);
            return NativeExpr{NativeExpr::Kind::Value,value,{}, {},signature.resultType,signature.resultBits,signature.resultUnsigned};
        }
        return boxed;
    }
    NativeExpr invoke(NativeExpr callee, const std::vector<NativeExpr>& args) {
        if(callee.kind==NativeExpr::Kind::StructType) {
            const auto definition=structDefinitions.find(callee.name);
            if(definition==structDefinitions.end())throw CompileError(lineMessage(token().line,"Struct tanimi bulunamadi: "+callee.name));
            if(args.size()>definition->second.size())throw CompileError(lineMessage(token().line,"Struct kurucusuna alan sayisindan fazla deger verildi."));
            auto instance=std::make_shared<NativeStructInstance>();
            instance->typeName=callee.name;
            for(std::size_t i=0;i<definition->second.size();++i) {
                const auto& field=definition->second[i];
                const std::string internalName="__pts_struct_"+std::to_string(current->local)+"_"+field.name;
                const std::string type=llvmType(field.type,field.bits);
                const std::string slot=current->addLocal(internalName,field.type,field.bits,field.isUnsigned,type);
                NativeExpr value;
                if(i<args.size())value=materialize(args[i]);
                else value=NativeExpr{NativeExpr::Kind::Value,field.defaultValue,{}, {},field.type,field.bits,field.isUnsigned};
                const std::string native=convertValue(std::move(value),field.type,field.bits,field.isUnsigned);
                current->line("store "+type+" "+native+", "+type+"* "+slot);
                instance->fields.emplace(field.name,NativeStructFieldSlot{slot,field.type,field.bits,field.isUnsigned});
            }
            NativeExpr result{NativeExpr::Kind::StructValue,{},callee.name,{}};
            result.staticClass=callee.name;
            result.structInstance=std::move(instance);
            return result;
        }
        if (callee.kind == NativeExpr::Kind::ClassType) { NativeExpr instance{NativeExpr::Kind::Value,
            call("pts_make", cstrArg(callee.name) + ", " + arguments(args) + ", i32 " + std::to_string(args.size())), {}, {}};instance.staticClass=callee.name;return instance; }
        if (callee.kind == NativeExpr::Kind::ListType) return callNamed(callee.value, args);
        if (callee.kind == NativeExpr::Kind::Qualified) return callNamed(callee.name, args);
        if (callee.kind == NativeExpr::Kind::Member) return NativeExpr{NativeExpr::Kind::Value,
            call("pts_call_method", "i8* " + callee.owner + ", " + cstrArg(callee.name) + ", " + arguments(args) + ", i32 " + std::to_string(args.size())), {}, {}};
        if (callee.kind == NativeExpr::Kind::Variable) {
            const auto local=current->locals.find(callee.name);
            if(local!=current->locals.end()&&local->second.type==NativeType::Function)
                return invokeFunctionPointer(callee.name,local->second,args,token().line);
            return callNamed(callee.name, args);
        }
        if (callee.kind == NativeExpr::Kind::Namespace) throw CompileError(lineMessage(token().line, "Modul adi fonksiyon gibi cagrilamaz."));
        throw CompileError(lineMessage(token().line, "Bu ifade cagrilabilir degil."));
    }
    int precedence() const {
        if (word("or") || is(Kind::OrOr)) return 1;
        if (word("and") || is(Kind::AndAnd)) return 2;
        if (is(Kind::BitOr)) return 3;
        if (is(Kind::BitXor)) return 4;
        if (is(Kind::BitAnd)) return 5;
        if (is(Kind::Equal)||is(Kind::NotEqual)) return 6;
        if (is(Kind::Less)||is(Kind::LessEqual)||is(Kind::Greater)||is(Kind::GreaterEqual)) return 7;
        if (is(Kind::ShiftLeft)||is(Kind::ShiftRight)) return 8;
        if (is(Kind::Plus)||is(Kind::Minus)) return 9;
        if (is(Kind::Star)||is(Kind::Slash)||is(Kind::Percent)) return 10;
        return 0;
    }
    std::string opText(const Token& op) const {
        switch(op.kind) {
            case Kind::Plus:return "+";case Kind::Minus:return "-";case Kind::Star:return "*";case Kind::Slash:return "/";case Kind::Percent:return "%";
            case Kind::Equal:return "==";case Kind::NotEqual:return "!=";case Kind::Less:return "<";case Kind::LessEqual:return "<=";case Kind::Greater:return ">";case Kind::GreaterEqual:return ">=";
            case Kind::AndAnd:return "and";case Kind::OrOr:return "or";case Kind::BitAnd:return "&";case Kind::BitOr:return "|";case Kind::BitXor:return "^";case Kind::ShiftLeft:return "<<";case Kind::ShiftRight:return ">>";default:return canonicalWord(op.text);
        }
    }
    NativeExpr expression(int minPrec = 1) {
        NativeExpr left = unaryExpr();
        while (precedence() >= minPrec && precedence() > 0) {
            int p = precedence(); Token operation = take();
            NativeExpr right = expression(p + 1);
            left = binaryValue(opText(operation), std::move(left), std::move(right));
        }
        return left;
    }
    NativeExpr unaryExpr() {
        if (match(Kind::Minus)) {
            NativeExpr value = materialize(unaryExpr());
            if (value.type == NativeType::Int) { const std::string result=current->temp();const std::string type=llvmType(NativeType::Int,value.bits);current->line(result+" = sub"+std::string(releaseOverflowUndefined&&!value.isUnsigned?" nsw":"")+" "+type+" 0, "+value.value);return NativeExpr{NativeExpr::Kind::Value,result,{}, {},NativeType::Int,value.bits,value.isUnsigned}; }
            if (value.type == NativeType::Float) { const std::string result=current->temp();current->line(result+" = fneg double "+value.value);return NativeExpr{NativeExpr::Kind::Value,result,{}, {},NativeType::Float}; }
            return NativeExpr{NativeExpr::Kind::Value,call("pts_unary",cstrArg("-")+", i8* "+valueOf(value)),{}, {}};
        }
        if (match(Kind::Tilde)) {
            NativeExpr value = materialize(unaryExpr());
            if (value.type == NativeType::Int) {
                const std::string result = current->temp(), type = llvmType(NativeType::Int, value.bits);
                current->line(result + " = xor " + type + " " + value.value + ", -1");
                return NativeExpr{NativeExpr::Kind::Value, result, {}, {}, NativeType::Int, value.bits, value.isUnsigned};
            }
            return NativeExpr{NativeExpr::Kind::Value, call("pts_unary", cstrArg("~") + ", i8* " + valueOf(value)), {}, {}};
        }
        if (match(Kind::Bang) || matchWord("not")) {
            NativeExpr value = materialize(unaryExpr());
            const std::string condition=value.type==NativeType::Bool?value.value:truthValue(value),result=current->temp();
            current->line(result+" = xor i1 "+condition+", true");return NativeExpr{NativeExpr::Kind::Value,result,{}, {},NativeType::Bool};
        }
        return postfixExpr(primaryExpr());
    }
    NativeExpr primaryExpr() {
        if (is(Kind::Number)) { std::string n=take().text; if(n.find('.')!=std::string::npos)return NativeExpr{NativeExpr::Kind::Value,n,{}, {},NativeType::Float}; return NativeExpr{NativeExpr::Kind::Value,n,{}, {},NativeType::Int}; }
        if (is(Kind::String)) return NativeExpr{NativeExpr::Kind::Value,stringValue(take().text),{}, {}};
        if (word("true")) { take();return NativeExpr{NativeExpr::Kind::Value,"true",{}, {},NativeType::Bool}; }
        if (word("false")) { take();return NativeExpr{NativeExpr::Kind::Value,"false",{}, {},NativeType::Bool}; }
        if (match(Kind::LParen)) { auto e=expression();expect(Kind::RParen,"')' bekleniyordu.");return e; }
        if (!is(Kind::Identifier)) throw CompileError(lineMessage(token().line,"Bir ifade bekleniyordu."));
        Token id=take(); std::string canonical=canonicalWord(id.text); std::string name=cppIdentifier(id.text);
        if (canonical=="new") { Token type=expectName("new sonrasinda tur bekleniyordu.");std::string t=nativeTypeName(type.text);if(t=="List")return NativeExpr{NativeExpr::Kind::ListType,"List",{}, {}};if(t=="FlowList")return NativeExpr{NativeExpr::Kind::ListType,"FlowList",{}, {}};if(!modulePrefix.empty()&&structDefinitions.contains(modulePrefix+"."+t))t=modulePrefix+"."+t;if(!modulePrefix.empty()&&classNames.contains(modulePrefix+"."+t))t=modulePrefix+"."+t;if(structDefinitions.contains(t))return NativeExpr{NativeExpr::Kind::StructType,{},t,{}};return NativeExpr{NativeExpr::Kind::ClassType,{},t,{}}; }
        if (canonical=="list" && match(Kind::Less)) { while(!is(Kind::Greater)&&!is(Kind::End))take();expect(Kind::Greater,"Generic tur '>' ile bitmeli.");return NativeExpr{NativeExpr::Kind::ListType,"List",{}, {}}; }
        if (canonical=="self" || canonical=="this") { NativeExpr self{NativeExpr::Kind::Value,call("pts_env_get","i8* "+current->env+", "+cstrArg("self")),{}, {}};self.staticClass=currentClass;return self; }
        if (canonical=="flowlist") return NativeExpr{NativeExpr::Kind::ListType,"FlowList",{}, {}};
        auto alias=aliases.find(name);if(alias!=aliases.end())name=alias->second;
        if (name=="Engine"||name=="Engine3D"||name=="Physics3D"||name=="UI"||name=="Color"||name=="File"||name=="Dosya"||name=="Console"||name=="Konsol"||name=="Math"||name=="Matematik"||name=="System"||name=="Sistem"||name=="String"||name=="Metin"||name=="random"||namespaceRoots.contains(name)) return NativeExpr{NativeExpr::Kind::Namespace,{},name,{}};
        if (structDefinitions.contains(name)) return NativeExpr{NativeExpr::Kind::StructType,{},name,{}};
        if (!modulePrefix.empty() && structDefinitions.contains(modulePrefix + "." + name)) return NativeExpr{NativeExpr::Kind::StructType,{},modulePrefix + "." + name,{}};
        if (classNames.contains(name)) return NativeExpr{NativeExpr::Kind::ClassType,{},name,{}};
        if (!modulePrefix.empty() && classNames.contains(modulePrefix + "." + name)) return NativeExpr{NativeExpr::Kind::ClassType,{},modulePrefix + "." + name,{}};
        const bool assignmentTarget=is(Kind::Assign)||is(Kind::PlusAssign)||is(Kind::MinusAssign)||is(Kind::StarAssign)||is(Kind::SlashAssign)||is(Kind::PercentAssign)||is(Kind::BitAndAssign)||is(Kind::BitOrAssign)||is(Kind::BitXorAssign)||is(Kind::ShiftLeftAssign)||is(Kind::ShiftRightAssign);
        const auto configured=configuredScalars.find(name);
        if(!assignmentTarget&&!is(Kind::LParen)&&configured!=configuredScalars.end()&&current!=&initFunction&&
           !current->locals.contains(name)&&!current->dynamicLocals.contains(name)&&
           !current->nativeObjectDataLocals.contains(name)&&!current->nativeStructLocals.contains(name))
            return configured->second;
        NativeExpr variable{NativeExpr::Kind::Variable,{},name,{}};const auto objectType=current->staticObjectTypes.find(name);if(objectType!=current->staticObjectTypes.end())variable.staticClass=objectType->second;const auto nativeObjectData=current->nativeObjectDataLocals.find(name);if(nativeObjectData!=current->nativeObjectDataLocals.end()){variable.nativeData=current->temp();current->line(variable.nativeData+" = load i8*, i8** "+nativeObjectData->second);}const auto nativeStruct=current->nativeStructLocals.find(name);if(nativeStruct!=current->nativeStructLocals.end())variable.structInstance=nativeStruct->second;return variable;
    }
    NativeExpr postfixExpr(NativeExpr value) {
        while(true) {
            if(match(Kind::Dot)||match(Kind::Arrow)) {
                Token member=expectName("Nokta/ok operatorunden sonra ad bekleniyordu.");std::string m=cppIdentifier(member.text);
                if(value.kind==NativeExpr::Kind::Namespace) {
                    if(value.name=="Color") {
                        const bool colorFunction = m=="count" || m=="palette_count" || m=="generated_count" ||
                            m=="rgb" || m=="hex" || m=="index4" || m=="rgb565" || m=="rgba64" ||
                            m=="rgb_depth" || m=="at" || m=="palette" || m=="generate" ||
                            m=="spectrum" || m=="named" || m=="name";
                        value=colorFunction
                            ? NativeExpr{NativeExpr::Kind::Qualified,{},"Color."+m,{}}
                            : NativeExpr{NativeExpr::Kind::Value,stringValue(m),{}, {}};
                    }
                    else { std::string joined=value.name+"."+m;value=namespaceRoots.contains(joined)?NativeExpr{NativeExpr::Kind::Namespace,{},joined,{}}:NativeExpr{NativeExpr::Kind::Qualified,{},joined,{}}; }
                } else if(value.kind==NativeExpr::Kind::Qualified && namespaceRoots.contains(value.name+"."+m)) {
                    value=NativeExpr{NativeExpr::Kind::Namespace,{},value.name+"."+m,{}};
                } else {
                    std::shared_ptr<NativeStructInstance> structInstance;
                    if(value.kind==NativeExpr::Kind::StructValue || value.kind==NativeExpr::Kind::Variable)
                        structInstance=value.structInstance;
                    if(structInstance) {
                        const auto field=structInstance->fields.find(m);
                        if(field==structInstance->fields.end())
                            throw CompileError(lineMessage(member.line,"Struct alanı bulunamadı: "+m));
                        NativeExpr memberExpr{NativeExpr::Kind::Member,{},m,field->second.slot,
                            field->second.type,field->second.bits,field->second.isUnsigned,-2,structInstance->typeName};
                        memberExpr.baseName=value.kind==NativeExpr::Kind::Variable?value.name:value.baseName;
                        value=std::move(memberExpr);
                        continue;
                    }
                    std::string owner;NativeExpr memberExpr{NativeExpr::Kind::Member,{},m,{}};memberExpr.staticClass=value.staticClass;
                    memberExpr.baseName=value.kind==NativeExpr::Kind::Variable?value.name:value.baseName;
                    const auto classFields=classFieldIndices.find(memberExpr.staticClass);
                    if(classFields!=classFieldIndices.end()){
                        const auto field=classFields->second.find(m);
                        if(field!=classFields->second.end()){
                            memberExpr.fieldIndex=field->second;
                            const auto typeMap=classFieldTypes.find(memberExpr.staticClass);
                            if(typeMap!=classFieldTypes.end()){
                                const auto fieldType=typeMap->second.find(m);
                                if(fieldType!=typeMap->second.end()){
                                    memberExpr.type=fieldType->second.type;
                                    memberExpr.bits=fieldType->second.bits;
                                    memberExpr.isUnsigned=fieldType->second.isUnsigned;
                                }
                            }
                            if(memberExpr.type!=NativeType::Dynamic) {
                                std::string base=value.nativeData;
                                if(base.empty()) { owner=valueOf(value);base=scalarCall("i8*","pts_native_data","i8* "+owner); }
                                const std::string byteAddress=current->temp();
                                current->line(byteAddress+" = getelementptr inbounds i8, i8* "+base+", i64 "+std::to_string(memberExpr.fieldIndex*8));
                                const std::string fieldAddress=current->temp();
                                current->line(fieldAddress+" = bitcast i8* "+byteAddress+" to "+llvmType(memberExpr.type,memberExpr.bits)+"*");
                                memberExpr.owner=fieldAddress;
                                memberExpr.fieldIndex=-3;
                            }
                        }
                    }
                    if(memberExpr.fieldIndex!=-3) { if(owner.empty())owner=valueOf(value);memberExpr.owner=owner; }
                    value=std::move(memberExpr);
                }
            } else if(match(Kind::LParen)) {
                std::vector<NativeExpr> args;if(!is(Kind::RParen)){do{args.push_back(expression());}while(match(Kind::Comma));}expect(Kind::RParen,"Fonksiyon cagrisinda ')' bekleniyordu.");value=invoke(std::move(value),args);
            } else if(match(Kind::LBracket)) {
                if(value.kind==NativeExpr::Kind::Variable) {
                    const auto fixed=current->fixedArrays.find(value.name);
                    if(fixed!=current->fixedArrays.end()) {
                        const std::string arrayName=value.name;
                        const int indexLine=token().line;
                        const bool negativeIntegerLiteral=is(Kind::Minus)&&look().kind==Kind::Number&&
                            look(2).kind==Kind::RBracket&&look().text.find('.')==std::string::npos;
                        NativeExpr indexExpr=materialize(expression());
                        expect(Kind::RBracket,"Sabit dizi indisinden sonra ']' bekleniyordu.");
                        if(indexExpr.type!=NativeType::Int&&indexExpr.type!=NativeType::Dynamic)
                            throw CompileError(lineMessage(indexLine,"Sabit dizi indisi tam sayi olmali."));
                        if(negativeIntegerLiteral)
                            throw CompileError(lineMessage(indexLine,"Sabit dizi indis sabiti sinir disinda: "+arrayName));
                        std::optional<bool> constantIndexInBounds;
                        if(indexExpr.kind==NativeExpr::Kind::Value&&indexExpr.type==NativeType::Int&&!indexExpr.value.empty()) {
                            std::int64_t signedIndex=0;
                            const char* begin=indexExpr.value.data();
                            const char* end=begin+indexExpr.value.size();
                            const auto signedResult=std::from_chars(begin,end,signedIndex,10);
                            if(signedResult.ec==std::errc{}&&signedResult.ptr==end)
                                constantIndexInBounds=signedIndex>=0&&static_cast<std::uint64_t>(signedIndex)<fixed->second.length;
                            else {
                                std::uint64_t unsignedIndex=0;
                                const auto unsignedResult=std::from_chars(begin,end,unsignedIndex,10);
                                if(unsignedResult.ec==std::errc{}&&unsignedResult.ptr==end)
                                    constantIndexInBounds=unsignedIndex<fixed->second.length;
                            }
                        }
                        if(constantIndexInBounds.has_value()&&!*constantIndexInBounds)
                            throw CompileError(lineMessage(indexLine,"Sabit dizi indis sabiti sinir disinda: "+arrayName));
                        const std::string index=convertValue(std::move(indexExpr),NativeType::Int,64,false);
                        if(!constantIndexInBounds.has_value())emitFixedArrayBoundsCheck(index,fixed->second.length,indexLine);
                        const std::string arrayLLVM="["+std::to_string(fixed->second.length)+" x "+llvmType(fixed->second.elementType,fixed->second.elementBits)+"]";
                        const std::string pointer=current->temp();
                        current->line(pointer+" = getelementptr inbounds "+arrayLLVM+", "+arrayLLVM+"* "+fixed->second.slot+", i64 0, i64 "+index);
                        NativeExpr element{NativeExpr::Kind::Member,{},arrayName,pointer,
                            fixed->second.elementType,fixed->second.elementBits,fixed->second.elementUnsigned,-2};
                        element.baseName=arrayName;
                        value=std::move(element);
                        continue;
                    }
                }
                const std::string container = valueOf(value);
                const std::string index = valueOf(expression());
                expect(Kind::RBracket,"Dizi/list indisinden sonra ']' bekleniyordu.");
                value=NativeExpr{NativeExpr::Kind::Index,{},index,container};
            } else break;
        }
        return value;
    }
    void parseBody(bool colon, const std::function<void()>& beforeClose = {}) {
        if(colon)expect(Kind::Colon,"Blok ':' ile bitmeli.");
        while(is(Kind::Newline))take();expect(Kind::Indent,"Blok govdesi girintili olmali.");
        while(!is(Kind::Dedent)&&!is(Kind::End)){skipLines();if(is(Kind::Dedent)||is(Kind::End))break;if(current->terminated)current->start(current->newLabel("dead_"));statement();}
        if(beforeClose)beforeClose();expect(Kind::Dedent,"Blok girintisi kapatilamadi.");
    }
    NativeFunctionSignature parseFunctionTypeArguments(int line) {
        expect(Kind::Less,"Function imzasi '<' ile baslamali.");
        NativeFunctionSignature signature;
        while(!is(Kind::Greater)&&!is(Kind::Arrow)&&!is(Kind::End)) {
            Token argument=expectName("Function imzasinda parametre turu bekleniyordu.");
            const std::string typeName=nativeTypeName(argument.text);
            if(is(Kind::Less))
                throw CompileError(lineMessage(argument.line,"Ici ice Function callback imzalari henuz desteklenmiyor."));
            const NativeType type=valueType(typeName);
            const IntegerTypeInfo integer=integerTypeInfo(typeName);
            if(type==NativeType::Dynamic&&canonicalWord(typeName)!="string"&&canonicalWord(typeName)!="auto")
                throw CompileError(lineMessage(argument.line,"Function imzasi int/uint, float/double, bool veya string parametrelerini destekliyor."));
            signature.parameterTypes.push_back(type);
            signature.parameterBits.push_back(integer.valid?integer.bits:type==NativeType::Bool?1:64);
            signature.parameterUnsigned.push_back(integer.valid&&integer.isUnsigned);
            signature.callbackParameterSignatures.emplace_back();
            if(!match(Kind::Comma))break;
        }
        if(match(Kind::Arrow)) {
            Token result=expectName("Function imzasinda donus turu bekleniyordu.");
            const std::string typeName=nativeTypeName(result.text);
            if(is(Kind::Less))
                throw CompileError(lineMessage(result.line,"Function pointer donus turleri henuz desteklenmiyor."));
            signature.resultType=valueType(typeName);
            const IntegerTypeInfo integer=integerTypeInfo(typeName);
            if(signature.resultType==NativeType::Dynamic&&canonicalWord(typeName)!="string"&&canonicalWord(typeName)!="void")
                throw CompileError(lineMessage(result.line,"Function donus imzasi int/uint, float/double, bool, string veya void olmali."));
            signature.resultBits=integer.valid?integer.bits:signature.resultType==NativeType::Bool?1:64;
            signature.resultUnsigned=integer.valid&&integer.isUnsigned;
            if(is(Kind::Comma))
                throw CompileError(lineMessage(token().line,"Function donus turunden sonra baska tur yazilamaz."));
        }
        expect(Kind::Greater,"Function imzasi '>' ile kapanmali.");
        (void)line;
        return signature;
    }
    std::string parseType() {
        parsedFunctionTypeSignature.reset();
        Token type=expectName("Tur adi bekleniyordu.");std::string t=nativeTypeName(type.text);
        if (integerWidthCandidate(t) && !integerTypeInfo(t).valid)
            throw CompileError(lineMessage(type.line, "Tamsayi bit genisligi 4 ile 64 arasinda olmali (ornek: i4, u16, int32, uint64)."));
        if(canonicalWord(t)=="function"&&is(Kind::Less))parsedFunctionTypeSignature=parseFunctionTypeArguments(type.line);
        else if(match(Kind::Less)){while(!is(Kind::Greater)&&!is(Kind::End))take();expect(Kind::Greater,"Generic tur '>' ile bitmeli.");}
        return t;
    }
    bool startsDeclaration() const {
        size_t at=pos;if(tokens[at].kind==Kind::Identifier&&canonicalWord(tokens[at].text)=="const")++at;
        if(at>=tokens.size()||tokens[at].kind!=Kind::Identifier)return false;
        std::string t=canonicalWord(tokens[at].text);
        const bool builtinType=valueType(t)!=NativeType::Dynamic||integerWidthCandidate(t)||t=="string"||t=="auto"||t=="list"||t=="flowlist";
        if(builtinType) {
            size_t next=at+1;
            if((t=="list"||t=="flowlist"||t=="function")&&next<tokens.size()&&tokens[next].kind==Kind::Less) {
                while(next<tokens.size()&&tokens[next].kind!=Kind::Greater&&tokens[next].kind!=Kind::Newline)++next;
                if(next<tokens.size()&&tokens[next].kind==Kind::Greater)++next;
            }
            if(next<tokens.size()&&tokens[next].kind==Kind::LBracket) {
                while(next<tokens.size()&&tokens[next].kind!=Kind::RBracket&&tokens[next].kind!=Kind::Newline)++next;
                if(next<tokens.size()&&tokens[next].kind==Kind::RBracket)++next;
            }
            return next<tokens.size()&&tokens[next].kind==Kind::Identifier;
        }
        const std::string customType=cppIdentifier(tokens[at].text);
        const bool knownType=classNames.contains(customType) || structDefinitions.contains(customType) ||
            (!modulePrefix.empty() && (classNames.contains(modulePrefix+"."+customType) || structDefinitions.contains(modulePrefix+"."+customType)));
        return knownType&&at+1<tokens.size()&&tokens[at+1].kind==Kind::Identifier;
    }
    std::string envSet(const std::string& env,const std::string& name,const std::string& value) {
        callVoid("pts_env_set","i8* "+env+", "+cstrArg(name)+", i8* "+value);return value;
    }
    std::string functionPointer(const std::string& symbol) { return "bitcast (i8* (i8*, i8**, i32)* @"+symbol+" to i8*)"; }
    std::vector<Parameter> parameterDefinitions() {
        expect(Kind::LParen,"Fonksiyon parametreleri '(' ile baslamali.");
        std::vector<Parameter> params;
        while(!is(Kind::RParen)&&!is(Kind::End)) {
            Token first=expectName("Parametre adi veya turu bekleniyordu.");
            Parameter parameter;
            if(match(Kind::Colon)) {
                parameter.name=cppIdentifier(first.text);
                const std::string typeName=parseType();
                parameter.type=valueType(typeName);
                if(parameter.type==NativeType::Function&&parsedFunctionTypeSignature) {
                    parameter.callbackSignature=*parsedFunctionTypeSignature;
                    parameter.hasCallbackSignature=true;
                }
                const auto integer=integerTypeInfo(typeName);
                if(integer.valid){parameter.bits=integer.bits;parameter.isUnsigned=integer.isUnsigned;}
            } else {
                const std::string typeName=nativeTypeName(first.text);
                const std::string canonical=canonicalWord(first.text);
                const bool typePrefix=valueType(typeName)!=NativeType::Dynamic || canonical=="string" || canonical=="auto" || canonical=="list" || canonical=="flowlist" || classNames.contains(cppIdentifier(first.text));
                if(typePrefix) {
                    parsedFunctionTypeSignature.reset();
                    if(canonical=="function"&&is(Kind::Less))parsedFunctionTypeSignature=parseFunctionTypeArguments(first.line);
                    else if(match(Kind::Less)){while(!is(Kind::Greater)&&!is(Kind::End))take();expect(Kind::Greater,"Generic tur '>' ile bitmeli.");}
                    parameter.name=cppIdentifier(expectName("Tur adindan sonra parametre adi bekleniyordu.").text);
                    parameter.type=valueType(typeName);
                    if(parameter.type==NativeType::Function&&parsedFunctionTypeSignature) {
                        parameter.callbackSignature=*parsedFunctionTypeSignature;
                        parameter.hasCallbackSignature=true;
                    }
                    const auto integer=integerTypeInfo(typeName);
                    if(integer.valid){parameter.bits=integer.bits;parameter.isUnsigned=integer.isUnsigned;}
                } else parameter.name=cppIdentifier(first.text);
            }
            params.push_back(std::move(parameter));
            if(!match(Kind::Comma))break;
        }
        expect(Kind::RParen,"Fonksiyon parametreleri ')' ile kapanmali.");
        return params;
    }
    std::string addFunction(const std::string& name,const std::vector<Parameter>& params,bool method,bool colon=true,
        NativeType resultType=NativeType::Dynamic,int resultBits=64,bool resultUnsigned=false) {
        const std::string wrapperSymbol="pts_fn_"+std::to_string(functionIndex++);
        const std::string nativeSymbol="pts_native_fn_"+std::to_string(functionIndex);
        bool useNativeAbi=!method&&resultType!=NativeType::Dynamic;
        for(const auto& parameter:params)if(parameter.type==NativeType::Dynamic||parameter.type==NativeType::Function)useNativeAbi=false;
        std::string registeredName;
        if (!method) {
            registeredName = modulePrefix.empty() ? name : modulePrefix + "." + name;
            if (!directFunctionSymbols.try_emplace(registeredName, wrapperSymbol).second)
                throw CompileError(lineMessage(token().line, "Ayni modulde fonksiyon adi tekrar tanimlanamaz: " + registeredName));
            functionSignatures[registeredName]=signatureOf(params,resultType,resultBits,resultUnsigned);
            if(useNativeAbi)directNativeFunctions.emplace(registeredName,
                DirectNativeFunction{nativeSymbol,params,resultType,resultBits,resultUnsigned,true});
            functionNeedsEnvironment[registeredName] = true;
        }
        auto* outer=current;FunctionContext fn;current=&fn;fn.start("entry");fn.directNative=useNativeAbi;
        fn.returnType=resultType;fn.returnBits=resultBits;fn.returnUnsigned=resultUnsigned;
        for(size_t i=0;i<params.size();++i){
            const std::string type=llvmType(params[i].type,params[i].bits);
            const bool pinnedBox=params[i].type==NativeType::Dynamic;
            const std::string slot=fn.addLocal(params[i].name,params[i].type,params[i].bits,params[i].isUnsigned,type,pinnedBox);
            if(pinnedBox)fn.dynamicLocals.insert(params[i].name);
            if(useNativeAbi)fn.line("store "+type+" %arg"+std::to_string(i)+", "+type+"* "+slot);
            else {
                std::string arg=fn.temp();fn.line(arg+" = call i8* @pts_arg(i8** %args, i32 "+std::to_string(i)+")");
                if(pinnedBox)fn.line("call void @pts_box_pin(i8* "+arg+")");
                const std::string value=params[i].type==NativeType::Function?arg:
                    convertValue(NativeExpr{NativeExpr::Kind::Value,arg,{},{}} ,params[i].type,params[i].bits,params[i].isUnsigned);
                fn.line("store "+type+" "+value+", "+type+"* "+slot);
            }
            if(params[i].type==NativeType::Function&&params[i].hasCallbackSignature) {
                auto& local=fn.locals.at(params[i].name);
                local.functionSignature=params[i].callbackSignature;
                local.hasFunctionSignature=true;
            }
        }
        if(method) { auto self=call("pts_env_get","i8* %parent, "+cstrArg("self"));envSet("%env","self",self); }
        if(name=="main" && !method && modulePrefix.empty())mainSymbol=wrapperSymbol;
        parseBody(colon,[&](){
            if(!current->terminated){
                current->releasePinnedBoxes();
                if(useNativeAbi)current->line("ret "+llvmType(resultType,resultBits)+" "+
                    (resultType==NativeType::Int?"0":resultType==NativeType::Float?"0.0":"false"));
                else current->line("ret i8* null");
                current->terminated=true;
            }
        });
        current->body << "}\n";

        if(useNativeAbi) {
            std::string body=fn.bodyWithAllocas();
            const bool needsEnvironment = body.find("%env") != std::string::npos;
            if (!registeredName.empty()) {
                functionNeedsEnvironment[registeredName] = needsEnvironment;
                auto native = directNativeFunctions.find(registeredName);
                if (native != directNativeFunctions.end()) native->second.needsParentEnvironment = needsEnvironment;
            }
            if(needsEnvironment) {
                const size_t firstLine=body.find('\n');
                if(firstLine!=std::string::npos)body.insert(firstLine+1,"  %env = call i8* @pts_env_create(i8* %parent, i8* null)\n");
            }
            std::ostringstream signature;
            signature<<"define internal "<<(releaseOptimizations?"fastcc ":"")
                     <<llvmType(resultType,resultBits)<<" @"<<nativeSymbol<<"(";
            if(needsEnvironment)signature<<"i8* %parent";
            for(size_t i=0;i<params.size();++i)signature<<(i||needsEnvironment?", ":"")<<llvmType(params[i].type,params[i].bits)<<" %arg"<<i;
            const bool tinyLeaf=body.size()<=1024&&body.find("call ")==std::string::npos;
            signature<<") "<<(tinyLeaf?"alwaysinline":"inlinehint")<<" {\n"<<body;
            functionsIR<<signature.str();

            std::ostringstream wrapper;
            wrapper<<"define internal i8* @"<<wrapperSymbol<<"(i8* %parent, i8** %args, i32 %argc) {\nentry:\n";
            std::string nativeArguments=needsEnvironment?"i8* %parent":std::string{};
            for(size_t i=0;i<params.size();++i) {
                const auto& parameter=params[i];
                const std::string boxed="%boxed_arg"+std::to_string(i);
                const std::string raw="%raw_arg"+std::to_string(i);
                const std::string source=parameter.type==NativeType::Int?(parameter.isUnsigned?"pts_as_uint":"pts_as_int"):
                    parameter.type==NativeType::Float?"pts_as_float":"pts_as_bool";
                wrapper<<"  "<<boxed<<" = call i8* @pts_arg(i8** %args, i32 "<<i<<")\n";
                wrapper<<"  "<<raw<<" = call "<<llvmType(parameter.type,parameter.type==NativeType::Int?64:parameter.bits)
                       <<" @"<<source<<"(i8* "<<boxed<<")\n";
                std::string nativeValue=raw;
                if(parameter.type==NativeType::Int&&parameter.bits<64) {
                    nativeValue="%native_arg"+std::to_string(i);
                    wrapper<<"  "<<nativeValue<<" = trunc i64 "<<raw<<" to "<<llvmType(NativeType::Int,parameter.bits)<<"\n";
                }
                if(!nativeArguments.empty())nativeArguments+=", ";
                nativeArguments+=llvmType(parameter.type,parameter.bits)+" "+nativeValue;
            }
            const std::string nativeResult="%native_result";
            wrapper<<"  "<<nativeResult<<" = call "<<(releaseOptimizations?"fastcc ":"")
                   <<llvmType(resultType,resultBits)<<" @"<<nativeSymbol<<"("<<nativeArguments<<")\n";
            const std::string boxedResult="%boxed_result";
            std::string resultArgument=llvmType(resultType,resultBits)+" "+nativeResult;
            if(resultType==NativeType::Int) {
                if(resultBits<64) {
                    const std::string wide="%wide_result";
                    wrapper<<"  "<<wide<<" = "<<(resultUnsigned?"zext ":"sext ")<<llvmType(NativeType::Int,resultBits)<<" "<<nativeResult<<" to i64\n";
                    resultArgument="i64 "+wide;
                }
                wrapper<<"  "<<boxedResult<<" = call i8* @"<<(resultUnsigned?"pts_uint":"pts_int")<<"("<<resultArgument<<")\n";
            } else if(resultType==NativeType::Float)
                wrapper<<"  "<<boxedResult<<" = call i8* @pts_float("<<resultArgument<<")\n";
            else wrapper<<"  "<<boxedResult<<" = call i8* @pts_bool("<<resultArgument<<")\n";
            wrapper<<"  ret i8* "<<boxedResult<<"\n}\n";
            functionsIR<<wrapper.str();
        } else {
            std::string body=fn.bodyWithAllocas();
            const bool needsEnvironment=body.find("%env")!=std::string::npos;
            if (!registeredName.empty()) functionNeedsEnvironment[registeredName]=needsEnvironment;
            if(name=="main"&&!method&&modulePrefix.empty())mainNeedsEnvironment=needsEnvironment;
            if(needsEnvironment) {
                const size_t firstLine=body.find('\n');
                if(firstLine!=std::string::npos)body.insert(firstLine+1,"  %env = call i8* @pts_env_create(i8* %parent, i8* null)\n");
            }
            std::ostringstream signature;signature<<"define internal i8* @"<<wrapperSymbol<<"(i8* %parent, i8** %args, i32 %argc) {\n"<<body;
            functionsIR<<signature.str();
        }
        current=outer;
        if(method)callVoid("pts_register_method",cstrArg(currentClass)+", "+cstrArg(name)+", i8* "+functionPointer(wrapperSymbol));
        else functionRegistrationCandidates.emplace_back(registeredName,wrapperSymbol);
        return wrapperSymbol;
    }
    void parseClass() {
        take();Token name=expectName("Sinif adi bekleniyordu.");std::string className=cppIdentifier(name.text);if(!modulePrefix.empty())className=modulePrefix+"."+className;classNames.insert(className);
        classFieldIndices.try_emplace(className);
        callVoid("pts_register_class",cstrArg(className));expect(Kind::Colon,"Sinif tanimi ':' ile bitmeli.");while(is(Kind::Newline))take();expect(Kind::Indent,"Sinif govdesi girintili olmali.");
        std::string old=currentClass;currentClass=className;
        while(!is(Kind::Dedent)&&!is(Kind::End)){
            skipLines();if(is(Kind::Dedent)||is(Kind::End))break;
            if(word("fn")){parseFunction(true);continue;}
            if(startsDeclaration()){
                matchWord("const");const std::string fieldType=parseType();const IntegerTypeInfo integer=integerTypeInfo(fieldType);Token field=expectName("Alan adi bekleniyordu.");std::string value;
                const std::string fieldName=cppIdentifier(field.text);auto& fieldIndices=classFieldIndices[className];if(fieldIndices.contains(fieldName))throw CompileError(lineMessage(field.line,"Ayni sinifta alan adi tekrar edemez: "+fieldName));fieldIndices.emplace(fieldName,static_cast<int>(fieldIndices.size()));
                classFieldTypes[className][fieldName]=NativeFieldType{valueType(fieldType),integer.valid?integer.bits:64,integer.valid&&integer.isUnsigned};
                if(match(Kind::Assign)){auto* save=current;current=&initFunction;NativeExpr initial=expression();value=valueOf(initial);current=save;}
                else {auto* save=current;current=&initFunction;value=call("pts_null","");current=save;}
                const NativeType nativeType=valueType(fieldType);const int nativeKind=nativeType==NativeType::Int?1:nativeType==NativeType::Float?2:nativeType==NativeType::Bool?3:0;
                callVoid("pts_class_field",cstrArg(className)+", "+cstrArg(fieldName)+", i8* "+value+", i32 "+std::to_string(integer.valid?integer.bits:0)+", i1 "+(integer.isUnsigned?"true":"false")+", i32 "+std::to_string(nativeKind));endLine();continue;
            }
            throw CompileError(lineMessage(token().line,"Sinif govdesinde yalnizca alanlar ve fn metotlari destekleniyor."));
        }
        expect(Kind::Dedent,"Sinif govdesi kapatilamadi.");currentClass=old;
    }
    void parseStruct() {
        take();
        Token name=expectName("Struct adi bekleniyordu.");
        std::string structName=cppIdentifier(name.text);
        if(!modulePrefix.empty())structName=modulePrefix+"."+structName;
        if(classNames.contains(structName)||structDefinitions.contains(structName))
            throw CompileError(lineMessage(name.line,"Bu tur adi daha once tanimlanmis: "+structName));
        expect(Kind::Colon,"Struct tanimi ':' ile bitmeli.");
        while(is(Kind::Newline))take();
        expect(Kind::Indent,"Struct govdesi girintili olmali.");
        std::vector<NativeStructFieldDefinition> fields;
        std::unordered_set<std::string> fieldNames;
        while(!is(Kind::Dedent)&&!is(Kind::End)) {
            skipLines();
            if(is(Kind::Dedent)||is(Kind::End))break;
            if(!startsDeclaration())
                throw CompileError(lineMessage(token().line,"Struct govdesinde yalnizca sayisal/bool alanlar destekleniyor."));
            matchWord("const");
            const std::string typeName=parseType();
            const NativeType type=valueType(typeName);
            const IntegerTypeInfo integer=integerTypeInfo(typeName);
            Token field=expectName("Struct alani adi bekleniyordu.");
            const std::string fieldName=cppIdentifier(field.text);
            if(!fieldNames.insert(fieldName).second)
                throw CompileError(lineMessage(field.line,"Struct alan adi tekrar edemez: "+fieldName));
            if(type!=NativeType::Int&&type!=NativeType::Float&&type!=NativeType::Bool)
                throw CompileError(lineMessage(field.line,"Struct alanlari int, float/double veya bool olmali."));
            NativeStructFieldDefinition definition;
            definition.name=fieldName;
            definition.type=type;
            definition.bits=integer.valid?integer.bits:64;
            definition.isUnsigned=integer.valid&&integer.isUnsigned;
            definition.defaultValue=type==NativeType::Bool?"false":type==NativeType::Float?"0.0":"0";
            if(match(Kind::Assign)) {
                bool negative=match(Kind::Minus);
                const Token value=take();
                if(type==NativeType::Bool) {
                    if(negative||value.kind!=Kind::Identifier||
                       (canonicalWord(value.text)!="true"&&canonicalWord(value.text)!="false"))
                        throw CompileError(lineMessage(value.line,"Bool struct varsayilani true veya false olmali."));
                    definition.defaultValue=canonicalWord(value.text)=="true"?"true":"false";
                } else if(value.kind==Kind::Number) {
                    if(type==NativeType::Int&&value.text.find('.')!=std::string::npos)
                        throw CompileError(lineMessage(value.line,"Tamsayi struct alanina tam sayi sabiti verin."));
                    definition.defaultValue=(negative?"-":"")+value.text;
                } else throw CompileError(lineMessage(value.line,"Struct varsayilanlari sabit sayi veya bool olmali."));
            }
            fields.push_back(std::move(definition));
            endLine();
        }
        expect(Kind::Dedent,"Struct govdesi kapatilamadi.");
        structNames.insert(structName);
        structDefinitions.emplace(structName,std::move(fields));
    }
    std::shared_ptr<NativeStructInstance> copyStructValue(const NativeExpr& source, bool clone) {
        const std::shared_ptr<NativeStructInstance> sourceInstance = source.structInstance;
        if (!sourceInstance)
            throw CompileError(lineMessage(token().line,"Struct degeri bekleniyordu."));
        if (!clone) return sourceInstance;
        auto definition=structDefinitions.find(sourceInstance->typeName);
        if(definition==structDefinitions.end())
            throw CompileError(lineMessage(token().line,"Struct tanimi bulunamadi: "+sourceInstance->typeName));
        auto copy=std::make_shared<NativeStructInstance>();
        copy->typeName=sourceInstance->typeName;
        copy->temporary=false;
        for(const auto& field:definition->second) {
            const auto from=sourceInstance->fields.find(field.name);
            if(from==sourceInstance->fields.end())
                throw CompileError(lineMessage(token().line,"Struct alaninda kopyalama hatasi: "+field.name));
            const std::string type=llvmType(field.type,field.bits);
            const std::string internalName="__pts_struct_copy_"+std::to_string(current->local)+"_"+field.name;
            const std::string slot=current->addLocal(internalName,field.type,field.bits,field.isUnsigned,type);
            const std::string value=current->temp();
            current->line(value+" = load "+type+", "+type+"* "+from->second.slot);
            current->line("store "+type+" "+value+", "+type+"* "+slot);
            copy->fields.emplace(field.name,NativeStructFieldSlot{slot,field.type,field.bits,field.isUnsigned});
        }
        return copy;
    }
    void parseProcess() {
        take();Token name=expectName("Surec adi bekleniyordu.");std::string className=cppIdentifier(name.text);classNames.insert(className);classFieldIndices.try_emplace(className);callVoid("pts_register_class",cstrArg(className));
        expect(Kind::LBracket,"Surec alanlari '[' ile baslamali.");if(!is(Kind::RBracket)){do{Token field=expectName("Alan adi bekleniyordu.");expect(Kind::Colon,"Alan adi ile tur arasinda ':' olmali.");const std::string fieldName=cppIdentifier(field.text);auto& fieldIndices=classFieldIndices[className];if(fieldIndices.contains(fieldName))throw CompileError(lineMessage(field.line,"Ayni surecte alan adi tekrar edemez: "+fieldName));fieldIndices.emplace(fieldName,static_cast<int>(fieldIndices.size()));const std::string fieldType=parseType();const IntegerTypeInfo integer=integerTypeInfo(fieldType);const NativeType nativeType=valueType(fieldType);classFieldTypes[className][fieldName]=NativeFieldType{nativeType,integer.valid?integer.bits:64,integer.valid&&integer.isUnsigned};std::string nil=call("pts_null","");const int nativeKind=nativeType==NativeType::Int?1:nativeType==NativeType::Float?2:nativeType==NativeType::Bool?3:0;callVoid("pts_class_field",cstrArg(className)+", "+cstrArg(fieldName)+", i8* "+nil+", i32 "+std::to_string(integer.valid?integer.bits:0)+", i1 "+(integer.isUnsigned?"true":"false")+", i32 "+std::to_string(nativeKind));}while(match(Kind::Comma));}
        expect(Kind::RBracket,"Surec alanlari ']' ile kapanmali.");endLine();expect(Kind::Indent,"Surec govdesi girintili olmali.");std::string old=currentClass;currentClass=className;
        while(!is(Kind::Dedent)&&!is(Kind::End)){skipLines();if(is(Kind::Dedent)||is(Kind::End))break;if(word("action")){parseAction();continue;}if(word("fn")){parseFunction(true);continue;}throw CompileError(lineMessage(token().line,"Surec govdesinde eylem/action tanimi bekleniyordu."));}
        expect(Kind::Dedent,"Surec govdesi kapatilamadi.");currentClass=old;
    }
    void parseAction() {
        take();Token name=expectName("Eylem adi bekleniyordu.");std::vector<Parameter> params;if(is(Kind::LParen))params=parameterDefinitions();
        if(std::any_of(params.begin(),params.end(),[](const Parameter& parameter){return parameter.type==NativeType::Function;}))
            throw CompileError(lineMessage(name.line,"Action/eylem callback parametreleri bu surumde desteklenmiyor; fn kullanin."));
        const std::string symbol="pts_fn_"+std::to_string(functionIndex++);auto* outer=current;FunctionContext fn;current=&fn;fn.start("entry");fn.line("%env = call i8* @pts_env_create(i8* %parent, i8* null)");
        auto self=call("pts_env_get","i8* %parent, "+cstrArg("self"));envSet("%env","self",self);fn.env="%env";
        for(size_t i=0;i<params.size();++i){auto arg=call("pts_arg","i8** %args, i32 "+std::to_string(i));if(params[i].type!=NativeType::Dynamic){const std::string type=llvmType(params[i].type,params[i].bits);const std::string slot=fn.addLocal(params[i].name,params[i].type,params[i].bits,params[i].isUnsigned,type);const std::string value=convertValue(NativeExpr{NativeExpr::Kind::Value,arg,{},{}},params[i].type,params[i].bits,params[i].isUnsigned);fn.line("store "+type+" "+value+", "+type+"* "+slot);}else { envSet("%env",params[i].name,arg);fn.dynamicLocals.insert(params[i].name); }}
        parseBody(false,[&](){if(!current->terminated){current->line("ret i8* null");current->terminated=true;}});fn.body<<"}\n";functionsIR<<"define internal i8* @"<<symbol<<"(i8* %parent, i8** %args, i32 %argc) {\n"<<fn.bodyWithAllocas();current=outer;callVoid("pts_register_method",cstrArg(currentClass)+", "+cstrArg(cppIdentifier(name.text))+", i8* "+functionPointer(symbol));
    }
    void parseFunction(bool method=false) {
        take();Token name=expectName("Fonksiyon adi bekleniyordu.");auto params=parameterDefinitions();
        if(method&&std::any_of(params.begin(),params.end(),[](const Parameter& parameter){return parameter.type==NativeType::Function;}))
            throw CompileError(lineMessage(name.line,"Method callback parametreleri bu surumde desteklenmiyor."));
        NativeType resultType=NativeType::Dynamic;int resultBits=64;bool resultUnsigned=false;
        if(match(Kind::Arrow)) {
            const std::string typeName=parseType();resultType=valueType(typeName);
            if(resultType==NativeType::Function)
                throw CompileError(lineMessage(name.line,"Function pointer donus degerleri henuz desteklenmiyor."));
            const auto integer=integerTypeInfo(typeName);
            if(integer.valid){resultBits=integer.bits;resultUnsigned=integer.isUnsigned;}
        }
        addFunction(cppIdentifier(name.text),params,method,true,resultType,resultBits,resultUnsigned);
    }
    fs::path resolveModule(const std::string& module) {
        fs::path relative;
        size_t start=0;
        while(start<module.size()) { size_t dot=module.find('.',start);relative/=module.substr(start,dot==std::string::npos?std::string::npos:dot-start);if(dot==std::string::npos)break;start=dot+1; }
        fs::path beside=sourcePath.parent_path()/(relative.wstring()+L".pts");
        if(fs::exists(beside))return fs::weakly_canonical(beside);
        fs::path inModules=sourcePath.parent_path()/L"modules"/(relative.wstring()+L".pts");
        if(fs::exists(inModules))return fs::weakly_canonical(inModules);
        // ben yaptım opencode - 2026-11-14 - paket modulu cozumleme
        const fs::path packaged=installedPackageModule(module);
        if(!packaged.empty())return packaged;
        throw CompileError(lineMessage(token().line,".pts modul dosyasi bulunamadi: "+module));
    }
    void parseModuleSource(const fs::path& path,const std::string& prefix) {
        const std::wstring key=fs::weakly_canonical(path).wstring();
        if(importedModules.contains(key))return;
        importedModules.insert(key);
        std::ifstream input(path,std::ios::binary);
        if(!input)throw CompileError("Modul dosyasi acilamadi: "+path.string());
        std::string source((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
        auto oldTokens=std::move(tokens);size_t oldPos=pos;fs::path oldPath=sourcePath;std::string oldPrefix=modulePrefix;
        tokens=Lexer(source).scan();pos=0;sourcePath=path;modulePrefix=prefix;preloadConfiguredScalars();
        try {
            while(!is(Kind::End)){skipLines();if(is(Kind::End))break;if(is(Kind::Indent)||is(Kind::Dedent))throw CompileError(lineMessage(token().line,"Beklenmeyen girinti."));statement();}
        } catch(...) { tokens=std::move(oldTokens);pos=oldPos;sourcePath=oldPath;modulePrefix=oldPrefix;throw; }
        tokens=std::move(oldTokens);pos=oldPos;sourcePath=oldPath;modulePrefix=oldPrefix;
    }
    void parseImport() {
        take();std::vector<std::pair<fs::path,std::string>> toLoad;
        auto moduleSegment=[&](){std::string segment=cppIdentifier(expectName("Modul adi bekleniyordu.").text);while(is(Kind::Minus)&&pos+1<tokens.size()&&tokens[pos+1].kind==Kind::Identifier){take();segment+="-"+cppIdentifier(expectName("Modul adi bekleniyordu.").text);}return segment;};
        do {
            std::string module=moduleSegment();while(match(Kind::Dot))module+="."+moduleSegment();std::string alias;if(matchWord("as"))alias=cppIdentifier(expectName("as sonrasinda ad bekleniyordu.").text);
            if(module=="math.random"){std::string a=alias.empty()?"random":alias;aliases[a]="random";namespaceRoots.insert(a);}
            else if(module=="math"||module=="Math"||module=="Matematik"||module=="system"||module=="System"||module=="Sistem"||module=="String"||module=="Metin"||module=="Engine"||module=="Engine3D"||module=="Physics3D"||module=="UI"||module=="Color"||module=="File"||module=="Dosya"||module=="Console"||module=="Konsol"||module=="Arena"||module=="Scheduler"||isStdlibNamespace(module)) { const std::string canonical=(module=="math"?"Math":module=="system"?"System":module=="Sistem"?"System":module);const std::string importedName=alias.empty()?(module=="math"?"math":module=="system"?"system":module):alias;namespaceRoots.insert(importedName);if(importedName!=canonical)aliases[importedName]=canonical; }
            else {std::string prefix=module;size_t dot=prefix.find('.');while(dot!=std::string::npos){namespaceRoots.insert(prefix.substr(0,dot));dot=prefix.find('.',dot+1);}namespaceRoots.insert(prefix);if(!alias.empty()){namespaceRoots.insert(alias);aliases[alias]=prefix;}toLoad.emplace_back(resolveModule(module),prefix);}
        }while(match(Kind::Comma));endLine();
        if(current!=&initFunction)throw CompileError(lineMessage(token().line,"import yalnizca dosyanin ust seviyesinde kullanilabilir."));
        for(const auto& [path,prefix]:toLoad)parseModuleSource(path,prefix);
    }
    void preloadConfiguredScalars() {
        int indentation=0;
        for(std::size_t i=0;i<tokens.size();++i) {
            if(tokens[i].kind==Kind::Indent) { ++indentation;continue; }
            if(tokens[i].kind==Kind::Dedent) { indentation=std::max(0,indentation-1);continue; }
            if(indentation!=0||tokens[i].kind!=Kind::Identifier)continue;
            if(canonicalWord(tokens[i].text)=="const") {
                std::size_t next=i+1;
                if(next>=tokens.size()||tokens[next].kind!=Kind::Identifier)continue;
                ++next;
                if(next<tokens.size()&&tokens[next].kind==Kind::Less) {
                    while(next<tokens.size()&&tokens[next].kind!=Kind::Greater&&tokens[next].kind!=Kind::Newline)++next;
                    if(next<tokens.size()&&tokens[next].kind==Kind::Greater)++next;
                }
                if(next<tokens.size()&&tokens[next].kind==Kind::Identifier) {
                    const std::string name=cppIdentifier(tokens[next].text);
                    globalConstNames.insert(name);
                    globalNames.insert(name);
                }
                continue;
            }
            if(canonicalWord(tokens[i].text)!="configure"||i+3>=tokens.size())continue;
            const Token& key=tokens[i+1];
            if(key.kind!=Kind::Identifier||tokens[i+2].kind!=Kind::Arrow)continue;
            if(canonicalWord(key.text)=="module")continue;
            const Token& value=tokens[i+3];
            const std::string name=cppIdentifier(key.text);
            configuredScalars.erase(name);
            if(value.kind==Kind::Number) {
                if(value.text.find('.')!=std::string::npos)
                    configuredScalars[name]=NativeExpr{NativeExpr::Kind::Value,value.text,{}, {},NativeType::Float};
                else {
                    const std::int64_t number=std::stoll(value.text);
                    configuredScalars[name]=NativeExpr{NativeExpr::Kind::Value,std::to_string(number),{}, {},NativeType::Int,64,false};
                }
            } else if(value.kind==Kind::Identifier&&(canonicalWord(value.text)=="true"||canonicalWord(value.text)=="false")) {
                const bool boolean=canonicalWord(value.text)=="true";
                configuredScalars[name]=NativeExpr{NativeExpr::Kind::Value,boolean?"true":"false",{}, {},NativeType::Bool};
            }
        }
    }
    void parseConfigure() {
        take();Token key=expectName("Yapilandirma adi bekleniyordu.");expect(Kind::Arrow,"Yapilandirma icin '->' bekleniyordu.");
        if(canonicalWord(key.text)=="module") { do { std::string n=cppIdentifier(expectName("Modul adi bekleniyordu.").text);if(n=="Engine"||n=="Engine3D"||n=="Physics3D"||n=="UI"||n=="Color"||n=="File"||n=="Dosya"||n=="Console"||n=="Konsol"||n=="Math"||n=="Matematik"||n=="System"||n=="Sistem"||n=="String"||n=="Metin"||n=="Arena"||isStdlibNamespace(n))namespaceRoots.insert(n);else if(n=="math.random"){aliases["random"]="random";namespaceRoots.insert("random");} } while(match(Kind::Comma)); }
        else {
            Token v=take();
            const std::string name=cppIdentifier(key.text);
            std::string val;
            configuredScalars.erase(name);
            if(v.kind==Kind::String)val=stringValue(v.text);
            else if(v.kind==Kind::Number) {
                if(v.text.find('.')!=std::string::npos) {
                    val=call("pts_float","double "+v.text);
                    configuredScalars[name]=NativeExpr{NativeExpr::Kind::Value,v.text,{}, {},NativeType::Float};
                } else {
                    const std::int64_t number=std::stoll(v.text);
                    val=intValue(number);
                    configuredScalars[name]=NativeExpr{NativeExpr::Kind::Value,std::to_string(number),{}, {},NativeType::Int,64,false};
                }
            } else if(v.kind==Kind::Identifier&&(canonicalWord(v.text)=="true"||canonicalWord(v.text)=="false")) {
                const bool value=canonicalWord(v.text)=="true";
                val=boolValue(value);
                configuredScalars[name]=NativeExpr{NativeExpr::Kind::Value,value?"true":"false",{}, {},NativeType::Bool};
            } else if(v.kind==Kind::Identifier)val=stringValue(v.text);
            else throw CompileError(lineMessage(v.line,"Yapilandirma degeri gecersiz."));
            envSet("%global",name,val);
        }
        endLine();
    }
    void parseIf() {
        take();std::string truth=truthValue(expression());std::string yes=current->newLabel("if_yes_"),no=current->newLabel("if_no_"),end=current->newLabel("if_end_");current->line("br i1 "+truth+", label %"+yes+", label %"+no);current->terminated=true;current->start(yes);parseBody(true);current->branch(end);current->start(no);
        if(matchWord("elif")){ // Re-enter the same conditional parser after consuming elif.
            std::string t=truthValue(expression());std::string y=current->newLabel("elif_yes_"),n=current->newLabel("elif_no_");current->line("br i1 "+t+", label %"+y+", label %"+n);current->terminated=true;current->start(y);parseBody(true);current->branch(end);current->start(n);if(matchWord("else")){parseBody(true);current->branch(end);}else current->branch(end);
        } else if(matchWord("else")){parseBody(true);current->branch(end);}else current->branch(end);
        current->start(end);
    }
    void parseWhile() {
        take();std::string conditionLabel=current->newLabel("while_cond_"),bodyLabel=current->newLabel("while_body_"),endLabel=current->newLabel("while_end_");current->branch(conditionLabel);current->start(conditionLabel);std::string t=truthValue(expression());current->line("br i1 "+t+", label %"+bodyLabel+", label %"+endLabel);current->terminated=true;current->start(bodyLabel);current->loops.push_back({endLabel,conditionLabel,createLoopMetadata()});parseBody(true);current->branch(conditionLabel);current->loops.pop_back();current->start(endLabel);
    }
    void parseFor(bool eachPrefix=false) {
        if(!eachPrefix)take();matchWord("each");Token item=expectName("Dongu degiskeni bekleniyordu.");if(!matchWord("in"))throw CompileError(lineMessage(token().line,"for dongusunde in/icin bekleniyordu."));std::string list=valueOf(expression());bool colon=is(Kind::Colon);
        const std::string itemName=cppIdentifier(item.text);
        if(current->isConstLocal(itemName))throw CompileError(lineMessage(item.line,"Dongu degiskeni const yerel degiskeni golgeleyemez: "+itemName));
        std::string iter=call("pts_iter_begin","i8* "+list);std::string cond=current->newLabel("for_cond_"),body=current->newLabel("for_body_"),end=current->newLabel("for_end_");current->branch(cond);current->start(cond);std::string itemValue=call("pts_iter_next","i8* "+iter);std::string has=current->temp();current->line(has+" = icmp ne i8* "+itemValue+", null");current->line("br i1 "+has+", label %"+body+", label %"+end);current->terminated=true;current->start(body);envSet(current->env,itemName,itemValue);current->dynamicLocals.insert(itemName);current->loops.push_back({end,cond,createLoopMetadata()});parseBody(colon);current->branch(cond);current->loops.pop_back();current->start(end);
    }
    void parseEach() { take();Token item=expectName("Dongu degiskeni bekleniyordu.");if(!matchWord("in"))throw CompileError(lineMessage(token().line,"'her' dongusunde 'icin' bekleniyordu."));std::string list=valueOf(expression());const std::string itemName=cppIdentifier(item.text);if(current->isConstLocal(itemName))throw CompileError(lineMessage(item.line,"Dongu degiskeni const yerel degiskeni golgeleyemez: "+itemName));std::string iter=call("pts_iter_begin","i8* "+list);std::string cond=current->newLabel("each_cond_"),body=current->newLabel("each_body_"),end=current->newLabel("each_end_");current->branch(cond);current->start(cond);std::string v=call("pts_iter_next","i8* "+iter);std::string has=current->temp();current->line(has+" = icmp ne i8* "+v+", null");current->line("br i1 "+has+", label %"+body+", label %"+end);current->terminated=true;current->start(body);envSet(current->env,itemName,v);current->dynamicLocals.insert(itemName);current->loops.push_back({end,cond,createLoopMetadata()});parseBody(false);current->branch(cond);current->loops.pop_back();current->start(end); }
    void parseMainFlow() { take();addFunction(modulePrefix.empty()?"main":"main_flow",{},false,false); }
    void parseDeclaration() {
        const bool isConst=matchWord("const");
        const std::string declaredType=parseType();
        const std::optional<NativeFunctionSignature> declaredFunctionSignature=parsedFunctionTypeSignature;
        std::optional<std::size_t> fixedArrayLength;
        if(match(Kind::LBracket)) {
            const Token length=take();
            if(length.kind!=Kind::Number||length.text.find('.')!=std::string::npos)
                throw CompileError(lineMessage(length.line,"Sabit dizi boyutu pozitif ve derleme zamaninda bilinen bir tam sayi olmali."));
            try {
                std::size_t consumed=0;
                const unsigned long long parsed=std::stoull(length.text,&consumed,10);
                if(consumed!=length.text.size()||parsed==0||parsed>8192)
                    throw CompileError(lineMessage(length.line,"Sabit dizi boyutu 1 ile 8192 arasinda olmali."));
                fixedArrayLength=static_cast<std::size_t>(parsed);
            } catch(const CompileError&) { throw; }
              catch(...) { throw CompileError(lineMessage(length.line,"Sabit dizi boyutu gecersiz.")); }
            expect(Kind::RBracket,"Sabit dizi boyutundan sonra ']' bekleniyordu.");
        }
        Token name=expectName("Degisken adi bekleniyordu.");
        const std::string variable=cppIdentifier(name.text);
        if(current!=&initFunction&&current->isConstLocal(variable))
            throw CompileError(lineMessage(name.line,"const yerel degisken golgelenemez veya yeniden tanimlanamaz: "+variable));
        // ben yaptım opencode - 2026-11-11 - sabit dizi baslatma ve const korumasi
        if(fixedArrayLength) {
            if(current==&initFunction)
                throw CompileError(lineMessage(name.line,"Sabit diziler yalnizca fonksiyon icinde tanimlanabilir."));
            const NativeType elementType=valueType(declaredType);
            if(elementType!=NativeType::Int&&elementType!=NativeType::Float&&elementType!=NativeType::Bool)
                throw CompileError(lineMessage(name.line,"Sabit dizi elemanlari int/uint, float/double veya bool olmali."));
            if(current->locals.contains(variable)||current->fixedArrays.contains(variable)||
               current->dynamicLocals.contains(variable)||current->nativeStructLocals.contains(variable))
                throw CompileError(lineMessage(name.line,"Ayni adla baska bir yerel bildirim zaten var: "+variable));
            const IntegerTypeInfo integer=integerTypeInfo(declaredType);
            const int bits=integer.valid?integer.bits:64;
            const bool isUnsigned=integer.valid&&integer.isUnsigned;
            const bool hasInitializer=match(Kind::Assign);
            std::vector<std::string> elements;
            if(hasInitializer) {
                expect(Kind::LBracket,"Sabit dizi baslaticisi '[' ile baslamali.");
                if(!is(Kind::RBracket)) {
                    while(true) {
                        elements.push_back(fixedArrayInitializerElement(elementType,bits,isUnsigned,elements.size()));
                        if(!is(Kind::RBracket)&&!is(Kind::Comma))
                            throw CompileError(lineMessage(token().line,"Sabit dizi baslaticisinda yalnizca derleme zamaninda sabit scalar degerler kullanilabilir."));
                        if(!match(Kind::Comma)) break;
                        if(is(Kind::RBracket))
                            throw CompileError(lineMessage(token().line,"Sabit dizi baslaticisinda sondaki virgul desteklenmiyor."));
                    }
                }
                expect(Kind::RBracket,"Sabit dizi baslaticisi ']' ile kapanmali.");
                if(elements.size()!=*fixedArrayLength)
                    throw CompileError(lineMessage(name.line,"Sabit dizi baslaticisi tam "+std::to_string(*fixedArrayLength)+" eleman icermeli; "+std::to_string(elements.size())+" eleman verildi."));
            } else if(isConst) {
                throw CompileError(lineMessage(name.line,"const sabit dizi ilk degerle baslatilmalidir."));
            }
            const std::string elementLLVM=llvmType(elementType,bits);
            const std::string arrayLLVM="["+std::to_string(*fixedArrayLength)+" x "+elementLLVM+"]";
            const std::string slot="%local"+std::to_string(current->local++);
            current->allocas<<"  "<<slot<<" = alloca "<<arrayLLVM<<'\n';
            if(hasInitializer) {
                for(std::size_t i=0;i<elements.size();++i) {
                    const std::string pointer=current->temp();
                    current->line(pointer+" = getelementptr inbounds "+arrayLLVM+", "+arrayLLVM+"* "+slot+", i64 0, i64 "+std::to_string(i));
                    current->line("store "+elementLLVM+" "+elements[i]+", "+elementLLVM+"* "+pointer);
                }
            } else {
                current->line("store "+arrayLLVM+" zeroinitializer, "+arrayLLVM+"* "+slot);
            }
            current->fixedArrays.emplace(variable,NativeFixedArray{slot,elementType,bits,isUnsigned,*fixedArrayLength});
            current->staticObjectTypes.erase(variable);
            current->dynamicLocals.erase(variable);
            if(isConst)current->constBindings.insert(variable);
            endLine();
            return;
        }
        NativeExpr initial{NativeExpr::Kind::Value,"null",{}, {}};
        const bool hasInitial=match(Kind::Assign);
        if(hasInitial)initial=expression();
        std::string resolvedStructType=declaredType;
        if(!modulePrefix.empty()&&structDefinitions.contains(modulePrefix+"."+resolvedStructType))
            resolvedStructType=modulePrefix+"."+resolvedStructType;
        auto declaredStruct=structDefinitions.find(resolvedStructType);
        const bool structInitializer=hasInitial && (initial.kind==NativeExpr::Kind::StructValue || initial.structInstance);
        if(structInitializer || declaredStruct!=structDefinitions.end()) {
            if(isConst&&current==&initFunction)
                throw CompileError(lineMessage(name.line,"Dosya duzeyinde const struct degeri bu surumde desteklenmiyor."));
            if(structInitializer && declaredType!="auto" && declaredType!=initial.name &&
               !(initial.structInstance && declaredType==initial.structInstance->typeName))
                throw CompileError(lineMessage(name.line,"Struct atamasinda turler uyusmuyor."));
            if(declaredStruct!=structDefinitions.end() && hasInitial &&
               !(initial.kind==NativeExpr::Kind::StructValue && initial.name==resolvedStructType) &&
               !(initial.structInstance && initial.structInstance->typeName==resolvedStructType))
                throw CompileError(lineMessage(name.line,"Struct atamasinda turler uyusmuyor."));
            if(declaredStruct==structDefinitions.end() && !structInitializer)
                throw CompileError(lineMessage(name.line,"Struct degiskeni ilk degerle baslatilmali."));
            if(structInitializer) {
                const bool clone=initial.kind==NativeExpr::Kind::Variable;
                auto instance=copyStructValue(initial,clone);
                instance->temporary=false;
                current->nativeStructLocals[variable]=std::move(instance);
            } else {
                NativeExpr typeExpr{NativeExpr::Kind::StructType,{},resolvedStructType,{}};
                NativeExpr instance=invoke(std::move(typeExpr),{});
                instance.structInstance->temporary=false;
                current->nativeStructLocals[variable]=std::move(instance.structInstance);
            }
            current->staticObjectTypes.erase(variable);
            if(isConst)current->constBindings.insert(variable);
            endLine();
            return;
        }
        if(isConst&&!hasInitial)
            throw CompileError(lineMessage(name.line,"const degisken ilk degerle baslatilmalidir."));
        NativeType type=valueType(declaredType);
        const auto declaredInteger=integerTypeInfo(declaredType);
        int bits=declaredInteger.valid?declaredInteger.bits:64;
        bool isUnsigned=declaredInteger.valid&&declaredInteger.isUnsigned;
        if(declaredType=="auto" && hasInitial) {
            NativeExpr candidate=materialize(initial);
            if(candidate.kind==NativeExpr::Kind::Value && candidate.type!=NativeType::Dynamic) {
                initial=std::move(candidate);type=initial.type;bits=initial.bits;isUnsigned=initial.isUnsigned;
            }
        }
        if(type==NativeType::Function) {
            if(current==&initFunction)
                throw CompileError(lineMessage(name.line,"Function yerelleri yalnizca fonksiyon govdesinde tanimlanabilir."));
            if(!hasInitial)
                throw CompileError(lineMessage(name.line,"Function degiskeni bir fonksiyonla baslatilmalidir."));
            initial=materialize(std::move(initial));
            if(initial.kind!=NativeExpr::Kind::Value||initial.type!=NativeType::Function)
                throw CompileError(lineMessage(name.line,"Function degiskenine yalnizca fonksiyon adresi atanabilir."));
            if(declaredFunctionSignature&&(!initial.hasFunctionSignature||
                functionSignatureKey(*declaredFunctionSignature)!=functionSignatureKey(initial.functionSignature)))
                throw CompileError(lineMessage(name.line,"Function degiskeninin imzasi atanan fonksiyonla uyusmuyor veya dogrulanamiyor."));
            const std::string slot=current->addLocal(variable,NativeType::Function,64,false,"i8*");
            current->line("store i8* "+initial.value+", i8** "+slot);
            auto& local=current->locals.at(variable);
            if(declaredFunctionSignature) {
                local.functionSignature=*declaredFunctionSignature;
                local.hasFunctionSignature=true;
            } else if(initial.hasFunctionSignature) {
                local.functionSignature=initial.functionSignature;
                local.hasFunctionSignature=true;
            }
            if(isConst)current->constBindings.insert(variable);
            current->dynamicLocals.erase(variable);
            current->staticObjectTypes.erase(variable);
            endLine();
            return;
        }
        if(type!=NativeType::Dynamic && current!=&initFunction) {
            const std::string llvmTypeName=llvmType(type,bits);
            const std::string slot=current->addLocal(variable,type,bits,isUnsigned,llvmTypeName);
            const std::string value=hasInitial?convertValue(initial,type,bits,isUnsigned):(type==NativeType::Int?"0":type==NativeType::Float?"0.0":"false");
            current->line("store "+llvmTypeName+" "+value+", "+llvmTypeName+"* "+slot);
        } else if(type==NativeType::Dynamic && current!=&initFunction && hasInitial && !initial.staticClass.empty()) {
            const std::string value=valueOf(initial);
            const std::string dataSlot=current->addPointerDataLocal(variable);
            const std::string data=scalarCall("i8*","pts_native_data","i8* "+value);
            envSet(current->env,variable,value);
            current->line("store i8* "+data+", i8** "+dataSlot);
        } else {
            std::string value;
            if(type!=NativeType::Dynamic) {
                const std::string raw=hasInitial?convertValue(initial,type,bits,isUnsigned):(type==NativeType::Int?"0":type==NativeType::Float?"0.0":"false");
                value=valueOf(NativeExpr{NativeExpr::Kind::Value,raw,{}, {},type,bits,isUnsigned});
            } else value=hasInitial?valueOf(initial):call("pts_null","");
            envSet(current->env,variable,value);
        }
        if(hasInitial&&!initial.staticClass.empty())current->staticObjectTypes[variable]=initial.staticClass;
        else current->staticObjectTypes.erase(variable);
        if(current==&initFunction&&isConst) {
            globalConstNames.insert(variable);
            globalNames.insert(variable);
        } else if(current!=&initFunction&&isConst)current->constBindings.insert(variable);
        if(type==NativeType::Dynamic)current->dynamicLocals.insert(variable);
        else current->dynamicLocals.erase(variable);
        endLine();
    }
    void parseSimple() {
        if(startsDeclaration()){parseDeclaration();return;}
        if(matchWord("break")){if(current->loops.empty())throw CompileError(lineMessage(token().line,"break dongu icinde olmali."));current->branch(current->loops.back().breakLabel);endLine();return;}
        if(matchWord("continue")){if(current->loops.empty())throw CompileError(lineMessage(token().line,"continue dongu icinde olmali."));current->branch(current->loops.back().continueLabel);endLine();return;}
        if(matchWord("return")){
            if(current->directNative) {
                const std::string type=llvmType(current->returnType,current->returnBits);
                const bool hasValue=!is(Kind::Newline)&&!is(Kind::Dedent)&&!is(Kind::End);
                const std::string result=hasValue
                    ? convertValue(expression(),current->returnType,current->returnBits,current->returnUnsigned)
                    : current->returnType==NativeType::Int?"0":current->returnType==NativeType::Float?"0.0":"false";
                current->line("ret "+type+" "+result);
            } else {
                std::string result="null";
                const bool hasValue=!is(Kind::Newline)&&!is(Kind::Dedent)&&!is(Kind::End);
                if(hasValue)result=valueOf(expression());
                if(!current->pinnedBoxSlots.empty()) {
                    if(hasValue)result=call("pts_box_copy","i8* "+result);
                    current->releasePinnedBoxes();
                }
                current->line("ret i8* "+result);
            }
            current->terminated=true;endLine();return;
        }
        NativeExpr lhs=expression();std::string assign;
        if(is(Kind::Assign)||is(Kind::PlusAssign)||is(Kind::MinusAssign)||is(Kind::StarAssign)||is(Kind::SlashAssign)||is(Kind::PercentAssign)||is(Kind::BitAndAssign)||is(Kind::BitOrAssign)||is(Kind::BitXorAssign)||is(Kind::ShiftLeftAssign)||is(Kind::ShiftRightAssign)){
            Token op=take();
            assign=op.kind==Kind::Assign?"=":op.kind==Kind::PlusAssign?"+":op.kind==Kind::MinusAssign?"-":op.kind==Kind::StarAssign?"*":op.kind==Kind::SlashAssign?"/":op.kind==Kind::PercentAssign?"%":op.kind==Kind::BitAndAssign?"&":op.kind==Kind::BitOrAssign?"|":op.kind==Kind::BitXorAssign?"^":op.kind==Kind::ShiftLeftAssign?"<<":">>";
            if(lhs.kind==NativeExpr::Kind::Variable&&current->fixedArrays.contains(lhs.name))
                throw CompileError(lineMessage(op.line,"Sabit dizi degiskeninin tamamina atama yapilamaz; eleman indisi kullanin."));
            if(lhs.kind==NativeExpr::Kind::Variable&&current->isConstLocal(lhs.name))
                throw CompileError(lineMessage(op.line,"const ile tanimlanan yerel degiskene atama yapilamaz: "+lhs.name));
            // ben yaptım opencode - 2026-11-11 - sabit dizi baslatma ve const korumasi
            // ben yaptım codex - REJECT: Dosya duzeyi const, ayni isimdeki yerel sabit diziyi maskeleyince fixedArrays kontrol edilmiyor; yerel yazma yanlislikla reddediliyor.
            if(lhs.kind==NativeExpr::Kind::Member&&!lhs.baseName.empty()&&
               current->fixedArrays.contains(lhs.baseName)&&current->isConstLocal(lhs.baseName))
                throw CompileError(lineMessage(op.line,"const sabit dizi elemanina atama yapilamaz: "+lhs.baseName));
            if(lhs.kind==NativeExpr::Kind::Member&&!lhs.baseName.empty()&&current->isConstLocal(lhs.baseName))
                throw CompileError(lineMessage(op.line,"const ile tanimlanan degerin alani degistirilemez: "+lhs.baseName+"."+lhs.name));
            if(lhs.kind==NativeExpr::Kind::Variable&&globalConstNames.contains(lhs.name)) {
                const bool shadowed=current!=&initFunction&&
                    (current->locals.contains(lhs.name)||current->dynamicLocals.contains(lhs.name)||
                     current->nativeStructLocals.contains(lhs.name)||current->nativeObjectDataLocals.contains(lhs.name));
                if(!shadowed)
                    throw CompileError(lineMessage(op.line,"Dosya duzeyindeki const degiskene atama yapilamaz: "+lhs.name));
            }
            if(lhs.kind==NativeExpr::Kind::Member&&!lhs.baseName.empty()&&globalConstNames.contains(lhs.baseName)) {
                const bool shadowed=current!=&initFunction&&
                    (current->locals.contains(lhs.baseName)||current->dynamicLocals.contains(lhs.baseName)||
                     current->fixedArrays.contains(lhs.baseName)||
                     current->nativeStructLocals.contains(lhs.baseName)||current->nativeObjectDataLocals.contains(lhs.baseName));
                if(!shadowed)
                    throw CompileError(lineMessage(op.line,"Dosya duzeyindeki const degerin alani degistirilemez: "+lhs.baseName+"."+lhs.name));
            }
        }
        if(!assign.empty()){
            const auto lhsKind=lhs.kind;const std::string lhsName=lhs.name,lhsOwner=lhs.owner;const int lhsFieldIndex=lhs.fieldIndex;NativeExpr rhsExpr=materialize(expression());
            if(lhsKind==NativeExpr::Kind::Variable) {
                auto nativeDataLocal=current->nativeObjectDataLocals.find(lhsName);
                if(nativeDataLocal!=current->nativeObjectDataLocals.end() && assign=="=") {
                    const std::string value=valueOf(rhsExpr);
                    envSet(current->env,lhsName,value);
                    current->dynamicLocals.insert(lhsName);
                    if(!rhsExpr.staticClass.empty()) {
                        const std::string data=scalarCall("i8*","pts_native_data","i8* "+value);
                        current->line("store i8* "+data+", i8** "+nativeDataLocal->second);
                        current->staticObjectTypes[lhsName]=rhsExpr.staticClass;
                    } else {
                        current->line("store i8* null, i8** "+nativeDataLocal->second);
                        current->staticObjectTypes.erase(lhsName);
                    }
                    endLine();
                    return;
                }
                auto structLocal=current->nativeStructLocals.find(lhsName);
                if(structLocal!=current->nativeStructLocals.end()) {
                    if(!(rhsExpr.kind==NativeExpr::Kind::StructValue || rhsExpr.structInstance) ||
                       !rhsExpr.structInstance || rhsExpr.structInstance->typeName!=structLocal->second->typeName)
                        throw CompileError(lineMessage(token().line,"Struct atamasinda ayni struct turu gerekli."));
                    if(rhsExpr.kind==NativeExpr::Kind::Variable) {
                        auto definition=structDefinitions.find(structLocal->second->typeName);
                        for(const auto& field:definition->second) {
                            auto& destination=structLocal->second->fields.at(field.name);
                            const auto& source=rhsExpr.structInstance->fields.at(field.name);
                            const std::string type=llvmType(field.type,field.bits),value=current->temp();
                            current->line(value+" = load "+type+", "+type+"* "+source.slot);
                            current->line("store "+type+" "+value+", "+type+"* "+destination.slot);
                        }
                    } else structLocal->second=rhsExpr.structInstance;
                    current->staticObjectTypes.erase(lhsName);
                    endLine();
                    return;
                }
                if((rhsExpr.kind==NativeExpr::Kind::StructValue || rhsExpr.structInstance) && rhsExpr.structInstance) {
                    current->nativeStructLocals[lhsName]=copyStructValue(rhsExpr,rhsExpr.kind==NativeExpr::Kind::Variable);
                    current->nativeStructLocals[lhsName]->temporary=false;
                    current->staticObjectTypes.erase(lhsName);
                    endLine();
                    return;
                }
                auto local=current->locals.find(lhsName);
                if(local!=current->locals.end()) {
                    if(local->second.type==NativeType::Function) {
                        if(assign!="=")
                            throw CompileError(lineMessage(token().line,"Function degiskenlerinde yalnizca '=' atamasi kullanilabilir."));
                        if(rhsExpr.kind!=NativeExpr::Kind::Value||rhsExpr.type!=NativeType::Function)
                            throw CompileError(lineMessage(token().line,"Function degiskenine yalnizca fonksiyon adresi atanabilir."));
                        if(local->second.hasFunctionSignature&&(!rhsExpr.hasFunctionSignature||
                            functionSignatureKey(local->second.functionSignature)!=functionSignatureKey(rhsExpr.functionSignature)))
                            throw CompileError(lineMessage(token().line,"Function atamasinda fonksiyon imzalari uyusmuyor veya dogrulanamiyor."));
                        if(!local->second.hasFunctionSignature&&rhsExpr.hasFunctionSignature) {
                            local->second.functionSignature=rhsExpr.functionSignature;
                            local->second.hasFunctionSignature=true;
                        }
                        current->line("store i8* "+rhsExpr.value+", i8** "+local->second.slot);
                        current->dynamicLocals.erase(lhsName);
                        current->staticObjectTypes.erase(lhsName);
                        endLine();
                        return;
                    }
                    NativeExpr assigned=std::move(rhsExpr);
                    if(assign!="=")assigned=binaryValue(assign,NativeExpr{NativeExpr::Kind::Variable,{},lhsName,{}},std::move(assigned));
                    const std::string type=llvmType(local->second.type,local->second.bits);
                    const std::string value=convertValue(std::move(assigned),local->second.type,local->second.bits,local->second.isUnsigned);
                    if(local->second.pinnedBox) {
                        current->line("call void @pts_box_pin(i8* "+value+")");
                        const std::string previous=current->temp();
                        current->line(previous+" = load i8*, i8** "+local->second.slot);
                        current->line("call void @pts_box_unpin(i8* "+previous+")");
                    }
                    current->line("store "+type+" "+value+", "+type+"* "+local->second.slot);
                } else if(current!=&initFunction && assign=="=" && rhsExpr.kind==NativeExpr::Kind::Value && rhsExpr.type!=NativeType::Dynamic) {
                    const NativeType type=rhsExpr.type;const std::string llvmTypeName=llvmType(type,rhsExpr.bits);const std::string slot=current->addLocal(lhsName,type,rhsExpr.bits,rhsExpr.isUnsigned,llvmTypeName);
                    current->line("store "+llvmTypeName+" "+rhsExpr.value+", "+llvmTypeName+"* "+slot);
                } else {
                    std::string rhs=valueOf(rhsExpr);
                    if(assign!="="){std::string old=valueOf(lhs);rhs=call("pts_binary",cstrArg(assign)+", i8* "+old+", i8* "+rhs);}
                    envSet(current->env,lhsName,rhs);
                    current->dynamicLocals.insert(lhsName);
                }
                if(assign=="="&&!rhsExpr.staticClass.empty())current->staticObjectTypes[lhsName]=rhsExpr.staticClass;
                else current->staticObjectTypes.erase(lhsName);
            } else if(lhsKind==NativeExpr::Kind::Member) {
                if((lhsFieldIndex==-2 || lhsFieldIndex==-3) && lhs.type!=NativeType::Dynamic) {
                    NativeExpr assigned=std::move(rhsExpr);
                    if(assign!="=")assigned=binaryValue(assign,materialize(lhs),std::move(assigned));
                    const std::string type=llvmType(lhs.type,lhs.bits);
                    const std::string value=convertValue(std::move(assigned),lhs.type,lhs.bits,lhs.isUnsigned);
                    current->line("store "+type+" "+value+", "+type+"* "+lhsOwner);
                } else if(lhsFieldIndex>=0 && lhs.type!=NativeType::Dynamic) {
                    NativeExpr assigned=std::move(rhsExpr);
                    if(assign!="=")assigned=binaryValue(assign,materialize(lhs),std::move(assigned));
                    std::string nativeValue=convertValue(std::move(assigned),lhs.type,lhs.bits,lhs.isUnsigned);
                    if(lhs.type==NativeType::Int) {
                        if(lhs.bits<64) {
                            const std::string wide=current->temp();
                            current->line(wide+" = "+std::string(lhs.isUnsigned?"zext ":"sext ")+llvmType(NativeType::Int,lhs.bits)+" "+nativeValue+" to i64");
                            nativeValue=wide;
                        }
                        callVoid(lhs.isUnsigned?"pts_set_member_u64":"pts_set_member_i64","i8* "+lhsOwner+", i64 "+std::to_string(lhsFieldIndex)+", i64 "+nativeValue);
                    } else if(lhs.type==NativeType::Float)
                        callVoid("pts_set_member_f64","i8* "+lhsOwner+", i64 "+std::to_string(lhsFieldIndex)+", double "+nativeValue);
                    else if(lhs.type==NativeType::Bool)
                        callVoid("pts_set_member_bool","i8* "+lhsOwner+", i64 "+std::to_string(lhsFieldIndex)+", i1 "+nativeValue);
                } else {
                    std::string rhs=valueOf(rhsExpr);
                    if(assign!="="){std::string old=valueOf(lhs);rhs=call("pts_binary",cstrArg(assign)+", i8* "+old+", i8* "+rhs);}
                    if(lhsFieldIndex>=0)callVoid("pts_set_member_index","i8* "+lhsOwner+", i64 "+std::to_string(lhsFieldIndex)+", i8* "+rhs);
                    else callVoid("pts_set_member","i8* "+lhsOwner+", "+cstrArg(lhsName)+", i8* "+rhs);
                }
            } else if(lhsKind==NativeExpr::Kind::Index) {
                std::string rhs=valueOf(rhsExpr);
                if(assign!="="){std::string old=valueOf(lhs);rhs=call("pts_binary",cstrArg(assign)+", i8* "+old+", i8* "+rhs);}
                callVoid("pts_index_set","i8* "+lhsOwner+", i8* "+lhsName+", i8* "+rhs);
            } else throw CompileError(lineMessage(token().line,"Atama solinda degisken veya alan bekleniyordu."));
        } else { (void)valueOf(lhs); }
        endLine();
    }
    void statement() {
        if(word("import")){parseImport();return;}if(word("configure")){parseConfigure();return;}if(word("class")){parseClass();return;}if(word("struct")){parseStruct();return;}if(word("process")){parseProcess();return;}if(word("action")){parseAction();return;}if(word("fn")){parseFunction();return;}if(word("main_flow")){parseMainFlow();return;}if(word("if")){parseIf();return;}if(word("while")){parseWhile();return;}if(word("for")){parseFor();return;}if(word("each")){parseEach();return;}parseSimple();
    }
    static const char* declarations() {
        return R"IR(declare void @pts_bootstrap()
declare i8* @pts_global_env()
declare i8* @pts_env_create(i8*, i8*)
declare void @pts_env_set(i8*, i8*, i8*)
declare i8* @pts_env_get(i8*, i8*)
declare i8* @pts_arg(i8**, i32)
declare void @pts_box_pin(i8*)
declare void @pts_box_unpin(i8*)
declare i8* @pts_box_copy(i8*)
declare i64 @pts_as_int(i8*)
declare i64 @pts_as_uint(i8*)
declare double @pts_as_float(i8*)
declare i1 @pts_as_bool(i8*)
declare i64 @pts_int_div(i64, i64)
declare i64 @pts_int_mod(i64, i64)
declare i64 @pts_uint_div(i64, i64)
declare i64 @pts_uint_mod(i64, i64)
declare i8* @pts_null()
declare i8* @pts_int(i64)
declare i8* @pts_uint(i64)
declare i8* @pts_float(double)
declare i8* @pts_bool(i1)
declare i8* @pts_string(i8*)
declare i8* @pts_color(i8*)
declare i8* @pts_color_call(i8*, i8**, i32)
declare i64 @pts_color_count()
declare i64 @pts_color_generated_count()
declare i8* @pts_binary(i8*, i8*, i8*)
declare i8* @pts_unary(i8*, i8*)
declare i1 @pts_truthy(i8*)
declare i8* @pts_to_string(i8*)
declare i8* @pts_get_member(i8*, i8*)
declare void @pts_set_member(i8*, i8*, i8*)
declare i8* @pts_get_member_index(i8*, i64)
declare void @pts_set_member_index(i8*, i64, i8*)
declare i64 @pts_get_member_i64(i8*, i64)
declare i64 @pts_get_member_u64(i8*, i64)
declare double @pts_get_member_f64(i8*, i64)
declare i1 @pts_get_member_bool(i8*, i64)
declare void @pts_set_member_i64(i8*, i64, i64)
declare void @pts_set_member_u64(i8*, i64, i64)
declare void @pts_set_member_f64(i8*, i64, double)
declare void @pts_set_member_bool(i8*, i64, i1)
declare i8* @pts_list_new(i1)
declare i8* @pts_index_get(i8*, i8*)
declare void @pts_index_set(i8*, i8*, i8*)
declare void @pts_array_bounds_fail(i64, i64) noreturn
declare i8* @pts_iter_begin(i8*)
declare i8* @pts_iter_next(i8*)
declare void @pts_register_function(i8*, i8*)
declare void @pts_register_class(i8*)
declare void @pts_class_field(i8*, i8*, i8*, i32, i1, i32)
declare i8* @pts_native_data(i8*)
declare void @pts_register_method(i8*, i8*, i8*)
declare i8* @pts_make(i8*, i8**, i32)
declare i8* @pts_call_global(i8*, i8*, i8**, i32)
declare i8* @pts_call_method(i8*, i8*, i8**, i32)
declare i8* @pts_engine_call(i8*, i8**, i32)
declare i8* @pts_engine3d_call(i8*, i8**, i32)
declare i8* @pts_ui_call(i8*, i8**, i32)
declare i8* @pts_file_call(i8*, i8**, i32)
declare i8* @pts_console_call(i8*, i8**, i32)
declare i8* @pts_math_call(i8*, i8**, i32)
declare double @llvm.fabs.f64(double)
declare double @llvm.sin.f64(double)
declare double @llvm.cos.f64(double)
declare double @llvm.sqrt.f64(double)
declare double @llvm.exp.f64(double)
declare double @llvm.log.f64(double)
declare double @llvm.log10.f64(double)
declare double @llvm.floor.f64(double)
declare double @llvm.ceil.f64(double)
declare double @llvm.round.f64(double)
declare double @llvm.pow.f64(double, double)
declare double @tan(double)
declare double @asin(double)
declare double @acos(double)
declare double @atan(double)
declare double @atan2(double, double)
declare i8* @pts_system_call(i8*, i8**, i32)
declare i8* @pts_string_call(i8*, i8**, i32)
; ben yaptim opencode - 2026-11-14 - stdlib runtime bildirimi
declare i8* @pts_stdlib_call(i8*, i8*, i8**, i32)
declare i8* @pts_random_call(i8*, i8**, i32)
declare i8* @pts_arena_call(i8*, i8**, i32)
declare i8* @pts_call_namespace(i8*, i8*, i8**, i32)
declare void @pts_print(i8**, i32)
)IR";
    }
public:
    NativeIRParser(std::vector<Token> input, fs::path path, bool releaseMode) : tokens(std::move(input)), sourcePath(std::move(path)), releaseOverflowUndefined(releaseMode) {
        releaseOptimizations = releaseMode;
        current=&initFunction;initFunction.start("entry");initFunction.env="%global";
    }
    std::string createLoopMetadata() {
        if (!releaseOptimizations) return {};
        const unsigned loopId = nextLoopMetadataId;
        nextLoopMetadataId += 3;
        loopMetadata << "!" << loopId << " = distinct !{!" << loopId << ", !" << loopId + 1
                     << ", !" << loopId + 2 << "}\n";
        loopMetadata << "!" << loopId + 1 << " = !{!\"llvm.loop.vectorize.enable\", i1 true}\n";
        loopMetadata << "!" << loopId + 2 << " = !{!\"llvm.loop.interleave.count\", i32 4}\n";
        return "!" + std::to_string(loopId);
    }
    const std::unordered_set<std::string>& usedStandardLibraryModules() const {
        return usedStdlibModules;
    }
    std::string parse(bool windowed) {
        preloadConfiguredScalars();
        while(!is(Kind::End)){skipLines();if(is(Kind::End))break;if(is(Kind::Indent)||is(Kind::Dedent))throw CompileError(lineMessage(token().line,"Beklenmeyen girinti."));statement();}
        if(!mainSymbol.empty()){}
        for(const auto& [name,symbol]:functionRegistrationCandidates)if(dynamicFunctionCalls.contains(name))
            callVoid("pts_register_function",cstrArg(name)+", i8* "+functionPointer(symbol));
        initFunction.line("ret void");initFunction.body<<"}\n";
        std::string initBody=initFunction.body.str();
        if(initBody.find("%global")!=std::string::npos) {
            const size_t firstLine=initBody.find('\n');
            if(firstLine!=std::string::npos)initBody.insert(firstLine+1,"  %global = call i8* @pts_global_env()\n");
        }
        std::ostringstream out;out<<"target triple = \"x86_64-w64-windows-gnu\"\n"<<declarations()<<'\n'<<strings.str()<<'\n';
        for(const auto& f:registrations.str())(void)f;
        out<<functionsIR.str();
        out<<"define internal void @pts_init() {\n"<<initBody;
        out<<"define i32 @main() {\nentry:\n  call void @pts_init()\n";
        if(!mainSymbol.empty()) {
            if(mainNeedsEnvironment)out<<"  %main_env = call i8* @pts_global_env()\n  %main_result = call i8* @"<<mainSymbol<<"(i8* %main_env, i8** null, i32 0)\n";
            else out<<"  %main_result = call i8* @"<<mainSymbol<<"(i8* null, i8** null, i32 0)\n";
        }
        out<<"  ret i32 0\n}\n";
        if(windowed) out<<"define i32 @WinMain(i8* %instance, i8* %previous, i8* %command, i32 %show) {\nentry:\n  %result = call i32 @main()\n  ret i32 %result\n}\n";
        out << loopMetadata.str();
        return out.str();
    }
};

static std::string applyReleaseIRHints(const std::string& ir, bool releaseMode) {
    if (!releaseMode) return ir;
    std::istringstream input(ir);
    std::vector<std::string> lines;
    std::string line;
    const std::array<std::string_view, 7> floatingOpcodes = {
        "fadd", "fsub", "fmul", "fdiv", "frem", "fcmp", "fneg"
    };
    while (std::getline(input, line)) {
        if (line.find(" = alloca ") != std::string::npos &&
            line.find(", align ") == std::string::npos) {
            line += ", align 32";
        }
        for (const std::string_view opcode : floatingOpcodes) {
            const std::string needle = " = " + std::string(opcode);
            const std::size_t position = line.find(needle);
            if (position == std::string::npos) continue;
            const std::size_t flagPosition = position + needle.size();
            if (line.compare(flagPosition, 5, " fast") != 0 &&
                line.compare(flagPosition, 9, " contract") != 0) {
                line.insert(flagPosition, " fast");
            }
        }
        const std::string callNeedle = " = call double @";
        if (const std::size_t callPosition = line.find(callNeedle); callPosition != std::string::npos)
            line.insert(callPosition + 8, "fast ");
        lines.push_back(std::move(line));
    }
    for (std::size_t i = 0; i + 1 < lines.size(); ++i) {
        const std::string callMarker = " = call fastcc ";
        const std::size_t callPosition = lines[i].find(callMarker);
        if (callPosition == std::string::npos) continue;
        const std::string result = lines[i].substr(2, callPosition - 2);
        const std::string returnPrefix = "  ret ";
        if (lines[i + 1].rfind(returnPrefix, 0) != 0) continue;
        const std::size_t lastSpace = lines[i + 1].find_last_of(' ');
        if (lastSpace == std::string::npos || lines[i + 1].substr(lastSpace + 1) != result) continue;
        lines[i].insert(callPosition + 3, "tail ");
    }
    std::ostringstream output;
    for (const std::string& outputLine : lines) output << outputLine << '\n';
    return output.str();
}

static std::wstring quoteArg(const std::wstring& value) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        if (c == L'"') { result.append(slashes * 2 + 1, L'\\'); result.push_back(c); }
        else { result.append(slashes, L'\\'); result.push_back(c); }
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

static std::wstring executablePath() {
    std::wstring buffer(32768, L'\0');
    DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size == 0 || size >= buffer.size()) throw std::runtime_error("Derleyici dosyasinin yolu okunamadi.");
    buffer.resize(size);
    return buffer;
}

static void configureZigCacheDefaults() {
    const bool needLocal=GetEnvironmentVariableW(L"ZIG_LOCAL_CACHE_DIR",nullptr,0)==0;
    const bool needGlobal=GetEnvironmentVariableW(L"ZIG_GLOBAL_CACHE_DIR",nullptr,0)==0;
    if(!needLocal&&!needGlobal)return;

    std::vector<wchar_t> buffer(32768);
    const DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",buffer.data(),static_cast<DWORD>(buffer.size()));
    if(length==0||length>=buffer.size())throw std::runtime_error("Zig onbellegi icin LOCALAPPDATA bulunamadi.");
    const fs::path cacheRoot=fs::path(std::wstring(buffer.data(),length))/L"PRCYTERAS";
    std::error_code error;
    fs::create_directories(cacheRoot,error);
    if(error)throw std::runtime_error("Zig onbellegi klasoru olusturulamadi: "+cacheRoot.string());
    const fs::path local=cacheRoot/L"zig-cache";
    const fs::path global=cacheRoot/L"zig-global-cache";
    if(needLocal) {
        fs::create_directories(local,error);
        if(error||!SetEnvironmentVariableW(L"ZIG_LOCAL_CACHE_DIR",local.c_str()))
            throw std::runtime_error("Zig yerel onbellegi ayarlanamadi.");
    }
    if(needGlobal) {
        error.clear();
        fs::create_directories(global,error);
        if(error||!SetEnvironmentVariableW(L"ZIG_GLOBAL_CACHE_DIR",global.c_str()))
            throw std::runtime_error("Zig genel onbellegi ayarlanamadi.");
    }
}

static DWORD runProcess(const std::wstring& app, const std::wstring& args) {
    std::wstring command = quoteArg(app) + L" " + args;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(app.c_str(), command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &process))
        throw std::runtime_error("LLVM arac zinciri baslatilamadi. PRCYTERAS kurulumunu onarin.");
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    return code;
}

static int usage() {
    const fs::path packageRoot = bundledPackagesRoot();
    std::size_t bundledCount = 0;
    std::error_code packageError;
    if (!packageRoot.empty() && fs::is_directory(packageRoot, packageError)) {
        for (fs::directory_iterator it(packageRoot, packageError), end; !packageError && it != end; it.increment(packageError)) {
            if (it->is_directory(packageError) && !packageError) ++bundledCount;
        }
    }
    std::cout << "PRCYTERAS Beta 1.0 (yerel paket katalogi: " << bundledCount << " paket)\n"
              << "Kullanim: pts cargo <kaynak.pts> [-o <herhangi-bir-konum\\cikti.exe>] [--release] [--portable] [--windowed]\n"
              << "          pts cargo install [paket...]  (paket yazilmazsa tum yerel katalog)\n"
              << "          pts cargo install --all\n"
              << "          pts cargo list\n"
              << "          pts cargo search\n"
              << "          pts cargo uninstall <paket>\n"
              << "Ornek: pts cargo merhaba.pts\n"
              << "Oyun: pts cargo oyun.pts --windowed --release\n"
              << "Baska konuma: pts cargo \"C:\\Projeler\\oyun.pts\" -o \"D:\\Derlemeler\\oyun.exe\" --windowed --release\n"
              << "Paket kur: pts cargo install [paket...] veya pts cargo install --all | Katalog: pts cargo search\n"
              << "Not: Paketler bu kurulumla birlikte gelen yerel katalogdan kopyalanir; uzak internet deposu yoktur.\n"
              << "Kurulu paketler: %LOCALAPPDATA%\\PRCYTERAS\\packages\n"
              << "-O1, -O2 ve -O3 alternatif optimizasyon seviyeleridir; normal derleme ve --release -O3 kullanir.\n"
              << "--release Polly stripmine, full LTO, PTS kodunda fast-math, hizalama, dongu ipuclari, fastcc ve sembol temizlemeyi acar.\n"
              << "--release: signed tasma tanimsizdir; fast-math kayan nokta sonuclarini degistirebilir.\n"
              << "Runtime IEEE kayan nokta davranisini korumak icin C++ runtime fast-math olmadan derlenir.\n"
              << "--portable, --release ciktisini farkli x64 islemcilerde calismaya daha uygun uretir.\n"
              << "Derleyici hatalari kaynak satirini ve konumunu gosterir.\n";
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    fs::path diagnosticSourcePath;
    std::string diagnosticSource;
    if (argc < 2 || std::wstring(argv[1]) == L"--yardim" || std::wstring(argv[1]) == L"--help") return usage();
    // ben yaptım opencode - 2026-11-14 - paket CLI komutlari
    if (std::wstring(argv[1]) == L"cargo" && argc >= 3) {
        const std::wstring command(argv[2]);
        if (command == L"install" || command == L"kur") {
            try {
                if (argc == 3) return installAllBundledPackages();
                if (argc == 4 && std::wstring(argv[3]) == L"--all") return installAllBundledPackages();
                for (int i = 3; i < argc; ++i) installBundledPackage(argv[i]);
                return 0;
            } catch (const std::exception& e) {
                std::cerr << "PRCYTERAS paket hatasi: " << e.what() << "\n";
                return 1;
            }
        }
        if (command == L"list" || command == L"liste") {
            try { return listInstalledPackages(); }
            catch (const std::exception& e) { std::cerr << "PRCYTERAS paket hatasi: " << e.what() << "\n"; return 1; }
        }
        if (command == L"search" || command == L"ara") {
            try { return searchBundledPackages(); }
            catch (const std::exception& e) { std::cerr << "PRCYTERAS paket hatasi: " << e.what() << "\n"; return 1; }
        }
        if (command == L"uninstall" || command == L"kaldir") {
            if (argc < 4) {
                std::wcerr << L"Kullanim: pts cargo uninstall <paket>\n";
                return 1;
            }
            try {
                for (int i = 3; i < argc; ++i) uninstallPackage(argv[i]);
                return 0;
            } catch (const std::exception& e) {
                std::cerr << "PRCYTERAS paket hatasi: " << e.what() << "\n";
                return 1;
            }
        }
    }
    if (std::wstring(argv[1]) != L"cargo" || argc < 3) return usage();
    try {
        fs::path sourcePath(argv[2]);
        diagnosticSourcePath = sourcePath;
        std::wstring extension = sourcePath.extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
        if (extension != L".pts") throw std::runtime_error("Kaynak dosyasi .pts uzantisini kullanmali.");
        fs::path outputPath = sourcePath;
        outputPath.replace_extension(L".exe");
        bool releaseMode = false;
        bool portableMode = false;
        bool windowed = false;
        for (int i = 3; i < argc; ++i) {
            if (std::wstring(argv[i]) == L"-o" && i + 1 < argc) outputPath = fs::path(argv[++i]);
            else if (std::wstring(argv[i]) == L"--release") releaseMode = true;
            else if (std::wstring(argv[i]) == L"--portable") portableMode = true;
            else if (std::wstring(argv[i]) == L"--windowed" || std::wstring(argv[i]) == L"--no-console") windowed = true;
            else throw std::runtime_error("Bilinmeyen veya eksik derleme secenegi.");
        }
        if (portableMode && !releaseMode) throw std::runtime_error("--portable secenegi --release ile birlikte kullanilmali.");
        if (!releaseMode) configureZigCacheDefaults();
        std::ifstream input(sourcePath, std::ios::binary);
        if (!input) throw std::runtime_error("Kaynak dosyasi acilamadi: " + sourcePath.string());
        std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        diagnosticSource = source;
        Lexer lexer(source);
        DWORD pid = GetCurrentProcessId();
        fs::path tempDir = fs::temp_directory_path() /
            ("prcyteras_" + std::to_string(pid) + "_" + std::to_string(GetTickCount64()));
        fs::create_directories(tempDir);
        NativeIRParser parser(lexer.scan(), sourcePath, releaseMode);
        std::string generated = applyReleaseIRHints(parser.parse(windowed), releaseMode);
        fs::path tempSource = tempDir / L"main.ll";
        {
            std::ofstream ir(tempSource, std::ios::binary | std::ios::trunc);
            if (!ir) throw std::runtime_error("Gecici LLVM IR dosyasi olusturulamadi.");
            ir.write(generated.data(), static_cast<std::streamsize>(generated.size()));
        }
        std::vector<wchar_t> irDumpPath(32768);
        const DWORD irDumpLength=GetEnvironmentVariableW(L"PRCYTERAS_DUMP_IR",irDumpPath.data(),static_cast<DWORD>(irDumpPath.size()));
        if(irDumpLength>0&&irDumpLength<irDumpPath.size())
            fs::copy_file(tempSource,fs::path(std::wstring(irDumpPath.data(),irDumpLength)),fs::copy_options::overwrite_existing);
        const fs::path executableDirectory = fs::path(executablePath()).parent_path();
        fs::path compiler;
        fs::path optTool;
        fs::path pollyPlugin;
        fs::path llvmSysroot;
        if (releaseMode) {
            const fs::path llvmDirectory = executableDirectory / L"llvm";
            compiler = llvmDirectory / L"bin" / L"clang++.exe";
            optTool = llvmDirectory / L"bin" / L"opt.exe";
            pollyPlugin = llvmDirectory / L"bin" / L"prcyteras-polly.dll";
            llvmSysroot = llvmDirectory / L"sysroot";
            if (!fs::exists(compiler) || !fs::exists(optTool) || !fs::exists(pollyPlugin) ||
                !fs::exists(llvmSysroot / L"include" / L"windows.h"))
                throw std::runtime_error("--release icin LLVM/Polly arac zinciri eksik. PRCYTERAS kurulumunu onarin.");
        } else {
            compiler = executableDirectory / L"zig" / L"zig.exe";
            if (!fs::exists(compiler)) throw std::runtime_error("LLVM arac zinciri bulunamadi. PRCYTERAS kurulumunu onarin.");
        }
        fs::path runtimeSource = executableDirectory / L"pts_runtime.cpp";
        if (!fs::exists(runtimeSource)) throw std::runtime_error("PRCYTERAS yerlesik calisma kutuphanesi bulunamadi. Kurulumu yenileyin.");
        fs::path iconResource = executableDirectory / L"pts.res";

        std::ifstream runtimeInput(runtimeSource, std::ios::binary);
        if (!runtimeInput) throw std::runtime_error("PRCYTERAS runtime kaynagi okunamadi.");
        const std::string runtimeText((std::istreambuf_iterator<char>(runtimeInput)), std::istreambuf_iterator<char>());
        const fs::path runtimeVisibilityHeader = tempDir / L"pts_runtime_visibility.h";
        std::ofstream visibility(runtimeVisibilityHeader, std::ios::binary | std::ios::trunc);
        if (!visibility) throw std::runtime_error("Runtime baglanti haritasi olusturulamadi.");
        visibility << "#pragma once\n";
        for (const std::string& module : stdlibNamespaces()) {
            const bool used = !releaseMode || parser.usedStandardLibraryModules().count(module) != 0;
            visibility << "#define PTS_MOD_USED_" << module << ' ' << (used ? 1 : 0) << '\n';
        }
        const std::string apiMarker = "PTS_RUNTIME_API(";
        std::size_t apiPosition = 0;
        std::size_t apiCount = 0;
        while ((apiPosition = runtimeText.find(apiMarker, apiPosition)) != std::string::npos) {
            const std::size_t nameStart = apiPosition + apiMarker.size();
            const std::size_t nameEnd = runtimeText.find(')', nameStart);
            if (nameEnd == std::string::npos) throw std::runtime_error("Runtime API isaretlemesi hatali.");
            const std::string name = runtimeText.substr(nameStart, nameEnd - nameStart);
            const std::string irName = "@" + name;
            std::size_t irPosition = 0;
            std::size_t references = 0;
            while ((irPosition = generated.find(irName, irPosition)) != std::string::npos) {
                const std::size_t afterName = irPosition + irName.size();
                if (afterName == generated.size() ||
                    !(std::isalnum(static_cast<unsigned char>(generated[afterName])) || generated[afterName] == '_')) {
                    ++references;
                }
                irPosition += irName.size();
            }
            visibility << "#define PTS_RUNTIME_USED_" << name << ' ' << (references > 1 ? 1 : 0) << '\n';
            ++apiCount;
            apiPosition = nameEnd + 1;
        }
        visibility.close();
        if (apiCount == 0) throw std::runtime_error("Runtime API baglanti isaretleri bulunamadi.");

        fs::path absoluteOutput = fs::absolute(outputPath);
        if (!absoluteOutput.parent_path().empty()) fs::create_directories(absoluteOutput.parent_path());
        DWORD result = 0;
        if (releaseMode) {
            const fs::path optimizedIR = tempDir / L"main.polly.ll";
            const fs::path programObject = tempDir / L"main.o";
            const fs::path runtimeObject = tempDir / L"pts_runtime.o";
            const std::wstring pluginArgument = quoteArg(L"-load-pass-plugin=" + pollyPlugin.wstring());
            const std::wstring sysrootArgument = quoteArg(L"--sysroot=" + llvmSysroot.wstring());
            std::wstring optArgs = pluginArgument +
                L" -polly -polly-vectorizer=stripmine -passes=default<O3>,polly " +
                quoteArg(tempSource.wstring()) + L" -S -o " + quoteArg(optimizedIR.wstring());
            std::wcout << L"PRCYTERAS: Polly LLVM optimizasyonu uygulaniyor...\n";
            result = runProcess(optTool.wstring(), optArgs);
            if (result != 0) {
                std::error_code ignored;
                fs::remove_all(tempDir, ignored);
                return static_cast<int>(result);
            }

            std::wstring commonCompileArgs =
                L"-target x86_64-w64-windows-gnu " + sysrootArgument +
                L" -O3 -flto=full -ffunction-sections -fdata-sections";
            if (!portableMode) commonCompileArgs += L" -march=native -mtune=native";
            commonCompileArgs += L" -std=c++20 -static ";

            std::wstring programArgs = commonCompileArgs +
                L" -ffast-math -c " + quoteArg(optimizedIR.wstring()) +
                L" -o " + quoteArg(programObject.wstring());
            std::wcout << L"PRCYTERAS: PTS kodu Clang -O3 ve full LTO icin hazirlaniyor...\n";
            result = runProcess(compiler.wstring(), programArgs);
            if (result == 0) {
                std::wstring runtimeArgs = commonCompileArgs +
                    L" -I " + quoteArg(tempDir.wstring()) +
                    L" -c " + quoteArg(runtimeSource.wstring()) +
                    L" -o " + quoteArg(runtimeObject.wstring());
                result = runProcess(compiler.wstring(), runtimeArgs);
            }
            if (result == 0) {
                std::wstring linkArgs =
                    L"-target x86_64-w64-windows-gnu " + sysrootArgument +
                    L" -O3 -flto=full -fuse-ld=lld -Wl,--gc-sections -s -static";
                if (!portableMode) linkArgs += L" -march=native -mtune=native";
                linkArgs += L" " + quoteArg(programObject.wstring()) +
                    L" " + quoteArg(runtimeObject.wstring()) +
                    (fs::exists(iconResource) ? L" " + quoteArg(iconResource.wstring()) : L"") +
                    L" -lgdi32 -luser32 -lwinmm -lmsimg32 -ld3d11 -ldxgi -ld3dcompiler_47 -lwindowscodecs -lole32 -loleaut32 -luuid -lmfplat -lmfreadwrite -lmfuuid -lshell32 -ladvapi32 -lbcrypt -lwininet -lws2_32 -liphlpapi -lcomdlg32";
                if (windowed) linkArgs += L" -Wl,--subsystem=windows";
                linkArgs += L" -o " + quoteArg(absoluteOutput.wstring());
                std::wcout << L"PRCYTERAS: Clang full LTO ile native EXE olusturuluyor...\n";
                result = runProcess(compiler.wstring(), linkArgs);
            }
        } else {
            std::wstring args = L"c++ -target x86_64-windows-gnu -O3 -I " +
                quoteArg(tempDir.wstring()) +
                L" -std=c++20 -static -lgdi32 -luser32 -lwinmm -lmsimg32 -ld3d11 -ldxgi -ld3dcompiler_47 -lwindowscodecs -lole32 -loleaut32 -luuid -lmfplat -lmfreadwrite -lmfuuid -lshell32 -ladvapi32 -lbcrypt -lwininet -lws2_32 -liphlpapi -lcomdlg32 ";
            if (windowed) args += L"-Wl,--subsystem=windows ";
            args += quoteArg(tempSource.wstring()) + L" " + quoteArg(runtimeSource.wstring()) +
                (fs::exists(iconResource) ? L" " + quoteArg(iconResource.wstring()) : L"") +
                L" -o " + quoteArg(absoluteOutput.wstring());
            std::wcout << L"PRCYTERAS: " << sourcePath.wstring() << L" LLVM arka ucuyla native EXE'ye derleniyor...\n";
            result = runProcess(compiler.wstring(), args);
        }
        std::error_code ignored;
        fs::remove_all(tempDir, ignored);
        if (result != 0) return static_cast<int>(result);
        std::wcout << L"Tamamlandi: " << absoluteOutput.wstring() << L"\n";
        return 0;
    } catch (const CompileError& e) {
        printDiagnostic(diagnosticSourcePath, diagnosticSource, e.what());
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "PRCYTERAS hatasi: " << e.what() << "\n";
        return 1;
    }
}
