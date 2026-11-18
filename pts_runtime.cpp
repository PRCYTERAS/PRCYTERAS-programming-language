#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <winsvc.h>
#include <bcrypt.h>
#include <mmsystem.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wincodec.h>
#include <xaudio2.h>
#include <wininet.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <commdlg.h>
#include <shlobj.h>
#include <sapi.h>
#include <xinput.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#ifdef BLACK
#undef BLACK
#endif
#ifdef WHITE
#undef WHITE
#endif
#ifdef RED
#undef RED
#endif
#ifdef GREEN
#undef GREEN
#endif
#ifdef BLUE
#undef BLUE
#endif
#ifdef BROWN
#undef BROWN
#endif
#ifdef YELLOW
#undef YELLOW
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iterator>
#include <map>
#include <queue>
#include <set>
#include <cwctype>
#include <cwchar>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "pts_runtime_visibility.h"

#define STB_VORBIS_NO_STDIO
#include "stb_vorbis.c"
#undef STB_VORBIS_NO_STDIO

#define PTS_RT_CAT_INNER(a, b) a##b
#define PTS_RT_CAT(a, b) PTS_RT_CAT_INNER(a, b)
#define PTS_RT_LINKAGE_0 static
#define PTS_RT_LINKAGE_1
#define PTS_RT_USED(name) PTS_RT_CAT(PTS_RUNTIME_USED_, name)
#define PTS_RUNTIME_API(name) PTS_RT_CAT(PTS_RT_LINKAGE_, PTS_RT_USED(name))
#define PTS_MODULE_ENABLED(name) PTS_MODULE_ENABLED_INNER(name)
#define PTS_MODULE_ENABLED_INNER(name) PTS_MOD_USED_##name

namespace fs = std::filesystem;

namespace {
struct TransparentStringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view value) const noexcept {
        return std::hash<std::string_view>{}(value);
    }
};
struct TransparentStringEqual {
    using is_transparent = void;
    bool operator()(std::string_view left, std::string_view right) const noexcept {
        return left == right;
    }
};
template <class T>
using StringMap = std::unordered_map<std::string, T, TransparentStringHash, TransparentStringEqual>;
using StringSet = std::unordered_set<std::string, TransparentStringHash, TransparentStringEqual>;

std::atomic<bool> consoleOutputUtf8{false};
std::atomic<bool> consoleInputUtf8{false};
void writeStdout(std::string_view text) {
    if(text.empty()) return;
    const bool nonAscii=std::any_of(text.begin(),text.end(),[](unsigned char value){return value>=0x80;});
    if(nonAscii&&GetConsoleWindow()) { bool expected=false;if(consoleOutputUtf8.compare_exchange_strong(expected,true,std::memory_order_relaxed))SetConsoleOutputCP(CP_UTF8); }
    std::fwrite(text.data(),1,text.size(),stdout);
    std::fflush(stdout);
}
void ensureConsoleInputUtf8() { if(GetConsoleWindow()) { bool expected=false;if(consoleInputUtf8.compare_exchange_strong(expected,true,std::memory_order_relaxed))SetConsoleCP(CP_UTF8); } }
void writeStderr(std::string_view text) { if(!text.empty()) std::fwrite(text.data(),1,text.size(),stderr); }
fs::path windowsApiPath(const fs::path& input) {
    std::wstring value=input.wstring();
    if(value.rfind(L"\\\\?\\",0)==0||value.size()<248)return fs::path(value);
    if(value.rfind(L"\\\\",0)==0)return fs::path(L"\\\\?\\UNC\\"+value.substr(2));
    if(value.size()>=3&&value[1]==L':'&&(value[2]==L'\\'||value[2]==L'/'))return fs::path(L"\\\\?\\"+value);
    return fs::path(value);
}
bool getPathIdentityAndFinal(const fs::path& input,bool directory,BY_HANDLE_FILE_INFORMATION& info,std::wstring& finalPath) {
    const fs::path apiPath=windowsApiPath(input);
    const DWORD flags=directory?FILE_FLAG_BACKUP_SEMANTICS:0;
    HANDLE handle=CreateFileW(apiPath.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,flags,nullptr);
    if(handle==INVALID_HANDLE_VALUE)return false;
    bool okay=GetFileInformationByHandle(handle,&info)!=0;
    DWORD needed=okay?GetFinalPathNameByHandleW(handle,nullptr,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS):0;
    std::vector<wchar_t> buffer(static_cast<std::size_t>(needed)+1);
    if(needed==0||GetFinalPathNameByHandleW(handle,buffer.data(),static_cast<DWORD>(buffer.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS)==0)okay=false;
    if(okay){finalPath.assign(buffer.data());if(finalPath.empty()||finalPath.back()!=L'\\')finalPath.push_back(L'\\');}
    CloseHandle(handle);return okay;
}
DWORD scanRootVolumeSerial=0;
std::wstring& scanRootFinalPathState() { static std::wstring value; return value; }
StringSet& scannedDirectoryIdsState() { static StringSet value; return value; }
struct Object;
struct ListValue;
struct ArenaValue {
    static constexpr std::size_t maxAlignment = 4096;
    std::vector<std::uint8_t> storage;
    std::size_t baseOffset = 0;
    std::size_t capacity = 0;
    std::size_t used = 0;

    std::uint8_t* data() { return storage.data() + baseOffset; }
    const std::uint8_t* data() const { return storage.data() + baseOffset; }
};
using Value = std::variant<std::monostate, std::int64_t, std::uint64_t, double, bool, std::string,
                           std::shared_ptr<Object>, std::shared_ptr<ListValue>>;
struct Box {
    Value value;
    std::size_t poolIndex = 0;
    std::size_t pinCount = 0;
    bool active = false;
    bool free = false;
};
class TemporaryBoxPool {
    static constexpr std::size_t blockCapacity = 1024;
    static constexpr std::size_t retainedBlockLimit = 8;
    std::vector<std::unique_ptr<Box[]>> blocks;
    std::vector<std::size_t> activeIndices;
    std::vector<std::size_t> freeIndices;
    std::size_t used = 0;
    std::size_t pinnedCount = 0;
    Box& at(std::size_t index) {
        const std::size_t blockIndex = index / blockCapacity;
        if (blockIndex >= blocks.size()) blocks.resize(blockIndex + 1);
        if (!blocks[blockIndex]) blocks[blockIndex] = std::make_unique<Box[]>(blockCapacity);
        return blocks[blockIndex][index % blockCapacity];
    }
public:
    void* make(Value value) {
        std::size_t index;
        if (freeIndices.empty()) index = used++;
        else { index = freeIndices.back(); freeIndices.pop_back(); }
        Box* result = &at(index);
        result->poolIndex = index;
        result->pinCount = 0;
        result->active = true;
        result->free = false;
        result->value = std::move(value);
        activeIndices.push_back(index);
        return result;
    }
    void pin(void* handle) {
        if (!handle) return;
        Box* value = static_cast<Box*>(handle);
        if (value->pinCount++ == 0) ++pinnedCount;
    }
    void unpin(void* handle) {
        if (!handle) return;
        Box* value = static_cast<Box*>(handle);
        if (value->pinCount == 0) return;
        if (--value->pinCount == 0) {
            --pinnedCount;
            value->value = std::monostate{};
            if (!value->active && !value->free) {
                value->free = true;
                freeIndices.push_back(value->poolIndex);
            }
        }
    }
    void clear() {
        for (const std::size_t index : activeIndices) {
            Box& value = at(index);
            if (!value.active) continue;
            value.active = false;
            if (value.pinCount == 0) {
                value.value = std::monostate{};
                if (!value.free) {
                    value.free = true;
                    freeIndices.push_back(index);
                }
            }
        }
        activeIndices.clear();
        if (pinnedCount == 0) {
            freeIndices.clear();
            used = 0;
            if (blocks.size() > retainedBlockLimit) blocks.resize(retainedBlockLimit);
        }
    }
};
struct Object {
    std::string type;
    std::vector<Value> fields;
    std::vector<std::uint64_t> nativeFields;
    StringMap<Value> dynamicFields;
};
struct ListValue { std::vector<Value> values; std::vector<bool> active; bool flow = false; };
struct Iterator { std::shared_ptr<ListValue> list; std::size_t index = 0; };
struct Environment {
    StringMap<Value> values;
    std::shared_ptr<Object> self;
    Environment* parent = nullptr;
};
using Function = void* (*)(void*, void**, int);
struct IntegerFieldType { int bits = 0; bool isUnsigned = false; };
struct ClassInfo {
    StringMap<std::size_t> fieldIndices;
    std::vector<Value> defaults;
    std::vector<std::string> fieldOrder;
    std::vector<IntegerFieldType> integerFieldTypes;
    std::vector<int> nativeFieldKinds;
    StringMap<IntegerFieldType> integerFields;
    StringMap<Function> methods;
};
StringMap<Function>& functionsState() { static StringMap<Function> value; return value; }
StringMap<ClassInfo>& classesState() { static StringMap<ClassInfo> value; return value; }
std::vector<std::unique_ptr<ArenaValue>>& arenasState() { static std::vector<std::unique_ptr<ArenaValue>> value; return value; }
std::mutex& arenaMutexState() { static std::mutex value; return value; }
Environment& globalEnvironmentState() { static Environment value; return value; }
TemporaryBoxPool& temporaryValuesState() { static TemporaryBoxPool value; return value; }
std::mt19937& randomEngineState() { static std::mt19937 value(static_cast<unsigned int>(GetTickCount64())); return value; }

void* box(Value value) {
    return temporaryValuesState().make(std::move(value));
}
Value valueOf(void* handle) { return handle ? static_cast<Box*>(handle)->value : Value{}; }
void* globalEnv() { return &globalEnvironmentState(); }
std::string stringOf(const Value& value) {
    if (const auto* item = std::get_if<std::string>(&value)) return *item;
    if (const auto* item = std::get_if<std::int64_t>(&value)) return std::to_string(*item);
    if (const auto* item = std::get_if<std::uint64_t>(&value)) return std::to_string(*item);
    if (const auto* item = std::get_if<double>(&value)) return std::to_string(*item);
    if (const auto* item = std::get_if<bool>(&value)) return *item ? "true" : "false";
    if (std::holds_alternative<std::monostate>(value)) return "null";
    return "<object>";
}
double numberOf(const Value& value) {
    if (const auto* item = std::get_if<std::int64_t>(&value)) return static_cast<double>(*item);
    if (const auto* item = std::get_if<std::uint64_t>(&value)) return static_cast<double>(*item);
    if (const auto* item = std::get_if<double>(&value)) return *item;
    if (const auto* item = std::get_if<bool>(&value)) return *item ? 1.0 : 0.0;
    if (const auto* item = std::get_if<std::string>(&value)) {
        try { return std::stod(*item); } catch (...) { return 0.0; }
    }
    return 0.0;
}
Value normalizeIntegerField(const ClassInfo& info, const std::string& name, const Value& value) {
    const auto field = info.integerFields.find(name);
    if (field == info.integerFields.end() || field->second.bits < 4 || field->second.bits > 64) return value;
    const int bits = field->second.bits;
    std::uint64_t raw = 0;
    if (std::holds_alternative<std::monostate>(value)) raw = 0;
    else if (const auto* item = std::get_if<std::uint64_t>(&value)) raw = *item;
    else if (const auto* item = std::get_if<std::int64_t>(&value)) raw = static_cast<std::uint64_t>(*item);
    else if (const auto* item = std::get_if<double>(&value)) {
        const long double n = *item;
        if (field->second.isUnsigned) raw = n <= 0 ? 0 : static_cast<std::uint64_t>(std::fmod(n, std::ldexp(1.0L, bits)));
        else raw = static_cast<std::uint64_t>(static_cast<std::int64_t>(n));
    } else if (const auto* item = std::get_if<bool>(&value)) raw = *item ? 1 : 0;
    else return value;
    const std::uint64_t mask = bits == 64 ? UINT64_MAX : ((std::uint64_t{1} << bits) - 1);
    raw &= mask;
    if (field->second.isUnsigned) return raw;
    if (bits < 64 && (raw & (std::uint64_t{1} << (bits - 1)))) raw |= ~mask;
    return static_cast<std::int64_t>(raw);
}
Value nativeFieldValue(const Object& object, std::size_t index);
void syncNativeField(Object& object, std::size_t index, const Value& value);
const Value* objectField(const Object& object, const std::string& name) {
    const auto type = classesState().find(object.type);
    if (type != classesState().end()) {
        const auto field = type->second.fieldIndices.find(name);
        if (field != type->second.fieldIndices.end() && field->second < object.fields.size()) {
            if(field->second<type->second.nativeFieldKinds.size() && type->second.nativeFieldKinds[field->second]!=0) {
                static thread_local Value nativeScratch;
                nativeScratch=nativeFieldValue(object,field->second);
                return &nativeScratch;
            }
            return &object.fields[field->second];
        }
    }
    const auto dynamic = object.dynamicFields.find(name);
    return dynamic == object.dynamicFields.end() ? nullptr : &dynamic->second;
}
void setObjectField(Object& object, const std::string& name, Value value) {
    const auto type = classesState().find(object.type);
    if (type != classesState().end()) {
        const auto field = type->second.fieldIndices.find(name);
        if (field != type->second.fieldIndices.end() && field->second < object.fields.size()) {
            object.fields[field->second] = normalizeIntegerField(type->second, name, value);
            syncNativeField(object,field->second,object.fields[field->second]);
            return;
        }
    }
    object.dynamicFields[name] = std::move(value);
}
const Value* objectFieldAtHandle(void* objectHandle, std::size_t index) {
    const Value value = valueOf(objectHandle);
    const auto* object = std::get_if<std::shared_ptr<Object>>(&value);
    if (!object || !*object || index >= (*object)->fields.size()) return nullptr;
    const auto type=classesState().find((*object)->type);
    if(type!=classesState().end() && index<type->second.nativeFieldKinds.size() && type->second.nativeFieldKinds[index]!=0) {
        static thread_local Value nativeScratch;
        nativeScratch=nativeFieldValue(**object,index);
        return &nativeScratch;
    }
    return &(*object)->fields[index];
}
void* boxedObjectFieldAt(void* objectHandle, std::size_t index) {
    const Value* field = objectFieldAtHandle(objectHandle, index);
    return field ? box(*field) : nullptr;
}
void setObjectFieldAtValue(void* objectHandle, std::size_t index, Value fieldValue) {
    const Value value = valueOf(objectHandle);
    const auto* object = std::get_if<std::shared_ptr<Object>>(&value);
    if (!object || !*object || index >= (*object)->fields.size()) return;
    const auto type = classesState().find((*object)->type);
    if (type != classesState().end() && index < type->second.fieldOrder.size())
        fieldValue = normalizeIntegerField(type->second, type->second.fieldOrder[index], fieldValue);
    (*object)->fields[index] = std::move(fieldValue);
    syncNativeField(**object,index,(*object)->fields[index]);
}
void setObjectFieldAt(void* objectHandle, std::size_t index, void* valueHandle) {
    setObjectFieldAtValue(objectHandle, index, valueOf(valueHandle));
}
bool truthy(const Value& value) {
    if (std::holds_alternative<std::monostate>(value)) return false;
    if (const auto* item = std::get_if<bool>(&value)) return *item;
    if (const auto* item = std::get_if<std::int64_t>(&value)) return *item != 0;
    if (const auto* item = std::get_if<std::uint64_t>(&value)) return *item != 0;
    if (const auto* item = std::get_if<double>(&value)) return *item != 0.0;
    if (const auto* item = std::get_if<std::string>(&value)) return !item->empty();
    return true;
}
Value nativeFieldValue(const Object& object, std::size_t index) {
    const auto type=classesState().find(object.type);
    if(type==classesState().end() || index>=object.nativeFields.size() || index>=type->second.nativeFieldKinds.size())
        return index<object.fields.size()?object.fields[index]:Value{};
    const int kind=type->second.nativeFieldKinds[index];
    const std::uint64_t raw=object.nativeFields[index];
    if(kind==1) {
        if(index>=type->second.integerFieldTypes.size())return static_cast<std::int64_t>(raw);
        const auto integer=type->second.integerFieldTypes[index];
        const int bits=integer.bits;
        const std::uint64_t mask=bits>=64?UINT64_MAX:((std::uint64_t{1}<<bits)-1);
        std::uint64_t normalized=raw&mask;
        if(integer.isUnsigned)return normalized;
        if(bits<64 && (normalized&(std::uint64_t{1}<<(bits-1))))normalized|=~mask;
        return static_cast<std::int64_t>(normalized);
    }
    if(kind==2) { double value=0.0;std::memcpy(&value,&raw,sizeof(value));return value; }
    if(kind==3)return raw!=0;
    return index<object.fields.size()?object.fields[index]:Value{};
}
void syncNativeField(Object& object, std::size_t index, const Value& value) {
    const auto type=classesState().find(object.type);
    if(type==classesState().end() || index>=object.nativeFields.size() || index>=type->second.nativeFieldKinds.size())return;
    const int kind=type->second.nativeFieldKinds[index];
    if(kind==1) {
        std::uint64_t raw=0;
        if(index<type->second.fieldOrder.size()) {
            Value normalized=normalizeIntegerField(type->second,type->second.fieldOrder[index],value);
            if(const auto* p=std::get_if<std::uint64_t>(&normalized))raw=*p;
            else if(const auto* p=std::get_if<std::int64_t>(&normalized))raw=static_cast<std::uint64_t>(*p);
        }
        object.nativeFields[index]=raw;
    } else if(kind==2) {
        const double number=numberOf(value);
        std::memcpy(&object.nativeFields[index],&number,sizeof(number));
    } else if(kind==3) object.nativeFields[index]=truthy(value)?1u:0u;
}
Value binary(const std::string& op, const Value& a, const Value& b) {
    if (op == "+" && (std::holds_alternative<std::string>(a) || std::holds_alternative<std::string>(b)))
        return stringOf(a) + stringOf(b);
    if (op == "==" || op == "!=") {
        bool equal = false;
        if (a.index() == b.index()) {
            if (std::holds_alternative<std::monostate>(a)) equal = true;
            else if (auto x = std::get_if<std::int64_t>(&a)) equal = *x == std::get<std::int64_t>(b);
            else if (auto x = std::get_if<std::uint64_t>(&a)) equal = *x == std::get<std::uint64_t>(b);
            else if (auto x = std::get_if<double>(&a)) equal = *x == std::get<double>(b);
            else if (auto x = std::get_if<bool>(&a)) equal = *x == std::get<bool>(b);
            else if (auto x = std::get_if<std::string>(&a)) equal = *x == std::get<std::string>(b);
            else if (auto x = std::get_if<std::shared_ptr<Object>>(&a)) equal = *x == std::get<std::shared_ptr<Object>>(b);
            else if (auto x = std::get_if<std::shared_ptr<ListValue>>(&a)) equal = *x == std::get<std::shared_ptr<ListValue>>(b);
        } else equal = numberOf(a) == numberOf(b);
        return op == "==" ? equal : !equal;
    }
    if (op == "and" || op == "&&") return truthy(a) && truthy(b);
    if (op == "or" || op == "||") return truthy(a) || truthy(b);
    if (std::holds_alternative<double>(a) || std::holds_alternative<double>(b)) {
        const double x = numberOf(a), y = numberOf(b);
        if (op == "<") return x < y;
        if (op == "<=") return x <= y;
        if (op == ">") return x > y;
        if (op == ">=") return x >= y;
        if (op == "+") return x + y;
        if (op == "-") return x - y;
        if (op == "*") return x * y;
        if (op == "/") return y == 0.0 ? 0.0 : x / y;
        if (op == "%") return y == 0.0 ? 0.0 : std::fmod(x, y);
    }
    const bool unsignedPair = std::holds_alternative<std::uint64_t>(a) && std::holds_alternative<std::uint64_t>(b);
    if (unsignedPair) {
        const auto x = std::get<std::uint64_t>(a), y = std::get<std::uint64_t>(b);
        if (op == "<") return x < y;
        if (op == "<=") return x <= y;
        if (op == ">") return x > y;
        if (op == ">=") return x >= y;
        if (op == "+") return x + y;
        if (op == "-") return x - y;
        if (op == "*") return x * y;
        if (op == "/") return y == 0 ? std::uint64_t{0} : x / y;
        if (op == "%") return y == 0 ? std::uint64_t{0} : x % y;
        if (op == "&") return x & y;
        if (op == "|") return x | y;
        if (op == "^") return x ^ y;
        if (op == "<<") return x << (y & 63);
        if (op == ">>") return x >> (y & 63);
    }
    if (op == "<") return numberOf(a) < numberOf(b);
    if (op == "<=") return numberOf(a) <= numberOf(b);
    if (op == ">") return numberOf(a) > numberOf(b);
    if (op == ">=") return numberOf(a) >= numberOf(b);
    if (op == "&") return static_cast<std::int64_t>(static_cast<std::uint64_t>(numberOf(a)) & static_cast<std::uint64_t>(numberOf(b)));
    if (op == "|") return static_cast<std::int64_t>(static_cast<std::uint64_t>(numberOf(a)) | static_cast<std::uint64_t>(numberOf(b)));
    if (op == "^") return static_cast<std::int64_t>(static_cast<std::uint64_t>(numberOf(a)) ^ static_cast<std::uint64_t>(numberOf(b)));
    if (op == "<<") return static_cast<std::int64_t>(static_cast<std::uint64_t>(numberOf(a)) << (static_cast<std::uint64_t>(numberOf(b)) & 63));
    if (op == ">>") return static_cast<std::int64_t>(static_cast<std::int64_t>(numberOf(a)) >> (static_cast<std::uint64_t>(numberOf(b)) & 63));
    const double x = numberOf(a), y = numberOf(b);
    if (op == "+") return static_cast<std::int64_t>(x + y);
    if (op == "-") return static_cast<std::int64_t>(x - y);
    if (op == "*") return static_cast<std::int64_t>(x * y);
    if (op == "/") return y == 0 ? std::int64_t{0} : static_cast<std::int64_t>(x / y);
    if (op == "%") return y == 0 ? std::int64_t{0} : static_cast<std::int64_t>(static_cast<std::int64_t>(x) % static_cast<std::int64_t>(y));
    return {};
}
Value unary(const std::string& op, const Value& value) {
    if (op == "!") return !truthy(value);
    if (op == "~") return static_cast<std::int64_t>(~static_cast<std::uint64_t>(numberOf(value)));
    if (op == "-") {
        if (const auto* floating = std::get_if<double>(&value)) return -*floating;
        return -static_cast<std::int64_t>(numberOf(value));
    }
    return value;
}
Value argument(void** args, int index) { return args && index >= 0 ? valueOf(args[index]) : Value{}; }
std::string argumentString(void** args, int index) { return stringOf(argument(args, index)); }
int argumentInt(void** args, int index) { return static_cast<int>(numberOf(argument(args, index))); }
// ben yaptım opencode - 2026-11-14 - stdlib sayisal arguman yardimcilari
std::int64_t ocInt(void** args, int index) {
    const Value value = argument(args, index);
    if (const auto* item = std::get_if<std::int64_t>(&value)) return *item;
    if (const auto* item = std::get_if<std::uint64_t>(&value)) return static_cast<std::int64_t>(*item);
    if (const auto* item = std::get_if<double>(&value)) return static_cast<std::int64_t>(*item);
    if (const auto* item = std::get_if<bool>(&value)) return *item ? 1 : 0;
    return 0;
}
double ocNum(void** args, int index) { return numberOf(argument(args, index)); }
std::uint64_t argumentUInt64(void** args, int index) {
    const Value value = argument(args, index);
    if (const auto* item = std::get_if<std::uint64_t>(&value)) return *item;
    if (const auto* item = std::get_if<std::int64_t>(&value)) return static_cast<std::uint64_t>(*item);
    if (const auto* item = std::get_if<double>(&value)) return static_cast<std::uint64_t>(std::max(0.0, *item));
    return 0;
}
std::string colorKey(const std::string& input) {
    std::string out;
    for (std::size_t i = 0; i < input.size();) {
        const auto a = static_cast<unsigned char>(input[i]);
        if (i + 1 < input.size()) {
            const auto b = static_cast<unsigned char>(input[i + 1]);
            char replacement = 0;
            if (a == 0xC3) {
                if (b == 0xA7) replacement = 'C';
                else if (b == 0x87) replacement = 'C';
                else if (b == 0xB6 || b == 0x96) replacement = 'O';
                else if (b == 0xBC || b == 0x9C) replacement = 'U';
            } else if (a == 0xC4) {
                if (b == 0x9F || b == 0x9E) replacement = 'G';
                else if (b == 0xB1 || b == 0xB0) replacement = 'I';
            } else if (a == 0xC5 && (b == 0x9F || b == 0x9E)) replacement = 'S';
            if (replacement) { out.push_back(replacement); i += 2; continue; }
        }
        if ((a >= 'a' && a <= 'z') || (a >= 'A' && a <= 'Z') || (a >= '0' && a <= '9'))
            out.push_back(static_cast<char>(std::toupper(a)));
        ++i;
    }
    return out;
}
struct PaletteEntry { const char* aliases; int family; int slot; };
static const PaletteEntry namedPalette[] = {
    {"Kırmızı|Pure Red", 0, 0},
    {"Al Kırmızı|Crimson", 0, 1},
    {"Bordo|Burgundy", 0, 2},
    {"Gül Kırmızısı|Rose Red", 0, 3},
    {"Tuğla Kırmızısı|Brick Red", 0, 4},
    {"Mercan Kırmızısı|Coral Red", 0, 5},
    {"Kiremit Rengi|Terra Cotta", 0, 6},
    {"Vişne Çürüğü|Cerise", 0, 7},
    {"Yakut Rengi|Ruby", 0, 8},
    {"Nar Çiçeği|Vermilion", 0, 9},
    {"Şarap Rengi|Wine", 0, 10},
    {"Kardinal Kırmızısı|Cardinal", 0, 11},
    {"Kestane Kırmızısı|Maroon", 0, 12},
    {"Ateş Kırmızısı|Fire Engine Red", 0, 13},
    {"Pas Rengi|Rust", 0, 14},
    {"Kızıl|Auburn", 0, 15},
    {"Kına Rengi|Henna", 0, 16},
    {"Kan Kırmızısı|Blood Red", 0, 17},
    {"Domates Kırmızısı|Tomato", 0, 18},
    {"Pers Kırmızısı|Persian Red", 0, 19},
    {"Venedik Kırmızısı|Venetian Red", 0, 20},
    {"Hindistan Kırmızısı|Indian Red", 0, 21},
    {"Saten Kırmızı|Satin Red", 0, 22},
    {"Lava Rengi|Lava", 0, 23},
    {"Ahududu|Raspberry", 0, 24},
    {"Çilek Kırmızısı|Strawberry", 0, 25},
    {"Kızılcık|Cranberry", 0, 26},
    {"Elma Kırmızısı|Candy Apple Red", 0, 27},
    {"Biber Kırmızısı|Chili Pepper", 0, 28},
    {"Garnet|Garnet", 0, 29},
    {"Kızıl Kahve|Mahogany", 0, 30},
    {"Sekoya Rengi|Redwood", 0, 31},
    {"Somon Kırmızısı|Salmon Red", 0, 32},
    {"Günbatımı Kırmızısı|Sunset Red", 0, 33},
    {"Begonvil Rengi|Bougainvillea", 0, 34},
    {"Kızıl Meyve|Berry", 0, 35},
    {"Maun|Russet", 0, 36},
    {"Sancak Kırmızısı|Pennant Red", 0, 37},
    {"Gül Kurusu (Koyu)|Dark Rose", 0, 38},
    {"Gelincik Kırmızısı|Poppy", 0, 39},
    {"Kızıl Elma|Crimson Glaze", 0, 40},
    {"Köz Rengi|Ember", 0, 41},
    {"Kırmızı Toprak|Red Ochre", 0, 42},
    {"Bordo Şarabı|Claret", 0, 43},
    {"Kızıl Şafak|Red Dawn", 0, 44},
    {"Kadife Kırmızı|Velvet Red", 0, 45},
    {"Sfenks Kırmızısı|Sphinx Red", 0, 46},
    {"Lal Rengi|Scarlet", 0, 47},
    {"Kızılötesi|Infrared", 0, 48},
    {"Rubin|Rubine Red", 0, 49},
    {"Turuncu|Orange", 1, 0},
    {"Mandalina|Tangerine", 1, 1},
    {"Kehribar|Amber", 1, 2},
    {"Kavun Rengi|Melon", 1, 3},
    {"Kayısı Rengi|Apricot", 1, 4},
    {"Şeftali Rengi|Peach", 1, 5},
    {"Bal Rengi|Honey", 1, 6},
    {"Somon|Salmon", 1, 7},
    {"Balkabağı|Pumpkin", 1, 8},
    {"Havuç Rengi|Carrot", 1, 9},
    {"Bakır Rengi|Copper", 1, 10},
    {"Bronz|Bronze", 1, 11},
    {"Zencefil Rengi|Ginger", 1, 12},
    {"Kadife Çiçeği|Marigold", 1, 13},
    {"Safran|Saffron", 1, 14},
    {"Yanık Turuncu|Burnt Orange", 1, 15},
    {"Pers Turuncusu|Persian Orange", 1, 16},
    {"Mercan|Coral", 1, 17},
    {"Açık Mercan|Light Coral", 1, 18},
    {"Koyu Somon|Dark Salmon", 1, 19},
    {"Ateş Turuncusu|Flame", 1, 20},
    {"Zinnober|Cinnabar", 1, 21},
    {"Altın Turuncu|Gold Orange", 1, 22},
    {"Güneş Turuncusu|Sun Orange", 1, 23},
    {"Çöl Turuncusu|Desert Orange", 1, 24},
    {"Siena Toprağı|Raw Sienna", 1, 25},
    {"Yanık Siena|Burnt Sienna", 1, 26},
    {"Hardal Turuncusu|Mustard Orange", 1, 27},
    {"Kaplan Gözü|Tiger's Eye", 1, 28},
    {"Mango Rengi|Mango", 1, 29},
    {"Papaya|Papaya", 1, 30},
    {"Koyot Rengi|Coyote", 1, 31},
    {"Bronz Turuncu|Bronze Orange", 1, 32},
    {"Paslı Turuncu|Rusty Orange", 1, 33},
    {"Açık Şeftali|Light Peach", 1, 34},
    {"Koyu Mandalina|Dark Tangerine", 1, 35},
    {"Güz Yaprağı|Autumn Leaf", 1, 36},
    {"Toprak Turuncusu|Clay", 1, 37},
    {"Neon Turuncu|Neon Orange", 1, 38},
    {"Egzotik Turuncu|Exotic Orange", 1, 39},
    {"Sıcak Turuncu|Hot Orange", 1, 40},
    {"Çöl Kumu|Desert Sand", 1, 41},
    {"Kızıl Turuncu|Red Orange", 1, 42},
    {"Günbatımı|Sunset", 1, 43},
    {"Sarımsı Turuncu|Yellow Orange", 1, 44},
    {"Koyu Kehribar|Dark Amber", 1, 45},
    {"Güneş Işığı|Sunburst", 1, 46},
    {"Altın Tozu|Gold Dust", 1, 47},
    {"Marmelat|Marmalade", 1, 48},
    {"Tantor|Tantor Orange", 1, 49},
    {"Sarı|Yellow", 2, 0},
    {"Limon Sarısı|Lemon Yellow", 2, 1},
    {"Altın Sarısı|Gold", 2, 2},
    {"Hardal Sarısı|Mustard", 2, 3},
    {"Kanarya Sarısı|Canary", 2, 4},
    {"Muz Sarısı|Banana", 2, 5},
    {"Civciv Sarısı|Chicks Yellow", 2, 6},
    {"Krem Rengi|Cream", 2, 7},
    {"Bej|Beige", 2, 8},
    {"Fildişi|Ivory", 2, 9},
    {"Saman Sarısı|Straw", 2, 10},
    {"Mısır Sarısı|Corn", 2, 11},
    {"Nergis Sarısı|Daffodil", 2, 12},
    {"Papatya Sarısı|Daisy", 2, 13},
    {"Güneş Sarısı|Sunny Yellow", 2, 14},
    {"Keten Rengi|Flax", 2, 15},
    {"Sarışın|Blonde", 2, 16},
    {"Şampanya|Champagne", 2, 17},
    {"Tereyağı Rengi|Butter", 2, 18},
    {"Vani̇lya|Vanilla", 2, 19},
    {"Bambu Rengi|Bamboo", 2, 20},
    {"Keten Tohumu|Linseed", 2, 21},
    {"Koyu Altın|Dark Gold", 2, 22},
    {"Açık Sarı|Light Yellow", 2, 23},
    {"Zeytuni Sarı|Olive Yellow", 2, 24},
    {"Neon Sarı|Neon Yellow", 2, 25},
    {"Fosforlu Sarı|Fluorescent Yellow", 2, 26},
    {"Toprak Sarısı|Ochre", 2, 27},
    {"Mimoza Rengi|Mimosa", 2, 28},
    {"Altın Başak|Goldenrod", 2, 29},
    {"Açık Altın Başak|Light Goldenrod", 2, 30},
    {"Koyu Altın Başak|Dark Goldenrod", 2, 31},
    {"Askeri Sarı|Khaki Yellow", 2, 32},
    {"Kum Sarısı|Sand Yellow", 2, 33},
    {"Soluk Sarı|Pale Yellow", 2, 34},
    {"Kükürt Sarısı|Sulphur", 2, 35},
    {"Asit Sarısı|Acid Yellow", 2, 36},
    {"Cadillac Sarısı|Cadillac Yellow", 2, 37},
    {"Kavun Sarısı|Melon Yellow", 2, 38},
    {"Günebakan|Sunflower", 2, 39},
    {"Esmer Şeker|Brown Sugar", 2, 40},
    {"Karamel|Caramel", 2, 41},
    {"Kaju Rengi|Cashew", 2, 42},
    {"Toz Sarı|Pastel Yellow", 2, 43},
    {"Lazer Sarısı|Laser Yellow", 2, 44},
    {"Elektrik Sarısı|Electric Yellow", 2, 45},
    {"Sitrin|Citrine", 2, 46},
    {"Kutsal Altın|Vegas Gold", 2, 47},
    {"Metalik Altın|Metallic Gold", 2, 48},
    {"Eski Altın|Old Gold", 2, 49},
    {"Yeşil|Green", 3, 0},
    {"Çimen Yeşili|Grass Green", 3, 1},
    {"Zümrüt Yeşili|Emerald", 3, 2},
    {"Zeytin Yeşili|Olive", 3, 3},
    {"Nane Yeşili|Mint", 3, 4},
    {"Adaçayı Yeşili|Sage", 3, 5},
    {"Fıstık Yeşili|Pistachio", 3, 6},
    {"Yosun Yeşili|Moss Green", 3, 7},
    {"Koyu Yeşil|Dark Green", 3, 8},
    {"Orman Yeşili|Forest Green", 3, 9},
    {"Çam Yeşili|Pine Green", 3, 10},
    {"Deniz Yeşili|Sea Green", 3, 11},
    {"Açık Deniz Yeşili|Light Sea Green", 3, 12},
    {"Koyu Deniz Yeşili|Dark Sea Green", 3, 13},
    {"Haki|Khaki", 3, 14},
    {"Asker Yeşili|Army Green", 3, 15},
    {"Limon Yeşili|Lime", 3, 16},
    {"Misket Limonu|Lime Green", 3, 17},
    {"Avokado Yeşili|Avocado", 3, 18},
    {"Defne Yeşili|Laurel", 3, 19},
    {"Fesleğen Yeşili|Basil", 3, 20},
    {"Bambu Yeşili|Bamboo Green", 3, 21},
    {"Yeşim Taşı|Jade", 3, 22},
    {"Turkuaz Yeşili|Turquoise Green", 3, 23},
    {"Kaktüs Yeşili|Cactus", 3, 24},
    {"Okaliptüs|Eucalyptus", 3, 25},
    {"Eğrelti Otu|Fern", 3, 26},
    {"Kereviz Rengi|Celery", 3, 27},
    {"Enginar Yeşili|Artichoke", 3, 28},
    {"Soluk Yeşil|Pale Green", 3, 29},
    {"Pastel Yeşil|Pastel Green", 3, 30},
    {"Neon Yeşil|Neon Green", 3, 31},
    {"Fosforlu Yeşil|Cyber Green", 3, 32},
    {"Elektrik Yeşili|Electric Green", 3, 33},
    {"Yonca Yeşili|Clover", 3, 34},
    {"Zümrüt Kıvılcımı|Emerald Spark", 3, 35},
    {"Avcı Yeşili|Hunter Green", 3, 36},
    {"Militan Yeşil|Military Green", 3, 37},
    {"Mersin Yeşili|Myrtle", 3, 38},
    {"Ofis Yeşili|Office Green", 3, 39},
    {"Fars Yeşili|Persian Green", 3, 40},
    {"Bataklık Yeşili|Swamp Green", 3, 41},
    {"Çay Yeşili|Tea Green", 3, 42},
    {"Zümrüt Karası|Black Emerald", 3, 43},
    {"Koyu Zeytin|Dark Olive Green", 3, 44},
    {"Zeytin Dalı|Olive Drab", 3, 45},
    {"Bahar Yeşili|Spring Green", 3, 46},
    {"Orkide Yeşili|Orchid Green", 3, 47},
    {"Malakit Yeşili|Malachite", 3, 48},
    {"Karahindiba Yeşili|Dandelion Green", 3, 49},
    {"Su Yeşili|Aqua Green", 3, 50},
    {"Deniz köpüğü|Seafoam Green", 3, 51},
    {"Nane Şekeri|Peppermint", 3, 52},
    {"Yeşil Çay|Green Tea", 3, 53},
    {"Briket Yeşili|Brunswick Green", 3, 54},
    {"Krom Yeşili|Chrome Green", 3, 55},
    {"Kelt Yeşili|Celtic Green", 3, 56},
    {"Bukalemun Yeşili|Chameleon", 3, 57},
    {"Timsah Yeşili|Crocodile", 3, 58},
    {"Kurbağa Yeşili|Frog Green", 3, 59},
    {"Yılan Yeşili|Serpent", 3, 60},
    {"Liken Yeşili|Lichen", 3, 61},
    {"Alp Yeşili|Alpine Green", 3, 62},
    {"Yaprak Yeşili|Leaf Green", 3, 63},
    {"Kivi Rengi|Kiwi", 3, 64},
    {"Yeşil Elma|Green Apple", 3, 65},
    {"Hücre Yeşili|Cell Green", 3, 66},
    {"Vadi Yeşili|Valley Green", 3, 67},
    {"Göl Yeşili|Lake Green", 3, 68},
    {"Viridiyan|Viridian", 3, 69},
    {"İslam Yeşili|Islamic Green", 3, 70},
    {"Kenevir Yeşili|Hemp", 3, 71},
    {"Koyu Avcı Yeşili|Dark Hunter", 3, 72},
    {"Kül Yeşili|Ash Green", 3, 73},
    {"Eski Yeşil|Old Green", 3, 74},
    {"Mavi|Blue", 4, 0},
    {"Gök Mavisi|Sky Blue", 4, 1},
    {"Deniz Mavisi|Navy Blue", 4, 2},
    {"Gece Mavisi|Midnight Blue", 4, 3},
    {"Turkuaz|Turquoise", 4, 4},
    {"Safir|Sapphire", 4, 5},
    {"Kobalt Mavisi|Cobalt", 4, 6},
    {"Çivit Mavisi|Indigo", 4, 7},
    {"Bebek Mavisi|Baby Blue", 4, 8},
    {"Buz Mavisi|Ice Blue", 4, 9},
    {"Çelik Mavisi|Steel Blue", 4, 10},
    {"Kraliyet Mavisi|Royal Blue", 4, 11},
    {"Elektrik Mavisi|Electric Blue", 4, 12},
    {"Camgöbeği|Cyan", 4, 13},
    {"Açık Camgöbeği|Light Cyan", 4, 14},
    {"Koyu Camgöbeği|Dark Cyan", 4, 15},
    {"Su Mavisi|Aqua", 4, 16},
    {"Açık Su Mavisi|Aqueous", 4, 17},
    {"Okyanus Mavisi|Ocean Blue", 4, 18},
    {"Derin Okyanus|Deep Ocean", 4, 19},
    {"Ege Mavisi|Aegean Blue", 4, 20},
    {"Akdeniz Mavisi|Mediterranean", 4, 21},
    {"Prusya Mavisi|Prussian Blue", 4, 22},
    {"Pudra Mavisi|Powder Blue", 4, 23},
    {"Kot Mavisi|Denim", 4, 24},
    {"Arduvaz Mavisi|Slate Blue", 4, 25},
    {"Açık Arduvaz Mavisi|Light Slate Blue", 4, 26},
    {"Koyu Arduvaz Mavisi|Dark Slate Blue", 4, 27},
    {"Peygamber Çiçeği|Cornflower Blue", 4, 28},
    {"Harp Okulu Mavisi|Cadet Blue", 4, 29},
    {"Gökçe|Cerulean", 4, 30},
    {"Gök mavisi (Açık)|Aero Blue", 4, 31},
    {"Hava Kuvvetleri Mavisi|Air Force Blue", 4, 32},
    {"Alice Mavisi|Alice Blue", 4, 33},
    {"Azor Mavisi|Azure", 4, 34},
    {"Koyu Azor|Dark Azure", 4, 35},
    {"Bermuda Mavisi|Bermuda", 4, 36},
    {"Bizans Mavisi|Byzantine Blue", 4, 37},
    {"Carolina Mavisi|Carolina Blue", 4, 38},
    {"Columbia Mavisi|Columbia Blue", 4, 39},
    {"Papatya Mavisi|Daisy Blue", 4, 40},
    {"Mısır Mavisi|Egyptian Blue", 4, 41},
    {"Unutma Beni Çiçeği|Forget-Me-Not", 4, 42},
    {"Buzul Mavisi|Glacier", 4, 43},
    {"Kot Karası|Dark Denim", 4, 44},
    {"Gökkuşağı Mavisi|Iris Blue", 4, 45},
    {"Maya Mavisi|Maya Blue", 4, 46},
    {"Oksijen Mavisi|Oxygen Blue", 4, 47},
    {"Yelken Mavisi|Sailor Blue", 4, 48},
    {"Arktik Mavi|Arctic Blue", 4, 49},
    {"Kutup Mavisi|Polar Blue", 4, 50},
    {"Duman Mavisi|Smoke Blue", 4, 51},
    {"Sis Mavisi|Mist Blue", 4, 52},
    {"Fırtına Mavisi|Storm Blue", 4, 53},
    {"Petrol Mavisi|Teal", 4, 54},
    {"Koyu Petrol|Dark Teal", 4, 55},
    {"Açık Petrol|Light Teal", 4, 56},
    {"Ördek Başı Yeşil/Mavi|Mallard", 4, 57},
    {"Kadet Mavisi|Cadet", 4, 58},
    {"Derin Gök Mavisi|Deep Sky Blue", 4, 59},
    {"Dodger Mavisi|Dodger Blue", 4, 60},
    {"Göl Mavisi|Lagoon", 4, 61},
    {"Tropikal Mavi|Tropical Blue", 4, 62},
    {"Uluslararası Klein Mavisi|IKB", 4, 63},
    {"Mavi Tebeşir|Blue Chalk", 4, 64},
    {"Yıldız Işığı|Starlight Blue", 4, 65},
    {"Geceyarısı|Midnight", 4, 66},
    {"Gölge Mavisi|Shadow Blue", 4, 67},
    {"Mavi Gri|Blue Gray", 4, 68},
    {"Safir Parıltısı|Sapphire Spark", 4, 69},
    {"Derin Safir|Dark Sapphire", 4, 70},
    {"Gök Tozu|Sky Dust", 4, 71},
    {"Zirkon Mavisi|Zircon", 4, 72},
    {"Denizci|Marine Blue", 4, 73},
    {"Kaptan Mavisi|Captain Blue", 4, 74},
    {"Mor|Purple", 5, 0},
    {"Menekşe|Violet", 5, 1},
    {"Eflatun|Magenta", 5, 2},
    {"Lila|Lilac", 5, 3},
    {"Lavanta|Lavender", 5, 4},
    {"Mürdüm|Plum", 5, 5},
    {"Fuya|Fuchsia", 5, 6},
    {"Pembe|Pink", 5, 7},
    {"Toz Pembe|Powder Pink", 5, 8},
    {"Şeker Pembesi|Baby Pink", 5, 9},
    {"Sıcak Pembe|Hot Pink", 5, 10},
    {"Neon Pembe|Neon Pink", 5, 11},
    {"Çingene Pembesi|Shocking Pink", 5, 12},
    {"Gül Kurusu|Rose", 5, 13},
    {"Açık Pembe|Light Pink", 5, 14},
    {"Koyu Pembe|Dark Pink", 5, 15},
    {"İnci Pembesi|Pearl Pink", 5, 16},
    {"Somon Pembesi|Salmon Pink", 5, 17},
    {"Koyu Mor|Dark Purple", 5, 18},
    {"Açık Mor|Light Purple", 5, 19},
    {"Orkide|Orchid", 5, 20},
    {"Koyu Orkide|Dark Orchid", 5, 21},
    {"Açık Orkide|Light Orchid", 5, 22},
    {"Deli Mor|Amethyst", 5, 23},
    {"Yaban Mersini|Mulberry", 5, 24},
    {"Böğürtlen|Blackberry", 5, 25},
    {"Patlıcan Moru|Eggplant / Aubergine", 5, 26},
    {"Üzüm Moru|Grape", 5, 27},
    {"Kraliyet Moru|Royal Purple", 5, 28},
    {"Bizans Moru|Byzantine", 5, 29},
    {"Gelinciğin Moru|Mauve", 5, 30},
    {"Koyu Eflatun|Dark Magenta", 5, 31},
    {"Koyu Menekşe|Dark Violet", 5, 32},
    {"Mavi Menekşe|Blue Violet", 5, 33},
    {"Kızıl Menekşe|Red Violet", 5, 34},
    {"Yabani Çilek|Wild Strawberry", 5, 35},
    {"Karnaval Pembesi|Carnival Pink", 5, 36},
    {"Karpuz Pembesi|Watermelon", 5, 37},
    {"Balerin Pembesi|Ballet Slipper", 5, 38},
    {"Allık Pembesi|Blush", 5, 39},
    {"Pamuk Şekeri|Cotton Candy", 5, 40},
    {"Sakız Pembesi|Bubblegum", 5, 41},
    {"Koral Pembe|Coral Pink", 5, 42},
    {"Gül Ağacı|Rosewood", 5, 43},
    {"Fildişi Pembe|Ivory Pink", 5, 44},
    {"Çöl Gülü|Desert Rose", 5, 45},
    {"Sisli Pembe|Misty Rose", 5, 46},
    {"Kadife Mor|Velvet Purple", 5, 47},
    {"Gölge Moru|Shadow Violet", 5, 48},
    {"Pudra Moru|Thistle", 5, 49},
    {"Koyu Lavanta|Dark Lavender", 5, 50},
    {"Lavanta Grisi|Lavender Gray", 5, 51},
    {"Lavanta Mavisi|Lavender Blue", 5, 52},
    {"Şarap Pembesi|Wine Pink", 5, 53},
    {"Şakayık Pembesi|Peony", 5, 54},
    {"Klorofil Pembesi|Acid Pink", 5, 55},
    {"Kozmos Pembesi|Cosmos", 5, 56},
    {"Kızılcık Pembesi|Cranberry Pink", 5, 57},
    {"Koyu Mürdüm|Dark Plum", 5, 58},
    {"Açık Mürdüm|Light Plum", 5, 59},
    {"Camgöbeği Pembe|Cyan Pink", 5, 60},
    {"Tango Pembesi|Tango Pink", 5, 61},
    {"Kızıl Lale|Tulip", 5, 62},
    {"Sümbül Rengi|Hyacinth", 5, 63},
    {"Gökkuşağı Pembesi|Rainbow Pink", 5, 64},
    {"Koyu Lila|Dark Lilac", 5, 65},
    {"Menekşe Kalbi|Violet Heart", 5, 66},
    {"Gence Moru|Gence Purple", 5, 67},
    {"Fuji Moru|Fuji Purple", 5, 68},
    {"Ütopya Moru|Utopia Purple", 5, 69},
    {"Gece Pembesi|Midnight Pink", 5, 70},
    {"Gül Badesi|Rose Nectar", 5, 71},
    {"Kadife Pembe|Velvet Pink", 5, 72},
    {"Gül Taneleri|Rose Petal", 5, 73},
    {"Kenevir Pembesi|Hemp Pink", 5, 74},
    {"Kahverengi|Brown", 6, 0},
    {"Çikolata Kahvesi|Chocolate", 6, 1},
    {"Koyu Çikolata|Dark Chocolate", 6, 2},
    {"Sütlü Çikolata|Milk Chocolate", 6, 3},
    {"Kakao Rengi|Cocoa", 6, 4},
    {"Kahve Rengi|Coffee", 6, 5},
    {"Espresso|Espresso", 6, 6},
    {"Latte|Latte", 6, 7},
    {"Kapuçino|Cappuccino", 6, 8},
    {"Tarçın Rengi|Cinnamon", 6, 9},
    {"Kestane|Chestnut", 6, 10},
    {"Fındık Kabuğu|Hazelnut", 6, 11},
    {"Ceviz Rengi|Walnut", 6, 12},
    {"Meşe|Oak", 6, 13},
    {"Maun Kahvesi|Mahogany Brown", 6, 14},
    {"Tik Ağacı|Teak", 6, 15},
    {"Sedir Ağacı|Cedar", 6, 16},
    {"Toprak Rengi|Earth", 6, 17},
    {"Çamur Rengi|Mud", 6, 18},
    {"Kil Rengi|Clay Brown", 6, 19},
    {"Kum Rengi|Sand", 6, 20},
    {"Çöl Kumu Kahvesi|Desert Sand", 6, 21},
    {"Vizon|Mink", 6, 22},
    {"Köstebek Rengi|Taupe", 6, 23},
    {"Koyu Köstebek|Dark Taupe", 6, 24},
    {"Açık Köstebek|Light Taupe", 6, 25},
    {"Deve Tüyü|Camel", 6, 26},
    {"Taba|Tan", 6, 27},
    {"Koyu Taba|Dark Tan", 6, 28},
    {"Süet|Suede", 6, 29},
    {"Hakiki Deri|Leather", 6, 30},
    {"Semer Kahvesi|Saddle Brown", 6, 31},
    {"Sepya|Sepia", 6, 32},
    {"Koyu Sepya|Dark Sepia", 6, 33},
    {"Bizon|Bison", 6, 34},
    {"Kunduz Rengi|Beaver", 6, 35},
    {"Samur Rengi|Sable", 6, 36},
    {"Geyik Kahvesi|Fawn", 6, 37},
    {"Bronz Kahve|Bronze Brown", 6, 38},
    {"Bakır Kahvesı|Copper Brown", 6, 39},
    {"Kızıl Kahve|Auburn Brown", 6, 40},
    {"Karamel Kahvesi|Caramel Brown", 6, 41},
    {"Pecan Cevizi|Pecan", 6, 42},
    {"Badem Rengi|Almond", 6, 43},
    {"Kavrulmuş Badem|Blanched Almond", 6, 44},
    {"Bisküvi Rengi|Biscuit", 6, 45},
    {"Kurabiye Rengi|Cookie", 6, 46},
    {"Buğday Rengi|Wheat", 6, 47},
    {"Yulaf Rengi|Oatmeal", 6, 48},
    {"Çavdar Rengi|Rye", 6, 49},
    {"Zencefilli Kurabiye|Gingerbread", 6, 50},
    {"Pekmez Rengi|Molasses", 6, 51},
    {"Akçaağaç Şurubu|Maple", 6, 52},
    {"Bal Kahvesi|Honey Brown", 6, 53},
    {"Kehribar Kahvesi|Amber Brown", 6, 54},
    {"Haki Kahve|Khaki Brown", 6, 55},
    {"Zeytuni Kahve|Olive Brown", 6, 56},
    {"Hardal Kahvesi|Mustard Brown", 6, 57},
    {"Kül Kahvesi|Ash Brown", 6, 58},
    {"Toz Kahve|Dusty Brown", 6, 59},
    {"Siyah|Black", 7, 0},
    {"Kömür Siyahı|Charcoal", 7, 1},
    {"Karbon Siyahı|Carbon Black", 7, 2},
    {"Jet Siyahı|Jet Black", 7, 3},
    {"Obsidiyen|Obsidian", 7, 4},
    {"Oniks|Onyx", 7, 5},
    {"Mürekkep Siyahı|Ink Black", 7, 6},
    {"Zift Rengi|Pitch Black", 7, 7},
    {"Gece Karası|Night Black", 7, 8},
    {"Kuzgun Siyahı|Raven", 7, 9},
    {"Kurum Siyahı|Soot", 7, 10},
    {"Petrol Siyahı|Oil Black", 7, 11},
    {"Gölge Siyahı|Shadow Black", 7, 12},
    {"Eski Siyah|Old Black", 7, 13},
    {"Yumuşak Siyah|Soft Black", 7, 14},
    {"Beyaz|White", 7, 15},
    {"Kar Beyazı|Snow White", 7, 16},
    {"Tebeşir Beyazı|Chalk White", 7, 17},
    {"Süt Beyazı|Milk White", 7, 18},
    {"Kağıt Beyazı|Paper White", 7, 19},
    {"Hayalet Beyazı|Ghost White", 7, 20},
    {"Çiçek Beyazı|Floral White", 7, 21},
    {"Antik Beyaz|Antique White", 7, 22},
    {"Navajo Beyazı|Navajo White", 7, 23},
    {"Keten Beyazı|Linen White", 7, 24},
    {"Duman Beyazı|Smoke White", 7, 25},
    {"İnci Beyazı|Pearl White", 7, 26},
    {"Alabaster|Alabaster", 7, 27},
    {"Porselen Beyazı|Porcelain", 7, 28},
    {"Deniz Kabuğu|Seashell", 7, 29},
    {"Fildişi Sahili|Ivory Coast", 7, 30},
    {"Kemik Rengi|Bone", 7, 31},
    {"Parşömen Rengi|Parchment", 7, 32},
    {"Gri|Gray", 7, 33},
    {"Açık Gri|Light Gray", 7, 34},
    {"Koyu Gri|Dark Gray", 7, 35},
    {"Gümüş Rengi|Silver", 7, 36},
    {"Metalik Gümüş|Metallic Silver", 7, 37},
    {"Platin|Platinum", 7, 38},
    {"Arduvaz Grisi|Slate Gray", 7, 39},
    {"Açık Arduvaz Grisi|Light Slate Gray", 7, 40},
    {"Koyu Arduvaz Grisi|Dark Slate Gray", 7, 41},
    {"Duman Grisi|Smoke Gray", 7, 42},
    {"Sis Grisi|Mist Gray", 7, 43},
    {"Bulut Grisi|Cloud Gray", 7, 44},
    {"Fırtına Grisi|Storm Gray", 7, 45},
    {"Kül Grisi|Ash Gray", 7, 46},
    {"Kurşun Grisi|Lead", 7, 47},
    {"Grafit|Graphite", 7, 48},
    {"Çelik Grisi|Steel Gray", 7, 49},
    {"Demir Grisi|Iron Gray", 7, 50},
    {"Beton Grisi|Concrete", 7, 51},
    {"Asfalt Rengi|Asphalt", 7, 52},
    {"Çakıl Grisi|Pebble", 7, 53},
    {"Kaya Grisi|Rock", 7, 54},
    {"Granit|Granite", 7, 55},
    {"Mermer Grisi|Marble", 7, 56},
    {"Toz Gri|Dust Gray", 7, 57},
    {"Güvercin Grisi|Dove Gray", 7, 58},
    {"Gölge Grisi|Shadow Gray", 7, 59},
    {"Fare Grisi|Mouse Gray", 7, 60},
    {"Sanayi Grisi|Industrial Gray", 7, 61},
    {"Mat Gümüş|Matte Silver", 7, 62},
    {"Gümüş Parıltısı|Silver Fox", 7, 63},
    {"Kozmik Gri|Cosmic Gray", 7, 64},
};
COLORREF paletteTone(int family, int slot) {
    static constexpr double hues[] = {0.0, 28.0, 52.0, 125.0, 210.0, 295.0, 28.0, 0.0};
    const double hue = hues[std::clamp(family, 0, 7)] + static_cast<double>((slot * 7) % 9 - 4) * 1.6;
    const double saturation = family == 7 ? 0.025 : family == 6 ? 0.24 + ((slot * 11) % 34) / 100.0 : 0.48 + ((slot * 13) % 38) / 100.0;
    const double lightness = family == 7 ? 0.16 + ((slot * 17) % 70) / 100.0 : 0.23 + ((slot * 19) % 48) / 100.0;
    const double c = (1.0 - std::abs(2.0 * lightness - 1.0)) * saturation;
    const double h = std::fmod(hue + 360.0, 360.0) / 60.0;
    const double x = c * (1.0 - std::abs(std::fmod(h, 2.0) - 1.0));
    double r = 0, g = 0, b = 0;
    if (h < 1) { r = c; g = x; }
    else if (h < 2) { r = x; g = c; }
    else if (h < 3) { g = c; b = x; }
    else if (h < 4) { g = x; b = c; }
    else if (h < 5) { r = x; b = c; }
    else { r = c; b = x; }
    const double m = lightness - c / 2.0;
    return RGB(static_cast<BYTE>(std::clamp((r + m) * 255.0, 0.0, 255.0)),
               static_cast<BYTE>(std::clamp((g + m) * 255.0, 0.0, 255.0)),
               static_cast<BYTE>(std::clamp((b + m) * 255.0, 0.0, 255.0)));
}
std::string rgbHex(unsigned r, unsigned g, unsigned b) {
    char text[8]{};
    std::snprintf(text, sizeof(text), "#%02X%02X%02X", r & 255, g & 255, b & 255);
    return text;
}
COLORREF colorOf(const std::string& color) {
    std::string value = color;
    if (value.size() == 7 && value[0] == '#') {
        try { return RGB(std::stoi(value.substr(1,2), nullptr, 16), std::stoi(value.substr(3,2), nullptr, 16), std::stoi(value.substr(5,2), nullptr, 16)); } catch (...) {}
    }
    const std::string key = colorKey(color);
    if (key == "WHITE" || key == "BEYAZ") return RGB(255,255,255);
    if (key == "RED" || key == "KIRMIZI" || key == "PURERED") return RGB(255,0,0);
    if (key == "GREEN" || key == "YESIL") return RGB(0,128,0);
    if (key == "BLUE" || key == "MAVI") return RGB(0,0,255);
    if (key == "BROWN" || key == "KAHVERENGI") return RGB(139,69,19);
    if (key == "YELLOW" || key == "SARI") return RGB(255,255,0);
    if (key == "BLACK" || key == "SIYAH") return RGB(0,0,0);
    static const StringMap<std::pair<int, int>> palette = [] {
        StringMap<std::pair<int, int>> result;
        for (const auto& item : namedPalette) {
            std::string aliases(item.aliases);
            std::size_t start = 0;
            while (start <= aliases.size()) {
                const std::size_t separator = aliases.find('|', start);
                const std::string alias = aliases.substr(start, separator == std::string::npos ? std::string::npos : separator - start);
                std::size_t wordStart = 0;
                while (wordStart <= alias.size()) {
                    const std::size_t slash = alias.find('/', wordStart);
                    const std::string word = alias.substr(wordStart, slash == std::string::npos ? std::string::npos : slash - wordStart);
                    if (!word.empty()) result.try_emplace(colorKey(word), item.family, item.slot);
                    if (slash == std::string::npos) break;
                    wordStart = slash + 1;
                }
                if (separator == std::string::npos) break;
                start = separator + 1;
            }
        }
        return result;
    }();
    const auto found = palette.find(key);
    return found == palette.end() ? RGB(0,0,0) : paletteTone(found->second.first, found->second.second);
}
std::string colorCall(const std::string& name, void** args, int argc) {
    static const COLORREF fourBit[16] = {
        RGB(0,0,0), RGB(0,0,128), RGB(0,128,0), RGB(0,128,128), RGB(128,0,0), RGB(128,0,128), RGB(128,128,0), RGB(192,192,192),
        RGB(128,128,128), RGB(0,0,255), RGB(0,255,0), RGB(0,255,255), RGB(255,0,0), RGB(255,0,255), RGB(255,255,0), RGB(255,255,255)
    };
    if (name == "index4" && argc == 1) {
        const COLORREF c = fourBit[argumentInt(args, 0) & 15];
        return rgbHex(GetRValue(c), GetGValue(c), GetBValue(c));
    }
    if ((name == "at" || name == "palette") && argc == 1) {
        const std::uint64_t index = argumentUInt64(args, 0);
        if (index >= sizeof(namedPalette) / sizeof(namedPalette[0])) return "#000000";
        const std::string aliases(namedPalette[index].aliases);
        const auto separator = aliases.find('|');
        const COLORREF c = colorOf(aliases.substr(0, separator));
        return rgbHex(GetRValue(c), GetGValue(c), GetBValue(c));
    }
    if ((name == "generate" || name == "spectrum") && argc == 1) {
        constexpr std::uint64_t generatedColors = 10000;
        const std::uint64_t index = argumentUInt64(args, 0);
        if (index >= generatedColors) return "#000000";
        // Odd multiplier permutes RGB565 values without collisions over this range.
        const auto packed = static_cast<std::uint16_t>((index * 40503u + 0x9E37u) & 0xFFFFu);
        return rgbHex(((packed >> 11) & 31) * 255 / 31,
                      ((packed >> 5) & 63) * 255 / 63,
                      (packed & 31) * 255 / 31);
    }
    if ((name == "named" || name == "name") && argc == 1) return argumentString(args, 0);
    if (name == "rgb565" && argc == 1) {
        const auto packed = static_cast<std::uint16_t>(argumentUInt64(args, 0));
        return rgbHex(((packed >> 11) & 31) * 255 / 31, ((packed >> 5) & 63) * 255 / 63, (packed & 31) * 255 / 31);
    }
    if (name == "rgb" && argc == 3) return rgbHex(argumentInt(args, 0), argumentInt(args, 1), argumentInt(args, 2));
    if (name == "hex" && argc == 1) {
        std::string hex = argumentString(args, 0);
        if (hex.size() == 6 && hex[0] != '#') hex.insert(hex.begin(), '#');
        if (hex.size() == 7 && hex[0] == '#') return hex;
    }
    if (name == "rgba64" && argc == 4) return rgbHex(std::min<std::uint64_t>(65535,argumentUInt64(args,0)) >> 8, std::min<std::uint64_t>(65535,argumentUInt64(args,1)) >> 8, std::min<std::uint64_t>(65535,argumentUInt64(args,2)) >> 8);
    if (name == "rgb_depth" && argc == 4) {
        const int bits = std::clamp(argumentInt(args, 3), 4, 64);
        const long double maximum = bits == 64 ? static_cast<long double>(UINT64_MAX) : static_cast<long double>((std::uint64_t{1} << bits) - 1);
        auto scale = [maximum](std::uint64_t channel) { return static_cast<unsigned>(std::clamp(std::llround(static_cast<long double>(channel) * 255.0L / maximum), 0LL, 255LL)); };
        return rgbHex(scale(argumentUInt64(args,0)), scale(argumentUInt64(args,1)), scale(argumentUInt64(args,2)));
    }
    return "#000000";
}
struct DrawCommand { int kind, x, y, w, h, size; std::string text, color; int sx=0,sy=0,sw=0,sh=0; };
struct EngineImage2D { UINT width=0,height=0; HBITMAP bitmap=nullptr; HDC dc=nullptr; HGDIOBJ oldBitmap=nullptr; };
HWND windowHandle = nullptr;
bool windowRunning = false;
COLORREF backgroundColor = RGB(0,0,0);
std::vector<DrawCommand>& drawCommandsState() { static std::vector<DrawCommand> value; return value; }
StringMap<EngineImage2D>& engineImages2DState() { static StringMap<EngineImage2D> value; return value; }
HDC engineBackBufferDC=nullptr;HBITMAP engineBackBufferBitmap=nullptr;HGDIOBJ engineBackBufferOldBitmap=nullptr;int engineBackBufferWidth=0,engineBackBufferHeight=0;
std::wstring& engineTextInputState() { static std::wstring value; return value; }
int engineMouseWheel=0;
int engineTargetFps = 60;
std::wstring engineFontFace = L"Segoe UI";
constexpr UINT ptsTrayCallbackMessage = WM_APP + 0x4A1;
NOTIFYICONDATAW ptsTrayIconData{};
HICON ptsTrayOwnedIcon = nullptr;
bool ptsTrayIconAdded = false;
std::string ptsTrayLastEvent;
struct EngineClockState {
    std::chrono::steady_clock::time_point frameStart = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastFrame = frameStart;
    std::chrono::steady_clock::time_point startTime = frameStart;
};
EngineClockState& engineClockState() { static EngineClockState value; return value; }
double engineDeltaSeconds = 1.0 / 60.0;
std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (n <= 0) return std::wstring(text.begin(), text.end());
    std::wstring result(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), n);
    return result;
}
std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size=WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    if(size<=0)return std::string(text.begin(),text.end());
    std::string result(static_cast<std::size_t>(size),'\0');
    WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),result.data(),size,nullptr,nullptr);
    return result;
}
void releaseImage2D(EngineImage2D& image) {
    if(image.dc&&image.oldBitmap)SelectObject(image.dc,image.oldBitmap);
    if(image.bitmap)DeleteObject(image.bitmap);
    if(image.dc)DeleteDC(image.dc);
    image={};
}
void clearImages2D() { for(auto& item:engineImages2DState())releaseImage2D(item.second);engineImages2DState().clear(); }
void resetBackBuffer() {
    if(engineBackBufferDC&&engineBackBufferOldBitmap)SelectObject(engineBackBufferDC,engineBackBufferOldBitmap);
    if(engineBackBufferBitmap)DeleteObject(engineBackBufferBitmap);
    if(engineBackBufferDC)DeleteDC(engineBackBufferDC);
    engineBackBufferDC=nullptr;engineBackBufferBitmap=nullptr;engineBackBufferOldBitmap=nullptr;engineBackBufferWidth=engineBackBufferHeight=0;
}
bool loadImage2D(const std::string& requestedPath,std::string& cacheKey) {
    std::error_code ec;const fs::path absolute=fs::absolute(fs::path(wide(requestedPath)),ec).lexically_normal();if(ec)return false;
    cacheKey=utf8(absolute.wstring());if(engineImages2DState().contains(cacheKey))return true;
    const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);const bool uninitialize=SUCCEEDED(com);
    IWICImagingFactory* factory=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;IWICFormatConverter* converter=nullptr;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr))hr=factory->CreateDecoderFromFilename(absolute.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);
    if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
    UINT width=0,height=0;if(SUCCEEDED(hr))hr=frame->GetSize(&width,&height);
    if(SUCCEEDED(hr)&&(width==0||height==0||width>16384||height>16384||static_cast<std::uint64_t>(width)*height>67108864ULL))hr=E_INVALIDARG;
    if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&converter);
    if(SUCCEEDED(hr))hr=converter->Initialize(frame,GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0.0,WICBitmapPaletteTypeCustom);
    EngineImage2D image;image.width=width;image.height=height;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=static_cast<LONG>(width);info.bmiHeader.biHeight=-static_cast<LONG>(height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* pixels=nullptr;HDC screen=GetDC(nullptr);if(SUCCEEDED(hr)){image.bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!image.bitmap||!pixels)hr=E_OUTOFMEMORY;}if(screen)ReleaseDC(nullptr,screen);
    if(SUCCEEDED(hr)){image.dc=CreateCompatibleDC(nullptr);if(!image.dc)hr=E_OUTOFMEMORY;else image.oldBitmap=SelectObject(image.dc,image.bitmap);}
    if(SUCCEEDED(hr))hr=converter->CopyPixels(nullptr,width*4,width*height*4,static_cast<BYTE*>(pixels));
    if(converter)converter->Release();if(frame)frame->Release();if(decoder)decoder->Release();if(factory)factory->Release();
    if(uninitialize)CoUninitialize();
    if(FAILED(hr)){releaseImage2D(image);return false;}
    engineImages2DState().emplace(cacheKey,image);return true;
}
LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (message == ptsTrayCallbackMessage) {
        const UINT event = LOWORD(lp);
        if (event == WM_LBUTTONUP) ptsTrayLastEvent = "left_click";
        else if (event == WM_LBUTTONDBLCLK) ptsTrayLastEvent = "double_click";
        else if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) ptsTrayLastEvent = "right_click";
        else if (event == WM_MBUTTONUP) ptsTrayLastEvent = "middle_click";
        return 0;
    }
    if (message == WM_CLOSE) { windowRunning = false; DestroyWindow(hwnd); return 0; }
    if (message == WM_DESTROY) { if(ptsTrayIconAdded){Shell_NotifyIconW(NIM_DELETE,&ptsTrayIconData);ptsTrayIconAdded=false;}if(ptsTrayOwnedIcon){DestroyIcon(ptsTrayOwnedIcon);ptsTrayOwnedIcon=nullptr;}windowHandle = nullptr; windowRunning = false; resetBackBuffer();clearImages2D();PostQuitMessage(0); return 0; }
    if (message == WM_CHAR) { auto& input=engineTextInputState();if(wp!=8&&wp!=13&&wp!=10&&input.size()<4096)input.push_back(static_cast<wchar_t>(wp));return 0; }
    if (message == WM_MOUSEWHEEL) { engineMouseWheel+=GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA;return 0; }
    if (message == WM_PAINT) {
        PAINTSTRUCT ps{}; HDC output = BeginPaint(hwnd, &ps); RECT client{}; GetClientRect(hwnd, &client);const int width=client.right-client.left,height=client.bottom-client.top;
        if(width>0&&height>0&&(width!=engineBackBufferWidth||height!=engineBackBufferHeight||!engineBackBufferDC)){
            resetBackBuffer();engineBackBufferDC=CreateCompatibleDC(output);if(engineBackBufferDC){engineBackBufferBitmap=CreateCompatibleBitmap(output,width,height);if(engineBackBufferBitmap){engineBackBufferOldBitmap=SelectObject(engineBackBufferDC,engineBackBufferBitmap);engineBackBufferWidth=width;engineBackBufferHeight=height;}else{DeleteDC(engineBackBufferDC);engineBackBufferDC=nullptr;}}
        }
        HDC dc=engineBackBufferDC?engineBackBufferDC:output;
        HBRUSH bg = CreateSolidBrush(backgroundColor); FillRect(dc, &client, bg); DeleteObject(bg); SetBkMode(dc, TRANSPARENT);
        for (const auto& c : drawCommandsState()) {
            COLORREF color = colorOf(c.color);
            if (c.kind == 2) {
                auto text = wide(c.text); HFONT font = CreateFontW(-std::max(8,c.size),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,engineFontFace.c_str());
                HGDIOBJ old = SelectObject(dc,font); SetTextColor(dc,color); TextOutW(dc,c.x,c.y,text.c_str(),static_cast<int>(text.size())); SelectObject(dc,old); DeleteObject(font);
            } else if(c.kind==3) {
                const auto image=engineImages2DState().find(c.text);if(image!=engineImages2DState().end()&&c.w>0&&c.h>0){const int sx=c.sw>0?c.sx:0,sy=c.sh>0?c.sy:0,sw=c.sw>0?c.sw:static_cast<int>(image->second.width),sh=c.sh>0?c.sh:static_cast<int>(image->second.height);BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};AlphaBlend(dc,c.x,c.y,c.w,c.h,image->second.dc,sx,sy,sw,sh,blend);}
            } else if(c.kind==4) {
                HPEN pen=CreatePen(PS_SOLID,std::max(1,c.size),color);HGDIOBJ old=SelectObject(dc,pen);MoveToEx(dc,c.x,c.y,nullptr);LineTo(dc,c.x+c.w,c.y+c.h);SelectObject(dc,old);DeleteObject(pen);
            } else {
                HBRUSH brush = CreateSolidBrush(color); HPEN pen = CreatePen(PS_SOLID,1,color); HGDIOBJ ob=SelectObject(dc,brush),op=SelectObject(dc,pen);
                if(c.kind==0) Rectangle(dc,c.x,c.y,c.x+c.w,c.y+c.h); else Ellipse(dc,c.x,c.y,c.x+c.w,c.y+c.h);
                SelectObject(dc,ob); SelectObject(dc,op); DeleteObject(brush); DeleteObject(pen);
            }
        }
        if(dc!=output)BitBlt(output,0,0,width,height,dc,0,0,SRCCOPY);
        EndPaint(hwnd,&ps); return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
void pumpMessages() {
    MSG msg{}; while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { if(msg.message==WM_QUIT) windowRunning=false; TranslateMessage(&msg); DispatchMessageW(&msg); }
}
int ptsLoadGltf(const std::string& filename);
int ptsLoadHeightmap(const std::string& filename, double spacing, double heightScale);
#include "pts_d3d11.hpp"
void* callMethod(Value receiver, const std::string& name, void** args, int argc);
void* callFunction(const std::string& name, void* env, void** args, int argc) {
    auto it = functionsState().find(name);
    if (it != functionsState().end()) return it->second(env, args, argc);
    if (name == "print" || name == "yaz" || name == "write") {
        for (int i=0;i<argc;++i) writeStdout(stringOf(argument(args,i)));
        writeStdout("\n"); return nullptr;
    }
    if (name == "string") return box(stringOf(argument(args,0)));
    return nullptr;
}
void* callMethod(Value receiver, const std::string& name, void** args, int argc) {
    if (auto object = std::get_if<std::shared_ptr<Object>>(&receiver)) {
        auto classIt = classesState().find((*object)->type);
        if (classIt != classesState().end()) {
            auto method = classIt->second.methods.find(name);
            if (method != classIt->second.methods.end()) {
                auto* env = new Environment(); env->self = *object; env->parent = &globalEnvironmentState();
                return method->second(env,args,argc);
            }
        }
    }
    if (auto list = std::get_if<std::shared_ptr<ListValue>>(&receiver)) {
        auto& value = **list;
        if (name == "add" || name == "ekle") { value.values.push_back(argument(args,0)); value.active.push_back(true); return nullptr; }
        if (name == "remove" || name == "sil") {
            Value target = argument(args,0);
            for (std::size_t i=0;i<value.values.size();++i) if(value.active[i] && binary("==",value.values[i],target)==Value(true)) { value.active[i]=false; break; }
            return nullptr;
        }
        if (name == "size" || name == "boyut") { std::int64_t n=0; for(bool active:value.active) if(active) ++n; return box(n); }
    }
    return nullptr;
}
void* engineCall(const std::string& name, void** args, int argc) {
    if(name=="init_window") {
        WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.style=CS_HREDRAW|CS_VREDRAW; wc.lpfnWndProc=windowProc; wc.hInstance=GetModuleHandleW(nullptr); wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512)); wc.hbrBackground=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)); wc.lpszClassName=L"PRCYTERAS_DirectWindow"; RegisterClassExW(&wc);
        int w=argumentInt(args,0), h=argumentInt(args,1); std::wstring title=wide(argumentString(args,2)); RECT r{0,0,w,h}; AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
        windowHandle=CreateWindowExW(0,wc.lpszClassName,title.c_str(),WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,wc.hInstance,nullptr);
        if(!windowHandle) return box(false); windowRunning=true; ShowWindow(windowHandle,SW_SHOW); UpdateWindow(windowHandle); return box(true);
    }
    if(name=="is_running") { pumpMessages(); return box(windowRunning); }
    if(name=="clear_screen") { engineClockState().frameStart=std::chrono::steady_clock::now(); backgroundColor=colorOf(argumentString(args,0)); drawCommandsState().clear(); return nullptr; }
    if(name=="draw_rect") { drawCommandsState().push_back({0,argumentInt(args,0),argumentInt(args,1),argumentInt(args,2),argumentInt(args,3),16,{},argumentString(args,4)}); return nullptr; }
    if(name=="draw_circle") { int x=argumentInt(args,0),y=argumentInt(args,1),r=argumentInt(args,2); drawCommandsState().push_back({1,x-r,y-r,r*2,r*2,16,{},argumentString(args,3)}); return nullptr; }
    if(name=="draw_text") { drawCommandsState().push_back({2,argumentInt(args,1),argumentInt(args,2),0,0,argumentInt(args,3),argumentString(args,0),argumentString(args,4)}); return nullptr; }
    if((name=="draw_image"||name=="draw_sprite")&&argc==5){std::string key;if(!loadImage2D(argumentString(args,0),key))return box(false);drawCommandsState().push_back({3,argumentInt(args,1),argumentInt(args,2),argumentInt(args,3),argumentInt(args,4),16,std::move(key),{}});return box(true);}
    if(name=="draw_sprite_region"&&argc==9){std::string key;if(!loadImage2D(argumentString(args,0),key))return box(false);const auto image=engineImages2DState().find(key);const int sx=argumentInt(args,1),sy=argumentInt(args,2),sw=argumentInt(args,3),sh=argumentInt(args,4),dx=argumentInt(args,5),dy=argumentInt(args,6),dw=argumentInt(args,7),dh=argumentInt(args,8);if(image==engineImages2DState().end()||sx<0||sy<0||sw<=0||sh<=0||dw<=0||dh<=0||static_cast<std::uint64_t>(sx)+sw>image->second.width||static_cast<std::uint64_t>(sy)+sh>image->second.height)return box(false);DrawCommand command{3,dx,dy,dw,dh,16,key,{}};command.sx=sx;command.sy=sy;command.sw=sw;command.sh=sh;drawCommandsState().push_back(std::move(command));return box(true);}
    if(name=="draw_sprite_frame"&&argc==8){std::string key;if(!loadImage2D(argumentString(args,0),key))return box(false);const auto image=engineImages2DState().find(key);const std::int64_t frame=argumentInt(args,1);const int fw=argumentInt(args,2),fh=argumentInt(args,3),dx=argumentInt(args,4),dy=argumentInt(args,5),dw=argumentInt(args,6),dh=argumentInt(args,7);if(image==engineImages2DState().end()||frame<0||fw<=0||fh<=0||dw<=0||dh<=0)return box(false);const int columns=static_cast<int>(image->second.width)/fw,rows=static_cast<int>(image->second.height)/fh;if(columns<=0||rows<=0||static_cast<std::uint64_t>(frame)>=static_cast<std::uint64_t>(columns)*rows)return box(false);DrawCommand command{3,dx,dy,dw,dh,16,key,{}};command.sx=static_cast<int>(frame%columns)*fw;command.sy=static_cast<int>(frame/columns)*fh;command.sw=fw;command.sh=fh;drawCommandsState().push_back(std::move(command));return box(true);}
    if(name=="unload_image"&&argc==1){const std::string requested=argumentString(args,0);std::error_code ec;const std::string key=utf8(fs::absolute(fs::path(wide(requested)),ec).lexically_normal().wstring());auto it=engineImages2DState().find(key);if(it!=engineImages2DState().end()){releaseImage2D(it->second);engineImages2DState().erase(it);}return nullptr;}
    if(name=="clear_image_cache"){clearImages2D();return nullptr;}
    if(name=="draw_line"&&(argc==5||argc==6)){drawCommandsState().push_back({4,argumentInt(args,0),argumentInt(args,1),argumentInt(args,2)-argumentInt(args,0),argumentInt(args,3)-argumentInt(args,1),std::max(1,argc==6?argumentInt(args,5):1),{},argumentString(args,4)});return nullptr;}
    if(name=="render_frame") {
        pumpMessages(); if(windowHandle){InvalidateRect(windowHandle,nullptr,FALSE);UpdateWindow(windowHandle);}
        auto& clock=engineClockState();if(engineTargetFps>0){const auto deadline=clock.frameStart+std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(1.0/engineTargetFps));std::this_thread::sleep_until(deadline);}
        const auto now=std::chrono::steady_clock::now();engineDeltaSeconds=std::clamp(std::chrono::duration<double>(now-clock.lastFrame).count(),0.000001,1.0);clock.lastFrame=now;clock.frameStart=now;temporaryValuesState().clear();return nullptr;
    }
    if(name=="set_target_fps"&&argc==1){engineTargetFps=std::clamp(argumentInt(args,0),0,1000);return nullptr;}
    if(name=="set_scan_count"&&argc==1){if(windowHandle){const std::wstring title=L"PRCYTERAS Guvenlik Merkezi - Taranan dosya: "+std::to_wstring(argumentInt(args,0));SetWindowTextW(windowHandle,title.c_str());}return nullptr;}
    if(name=="delta_time")return box(engineDeltaSeconds);
    if(name=="set_font_face"&&argc==1){const std::wstring face=wide(argumentString(args,0));if(face.empty()||face.size()>64)return box(false);engineFontFace=face;return box(true);}
    if(name=="font_face"&&argc==0)return box(utf8(engineFontFace));
    if(name=="time_seconds")return box(std::chrono::duration<double>(std::chrono::steady_clock::now()-engineClockState().startTime).count());
    if(name=="window_width"||name=="window_height"){RECT r{};if(!windowHandle||!GetClientRect(windowHandle,&r))return box(std::int64_t{0});return box(static_cast<std::int64_t>(name=="window_width"?r.right-r.left:r.bottom-r.top));}
    if(name=="key_down"&&argc==1){if(windowHandle&&GetForegroundWindow()!=windowHandle)return box(false);const int key=ptsVirtualKey(argumentString(args,0));return box(key&&(GetAsyncKeyState(key)&0x8000));}
    if(name=="mouse_x"||name=="mouse_y"){if(!windowHandle)return box(std::int64_t{0});POINT p{};if(!GetCursorPos(&p)||!ScreenToClient(windowHandle,&p))return box(std::int64_t{0});return box(static_cast<std::int64_t>(name=="mouse_x"?p.x:p.y));}
    if(name=="mouse_button_down"&&argc==1){if(windowHandle&&GetForegroundWindow()!=windowHandle)return box(false);std::string button=argumentString(args,0);std::transform(button.begin(),button.end(),button.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});const int key=button=="LEFT"||button=="MOUSE_LEFT"?VK_LBUTTON:button=="RIGHT"||button=="MOUSE_RIGHT"?VK_RBUTTON:button=="MIDDLE"||button=="MOUSE_MIDDLE"?VK_MBUTTON:0;return box(key&&(GetAsyncKeyState(key)&0x8000));}
    if(name=="mouse_wheel_delta"){const int result=engineMouseWheel;engineMouseWheel=0;return box(static_cast<std::int64_t>(result));}
    if(name=="get_text_input"){auto& input=engineTextInputState();const std::string result=utf8(input);input.clear();return box(result);}
    if(name=="request_close"){if(windowHandle)PostMessageW(windowHandle,WM_CLOSE,0,0);return nullptr;}
    if(name=="get_input") { if(windowHandle&&GetForegroundWindow()!=windowHandle)return box(std::string());if(GetAsyncKeyState(VK_ESCAPE)&0x8000){if(windowHandle)PostMessageW(windowHandle,WM_CLOSE,0,0);return box(std::string("CIKIS"));} if((GetAsyncKeyState(VK_LEFT)&0x8000)||(GetAsyncKeyState('A')&0x8000))return box(std::string("SOL"));if((GetAsyncKeyState(VK_RIGHT)&0x8000)||(GetAsyncKeyState('D')&0x8000))return box(std::string("SAG"));return box(std::string()); }
    if(name=="play_sound") { PlaySoundA(argumentString(args,0).c_str(),nullptr,SND_FILENAME|SND_ASYNC|SND_NODEFAULT); return nullptr; }
    if(name=="sleep"||name=="wait"||name=="bekle") { int ms=argumentInt(args,0);if(ms>0)Sleep(static_cast<DWORD>(ms));temporaryValuesState().clear();return nullptr; }
    if(name=="yaz"||name=="write"||name=="print") { for(int i=0;i<argc;++i)writeStdout(stringOf(argument(args,i)));writeStdout("\n");return nullptr; }
    if(name=="cizgi_cek") { writeStdout("---------------------------------------\n");return nullptr; }
    if(name=="baslik_ayarla"||name=="set_title") { auto title=wide(argumentString(args,0));SetConsoleTitleW(title.c_str());if(windowHandle)SetWindowTextW(windowHandle,title.c_str());return nullptr; }
    if(name=="renk_degistir"||name=="set_color") { WORD a=FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE;auto c=argumentString(args,0);if(c=="RED"||c=="KIRMIZI")a=FOREGROUND_RED|FOREGROUND_INTENSITY;else if(c=="GREEN"||c=="YESIL"||c=="YEŞİL")a=FOREGROUND_GREEN|FOREGROUND_INTENSITY;SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),a);return nullptr; }
    return nullptr;
}

// ben yaptım opencode - 2026-11-14 - JSON, Base64, Regex ve DateTime yerel arka uclari
void appendJsonString(std::string& out, const std::string& text) {
    out.push_back('"');
    static const char hex[] = "0123456789abcdef";
    for (const unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(hex[(c >> 4) & 0xF]);
                    out.push_back(hex[c & 0xF]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
}

void appendJsonIndent(std::string& out, int level, int width) {
    if (width < 0) width = 0;
    out.append(static_cast<std::size_t>(level) * static_cast<std::size_t>(width), ' ');
}

bool jsonSerialize(const Value& value, std::string& out, bool pretty, int level, int indent) {
    if (const auto* item = std::get_if<std::string>(&value)) { appendJsonString(out, *item); return true; }
    if (const auto* item = std::get_if<std::int64_t>(&value)) { out += std::to_string(*item); return true; }
    if (const auto* item = std::get_if<std::uint64_t>(&value)) { out += std::to_string(*item); return true; }
    if (const auto* item = std::get_if<bool>(&value)) { out += *item ? "true" : "false"; return true; }
    if (const auto* item = std::get_if<double>(&value)) {
        if (!std::isfinite(*item)) { out += "null"; return true; }
        char buffer[64]{};
        std::snprintf(buffer, sizeof(buffer), "%.17g", *item);
        out += buffer;
        return true;
    }
    if (std::holds_alternative<std::monostate>(value)) { out += "null"; return true; }
    if (const auto* object = std::get_if<std::shared_ptr<Object>>(&value)) {
        if (!*object) { out += "null"; return true; }
        std::vector<std::pair<std::string, Value>> fields;
        fields.reserve((*object)->dynamicFields.size());
        for (const auto& field : (*object)->dynamicFields) fields.emplace_back(field.first, field.second);
        std::sort(fields.begin(), fields.end(), [](const auto& left, const auto& right) { return left.first < right.first; });
        out.push_back('{');
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (i) out.push_back(',');
            if (pretty) { out.push_back('\n'); appendJsonIndent(out, level + 1, indent); }
            appendJsonString(out, fields[i].first);
            out += pretty ? ": " : ":";
            jsonSerialize(fields[i].second, out, pretty, level + 1, indent);
        }
        if (pretty && !fields.empty()) { out.push_back('\n'); appendJsonIndent(out, level, indent); }
        out.push_back('}');
        return true;
    }
    if (const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value)) {
        out.push_back('[');
        bool first = true;
        for (std::size_t i = 0; i < (*list)->values.size(); ++i) {
            if (i < (*list)->active.size() && !(*list)->active[i]) continue;
            if (!first) out.push_back(',');
            first = false;
            if (pretty) { out.push_back('\n'); appendJsonIndent(out, level + 1, indent); }
            jsonSerialize((*list)->values[i], out, pretty, level + 1, indent);
        }
        if (pretty && !first) { out.push_back('\n'); appendJsonIndent(out, level, indent); }
        out.push_back(']');
        return true;
    }
    out += "null";
    return true;
}

struct JsonReader {
    std::string text;
    std::size_t pos = 0;
    int depth = 0;
    std::string error;

    explicit JsonReader(std::string source) : text(std::move(source)) {}

    bool fail(const std::string& message) {
        if (error.empty()) error = message + " (konum " + std::to_string(pos) + ")";
        return false;
    }
    void skip() {
        while (pos < text.size()) {
            const char c = text[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos;
            else break;
        }
    }
    bool consume(char wanted) {
        skip();
        if (pos >= text.size() || text[pos] != wanted) return false;
        ++pos;
        return true;
    }
    static void appendUtf8(std::string& out, unsigned int code) {
        if (code <= 0x7F) out.push_back(static_cast<char>(code));
        else if (code <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else if (code <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }
    bool parseHex4(unsigned int& value) {
        if (pos + 4 > text.size()) return fail("\\u kacisi eksik");
        value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text[pos++];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<unsigned int>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned int>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned int>(c - 'A' + 10);
            else return fail("gecersiz \\u kacisi");
        }
        return true;
    }
    bool parseString(std::string& out) {
        skip();
        if (pos >= text.size() || text[pos] != '"') return fail("metin bekleniyordu");
        ++pos;
        while (pos < text.size()) {
            const unsigned char c = static_cast<unsigned char>(text[pos++]);
            if (c == '"') return true;
            if (c == '\\') {
                if (pos >= text.size()) return fail("kacis eksik");
                const char escape = text[pos++];
                switch (escape) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        unsigned int code = 0;
                        if (!parseHex4(code)) return false;
                        if (code >= 0xD800 && code <= 0xDBFF) {
                            if (pos + 6 > text.size() || text[pos] != '\\' || text[pos + 1] != 'u')
                                return fail("surrogate cifti eksik");
                            pos += 2;
                            unsigned int low = 0;
                            if (!parseHex4(low)) return false;
                            if (low < 0xDC00 || low > 0xDFFF) return fail("gecersiz surrogate cifti");
                            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                        } else if (code >= 0xDC00 && code <= 0xDFFF) {
                            return fail("gecersiz surrogate");
                        }
                        appendUtf8(out, code);
                        break;
                    }
                    default: return fail("bilinmeyen kacis");
                }
            } else if (c < 0x20) {
                return fail("metin icinde kontrol karakteri");
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
        return fail("metin kapanmadi");
    }
    bool parseNumber(Value& out) {
        skip();
        const std::size_t start = pos;
        if (pos < text.size() && text[pos] == '-') ++pos;
        if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))) return fail("sayi bekleniyordu");
        if (text[pos] == '0') ++pos;
        else while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
        if (pos < text.size() && text[pos] == '.') {
            ++pos;
            if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))) return fail("ondalik kisim eksik");
            while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
        }
        if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
            ++pos;
            if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) ++pos;
            if (pos >= text.size() || !std::isdigit(static_cast<unsigned char>(text[pos]))) return fail("us eksik");
            while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
        }
        const std::string token = text.substr(start, pos - start);
        if (token.find_first_of(".eE") == std::string::npos) {
            try {
                std::size_t used = 0;
                const long long number = std::stoll(token, &used, 10);
                if (used == token.size()) { out = static_cast<std::int64_t>(number); return true; }
            } catch (...) {}
        }
        try {
            std::size_t used = 0;
            const double number = std::stod(token, &used);
            if (used != token.size() || !std::isfinite(number)) return fail("gecersiz sayi");
            out = number;
            return true;
        } catch (...) {
            return fail("gecersiz sayi");
        }
    }
    bool parseValue(Value& out) {
        skip();
        if (pos >= text.size()) return fail("deger bekleniyordu");
        if (++depth > 512) return fail("ic ice yapi cok derin");
        bool result = false;
        const char c = text[pos];
        if (c == '{') result = parseObject(out);
        else if (c == '[') result = parseArray(out);
        else if (c == '"') { std::string textValue; result = parseString(textValue); if (result) out = std::move(textValue); }
        else if (c == 't') { if (text.compare(pos, 4, "true") == 0) { pos += 4; out = true; result = true; } else result = fail("gecersiz deger"); }
        else if (c == 'f') { if (text.compare(pos, 5, "false") == 0) { pos += 5; out = false; result = true; } else result = fail("gecersiz deger"); }
        else if (c == 'n') { if (text.compare(pos, 4, "null") == 0) { pos += 4; out = Value{}; result = true; } else result = fail("gecersiz deger"); }
        else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) result = parseNumber(out);
        else result = fail("gecersiz deger");
        --depth;
        return result;
    }
    bool parseArray(Value& out) {
        if (!consume('[')) return fail("'[' bekleniyordu");
        auto list = std::make_shared<ListValue>();
        skip();
        if (consume(']')) { out = list; return true; }
        while (true) {
            Value item;
            if (!parseValue(item)) return false;
            list->values.push_back(std::move(item));
            list->active.push_back(true);
            skip();
            if (consume(']')) break;
            if (!consume(',')) return fail("',' veya ']' bekleniyordu");
        }
        out = list;
        return true;
    }
    bool parseObject(Value& out) {
        if (!consume('{')) return fail("'{' bekleniyordu");
        auto object = std::make_shared<Object>();
        object->type = "JsonObject";
        skip();
        if (consume('}')) { out = object; return true; }
        while (true) {
            std::string key;
            if (!parseString(key)) return false;
            if (!consume(':')) return fail("':' bekleniyordu");
            Value item;
            if (!parseValue(item)) return false;
            object->dynamicFields[key] = std::move(item);
            skip();
            if (consume('}')) break;
            if (!consume(',')) return fail("',' veya '}' bekleniyordu");
        }
        out = object;
        return true;
    }
    bool parseDocument(Value& out) {
        if (!parseValue(out)) return false;
        skip();
        if (pos != text.size()) return fail("belge sonunda fazladan veri");
        return true;
    }
};

const Value* jsonField(const Value& container, const std::string& key) {
    const auto* object = std::get_if<std::shared_ptr<Object>>(&container);
    if (!object || !*object) return nullptr;
    const auto found = (*object)->dynamicFields.find(key);
    return found == (*object)->dynamicFields.end() ? nullptr : &found->second;
}

bool jsonIndex(const Value& value, std::size_t& index) {
    if (const auto* item = std::get_if<std::int64_t>(&value)) {
        if (*item < 0) return false;
        index = static_cast<std::size_t>(*item);
        return true;
    }
    if (const auto* item = std::get_if<std::uint64_t>(&value)) {
        if (*item > std::numeric_limits<std::size_t>::max()) return false;
        index = static_cast<std::size_t>(*item);
        return true;
    }
    if (const auto* item = std::get_if<double>(&value)) {
        if (!std::isfinite(*item) || *item < 0 || std::trunc(*item) != *item) return false;
        index = static_cast<std::size_t>(*item);
        return true;
    }
    return false;
}

bool jsonListElement(const ListValue& list, std::size_t wanted, std::size_t& physical) {
    std::size_t visible = 0;
    for (std::size_t i = 0; i < list.values.size(); ++i) {
        if (i < list.active.size() && !list.active[i]) continue;
        if (visible++ == wanted) { physical = i; return true; }
    }
    return false;
}

std::size_t jsonCodePoints(const std::string& text) {
    std::size_t count = 0;
    for (const unsigned char c : text) if ((c & 0xC0) != 0x80) ++count;
    return count;
}

void* jsonCall(const std::string& op, void** args, int argc) {
    if ((op == "parse" || op == "coz") && argc == 1) {
        JsonReader reader(argumentString(args, 0));
        Value result;
        if (!reader.parseDocument(result)) {
            writeStderr("PRCYTERAS Json.parse hatasi: ");
            writeStderr(reader.error);
            writeStderr("\n");
            return nullptr;
        }
        return box(std::move(result));
    }
    if ((op == "stringify" || op == "yaz") && (argc == 1 || argc == 2)) {
        const bool pretty = argc == 2 && truthy(argument(args, 1));
        std::string result;
        jsonSerialize(argument(args, 0), result, pretty, 0, 2);
        return box(std::move(result));
    }
    if ((op == "stringify_pretty" || op == "pretty") && (argc == 1 || argc == 2)) {
        const int indent = argc == 2 ? std::clamp(argumentInt(args, 1), 0, 16) : 2;
        std::string result;
        jsonSerialize(argument(args, 0), result, true, 0, indent);
        return box(std::move(result));
    }
    if ((op == "get" || op == "al") && argc == 2) {
        const Value* found = jsonField(argument(args, 0), argumentString(args, 1));
        return found ? box(*found) : box(Value{});
    }
    if ((op == "set" || op == "ata") && argc == 3) {
        const Value container = argument(args, 0);
        const auto* object = std::get_if<std::shared_ptr<Object>>(&container);
        if (!object || !*object) return nullptr;
        (*object)->dynamicFields[argumentString(args, 1)] = argument(args, 2);
        return box(container);
    }
    if ((op == "has" || op == "contains" || op == "var_mi") && argc == 2)
        return box(jsonField(argument(args, 0), argumentString(args, 1)) != nullptr);
    if ((op == "remove" || op == "sil") && argc == 2) {
        const Value container = argument(args, 0);
        const auto* object = std::get_if<std::shared_ptr<Object>>(&container);
        if (!object || !*object) return box(false);
        return box((*object)->dynamicFields.erase(argumentString(args, 1)) > 0);
    }
    if ((op == "remove_at" || op == "indis_sil") && argc == 2) {
        const Value container = argument(args, 0);
        const auto* list = std::get_if<std::shared_ptr<ListValue>>(&container);
        std::size_t wanted = 0, physical = 0;
        if (!list || !jsonIndex(argument(args, 1), wanted) || !jsonListElement(**list, wanted, physical)) return box(false);
        (*list)->active[physical] = false;
        return box(true);
    }
    if ((op == "keys" || op == "anahtarlar") && argc == 1) {
        auto list = std::make_shared<ListValue>();
        const Value container = argument(args, 0);
        if (const auto* object = std::get_if<std::shared_ptr<Object>>(&container)) {
            std::vector<std::string> keys;
            keys.reserve((*object)->dynamicFields.size());
            for (const auto& field : (*object)->dynamicFields) keys.push_back(field.first);
            std::sort(keys.begin(), keys.end());
            for (auto& key : keys) { list->values.emplace_back(std::move(key)); list->active.push_back(true); }
        }
        return box(std::move(list));
    }
    if ((op == "values" || op == "degerler") && argc == 1) {
        auto list = std::make_shared<ListValue>();
        const Value container = argument(args, 0);
        if (const auto* object = std::get_if<std::shared_ptr<Object>>(&container)) {
            std::vector<std::pair<std::string, Value>> fields;
            for (const auto& field : (*object)->dynamicFields) fields.emplace_back(field.first, field.second);
            std::sort(fields.begin(), fields.end(), [](const auto& left, const auto& right) { return left.first < right.first; });
            for (auto& field : fields) { list->values.push_back(std::move(field.second)); list->active.push_back(true); }
        }
        return box(std::move(list));
    }
    if ((op == "at" || op == "eleman") && argc == 2) {
        const Value container = argument(args, 0);
        const auto* list = std::get_if<std::shared_ptr<ListValue>>(&container);
        std::size_t wanted = 0, physical = 0;
        if (!list || !jsonIndex(argument(args, 1), wanted) || !jsonListElement(**list, wanted, physical)) return box(Value{});
        return box((*list)->values[physical]);
    }
    if ((op == "push" || op == "ekle") && argc == 2) {
        const Value container = argument(args, 0);
        const auto* list = std::get_if<std::shared_ptr<ListValue>>(&container);
        if (!list || !*list) return nullptr;
        (*list)->values.push_back(argument(args, 1));
        (*list)->active.push_back(true);
        return box(container);
    }
    if ((op == "length" || op == "size" || op == "uzunluk") && argc == 1) {
        const Value container = argument(args, 0);
        if (const auto* text = std::get_if<std::string>(&container)) return box(static_cast<std::int64_t>(jsonCodePoints(*text)));
        if (const auto* object = std::get_if<std::shared_ptr<Object>>(&container)) return box(static_cast<std::int64_t>((*object)->dynamicFields.size()));
        if (const auto* list = std::get_if<std::shared_ptr<ListValue>>(&container)) {
            std::int64_t count = 0;
            for (const bool active : (*list)->active) if (active) ++count;
            return box(count);
        }
        return box(std::int64_t{0});
    }
    if ((op == "type" || op == "tur") && argc == 1) {
        const Value container = argument(args, 0);
        const char* name = std::holds_alternative<std::monostate>(container) ? "null" :
            std::holds_alternative<bool>(container) ? "boolean" :
            std::holds_alternative<std::string>(container) ? "string" :
            std::holds_alternative<std::shared_ptr<Object>>(container) ? "object" :
            std::holds_alternative<std::shared_ptr<ListValue>>(container) ? "array" : "number";
        return box(std::string(name));
    }
    if ((op == "is_null" || op == "bos") && argc == 1) return box(std::holds_alternative<std::monostate>(argument(args, 0)));
    if ((op == "is_object" || op == "nesne_mi") && argc == 1) return box(std::holds_alternative<std::shared_ptr<Object>>(argument(args, 0)));
    if ((op == "is_array" || op == "dizi_mi") && argc == 1) return box(std::holds_alternative<std::shared_ptr<ListValue>>(argument(args, 0)));
    if ((op == "is_string" || op == "metin_mi") && argc == 1) return box(std::holds_alternative<std::string>(argument(args, 0)));
    if ((op == "is_number" || op == "sayi_mi") && argc == 1) {
        const Value value = argument(args, 0);
        return box(std::holds_alternative<std::int64_t>(value) || std::holds_alternative<std::uint64_t>(value) || std::holds_alternative<double>(value));
    }
    if ((op == "is_bool" || op == "mantik_mi") && argc == 1) return box(std::holds_alternative<bool>(argument(args, 0)));
    if ((op == "as_string" || op == "metin") && argc == 1) {
        const Value value = argument(args, 0);
        if (std::holds_alternative<std::shared_ptr<Object>>(value) || std::holds_alternative<std::shared_ptr<ListValue>>(value)) return box(std::string());
        return box(stringOf(value));
    }
    if ((op == "as_int" || op == "tamsayi") && argc == 1) return box(static_cast<std::int64_t>(numberOf(argument(args, 0))));
    if ((op == "as_float" || op == "ondalik") && argc == 1) return box(numberOf(argument(args, 0)));
    if ((op == "as_bool" || op == "mantik") && argc == 1) return box(truthy(argument(args, 0)));
    if ((op == "new_object" || op == "yeni_nesne") && argc == 0) {
        auto object = std::make_shared<Object>();
        object->type = "JsonObject";
        return box(Value{object});
    }
    if ((op == "new_array" || op == "yeni_dizi") && argc == 0) return box(Value{std::make_shared<ListValue>()});
    if (op == "merge" && argc == 2) {
        const Value target = argument(args, 0);
        const auto* targetObject = std::get_if<std::shared_ptr<Object>>(&target);
        const Value source = argument(args, 1);
        const auto* sourceObject = std::get_if<std::shared_ptr<Object>>(&source);
        if (!targetObject || !*targetObject || !sourceObject || !*sourceObject) return nullptr;
        for (const auto& field : (*sourceObject)->dynamicFields) (*targetObject)->dynamicFields[field.first] = field.second;
        return box(target);
    }
    writeStderr("PRCYTERAS: Json.");
    writeStderr(op);
    writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");
    return nullptr;
}

const char* base64Alphabet(bool urlSafe) {
    return urlSafe ? "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
                   : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}

std::string base64Encode(const std::string& input, bool urlSafe) {
    const char* alphabet = base64Alphabet(urlSafe);
    std::string result;
    result.reserve(((input.size() + 2) / 3) * 4);
    for (std::size_t i = 0; i < input.size(); i += 3) {
        const unsigned int b0 = static_cast<unsigned char>(input[i]);
        const unsigned int b1 = i + 1 < input.size() ? static_cast<unsigned char>(input[i + 1]) : 0;
        const unsigned int b2 = i + 2 < input.size() ? static_cast<unsigned char>(input[i + 2]) : 0;
        const unsigned int value = (b0 << 16) | (b1 << 8) | b2;
        result.push_back(alphabet[(value >> 18) & 63]);
        result.push_back(alphabet[(value >> 12) & 63]);
        result.push_back(i + 1 < input.size() ? alphabet[(value >> 6) & 63] : '=');
        result.push_back(i + 2 < input.size() ? alphabet[value & 63] : '=');
    }
    return result;
}

bool base64Decode(const std::string& input, bool urlSafe, std::string& output) {
    output.clear();
    output.reserve((input.size() / 4) * 3);
    unsigned int buffer = 0;
    int bits = 0;
    bool padding = false;
    for (const unsigned char c : input) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        if (c == '=') { padding = true; continue; }
        if (padding) return false;
        int value = -1;
        if (c >= 'A' && c <= 'Z') value = c - 'A';
        else if (c >= 'a' && c <= 'z') value = c - 'a' + 26;
        else if (c >= '0' && c <= '9') value = c - '0' + 52;
        else if (c == '+' || (urlSafe && c == '-')) value = 62;
        else if (c == '/' || (urlSafe && c == '_')) value = 63;
        if (value < 0) return false;
        buffer = (buffer << 6) | static_cast<unsigned int>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<char>((buffer >> bits) & 0xFF));
        }
    }
    return bits < 6;
}

std::string hexEncode(const std::string& input) {
    static const char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(input.size() * 2);
    for (const unsigned char c : input) {
        result.push_back(digits[(c >> 4) & 0xF]);
        result.push_back(digits[c & 0xF]);
    }
    return result;
}

bool hexDecode(const std::string& input, std::string& output) {
    output.clear();
    if (input.size() % 2 != 0) return false;
    output.reserve(input.size() / 2);
    for (std::size_t i = 0; i < input.size(); i += 2) {
        const auto digit = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const int high = digit(input[i]), low = digit(input[i + 1]);
        if (high < 0 || low < 0) return false;
        output.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

bool valueBytes(const Value& value, std::string& output) {
    output.clear();
    if (const auto* text = std::get_if<std::string>(&value)) { output = *text; return true; }
    const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value);
    if (!list || !*list) return false;
    for (std::size_t i = 0; i < (*list)->values.size(); ++i) {
        if (i < (*list)->active.size() && !(*list)->active[i]) continue;
        const double number = numberOf((*list)->values[i]);
        if (!std::isfinite(number) || number < 0 || number > 255 || std::trunc(number) != number) return false;
        output.push_back(static_cast<char>(static_cast<unsigned char>(number)));
    }
    return true;
}

Value bytesList(const std::string& input) {
    auto list = std::make_shared<ListValue>();
    list->values.reserve(input.size());
    for (const unsigned char c : input) list->values.emplace_back(static_cast<std::int64_t>(c));
    list->active.assign(list->values.size(), true);
    return Value{list};
}

void* base64Call(const std::string& op, void** args, int argc) {
    if ((op == "encode" || op == "kodla") && (argc == 1 || argc == 2)) {
        std::string bytes;
        if (!valueBytes(argument(args, 0), bytes)) { writeStderr("PRCYTERAS Base64.encode: metin veya bayt listesi bekleniyordu.\n"); return nullptr; }
        return box(base64Encode(bytes, argc == 2 && truthy(argument(args, 1))));
    }
    if ((op == "encode_url" || op == "url_kodla") && argc == 1) {
        std::string bytes;
        if (!valueBytes(argument(args, 0), bytes)) return nullptr;
        return box(base64Encode(bytes, true));
    }
    if ((op == "decode" || op == "coz") && (argc == 1 || argc == 2)) {
        std::string output;
        if (!base64Decode(argumentString(args, 0), argc == 2 && truthy(argument(args, 1)), output)) {
            writeStderr("PRCYTERAS Base64.decode: gecersiz base64 verisi.\n");
            return nullptr;
        }
        return box(std::move(output));
    }
    if ((op == "decode_url" || op == "url_coz") && argc == 1) {
        std::string output;
        if (!base64Decode(argumentString(args, 0), true, output)) return nullptr;
        return box(std::move(output));
    }
    if ((op == "encode_bytes" || op == "bayt_kodla") && (argc == 1 || argc == 2)) {
        std::string bytes;
        if (!valueBytes(argument(args, 0), bytes)) return nullptr;
        return box(base64Encode(bytes, argc == 2 && truthy(argument(args, 1))));
    }
    if ((op == "decode_bytes" || op == "bayt_coz") && (argc == 1 || argc == 2)) {
        std::string output;
        if (!base64Decode(argumentString(args, 0), argc == 2 && truthy(argument(args, 1)), output)) return nullptr;
        return box(bytesList(output));
    }
    if ((op == "valid" || op == "gecerli_mi") && argc == 1) {
        std::string output;
        return box(base64Decode(argumentString(args, 0), false, output));
    }
    if ((op == "hex_encode" || op == "hex_kodla") && argc == 1) {
        std::string bytes;
        if (!valueBytes(argument(args, 0), bytes)) return nullptr;
        return box(hexEncode(bytes));
    }
    if ((op == "hex_decode" || op == "hex_coz") && argc == 1) {
        std::string output;
        if (!hexDecode(argumentString(args, 0), output)) { writeStderr("PRCYTERAS Base64.hex_decode: gecersiz hex verisi.\n"); return nullptr; }
        return box(std::move(output));
    }
    if ((op == "hex_decode_bytes" || op == "hex_bayt_coz") && argc == 1) {
        std::string output;
        if (!hexDecode(argumentString(args, 0), output)) return nullptr;
        return box(bytesList(output));
    }
    writeStderr("PRCYTERAS: Base64.");
    writeStderr(op);
    writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");
    return nullptr;
}

bool makeRegex(const std::string& pattern, const std::string& flags, std::regex& expression) {
    auto options = std::regex_constants::ECMAScript;
    if (flags.find('i') != std::string::npos) options |= std::regex_constants::icase;
    if (flags.find('m') != std::string::npos) options |= std::regex_constants::multiline;
    try {
        expression = std::regex(pattern, options);
        return true;
    } catch (const std::regex_error& error) {
        writeStderr("PRCYTERAS Regex: gecersiz desen: ");
        writeStderr(error.what());
        writeStderr("\n");
        return false;
    }
}

void* regexCall(const std::string& op, void** args, int argc) {
    if ((op == "escape" || op == "kacir") && argc == 1) {
        const std::string pattern = argumentString(args, 0);
        std::string result;
        result.reserve(pattern.size() * 2);
        for (const char c : pattern) {
            if (std::string_view("\\^$.|?*+()[]{}").find(c) != std::string_view::npos) result.push_back('\\');
            result.push_back(c);
        }
        return box(std::move(result));
    }
    if (argc < 2) {
        writeStderr("PRCYTERAS: Regex.");
        writeStderr(op);
        writeStderr(" icin desen ve metin gerekli.\n");
        return nullptr;
    }
    const std::string flags = argc >= 3 ? argumentString(args, 2) : std::string();
    std::regex expression;
    if (!makeRegex(argumentString(args, 0), flags, expression)) return nullptr;
    const std::string text = argumentString(args, 1);
    if (op == "is_match" || op == "test" || op == "eslesir_mi") return box(std::regex_search(text, expression));
    if (op == "find" || op == "search" || op == "ara") {
        std::smatch match;
        if (!std::regex_search(text, match, expression)) return box(std::string());
        return box(match.str());
    }
    if (op == "find_index" || op == "index" || op == "konum") {
        std::smatch match;
        if (!std::regex_search(text, match, expression)) return box(std::int64_t{-1});
        return box(static_cast<std::int64_t>(match.position()));
    }
    if (op == "find_all" || op == "tumunu_bul") {
        auto list = std::make_shared<ListValue>();
        for (std::sregex_iterator it(text.begin(), text.end(), expression), end; it != end; ++it) {
            list->values.emplace_back(it->str());
            list->active.push_back(true);
        }
        return box(Value{list});
    }
    if (op == "groups" || op == "gruplar") {
        auto list = std::make_shared<ListValue>();
        std::smatch match;
        if (std::regex_search(text, match, expression)) {
            for (std::size_t i = 0; i < match.size(); ++i) {
                list->values.emplace_back(match[i].matched ? match[i].str() : std::string());
                list->active.push_back(true);
            }
        }
        return box(Value{list});
    }
    if ((op == "replace" || op == "degistir") && argc >= 3) {
        const std::string replacement = argumentString(args, 2);
        const bool firstOnly = argc >= 4 && truthy(argument(args, 3));
        auto flagsOut = std::regex_constants::match_default;
        if (firstOnly) flagsOut |= std::regex_constants::format_first_only;
        return box(std::regex_replace(text, expression, replacement, flagsOut));
    }
    if ((op == "replace_first" || op == "ilk_degistir") && argc >= 3)
        return box(std::regex_replace(text, expression, argumentString(args, 2), std::regex_constants::format_first_only));
    if (op == "split" || op == "bol") {
        auto list = std::make_shared<ListValue>();
        for (std::sregex_token_iterator it(text.begin(), text.end(), expression, -1), end; it != end; ++it) {
            list->values.emplace_back(it->str());
            list->active.push_back(true);
        }
        return box(Value{list});
    }
    if (op == "count" || op == "say") {
        std::int64_t count = 0;
        for (std::sregex_iterator it(text.begin(), text.end(), expression), end; it != end; ++it) ++count;
        return box(count);
    }
    writeStderr("PRCYTERAS: Regex.");
    writeStderr(op);
    writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");
    return nullptr;
}

std::tm localTimeFromUnix(std::int64_t seconds) {
    const std::time_t time = static_cast<std::time_t>(seconds);
    std::tm result{};
    const std::tm* value = std::localtime(&time);
    if (value) result = *value;
    return result;
}

std::tm utcTimeFromUnix(std::int64_t seconds) {
    const std::time_t time = static_cast<std::time_t>(seconds);
    std::tm result{};
    const std::tm* value = std::gmtime(&time);
    if (value) result = *value;
    return result;
}

std::string formatTime(std::tm time, const std::string& format) {
    std::array<char, 1024> buffer{};
    const std::size_t length = std::strftime(buffer.data(), buffer.size(), format.c_str(), &time);
    return length ? std::string(buffer.data(), length) : std::string();
}

std::int64_t unixFromUtcParts(int year, int month, int day, int hour, int minute, int second) {
    std::tm time{};
    time.tm_year = year - 1900;
    time.tm_mon = month - 1;
    time.tm_mday = day;
    time.tm_hour = hour;
    time.tm_min = minute;
    time.tm_sec = second;
    time.tm_isdst = 0;
    return static_cast<std::int64_t>(_mkgmtime64(&time));
}

std::int64_t unixFromLocalParts(int year, int month, int day, int hour, int minute, int second) {
    std::tm time{};
    time.tm_year = year - 1900;
    time.tm_mon = month - 1;
    time.tm_mday = day;
    time.tm_hour = hour;
    time.tm_min = minute;
    time.tm_sec = second;
    time.tm_isdst = -1;
    return static_cast<std::int64_t>(std::mktime(&time));
}

bool parseIsoTime(const std::string& text, std::int64_t& result) {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0, used = 0;
    int count = std::sscanf(text.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%n", &year, &month, &day, &hour, &minute, &second, &used);
    if (count != 6) {
        used = 0;
        count = std::sscanf(text.c_str(), "%4d-%2d-%2d%n", &year, &month, &day, &used);
        if (count != 3) return false;
    }
    std::size_t pos = static_cast<std::size_t>(used);
    if (pos < text.size() && text[pos] == '.') {
        ++pos;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) ++pos;
    }
    int offsetMinutes = 0;
    if (pos < text.size() && (text[pos] == 'Z' || text[pos] == 'z')) ++pos;
    else if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
        const int sign = text[pos] == '-' ? -1 : 1;
        ++pos;
        int offsetHour = 0, offsetMinute = 0;
        if (std::sscanf(text.c_str() + pos, "%2d:%2d", &offsetHour, &offsetMinute) != 2) return false;
        offsetMinutes = sign * (offsetHour * 60 + offsetMinute);
        pos += 5;
    }
    if (pos != text.size()) return false;
    const std::int64_t utc = unixFromUtcParts(year, month, day, hour, minute, second);
    if (utc == -1) return false;
    result = utc - static_cast<std::int64_t>(offsetMinutes) * 60;
    return true;
}

void* dateTimeCall(const std::string& op, void** args, int argc) {
    const auto now = std::chrono::system_clock::now();
    if (op == "now_unix" || op == "simdi") return box(static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count()));
    if (op == "now_ms" || op == "simdi_ms") return box(static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()));
    if (op == "now_iso" || op == "simdi_iso") return box(formatTime(utcTimeFromUnix(static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count())), "%Y-%m-%dT%H:%M:%SZ"));
    if ((op == "to_iso" || op == "iso") && argc == 1) return box(formatTime(utcTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))), "%Y-%m-%dT%H:%M:%SZ"));
    if ((op == "parse_iso" || op == "iso_coz") && argc == 1) {
        std::int64_t result = 0;
        if (!parseIsoTime(argumentString(args, 0), result)) { writeStderr("PRCYTERAS DateTime.parse_iso: gecersiz ISO-8601 metni.\n"); return nullptr; }
        return box(result);
    }
    if ((op == "format" || op == "bicimle") && argc == 2) return box(formatTime(localTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))), argumentString(args, 1)));
    if ((op == "format_utc" || op == "utc_bicimle") && argc == 2) return box(formatTime(utcTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))), argumentString(args, 1)));
    if (argc == 1 && (op == "year" || op == "yil")) return box(static_cast<std::int64_t>(localTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))).tm_year + 1900));
    if (argc == 1 && (op == "month" || op == "ay")) return box(static_cast<std::int64_t>(localTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))).tm_mon + 1));
    if (argc == 1 && (op == "day" || op == "gun")) return box(static_cast<std::int64_t>(localTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))).tm_mday));
    if (argc == 1 && (op == "hour" || op == "saat")) return box(static_cast<std::int64_t>(localTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))).tm_hour));
    if (argc == 1 && (op == "minute" || op == "dakika")) return box(static_cast<std::int64_t>(localTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))).tm_min));
    if (argc == 1 && (op == "second" || op == "saniye")) return box(static_cast<std::int64_t>(localTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))).tm_sec));
    if (argc == 1 && (op == "weekday" || op == "hafta_gunu")) return box(static_cast<std::int64_t>(localTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))).tm_wday));
    if (argc == 1 && (op == "day_of_year" || op == "yil_gunu")) return box(static_cast<std::int64_t>(localTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))).tm_yday + 1));
    if ((op == "add_seconds" || op == "saniye_ekle") && argc == 2) return box(static_cast<std::int64_t>(numberOf(argument(args, 0)) + numberOf(argument(args, 1))));
    if ((op == "add_days" || op == "gun_ekle") && argc == 2) return box(static_cast<std::int64_t>(numberOf(argument(args, 0)) + numberOf(argument(args, 1)) * 86400.0));
    if ((op == "diff_seconds" || op == "fark_saniye") && argc == 2) return box(static_cast<std::int64_t>(numberOf(argument(args, 0)) - numberOf(argument(args, 1))));
    if ((op == "unix_from_parts" || op == "parcalardan") && argc == 6)
        return box(unixFromLocalParts(argumentInt(args, 0), argumentInt(args, 1), argumentInt(args, 2), argumentInt(args, 3), argumentInt(args, 4), argumentInt(args, 5)));
    if ((op == "utc_from_parts" || op == "utc_parcalardan") && argc == 6)
        return box(unixFromUtcParts(argumentInt(args, 0), argumentInt(args, 1), argumentInt(args, 2), argumentInt(args, 3), argumentInt(args, 4), argumentInt(args, 5)));
    if ((op == "is_leap_year" || op == "artik_yil") && argc == 1) {
        const int year = argumentInt(args, 0);
        return box(year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    }
    if ((op == "days_in_month" || op == "ay_gun_sayisi") && argc == 2) {
        const int year = argumentInt(args, 0), month = argumentInt(args, 1);
        static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month < 1 || month > 12) return box(std::int64_t{0});
        const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
        return box(static_cast<std::int64_t>(days[month - 1] + (month == 2 && leap ? 1 : 0)));
    }
    if (op == "timezone_offset_minutes" || op == "saat_dilimi") {
        const std::time_t current = std::time(nullptr);
        const std::tm local = localTimeFromUnix(static_cast<std::int64_t>(current));
        const std::int64_t localAsUtc = unixFromUtcParts(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);
        return box(static_cast<std::int64_t>((localAsUtc - static_cast<std::int64_t>(current)) / 60));
    }
    if ((op == "month_name" || op == "ay_adi") && argc == 1) {
        static const char* names[] = {"", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
        const int month = argumentInt(args, 0);
        return box(std::string(month >= 1 && month <= 12 ? names[month] : ""));
    }
    if ((op == "day_name" || op == "gun_adi") && argc == 1) {
        static const char* names[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        const int weekday = localTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0)))).tm_wday;
        return box(std::string(weekday >= 0 && weekday < 7 ? names[weekday] : ""));
    }
    if ((op == "start_of_day" || op == "gun_basi") && argc == 1) {
        const std::tm local = localTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0))));
        return box(unixFromLocalParts(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, 0, 0, 0));
    }
    if ((op == "start_of_day_utc" || op == "utc_gun_basi") && argc == 1) {
        const std::tm utc = utcTimeFromUnix(static_cast<std::int64_t>(numberOf(argument(args, 0))));
        return box(unixFromUtcParts(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, 0, 0, 0));
    }
    writeStderr("PRCYTERAS: DateTime.");
    writeStderr(op);
    writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");
    return nullptr;
}

// ben yaptım opencode - 2026-11-14 - TOML/YAML/XML/dotenv/UUID/Logger/Stats/Fuzzy arka uclari
Value stdlibObject(const std::string& type) {
    auto object = std::make_shared<Object>();
    object->type = type;
    return Value{object};
}
Value stdlibArray() {
    return Value{std::make_shared<ListValue>()};
}
void stdlibArrayAppend(Value& array, Value item) {
    if (auto* list = std::get_if<std::shared_ptr<ListValue>>(&array)) {
        (*list)->values.push_back(std::move(item));
        (*list)->active.push_back(true);
    }
}
bool stdlibIsObject(const Value& value) {
    const auto* object = std::get_if<std::shared_ptr<Object>>(&value);
    return object && *object;
}
bool stdlibIsArray(const Value& value) {
    const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value);
    return list && *list;
}
std::string stdlibTrim(std::string_view text) {
    std::size_t first = 0;
    std::size_t last = text.size();
    while (first < last && (text[first] == ' ' || text[first] == '\t' || text[first] == '\r' || text[first] == '\n')) ++first;
    while (last > first && (text[last - 1] == ' ' || text[last - 1] == '\t' || text[last - 1] == '\r' || text[last - 1] == '\n')) --last;
    return std::string(text.substr(first, last - first));
}
std::string stdlibLower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
std::shared_ptr<Object> stdlibEnsureTable(const std::shared_ptr<Object>& root, const std::vector<std::string>& path, bool appendArrayTable) {
    std::shared_ptr<Object> current = root;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const bool last = i + 1 == path.size();
        auto& slot = current->dynamicFields[path[i]];
        if (last && appendArrayTable) {
            auto* list = std::get_if<std::shared_ptr<ListValue>>(&slot);
            if (!list || !*list) {
                slot = stdlibArray();
                list = std::get_if<std::shared_ptr<ListValue>>(&slot);
            }
            auto child = std::make_shared<Object>();
            child->type = "TomlTable";
            (*list)->values.emplace_back(Value{child});
            (*list)->active.push_back(true);
            return child;
        }
        if (auto* list = std::get_if<std::shared_ptr<ListValue>>(&slot)) {
            if ((*list)->values.empty()) return nullptr;
            auto* child = std::get_if<std::shared_ptr<Object>>(&(*list)->values.back());
            if (!child || !*child) return nullptr;
            current = *child;
            continue;
        }
        auto* object = std::get_if<std::shared_ptr<Object>>(&slot);
        if (!object || !*object) {
            auto child = std::make_shared<Object>();
            child->type = "TomlTable";
            slot = Value{child};
            current = child;
        } else {
            current = *object;
        }
    }
    return current;
}

struct TomlParser {
    std::string text;
    std::size_t pos = 0;
    int line = 1;
    std::string error;

    explicit TomlParser(std::string source) : text(std::move(source)) {}

    bool fail(const std::string& message) {
        if (error.empty()) error = message + " (satir " + std::to_string(line) + ")";
        return false;
    }
    bool eof() const { return pos >= text.size(); }
    char peek(std::size_t offset = 0) const { return pos + offset < text.size() ? text[pos + offset] : '\0'; }
    void advance() {
        if (pos >= text.size()) return;
        if (text[pos] == '\n') ++line;
        ++pos;
    }
    void skipSpaces() { while (!eof() && (peek() == ' ' || peek() == '\t')) advance(); }
    void skipComment() { if (peek() == '#') while (!eof() && peek() != '\n') advance(); }
    void skipAll() {
        while (!eof()) {
            if (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n') { advance(); continue; }
            if (peek() == '#') { skipComment(); continue; }
            break;
        }
    }
    void skipToLineEnd() {
        skipSpaces();
        skipComment();
        if (!eof() && peek() == '\r') advance();
        if (!eof() && peek() == '\n') advance();
        else if (!eof()) fail("satir sonu bekleniyordu");
    }
    bool parseKeySegment(std::string& key) {
        skipSpaces();
        if (peek() == '"' || peek() == '\'') return parseString(key);
        std::string result;
        while (!eof()) {
            const char c = peek();
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
                result.push_back(c);
                advance();
            } else break;
        }
        if (result.empty()) return fail("anahtar bekleniyordu");
        key = std::move(result);
        return true;
    }
    bool parseKeyPath(std::vector<std::string>& path) {
        path.clear();
        std::string segment;
        if (!parseKeySegment(segment)) return false;
        path.push_back(std::move(segment));
        while (true) {
            skipSpaces();
            if (peek() != '.') break;
            advance();
            if (!parseKeySegment(segment)) return false;
            path.push_back(std::move(segment));
        }
        return true;
    }
    bool parseString(std::string& out) {
        out.clear();
        if (peek() == '"' && peek(1) == '"' && peek(2) == '"') {
            pos += 3;
            while (!eof()) {
                if (peek() == '"' && peek(1) == '"' && peek(2) == '"') { pos += 3; return true; }
                if (peek() == '\\') { advance(); if (!parseEscape(out)) return false; }
                else { out.push_back(peek()); advance(); }
            }
            return fail("cok satirli metin kapanmadi");
        }
        if (peek() == '\'' && peek(1) == '\'' && peek(2) == '\'') {
            pos += 3;
            while (!eof()) {
                if (peek() == '\'' && peek(1) == '\'' && peek(2) == '\'') { pos += 3; return true; }
                out.push_back(peek());
                advance();
            }
            return fail("cok satirli literal metin kapanmadi");
        }
        const char quote = peek();
        if (quote != '"' && quote != '\'') return fail("metin bekleniyordu");
        advance();
        while (!eof()) {
            if (peek() == quote) { advance(); return true; }
            if (quote == '"' && peek() == '\\') { advance(); if (!parseEscape(out)) return false; }
            else if (peek() == '\n') return fail("metin ayni satirda bitmeli");
            else { out.push_back(peek()); advance(); }
        }
        return fail("metin kapanmadi");
    }
    bool parseEscape(std::string& out) {
        if (eof()) return fail("kacis eksik");
        const char c = peek();
        advance();
        switch (c) {
            case 'b': out.push_back('\b'); return true;
            case 't': out.push_back('\t'); return true;
            case 'n': out.push_back('\n'); return true;
            case 'f': out.push_back('\f'); return true;
            case 'r': out.push_back('\r'); return true;
            case '"': out.push_back('"'); return true;
            case '\\': out.push_back('\\'); return true;
            case 'u': case 'U': {
                const int digits = c == 'u' ? 4 : 8;
                unsigned int code = 0;
                for (int i = 0; i < digits; ++i) {
                    if (eof()) return fail("unicode kacisi eksik");
                    const char h = peek();
                    advance();
                    code <<= 4;
                    if (h >= '0' && h <= '9') code |= static_cast<unsigned int>(h - '0');
                    else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned int>(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned int>(h - 'A' + 10);
                    else return fail("gecersiz unicode kacisi");
                }
                JsonReader::appendUtf8(out, code);
                return true;
            }
            default: return fail("bilinmeyen kacis");
        }
    }
    bool parseValue(Value& out);
    bool parseArray(Value& out) {
        if (peek() != '[') return fail("'[' bekleniyordu");
        advance();
        auto list = std::make_shared<ListValue>();
        skipAll();
        if (peek() == ']') { advance(); out = list; return true; }
        while (true) {
            Value item;
            if (!parseValue(item)) return false;
            list->values.push_back(std::move(item));
            list->active.push_back(true);
            skipAll();
            if (peek() == ']') { advance(); break; }
            if (peek() != ',') return fail("',' veya ']' bekleniyordu");
            advance();
            skipAll();
        }
        out = list;
        return true;
    }
    bool parseInlineTable(Value& out) {
        if (peek() != '{') return fail("'{' bekleniyordu");
        advance();
        auto object = std::make_shared<Object>();
        object->type = "TomlTable";
        skipAll();
        if (peek() == '}') { advance(); out = object; return true; }
        while (true) {
            std::vector<std::string> path;
            if (!parseKeyPath(path)) return false;
            skipAll();
            if (peek() != '=') return fail("'=' bekleniyordu");
            advance();
            skipAll();
            Value value;
            if (!parseValue(value)) return false;
            if (!setKey(object, path, std::move(value))) return false;
            skipAll();
            if (peek() == '}') { advance(); break; }
            if (peek() != ',') return fail("',' veya '}' bekleniyordu");
            advance();
            skipAll();
        }
        out = object;
        return true;
    }
    bool setKey(const std::shared_ptr<Object>& root, const std::vector<std::string>& path, Value value) {
        if (path.empty()) return fail("anahtar yolu bos");
        std::vector<std::string> parent(path.begin(), path.end() - 1);
        auto table = stdlibEnsureTable(root, parent, false);
        if (!table) return fail("tablo olusturulamadi");
        if (table->dynamicFields.contains(path.back())) return fail("ayni anahtar tekrar edemez: " + path.back());
        table->dynamicFields[path.back()] = std::move(value);
        return true;
    }
    bool parseBareValue(Value& out) {
        const std::size_t start = pos;
        while (!eof()) {
            const char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == ']' || c == '}' || c == '#') break;
            advance();
        }
        std::string token = text.substr(start, pos - start);
        if (token.empty()) return fail("deger bekleniyordu");
        const std::string lower = stdlibLower(token);
        if (lower == "true") { out = true; return true; }
        if (lower == "false") { out = false; return true; }
        if (lower == "inf" || lower == "+inf") { out = std::numeric_limits<double>::infinity(); return true; }
        if (lower == "-inf") { out = -std::numeric_limits<double>::infinity(); return true; }
        if (lower == "nan" || lower == "+nan" || lower == "-nan") { out = std::numeric_limits<double>::quiet_NaN(); return true; }
        if (token.size() >= 10 && std::isdigit(static_cast<unsigned char>(token[0])) && token[4] == '-') { out = token; return true; }
        std::string number = token;
        number.erase(std::remove(number.begin(), number.end(), '_'), number.end());
        const bool isFloat = number.find('.') != std::string::npos || number.find('e') != std::string::npos || number.find('E') != std::string::npos;
        try {
            std::size_t used = 0;
            if (!isFloat && number.size() > 2 && number[0] == '0' &&
                (number[1] == 'x' || number[1] == 'X' || number[1] == 'o' || number[1] == 'O' || number[1] == 'b' || number[1] == 'B')) {
                int base = 10;
                if (number[1] == 'x' || number[1] == 'X') base = 16;
                else if (number[1] == 'o' || number[1] == 'O') base = 8;
                else base = 2;
                const long long value = std::stoll(number.substr(2), &used, base);
                if (used == number.size() - 2) { out = static_cast<std::int64_t>(value); return true; }
            } else if (!isFloat) {
                const long long value = std::stoll(number, &used, 10);
                if (used == number.size()) { out = static_cast<std::int64_t>(value); return true; }
            }
            const double value = std::stod(number, &used);
            if (used == number.size()) { out = value; return true; }
        } catch (...) {}
        out = token;
        return true;
    }
    bool parseDocument(Value& out) {
        auto root = std::make_shared<Object>();
        root->type = "TomlTable";
        std::shared_ptr<Object> current = root;
        while (true) {
            skipAll();
            if (eof()) break;
            if (peek() == '[') {
                const bool arrayTable = peek(1) == '[';
                pos += arrayTable ? 2 : 1;
                std::vector<std::string> path;
                if (!parseKeyPath(path)) return false;
                skipSpaces();
                if (arrayTable) {
                    if (peek() != ']' || peek(1) != ']') return fail("']]' bekleniyordu");
                    pos += 2;
                } else {
                    if (peek() != ']') return fail("']' bekleniyordu");
                    ++pos;
                }
                auto table = stdlibEnsureTable(root, path, arrayTable);
                if (!table) return fail("tablo basligi gecersiz");
                current = table;
                skipToLineEnd();
                if (!error.empty()) return false;
                continue;
            }
            std::vector<std::string> path;
            if (!parseKeyPath(path)) return false;
            skipSpaces();
            if (peek() != '=') return fail("'=' bekleniyordu");
            advance();
            skipSpaces();
            Value value;
            if (!parseValue(value)) return false;
            if (!setKey(current, path, std::move(value))) return false;
            skipToLineEnd();
            if (!error.empty()) return false;
        }
        out = Value{root};
        return true;
    }
};

bool TomlParser::parseValue(Value& out) {
    skipSpaces();
    if (eof()) return fail("deger bekleniyordu");
    if (peek() == '"' || peek() == '\'') {
        std::string textValue;
        if (!parseString(textValue)) return false;
        out = std::move(textValue);
        return true;
    }
    if (peek() == '[') return parseArray(out);
    if (peek() == '{') return parseInlineTable(out);
    return parseBareValue(out);
}

void tomlEscape(std::string& out, const std::string& text) {
    out.push_back('"');
    static const char hex[] = "0123456789abcdef";
    for (const unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(hex[(c >> 4) & 0xF]);
                    out.push_back(hex[c & 0xF]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    out.push_back('"');
}
std::string tomlScalar(const Value& value);
bool tomlIsTableArray(const Value& value) {
    const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value);
    if (!list || !*list) return false;
    for (std::size_t i = 0; i < (*list)->values.size(); ++i) {
        if (i < (*list)->active.size() && !(*list)->active[i]) continue;
        return stdlibIsObject((*list)->values[i]);
    }
    return false;
}
std::string tomlScalar(const Value& value) {
    if (const auto* item = std::get_if<std::string>(&value)) {
        std::string out;
        tomlEscape(out, *item);
        return out;
    }
    if (const auto* item = std::get_if<std::int64_t>(&value)) return std::to_string(*item);
    if (const auto* item = std::get_if<std::uint64_t>(&value)) return std::to_string(*item);
    if (const auto* item = std::get_if<bool>(&value)) return *item ? "true" : "false";
    if (const auto* item = std::get_if<double>(&value)) {
        if (std::isnan(*item)) return "nan";
        if (std::isinf(*item)) return *item < 0 ? "-inf" : "inf";
        std::ostringstream stream;
        stream << std::setprecision(17) << *item;
        std::string text = stream.str();
        if (text.find('.') == std::string::npos && text.find('e') == std::string::npos && text.find('E') == std::string::npos) text += ".0";
        return text;
    }
    if (const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value)) {
        std::string out = "[";
        bool first = true;
        for (std::size_t i = 0; i < (*list)->values.size(); ++i) {
            if (i < (*list)->active.size() && !(*list)->active[i]) continue;
            if (!first) out += ", ";
            first = false;
            out += tomlScalar((*list)->values[i]);
        }
        out += "]";
        return out;
    }
    if (const auto* object = std::get_if<std::shared_ptr<Object>>(&value)) {
        std::vector<std::string> keys;
        for (const auto& field : (*object)->dynamicFields) keys.push_back(field.first);
        std::sort(keys.begin(), keys.end());
        std::string out = "{";
        bool first = true;
        for (const auto& key : keys) {
            if (!first) out += ", ";
            first = false;
            out += key + " = " + tomlScalar((*object)->dynamicFields.at(key));
        }
        out += "}";
        return out;
    }
    return "\"\"";
}
void tomlEmitTableBody(const Value& table, const std::string& prefix, std::string& out) {
    const auto* object = std::get_if<std::shared_ptr<Object>>(&table);
    if (!object || !*object) return;
    std::vector<std::string> keys;
    for (const auto& field : (*object)->dynamicFields) keys.push_back(field.first);
    std::sort(keys.begin(), keys.end());
    for (const auto& key : keys) {
        const Value& value = (*object)->dynamicFields.at(key);
        if (stdlibIsObject(value)) continue;
        if (stdlibIsArray(value) && tomlIsTableArray(value)) continue;
        out += key + " = " + tomlScalar(value) + "\n";
    }
    for (const auto& key : keys) {
        const Value& value = (*object)->dynamicFields.at(key);
        const std::string path = prefix.empty() ? key : prefix + "." + key;
        if (stdlibIsObject(value)) {
            out += "\n[" + path + "]\n";
            tomlEmitTableBody(value, path, out);
        } else if (stdlibIsArray(value) && tomlIsTableArray(value)) {
            const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value);
            for (std::size_t i = 0; i < (*list)->values.size(); ++i) {
                if (i < (*list)->active.size() && !(*list)->active[i]) continue;
                out += "\n[[" + path + "]]\n";
                tomlEmitTableBody((*list)->values[i], path, out);
            }
        }
    }
}
std::string tomlSerialize(const Value& value) {
    std::string out;
    tomlEmitTableBody(value, "", out);
    return stdlibTrim(out);
}
void* tomlCall(const std::string& op, void** args, int argc) {
    if ((op == "parse" || op == "coz") && argc == 1) {
        TomlParser parser(argumentString(args, 0));
        Value result;
        if (!parser.parseDocument(result)) {
            writeStderr("PRCYTERAS Toml.parse hatasi: ");
            writeStderr(parser.error);
            writeStderr("\n");
            return nullptr;
        }
        return box(std::move(result));
    }
    if ((op == "stringify" || op == "yaz") && argc == 1) return box(tomlSerialize(argument(args, 0)));
    return jsonCall(op, args, argc);
}

// ben yaptım opencode - 2026-11-14 - YAML ayristirici ve serilestirici
std::string yamlStripComment(const std::string& line) {
    char quote = 0;
    bool escape = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quote) {
            if (escape) escape = false;
            else if (c == '\\' && quote == '"') escape = true;
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') { quote = c; continue; }
        if (c == '#' && (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t')) return line.substr(0, i);
    }
    return line;
}
std::size_t yamlFindColon(const std::string& text) {
    char quote = 0;
    bool escape = false;
    int depth = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (quote) {
            if (escape) escape = false;
            else if (c == '\\' && quote == '"') escape = true;
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') { quote = c; continue; }
        if (c == '[' || c == '{') ++depth;
        else if (c == ']' || c == '}') --depth;
        else if (c == ':' && depth == 0) return i;
    }
    return std::string::npos;
}
std::string yamlUnquoteKey(const std::string& text) {
    const std::string trimmed = stdlibTrim(text);
    if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
        std::string out;
        for (std::size_t i = 1; i + 1 < trimmed.size(); ++i) {
            if (trimmed[i] == '\\' && i + 2 < trimmed.size()) {
                const char next = trimmed[++i];
                if (next == 'n') out.push_back('\n');
                else if (next == 't') out.push_back('\t');
                else if (next == 'r') out.push_back('\r');
                else out.push_back(next);
            } else out.push_back(trimmed[i]);
        }
        return out;
    }
    if (trimmed.size() >= 2 && trimmed.front() == '\'' && trimmed.back() == '\'') return trimmed.substr(1, trimmed.size() - 2);
    return trimmed;
}
struct YamlParser {
    struct Line { int indent; std::string content; std::string raw; int number; };
    std::vector<Line> lines;
    std::string error;

    explicit YamlParser(const std::string& source) { prepare(source); }
    bool fail(const std::string& message) { if (error.empty()) error = message; return false; }
    void prepare(const std::string& source) {
        std::istringstream stream(source);
        std::string raw;
        int number = 0;
        while (std::getline(stream, raw)) {
            ++number;
            if (!raw.empty() && raw.back() == '\r') raw.pop_back();
            if (number == 1 && raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF &&
                static_cast<unsigned char>(raw[1]) == 0xBB && static_cast<unsigned char>(raw[2]) == 0xBF) raw.erase(0, 3);
            int indent = 0;
            while (indent < static_cast<int>(raw.size()) && raw[indent] == ' ') ++indent;
            if (indent < static_cast<int>(raw.size()) && raw[indent] == '\t') {
                fail("YAML girintisinde sekme kullanilamaz (satir " + std::to_string(number) + ")");
                return;
            }
            const std::string withoutIndent = raw.substr(static_cast<std::size_t>(indent));
            const std::string content = stdlibTrim(yamlStripComment(withoutIndent));
            if (content.empty() || content == "---" || content == "...") continue;
            lines.push_back(Line{indent, content, raw, number});
        }
    }
    bool isSequenceLine(const std::string& content) const {
        return content == "-" || content.rfind("- ", 0) == 0;
    }
    std::vector<std::string> splitInline(const std::string& text, char separator) const {
        std::vector<std::string> parts;
        std::string current;
        int depth = 0;
        char quote = 0;
        bool escape = false;
        for (const char c : text) {
            if (quote) {
                current.push_back(c);
                if (escape) escape = false;
                else if (c == '\\' && quote == '"') escape = true;
                else if (c == quote) quote = 0;
                continue;
            }
            if (c == '"' || c == '\'') { quote = c; current.push_back(c); continue; }
            if (c == '[' || c == '{') ++depth;
            else if (c == ']' || c == '}') --depth;
            if (c == separator && depth == 0) {
                parts.push_back(stdlibTrim(current));
                current.clear();
            } else current.push_back(c);
        }
        const std::string tail = stdlibTrim(current);
        if (!tail.empty()) parts.push_back(tail);
        return parts;
    }
    bool parseQuoted(const std::string& text, std::string& out) const {
        out.clear();
        if (text.size() < 2) return false;
        const char quote = text.front();
        if (quote != '"' && quote != '\'') return false;
        if (text.back() != quote) return false;
        if (quote == '\'') {
            for (std::size_t i = 1; i + 1 < text.size(); ++i) {
                if (text[i] == '\'' && i + 2 < text.size() && text[i + 1] == '\'') { out.push_back('\''); ++i; }
                else out.push_back(text[i]);
            }
            return true;
        }
        for (std::size_t i = 1; i + 1 < text.size(); ++i) {
            if (text[i] != '\\') { out.push_back(text[i]); continue; }
            if (i + 1 >= text.size() - 1) break;
            const char c = text[++i];
            if (c == 'n') out.push_back('\n');
            else if (c == 'r') out.push_back('\r');
            else if (c == 't') out.push_back('\t');
            else if (c == 'b') out.push_back('\b');
            else if (c == 'f') out.push_back('\f');
            else if (c == '0') out.push_back('\0');
            else out.push_back(c);
        }
        return true;
    }
    bool parseInlineList(const std::string& text, Value& out) const {
        auto list = std::make_shared<ListValue>();
        const std::string inside = stdlibTrim(text.substr(1, text.size() - 2));
        if (!inside.empty()) {
            for (const std::string& part : splitInline(inside, ',')) {
                list->values.push_back(parseScalar(part));
                list->active.push_back(true);
            }
        }
        out = Value{list};
        return true;
    }
    bool parseInlineMap(const std::string& text, Value& out) const {
        auto object = std::make_shared<Object>();
        object->type = "YamlMap";
        const std::string inside = stdlibTrim(text.substr(1, text.size() - 2));
        if (!inside.empty()) {
            for (const std::string& part : splitInline(inside, ',')) {
                const std::size_t colon = yamlFindColon(part);
                if (colon == std::string::npos) continue;
                object->dynamicFields[yamlUnquoteKey(part.substr(0, colon))] = parseScalar(part.substr(colon + 1));
            }
        }
        out = Value{object};
        return true;
    }
    Value parseScalar(const std::string& input) const {
        const std::string text = stdlibTrim(input);
        if (text.empty()) return Value{};
        const std::string lower = stdlibLower(text);
        if (lower == "null" || lower == "~") return Value{};
        if (lower == "true") return true;
        if (lower == "false") return false;
        if (text.front() == '"' || text.front() == '\'') {
            std::string out;
            if (parseQuoted(text, out)) return out;
        }
        if (text.front() == '[' && text.back() == ']') {
            Value out;
            if (parseInlineList(text, out)) return out;
        }
        if (text.front() == '{' && text.back() == '}') {
            Value out;
            if (parseInlineMap(text, out)) return out;
        }
        try {
            std::size_t used = 0;
            const long long integer = std::stoll(text, &used, 10);
            if (used == text.size()) return static_cast<std::int64_t>(integer);
        } catch (...) {}
        try {
            std::size_t used = 0;
            const double number = std::stod(text, &used);
            if (used == text.size()) return number;
        } catch (...) {}
        return text;
    }
    Value parseBlockScalar(std::size_t& i, int parentIndent, const std::string& indicator) {
        const bool folded = !indicator.empty() && indicator.front() == '>';
        const bool strip = indicator.find('-') != std::string::npos;
        const bool keep = indicator.find('+') != std::string::npos;
        if (i >= lines.size() || lines[i].indent <= parentIndent) return std::string();
        const int baseIndent = lines[i].indent;
        std::string result;
        bool first = true;
        while (i < lines.size() && lines[i].indent > parentIndent) {
            const std::string& raw = lines[i].raw;
            const std::size_t cut = std::min<std::size_t>(raw.size(), static_cast<std::size_t>(baseIndent));
            const std::string text = raw.substr(cut);
            if (first) { result = text; first = false; }
            else if (text.empty()) result += "\n";
            else if (folded) result += " " + text;
            else result += "\n" + text;
            ++i;
        }
        if (strip) {
            while (!result.empty() && result.back() == '\n') result.pop_back();
        } else if (keep) {
            result += "\n";
        } else if (!result.empty() && result.back() != '\n') {
            result += "\n";
        }
        return result;
    }
    void parseMappingEntry(std::size_t& i, int indent, const std::shared_ptr<Object>& object) {
        if (i >= lines.size()) return;
        const std::string content = lines[i].content;
        const std::size_t colon = yamlFindColon(content);
        if (colon == std::string::npos) { fail("YAML anahtar/deger bekleniyordu (satir " + std::to_string(lines[i].number) + ")"); return; }
        const std::string key = yamlUnquoteKey(content.substr(0, colon));
        const std::string valueText = stdlibTrim(content.substr(colon + 1));
        ++i;
        if (valueText.empty()) {
            if (i < lines.size() && lines[i].indent > indent) object->dynamicFields[key] = parseBlock(i, lines[i].indent);
            else object->dynamicFields[key] = Value{};
        } else if (valueText.front() == '|' || valueText.front() == '>') {
            object->dynamicFields[key] = parseBlockScalar(i, indent, valueText);
        } else {
            object->dynamicFields[key] = parseScalar(valueText);
        }
    }
    Value parseMapping(std::size_t& i, int indent) {
        auto object = std::make_shared<Object>();
        object->type = "YamlMap";
        while (i < lines.size() && lines[i].indent == indent && !isSequenceLine(lines[i].content)) {
            parseMappingEntry(i, indent, object);
            if (!error.empty()) break;
        }
        return Value{object};
    }
    Value parseSequence(std::size_t& i, int indent) {
        auto list = std::make_shared<ListValue>();
        while (i < lines.size() && lines[i].indent == indent && isSequenceLine(lines[i].content)) {
            const std::string content = lines[i].content;
            const std::string rest = content == "-" ? std::string() : stdlibTrim(content.substr(2));
            ++i;
            if (rest.empty()) {
                if (i < lines.size() && lines[i].indent > indent) list->values.push_back(parseBlock(i, lines[i].indent));
                else list->values.push_back(Value{});
                list->active.push_back(true);
                continue;
            }
            const std::size_t colon = yamlFindColon(rest);
            if (colon != std::string::npos) {
                auto object = std::make_shared<Object>();
                object->type = "YamlMap";
                const std::string key = yamlUnquoteKey(rest.substr(0, colon));
                const std::string valueText = stdlibTrim(rest.substr(colon + 1));
                if (valueText.empty()) {
                    if (i < lines.size() && lines[i].indent > indent) object->dynamicFields[key] = parseBlock(i, lines[i].indent);
                    else object->dynamicFields[key] = Value{};
                } else if (valueText.front() == '|' || valueText.front() == '>') {
                    object->dynamicFields[key] = parseBlockScalar(i, indent, valueText);
                } else {
                    object->dynamicFields[key] = parseScalar(valueText);
                }
                int childIndent = indent + 2;
                if (i < lines.size() && lines[i].indent > indent) childIndent = lines[i].indent;
                while (i < lines.size() && lines[i].indent == childIndent && !isSequenceLine(lines[i].content)) {
                    parseMappingEntry(i, childIndent, object);
                    if (!error.empty()) break;
                }
                list->values.push_back(Value{object});
                list->active.push_back(true);
                continue;
            }
            if (rest.front() == '|' || rest.front() == '>') list->values.push_back(parseBlockScalar(i, indent, rest));
            else list->values.push_back(parseScalar(rest));
            list->active.push_back(true);
        }
        return Value{list};
    }
    Value parseBlock(std::size_t& i, int indent) {
        if (i >= lines.size()) return Value{};
        if (lines[i].indent < indent) return Value{};
        if (lines[i].indent > indent) indent = lines[i].indent;
        if (isSequenceLine(lines[i].content)) return parseSequence(i, indent);
        if (yamlFindColon(lines[i].content) == std::string::npos) {
            const std::string text = lines[i].content;
            ++i;
            return parseScalar(text);
        }
        return parseMapping(i, indent);
    }
    bool parseDocument(Value& out) {
        if (!error.empty()) return false;
        if (lines.empty()) { out = stdlibObject("YamlMap"); return true; }
        std::size_t i = 0;
        out = parseBlock(i, lines[0].indent);
        return error.empty();
    }
};
std::string yamlEscape(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out.push_back(c);
    }
    out += "\"";
    return out;
}
bool yamlNeedsQuote(const std::string& text) {
    if (text.empty()) return true;
    if (text.front() == ' ' || text.back() == ' ') return true;
    if (text == "null" || text == "true" || text == "false" || text == "~") return true;
    if (text.front() == '-' || text.front() == '?' || text.front() == ':' || text.front() == '#' ||
        text.front() == '&' || text.front() == '*' || text.front() == '!' || text.front() == '|' ||
        text.front() == '>' || text.front() == '@' || text.front() == '`') return true;
    for (const char c : text) {
        if (c == ':' || c == '#' || c == '[' || c == ']' || c == '{' || c == '}' || c == ',' ||
            c == '\n' || c == '\r' || c == '\t' || c == '"' || c == '\'') return true;
    }
    return false;
}
std::string yamlKeyText(const std::string& key) {
    return yamlNeedsQuote(key) ? yamlEscape(key) : key;
}
std::string yamlScalarText(const Value& value) {
    if (const auto* item = std::get_if<std::string>(&value)) return yamlNeedsQuote(*item) ? yamlEscape(*item) : *item;
    if (const auto* item = std::get_if<std::int64_t>(&value)) return std::to_string(*item);
    if (const auto* item = std::get_if<std::uint64_t>(&value)) return std::to_string(*item);
    if (const auto* item = std::get_if<bool>(&value)) return *item ? "true" : "false";
    if (const auto* item = std::get_if<double>(&value)) {
        std::ostringstream stream;
        stream << std::setprecision(17) << *item;
        return stream.str();
    }
    if (stdlibIsObject(value)) {
        const auto* object = std::get_if<std::shared_ptr<Object>>(&value);
        if ((*object)->dynamicFields.empty()) return "{}";
        return "{}";
    }
    if (stdlibIsArray(value)) {
        const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value);
        if ((*list)->values.empty()) return "[]";
        return "[]";
    }
    return "null";
}
bool yamlNonEmptyObject(const Value& value) {
    const auto* object = std::get_if<std::shared_ptr<Object>>(&value);
    return object && *object && !(*object)->dynamicFields.empty();
}
bool yamlNonEmptyArray(const Value& value) {
    const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value);
    return list && *list && !(*list)->values.empty();
}
void yamlEmit(const Value& value, int indent, std::string& out) {
    const std::string spaces(static_cast<std::size_t>(std::max(0, indent)), ' ');
    if (stdlibIsObject(value)) {
        const auto* object = std::get_if<std::shared_ptr<Object>>(&value);
        std::vector<std::string> keys;
        for (const auto& field : (*object)->dynamicFields) keys.push_back(field.first);
        std::sort(keys.begin(), keys.end());
        if (keys.empty()) { out += spaces + "{}\n"; return; }
        for (const auto& key : keys) {
            const Value& child = (*object)->dynamicFields.at(key);
            out += spaces + yamlKeyText(key) + ":";
            if (yamlNonEmptyObject(child) || yamlNonEmptyArray(child)) {
                out += "\n";
                yamlEmit(child, indent + 2, out);
            } else {
                out += " " + yamlScalarText(child) + "\n";
            }
        }
        return;
    }
    if (stdlibIsArray(value)) {
        const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value);
        if ((*list)->values.empty()) { out += spaces + "[]\n"; return; }
        for (std::size_t i = 0; i < (*list)->values.size(); ++i) {
            if (i < (*list)->active.size() && !(*list)->active[i]) continue;
            const Value& child = (*list)->values[i];
            out += spaces + "-";
            if (yamlNonEmptyObject(child) || yamlNonEmptyArray(child)) {
                out += "\n";
                yamlEmit(child, indent + 2, out);
            } else {
                out += " " + yamlScalarText(child) + "\n";
            }
        }
        return;
    }
    out += spaces + yamlScalarText(value) + "\n";
}
std::string yamlSerialize(const Value& value) {
    std::string out;
    yamlEmit(value, 0, out);
    return stdlibTrim(out);
}
void* yamlCall(const std::string& op, void** args, int argc) {
    if ((op == "parse" || op == "coz") && argc == 1) {
        YamlParser parser(argumentString(args, 0));
        Value result;
        if (!parser.parseDocument(result)) {
            writeStderr("PRCYTERAS Yaml.parse hatasi: ");
            writeStderr(parser.error);
            writeStderr("\n");
            return nullptr;
        }
        return box(std::move(result));
    }
    if ((op == "stringify" || op == "yaz") && argc == 1) return box(yamlSerialize(argument(args, 0)));
    return jsonCall(op, args, argc);
}

// ben yaptım opencode - 2026-11-14 - XML ayristirici ve serilestirici
void xmlAppendUtf8(std::string& out, unsigned int code) { JsonReader::appendUtf8(out, code); }
std::string xmlDecodeEntities(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '&') { out.push_back(text[i]); continue; }
        const std::size_t end = text.find(';', i + 1);
        if (end == std::string::npos) { out.push_back('&'); continue; }
        const std::string entity = text.substr(i + 1, end - i - 1);
        if (entity == "amp") out.push_back('&');
        else if (entity == "lt") out.push_back('<');
        else if (entity == "gt") out.push_back('>');
        else if (entity == "quot") out.push_back('"');
        else if (entity == "apos") out.push_back('\'');
        else if (!entity.empty() && entity[0] == '#') {
            unsigned int code = 0;
            bool valid = true;
            if (entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X')) {
                if (entity.size() == 2) valid = false;
                for (std::size_t k = 2; valid && k < entity.size(); ++k) {
                    const char c = entity[k];
                    code <<= 4;
                    if (c >= '0' && c <= '9') code |= static_cast<unsigned int>(c - '0');
                    else if (c >= 'a' && c <= 'f') code |= static_cast<unsigned int>(c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') code |= static_cast<unsigned int>(c - 'A' + 10);
                    else valid = false;
                }
            } else {
                for (std::size_t k = 1; valid && k < entity.size(); ++k) {
                    const char c = entity[k];
                    if (c < '0' || c > '9') valid = false;
                    else code = code * 10 + static_cast<unsigned int>(c - '0');
                }
            }
            if (valid && code > 0) xmlAppendUtf8(out, code);
            else { out.push_back('&'); out += entity; out.push_back(';'); }
        } else {
            out.push_back('&');
            out += entity;
            out.push_back(';');
        }
        i = end;
    }
    return out;
}
std::string xmlEscapeText(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out.push_back(c);
    }
    return out;
}
std::string xmlEscapeAttribute(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out.push_back(c);
    }
    return out;
}
struct XmlParser {
    std::string text;
    std::size_t pos = 0;
    std::string error;
    explicit XmlParser(std::string source) : text(std::move(source)) {}
    bool fail(const std::string& message) { if (error.empty()) error = message; return false; }
    bool eof() const { return pos >= text.size(); }
    char peek(std::size_t offset = 0) const { return pos + offset < text.size() ? text[pos + offset] : '\0'; }
    void advance() { if (pos < text.size()) ++pos; }
    bool startsWith(const char* marker) const {
        const std::size_t length = std::strlen(marker);
        return text.compare(pos, length, marker) == 0;
    }
    void skipSpaces() { while (!eof() && (peek() == ' ' || peek() == '\t' || peek() == '\r' || peek() == '\n')) advance(); }
    void skipUntil(const char* marker) {
        const std::size_t length = std::strlen(marker);
        const std::size_t found = text.find(marker, pos);
        pos = found == std::string::npos ? text.size() : found + length;
    }
    void skipDoctype() {
        int depth = 0;
        char quote = 0;
        while (!eof()) {
            const char c = peek();
            advance();
            if (quote) { if (c == quote) quote = 0; continue; }
            if (c == '"' || c == '\'') quote = c;
            else if (c == '[') ++depth;
            else if (c == ']') { if (depth > 0) --depth; }
            else if (c == '>' && depth == 0) return;
        }
    }
    bool skipMisc() {
        while (true) {
            skipSpaces();
            if (startsWith("<!--")) { pos += 4; skipUntil("-->"); continue; }
            if (startsWith("<?")) { pos += 2; skipUntil("?>"); continue; }
            if (startsWith("<!DOCTYPE")) { pos += 9; skipDoctype(); continue; }
            break;
        }
        return true;
    }
    bool parseName(std::string& name) {
        name.clear();
        while (!eof()) {
            const char c = peek();
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '_' || c == '-' || c == '.' || c == ':') { name.push_back(c); advance(); }
            else break;
        }
        if (name.empty()) return fail("XML etiket adi bekleniyordu");
        return true;
    }
    bool parseQuoted(std::string& out) {
        const char quote = peek();
        if (quote != '"' && quote != '\'') return fail("XML oznitelik degeri tirnak icinde olmali");
        advance();
        std::string raw;
        while (!eof() && peek() != quote) { raw.push_back(peek()); advance(); }
        if (eof()) return fail("XML oznitelik degeri kapanmadi");
        advance();
        out = xmlDecodeEntities(raw);
        return true;
    }
    bool parseAttributes(std::shared_ptr<Object>& attributes, bool& selfClosing, bool& closed) {
        attributes = std::make_shared<Object>();
        attributes->type = "XmlAttributes";
        selfClosing = false;
        closed = false;
        while (true) {
            skipSpaces();
            if (eof()) return fail("XML etiketi kapanmadi");
            if (startsWith("/>")) { pos += 2; selfClosing = true; closed = true; return true; }
            if (peek() == '>') { advance(); closed = true; return true; }
            std::string name;
            if (!parseName(name)) return false;
            skipSpaces();
            if (peek() != '=') return fail("XML oznitelik degeri icin '=' bekleniyordu");
            advance();
            skipSpaces();
            std::string value;
            if (!parseQuoted(value)) return false;
            attributes->dynamicFields[name] = std::move(value);
        }
    }
    bool parseElement(Value& out) {
        skipSpaces();
        if (peek() != '<') return fail("XML etiketi bekleniyordu");
        advance();
        if (peek() == '/' || peek() == '!' || peek() == '?') return fail("XML etiketi gecersiz");
        std::string name;
        if (!parseName(name)) return false;
        std::shared_ptr<Object> attributes;
        bool selfClosing = false;
        bool closed = false;
        if (!parseAttributes(attributes, selfClosing, closed)) return false;
        auto element = std::make_shared<Object>();
        element->type = "XmlElement";
        element->dynamicFields["name"] = name;
        element->dynamicFields["attributes"] = Value{attributes};
        auto children = std::make_shared<ListValue>();
        std::string text;
        if (!selfClosing) {
            while (true) {
                if (eof()) return fail("XML elemani kapanmadi: " + name);
                if (startsWith("</")) {
                    pos += 2;
                    std::string closing;
                    if (!parseName(closing)) return false;
                    skipSpaces();
                    if (peek() != '>') return fail("XML kapanis etiketi bozuk");
                    advance();
                    if (closing != name) return fail("XML kapanis etiketi uyusmuyor: " + closing + " != " + name);
                    break;
                }
                if (startsWith("<!--")) { pos += 4; skipUntil("-->"); continue; }
                if (startsWith("<![CDATA[")) {
                    pos += 9;
                    const std::size_t found = text.find("]]>", pos);
                    if (found == std::string::npos) return fail("XML CDATA kapanmadi");
                    text.append(this->text, pos, found - pos);
                    pos = found + 3;
                    continue;
                }
                if (startsWith("<?")) { pos += 2; skipUntil("?>"); continue; }
                if (peek() == '<') {
                    Value child;
                    if (!parseElement(child)) return false;
                    children->values.push_back(std::move(child));
                    children->active.push_back(true);
                    continue;
                }
                std::string raw;
                while (!eof() && peek() != '<') { raw.push_back(peek()); advance(); }
                text += xmlDecodeEntities(raw);
            }
        }
        element->dynamicFields["children"] = Value{children};
        element->dynamicFields["text"] = stdlibTrim(text);
        out = Value{element};
        return true;
    }
    bool parseDocument(Value& out) {
        skipMisc();
        if (eof()) return fail("XML kok elemani bulunamadi");
        if (!parseElement(out)) return false;
        skipMisc();
        if (!eof()) return fail("XML kok elemanindan sonra fazladan icerik var");
        return true;
    }
};
bool xmlIsElement(const Value& value) {
    const auto* object = std::get_if<std::shared_ptr<Object>>(&value);
    return object && *object && (*object)->type == "XmlElement";
}
std::string xmlElementName(const Value& value) {
    if (const Value* name = jsonField(value, "name")) return stringOf(*name);
    return std::string();
}
const Value* xmlElementField(const Value& value, const char* key) {
    return xmlIsElement(value) ? jsonField(value, key) : nullptr;
}
Value xmlAttribute(const Value& element, const std::string& name) {
    const Value* attributes = xmlElementField(element, "attributes");
    if (!attributes) return Value{};
    const Value* found = jsonField(*attributes, name);
    return found ? *found : Value{};
}
Value xmlFindChild(const Value& element, const std::string& name, bool deep) {
    const Value* children = xmlElementField(element, "children");
    if (!children) return Value{};
    const auto* list = std::get_if<std::shared_ptr<ListValue>>(children);
    if (!list || !*list) return Value{};
    for (std::size_t i = 0; i < (*list)->values.size(); ++i) {
        if (i < (*list)->active.size() && !(*list)->active[i]) continue;
        if (xmlElementName((*list)->values[i]) == name) return (*list)->values[i];
    }
    if (!deep) return Value{};
    for (std::size_t i = 0; i < (*list)->values.size(); ++i) {
        if (i < (*list)->active.size() && !(*list)->active[i]) continue;
        const Value found = xmlFindChild((*list)->values[i], name, true);
        if (!std::holds_alternative<std::monostate>(found)) return found;
    }
    return Value{};
}
void xmlCollect(const Value& element, const std::string& name, std::shared_ptr<ListValue>& out) {
    const Value* children = xmlElementField(element, "children");
    if (!children) return;
    const auto* list = std::get_if<std::shared_ptr<ListValue>>(children);
    if (!list || !*list) return;
    for (std::size_t i = 0; i < (*list)->values.size(); ++i) {
        if (i < (*list)->active.size() && !(*list)->active[i]) continue;
        const Value& child = (*list)->values[i];
        if (xmlElementName(child) == name) { out->values.push_back(child); out->active.push_back(true); }
        xmlCollect(child, name, out);
    }
}
void xmlEmit(const Value& element, int indent, bool pretty, std::string& out) {
    const std::string spaces = pretty ? std::string(static_cast<std::size_t>(std::max(0, indent)), ' ') : std::string();
    const std::string name = xmlElementName(element);
    out += spaces + "<" + name;
    const Value* attributes = xmlElementField(element, "attributes");
    if (attributes) {
        const auto* object = std::get_if<std::shared_ptr<Object>>(attributes);
        if (object && *object) {
            std::vector<std::string> keys;
            for (const auto& field : (*object)->dynamicFields) keys.push_back(field.first);
            std::sort(keys.begin(), keys.end());
            for (const auto& key : keys) {
                out += " " + key + "=\"" + xmlEscapeAttribute(stringOf((*object)->dynamicFields.at(key))) + "\"";
            }
        }
    }
    const Value* childrenValue = xmlElementField(element, "children");
    const auto* children = childrenValue ? std::get_if<std::shared_ptr<ListValue>>(childrenValue) : nullptr;
    std::vector<Value> activeChildren;
    if (children && *children) {
        for (std::size_t i = 0; i < (*children)->values.size(); ++i) {
            if (i < (*children)->active.size() && !(*children)->active[i]) continue;
            activeChildren.push_back((*children)->values[i]);
        }
    }
    std::string text;
    if (const Value* textValue = xmlElementField(element, "text")) text = stringOf(*textValue);
    if (activeChildren.empty() && text.empty()) { out += "/>"; if (pretty) out += "\n"; return; }
    out += ">";
    if (activeChildren.empty()) {
        out += xmlEscapeText(text) + "</" + name + ">";
        if (pretty) out += "\n";
        return;
    }
    if (pretty) out += "\n";
    for (const auto& child : activeChildren) xmlEmit(child, indent + 2, pretty, out);
    out += spaces + "</" + name + ">";
    if (pretty) out += "\n";
}
std::string xmlSerialize(const Value& element, bool pretty) {
    std::string out;
    xmlEmit(element, 0, pretty, out);
    return pretty ? stdlibTrim(out) : out;
}
void* xmlCall(const std::string& op, void** args, int argc) {
    if ((op == "parse" || op == "coz" || op == "load") && argc == 1) {
        XmlParser parser(argumentString(args, 0));
        Value result;
        if (!parser.parseDocument(result)) {
            writeStderr("PRCYTERAS Xml.parse hatasi: ");
            writeStderr(parser.error);
            writeStderr("\n");
            return nullptr;
        }
        return box(std::move(result));
    }
    if ((op == "stringify" || op == "yaz") && (argc == 1 || argc == 2)) {
        const bool pretty = argc == 2 ? truthy(argument(args, 1)) : true;
        return box(xmlSerialize(argument(args, 0), pretty));
    }
    if ((op == "get" || op == "al") && argc == 2) {
        const Value element = argument(args, 0);
        const std::string key = argumentString(args, 1);
        if (key == "name" || key == "text" || key == "attributes" || key == "children") {
            const Value* found = xmlElementField(element, key.c_str());
            return box(found ? *found : Value{});
        }
        if (!key.empty() && key[0] == '@') return box(xmlAttribute(element, key.substr(1)));
        return box(xmlFindChild(element, key, false));
    }
    if ((op == "find" || op == "ara") && argc == 2) return box(xmlFindChild(argument(args, 0), argumentString(args, 1), true));
    if ((op == "find_all" || op == "hepsini_ara") && argc == 2) {
        auto list = std::make_shared<ListValue>();
        xmlCollect(argument(args, 0), argumentString(args, 1), list);
        return box(Value{list});
    }
    if ((op == "attr" || op == "oznitelik") && argc == 2) return box(xmlAttribute(argument(args, 0), argumentString(args, 1)));
    if ((op == "attrs" || op == "oznitelikler") && argc == 1) {
        const Value* found = xmlElementField(argument(args, 0), "attributes");
        return box(found ? *found : stdlibObject("XmlAttributes"));
    }
    if ((op == "text" || op == "metin") && argc == 1) {
        const Value* found = xmlElementField(argument(args, 0), "text");
        return box(found ? *found : Value{std::string()});
    }
    if ((op == "children" || op == "cocuklar") && argc == 1) {
        const Value* found = xmlElementField(argument(args, 0), "children");
        return box(found ? *found : Value{std::make_shared<ListValue>()});
    }
    if ((op == "child" || op == "cocuk") && argc == 2) return box(xmlFindChild(argument(args, 0), argumentString(args, 1), false));
    if ((op == "name" || op == "ad") && argc == 1) return box(xmlElementName(argument(args, 0)));
    if ((op == "has" || op == "var_mi") && argc == 2) {
        const Value element = argument(args, 0);
        const std::string key = argumentString(args, 1);
        if (key == "name" || key == "text" || key == "attributes" || key == "children") return box(true);
        if (!key.empty() && key[0] == '@') return box(!std::holds_alternative<std::monostate>(xmlAttribute(element, key.substr(1))));
        return box(!std::holds_alternative<std::monostate>(xmlFindChild(element, key, false)));
    }
    if ((op == "count" || op == "sayi") && argc == 1) {
        const Value* found = xmlElementField(argument(args, 0), "children");
        const auto* list = found ? std::get_if<std::shared_ptr<ListValue>>(found) : nullptr;
        std::int64_t count = 0;
        if (list && *list) for (const bool active : (*list)->active) if (active) ++count;
        return box(count);
    }
    if ((op == "escape" || op == "kacis") && argc == 1) return box(xmlEscapeText(argumentString(args, 0)));
    if ((op == "unescape" || op == "kacis_coz") && argc == 1) return box(xmlDecodeEntities(argumentString(args, 0)));
    if ((op == "type" || op == "tur") && argc == 1) return box(std::string("element"));
    return jsonCall(op, args, argc);
}

// ben yaptım opencode - 2026-11-14 - dotenv ayristirici
struct DotenvParser {
    std::string text;
    std::size_t pos = 0;
    int line = 1;
    std::string error;
    explicit DotenvParser(std::string source) : text(std::move(source)) {}
    bool fail(const std::string& message) { if (error.empty()) error = message + " (satir " + std::to_string(line) + ")"; return false; }
    bool eof() const { return pos >= text.size(); }
    char peek(std::size_t offset = 0) const { return pos + offset < text.size() ? text[pos + offset] : '\0'; }
    void advance() { if (pos >= text.size()) return; if (text[pos] == '\n') ++line; ++pos; }
    void skipSpaces() { while (!eof() && (peek() == ' ' || peek() == '\t' || peek() == '\r')) advance(); }
    bool atLineEnd() const { return eof() || peek() == '\n'; }
    void skipToLineEnd() { while (!eof() && peek() != '\n') advance(); if (!eof()) advance(); }
    bool parseKey(std::string& key) {
        key.clear();
        while (!eof()) {
            const char c = peek();
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-') {
                key.push_back(c);
                advance();
            } else break;
        }
        if (key.empty()) return fail("dotenv anahtari bekleniyordu");
        return true;
    }
    bool parseDoubleQuoted(std::string& out) {
        advance();
        while (true) {
            if (eof()) return fail("dotenv cift tirnak kapanmadi");
            const char c = peek();
            if (c == '"') { advance(); return true; }
            if (c == '\\') {
                advance();
                if (eof()) return fail("dotenv kacis eksik");
                const char escaped = peek();
                advance();
                switch (escaped) {
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case '"': out.push_back('"'); break;
                    case '\'': out.push_back('\''); break;
                    case '\\': out.push_back('\\'); break;
                    case '$': out.push_back('$'); break;
                    default: out.push_back(escaped); break;
                }
                continue;
            }
            if (c == '\n') { out.push_back('\n'); advance(); continue; }
            out.push_back(c);
            advance();
        }
    }
    bool parseSingleQuoted(std::string& out) {
        advance();
        while (true) {
            if (eof()) return fail("dotenv tek tirnak kapanmadi");
            const char c = peek();
            if (c == '\'') { advance(); return true; }
            out.push_back(c);
            advance();
        }
    }
    bool parseUnquoted(std::string& out) {
        while (!eof() && peek() != '\n') {
            const char c = peek();
            if ((c == ' ' || c == '\t' || c == '\r') && peek(1) == '#') break;
            if (c == '#' && out.empty()) break;
            out.push_back(c);
            advance();
        }
        out = stdlibTrim(out);
        return true;
    }
    bool parseDocument(Value& result) {
        auto object = std::make_shared<Object>();
        object->type = "Dotenv";
        while (!eof()) {
            skipSpaces();
            if (atLineEnd()) { if (!eof()) advance(); continue; }
            if (peek() == '#') { skipToLineEnd(); continue; }
            if (text.compare(pos, 7, "export ") == 0) {
                pos += 7;
                skipSpaces();
            }
            std::string key;
            if (!parseKey(key)) return false;
            skipSpaces();
            if (peek() != '=') return fail("dotenv '=' bekleniyordu: " + key);
            advance();
            skipSpaces();
            std::string value;
            if (peek() == '"') { if (!parseDoubleQuoted(value)) return false; }
            else if (peek() == '\'') { if (!parseSingleQuoted(value)) return false; }
            else { if (!parseUnquoted(value)) return false; }
            skipSpaces();
            if (!atLineEnd()) {
                if (peek() == '#') skipToLineEnd();
                else return fail("dotenv satir sonunda beklenmeyen icerik: " + key);
            }
            if (!eof()) advance();
            object->dynamicFields[key] = std::move(value);
        }
        result = Value{object};
        return true;
    }
};
std::string dotenvEscape(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out.push_back(c);
    }
    out += "\"";
    return out;
}
bool dotenvNeedsQuote(const std::string& text) {
    if (text.empty()) return true;
    if (text.front() == ' ' || text.back() == ' ') return true;
    for (const char c : text) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#' || c == '"' || c == '\'' || c == '\\' || c == '$') return true;
    }
    return false;
}
std::string dotenvSerialize(const Value& value) {
    const auto* object = std::get_if<std::shared_ptr<Object>>(&value);
    if (!object || !*object) return std::string();
    std::vector<std::string> keys;
    for (const auto& field : (*object)->dynamicFields) keys.push_back(field.first);
    std::sort(keys.begin(), keys.end());
    std::string out;
    for (const auto& key : keys) {
        const std::string text = stringOf((*object)->dynamicFields.at(key));
        out += key + "=" + (dotenvNeedsQuote(text) ? dotenvEscape(text) : text) + "\n";
    }
    return stdlibTrim(out);
}
std::string dotenvExpandText(const std::string& input, const std::shared_ptr<Object>& values, int depth);
std::string dotenvLookup(const std::string& name, const std::shared_ptr<Object>& values, int depth) {
    if (!values) return std::string();
    const auto found = values->dynamicFields.find(name);
    if (found == values->dynamicFields.end()) return std::string();
    return dotenvExpandText(stringOf(found->second), values, depth + 1);
}
std::string dotenvExpandText(const std::string& input, const std::shared_ptr<Object>& values, int depth) {
    if (depth > 16) return input;
    std::string out;
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] != '$') { out.push_back(input[i]); continue; }
        if (i + 1 < input.size() && input[i + 1] == '{') {
            const std::size_t end = input.find('}', i + 2);
            if (end == std::string::npos) { out.push_back('$'); continue; }
            out += dotenvLookup(input.substr(i + 2, end - i - 2), values, depth);
            i = end;
            continue;
        }
        std::size_t end = i + 1;
        if (end < input.size() && (std::isalpha(static_cast<unsigned char>(input[end])) || input[end] == '_')) {
            ++end;
            while (end < input.size() && (std::isalnum(static_cast<unsigned char>(input[end])) || input[end] == '_')) ++end;
            out += dotenvLookup(input.substr(i + 1, end - i - 1), values, depth);
            i = end - 1;
        } else {
            out.push_back('$');
        }
    }
    return out;
}
void* dotenvCall(const std::string& op, void** args, int argc) {
    if ((op == "parse" || op == "coz") && argc == 1) {
        DotenvParser parser(argumentString(args, 0));
        Value result;
        if (!parser.parseDocument(result)) {
            writeStderr("PRCYTERAS Dotenv.parse hatasi: ");
            writeStderr(parser.error);
            writeStderr("\n");
            return nullptr;
        }
        return box(std::move(result));
    }
    if ((op == "read" || op == "oku" || op == "load" || op == "yukle") && argc == 1) {
        std::ifstream input(fs::path(argumentString(args, 0)), std::ios::binary);
        if (!input) {
            writeStderr("PRCYTERAS Dotenv.read: dosya okunamadi.\n");
            return nullptr;
        }
        std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        DotenvParser parser(std::move(text));
        Value result;
        if (!parser.parseDocument(result)) {
            writeStderr("PRCYTERAS Dotenv.read hatasi: ");
            writeStderr(parser.error);
            writeStderr("\n");
            return nullptr;
        }
        return box(std::move(result));
    }
    if ((op == "stringify" || op == "yaz") && argc == 1) return box(dotenvSerialize(argument(args, 0)));
    if ((op == "expand" || op == "genislet") && (argc == 1 || argc == 2)) {
        const Value container = argument(args, 0);
        const auto* object = std::get_if<std::shared_ptr<Object>>(&container);
        if (!object || !*object) return nullptr;
        std::shared_ptr<Object> environment = *object;
        if (argc == 2) {
            const Value extra = argument(args, 1);
            const auto* extraObject = std::get_if<std::shared_ptr<Object>>(&extra);
            if (extraObject && *extraObject) {
                environment = std::make_shared<Object>();
                environment->type = "Dotenv";
                for (const auto& field : (*object)->dynamicFields) environment->dynamicFields[field.first] = field.second;
                for (const auto& field : (*extraObject)->dynamicFields) environment->dynamicFields[field.first] = field.second;
            }
        }
        auto result = std::make_shared<Object>();
        result->type = "Dotenv";
        for (const auto& field : environment->dynamicFields) result->dynamicFields[field.first] = dotenvExpandText(stringOf(field.second), environment, 0);
        return box(Value{result});
    }
    if ((op == "expand_text" || op == "metin_genislet") && argc == 2) {
        const Value container = argument(args, 1);
        const auto* object = std::get_if<std::shared_ptr<Object>>(&container);
        if (!object || !*object) return box(argumentString(args, 0));
        return box(dotenvExpandText(argumentString(args, 0), *object, 0));
    }
    return jsonCall(op, args, argc);
}

// ben yaptım opencode - 2026-11-14 - UUID uretimi ve dogrulama
struct UuidSha1 {
    std::uint32_t state[5];
    std::uint64_t total = 0;
    unsigned char buffer[64];
    std::size_t used = 0;
    UuidSha1() : state{0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u} {}
    static std::uint32_t rotate(std::uint32_t value, int bits) { return (value << bits) | (value >> (32 - bits)); }
    void transform(const unsigned char* block) {
        std::uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                   (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                   static_cast<std::uint32_t>(block[i * 4 + 3]);
        }
        for (int i = 16; i < 80; ++i) w[i] = rotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
        for (int i = 0; i < 80; ++i) {
            std::uint32_t f = 0, k = 0;
            if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
            const std::uint32_t temp = rotate(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotate(b, 30); b = a; a = temp;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
    }
    void update(const unsigned char* data, std::size_t length) {
        total += length;
        while (length > 0) {
            const std::size_t take = std::min<std::size_t>(64 - used, length);
            std::memcpy(buffer + used, data, take);
            used += take;
            data += take;
            length -= take;
            if (used == 64) { transform(buffer); used = 0; }
        }
    }
    void final(unsigned char out[20]) {
        const std::uint64_t bits = total * 8;
        const unsigned char pad = 0x80;
        update(&pad, 1);
        const unsigned char zero = 0;
        while (used != 56) update(&zero, 1);
        unsigned char lengthBytes[8];
        for (int i = 0; i < 8; ++i) lengthBytes[i] = static_cast<unsigned char>((bits >> (56 - i * 8)) & 0xFF);
        update(lengthBytes, 8);
        for (int i = 0; i < 5; ++i) {
            out[i * 4] = static_cast<unsigned char>((state[i] >> 24) & 0xFF);
            out[i * 4 + 1] = static_cast<unsigned char>((state[i] >> 16) & 0xFF);
            out[i * 4 + 2] = static_cast<unsigned char>((state[i] >> 8) & 0xFF);
            out[i * 4 + 3] = static_cast<unsigned char>(state[i] & 0xFF);
        }
    }
};
using UuidBytes = std::array<unsigned char, 16>;
std::string uuidFormat(const UuidBytes& bytes, bool upper) {
    static const char lowerHex[] = "0123456789abcdef";
    static const char upperHex[] = "0123456789ABCDEF";
    const char* hex = upper ? upperHex : lowerHex;
    std::string out;
    out.reserve(36);
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out.push_back('-');
        out.push_back(hex[(bytes[i] >> 4) & 0xF]);
        out.push_back(hex[bytes[i] & 0xF]);
    }
    return out;
}
int uuidHexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
bool uuidParse(const std::string& input, UuidBytes& out) {
    std::string text = stdlibTrim(input);
    if (text.rfind("urn:uuid:", 0) == 0) text = text.substr(9);
    if (text.size() >= 2 && text.front() == '{' && text.back() == '}') text = text.substr(1, text.size() - 2);
    std::string compact;
    compact.reserve(32);
    for (const char c : text) {
        if (c == '-') continue;
        if (uuidHexValue(c) < 0) return false;
        compact.push_back(c);
    }
    if (compact.size() != 32) return false;
    for (int i = 0; i < 16; ++i) out[i] = static_cast<unsigned char>((uuidHexValue(compact[i * 2]) << 4) | uuidHexValue(compact[i * 2 + 1]));
    return true;
}
std::mt19937_64& uuidEngine() {
    static thread_local std::mt19937_64 engine = []() {
        std::random_device device;
        const auto now = static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        const std::uint64_t seed = (static_cast<std::uint64_t>(device()) << 32) ^ static_cast<std::uint64_t>(device()) ^ now;
        return std::mt19937_64(seed);
    }();
    return engine;
}
void uuidFillRandom(UuidBytes& bytes, std::size_t start = 0) {
    auto& engine = uuidEngine();
    for (std::size_t i = start; i < bytes.size(); i += 8) {
        const std::uint64_t value = engine();
        for (std::size_t k = 0; k < 8 && i + k < bytes.size(); ++k) bytes[i + k] = static_cast<unsigned char>((value >> (k * 8)) & 0xFF);
    }
}
UuidBytes uuidV4() {
    UuidBytes bytes{};
    uuidFillRandom(bytes);
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);
    return bytes;
}
UuidBytes uuidV7() {
    UuidBytes bytes{};
    uuidFillRandom(bytes);
    const std::uint64_t millis = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    bytes[0] = static_cast<unsigned char>((millis >> 40) & 0xFF);
    bytes[1] = static_cast<unsigned char>((millis >> 32) & 0xFF);
    bytes[2] = static_cast<unsigned char>((millis >> 24) & 0xFF);
    bytes[3] = static_cast<unsigned char>((millis >> 16) & 0xFF);
    bytes[4] = static_cast<unsigned char>((millis >> 8) & 0xFF);
    bytes[5] = static_cast<unsigned char>(millis & 0xFF);
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x70);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);
    return bytes;
}
UuidBytes uuidV5(const UuidBytes& nameSpace, const std::string& name) {
    UuidSha1 sha;
    sha.update(nameSpace.data(), nameSpace.size());
    sha.update(reinterpret_cast<const unsigned char*>(name.data()), name.size());
    unsigned char digest[20];
    sha.final(digest);
    UuidBytes bytes{};
    std::memcpy(bytes.data(), digest, 16);
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x50);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80);
    return bytes;
}
bool uuidNamespace(const std::string& input, UuidBytes& out) {
    const std::string lower = stdlibLower(stdlibTrim(input));
    if (lower == "dns") { uuidParse("6ba7b810-9dad-11d1-80b4-00c04fd430c8", out); return true; }
    if (lower == "url") { uuidParse("6ba7b811-9dad-11d1-80b4-00c04fd430c8", out); return true; }
    if (lower == "oid") { uuidParse("6ba7b812-9dad-11d1-80b4-00c04fd430c8", out); return true; }
    if (lower == "x500") { uuidParse("6ba7b814-9dad-11d1-80b4-00c04fd430c8", out); return true; }
    return uuidParse(input, out);
}
void* uuidCall(const std::string& op, void** args, int argc) {
    if ((op == "v4" || op == "new" || op == "yeni" || op == "generate" || op == "uret") && argc == 0) return box(uuidFormat(uuidV4(), false));
    if ((op == "v7" || op == "time" || op == "zaman") && argc == 0) return box(uuidFormat(uuidV7(), false));
    if ((op == "v5" || op == "name") && argc == 2) {
        UuidBytes nameSpace;
        if (!uuidNamespace(argumentString(args, 0), nameSpace)) {
            writeStderr("PRCYTERAS Uuid.v5: gecersiz namespace.\n");
            return nullptr;
        }
        return box(uuidFormat(uuidV5(nameSpace, argumentString(args, 1)), false));
    }
    if ((op == "nil" || op == "bos") && argc == 0) return box(std::string("00000000-0000-0000-0000-000000000000"));
    if ((op == "is_valid" || op == "gecerli_mi") && argc == 1) {
        UuidBytes bytes{};
        return box(uuidParse(argumentString(args, 0), bytes));
    }
    if ((op == "parse" || op == "coz" || op == "normalize") && argc == 1) {
        UuidBytes bytes{};
        if (!uuidParse(argumentString(args, 0), bytes)) {
            writeStderr("PRCYTERAS Uuid.parse: gecersiz UUID.\n");
            return nullptr;
        }
        return box(uuidFormat(bytes, false));
    }
    if ((op == "version" || op == "surum") && argc == 1) {
        UuidBytes bytes{};
        if (!uuidParse(argumentString(args, 0), bytes)) return box(std::int64_t{0});
        return box(static_cast<std::int64_t>((bytes[6] >> 4) & 0x0F));
    }
    if ((op == "variant" || op == "varyant") && argc == 1) {
        UuidBytes bytes{};
        if (!uuidParse(argumentString(args, 0), bytes)) return box(std::string("invalid"));
        const unsigned char b = bytes[8];
        if ((b & 0x80) == 0) return box(std::string("ncs"));
        if ((b & 0xC0) == 0x80) return box(std::string("rfc4122"));
        if ((b & 0xE0) == 0xC0) return box(std::string("microsoft"));
        return box(std::string("future"));
    }
    if ((op == "to_upper" || op == "buyuk") && argc == 1) {
        UuidBytes bytes{};
        if (!uuidParse(argumentString(args, 0), bytes)) return nullptr;
        return box(uuidFormat(bytes, true));
    }
    if ((op == "to_lower" || op == "kucuk") && argc == 1) {
        UuidBytes bytes{};
        if (!uuidParse(argumentString(args, 0), bytes)) return nullptr;
        return box(uuidFormat(bytes, false));
    }
    if ((op == "equals" || op == "esit") && argc == 2) {
        UuidBytes left{}, right{};
        return box(uuidParse(argumentString(args, 0), left) && uuidParse(argumentString(args, 1), right) && left == right);
    }
    writeStderr("PRCYTERAS: Uuid.");
    writeStderr(op);
    writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");
    return nullptr;
}

// ben yaptım opencode - 2026-11-14 - logger
int loggerLevelValue(const std::string& input) {
    const std::string level = stdlibLower(stdlibTrim(input));
    if (level == "trace" || level == "0") return 0;
    if (level == "debug" || level == "1") return 1;
    if (level == "info" || level == "information" || level == "2") return 2;
    if (level == "warn" || level == "warning" || level == "3") return 3;
    if (level == "error" || level == "err" || level == "4") return 4;
    if (level == "fatal" || level == "critical" || level == "crit" || level == "5") return 5;
    if (level == "off" || level == "none" || level == "silent" || level == "6") return 6;
    return -1;
}
const char* loggerLevelName(int level) {
    switch (level) {
        case 0: return "trace";
        case 1: return "debug";
        case 2: return "info";
        case 3: return "warn";
        case 4: return "error";
        case 5: return "fatal";
        default: return "off";
    }
}
std::shared_ptr<Object> loggerObject(const Value& value) {
    const auto* object = std::get_if<std::shared_ptr<Object>>(&value);
    if (!object || !*object || (*object)->type != "Logger") return nullptr;
    return *object;
}
std::string loggerFieldString(const std::shared_ptr<Object>& logger, const std::string& key, const std::string& fallback) {
    if (!logger) return fallback;
    const auto found = logger->dynamicFields.find(key);
    if (found == logger->dynamicFields.end()) return fallback;
    return stringOf(found->second);
}
std::string loggerTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &utc);
    std::ostringstream stream;
    stream << buffer << "." << std::setw(3) << std::setfill('0') << millis.count() << "Z";
    return stream.str();
}
std::string loggerValueText(const Value& value) {
    if (std::holds_alternative<std::shared_ptr<Object>>(value) || std::holds_alternative<std::shared_ptr<ListValue>>(value)) {
        std::string out;
        jsonSerialize(value, out, false, 0, 0);
        return out;
    }
    return stringOf(value);
}
std::string loggerTextLine(const std::shared_ptr<Object>& logger, const std::string& level, const std::string& message) {
    std::string out = loggerTimestamp() + " [" + level + "]";
    const std::string name = loggerFieldString(logger, "name", "");
    if (!name.empty()) out += " " + name;
    out += " " + message;
    const Value* fields = jsonField(Value{logger}, "fields");
    const auto* fieldObject = fields ? std::get_if<std::shared_ptr<Object>>(fields) : nullptr;
    if (fieldObject && *fieldObject) {
        std::vector<std::string> keys;
        for (const auto& field : (*fieldObject)->dynamicFields) keys.push_back(field.first);
        std::sort(keys.begin(), keys.end());
        for (const auto& key : keys) {
            std::string rendered = loggerValueText((*fieldObject)->dynamicFields.at(key));
            if (rendered.empty() || rendered.find_first_of(" \t\n\"=") != std::string::npos) {
                std::string quoted;
                jsonSerialize(Value{rendered}, quoted, false, 0, 0);
                rendered = quoted;
            }
            out += " " + key + "=" + rendered;
        }
    }
    return out;
}
std::string loggerJsonLine(const std::shared_ptr<Object>& logger, const std::string& level, const std::string& message) {
    auto record = std::make_shared<Object>();
    record->type = "LogRecord";
    record->dynamicFields["time"] = loggerTimestamp();
    record->dynamicFields["level"] = level;
    record->dynamicFields["logger"] = loggerFieldString(logger, "name", "");
    record->dynamicFields["message"] = message;
    const Value* fields = jsonField(Value{logger}, "fields");
    record->dynamicFields["fields"] = fields ? *fields : stdlibObject("JsonObject");
    std::string out;
    jsonSerialize(Value{record}, out, false, 0, 0);
    return out;
}
void loggerWriteLine(const std::shared_ptr<Object>& logger, int level, const std::string& message) {
    const std::string format = loggerFieldString(logger, "format", "text");
    const std::string line = (format == "json" ? loggerJsonLine(logger, loggerLevelName(level), message)
                                               : loggerTextLine(logger, loggerLevelName(level), message)) + "\n";
    const std::string path = loggerFieldString(logger, "file", "");
    if (!path.empty()) {
        std::ofstream output(fs::path(path), std::ios::binary | std::ios::app);
        if (output) output.write(line.data(), static_cast<std::streamsize>(line.size()));
        return;
    }
    if (level >= 3) writeStderr(line);
    else std::fwrite(line.data(), 1, line.size(), stdout);
    std::fflush(level >= 3 ? stderr : stdout);
}
Value loggerClone(const std::shared_ptr<Object>& logger) {
    auto copy = std::make_shared<Object>();
    copy->type = "Logger";
    if (logger) for (const auto& field : logger->dynamicFields) copy->dynamicFields[field.first] = field.second;
    return Value{copy};
}
void* loggerCall(const std::string& op, void** args, int argc) {
    if ((op == "create" || op == "yeni" || op == "new") && (argc == 0 || argc == 1)) {
        auto logger = std::make_shared<Object>();
        logger->type = "Logger";
        logger->dynamicFields["name"] = argc == 1 ? argumentString(args, 0) : std::string("app");
        logger->dynamicFields["level"] = std::string("info");
        logger->dynamicFields["file"] = std::string();
        logger->dynamicFields["format"] = std::string("text");
        logger->dynamicFields["fields"] = stdlibObject("JsonObject");
        return box(Value{logger});
    }
    if ((op == "child" || op == "cocuk") && argc == 2) {
        auto logger = loggerObject(argument(args, 0));
        if (!logger) return nullptr;
        Value copy = loggerClone(logger);
        auto* object = std::get_if<std::shared_ptr<Object>>(&copy);
        const std::string parent = loggerFieldString(logger, "name", "");
        const std::string childName = argumentString(args, 1);
        (*object)->dynamicFields["name"] = parent.empty() ? childName : parent + "." + childName;
        return box(std::move(copy));
    }
    if ((op == "with_name" || op == "ad_ver") && argc == 2) {
        auto logger = loggerObject(argument(args, 0));
        if (!logger) return nullptr;
        Value copy = loggerClone(logger);
        std::get_if<std::shared_ptr<Object>>(&copy)->get()->dynamicFields["name"] = argumentString(args, 1);
        return box(std::move(copy));
    }
    if ((op == "with_field" || op == "alan_ekle") && argc == 3) {
        auto logger = loggerObject(argument(args, 0));
        if (!logger) return nullptr;
        Value copy = loggerClone(logger);
        auto* object = std::get_if<std::shared_ptr<Object>>(&copy);
        Value* fields = nullptr;
        auto found = (*object)->dynamicFields.find("fields");
        if (found != (*object)->dynamicFields.end()) fields = &found->second;
        if (!fields) { (*object)->dynamicFields["fields"] = stdlibObject("JsonObject"); fields = &(*object)->dynamicFields["fields"]; }
        const auto* fieldObject = std::get_if<std::shared_ptr<Object>>(fields);
        if (!fieldObject || !*fieldObject) return nullptr;
        (*fieldObject)->dynamicFields[argumentString(args, 1)] = argument(args, 2);
        return box(std::move(copy));
    }
    if ((op == "set_level" || op == "seviye_ayarla") && argc == 2) {
        auto logger = loggerObject(argument(args, 0));
        const int level = loggerLevelValue(argumentString(args, 1));
        if (!logger || level < 0) {
            writeStderr("PRCYTERAS Logger.set_level: gecersiz seviye.\n");
            return nullptr;
        }
        Value copy = loggerClone(logger);
        std::get_if<std::shared_ptr<Object>>(&copy)->get()->dynamicFields["level"] = std::string(loggerLevelName(level));
        return box(std::move(copy));
    }
    if ((op == "get_level" || op == "seviye") && argc == 1) {
        auto logger = loggerObject(argument(args, 0));
        if (!logger) return nullptr;
        return box(loggerFieldString(logger, "level", "info"));
    }
    if ((op == "is_enabled" || op == "etkin_mi") && argc == 2) {
        auto logger = loggerObject(argument(args, 0));
        const int requested = loggerLevelValue(argumentString(args, 1));
        if (!logger || requested < 0) return box(false);
        const int configured = loggerLevelValue(loggerFieldString(logger, "level", "info"));
        return box(configured >= 0 && requested >= configured && configured < 6);
    }
    if ((op == "set_file" || op == "dosya_ayarla") && argc == 2) {
        auto logger = loggerObject(argument(args, 0));
        if (!logger) return nullptr;
        Value copy = loggerClone(logger);
        std::get_if<std::shared_ptr<Object>>(&copy)->get()->dynamicFields["file"] = argumentString(args, 1);
        return box(std::move(copy));
    }
    if ((op == "set_format" || op == "bicim_ayarla") && argc == 2) {
        auto logger = loggerObject(argument(args, 0));
        if (!logger) return nullptr;
        const std::string format = stdlibLower(argumentString(args, 1));
        if (format != "text" && format != "json") {
            writeStderr("PRCYTERAS Logger.set_format: 'text' veya 'json' bekleniyordu.\n");
            return nullptr;
        }
        Value copy = loggerClone(logger);
        std::get_if<std::shared_ptr<Object>>(&copy)->get()->dynamicFields["format"] = format;
        return box(std::move(copy));
    }
    if ((op == "name" || op == "ad") && argc == 1) {
        auto logger = loggerObject(argument(args, 0));
        return logger ? box(loggerFieldString(logger, "name", "")) : nullptr;
    }
    if ((op == "file" || op == "dosya") && argc == 1) {
        auto logger = loggerObject(argument(args, 0));
        return logger ? box(loggerFieldString(logger, "file", "")) : nullptr;
    }
    if ((op == "format" || op == "bicim") && argc == 1) {
        auto logger = loggerObject(argument(args, 0));
        return logger ? box(loggerFieldString(logger, "format", "text")) : nullptr;
    }
    if ((op == "fields" || op == "alanlar") && argc == 1) {
        auto logger = loggerObject(argument(args, 0));
        if (!logger) return nullptr;
        const Value* fields = jsonField(Value{logger}, "fields");
        return box(fields ? *fields : stdlibObject("JsonObject"));
    }
    if ((op == "log" || op == "yaz") && argc == 3) {
        auto logger = loggerObject(argument(args, 0));
        const int level = loggerLevelValue(argumentString(args, 1));
        if (!logger || level < 0) {
            writeStderr("PRCYTERAS Logger.log: gecersiz logger veya seviye.\n");
            return box(false);
        }
        const int configured = loggerLevelValue(loggerFieldString(logger, "level", "info"));
        if (configured < 0 || configured == 6 || level < configured) return box(false);
        loggerWriteLine(logger, level, loggerValueText(argument(args, 2)));
        return box(true);
    }
    if ((op == "trace" || op == "debug" || op == "info" || op == "warn" || op == "warning" ||
         op == "error" || op == "fatal" || op == "critical") && argc == 2) {
        auto logger = loggerObject(argument(args, 0));
        const int level = op == "trace" ? 0 : op == "debug" ? 1 : op == "info" ? 2 :
                          (op == "warn" || op == "warning") ? 3 : (op == "error") ? 4 : 5;
        if (!logger) return box(false);
        const int configured = loggerLevelValue(loggerFieldString(logger, "level", "info"));
        if (configured < 0 || configured == 6 || level < configured) return box(false);
        loggerWriteLine(logger, level, loggerValueText(argument(args, 1)));
        return box(true);
    }
    if ((op == "flush" || op == "bosalt") && argc == 1) {
        std::fflush(stdout);
        std::fflush(stderr);
        return box(true);
    }
    writeStderr("PRCYTERAS: Logger.");
    writeStderr(op);
    writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");
    return nullptr;
}

// ben yaptım opencode - 2026-11-14 - istatistik
std::vector<double> statsList(const Value& value) {
    std::vector<double> out;
    if (const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value)) {
        if (*list) {
            for (std::size_t i = 0; i < (*list)->values.size(); ++i) {
                if (i < (*list)->active.size() && !(*list)->active[i]) continue;
                out.push_back(numberOf((*list)->values[i]));
            }
            return out;
        }
    }
    if (!std::holds_alternative<std::monostate>(value)) out.push_back(numberOf(value));
    return out;
}
std::vector<double> statsValues(void** args, int argc) {
    std::vector<double> out;
    if (argc == 1) return statsList(argument(args, 0));
    out.reserve(static_cast<std::size_t>(std::max(0, argc)));
    for (int i = 0; i < argc; ++i) out.push_back(numberOf(argument(args, i)));
    return out;
}
double statsSum(const std::vector<double>& values) {
    double total = 0.0;
    for (const double value : values) total += value;
    return total;
}
double statsMean(const std::vector<double>& values) {
    if (values.empty()) return std::numeric_limits<double>::quiet_NaN();
    return statsSum(values) / static_cast<double>(values.size());
}
double statsVariance(const std::vector<double>& values, bool sample) {
    if (values.empty() || (sample && values.size() < 2)) return std::numeric_limits<double>::quiet_NaN();
    const double mean = statsMean(values);
    double total = 0.0;
    for (const double value : values) { const double diff = value - mean; total += diff * diff; }
    return total / static_cast<double>(sample ? values.size() - 1 : values.size());
}
double statsMedian(std::vector<double> values) {
    if (values.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    if (values.size() % 2 == 1) return values[middle];
    return (values[middle - 1] + values[middle]) / 2.0;
}
double statsPercentile(std::vector<double> values, double percent) {
    if (values.empty()) return std::numeric_limits<double>::quiet_NaN();
    if (!std::isfinite(percent)) return std::numeric_limits<double>::quiet_NaN();
    if (percent > 1.0) percent /= 100.0;
    if (percent < 0.0) percent = 0.0;
    if (percent > 1.0) percent = 1.0;
    std::sort(values.begin(), values.end());
    const double position = percent * static_cast<double>(values.size() - 1);
    const std::size_t lower = static_cast<std::size_t>(std::floor(position));
    const std::size_t upper = static_cast<std::size_t>(std::ceil(position));
    if (lower == upper) return values[lower];
    const double fraction = position - static_cast<double>(lower);
    return values[lower] + (values[upper] - values[lower]) * fraction;
}
Value statsArray(const std::vector<double>& values) {
    auto list = std::make_shared<ListValue>();
    list->values.reserve(values.size());
    for (const double value : values) { list->values.emplace_back(value); list->active.push_back(true); }
    return Value{list};
}
Value statsSummary(const std::vector<double>& values) {
    auto object = std::make_shared<Object>();
    object->type = "StatsSummary";
    object->dynamicFields["count"] = static_cast<std::int64_t>(values.size());
    object->dynamicFields["sum"] = statsSum(values);
    object->dynamicFields["mean"] = statsMean(values);
    object->dynamicFields["min"] = values.empty() ? std::numeric_limits<double>::quiet_NaN() : *std::min_element(values.begin(), values.end());
    object->dynamicFields["max"] = values.empty() ? std::numeric_limits<double>::quiet_NaN() : *std::max_element(values.begin(), values.end());
    object->dynamicFields["median"] = statsMedian(values);
    object->dynamicFields["variance"] = statsVariance(values, false);
    const double variance = statsVariance(values, false);
    object->dynamicFields["stddev"] = std::isnan(variance) ? variance : std::sqrt(variance);
    return Value{object};
}
void* statsCall(const std::string& op, void** args, int argc) {
    if ((op == "sum" || op == "toplam") && argc >= 0) return box(statsSum(statsValues(args, argc)));
    if ((op == "mean" || op == "average" || op == "ortalama") && argc >= 0) return box(statsMean(statsValues(args, argc)));
    if ((op == "count" || op == "adet" || op == "size") && argc >= 0) return box(static_cast<std::int64_t>(statsValues(args, argc).size()));
    if ((op == "min" || op == "en_kucuk") && argc >= 0) {
        const auto values = statsValues(args, argc);
        return box(values.empty() ? std::numeric_limits<double>::quiet_NaN() : *std::min_element(values.begin(), values.end()));
    }
    if ((op == "max" || op == "en_buyuk") && argc >= 0) {
        const auto values = statsValues(args, argc);
        return box(values.empty() ? std::numeric_limits<double>::quiet_NaN() : *std::max_element(values.begin(), values.end()));
    }
    if ((op == "range" || op == "aralik") && argc >= 0) {
        const auto values = statsValues(args, argc);
        if (values.empty()) return box(std::numeric_limits<double>::quiet_NaN());
        return box(*std::max_element(values.begin(), values.end()) - *std::min_element(values.begin(), values.end()));
    }
    if ((op == "product" || op == "carpim") && argc >= 0) {
        const auto values = statsValues(args, argc);
        if (values.empty()) return box(0.0);
        double product = 1.0;
        for (const double value : values) product *= value;
        return box(product);
    }
    if ((op == "median" || op == "medyan") && argc >= 0) return box(statsMedian(statsValues(args, argc)));
    if ((op == "mode" || op == "mod") && argc >= 0) {
        const auto values = statsValues(args, argc);
        if (values.empty()) return box(std::numeric_limits<double>::quiet_NaN());
        std::unordered_map<double, std::size_t> counts;
        double best = values.front();
        std::size_t bestCount = 0;
        for (const double value : values) {
            const std::size_t count = ++counts[value];
            if (count > bestCount) { bestCount = count; best = value; }
        }
        return box(best);
    }
    if ((op == "variance" || op == "varyans") && argc >= 0) return box(statsVariance(statsValues(args, argc), false));
    if ((op == "sample_variance" || op == "ornek_varyans") && argc >= 0) return box(statsVariance(statsValues(args, argc), true));
    if ((op == "stddev" || op == "std" || op == "standart_sapma") && argc >= 0) {
        const double variance = statsVariance(statsValues(args, argc), false);
        return box(std::isnan(variance) ? variance : std::sqrt(variance));
    }
    if ((op == "sample_stddev" || op == "ornek_standart_sapma") && argc >= 0) {
        const double variance = statsVariance(statsValues(args, argc), true);
        return box(std::isnan(variance) ? variance : std::sqrt(variance));
    }
    if ((op == "percentile" || op == "yuzdelik" || op == "quantile") && argc >= 2) {
        std::vector<double> values;
        if (argc == 2) values = statsList(argument(args, 0));
        else for (int i = 0; i < argc - 1; ++i) values.push_back(numberOf(argument(args, i)));
        const double percent = numberOf(argument(args, argc - 1));
        return box(statsPercentile(std::move(values), percent));
    }
    if ((op == "zscore" || op == "z_puani") && argc == 2) {
        const double value = numberOf(argument(args, 0));
        const auto values = statsList(argument(args, 1));
        const double mean = statsMean(values);
        const double variance = statsVariance(values, false);
        const double deviation = std::isnan(variance) ? 0.0 : std::sqrt(variance);
        return box(deviation == 0.0 ? 0.0 : (value - mean) / deviation);
    }
    if ((op == "zscores" || op == "z_puanlari" || op == "normalize" || op == "normalize_et") && argc == 1) {
        const auto values = statsList(argument(args, 0));
        const double mean = statsMean(values);
        const double variance = statsVariance(values, false);
        const double deviation = std::isnan(variance) ? 0.0 : std::sqrt(variance);
        std::vector<double> result;
        result.reserve(values.size());
        for (const double value : values) result.push_back(deviation == 0.0 ? 0.0 : (value - mean) / deviation);
        return box(statsArray(result));
    }
    if ((op == "covariance" || op == "kovaryans" || op == "sample_covariance") && argc == 2) {
        const auto left = statsList(argument(args, 0));
        const auto right = statsList(argument(args, 1));
        const std::size_t count = std::min(left.size(), right.size());
        if (count == 0) return box(std::numeric_limits<double>::quiet_NaN());
        const bool sample = op == "sample_covariance";
        if (sample && count < 2) return box(std::numeric_limits<double>::quiet_NaN());
        const double leftMean = statsMean(std::vector<double>(left.begin(), left.begin() + static_cast<std::ptrdiff_t>(count)));
        const double rightMean = statsMean(std::vector<double>(right.begin(), right.begin() + static_cast<std::ptrdiff_t>(count)));
        double total = 0.0;
        for (std::size_t i = 0; i < count; ++i) total += (left[i] - leftMean) * (right[i] - rightMean);
        return box(total / static_cast<double>(sample ? count - 1 : count));
    }
    if ((op == "correlation" || op == "korelasyon" || op == "pearson") && argc == 2) {
        const auto left = statsList(argument(args, 0));
        const auto right = statsList(argument(args, 1));
        const std::size_t count = std::min(left.size(), right.size());
        if (count < 2) return box(0.0);
        const std::vector<double> leftPart(left.begin(), left.begin() + static_cast<std::ptrdiff_t>(count));
        const std::vector<double> rightPart(right.begin(), right.begin() + static_cast<std::ptrdiff_t>(count));
        const double leftMean = statsMean(leftPart);
        const double rightMean = statsMean(rightPart);
        double covariance = 0.0, leftVariance = 0.0, rightVariance = 0.0;
        for (std::size_t i = 0; i < count; ++i) {
            const double dl = leftPart[i] - leftMean;
            const double dr = rightPart[i] - rightMean;
            covariance += dl * dr;
            leftVariance += dl * dl;
            rightVariance += dr * dr;
        }
        if (leftVariance == 0.0 || rightVariance == 0.0) return box(0.0);
        return box(covariance / std::sqrt(leftVariance * rightVariance));
    }
    if ((op == "linear_regression" || op == "regression" || op == "regresyon" || op == "linear_fit") && argc == 2) {
        const auto left = statsList(argument(args, 0));
        const auto right = statsList(argument(args, 1));
        const std::size_t count = std::min(left.size(), right.size());
        auto object = std::make_shared<Object>();
        object->type = "LinearRegression";
        object->dynamicFields["count"] = static_cast<std::int64_t>(count);
        if (count < 2) {
            object->dynamicFields["slope"] = 0.0;
            object->dynamicFields["intercept"] = 0.0;
            object->dynamicFields["r2"] = 0.0;
            return box(Value{object});
        }
        const std::vector<double> x(left.begin(), left.begin() + static_cast<std::ptrdiff_t>(count));
        const std::vector<double> y(right.begin(), right.begin() + static_cast<std::ptrdiff_t>(count));
        const double xMean = statsMean(x);
        const double yMean = statsMean(y);
        double sxy = 0.0, sxx = 0.0, syy = 0.0;
        for (std::size_t i = 0; i < count; ++i) {
            const double dx = x[i] - xMean;
            const double dy = y[i] - yMean;
            sxy += dx * dy;
            sxx += dx * dx;
            syy += dy * dy;
        }
        const double slope = sxx == 0.0 ? 0.0 : sxy / sxx;
        const double intercept = yMean - slope * xMean;
        const double r2 = (sxx == 0.0 || syy == 0.0) ? 0.0 : (sxy * sxy) / (sxx * syy);
        object->dynamicFields["slope"] = slope;
        object->dynamicFields["intercept"] = intercept;
        object->dynamicFields["r2"] = r2;
        return box(Value{object});
    }
    if ((op == "summary" || op == "ozet") && argc >= 0) return box(statsSummary(statsValues(args, argc)));
    if ((op == "moving_average" || op == "hareketli_ortalama") && argc == 2) {
        const auto values = statsList(argument(args, 0));
        const int window = std::max(1, argumentInt(args, 1));
        std::vector<double> result;
        if (values.size() >= static_cast<std::size_t>(window)) {
            double total = 0.0;
            for (std::size_t i = 0; i < values.size(); ++i) {
                total += values[i];
                if (i >= static_cast<std::size_t>(window)) total -= values[i - static_cast<std::size_t>(window)];
                if (i + 1 >= static_cast<std::size_t>(window)) result.push_back(total / static_cast<double>(window));
            }
        }
        return box(statsArray(result));
    }
    if ((op == "histogram" || op == "histogram_olustur") && argc == 2) {
        const auto values = statsList(argument(args, 0));
        const int bins = std::max(1, argumentInt(args, 1));
        auto object = std::make_shared<Object>();
        object->type = "Histogram";
        if (values.empty()) {
            object->dynamicFields["min"] = 0.0;
            object->dynamicFields["max"] = 0.0;
            object->dynamicFields["width"] = 0.0;
            object->dynamicFields["counts"] = statsArray({});
            return box(Value{object});
        }
        const double minimum = *std::min_element(values.begin(), values.end());
        const double maximum = *std::max_element(values.begin(), values.end());
        const double width = maximum == minimum ? 1.0 : (maximum - minimum) / static_cast<double>(bins);
        std::vector<double> counts(static_cast<std::size_t>(bins), 0.0);
        for (const double value : values) {
            int index = static_cast<int>(std::floor((value - minimum) / width));
            if (index < 0) index = 0;
            if (index >= bins) index = bins - 1;
            counts[static_cast<std::size_t>(index)] += 1.0;
        }
        object->dynamicFields["min"] = minimum;
        object->dynamicFields["max"] = maximum;
        object->dynamicFields["width"] = width;
        object->dynamicFields["counts"] = statsArray(counts);
        return box(Value{object});
    }
    writeStderr("PRCYTERAS: Stats.");
    writeStderr(op);
    writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");
    return nullptr;
}

// ben yaptım opencode - 2026-11-14 - fuzzy arama
using FuzzyPoints = std::vector<std::uint32_t>;
FuzzyPoints fuzzyPoints(const std::string& text) {
    FuzzyPoints out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        std::uint32_t code = c;
        std::size_t length = 1;
        if ((c & 0x80) == 0) { code = c; length = 1; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) { code = ((c & 0x1Fu) << 6) | (static_cast<unsigned char>(text[i + 1]) & 0x3Fu); length = 2; }
        else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) { code = ((c & 0x0Fu) << 12) | ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(text[i + 2]) & 0x3Fu); length = 3; }
        else if ((c & 0xF8) == 0xF0 && i + 3 < text.size()) { code = ((c & 0x07u) << 18) | ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 12) | ((static_cast<unsigned char>(text[i + 2]) & 0x3Fu) << 6) | (static_cast<unsigned char>(text[i + 3]) & 0x3Fu); length = 4; }
        out.push_back(code);
        i += length;
    }
    return out;
}
std::size_t fuzzyDistancePoints(const FuzzyPoints& left, const FuzzyPoints& right) {
    if (left.empty()) return right.size();
    if (right.empty()) return left.size();
    std::vector<std::size_t> previous(right.size() + 1);
    std::vector<std::size_t> current(right.size() + 1);
    for (std::size_t j = 0; j <= right.size(); ++j) previous[j] = j;
    for (std::size_t i = 1; i <= left.size(); ++i) {
        current[0] = i;
        for (std::size_t j = 1; j <= right.size(); ++j) {
            const std::size_t cost = left[i - 1] == right[j - 1] ? 0 : 1;
            current[j] = std::min({previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost});
        }
        previous.swap(current);
    }
    return previous[right.size()];
}
double fuzzyRatioPoints(const FuzzyPoints& left, const FuzzyPoints& right) {
    if (left.empty() && right.empty()) return 1.0;
    const std::size_t longest = std::max(left.size(), right.size());
    if (longest == 0) return 1.0;
    const std::size_t distance = fuzzyDistancePoints(left, right);
    return 1.0 - static_cast<double>(distance) / static_cast<double>(longest);
}
double fuzzyRatio(const std::string& left, const std::string& right) {
    return fuzzyRatioPoints(fuzzyPoints(left), fuzzyPoints(right));
}
double fuzzyPartialRatioPoints(const FuzzyPoints& query, const FuzzyPoints& text) {
    if (query.empty()) return text.empty() ? 1.0 : 0.0;
    if (text.empty()) return 0.0;
    if (query.size() >= text.size()) return fuzzyRatioPoints(query, text);
    double best = 0.0;
    for (std::size_t start = 0; start + query.size() <= text.size(); ++start) {
        FuzzyPoints window(text.begin() + static_cast<std::ptrdiff_t>(start), text.begin() + static_cast<std::ptrdiff_t>(start + query.size()));
        best = std::max(best, fuzzyRatioPoints(query, window));
        if (best >= 1.0) break;
    }
    return best;
}
double fuzzyPartialRatio(const std::string& query, const std::string& text) {
    return fuzzyPartialRatioPoints(fuzzyPoints(query), fuzzyPoints(text));
}
std::string fuzzyFold(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
std::vector<std::string> fuzzyTokens(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    for (const char c : text) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!current.empty()) { tokens.push_back(fuzzyFold(current)); current.clear(); }
        } else current.push_back(c);
    }
    if (!current.empty()) tokens.push_back(fuzzyFold(current));
    return tokens;
}
std::string fuzzyJoinTokens(std::vector<std::string> tokens) {
    std::sort(tokens.begin(), tokens.end());
    std::string out;
    for (std::size_t i = 0; i < tokens.size(); ++i) { if (i) out.push_back(' '); out += tokens[i]; }
    return out;
}
double fuzzyTokenSortRatio(const std::string& left, const std::string& right) {
    return fuzzyRatio(fuzzyJoinTokens(fuzzyTokens(left)), fuzzyJoinTokens(fuzzyTokens(right)));
}
double fuzzyTokenSetRatio(const std::string& left, const std::string& right) {
    auto leftTokens = fuzzyTokens(left);
    auto rightTokens = fuzzyTokens(right);
    std::sort(leftTokens.begin(), leftTokens.end());
    leftTokens.erase(std::unique(leftTokens.begin(), leftTokens.end()), leftTokens.end());
    std::sort(rightTokens.begin(), rightTokens.end());
    rightTokens.erase(std::unique(rightTokens.begin(), rightTokens.end()), rightTokens.end());
    std::vector<std::string> intersection, leftOnly, rightOnly;
    std::set_intersection(leftTokens.begin(), leftTokens.end(), rightTokens.begin(), rightTokens.end(), std::back_inserter(intersection));
    std::set_difference(leftTokens.begin(), leftTokens.end(), rightTokens.begin(), rightTokens.end(), std::back_inserter(leftOnly));
    std::set_difference(rightTokens.begin(), rightTokens.end(), leftTokens.begin(), leftTokens.end(), std::back_inserter(rightOnly));
    const std::string base = fuzzyJoinTokens(intersection);
    const std::string leftText = fuzzyJoinTokens([&]() { std::vector<std::string> merged = intersection; merged.insert(merged.end(), leftOnly.begin(), leftOnly.end()); return merged; }());
    const std::string rightText = fuzzyJoinTokens([&]() { std::vector<std::string> merged = intersection; merged.insert(merged.end(), rightOnly.begin(), rightOnly.end()); return merged; }());
    double best = std::max(fuzzyRatio(base, leftText), fuzzyRatio(base, rightText));
    best = std::max(best, fuzzyRatio(leftText, rightText));
    return best;
}
bool fuzzyContains(const std::string& text, const std::string& query) {
    return fuzzyFold(text).find(fuzzyFold(query)) != std::string::npos;
}
bool fuzzyStartsWith(const std::string& text, const std::string& query) {
    const std::string foldedText = fuzzyFold(text);
    const std::string foldedQuery = fuzzyFold(query);
    return foldedText.rfind(foldedQuery, 0) == 0;
}
bool fuzzyEndsWith(const std::string& text, const std::string& query) {
    const std::string foldedText = fuzzyFold(text);
    const std::string foldedQuery = fuzzyFold(query);
    return foldedText.size() >= foldedQuery.size() && foldedText.compare(foldedText.size() - foldedQuery.size(), foldedQuery.size(), foldedQuery) == 0;
}
double fuzzyScore(const std::string& query, const std::string& text) {
    if (query.empty()) return text.empty() ? 1.0 : 0.0;
    if (text.empty()) return 0.0;
    if (fuzzyContains(text, query)) return 1.0;
    const double base = fuzzyRatio(query, text);
    const double partial = fuzzyPartialRatio(query, text);
    const double tokenSort = fuzzyTokenSortRatio(query, text);
    const double tokenSet = fuzzyTokenSetRatio(query, text);
    return std::max({base, partial, tokenSort, tokenSet});
}
std::vector<std::string> fuzzyCandidates(const Value& value) {
    std::vector<std::string> out;
    if (const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value)) {
        if (*list) {
            for (std::size_t i = 0; i < (*list)->values.size(); ++i) {
                if (i < (*list)->active.size() && !(*list)->active[i]) continue;
                out.push_back(stringOf((*list)->values[i]));
            }
            return out;
        }
    }
    if (const auto* object = std::get_if<std::shared_ptr<Object>>(&value)) {
        if (*object) {
            std::vector<std::string> keys;
            for (const auto& field : (*object)->dynamicFields) keys.push_back(field.first);
            std::sort(keys.begin(), keys.end());
            return keys;
        }
    }
    if (!std::holds_alternative<std::monostate>(value)) out.push_back(stringOf(value));
    return out;
}
void* fuzzyCall(const std::string& op, void** args, int argc) {
    if ((op == "distance" || op == "mesafe" || op == "levenshtein") && argc == 2) {
        return box(static_cast<std::int64_t>(fuzzyDistancePoints(fuzzyPoints(argumentString(args, 0)), fuzzyPoints(argumentString(args, 1)))));
    }
    if ((op == "ratio" || op == "oran" || op == "similarity" || op == "benzerlik") && argc == 2) {
        return box(fuzzyRatio(argumentString(args, 0), argumentString(args, 1)));
    }
    if ((op == "partial_ratio" || op == "kismi_oran") && argc == 2) {
        return box(fuzzyPartialRatio(argumentString(args, 0), argumentString(args, 1)));
    }
    if ((op == "token_sort_ratio" || op == "token_oran") && argc == 2) {
        return box(fuzzyTokenSortRatio(argumentString(args, 0), argumentString(args, 1)));
    }
    if ((op == "token_set_ratio" || op == "token_kume_oran") && argc == 2) {
        return box(fuzzyTokenSetRatio(argumentString(args, 0), argumentString(args, 1)));
    }
    if ((op == "score" || op == "puan") && argc == 2) {
        return box(fuzzyScore(argumentString(args, 0), argumentString(args, 1)));
    }
    if ((op == "contains" || op == "icerir") && argc == 2) {
        return box(fuzzyContains(argumentString(args, 0), argumentString(args, 1)));
    }
    if ((op == "starts_with" || op == "ile_baslar") && argc == 2) {
        return box(fuzzyStartsWith(argumentString(args, 0), argumentString(args, 1)));
    }
    if ((op == "ends_with" || op == "ile_biter") && argc == 2) {
        return box(fuzzyEndsWith(argumentString(args, 0), argumentString(args, 1)));
    }
    if ((op == "match" || op == "eslesir") && (argc == 2 || argc == 3)) {
        const double threshold = argc == 3 ? numberOf(argument(args, 2)) : 0.6;
        return box(fuzzyScore(argumentString(args, 0), argumentString(args, 1)) >= threshold);
    }
    if ((op == "search" || op == "ara") && (argc == 2 || argc == 3 || argc == 4)) {
        const std::string query = argumentString(args, 0);
        const auto candidates = fuzzyCandidates(argument(args, 1));
        const double threshold = argc >= 3 ? numberOf(argument(args, 2)) : 0.0;
        const int limit = argc >= 4 ? argumentInt(args, 3) : 0;
        struct Hit { std::size_t index; std::string item; double score; double ratio; double partial; };
        std::vector<Hit> hits;
        hits.reserve(candidates.size());
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const double score = fuzzyScore(query, candidates[i]);
            if (score < threshold) continue;
            hits.push_back(Hit{i, candidates[i], score, fuzzyRatio(query, candidates[i]), fuzzyPartialRatio(query, candidates[i])});
        }
        std::stable_sort(hits.begin(), hits.end(), [](const Hit& left, const Hit& right) { return left.score > right.score; });
        auto list = std::make_shared<ListValue>();
        std::size_t count = 0;
        for (const auto& hit : hits) {
            if (limit > 0 && count >= static_cast<std::size_t>(limit)) break;
            auto object = std::make_shared<Object>();
            object->type = "FuzzyHit";
            object->dynamicFields["item"] = hit.item;
            object->dynamicFields["score"] = hit.score;
            object->dynamicFields["ratio"] = hit.ratio;
            object->dynamicFields["partial"] = hit.partial;
            object->dynamicFields["index"] = static_cast<std::int64_t>(hit.index);
            list->values.emplace_back(Value{object});
            list->active.push_back(true);
            ++count;
        }
        return box(Value{list});
    }
    if ((op == "best" || op == "en_iyi") && (argc == 2 || argc == 3)) {
        const std::string query = argumentString(args, 0);
        const auto candidates = fuzzyCandidates(argument(args, 1));
        const double threshold = argc == 3 ? numberOf(argument(args, 2)) : 0.0;
        std::size_t bestIndex = 0;
        double bestScore = -1.0;
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const double score = fuzzyScore(query, candidates[i]);
            if (score > bestScore) { bestScore = score; bestIndex = i; }
        }
        if (bestScore < threshold) return box(Value{});
        auto object = std::make_shared<Object>();
        object->type = "FuzzyHit";
        object->dynamicFields["item"] = candidates[bestIndex];
        object->dynamicFields["score"] = bestScore;
        object->dynamicFields["index"] = static_cast<std::int64_t>(bestIndex);
        return box(Value{object});
    }
    writeStderr("PRCYTERAS: Fuzzy.");
    writeStderr(op);
    writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");
    return nullptr;
}

// ben yaptım opencode - 2026-11-14 - 100 kutuphane ortak altyapisi
std::string ocReadFile(const std::string& path, bool& ok) {
    std::ifstream input(fs::path(wide(path)), std::ios::binary);
    if (!input) { ok = false; return std::string(); }
    std::string data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    ok = true;
    return data;
}
bool ocWriteFile(const std::string& path, const std::string& data) {
    const fs::path target(wide(path));
    std::error_code error;
    if (target.has_parent_path()) fs::create_directories(target.parent_path(), error);
    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(output);
}
Value ocField(const Value& container, const std::string& key) {
    const Value* found = jsonField(container, key);
    return found ? *found : Value{};
}
void ocSetField(const Value& container, const std::string& key, Value value) {
    if (auto* object = std::get_if<std::shared_ptr<Object>>(&container)) {
        if (*object) (*object)->dynamicFields[key] = std::move(value);
    }
}
std::shared_ptr<Object> ocObjectOf(const Value& value) {
    const auto* object = std::get_if<std::shared_ptr<Object>>(&value);
    return object && *object ? *object : nullptr;
}
std::shared_ptr<ListValue> ocListOf(const Value& value) {
    const auto* list = std::get_if<std::shared_ptr<ListValue>>(&value);
    return list && *list ? *list : nullptr;
}
std::vector<Value> ocValues(const Value& value) {
    std::vector<Value> out;
    if (auto list = ocListOf(value)) {
        for (std::size_t i = 0; i < list->values.size(); ++i)
            if (i >= list->active.size() || list->active[i]) out.push_back(list->values[i]);
    } else if (!std::holds_alternative<std::monostate>(value)) {
        out.push_back(value);
    }
    return out;
}
std::vector<std::string> ocStringList(const Value& value) {
    std::vector<std::string> out;
    for (const auto& item : ocValues(value)) out.push_back(stringOf(item));
    return out;
}
std::vector<double> ocNumberList(const Value& value) {
    std::vector<double> out;
    for (const auto& item : ocValues(value)) out.push_back(numberOf(item));
    return out;
}
Value ocStringArray(const std::vector<std::string>& values) {
    Value array = stdlibArray();
    for (const auto& item : values) stdlibArrayAppend(array, item);
    return array;
}
Value ocNumberArray(const std::vector<double>& values) {
    Value array = stdlibArray();
    for (const double item : values) stdlibArrayAppend(array, item);
    return array;
}
std::string ocHex(const unsigned char* data, std::size_t size) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(digits[(data[i] >> 4) & 0xF]);
        out.push_back(digits[data[i] & 0xF]);
    }
    return out;
}
std::string ocHex(const std::string& data) { return ocHex(reinterpret_cast<const unsigned char*>(data.data()), data.size()); }
std::vector<unsigned char> ocUnhex(const std::string& text) {
    std::vector<unsigned char> out;
    int high = -1;
    for (const char c : text) {
        int value = -1;
        if (c >= '0' && c <= '9') value = c - '0';
        else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
        else continue;
        if (high < 0) high = value;
        else { out.push_back(static_cast<unsigned char>((high << 4) | value)); high = -1; }
    }
    return out;
}
struct OcSha256 {
    std::uint32_t state[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    std::uint64_t total = 0;
    unsigned char buffer[64]{};
    std::size_t used = 0;
    static std::uint32_t rotate(std::uint32_t value, int bits) { return (value >> bits) | (value << (32 - bits)); }
    void transform(const unsigned char* block) {
        static const std::uint32_t k[64] = {
            0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
            0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
            0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
            0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
            0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
            0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
            0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
            0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) w[i] = (static_cast<std::uint32_t>(block[i*4]) << 24) | (static_cast<std::uint32_t>(block[i*4+1]) << 16) | (static_cast<std::uint32_t>(block[i*4+2]) << 8) | block[i*4+3];
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotate(w[i-15],7) ^ rotate(w[i-15],18) ^ (w[i-15] >> 3);
            const std::uint32_t s1 = rotate(w[i-2],17) ^ rotate(w[i-2],19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        std::uint32_t a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotate(e,6) ^ rotate(e,11) ^ rotate(e,25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t t1 = h + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 = rotate(a,2) ^ rotate(a,13) ^ rotate(a,22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
    }
    void update(const unsigned char* data, std::size_t size) {
        total += size;
        while (size > 0) {
            const std::size_t take = std::min<std::size_t>(64 - used, size);
            std::memcpy(buffer + used, data, take);
            used += take; data += take; size -= take;
            if (used == 64) { transform(buffer); used = 0; }
        }
    }
    void finish(unsigned char out[32]) {
        const std::uint64_t bits = total * 8;
        const unsigned char pad = 0x80;
        update(&pad, 1);
        const unsigned char zero = 0;
        while (used != 56) update(&zero, 1);
        unsigned char length[8];
        for (int i = 0; i < 8; ++i) length[i] = static_cast<unsigned char>((bits >> (56 - i*8)) & 0xFF);
        update(length, 8);
        for (int i = 0; i < 8; ++i) {
            out[i*4] = static_cast<unsigned char>((state[i] >> 24) & 0xFF);
            out[i*4+1] = static_cast<unsigned char>((state[i] >> 16) & 0xFF);
            out[i*4+2] = static_cast<unsigned char>((state[i] >> 8) & 0xFF);
            out[i*4+3] = static_cast<unsigned char>(state[i] & 0xFF);
        }
    }
};
std::string ocSha256Raw(const std::string& data) {
    OcSha256 hash; hash.update(reinterpret_cast<const unsigned char*>(data.data()), data.size());
    unsigned char digest[32]; hash.finish(digest);
    return std::string(reinterpret_cast<const char*>(digest), 32);
}
std::string ocSha256(const std::string& data) { return ocHex(ocSha256Raw(data)); }
std::string ocHmacSha256Raw(const std::string& key, const std::string& data) {
    std::string block = key.size() > 64 ? ocSha256Raw(key) : key;
    block.resize(64, '\0');
    std::string inner(64, '\0'), outer(64, '\0');
    for (std::size_t i = 0; i < 64; ++i) {
        inner[i] = static_cast<char>(static_cast<unsigned char>(block[i]) ^ 0x36);
        outer[i] = static_cast<char>(static_cast<unsigned char>(block[i]) ^ 0x5c);
    }
    return ocSha256Raw(outer + ocSha256Raw(inner + data));
}
std::string ocBase64UrlEncode(const std::string& data) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    std::size_t i = 0;
    while (i + 2 < data.size()) {
        const unsigned int value = (static_cast<unsigned char>(data[i]) << 16) | (static_cast<unsigned char>(data[i+1]) << 8) | static_cast<unsigned char>(data[i+2]);
        out.push_back(table[(value >> 18) & 63]); out.push_back(table[(value >> 12) & 63]);
        out.push_back(table[(value >> 6) & 63]); out.push_back(table[value & 63]);
        i += 3;
    }
    if (i + 1 == data.size()) {
        const unsigned int value = static_cast<unsigned char>(data[i]) << 16;
        out.push_back(table[(value >> 18) & 63]); out.push_back(table[(value >> 12) & 63]);
    } else if (i + 2 == data.size()) {
        const unsigned int value = (static_cast<unsigned char>(data[i]) << 16) | (static_cast<unsigned char>(data[i+1]) << 8);
        out.push_back(table[(value >> 18) & 63]); out.push_back(table[(value >> 12) & 63]); out.push_back(table[(value >> 6) & 63]);
    }
    return out;
}
std::string ocBase64UrlDecode(const std::string& text, bool& ok) {
    auto valueOfChar = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '-' || c == '+') return 62;
        if (c == '_' || c == '/') return 63;
        return -1;
    };
    std::string out;
    int buffer = 0, bits = 0;
    for (const char c : text) {
        if (c == '=') break;
        const int value = valueOfChar(c);
        if (value < 0) continue;
        buffer = (buffer << 6) | value;
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back(static_cast<char>((buffer >> bits) & 0xFF)); }
    }
    ok = true;
    return out;
}
std::string ocRandomBytes(std::size_t count) {
    std::string out(count, '\0');
    if (count == 0) return out;
    if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(out.data()), static_cast<ULONG>(count), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
        for (std::size_t i = 0; i < count; ++i) out[i] = static_cast<char>(std::rand() & 0xFF);
    return out;
}
std::string ocRandomHex(std::size_t bytes) { return ocHex(ocRandomBytes(bytes)); }
std::string ocUrlEncode(const std::string& text) {
    static const char digits[] = "0123456789ABCDEF";
    std::string out;
    for (const unsigned char c : text) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out.push_back(static_cast<char>(c));
        else { out.push_back('%'); out.push_back(digits[(c >> 4) & 0xF]); out.push_back(digits[c & 0xF]); }
    }
    return out;
}
std::string ocUrlDecode(const std::string& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            const auto hexValue = [](char c) -> int { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; };
            const int high = hexValue(text[i+1]), low = hexValue(text[i+2]);
            if (high >= 0 && low >= 0) { out.push_back(static_cast<char>((high << 4) | low)); i += 2; continue; }
        }
        out.push_back(text[i] == '+' ? ' ' : text[i]);
    }
    return out;
}
std::int64_t ocNowMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
double ocNowMicros() {
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
Value ocParseJson(const std::string& text, bool& ok) {
    JsonReader reader(text);
    Value value;
    ok = reader.parseDocument(value);
    return value;
}
std::string ocJsonText(const Value& value) {
    std::string out;
    jsonSerialize(value, out, false, 0, 0);
    return out;
}
std::string ocJsonString(const Value& object, const std::string& key) {
    return stringOf(ocField(object, key));
}
int ptsLoadGltf(const std::string& filename) {
    if (!ptsD3D11.device) return -1;
    try {
        const fs::path modelPath = ptsResolveAssetPath(filename);
        std::error_code fileError;
        const auto fileSize = fs::file_size(modelPath, fileError);
        if (fileError || fileSize < 4 || fileSize > 256ULL * 1024 * 1024) return -1;
        bool readOkay = false;
        const std::string file = ocReadFile(modelPath.string(), readOkay);
        if (!readOkay || file.size() != fileSize) return -1;
        std::string jsonText, binaryChunk;
        if (file.size() >= 12 && file.compare(0, 4, "glTF") == 0) {
            auto readU32 = [&](std::size_t offset, std::uint32_t& out) {
                if (offset + 4 > file.size()) return false;
                out = static_cast<std::uint8_t>(file[offset]) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(file[offset + 1])) << 8) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(file[offset + 2])) << 16) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(file[offset + 3])) << 24);
                return true;
            };
            std::uint32_t version = 0, declaredLength = 0;
            if (!readU32(4, version) || !readU32(8, declaredLength) || version != 2 || declaredLength > file.size() || declaredLength < 20) return -1;
            std::size_t cursor = 12;
            while (cursor + 8 <= declaredLength) {
                std::uint32_t chunkLength = 0, chunkType = 0;
                if (!readU32(cursor, chunkLength) || !readU32(cursor + 4, chunkType) || chunkLength > declaredLength - cursor - 8) return -1;
                cursor += 8;
                if (chunkType == 0x4E4F534A && jsonText.empty()) jsonText.assign(file.data() + cursor, chunkLength);
                else if (chunkType == 0x004E4942 && binaryChunk.empty()) binaryChunk.assign(file.data() + cursor, chunkLength);
                cursor += chunkLength;
            }
            if (jsonText.empty() || cursor != declaredLength) return -1;
        } else if (modelPath.extension() == L".gltf" || modelPath.extension() == L".GLTF") {
            jsonText = file;
        } else return -1;
        bool jsonOkay = false;
        const Value root = ocParseJson(jsonText, jsonOkay);
        if (!jsonOkay) return -1;
        const auto buffers = ocValues(ocField(root, "buffers"));
        const auto views = ocValues(ocField(root, "bufferViews"));
        const auto accessors = ocValues(ocField(root, "accessors"));
        const auto meshes = ocValues(ocField(root, "meshes"));
        if (buffers.empty() || views.empty() || accessors.empty() || meshes.empty()) return -1;
        std::string binary = std::move(binaryChunk);
        const std::string uri = ocJsonString(buffers[0], "uri");
        if (!uri.empty()) {
            if (uri.rfind("data:", 0) == 0) {
                const auto comma = uri.find(',');
                if (comma == std::string::npos || uri.substr(0, comma).find(";base64") == std::string::npos || !base64Decode(uri.substr(comma + 1), false, binary)) return -1;
            } else {
                const fs::path external = modelPath.parent_path() / fs::path(wide(ocUrlDecode(uri)));
                binary = ocReadFile(external.string(), readOkay);
                if (!readOkay) return -1;
            }
        }
        const std::int64_t declaredBufferLength = static_cast<std::int64_t>(numberOf(ocField(buffers[0], "byteLength")));
        if (declaredBufferLength <= 0 || static_cast<std::uint64_t>(declaredBufferLength) > binary.size()) return -1;
        const auto primitives = ocValues(ocField(meshes[0], "primitives"));
        if (primitives.empty()) return -1;
        const Value primitive = primitives[0];
        const std::int64_t mode = std::holds_alternative<std::monostate>(ocField(primitive, "mode")) ? 4 : static_cast<std::int64_t>(numberOf(ocField(primitive, "mode")));
        if (mode != 4) return -1;
        const Value attributes = ocField(primitive, "attributes");
        auto indexValue = [](const Value& value, std::size_t limit) -> std::optional<std::size_t> {
            const double number = numberOf(value);
            if (!std::isfinite(number) || number < 0 || std::floor(number) != number || number >= static_cast<double>(limit)) return std::nullopt;
            return static_cast<std::size_t>(number);
        };
        auto readAccessor = [&](const Value& accessorValue, int requiredComponents, std::vector<double>& values, int allowedComponent) -> bool {
            const auto accessorIndex = indexValue(accessorValue, accessors.size());
            if (!accessorIndex) return false;
            const Value accessor = accessors[*accessorIndex];
            const auto viewIndex = indexValue(ocField(accessor, "bufferView"), views.size());
            if (!viewIndex) return false;
            const Value view = views[*viewIndex];
            if (static_cast<std::int64_t>(numberOf(ocField(view, "buffer"))) != 0) return false;
            const std::int64_t viewOffset = static_cast<std::int64_t>(numberOf(ocField(view, "byteOffset")));
            const std::int64_t viewLength = static_cast<std::int64_t>(numberOf(ocField(view, "byteLength")));
            const std::int64_t accessorOffset = static_cast<std::int64_t>(numberOf(ocField(accessor, "byteOffset")));
            const std::int64_t count = static_cast<std::int64_t>(numberOf(ocField(accessor, "count")));
            const int componentType = static_cast<int>(numberOf(ocField(accessor, "componentType")));
            const std::string type = ocJsonString(accessor, "type");
            const int components = type == "SCALAR" ? 1 : type == "VEC2" ? 2 : type == "VEC3" ? 3 : type == "VEC4" ? 4 : 0;
            const int componentBytes = componentType == 5126 || componentType == 5125 ? 4 : componentType == 5123 ? 2 : componentType == 5121 ? 1 : 0;
            if (components != requiredComponents || componentType != allowedComponent || count <= 0 || count > 10000000 ||
                viewOffset < 0 || viewLength < 0 || accessorOffset < 0 || componentBytes == 0) return false;
            const std::uint64_t packed = static_cast<std::uint64_t>(components) * componentBytes;
            const std::int64_t strideValue = static_cast<std::int64_t>(numberOf(ocField(view, "byteStride")));
            const std::uint64_t stride = strideValue > 0 ? static_cast<std::uint64_t>(strideValue) : packed;
            const auto viewOffsetU = static_cast<std::uint64_t>(viewOffset);
            const auto viewLengthU = static_cast<std::uint64_t>(viewLength);
            const auto accessorOffsetU = static_cast<std::uint64_t>(accessorOffset);
            if (stride < packed || viewOffsetU > binary.size() || viewLengthU > binary.size() - viewOffsetU ||
                accessorOffsetU > viewLengthU || packed > viewLengthU - accessorOffsetU ||
                (count > 1 && stride > (viewLengthU - accessorOffsetU - packed) / static_cast<std::uint64_t>(count - 1))) return false;
            values.clear(); values.reserve(static_cast<std::size_t>(count) * components);
            for (std::int64_t element = 0; element < count; ++element) {
                const std::size_t base = static_cast<std::size_t>(viewOffset + accessorOffset + element * static_cast<std::int64_t>(stride));
                for (int component = 0; component < components; ++component) {
                    const std::size_t offset = base + static_cast<std::size_t>(component) * componentBytes;
                    if (componentType == 5126) { float value = 0; std::memcpy(&value, binary.data() + offset, sizeof(value)); if (!std::isfinite(value)) return false; values.push_back(value); }
                    else if (componentType == 5125) { std::uint32_t value = 0; std::memcpy(&value, binary.data() + offset, sizeof(value)); values.push_back(value); }
                    else if (componentType == 5123) { std::uint16_t value = 0; std::memcpy(&value, binary.data() + offset, sizeof(value)); values.push_back(value); }
                    else values.push_back(static_cast<std::uint8_t>(binary[offset]));
                }
            }
            return true;
        };
        std::vector<double> positions, normals, texcoords, indices;
        if (!readAccessor(ocField(attributes, "POSITION"), 3, positions, 5126) || positions.size() % 3 != 0) return -1;
        const std::size_t vertexCount = positions.size() / 3;
        const Value normalAccessor = ocField(attributes, "NORMAL");
        const bool hasNormals = !std::holds_alternative<std::monostate>(normalAccessor);
        if (hasNormals && (!readAccessor(normalAccessor, 3, normals, 5126) || normals.size() != positions.size())) return -1;
        const Value uvAccessor = ocField(attributes, "TEXCOORD_0");
        const bool hasUv = !std::holds_alternative<std::monostate>(uvAccessor);
        if (hasUv && (!readAccessor(uvAccessor, 2, texcoords, 5126) || texcoords.size() / 2 != vertexCount)) return -1;
        const Value indicesAccessor = ocField(primitive, "indices");
        if (!std::holds_alternative<std::monostate>(indicesAccessor)) {
            const auto accessorIndex = indexValue(indicesAccessor, accessors.size());
            if (!accessorIndex) return -1;
            const int componentType = static_cast<int>(numberOf(ocField(accessors[*accessorIndex], "componentType")));
            if (!readAccessor(indicesAccessor, 1, indices, componentType) || (componentType != 5121 && componentType != 5123 && componentType != 5125)) return -1;
        } else {
            indices.reserve(vertexCount);
            for (std::size_t i = 0; i < vertexCount; ++i) indices.push_back(static_cast<double>(i));
        }
        if (indices.empty() || indices.size() % 3 != 0 || indices.size() > 30000000) return -1;
        std::vector<PtsVertex3D> vertices;
        vertices.reserve(indices.size());
        for (std::size_t triangle = 0; triangle < indices.size(); triangle += 3) {
            std::size_t ids[3]{};
            for (int corner = 0; corner < 3; ++corner) {
                const double raw = indices[triangle + corner];
                if (!std::isfinite(raw) || raw < 0 || std::floor(raw) != raw || raw >= static_cast<double>(vertexCount)) return -1;
                ids[corner] = static_cast<std::size_t>(raw);
            }
            const auto pos = [&](int corner) { const auto id = ids[corner] * 3; return PtsVec3{static_cast<float>(positions[id]), static_cast<float>(positions[id + 1]), static_cast<float>(positions[id + 2])}; };
            const PtsVec3 faceNormal = ptsNormalize(ptsCross(ptsSub(pos(1), pos(0)), ptsSub(pos(2), pos(0))));
            for (int corner = 0; corner < 3; ++corner) {
                const auto id = ids[corner];
                const float nx = hasNormals ? static_cast<float>(normals[id * 3]) : faceNormal.x;
                const float ny = hasNormals ? static_cast<float>(normals[id * 3 + 1]) : faceNormal.y;
                const float nz = hasNormals ? static_cast<float>(normals[id * 3 + 2]) : faceNormal.z;
                vertices.push_back({static_cast<float>(positions[id * 3]), static_cast<float>(positions[id * 3 + 1]), static_cast<float>(positions[id * 3 + 2]),
                    hasUv ? static_cast<float>(texcoords[id * 2]) : 0.0f, hasUv ? 1.0f - static_cast<float>(texcoords[id * 2 + 1]) : 0.0f, nx, ny, nz});
            }
        }
        PtsMesh3D mesh = ptsCreateMesh(vertices);
        if (!mesh.vertices || mesh.count == 0) return -1;
        ptsD3D11.models.push_back(mesh);
        return static_cast<int>(ptsD3D11.models.size() - 1);
    } catch (const std::exception& error) {
        writeStderr("PRCYTERAS Engine3D glTF hatasi: "); writeStderr(error.what()); writeStderr("\n");
        return -1;
    }
}
int ptsLoadHeightmap(const std::string& filename, double spacing, double heightScale) {
    if (!ptsD3D11.device || !std::isfinite(spacing) || !std::isfinite(heightScale) || spacing <= 0.0 || spacing > 10000.0 || std::abs(heightScale) > 10000.0) return -1;
    const std::wstring path = ptsResolveAssetPath(filename).wstring();
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(com);
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    UINT width = 0, height = 0;
    if (SUCCEEDED(hr)) hr = converter->GetSize(&width, &height);
    std::vector<BYTE> rgba;
    if (SUCCEEDED(hr) && width >= 2 && height >= 2 && width <= 2048 && height <= 2048 && static_cast<std::uint64_t>(width) * height <= 1048576ULL) {
        rgba.resize(static_cast<std::size_t>(width) * height * 4);
        hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(rgba.size()), rgba.data());
    } else if (SUCCEEDED(hr)) hr = E_INVALIDARG;
    if (converter) converter->Release(); if (frame) frame->Release(); if (decoder) decoder->Release(); if (factory) factory->Release();
    if (uninitialize) CoUninitialize();
    if (FAILED(hr)) return -1;
    try {
        const auto sample = [&](int x, int z) {
            const std::size_t at = (static_cast<std::size_t>(z) * width + static_cast<std::size_t>(x)) * 4;
            return static_cast<float>((static_cast<unsigned>(rgba[at]) + rgba[at + 1] + rgba[at + 2]) / (3.0 * 255.0));
        };
        std::vector<float> heights(static_cast<std::size_t>(width) * height);
        for (UINT z = 0; z < height; ++z) for (UINT x = 0; x < width; ++x) heights[static_cast<std::size_t>(z) * width + x] = sample(static_cast<int>(x), static_cast<int>(z)) * static_cast<float>(heightScale);
        std::vector<PtsVec3> normals(heights.size());
        for (UINT z = 0; z < height; ++z) for (UINT x = 0; x < width; ++x) {
            const UINT left = x == 0 ? x : x - 1, right = std::min(x + 1, width - 1);
            const UINT z0 = z == 0 ? z : z - 1, z1 = std::min(z + 1, height - 1);
            const float dx = (heights[static_cast<std::size_t>(z) * width + right] - heights[static_cast<std::size_t>(z) * width + left]) /
                static_cast<float>(std::max(1u, right - left) * spacing);
            const float dz = (heights[static_cast<std::size_t>(z1) * width + x] - heights[static_cast<std::size_t>(z0) * width + x]) /
                static_cast<float>(std::max(1u, z1 - z0) * spacing);
            normals[static_cast<std::size_t>(z) * width + x] = ptsNormalize({-dx, 1.0f, -dz});
        }
        auto vertex = [&](UINT x, UINT z) {
            const std::size_t at = static_cast<std::size_t>(z) * width + x;
            return PtsVertex3D{static_cast<float>(x * spacing), heights[at], static_cast<float>(z * spacing),
                static_cast<float>(x) / (width - 1), static_cast<float>(z) / (height - 1), normals[at].x, normals[at].y, normals[at].z};
        };
        std::vector<PtsVertex3D> vertices;
        vertices.reserve(static_cast<std::size_t>(width - 1) * (height - 1) * 6);
        for (UINT z = 0; z + 1 < height; ++z) for (UINT x = 0; x + 1 < width; ++x) {
            const PtsVertex3D a = vertex(x, z), b = vertex(x + 1, z), c = vertex(x, z + 1), d = vertex(x + 1, z + 1);
            vertices.insert(vertices.end(), {a, c, b, b, c, d});
        }
        PtsMesh3D mesh = ptsCreateMesh(vertices);
        if (!mesh.vertices || mesh.count == 0) return -1;
        ptsD3D11.models.push_back(mesh);
        return static_cast<int>(ptsD3D11.models.size() - 1);
    } catch (...) { return -1; }
}
struct OcHttpResponse {
    bool ok = false;
    int status = 0;
    std::string body;
    std::string headers;
    std::string error;
};
OcHttpResponse ocHttpRequest(const std::string& method, const std::string& url, const std::string& body,
                             const std::vector<std::pair<std::string,std::string>>& headers, int timeoutMs) {
    OcHttpResponse response;
    std::wstring wideUrl = wide(url);
    const std::size_t schemeEnd = wideUrl.find(L"://");
    if (schemeEnd == std::wstring::npos) { response.error = "gecersiz URL semasi"; return response; }
    const std::wstring scheme = wideUrl.substr(0, schemeEnd);
    std::wstring rest = wideUrl.substr(schemeEnd + 3);
    const std::size_t slash = rest.find(L'/');
    std::wstring hostPort = slash == std::wstring::npos ? rest : rest.substr(0, slash);
    std::wstring object = slash == std::wstring::npos ? L"/" : rest.substr(slash);
    std::wstring host = hostPort;
    INTERNET_PORT port = scheme == L"https" ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    const std::size_t colon = hostPort.rfind(L':');
    if (colon != std::wstring::npos && hostPort.find(L']') == std::wstring::npos) {
        host = hostPort.substr(0, colon);
        try { port = static_cast<INTERNET_PORT>(std::stoi(hostPort.substr(colon + 1))); } catch (...) {}
    }
    const bool secure = scheme == L"https";
    HINTERNET session = InternetOpenW(L"PRCYTERAS/0.11", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!session) { response.error = "InternetOpen basarisiz"; return response; }
    if (timeoutMs > 0) {
        InternetSetOptionW(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
        InternetSetOptionW(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
        InternetSetOptionW(session, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    }
    HINTERNET connection = InternetConnectW(session, host.c_str(), port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!connection) { response.error = "baglanti kurulamadi: " + utf8(host); InternetCloseHandle(session); return response; }
    const std::wstring wideMethod = wide(method);
    DWORD flags = secure ? INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE : INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    HINTERNET request = HttpOpenRequestW(connection, wideMethod.c_str(), object.c_str(), nullptr, nullptr, nullptr, flags, 0);
    if (!request) { response.error = "istek acilamadi"; InternetCloseHandle(connection); InternetCloseHandle(session); return response; }
    std::wstring headerBlock;
    for (const auto& header : headers) headerBlock += wide(header.first + ": " + header.second + "\r\n");
    BOOL sent = HttpSendRequestW(request, headerBlock.empty() ? nullptr : headerBlock.c_str(),
        headerBlock.empty() ? 0 : static_cast<DWORD>(headerBlock.size()),
        body.empty() ? nullptr : const_cast<char*>(body.data()), static_cast<DWORD>(body.size()));
    if (!sent) { response.error = "istek gonderilemedi (" + std::to_string(GetLastError()) + ")"; InternetCloseHandle(request); InternetCloseHandle(connection); InternetCloseHandle(session); return response; }
    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    if (HttpQueryInfoW(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, nullptr))
        response.status = static_cast<int>(statusCode);
    wchar_t headerBuffer[8192]; DWORD headerSize = sizeof(headerBuffer) - sizeof(wchar_t);
    if (HttpQueryInfoW(request, HTTP_QUERY_RAW_HEADERS_CRLF, headerBuffer, &headerSize, nullptr))
        response.headers = utf8(std::wstring(headerBuffer, headerSize / sizeof(wchar_t)));
    char buffer[8192]; DWORD read = 0;
    while (InternetReadFile(request, buffer, sizeof(buffer), &read) && read > 0) response.body.append(buffer, read);
    response.ok = response.status >= 200 && response.status < 300;
    if (response.status == 0) response.error = "yanit alinamadi";
    InternetCloseHandle(request); InternetCloseHandle(connection); InternetCloseHandle(session);
    return response;
}

// ben yaptım opencode - 2026-11-14 - linalg
Value ocVec(double x, double y, double z = 0.0, double w = 0.0, bool four = false) {
    Value value = stdlibObject("Vec");
    ocSetField(value, "x", x); ocSetField(value, "y", y); ocSetField(value, "z", z); ocSetField(value, "w", w);
    ocSetField(value, "dim", std::int64_t{four ? 4 : (z != 0.0 ? 3 : 2)});
    return value;
}
double ocComp(const Value& value, const char* key) { return numberOf(ocField(value, key)); }
void* linAlgCall(const std::string& op, void** args, int argc) {
    if ((op == "vec2" || op == "vektor2") && argc == 2) return box(ocVec(ocNum(args,0), ocNum(args,1)));
    if ((op == "vec3" || op == "vektor3") && argc == 3) return box(ocVec(ocNum(args,0), ocNum(args,1), ocNum(args,2)));
    if ((op == "vec4" || op == "vektor4") && argc == 4) return box(ocVec(ocNum(args,0), ocNum(args,1), ocNum(args,2), ocNum(args,3), true));
    if ((op == "add" || op == "topla") && argc == 2) {
        const Value a = argument(args,0), b = argument(args,1);
        return box(ocVec(ocComp(a,"x")+ocComp(b,"x"), ocComp(a,"y")+ocComp(b,"y"), ocComp(a,"z")+ocComp(b,"z"), ocComp(a,"w")+ocComp(b,"w"), numberOf(ocField(a,"dim")) >= 4));
    }
    if ((op == "sub" || op == "cikar") && argc == 2) {
        const Value a = argument(args,0), b = argument(args,1);
        return box(ocVec(ocComp(a,"x")-ocComp(b,"x"), ocComp(a,"y")-ocComp(b,"y"), ocComp(a,"z")-ocComp(b,"z"), ocComp(a,"w")-ocComp(b,"w"), numberOf(ocField(a,"dim")) >= 4));
    }
    if ((op == "scale" || op == "olcekle") && argc == 2) {
        const Value a = argument(args,0); const double s = ocNum(args,1);
        return box(ocVec(ocComp(a,"x")*s, ocComp(a,"y")*s, ocComp(a,"z")*s, ocComp(a,"w")*s, numberOf(ocField(a,"dim")) >= 4));
    }
    if ((op == "dot" || op == "nokta") && argc == 2) {
        const Value a = argument(args,0), b = argument(args,1);
        return box(ocComp(a,"x")*ocComp(b,"x") + ocComp(a,"y")*ocComp(b,"y") + ocComp(a,"z")*ocComp(b,"z") + ocComp(a,"w")*ocComp(b,"w"));
    }
    if ((op == "cross" || op == "capraz") && argc == 2) {
        const Value a = argument(args,0), b = argument(args,1);
        const double ax=ocComp(a,"x"), ay=ocComp(a,"y"), az=ocComp(a,"z");
        const double bx=ocComp(b,"x"), by=ocComp(b,"y"), bz=ocComp(b,"z");
        return box(ocVec(ay*bz-az*by, az*bx-ax*bz, ax*by-ay*bx));
    }
    if ((op == "length" || op == "uzunluk") && argc == 1) {
        const Value a = argument(args,0);
        return box(std::sqrt(ocComp(a,"x")*ocComp(a,"x") + ocComp(a,"y")*ocComp(a,"y") + ocComp(a,"z")*ocComp(a,"z") + ocComp(a,"w")*ocComp(a,"w")));
    }
    if ((op == "normalize" || op == "normallestir") && argc == 1) {
        const Value a = argument(args,0);
        const double length = std::sqrt(ocComp(a,"x")*ocComp(a,"x") + ocComp(a,"y")*ocComp(a,"y") + ocComp(a,"z")*ocComp(a,"z") + ocComp(a,"w")*ocComp(a,"w"));
        if (length == 0.0) return box(ocVec(0,0,0,0, numberOf(ocField(a,"dim")) >= 4));
        return box(ocVec(ocComp(a,"x")/length, ocComp(a,"y")/length, ocComp(a,"z")/length, ocComp(a,"w")/length, numberOf(ocField(a,"dim")) >= 4));
    }
    if ((op == "distance" || op == "mesafe") && argc == 2) {
        const Value a = argument(args,0), b = argument(args,1);
        const double dx=ocComp(a,"x")-ocComp(b,"x"), dy=ocComp(a,"y")-ocComp(b,"y"), dz=ocComp(a,"z")-ocComp(b,"z");
        return box(std::sqrt(dx*dx+dy*dy+dz*dz));
    }
    if ((op == "lerp" || op == "karistir") && argc == 3) {
        const Value a = argument(args,0), b = argument(args,1); const double t = ocNum(args,2);
        return box(ocVec(ocComp(a,"x")+(ocComp(b,"x")-ocComp(a,"x"))*t, ocComp(a,"y")+(ocComp(b,"y")-ocComp(a,"y"))*t, ocComp(a,"z")+(ocComp(b,"z")-ocComp(a,"z"))*t, ocComp(a,"w")+(ocComp(b,"w")-ocComp(a,"w"))*t, numberOf(ocField(a,"dim")) >= 4));
    }
    if ((op == "mat4_identity" || op == "birim_matris") && argc == 0) {
        Value m = stdlibObject("Mat4");
        const char* keys[16] = {"m00","m01","m02","m03","m10","m11","m12","m13","m20","m21","m22","m23","m30","m31","m32","m33"};
        for (int i=0;i<16;++i) ocSetField(m, keys[i], i%5==0 ? 1.0 : 0.0);
        return box(m);
    }
    auto matIdentity = [&]() {
        Value m = stdlibObject("Mat4");
        const char* keys[16] = {"m00","m01","m02","m03","m10","m11","m12","m13","m20","m21","m22","m23","m30","m31","m32","m33"};
        for (int i=0;i<16;++i) ocSetField(m, keys[i], i%5==0 ? 1.0 : 0.0);
        return m;
    };
    auto matMul = [](const Value& a, const Value& b) {
        Value m = stdlibObject("Mat4");
        const char* keys[16] = {"m00","m01","m02","m03","m10","m11","m12","m13","m20","m21","m22","m23","m30","m31","m32","m33"};
        for (int r=0;r<4;++r) for (int c=0;c<4;++c) {
            double sum=0;
            for (int k=0;k<4;++k) sum += ocComp(a, keys[r*4+k]) * ocComp(b, keys[k*4+c]);
            ocSetField(m, keys[r*4+c], sum);
        }
        return m;
    };
    if ((op == "mat4_translate" || op == "otele") && argc == 3) {
        Value m = matIdentity(); ocSetField(m,"m03",ocNum(args,0)); ocSetField(m,"m13",ocNum(args,1)); ocSetField(m,"m23",ocNum(args,2)); return box(m);
    }
    if ((op == "mat4_scale" || op == "olcek_matris") && argc == 3) {
        Value m = matIdentity(); ocSetField(m,"m00",ocNum(args,0)); ocSetField(m,"m11",ocNum(args,1)); ocSetField(m,"m22",ocNum(args,2)); return box(m);
    }
    if ((op == "mat4_rotate_x" || op == "x_dondur") && argc == 1) {
        Value m = matIdentity(); const double c=std::cos(ocNum(args,0)), s=std::sin(ocNum(args,0));
        ocSetField(m,"m11",c); ocSetField(m,"m12",-s); ocSetField(m,"m21",s); ocSetField(m,"m22",c); return box(m);
    }
    if ((op == "mat4_rotate_y" || op == "y_dondur") && argc == 1) {
        Value m = matIdentity(); const double c=std::cos(ocNum(args,0)), s=std::sin(ocNum(args,0));
        ocSetField(m,"m00",c); ocSetField(m,"m02",s); ocSetField(m,"m20",-s); ocSetField(m,"m22",c); return box(m);
    }
    if ((op == "mat4_rotate_z" || op == "z_dondur") && argc == 1) {
        Value m = matIdentity(); const double c=std::cos(ocNum(args,0)), s=std::sin(ocNum(args,0));
        ocSetField(m,"m00",c); ocSetField(m,"m01",-s); ocSetField(m,"m10",s); ocSetField(m,"m11",c); return box(m);
    }
    if ((op == "mat4_multiply" || op == "matris_carp") && argc == 2) return box(matMul(argument(args,0), argument(args,1)));
    if ((op == "mat4_transform" || op == "matris_uygula") && argc == 2) {
        const Value m = argument(args,0), v = argument(args,1);
        const double x=ocComp(v,"x"), y=ocComp(v,"y"), z=ocComp(v,"z"), w=numberOf(ocField(v,"dim"))>=4?ocComp(v,"w"):1.0;
        return box(ocVec(ocComp(m,"m00")*x+ocComp(m,"m01")*y+ocComp(m,"m02")*z+ocComp(m,"m03")*w,
                         ocComp(m,"m10")*x+ocComp(m,"m11")*y+ocComp(m,"m12")*z+ocComp(m,"m13")*w,
                         ocComp(m,"m20")*x+ocComp(m,"m21")*y+ocComp(m,"m22")*z+ocComp(m,"m23")*w,
                         ocComp(m,"m30")*x+ocComp(m,"m31")*y+ocComp(m,"m32")*z+ocComp(m,"m33")*w, true));
    }
    if ((op == "quat_identity" || op == "birim_quat") && argc == 0) {
        Value q = stdlibObject("Quat"); ocSetField(q,"x",0.0); ocSetField(q,"y",0.0); ocSetField(q,"z",0.0); ocSetField(q,"w",1.0); return box(q);
    }
    if ((op == "quat_from_axis_angle" || op == "eksen_aci") && argc == 2) {
        const Value axis = argument(args,0); const double angle = ocNum(args,1), half = angle*0.5, s = std::sin(half);
        Value q = stdlibObject("Quat"); ocSetField(q,"x",ocComp(axis,"x")*s); ocSetField(q,"y",ocComp(axis,"y")*s); ocSetField(q,"z",ocComp(axis,"z")*s); ocSetField(q,"w",std::cos(half)); return box(q);
    }
    if ((op == "quat_multiply" || op == "quat_carp") && argc == 2) {
        const Value a=argument(args,0), b=argument(args,1);
        const double ax=ocComp(a,"x"),ay=ocComp(a,"y"),az=ocComp(a,"z"),aw=ocComp(a,"w");
        const double bx=ocComp(b,"x"),by=ocComp(b,"y"),bz=ocComp(b,"z"),bw=ocComp(b,"w");
        Value q = stdlibObject("Quat");
        ocSetField(q,"x",aw*bx+ax*bw+ay*bz-az*by); ocSetField(q,"y",aw*by-ax*bz+ay*bw+az*bx);
        ocSetField(q,"z",aw*bz+ax*by-ay*bx+az*bw); ocSetField(q,"w",aw*bw-ax*bx-ay*by-az*bz); return box(q);
    }
    if ((op == "quat_rotate" || op == "quat_dondur") && argc == 2) {
        const Value q=argument(args,0), v=argument(args,1);
        const double qx=ocComp(q,"x"),qy=ocComp(q,"y"),qz=ocComp(q,"z"),qw=ocComp(q,"w");
        const double vx=ocComp(v,"x"),vy=ocComp(v,"y"),vz=ocComp(v,"z");
        const double tx=2*(qy*vz-qz*vy), ty=2*(qz*vx-qx*vz), tz=2*(qx*vy-qy*vx);
        return box(ocVec(vx+qw*tx+(qy*tz-qz*ty), vy+qw*ty+(qz*tx-qx*tz), vz+qw*tz+(qx*ty-qy*tx)));
    }
    writeStderr("PRCYTERAS: LinAlg."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - fixed-point
std::int64_t ocFixed(double value) { return static_cast<std::int64_t>(std::llround(value * 65536.0)); }
void* fixedPointCall(const std::string& op, void** args, int argc) {
    if ((op == "from_int" || op == "tamsayidan") && argc == 1) return box(static_cast<std::int64_t>(ocInt(args,0)) * 65536);
    if ((op == "from_float" || op == "ondaliktan") && argc == 1) return box(ocFixed(ocNum(args,0)));
    if ((op == "to_float" || op == "ondalik") && argc == 1) return box(static_cast<double>(ocInt(args,0)) / 65536.0);
    if ((op == "to_int" || op == "tamsayi") && argc == 1) return box(static_cast<std::int64_t>(ocInt(args,0) / 65536));
    if ((op == "add" || op == "topla") && argc == 2) return box(static_cast<std::int64_t>(ocInt(args,0)) + ocInt(args,1));
    if ((op == "sub" || op == "cikar") && argc == 2) return box(static_cast<std::int64_t>(ocInt(args,0)) - ocInt(args,1));
    if ((op == "mul" || op == "carp") && argc == 2) return box(static_cast<std::int64_t>((static_cast<__int128>(ocInt(args,0)) * ocInt(args,1)) / 65536));
    if ((op == "div" || op == "bol") && argc == 2) { const std::int64_t b=ocInt(args,1); if(b==0) return box(std::int64_t{0}); return box(static_cast<std::int64_t>((static_cast<__int128>(ocInt(args,0)) * 65536) / b)); }
    if ((op == "sqrt" || op == "karekok") && argc == 1) return box(ocFixed(std::sqrt(std::max(0.0, static_cast<double>(ocInt(args,0)) / 65536.0))));
    if ((op == "abs" || op == "mutlak") && argc == 1) return box(static_cast<std::int64_t>(std::llabs(ocInt(args,0))));
    if ((op == "min" || op == "kucuk") && argc == 2) return box(std::min(static_cast<std::int64_t>(ocInt(args,0)), static_cast<std::int64_t>(ocInt(args,1))));
    if ((op == "max" || op == "buyuk") && argc == 2) return box(std::max(static_cast<std::int64_t>(ocInt(args,0)), static_cast<std::int64_t>(ocInt(args,1))));
    if ((op == "floor" || op == "taban") && argc == 1) { std::int64_t v=ocInt(args,0); return box(static_cast<std::int64_t>((v>=0?v:v-65535)/65536*65536)); }
    if ((op == "ceil" || op == "tavan") && argc == 1) { std::int64_t v=ocInt(args,0); return box(static_cast<std::int64_t>((v>=0?v+65535:v)/65536*65536)); }
    if ((op == "round" || op == "yuvarla") && argc == 1) { std::int64_t v=ocInt(args,0); return box(static_cast<std::int64_t>(((v>=0?v+32768:v-32768)/65536)*65536)); }
    if ((op == "compare" || op == "karsilastir") && argc == 2) { const std::int64_t a=ocInt(args,0),b=ocInt(args,1); return box(std::int64_t{a<b?-1:(a>b?1:0)}); }
    writeStderr("PRCYTERAS: FixedPoint."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - bigint
using OcBig = std::vector<std::uint32_t>;
OcBig ocBigParse(const std::string& text, bool& ok) {
    OcBig out; ok = true; bool negative = false; std::size_t i = 0;
    while (i < text.size() && (text[i]==' '||text[i]=='\t')) ++i;
    if (i < text.size() && (text[i]=='-'||text[i]=='+')) { negative = text[i]=='-'; ++i; }
    if (i >= text.size()) { ok = false; return {}; }
    for (; i < text.size(); ++i) {
        if (text[i] < '0' || text[i] > '9') { ok = false; return {}; }
        std::uint64_t carry = static_cast<std::uint64_t>(text[i]-'0');
        for (std::size_t k=0;k<out.size();++k) { const std::uint64_t value = static_cast<std::uint64_t>(out[k])*10 + carry; out[k]=static_cast<std::uint32_t>(value%1000000000u); carry=value/1000000000u; }
        if (carry) out.push_back(static_cast<std::uint32_t>(carry));
    }
    if (negative && !out.empty() && !(out.size()==1 && out[0]==0)) out.push_back(0xFFFFFFFFu);
    return out;
}
std::string ocBigString(const OcBig& value) {
    if (value.empty()) return "0";
    const bool negative = value.back() == 0xFFFFFFFFu;
    OcBig digits = value; if (negative) digits.pop_back();
    if (digits.empty()) return "0";
    std::ostringstream out; if (negative) out << '-';
    out << digits.back();
    for (std::size_t i = digits.size()-1; i-- > 0;) out << std::setw(9) << std::setfill('0') << digits[i];
    return out.str();
}
OcBig ocBigAbs(OcBig value) { if (!value.empty() && value.back()==0xFFFFFFFFu) value.pop_back(); return value; }
int ocBigCmpAbs(const OcBig& a, const OcBig& b) {
    OcBig x=ocBigAbs(a), y=ocBigAbs(b);
    while (x.size()>1 && x.back()==0) x.pop_back();
    while (y.size()>1 && y.back()==0) y.pop_back();
    if (x.size()!=y.size()) return x.size()<y.size()?-1:1;
    for (std::size_t i=x.size(); i-- > 0;) if (x[i]!=y[i]) return x[i]<y[i]?-1:1;
    return 0;
}
OcBig ocBigAddAbs(const OcBig& a, const OcBig& b) {
    OcBig out; std::uint64_t carry=0;
    for (std::size_t i=0;i<std::max(a.size(),b.size())||carry;++i) { std::uint64_t value=carry; if(i<a.size())value+=a[i]; if(i<b.size())value+=b[i]; out.push_back(static_cast<std::uint32_t>(value%1000000000u)); carry=value/1000000000u; }
    return out;
}
OcBig ocBigSubAbs(const OcBig& a, const OcBig& b) {
    OcBig out; std::int64_t borrow=0;
    for (std::size_t i=0;i<a.size();++i) { std::int64_t value=static_cast<std::int64_t>(a[i])-(i<b.size()?b[i]:0)-borrow; borrow=0; if(value<0){value+=1000000000;borrow=1;} out.push_back(static_cast<std::uint32_t>(value)); }
    while (out.size()>1 && out.back()==0) out.pop_back();
    return out;
}
OcBig ocBigMulAbs(const OcBig& a, const OcBig& b) {
    OcBig out(a.size()+b.size(), 0);
    for (std::size_t i=0;i<a.size();++i) { std::uint64_t carry=0; for(std::size_t j=0;j<b.size()||carry;++j){ std::uint64_t value=out[i+j]+carry+(j<b.size()?static_cast<std::uint64_t>(a[i])*b[j]:0); out[i+j]=static_cast<std::uint32_t>(value%1000000000u); carry=value/1000000000u; } }
    while (out.size()>1 && out.back()==0) out.pop_back();
    return out;
}
OcBig ocBigDivAbs(const OcBig& a, const OcBig& b) {
    if (b.empty() || (b.size()==1 && b[0]==0)) return {};
    OcBig result, remainder;
    for (std::size_t i=a.size(); i-- > 0;) {
        remainder.insert(remainder.begin(), a[i]);
        while (remainder.size()>1 && remainder.back()==0) remainder.pop_back();
        std::uint32_t digit=0;
        while (ocBigCmpAbs(remainder,b)>=0) { remainder=ocBigSubAbs(remainder,b); ++digit; }
        result.insert(result.begin(), digit);
    }
    while (result.size()>1 && result.back()==0) result.pop_back();
    return result;
}
void* bigIntCall(const std::string& op, void** args, int argc) {
    if ((op == "parse" || op == "coz" || op == "from_string") && argc == 1) { bool ok=false; ocBigParse(argumentString(args,0),ok); if(!ok) return nullptr; return box(argumentString(args,0)); }
    if ((op == "from_int" || op == "tamsayidan") && argc == 1) return box(std::to_string(ocInt(args,0)));
    if ((op == "to_string" || op == "metin") && argc == 1) { bool ok=false; auto v=ocBigParse(argumentString(args,0),ok); return box(ok?ocBigString(v):std::string("0")); }
    auto binary = [&](const char* which) -> void* {
        bool okA=false, okB=false; OcBig a=ocBigParse(argumentString(args,0),okA), b=ocBigParse(argumentString(args,1),okB);
        if(!okA||!okB) return nullptr;
        const bool negA=!a.empty()&&a.back()==0xFFFFFFFFu, negB=!b.empty()&&b.back()==0xFFFFFFFFu;
        a=ocBigAbs(a); b=ocBigAbs(b);
        OcBig result;
        if (std::strcmp(which,"add")==0) {
            if (negA==negB) { result=ocBigAddAbs(a,b); if(negA&&!(result.size()==1&&result[0]==0)) result.push_back(0xFFFFFFFFu); }
            else if (ocBigCmpAbs(a,b)>=0) { result=ocBigSubAbs(a,b); if(negA&&!(result.size()==1&&result[0]==0)) result.push_back(0xFFFFFFFFu); }
            else { result=ocBigSubAbs(b,a); if(negB&&!(result.size()==1&&result[0]==0)) result.push_back(0xFFFFFFFFu); }
        } else if (std::strcmp(which,"sub")==0) {
            if (negA!=negB) { result=ocBigAddAbs(a,b); if(negA&&!(result.size()==1&&result[0]==0)) result.push_back(0xFFFFFFFFu); }
            else if (ocBigCmpAbs(a,b)>=0) { result=ocBigSubAbs(a,b); if(negA&&!(result.size()==1&&result[0]==0)) result.push_back(0xFFFFFFFFu); }
            else { result=ocBigSubAbs(b,a); if(!negA&&!(result.size()==1&&result[0]==0)) result.push_back(0xFFFFFFFFu); }
        } else if (std::strcmp(which,"mul")==0) {
            result=ocBigMulAbs(a,b); if((negA!=negB)&&!(result.size()==1&&result[0]==0)) result.push_back(0xFFFFFFFFu);
        } else {
            result=ocBigDivAbs(a,b); if((negA!=negB)&&!(result.size()==1&&result[0]==0)) result.push_back(0xFFFFFFFFu);
        }
        return box(ocBigString(result));
    };
    if ((op == "add" || op == "topla") && argc == 2) return binary("add");
    if ((op == "sub" || op == "cikar") && argc == 2) return binary("sub");
    if ((op == "mul" || op == "carp") && argc == 2) return binary("mul");
    if ((op == "div" || op == "bol") && argc == 2) return binary("div");
    if ((op == "mod" || op == "kalan") && argc == 2) {
        bool okA=false,okB=false; OcBig a=ocBigAbs(ocBigParse(argumentString(args,0),okA)), b=ocBigAbs(ocBigParse(argumentString(args,1),okB));
        if(!okA||!okB||b.empty()) return nullptr;
        OcBig q=ocBigDivAbs(a,b); OcBig product=ocBigMulAbs(q,b); return box(ocBigString(ocBigSubAbs(a,product)));
    }
    if ((op == "pow" || op == "kuvvet") && argc == 2) {
        bool ok=false; OcBig base=ocBigParse(argumentString(args,0),ok); if(!ok) return nullptr;
        const bool negative=!base.empty()&&base.back()==0xFFFFFFFFu; base=ocBigAbs(base);
        long long exponent=ocInt(args,1); if(exponent<0) return box(std::string("0"));
        OcBig result{1};
        while (exponent>0) { if(exponent&1) result=ocBigMulAbs(result,base); base=ocBigMulAbs(base,base); exponent>>=1; }
        if(negative && (ocInt(args,1)&1) && !(result.size()==1&&result[0]==0)) result.push_back(0xFFFFFFFFu);
        return box(ocBigString(result));
    }
    if ((op == "gcd" || op == "ebob") && argc == 2) {
        bool okA=false,okB=false; OcBig a=ocBigAbs(ocBigParse(argumentString(args,0),okA)), b=ocBigAbs(ocBigParse(argumentString(args,1),okB));
        if(!okA||!okB) return nullptr;
        while (!(b.size()==1 && b[0]==0)) { OcBig q=ocBigDivAbs(a,b); OcBig product=ocBigMulAbs(q,b); OcBig r=ocBigSubAbs(a,product); a=b; b=r; }
        return box(ocBigString(a));
    }
    if ((op == "abs" || op == "mutlak") && argc == 1) { bool ok=false; auto v=ocBigParse(argumentString(args,0),ok); if(!ok) return nullptr; return box(ocBigString(ocBigAbs(v))); }
    if ((op == "neg" || op == "negatif") && argc == 1) { bool ok=false; auto v=ocBigParse(argumentString(args,0),ok); if(!ok) return nullptr; if(!(v.size()==1&&v[0]==0)){ if(!v.empty()&&v.back()==0xFFFFFFFFu)v.pop_back(); else v.push_back(0xFFFFFFFFu);} return box(ocBigString(v)); }
    if ((op == "compare" || op == "karsilastir") && argc == 2) {
        bool okA=false,okB=false; OcBig a=ocBigParse(argumentString(args,0),okA), b=ocBigParse(argumentString(args,1),okB); if(!okA||!okB) return nullptr;
        const bool negA=!a.empty()&&a.back()==0xFFFFFFFFu, negB=!b.empty()&&b.back()==0xFFFFFFFFu;
        if(negA!=negB) return box(std::int64_t{negA?-1:1});
        const int cmp=ocBigCmpAbs(a,b); return box(std::int64_t{negA?-cmp:cmp});
    }
    if ((op == "is_zero" || op == "sifir_mi") && argc == 1) { bool ok=false; auto v=ocBigAbs(ocBigParse(argumentString(args,0),ok)); return box(ok && v.size()==1 && v[0]==0); }
    writeStderr("PRCYTERAS: BigInt."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - decimal
std::string ocDecimalNormalize(const std::string& input, bool& ok) {
    ok = true; std::string text = stdlibTrim(input); bool negative=false; std::size_t i=0;
    if (i<text.size() && (text[i]=='-'||text[i]=='+')) { negative=text[i]=='-'; ++i; }
    std::string digits; int scale=0; bool dot=false, any=false;
    for (; i<text.size(); ++i) {
        if (text[i]>='0'&&text[i]<='9') { digits.push_back(text[i]); any=true; if(dot)++scale; }
        else if (text[i]=='.'&&!dot) dot=true;
        else if (text[i]=='e'||text[i]=='E') { ++i; bool expNeg=false; if(i<text.size()&&(text[i]=='-'||text[i]=='+')){expNeg=text[i]=='-';++i;} int exponent=0; for(;i<text.size()&&text[i]>='0'&&text[i]<='9';++i) exponent=exponent*10+(text[i]-'0'); scale += expNeg?exponent:-exponent; any=true; break; }
        else if (text[i]==' '||text[i]=='\t') continue;
        else { ok=false; return "0"; }
    }
    if (!any) { ok=false; return "0"; }
    while (scale<0) { digits.push_back('0'); ++scale; }
    while (digits.size()>1 && digits.front()=='0') digits.erase(digits.begin());
    if (digits.empty()) digits="0";
    if (digits=="0") { negative=false; scale=0; }
    return (negative?"-":"") + digits + "|" + std::to_string(scale);
}
std::string ocDecimalText(const std::string& normalized) {
    const std::size_t bar=normalized.find('|'); if(bar==std::string::npos) return normalized;
    std::string digits=normalized.substr(0,bar); int scale=std::stoi(normalized.substr(bar+1)); bool negative=false;
    if(!digits.empty()&&digits[0]=='-'){negative=true;digits.erase(digits.begin());}
    if(scale<=0){ std::string out=digits+std::string(static_cast<std::size_t>(-scale),'0'); return (negative?"-":"")+out; }
    if(static_cast<std::size_t>(scale)>=digits.size()) digits=std::string(static_cast<std::size_t>(scale)-digits.size()+1,'0')+digits;
    const std::size_t dot=digits.size()-static_cast<std::size_t>(scale);
    return (negative?"-":"")+digits.substr(0,dot)+"."+digits.substr(dot);
}
std::vector<int> ocDecimalDigits(const std::string& normalized, int& scale, bool& negative) {
    const std::size_t bar=normalized.find('|'); std::string digits=normalized.substr(0,bar); scale=std::stoi(normalized.substr(bar+1)); negative=false;
    if(!digits.empty()&&digits[0]=='-'){negative=true;digits.erase(digits.begin());}
    if(scale<0){digits+=std::string(static_cast<std::size_t>(-scale),'0');scale=0;}
    std::vector<int> out; out.reserve(digits.size()); for(char c:digits)out.push_back(c-'0'); return out;
}
std::string ocDecimalFromDigits(std::vector<int> digits, int scale, bool negative) {
    while(digits.size()>1&&digits.front()==0)digits.erase(digits.begin());
    if(digits.empty())digits.push_back(0);
    bool zero=true; for(int d:digits)if(d){zero=false;break;}
    if(zero){negative=false;scale=0;}
    std::string out; if(negative)out.push_back('-'); for(int d:digits)out.push_back(static_cast<char>('0'+d)); out.push_back('|'); out+=std::to_string(scale); return out;
}
std::string ocDecimalAdd(const std::string& a, const std::string& b, bool subtract) {
    bool validA=false,validB=false;
    const std::string normalizedA=ocDecimalNormalize(a,validA);
    const std::string normalizedB=ocDecimalNormalize(b,validB);
    if(!validA||!validB)return {};
    int sa=0,sb=0; bool na=false,nb=false;
    std::vector<int> da=ocDecimalDigits(normalizedA,sa,na), db=ocDecimalDigits(normalizedB,sb,nb);
    if(subtract)nb=!nb;
    const int scale=std::max(sa,sb);
    da.insert(da.end(),static_cast<std::size_t>(scale-sa),0); db.insert(db.end(),static_cast<std::size_t>(scale-sb),0);
    const std::size_t size=std::max(da.size(),db.size()); da.insert(da.begin(),size-da.size(),0); db.insert(db.begin(),size-db.size(),0);
    if(na==nb) { std::vector<int> out(size+1,0); int carry=0; for(std::size_t i=size;i-- > 0;){int v=da[i]+db[i]+carry;out[i+1]=v%10;carry=v/10;} out[0]=carry; return ocDecimalFromDigits(out,scale,na); }
    if(ocBigCmpAbs(OcBig(da.begin(),da.end()),OcBig(db.begin(),db.end()))<0){std::swap(da,db);std::swap(na,nb);}
    std::vector<int> out(size,0); int borrow=0; for(std::size_t i=size;i-- > 0;){int v=da[i]-db[i]-borrow;borrow=0;if(v<0){v+=10;borrow=1;}out[i]=v;} return ocDecimalFromDigits(out,scale,na);
}
void* decimalCall(const std::string& op, void** args, int argc) {
    if ((op == "parse" || op == "coz" || op == "from_string") && argc == 1) { bool ok=false; auto v=ocDecimalNormalize(argumentString(args,0),ok); return ok?box(v):nullptr; }
    if ((op == "to_string" || op == "metin") && argc == 1) return box(ocDecimalText(argumentString(args,0)));
    if ((op == "from_int" || op == "tamsayidan") && argc == 1) { bool ok=false; return box(ocDecimalNormalize(std::to_string(ocInt(args,0)),ok)); }
    if ((op == "from_float" || op == "ondaliktan") && argc == 1) { bool ok=false; std::ostringstream out; out<<std::setprecision(15)<<ocNum(args,0); return box(ocDecimalNormalize(out.str(),ok)); }
    if ((op == "add" || op == "topla") && argc == 2) return box(ocDecimalAdd(argumentString(args,0),argumentString(args,1),false));
    if ((op == "sub" || op == "cikar") && argc == 2) return box(ocDecimalAdd(argumentString(args,0),argumentString(args,1),true));
    if ((op == "mul" || op == "carp") && argc == 2) {
        bool okA=false,okB=false; const std::string a=ocDecimalNormalize(argumentString(args,0),okA), b=ocDecimalNormalize(argumentString(args,1),okB); if(!okA||!okB)return nullptr;
        int sa=0,sb=0; bool na=false,nb=false; auto da=ocDecimalDigits(a,sa,na), db=ocDecimalDigits(b,sb,nb);
        std::vector<int> out(da.size()+db.size(),0);
        for(std::size_t i=da.size();i-- > 0;) for(std::size_t j=db.size();j-- > 0;){out[i+j+1]+=da[i]*db[j];}
        for(std::size_t i=out.size();i-- > 1;){out[i-1]+=out[i]/10;out[i]%=10;}
        return box(ocDecimalFromDigits(out,sa+sb,na!=nb));
    }
    if ((op == "div" || op == "bol") && argc >= 2) {
        bool okA=false,okB=false; const std::string a=ocDecimalNormalize(argumentString(args,0),okA), b=ocDecimalNormalize(argumentString(args,1),okB); if(!okA||!okB)return nullptr;
        const int precision=argc>=3?std::clamp(argumentInt(args,2),0,18):10;
        int sa=0,sb=0; bool na=false,nb=false; auto da=ocDecimalDigits(a,sa,na), db=ocDecimalDigits(b,sb,nb);
        bool zero=true; for(int d:db)if(d){zero=false;break;} if(zero)return nullptr;
        for(int i=0;i<precision+sb-sa;++i)da.push_back(0);
        std::vector<int> quotient;
        std::vector<int> remainder;
        for(int digit:da){remainder.push_back(digit); while(remainder.size()>1&&remainder.front()==0)remainder.erase(remainder.begin()); int q=0; while(ocBigCmpAbs(OcBig(remainder.begin(),remainder.end()),OcBig(db.begin(),db.end()))>=0){OcBig r(remainder.begin(),remainder.end()),d(db.begin(),db.end()); OcBig qq=ocBigDivAbs(r,d); OcBig product=ocBigMulAbs(qq,d); OcBig next=ocBigSubAbs(r,product); remainder.assign(next.begin(),next.end()); q+=static_cast<int>(qq.empty()?0:qq[0]); if(qq.size()!=1)q=9;} quotient.push_back(std::min(q,9));}
        return box(ocDecimalFromDigits(quotient,std::max(0,precision),na!=nb));
    }
    if ((op == "round" || op == "yuvarla") && argc >= 1) {
        bool ok=false; auto v=ocDecimalNormalize(argumentString(args,0),ok); if(!ok)return nullptr;
        const int places=argc>=2?argumentInt(args,1):0; int scale=0; bool negative=false; auto digits=ocDecimalDigits(v,scale,negative);
        if(places>=scale)return box(v);
        const int drop=scale-places; for(int i=0;i<drop;++i){int last=digits.back();digits.pop_back(); if(i==drop-1&&last>=5){int carry=1; for(std::size_t k=digits.size();k-- > 0&&carry;){int sum=digits[k]+carry;digits[k]=sum%10;carry=sum/10;} if(carry)digits.insert(digits.begin(),1);}}
        return box(ocDecimalFromDigits(digits,places,negative));
    }
    if ((op == "compare" || op == "karsilastir") && argc == 2) {
        const std::string diff=ocDecimalAdd(argumentString(args,0),argumentString(args,1),true); bool ok=false; auto v=ocDecimalNormalize(ocDecimalText(diff),ok);
        const bool negative=!v.empty()&&v[0]=='-'; bool zero=true; for(char c:v){if(c>='1'&&c<='9'){zero=false;break;}}
        return box(std::int64_t{zero?0:(negative?-1:1)});
    }
    writeStderr("PRCYTERAS: Decimal."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - noise
double ocFade(double t) { return t*t*t*(t*(t*6-15)+10); }
double ocLerp(double a, double b, double t) { return a+(b-a)*t; }
double ocGrad(int hash, double x, double y, double z) {
    const int h=hash&15; const double u=h<8?x:y, v=h<4?y:(h==12||h==14?x:z);
    return ((h&1)?-u:u)+((h&2)?-v:v);
}
double ocPerlin2(double x, double y, int seed) {
    const int xi=static_cast<int>(std::floor(x))&255, yi=static_cast<int>(std::floor(y))&255;
    const double xf=x-std::floor(x), yf=y-std::floor(y);
    auto hash=[seed](int a,int b){ std::uint32_t h=static_cast<std::uint32_t>(a*374761393+b*668265263+seed*1442695041); h=(h^(h>>13))*1274126177u; return static_cast<int>(h^(h>>16)); };
    const int aa=hash(xi,yi), ab=hash(xi,yi+1), ba=hash(xi+1,yi), bb=hash(xi+1,yi+1);
    const double u=ocFade(xf), v=ocFade(yf);
    return ocLerp(ocLerp(ocGrad(aa,xf,yf,0),ocGrad(ba,xf-1,yf,0),u), ocLerp(ocGrad(ab,xf,yf-1,0),ocGrad(bb,xf-1,yf-1,0),u), v);
}
double ocPerlin3(double x, double y, double z, int seed) {
    const int xi=static_cast<int>(std::floor(x))&255, yi=static_cast<int>(std::floor(y))&255, zi=static_cast<int>(std::floor(z))&255;
    const double xf=x-std::floor(x), yf=y-std::floor(y), zf=z-std::floor(z);
    auto hash=[seed](int a,int b,int c){ std::uint32_t h=static_cast<std::uint32_t>(a*374761393+b*668265263+c*2246822519u+seed*1442695041); h=(h^(h>>13))*1274126177u; return static_cast<int>(h^(h>>16)); };
    const double u=ocFade(xf), v=ocFade(yf), w=ocFade(zf);
    auto corner=[&](int dx,int dy,int dz){ return ocGrad(hash(xi+dx,yi+dy,zi+dz), xf-dx, yf-dy, zf-dz); };
    const double x00=ocLerp(corner(0,0,0),corner(1,0,0),u), x10=ocLerp(corner(0,1,0),corner(1,1,0),u);
    const double x01=ocLerp(corner(0,0,1),corner(1,0,1),u), x11=ocLerp(corner(0,1,1),corner(1,1,1),u);
    return ocLerp(ocLerp(x00,x10,v),ocLerp(x01,x11,v),w);
}
double ocWorley2(double x, double y, int seed) {
    const int xi=static_cast<int>(std::floor(x)), yi=static_cast<int>(std::floor(y)); double best=1e9;
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){ const int cx=xi+dx,cy=yi+dy; std::uint32_t h=static_cast<std::uint32_t>(cx*374761393+cy*668265263+seed*1442695041); h=(h^(h>>13))*1274126177u; const double px=cx+((h&0xFFFF)/65535.0), py=cy+(((h>>16)&0xFFFF)/65535.0); const double d=(px-x)*(px-x)+(py-y)*(py-y); best=std::min(best,d);} return std::sqrt(best);
}
void* noiseCall(const std::string& op, void** args, int argc) {
    const int seed=argc>=3?argumentInt(args,2):0;
    if ((op == "perlin2" || op == "perlin") && argc >= 2) return box(ocPerlin2(ocNum(args,0),ocNum(args,1),seed));
    if ((op == "perlin3" || op == "perlin_3d") && argc >= 3) return box(ocPerlin3(ocNum(args,0),ocNum(args,1),ocNum(args,2),argc>=4?argumentInt(args,3):0));
    if ((op == "simplex2" || op == "simplex") && argc >= 2) {
        const double x=ocNum(args,0), y=ocNum(args,1); double total=0, amplitude=1, frequency=1, maxValue=0;
        for(int octave=0;octave<4;++octave){ total+=ocPerlin2(x*frequency,y*frequency,seed+octave*17)*amplitude; maxValue+=amplitude; amplitude*=0.5; frequency*=2; }
        return box(total/maxValue);
    }
    if ((op == "worley2" || op == "worley") && argc >= 2) return box(ocWorley2(ocNum(args,0),ocNum(args,1),seed));
    if ((op == "fbm2" || op == "fbm") && argc >= 2) {
        const int octaves=argc>=3?std::clamp(argumentInt(args,2),1,12):5; const double persistence=argc>=4?ocNum(args,3):0.5;
        double total=0, amplitude=1, frequency=1, maxValue=0;
        for(int i=0;i<octaves;++i){total+=ocPerlin2(ocNum(args,0)*frequency,ocNum(args,1)*frequency,seed+i*31)*amplitude;maxValue+=amplitude;amplitude*=persistence;frequency*=2;}
        return box(maxValue==0?0.0:total/maxValue);
    }
    writeStderr("PRCYTERAS: Noise."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - binary-pack
void ocPackAppend(std::string& out, std::uint64_t value, int bytes, bool little) {
    for(int i=0;i<bytes;++i){ const int shift=(little?i:bytes-1-i)*8; out.push_back(static_cast<char>((value>>shift)&0xFF)); }
}
std::uint64_t ocPackRead(const std::string& data, std::size_t offset, int bytes, bool little) {
    std::uint64_t value=0; for(int i=0;i<bytes;++i){ const int shift=(little?i:bytes-1-i)*8; if(offset+static_cast<std::size_t>(i)<data.size()) value|=static_cast<std::uint64_t>(static_cast<unsigned char>(data[offset+i]))<<shift; } return value;
}
void* binaryPackCall(const std::string& op, void** args, int argc) {
    const bool little=argc>=2?truthy(argument(args,1)):true;
    if ((op == "u8" || op == "i8") && argc >= 1) { std::string out; ocPackAppend(out,static_cast<std::uint64_t>(ocInt(args,0)),1,little); return box(out); }
    if ((op == "u16" || op == "i16") && argc >= 1) { std::string out; ocPackAppend(out,static_cast<std::uint64_t>(ocInt(args,0)),2,little); return box(out); }
    if ((op == "u32" || op == "i32" || op == "f32") && argc >= 1) {
        std::string out; if(op=="f32"){float f=static_cast<float>(ocNum(args,0));std::uint32_t bits;std::memcpy(&bits,&f,4);ocPackAppend(out,bits,4,little);} else ocPackAppend(out,static_cast<std::uint64_t>(ocInt(args,0)),4,little); return box(out);
    }
    if ((op == "u64" || op == "i64" || op == "f64") && argc >= 1) {
        std::string out; if(op=="f64"){double d=ocNum(args,0);std::uint64_t bits;std::memcpy(&bits,&d,8);ocPackAppend(out,bits,8,little);} else ocPackAppend(out,static_cast<std::uint64_t>(ocInt(args,0)),8,little); return box(out);
    }
    if ((op == "string" || op == "metin") && argc >= 1) {
        const std::string text=argumentString(args,0); std::string out; ocPackAppend(out,text.size(),4,little); out+=text; return box(out);
    }
    if ((op == "bytes" || op == "baytlar") && argc >= 1) return box(argumentString(args,0));
    if ((op == "read_u8" || op == "read_i8") && argc >= 2) return box(static_cast<std::int64_t>(static_cast<std::int8_t>(ocPackRead(argumentString(args,0),static_cast<std::size_t>(ocInt(args,1)),1,little))));
    if ((op == "read_u16" || op == "read_i16") && argc >= 2) return box(static_cast<std::int64_t>(static_cast<std::int16_t>(ocPackRead(argumentString(args,0),static_cast<std::size_t>(ocInt(args,1)),2,little))));
    if ((op == "read_u32" || op == "read_i32") && argc >= 2) return box(static_cast<std::int64_t>(static_cast<std::int32_t>(ocPackRead(argumentString(args,0),static_cast<std::size_t>(ocInt(args,1)),4,little))));
    if ((op == "read_u64" || op == "read_i64") && argc >= 2) return box(static_cast<std::int64_t>(ocPackRead(argumentString(args,0),static_cast<std::size_t>(ocInt(args,1)),8,little)));
    if ((op == "read_f32") && argc >= 2) { std::uint32_t bits=static_cast<std::uint32_t>(ocPackRead(argumentString(args,0),static_cast<std::size_t>(ocInt(args,1)),4,little)); float f; std::memcpy(&f,&bits,4); return box(static_cast<double>(f)); }
    if ((op == "read_f64") && argc >= 2) { std::uint64_t bits=ocPackRead(argumentString(args,0),static_cast<std::size_t>(ocInt(args,1)),8,little); double d; std::memcpy(&d,&bits,8); return box(d); }
    if ((op == "read_string") && argc >= 2) { const std::string data=argumentString(args,0); const std::size_t offset=static_cast<std::size_t>(ocInt(args,1)); const std::size_t length=static_cast<std::size_t>(ocPackRead(data,offset,4,little)); return box(offset+4+length<=data.size()?data.substr(offset+4,length):std::string()); }
    if ((op == "size" || op == "uzunluk") && argc >= 1) return box(static_cast<std::int64_t>(argumentString(args,0).size()));
    writeStderr("PRCYTERAS: BinaryPack."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - profiler
struct OcProfilerState {
    std::unordered_map<std::string, std::vector<double>> samples;
    std::unordered_map<std::string, double> starts;
    std::mutex mutex;
};
OcProfilerState& ocProfiler() { static OcProfilerState state; return state; }
void* profilerCall(const std::string& op, void** args, int argc) {
    auto& state=ocProfiler();
    if ((op == "now" || op == "simdi") && argc == 0) return box(ocNowMicros());
    if ((op == "now_millis" || op == "simdi_ms") && argc == 0) return box(static_cast<std::int64_t>(ocNowMillis()));
    if ((op == "start" || op == "baslat") && argc >= 1) { std::lock_guard<std::mutex> lock(state.mutex); state.starts[argumentString(args,0)]=ocNowMicros(); return box(true); }
    if ((op == "stop" || op == "durdur") && argc >= 1) {
        const std::string name=argumentString(args,0); std::lock_guard<std::mutex> lock(state.mutex); const auto found=state.starts.find(name); if(found==state.starts.end())return box(-1.0);
        const double elapsed=ocNowMicros()-found->second; state.starts.erase(found); state.samples[name].push_back(elapsed); return box(elapsed);
    }
    if ((op == "elapsed" || op == "gecen") && argc >= 1) { std::lock_guard<std::mutex> lock(state.mutex); const auto found=state.starts.find(argumentString(args,0)); return box(found==state.starts.end()?-1.0:ocNowMicros()-found->second); }
    if ((op == "reset" || op == "sifirla") && argc >= 0) { std::lock_guard<std::mutex> lock(state.mutex); state.samples.clear(); state.starts.clear(); return box(true); }
    if ((op == "report" || op == "rapor") && argc == 0) {
        std::lock_guard<std::mutex> lock(state.mutex); Value report=stdlibObject("ProfilerReport");
        for(const auto& entry:state.samples){ if(entry.second.empty())continue; double total=0,min=entry.second[0],max=entry.second[0]; for(double v:entry.second){total+=v;min=std::min(min,v);max=std::max(max,v);} Value item=stdlibObject("ProfileSample"); ocSetField(item,"count",static_cast<std::int64_t>(entry.second.size()));ocSetField(item,"total_us",total);ocSetField(item,"avg_us",total/static_cast<double>(entry.second.size()));ocSetField(item,"min_us",min);ocSetField(item,"max_us",max);ocSetField(report,entry.first,item);} return box(report);
    }
    if ((op == "samples" || op == "ornekler") && argc >= 1) { std::lock_guard<std::mutex> lock(state.mutex); return box(ocNumberArray(state.samples[argumentString(args,0)])); }
    writeStderr("PRCYTERAS: Profiler."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - unit-test
struct OcTestState { int passed=0, failed=0; std::vector<std::string> messages; std::mutex mutex; };
OcTestState& ocTests() { static OcTestState state; return state; }
void* unitTestCall(const std::string& op, void** args, int argc) {
    auto& state=ocTests();
    auto record=[&](bool ok,const std::string& message){ std::lock_guard<std::mutex> lock(state.mutex); if(ok)++state.passed; else {++state.failed; state.messages.push_back(message);} return box(ok); };
    if ((op == "assert_true" || op == "dogru") && argc >= 1) return record(truthy(argument(args,0)), argc>=2?argumentString(args,1):"assert_true basarisiz");
    if ((op == "assert_false" || op == "yanlis") && argc >= 1) return record(!truthy(argument(args,0)), argc>=2?argumentString(args,1):"assert_false basarisiz");
    if ((op == "assert_eq" || op == "esit") && argc >= 2) return record(binary("==",argument(args,0),argument(args,1))==Value(true), argc>=3?argumentString(args,2):"assert_eq basarisiz");
    if ((op == "assert_ne" || op == "esit_degil") && argc >= 2) return record(binary("!=",argument(args,0),argument(args,1))==Value(true), argc>=3?argumentString(args,2):"assert_ne basarisiz");
    if ((op == "assert_near" || op == "yaklasik") && argc >= 2) { const double a=ocNum(args,0),b=ocNum(args,1),tolerance=argc>=3?ocNum(args,2):1e-9; return record(std::fabs(a-b)<=tolerance, argc>=4?argumentString(args,3):"assert_near basarisiz"); }
    if ((op == "fail" || op == "basarisiz") && argc >= 0) return record(false, argc>=1?argumentString(args,0):"fail");
    if ((op == "pass" || op == "gec") && argc >= 0) return record(true, "");
    if ((op == "summary" || op == "ozet") && argc == 0) { std::lock_guard<std::mutex> lock(state.mutex); Value value=stdlibObject("TestSummary"); ocSetField(value,"passed",static_cast<std::int64_t>(state.passed));ocSetField(value,"failed",static_cast<std::int64_t>(state.failed));ocSetField(value,"total",static_cast<std::int64_t>(state.passed+state.failed));ocSetField(value,"messages",ocStringArray(state.messages));return box(value); }
    if ((op == "reset" || op == "sifirla") && argc == 0) { std::lock_guard<std::mutex> lock(state.mutex); state.passed=state.failed=0; state.messages.clear(); return box(true); }
    writeStderr("PRCYTERAS: UnitTest."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - cli-args
void* cliArgsCall(const std::string& op, void** args, int argc) {
    if ((op == "parse" || op == "coz") && argc >= 1) {
        const auto argumentsList=ocStringList(argument(args,0)); Value result=stdlibObject("CliArgs"); Value flags=stdlibObject("JsonObject"); Value values=stdlibObject("JsonObject"); Value positionals=stdlibArray();
        for(std::size_t i=0;i<argumentsList.size();++i){ const std::string& item=argumentsList[i]; if(item.rfind("--",0)==0){ std::string name=item.substr(2); const std::size_t equals=name.find('='); if(equals!=std::string::npos){ocSetField(values,name.substr(0,equals),name.substr(equals+1));} else if(i+1<argumentsList.size()&&argumentsList[i+1].rfind("-",0)!=0){ocSetField(values,name,argumentsList[++i]);} else {ocSetField(flags,name,true);} } else if(item.size()>1&&item[0]=='-'){ocSetField(flags,item.substr(1),true);} else stdlibArrayAppend(positionals,item); }
        ocSetField(result,"flags",flags);ocSetField(result,"values",values);ocSetField(result,"positionals",positionals);ocSetField(result,"program",argumentsList.empty()?std::string():argumentsList[0]);return box(result);
    }
    if ((op == "flag" || op == "bayrak") && argc >= 2) { const Value flags=ocField(argument(args,0),"flags"); return box(truthy(ocField(flags,argumentString(args,1)))); }
    if ((op == "value" || op == "deger") && argc >= 2) { const Value values=ocField(argument(args,0),"values"); const Value found=ocField(values,argumentString(args,1)); return box(std::holds_alternative<std::monostate>(found)?(argc>=3?argument(args,2):Value{}):found); }
    if ((op == "positional" || op == "konumsal") && argc >= 2) { const Value list=ocField(argument(args,0),"positionals"); const auto items=ocValues(list); const std::size_t index=static_cast<std::size_t>(std::max<std::int64_t>(0,ocInt(args,1))); return box(index<items.size()?items[index]:(argc>=3?argument(args,2):Value{})); }
    if ((op == "positional_count" || op == "konumsal_sayi") && argc >= 1) return box(static_cast<std::int64_t>(ocValues(ocField(argument(args,0),"positionals")).size()));
    if ((op == "help" || op == "yardim") && argc >= 1) {
        const Value spec=argument(args,0); std::string out; const Value title=ocField(spec,"title"); out+=std::holds_alternative<std::monostate>(title)?std::string("Kullanim"):stringOf(title); out+="\n"; const Value options=ocField(spec,"options"); if(auto list=ocListOf(options)){for(const auto& item:ocValues(options)){out+="  "+stringOf(ocField(item,"name"))+"\t"+stringOf(ocField(item,"description"))+"\n";}} return box(out);
    }
    writeStderr("PRCYTERAS: CliArgs."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - terminal-ui
void* terminalUiCall(const std::string& op, void** args, int argc) {
    if ((op == "color" || op == "renk") && argc >= 1) {
        const std::string color=argumentString(args,0); std::string code="\x1b[";
        if(color=="red"||color=="kirmizi")code+="31";else if(color=="green"||color=="yesil")code+="32";else if(color=="yellow"||color=="sari")code+="33";else if(color=="blue"||color=="mavi")code+="34";else if(color=="magenta"||color=="mor")code+="35";else if(color=="cyan"||color=="camgobegi")code+="36";else if(color=="white"||color=="beyaz")code+="37";else if(color=="gray"||color=="gri")code+="90";else code+="0";
        const std::string text=argc>=2?argumentString(args,1):""; return box(code+"m"+text+"\x1b[0m");
    }
    if ((op == "reset" || op == "sifirla") && argc == 0) return box(std::string("\x1b[0m"));
    if ((op == "clear" || op == "temizle") && argc == 0) return box(std::string("\x1b[2J\x1b[H"));
    if ((op == "move" || op == "tasi") && argc >= 2) return box("\x1b["+std::to_string(ocInt(args,1)+1)+";"+std::to_string(ocInt(args,0)+1)+"H");
    if ((op == "progress" || op == "ilerleme") && argc >= 1) {
        const int percent=std::clamp(argumentInt(args,0),0,100); const int width=argc>=2?std::clamp(argumentInt(args,1),4,80):30; const int filled=percent*width/100;
        std::string out="["; for(int i=0;i<width;++i)out+=(i<filled?"#":"-"); out+="] "+std::to_string(percent)+"%"; return box(out);
    }
    if ((op == "table" || op == "tablo") && argc >= 1) {
        const auto rows=ocValues(argument(args,0)); std::vector<std::vector<std::string>> cells; std::size_t columns=0;
        for(const auto& row:rows){ auto values=ocStringList(row); columns=std::max(columns,values.size()); cells.push_back(values); }
        std::vector<std::size_t> widths(columns,0); for(const auto& row:cells)for(std::size_t i=0;i<row.size();++i)widths[i]=std::max(widths[i],row[i].size());
        std::string out; for(const auto& row:cells){out+="|";for(std::size_t i=0;i<columns;++i){const std::string value=i<row.size()?row[i]:"";out+=" "+value+std::string(widths[i]-value.size()+1,' ')+"|";}out+="\n";}
        if(!cells.empty()){out+="|";for(std::size_t i=0;i<columns;++i)out+=std::string(widths[i]+2,'-')+"|";out+="\n";} return box(out);
    }
    if ((op == "menu" || op == "menu_ciz") && argc >= 1) { const auto items=ocStringList(argument(args,0)); const int selected=argc>=2?argumentInt(args,1):-1; std::string out; for(std::size_t i=0;i<items.size();++i){out+=(static_cast<int>(i)==selected?"> ":"  ")+items[i]+"\n";} return box(out); }
    if ((op == "spinner" || op == "donen") && argc >= 1) { static const char* frames[]={"|","/","-","\\"}; return box(std::string(frames[std::abs(argumentInt(args,0))%4])); }
    writeStderr("PRCYTERAS: TerminalUi."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}

// ben yaptım opencode - 2026-11-14 - pathfinding
struct OcGrid { int width=0, height=0; std::vector<std::uint8_t> cells; };
OcGrid ocGridParse(const Value& value) {
    OcGrid grid;
    const auto rows = ocValues(value);
    grid.height = static_cast<int>(rows.size());
    for (const auto& row : rows) {
        const auto cells = ocValues(row);
        grid.width = std::max(grid.width, static_cast<int>(cells.size()));
    }
    grid.cells.assign(static_cast<std::size_t>(grid.width) * static_cast<std::size_t>(grid.height), 0);
    for (int y = 0; y < grid.height; ++y) {
        const auto cells = ocValues(rows[static_cast<std::size_t>(y)]);
        for (int x = 0; x < static_cast<int>(cells.size()); ++x)
            grid.cells[static_cast<std::size_t>(y) * grid.width + x] = truthy(cells[static_cast<std::size_t>(x)]) ? 1 : 0;
    }
    return grid;
}
bool ocGridWalk(const OcGrid& grid, int x, int y) {
    if (x < 0 || y < 0 || x >= grid.width || y >= grid.height) return false;
    return grid.cells[static_cast<std::size_t>(y) * grid.width + x] != 0;
}
Value ocPointValue(int x, int y) { Value p = stdlibObject("Point"); ocSetField(p, "x", static_cast<std::int64_t>(x)); ocSetField(p, "y", static_cast<std::int64_t>(y)); return p; }
Value ocPathValue(const std::vector<int>& path, const OcGrid& grid) {
    Value out = stdlibArray();
    for (const int index : path) stdlibArrayAppend(out, ocPointValue(index % grid.width, index / grid.width));
    return out;
}
std::vector<int> ocSearchPath(const OcGrid& grid, int sx, int sy, int gx, int gy, const std::string& mode) {
    std::vector<int> empty;
    if (!ocGridWalk(grid, sx, sy) || !ocGridWalk(grid, gx, gy)) return empty;
    const std::size_t total = static_cast<std::size_t>(grid.width) * grid.height;
    const int start = sy * grid.width + sx, goal = gy * grid.width + gx;
    std::vector<double> dist(total, std::numeric_limits<double>::infinity());
    std::vector<int> parent(total, -1);
    std::vector<std::uint8_t> closed(total, 0);
    auto heuristic = [&](int index) {
        if (mode != "astar") return 0.0;
        const int x = index % grid.width, y = index / grid.width;
        return static_cast<double>(std::abs(x - gx) + std::abs(y - gy));
    };
    using Entry = std::pair<double, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
    dist[static_cast<std::size_t>(start)] = 0.0;
    open.push({heuristic(start), start});
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (!open.empty()) {
        const int current = open.top().second; open.pop();
        if (closed[static_cast<std::size_t>(current)]) continue;
        closed[static_cast<std::size_t>(current)] = 1;
        if (current == goal) break;
        const int cx = current % grid.width, cy = current / grid.width;
        for (int dir = 0; dir < 4; ++dir) {
            const int nx = cx + dx[dir], ny = cy + dy[dir];
            if (!ocGridWalk(grid, nx, ny)) continue;
            const int next = ny * grid.width + nx;
            const double cost = 1.0;
            const double candidate = dist[static_cast<std::size_t>(current)] + cost;
            if (candidate < dist[static_cast<std::size_t>(next)]) {
                dist[static_cast<std::size_t>(next)] = candidate;
                parent[static_cast<std::size_t>(next)] = current;
                open.push({candidate + heuristic(next), next});
            }
        }
    }
    if (parent[static_cast<std::size_t>(goal)] < 0 && goal != start) return empty;
    std::vector<int> path;
    for (int at = goal; at >= 0; at = parent[static_cast<std::size_t>(at)]) { path.push_back(at); if (at == start) break; }
    std::reverse(path.begin(), path.end());
    if (path.empty() || path.front() != start) return empty;
    return path;
}
void* navMesh3DCall(const std::string& op, void** args, int argc) {
    if ((op == "find_path" || op == "astar" || op == "yol_bul") && argc == 3) {
        const auto nodes = ocValues(argument(args, 0));
        const int start = argumentInt(args, 1), goal = argumentInt(args, 2);
        Value empty = stdlibArray();
        if (nodes.empty() || nodes.size() > 250000 || start < 0 || goal < 0 ||
            static_cast<std::size_t>(start) >= nodes.size() || static_cast<std::size_t>(goal) >= nodes.size()) return box(empty);
        struct Node { double x=0, y=0, z=0; std::vector<int> edges; };
        std::vector<Node> graph(nodes.size());
        std::size_t edgeCount = 0;
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            const Value node = nodes[i];
            graph[i].x = numberOf(ocField(node, "x")); graph[i].y = numberOf(ocField(node, "y")); graph[i].z = numberOf(ocField(node, "z"));
            if (!std::isfinite(graph[i].x) || !std::isfinite(graph[i].y) || !std::isfinite(graph[i].z)) return box(empty);
            for (const auto& neighbor : ocValues(ocField(node, "neighbors"))) {
                const double value = numberOf(neighbor);
                if (!std::isfinite(value) || value < 0 || std::floor(value) != value || value >= nodes.size()) return box(empty);
                graph[i].edges.push_back(static_cast<int>(value));
                if (++edgeCount > 4000000) return box(empty);
            }
        }
        auto distance3 = [&](int a, int b) {
            const double dx = graph[static_cast<std::size_t>(a)].x - graph[static_cast<std::size_t>(b)].x;
            const double dy = graph[static_cast<std::size_t>(a)].y - graph[static_cast<std::size_t>(b)].y;
            const double dz = graph[static_cast<std::size_t>(a)].z - graph[static_cast<std::size_t>(b)].z;
            return std::sqrt(dx*dx + dy*dy + dz*dz);
        };
        struct Entry { double score; int node; };
        struct Greater { bool operator()(const Entry& a, const Entry& b) const { return a.score > b.score; } };
        std::priority_queue<Entry, std::vector<Entry>, Greater> open;
        std::vector<double> costs(nodes.size(), std::numeric_limits<double>::infinity());
        std::vector<int> parents(nodes.size(), -1);
        std::vector<bool> closed(nodes.size(), false);
        costs[static_cast<std::size_t>(start)] = 0.0;
        open.push({distance3(start, goal), start});
        while (!open.empty()) {
            const Entry current = open.top(); open.pop();
            if (closed[static_cast<std::size_t>(current.node)]) continue;
            if (current.node == goal) break;
            closed[static_cast<std::size_t>(current.node)] = true;
            for (const int next : graph[static_cast<std::size_t>(current.node)].edges) {
                if (closed[static_cast<std::size_t>(next)]) continue;
                const double candidate = costs[static_cast<std::size_t>(current.node)] + distance3(current.node, next);
                if (candidate < costs[static_cast<std::size_t>(next)]) {
                    costs[static_cast<std::size_t>(next)] = candidate;
                    parents[static_cast<std::size_t>(next)] = current.node;
                    open.push({candidate + distance3(next, goal), next});
                }
            }
        }
        if (start != goal && parents[static_cast<std::size_t>(goal)] < 0) return box(empty);
        std::vector<int> path;
        for (int at = goal, steps = 0; at >= 0 && steps <= static_cast<int>(nodes.size()); at = parents[static_cast<std::size_t>(at)], ++steps) {
            path.push_back(at);
            if (at == start) break;
        }
        if (path.empty() || path.back() != start) return box(empty);
        std::reverse(path.begin(), path.end());
        Value result = stdlibArray();
        for (const int index : path) stdlibArrayAppend(result, static_cast<std::int64_t>(index));
        return box(result);
    }
    if ((op == "path_length" || op == "yol_uzunlugu") && argc == 2) {
        const auto nodes = ocValues(argument(args, 0));
        const auto path = ocValues(argument(args, 1));
        double length = 0.0;
        for (std::size_t i = 1; i < path.size(); ++i) {
            const double indexA = numberOf(path[i-1]), indexB = numberOf(path[i]);
            if (!std::isfinite(indexA) || !std::isfinite(indexB) || indexA < 0 || indexB < 0 || std::floor(indexA) != indexA || std::floor(indexB) != indexB) return box(-1.0);
            const auto a = static_cast<std::size_t>(indexA);
            const auto b = static_cast<std::size_t>(indexB);
            if (a >= nodes.size() || b >= nodes.size()) return box(-1.0);
            const double dx = numberOf(ocField(nodes[a], "x")) - numberOf(ocField(nodes[b], "x"));
            const double dy = numberOf(ocField(nodes[a], "y")) - numberOf(ocField(nodes[b], "y"));
            const double dz = numberOf(ocField(nodes[a], "z")) - numberOf(ocField(nodes[b], "z"));
            length += std::sqrt(dx*dx + dy*dy + dz*dz);
        }
        return box(length);
    }
    writeStderr("PRCYTERAS: NavMesh3D."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* pathfindingCall(const std::string& op, void** args, int argc) {
    if ((op == "astar" || op == "dijkstra" || op == "bfs" || op == "dfs" || op == "yol_bul") && argc >= 5) {
        const OcGrid grid = ocGridParse(argument(args, 0));
        return box(ocPathValue(ocSearchPath(grid, argumentInt(args,1), argumentInt(args,2), argumentInt(args,3), argumentInt(args,4),
            op == "bfs" || op == "dfs" ? "bfs" : (op == "dijkstra" ? "dijkstra" : "astar")), grid));
    }
    if ((op == "flow_field" || op == "akis_alani") && argc >= 3) {
        const OcGrid grid = ocGridParse(argument(args, 0));
        const int gx = argumentInt(args,1), gy = argumentInt(args,2);
        std::vector<double> distance(static_cast<std::size_t>(grid.width) * grid.height, std::numeric_limits<double>::infinity());
        if (ocGridWalk(grid, gx, gy)) {
            using Entry = std::pair<double,int>;
            std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
            distance[static_cast<std::size_t>(gy) * grid.width + gx] = 0.0;
            open.push({0.0, gy * grid.width + gx});
            const int dx[4] = {1,-1,0,0}, dy[4] = {0,0,1,-1};
            while (!open.empty()) {
                const int current = open.top().second; open.pop();
                const int cx = current % grid.width, cy = current / grid.width;
                for (int dir = 0; dir < 4; ++dir) {
                    const int nx = cx + dx[dir], ny = cy + dy[dir];
                    if (!ocGridWalk(grid, nx, ny)) continue;
                    const int next = ny * grid.width + nx;
                    const double candidate = distance[static_cast<std::size_t>(current)] + 1.0;
                    if (candidate < distance[static_cast<std::size_t>(next)]) { distance[static_cast<std::size_t>(next)] = candidate; open.push({candidate, next}); }
                }
            }
        }
        Value out = stdlibArray();
        for (const double value : distance) stdlibArrayAppend(out, std::isfinite(value) ? value : -1.0);
        return box(out);
    }
    if ((op == "line_of_sight" || op == "gorus_hatti") && argc >= 5) {
        const OcGrid grid = ocGridParse(argument(args, 0));
        int x0 = argumentInt(args,1), y0 = argumentInt(args,2);
        const int x1 = argumentInt(args,3), y1 = argumentInt(args,4);
        const int dx = std::abs(x1-x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1-y0), sy = y0 < y1 ? 1 : -1;
        int error = dx + dy; bool visible = true;
        while (true) {
            if (!ocGridWalk(grid, x0, y0)) { visible = false; break; }
            if (x0 == x1 && y0 == y1) break;
            const int twice = 2 * error;
            if (twice >= dy) { error += dy; x0 += sx; }
            if (twice <= dx) { error += dx; y0 += sy; }
        }
        return box(visible);
    }
    if ((op == "path_length" || op == "yol_uzunlugu") && argc >= 1) {
        const auto points = ocValues(argument(args,0));
        double length = 0.0;
        for (std::size_t i = 1; i < points.size(); ++i) {
            const double dx = numberOf(ocField(points[i],"x")) - numberOf(ocField(points[i-1],"x"));
            const double dy = numberOf(ocField(points[i],"y")) - numberOf(ocField(points[i-1],"y"));
            length += std::sqrt(dx*dx + dy*dy);
        }
        return box(length);
    }
    writeStderr("PRCYTERAS: Pathfinding."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - behavior-tree
bool ocBtCompare(const Value& left, const std::string& compare, const Value& right) {
    if (compare == "==") return binary("==", left, right) == Value(true);
    if (compare == "!=") return binary("!=", left, right) == Value(true);
    if (compare == ">") return numberOf(left) > numberOf(right);
    if (compare == "<") return numberOf(left) < numberOf(right);
    if (compare == ">=") return numberOf(left) >= numberOf(right);
    if (compare == "<=") return numberOf(left) <= numberOf(right);
    if (compare == "contains") return stringOf(left).find(stringOf(right)) != std::string::npos;
    return false;
}
bool ocBtTick(const Value& node, Value& blackboard) {
    const std::string type = stringOf(ocField(node, "type"));
    if (type == "sequence" || type == "selector") {
        const bool selector = type == "selector";
        for (const auto& child : ocValues(ocField(node, "children"))) {
            const bool ok = ocBtTick(child, blackboard);
            if (selector && ok) return true;
            if (!selector && !ok) return false;
        }
        return !selector;
    }
    if (type == "inverter") { const auto children = ocValues(ocField(node, "children")); return !children.empty() ? !ocBtTick(children[0], blackboard) : false; }
    if (type == "succeed") return true;
    if (type == "fail") return false;
    if (type == "condition") {
        const std::string key = stringOf(ocField(node, "key"));
        const Value left = ocField(blackboard, key);
        return ocBtCompare(left, stringOf(ocField(node, "op")), ocField(node, "value"));
    }
    if (type == "set") { ocSetField(blackboard, stringOf(ocField(node, "key")), ocField(node, "value")); return true; }
    if (type == "increment") {
        const std::string key = stringOf(ocField(node, "key"));
        ocSetField(blackboard, key, static_cast<std::int64_t>(numberOf(ocField(blackboard, key))) + 1);
        return true;
    }
    if (type == "action") {
        const std::string action = stringOf(ocField(node, "name"));
        if (action == "clear") { blackboard = stdlibObject("Blackboard"); return true; }
        return truthy(ocField(node, "result"));
    }
    return false;
}
void* behaviorTreeCall(const std::string& op, void** args, int argc) {
    if ((op == "tick" || op == "calistir") && argc >= 1) {
        Value blackboard = argc >= 2 ? argument(args,1) : stdlibObject("Blackboard");
        const bool ok = ocBtTick(argument(args,0), blackboard);
        Value result = stdlibObject("BtResult");
        ocSetField(result, "status", std::string(ok ? "success" : "failure"));
        ocSetField(result, "success", ok);
        ocSetField(result, "blackboard", blackboard);
        return box(result);
    }
    if ((op == "sequence" || op == "dizi") && argc >= 1) { Value node = stdlibObject("BtNode"); ocSetField(node,"type",std::string("sequence")); ocSetField(node,"children",argument(args,0)); return box(node); }
    if ((op == "selector" || op == "secici") && argc >= 1) { Value node = stdlibObject("BtNode"); ocSetField(node,"type",std::string("selector")); ocSetField(node,"children",argument(args,0)); return box(node); }
    if ((op == "condition" || op == "kosul") && argc >= 3) { Value node = stdlibObject("BtNode"); ocSetField(node,"type",std::string("condition")); ocSetField(node,"key",argument(args,0)); ocSetField(node,"op",argument(args,1)); ocSetField(node,"value",argument(args,2)); return box(node); }
    if ((op == "set" || op == "ata") && argc >= 2) { Value node = stdlibObject("BtNode"); ocSetField(node,"type",std::string("set")); ocSetField(node,"key",argument(args,0)); ocSetField(node,"value",argument(args,1)); return box(node); }
    if ((op == "blackboard" || op == "karatahta") && argc == 0) return box(stdlibObject("Blackboard"));
    writeStderr("PRCYTERAS: BehaviorTree."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - ecs
struct OcEcs {
    int next = 1;
    std::set<int> alive;
    std::map<int, std::map<std::string, Value>> components;
    std::mutex mutex;
};
std::map<std::string, OcEcs>& ocEcsWorlds() { static std::map<std::string, OcEcs> worlds; return worlds; }
OcEcs& ocEcsWorld(const std::string& name) { return ocEcsWorlds()[name.empty() ? std::string("default") : name]; }
void* ecsCall(const std::string& op, void** args, int argc) {
    auto& worlds = ocEcsWorlds();
    if ((op == "world" || op == "dunya") && argc >= 1) { std::lock_guard<std::mutex> lock(ocEcsWorld(argumentString(args,0)).mutex); return box(argumentString(args,0)); }
    if ((op == "create" || op == "olustur") && argc >= 1) {
        OcEcs& world = ocEcsWorld(argumentString(args,0)); std::lock_guard<std::mutex> lock(world.mutex);
        const int entity = world.next++; world.alive.insert(entity); return box(static_cast<std::int64_t>(entity));
    }
    if ((op == "destroy" || op == "yok_et") && argc >= 2) {
        OcEcs& world = ocEcsWorld(argumentString(args,0)); std::lock_guard<std::mutex> lock(world.mutex);
        const int entity = argumentInt(args,1); world.alive.erase(entity); world.components.erase(entity); return box(true);
    }
    if ((op == "alive" || op == "canli") && argc >= 2) {
        OcEcs& world = ocEcsWorld(argumentString(args,0)); std::lock_guard<std::mutex> lock(world.mutex);
        return box(world.alive.count(argumentInt(args,1)) != 0);
    }
    if ((op == "set" || op == "ata") && argc >= 4) {
        OcEcs& world = ocEcsWorld(argumentString(args,0)); std::lock_guard<std::mutex> lock(world.mutex);
        const int entity = argumentInt(args,1); if (!world.alive.count(entity)) return box(false);
        world.components[entity][argumentString(args,2)] = argument(args,3); return box(true);
    }
    if ((op == "get" || op == "al") && argc >= 3) {
        OcEcs& world = ocEcsWorld(argumentString(args,0)); std::lock_guard<std::mutex> lock(world.mutex);
        const auto found = world.components.find(argumentInt(args,1));
        if (found == world.components.end()) return box(Value{});
        const auto item = found->second.find(argumentString(args,2));
        return box(item == found->second.end() ? Value{} : item->second);
    }
    if ((op == "has" || op == "var_mi") && argc >= 3) {
        OcEcs& world = ocEcsWorld(argumentString(args,0)); std::lock_guard<std::mutex> lock(world.mutex);
        const auto found = world.components.find(argumentInt(args,1));
        return box(found != world.components.end() && found->second.count(argumentString(args,2)) != 0);
    }
    if ((op == "remove" || op == "kaldir") && argc >= 3) {
        OcEcs& world = ocEcsWorld(argumentString(args,0)); std::lock_guard<std::mutex> lock(world.mutex);
        const auto found = world.components.find(argumentInt(args,1));
        if (found != world.components.end()) found->second.erase(argumentString(args,2));
        return box(true);
    }
    if ((op == "query" || op == "sorgula") && argc >= 2) {
        OcEcs& world = ocEcsWorld(argumentString(args,0)); std::lock_guard<std::mutex> lock(world.mutex);
        const auto required = ocStringList(argument(args,1)); Value out = stdlibArray();
        for (const int entity : world.alive) {
            const auto found = world.components.find(entity);
            if (found == world.components.end()) continue;
            bool matches = true;
            for (const auto& name : required) if (!found->second.count(name)) { matches = false; break; }
            if (!matches) continue;
            Value row = stdlibObject("EntityView"); ocSetField(row,"entity",static_cast<std::int64_t>(entity));
            for (const auto& name : required) ocSetField(row, name, found->second.at(name));
            stdlibArrayAppend(out, row);
        }
        return box(out);
    }
    if ((op == "count" || op == "sayi") && argc >= 1) { OcEcs& world = ocEcsWorld(argumentString(args,0)); std::lock_guard<std::mutex> lock(world.mutex); return box(static_cast<std::int64_t>(world.alive.size())); }
    if ((op == "clear" || op == "temizle") && argc >= 1) { OcEcs& world = ocEcsWorld(argumentString(args,0)); std::lock_guard<std::mutex> lock(world.mutex); world.alive.clear(); world.components.clear(); return box(true); }
    writeStderr("PRCYTERAS: Ecs."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - rate-limiter
struct OcBucket { double capacity=1, rate=1, tokens=1; std::int64_t last=0; std::mutex mutex; };
std::map<std::string, OcBucket>& ocBuckets() { static std::map<std::string, OcBucket> buckets; return buckets; }
void* rateLimiterCall(const std::string& op, void** args, int argc) {
    auto& buckets = ocBuckets();
    if ((op == "create" || op == "olustur") && argc >= 3) {
        const std::string key = argumentString(args,0); auto& bucket = buckets[key]; std::lock_guard<std::mutex> lock(bucket.mutex);
        bucket.capacity = std::max(1.0, ocNum(args,1)); bucket.rate = std::max(0.0, ocNum(args,2)); bucket.tokens = bucket.capacity; bucket.last = ocNowMillis();
        return box(true);
    }
    auto refill = [&](OcBucket& bucket) {
        const std::int64_t now = ocNowMillis(); const double elapsed = static_cast<double>(now - bucket.last) / 1000.0;
        bucket.last = now; bucket.tokens = std::min(bucket.capacity, bucket.tokens + elapsed * bucket.rate);
    };
    if ((op == "allow" || op == "izin") && argc >= 1) {
        auto found = buckets.find(argumentString(args,0)); if (found == buckets.end()) return box(true);
        std::lock_guard<std::mutex> lock(found->second.mutex); refill(found->second);
        const double cost = argc >= 2 ? std::max(0.0, ocNum(args,1)) : 1.0;
        if (found->second.tokens + 1e-9 < cost) return box(false);
        found->second.tokens -= cost; return box(true);
    }
    if ((op == "tokens" || op == "jeton") && argc >= 1) {
        auto found = buckets.find(argumentString(args,0)); if (found == buckets.end()) return box(0.0);
        std::lock_guard<std::mutex> lock(found->second.mutex); refill(found->second); return box(found->second.tokens);
    }
    if ((op == "reset" || op == "sifirla") && argc >= 1) { auto found = buckets.find(argumentString(args,0)); if (found != buckets.end()) { std::lock_guard<std::mutex> lock(found->second.mutex); found->second.tokens = found->second.capacity; found->second.last = ocNowMillis(); } return box(true); }
    if ((op == "remove" || op == "kaldir") && argc >= 1) { buckets.erase(argumentString(args,0)); return box(true); }
    writeStderr("PRCYTERAS: RateLimiter."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// Poll-based scheduler: callers invoke tick() from their main loop; callbacks are intentionally not run on hidden worker threads.
struct OcScheduledTask { std::int64_t dueSteadyMs=0, intervalMs=0; bool repeating=false; };
std::map<std::string, OcScheduledTask>& ocScheduledTasks() { static std::map<std::string, OcScheduledTask> tasks; return tasks; }
std::mutex& ocScheduledTasksMutex() { static std::mutex mutex; return mutex; }
std::int64_t ocSteadyMillis() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
void* schedulerCall(const std::string& op, void** args, int argc) {
    auto& tasks = ocScheduledTasks();
    auto add = [&](const std::string& id, std::int64_t delay, std::int64_t interval) -> bool {
        if (id.empty() || delay < 0 || interval < 0) return false;
        const std::int64_t now = ocSteadyMillis();
        if (delay > std::numeric_limits<std::int64_t>::max() - now) return false;
        std::lock_guard<std::mutex> lock(ocScheduledTasksMutex());
        tasks[id] = OcScheduledTask{now + delay, interval, interval > 0};
        return true;
    };
    if ((op == "after" || op == "sonra") && argc == 2) return box(add(argumentString(args,0), argumentInt(args,1), 0));
    if ((op == "every" || op == "her" || op == "interval") && argc == 2) {
        const std::int64_t interval = argumentInt(args,1);
        return box(interval > 0 && add(argumentString(args,0), interval, interval));
    }
    if ((op == "at" || op == "unix_ms") && argc == 2) {
        const std::int64_t target = argumentInt(args,1);
        const std::int64_t wallNow = ocNowMillis();
        const std::int64_t delay = target <= wallNow ? 0 : target - wallNow;
        return box(add(argumentString(args,0), delay, 0));
    }
    if ((op == "tick" || op == "poll" || op == "guncelle") && argc == 0) {
        std::vector<std::string> dueIds;
        const std::int64_t now = ocSteadyMillis();
        std::lock_guard<std::mutex> lock(ocScheduledTasksMutex());
        for (auto it = tasks.begin(); it != tasks.end();) {
            auto& task = it->second;
            if (task.dueSteadyMs > now) { ++it; continue; }
            dueIds.push_back(it->first);
            if (!task.repeating) { it = tasks.erase(it); continue; }
            const std::int64_t elapsed = now - task.dueSteadyMs;
            const std::int64_t steps = elapsed / task.intervalMs + 1;
            if (steps > (std::numeric_limits<std::int64_t>::max() - task.dueSteadyMs) / task.intervalMs)
                task.dueSteadyMs = std::numeric_limits<std::int64_t>::max();
            else task.dueSteadyMs += steps * task.intervalMs;
            ++it;
        }
        return box(ocStringArray(dueIds));
    }
    if ((op == "cancel" || op == "kaldir") && argc == 1) {
        std::lock_guard<std::mutex> lock(ocScheduledTasksMutex());
        return box(tasks.erase(argumentString(args,0)) != 0);
    }
    if ((op == "clear" || op == "temizle") && argc == 0) {
        std::lock_guard<std::mutex> lock(ocScheduledTasksMutex()); tasks.clear(); return box(true);
    }
    if ((op == "count" || op == "sayi") && argc == 0) {
        std::lock_guard<std::mutex> lock(ocScheduledTasksMutex()); return box(static_cast<std::int64_t>(tasks.size()));
    }
    if ((op == "next_delay_ms" || op == "next_delay" || op == "sonraki_ms") && argc == 0) {
        std::lock_guard<std::mutex> lock(ocScheduledTasksMutex());
        if (tasks.empty()) return box(static_cast<std::int64_t>(-1));
        const std::int64_t now = ocSteadyMillis(); std::int64_t due = std::numeric_limits<std::int64_t>::max();
        for (const auto& entry : tasks) due = std::min(due, entry.second.dueSteadyMs);
        return box(std::max<std::int64_t>(0, due - now));
    }
    writeStderr("PRCYTERAS: Scheduler."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - localization
std::map<std::string, std::map<std::string, std::string>>& ocCatalogs() { static std::map<std::string, std::map<std::string, std::string>> catalogs; return catalogs; }
std::string& ocLanguage() { static std::string language = "en"; return language; }
void ocFlattenCatalog(const Value& value, const std::string& prefix, std::map<std::string, std::string>& out) {
    if (auto object = ocObjectOf(value)) {
        for (const auto& entry : object->dynamicFields) ocFlattenCatalog(entry.second, prefix.empty() ? entry.first : prefix + "." + entry.first, out);
    } else if (!std::holds_alternative<std::monostate>(value)) {
        out[prefix] = stringOf(value);
    }
}
void* localizationCall(const std::string& op, void** args, int argc) {
    if ((op == "load" || op == "yukle") && argc >= 2) {
        const std::string language = argumentString(args,0);
        bool ok = false; const Value document = ocParseJson(argumentString(args,1), ok);
        if (!ok) return box(false);
        auto& catalog = ocCatalogs()[language]; catalog.clear(); ocFlattenCatalog(document, "", catalog); return box(true);
    }
    if ((op == "language" || op == "dil") && argc >= 1) { ocLanguage() = argumentString(args,0); return box(true); }
    if ((op == "current" || op == "mevcut") && argc == 0) return box(ocLanguage());
    if ((op == "languages" || op == "diller") && argc == 0) { std::vector<std::string> names; for (const auto& entry : ocCatalogs()) names.push_back(entry.first); return box(ocStringArray(names)); }
    if ((op == "t" || op == "cevir") && argc >= 1) {
        const std::string key = argumentString(args,0);
        const std::string language = argc >= 2 ? argumentString(args,1) : ocLanguage();
        auto found = ocCatalogs().find(language);
        if (found == ocCatalogs().end()) found = ocCatalogs().find("en");
        if (found == ocCatalogs().end()) return box(key);
        const auto item = found->second.find(key);
        return box(item == found->second.end() ? key : item->second);
    }
    if ((op == "format" || op == "bicimle") && argc >= 2) {
        const std::string key = argumentString(args,0); const Value params = argument(args,1);
        const std::string language = argc >= 3 ? argumentString(args,2) : ocLanguage();
        std::string text = key;
        auto found = ocCatalogs().find(language); if (found == ocCatalogs().end()) found = ocCatalogs().find("en");
        if (found != ocCatalogs().end()) { const auto item = found->second.find(key); if (item != found->second.end()) text = item->second; }
        if (auto object = ocObjectOf(params)) {
            for (const auto& entry : object->dynamicFields) {
                const std::string token = "{" + entry.first + "}"; std::size_t at = 0;
                while ((at = text.find(token, at)) != std::string::npos) { text.replace(at, token.size(), stringOf(entry.second)); at += stringOf(entry.second).size(); }
            }
        }
        return box(text);
    }
    if ((op == "plural" || op == "cogul") && argc >= 2) {
        const double count = ocNum(args,1); const std::string language = argc >= 3 ? argumentString(args,2) : ocLanguage();
        std::string key = argumentString(args,0);
        if (language == "tr") return box(key + (count == 1.0 ? "" : "ler"));
        return box(key + (count == 1.0 ? "" : "s"));
    }
    if ((op == "has" || op == "var_mi") && argc >= 1) {
        const std::string key = argumentString(args,0); const std::string language = argc >= 2 ? argumentString(args,1) : ocLanguage();
        auto found = ocCatalogs().find(language); return box(found != ocCatalogs().end() && found->second.count(key) != 0);
    }
    writeStderr("PRCYTERAS: Localization."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - tween
double ocEase(const std::string& name, double t) {
    t = std::clamp(t, 0.0, 1.0);
    if (name == "linear") return t;
    if (name == "in_quad") return t*t;
    if (name == "out_quad") return t*(2-t);
    if (name == "in_out_quad") return t < 0.5 ? 2*t*t : -1+(4-2*t)*t;
    if (name == "in_cubic") return t*t*t;
    if (name == "out_cubic") return 1-std::pow(1-t,3);
    if (name == "in_out_cubic") return t < 0.5 ? 4*t*t*t : 1-std::pow(-2*t+2,3)/2;
    if (name == "out_back") { const double c1=1.70158, c3=c1+1; return 1+c3*std::pow(t-1,3)+c1*std::pow(t-1,2); }
    if (name == "out_bounce") {
        const double n1=7.5625, d1=2.75;
        if (t < 1/d1) return n1*t*t;
        if (t < 2/d1) { t -= 1.5/d1; return n1*t*t+0.75; }
        if (t < 2.5/d1) { t -= 2.25/d1; return n1*t*t+0.9375; }
        t -= 2.625/d1; return n1*t*t+0.984375;
    }
    if (name == "out_elastic") { if (t==0||t==1) return t; const double c4=(2*3.14159265358979323846)/3; return std::pow(2,-10*t)*std::sin((t*10-0.75)*c4)+1; }
    return t;
}
struct OcTween { double from=0, to=1, duration=1, elapsed=0; std::string easing="linear"; bool loop=false, done=false; };
std::map<std::string, OcTween>& ocTweens() { static std::map<std::string, OcTween> tweens; return tweens; }
void* tweenCall(const std::string& op, void** args, int argc) {
    if ((op == "ease" || op == "yumusat") && argc >= 2) return box(ocEase(argumentString(args,0), ocNum(args,1)));
    if ((op == "create" || op == "olustur") && argc >= 4) {
        OcTween tween; tween.from = ocNum(args,0); tween.to = ocNum(args,1); tween.duration = std::max(1e-6, ocNum(args,2)); tween.easing = argumentString(args,3);
        tween.loop = argc >= 5 ? truthy(argument(args,4)) : false; ocTweens()[argc >= 6 ? argumentString(args,5) : std::string("default")] = tween; return box(true);
    }
    if ((op == "update" || op == "guncelle") && argc >= 1) {
        auto& tweens = ocTweens(); auto found = tweens.find(argumentString(args,0)); if (found == tweens.end()) return box(0.0);
        found->second.elapsed += argc >= 2 ? std::max(0.0, ocNum(args,1)) : 0.016;
        if (found->second.elapsed >= found->second.duration) { if (found->second.loop) found->second.elapsed = std::fmod(found->second.elapsed, found->second.duration); else { found->second.elapsed = found->second.duration; found->second.done = true; } }
        const double t = found->second.elapsed / found->second.duration;
        return box(found->second.from + (found->second.to - found->second.from) * ocEase(found->second.easing, t));
    }
    if ((op == "value" || op == "deger") && argc >= 1) {
        auto& tweens = ocTweens(); auto found = tweens.find(argumentString(args,0)); if (found == tweens.end()) return box(0.0);
        const double t = std::clamp(found->second.elapsed / found->second.duration, 0.0, 1.0);
        return box(found->second.from + (found->second.to - found->second.from) * ocEase(found->second.easing, t));
    }
    if ((op == "done" || op == "bitti") && argc >= 1) { auto& tweens = ocTweens(); auto found = tweens.find(argumentString(args,0)); return box(found != tweens.end() && found->second.done); }
    if ((op == "reset" || op == "sifirla") && argc >= 1) { auto& tweens = ocTweens(); auto found = tweens.find(argumentString(args,0)); if (found != tweens.end()) { found->second.elapsed = 0; found->second.done = false; } return box(true); }
    writeStderr("PRCYTERAS: Tween."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - camera-2d
struct OcCamera2D { double x=0,y=0,zoom=1,rotation=0,shake=0,shakeTime=0; int width=800,height=600; };
std::map<std::string, OcCamera2D>& ocCameras() { static std::map<std::string, OcCamera2D> cameras; return cameras; }
Value ocScreenPoint(double x, double y) { Value p = stdlibObject("ScreenPoint"); ocSetField(p,"x",x); ocSetField(p,"y",y); return p; }
void* camera2dCall(const std::string& op, void** args, int argc) {
    auto& cameras = ocCameras();
    if ((op == "create" || op == "olustur") && argc >= 2) {
        OcCamera2D camera; camera.x = ocNum(args,0); camera.y = ocNum(args,1); camera.zoom = argc >= 3 ? ocNum(args,2) : 1.0;
        camera.width = argc >= 4 ? argumentInt(args,3) : 800; camera.height = argc >= 5 ? argumentInt(args,4) : 600;
        cameras[argc >= 6 ? argumentString(args,5) : std::string("main")] = camera; return box(true);
    }
    if ((op == "world_to_screen" || op == "dunyadan_ekrana") && argc >= 2) {
        const std::string name = argc >= 3 ? argumentString(args,2) : std::string("main");
        auto found = cameras.find(name); if (found == cameras.end()) return box(ocScreenPoint(0,0));
        const auto& cam = found->second;
        const double dx = (ocNum(args,0) - cam.x) * cam.zoom, dy = (ocNum(args,1) - cam.y) * cam.zoom;
        const double c = std::cos(cam.rotation), s = std::sin(cam.rotation);
        return box(ocScreenPoint(cam.width/2.0 + dx*c - dy*s, cam.height/2.0 + dx*s + dy*c));
    }
    if ((op == "screen_to_world" || op == "ekrandan_dunyaya") && argc >= 2) {
        const std::string name = argc >= 3 ? argumentString(args,2) : std::string("main");
        auto found = cameras.find(name); if (found == cameras.end()) return box(ocScreenPoint(0,0));
        const auto& cam = found->second;
        double dx = (ocNum(args,0) - cam.width/2.0) / cam.zoom, dy = (ocNum(args,1) - cam.height/2.0) / cam.zoom;
        const double c = std::cos(-cam.rotation), s = std::sin(-cam.rotation);
        return box(ocScreenPoint(cam.x + dx*c - dy*s, cam.y + dx*s + dy*c));
    }
    if ((op == "follow" || op == "takip") && argc >= 2) {
        const std::string name = argc >= 3 ? argumentString(args,2) : std::string("main");
        auto& cam = cameras[name]; const double t = argc >= 4 ? std::clamp(ocNum(args,3), 0.0, 1.0) : 0.1;
        cam.x += (ocNum(args,0) - cam.x) * t; cam.y += (ocNum(args,1) - cam.y) * t; return box(true);
    }
    if ((op == "shake" || op == "sarsinti") && argc >= 1) {
        const std::string name = argc >= 2 ? argumentString(args,1) : std::string("main");
        auto& cam = cameras[name]; cam.shake = std::max(0.0, ocNum(args,0)); cam.shakeTime = argc >= 3 ? ocNum(args,2) : 0.5; return box(true);
    }
    if ((op == "update" || op == "guncelle") && argc >= 1) {
        const std::string name = argc >= 2 ? argumentString(args,1) : std::string("main");
        auto& cam = cameras[name]; const double dt = std::max(0.0, ocNum(args,0));
        if (cam.shakeTime > 0) { cam.shakeTime = std::max(0.0, cam.shakeTime - dt); if (cam.shakeTime == 0) cam.shake = 0; }
        return box(true);
    }
    if ((op == "offset" || op == "kayma") && argc >= 0) {
        const std::string name = argc >= 1 ? argumentString(args,0) : std::string("main");
        auto& cam = cameras[name];
        const double amount = cam.shake * (cam.shakeTime > 0 ? cam.shakeTime : 0);
        return box(ocScreenPoint(std::sin(ocNowMicros()*0.05)*amount, std::cos(ocNowMicros()*0.07)*amount));
    }
    if ((op == "zoom" || op == "yakinlastir") && argc >= 1) { const std::string name = argc >= 2 ? argumentString(args,1) : std::string("main"); cameras[name].zoom = std::max(0.01, ocNum(args,0)); return box(true); }
    if ((op == "get" || op == "al") && argc >= 1) {
        auto found = cameras.find(argumentString(args,0)); if (found == cameras.end()) return box(Value{});
        Value out = stdlibObject("Camera2D"); ocSetField(out,"x",found->second.x); ocSetField(out,"y",found->second.y); ocSetField(out,"zoom",found->second.zoom);
        ocSetField(out,"rotation",found->second.rotation); ocSetField(out,"width",static_cast<std::int64_t>(found->second.width)); ocSetField(out,"height",static_cast<std::int64_t>(found->second.height)); return box(out);
    }
    writeStderr("PRCYTERAS: Camera2D."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - particle-2d
struct OcParticle { double x=0,y=0,vx=0,vy=0,life=1,maxLife=1,size=2; };
struct OcEmitter { std::vector<OcParticle> particles; double rate=0, accumulator=0, gravity=0, drag=0; };
std::map<std::string, OcEmitter>& ocEmitters() { static std::map<std::string, OcEmitter> emitters; return emitters; }
void* particle2dCall(const std::string& op, void** args, int argc) {
    auto& emitters = ocEmitters();
    if ((op == "create" || op == "olustur") && argc >= 2) {
        OcEmitter emitter; emitter.rate = std::max(0.0, ocNum(args,0)); emitter.gravity = argc >= 3 ? ocNum(args,2) : 0.0; emitter.drag = argc >= 4 ? ocNum(args,3) : 0.0;
        emitters[argc >= 5 ? argumentString(args,4) : std::string("default")] = emitter; return box(true);
    }
    if ((op == "burst" || op == "patlama") && argc >= 1) {
        const std::string name = argc >= 2 ? argumentString(args,1) : std::string("default");
        auto& emitter = emitters[name]; const int count = std::clamp(argumentInt(args,0), 0, 100000);
        for (int i = 0; i < count; ++i) {
            OcParticle p; p.x = 0; p.y = 0; const double angle = (static_cast<double>(std::rand()) / RAND_MAX) * 6.283185307179586;
            const double speed = 20.0 + (static_cast<double>(std::rand()) / RAND_MAX) * 80.0;
            p.vx = std::cos(angle)*speed; p.vy = std::sin(angle)*speed; p.life = p.maxLife = 0.5 + (static_cast<double>(std::rand()) / RAND_MAX) * 1.5;
            p.size = 1.0 + (static_cast<double>(std::rand()) / RAND_MAX) * 3.0; emitter.particles.push_back(p);
        }
        return box(true);
    }
    if ((op == "update" || op == "guncelle") && argc >= 1) {
        const std::string name = argc >= 2 ? argumentString(args,1) : std::string("default");
        auto& emitter = emitters[name]; const double dt = std::max(0.0, ocNum(args,0));
        emitter.accumulator += emitter.rate * dt;
        while (emitter.accumulator >= 1.0) {
            emitter.accumulator -= 1.0; OcParticle p; const double angle = (static_cast<double>(std::rand()) / RAND_MAX) * 6.283185307179586;
            const double speed = 20.0 + (static_cast<double>(std::rand()) / RAND_MAX) * 60.0; p.vx = std::cos(angle)*speed; p.vy = std::sin(angle)*speed;
            p.life = p.maxLife = 0.6 + (static_cast<double>(std::rand()) / RAND_MAX) * 1.2; p.size = 1.0 + (static_cast<double>(std::rand()) / RAND_MAX) * 2.0; emitter.particles.push_back(p);
        }
        for (auto& p : emitter.particles) { p.vy += emitter.gravity * dt; p.vx -= p.vx * emitter.drag * dt; p.vy -= p.vy * emitter.drag * dt; p.x += p.vx*dt; p.y += p.vy*dt; p.life -= dt; }
        emitter.particles.erase(std::remove_if(emitter.particles.begin(), emitter.particles.end(), [](const OcParticle& p){ return p.life <= 0; }), emitter.particles.end());
        return box(static_cast<std::int64_t>(emitter.particles.size()));
    }
    if ((op == "count" || op == "sayi") && argc >= 0) { const std::string name = argc >= 1 ? argumentString(args,0) : std::string("default"); return box(static_cast<std::int64_t>(emitters[name].particles.size())); }
    if ((op == "particles" || op == "parcaciklar") && argc >= 0) {
        const std::string name = argc >= 1 ? argumentString(args,0) : std::string("default"); Value out = stdlibArray();
        for (const auto& p : emitters[name].particles) { Value item = stdlibObject("Particle"); ocSetField(item,"x",p.x); ocSetField(item,"y",p.y); ocSetField(item,"vx",p.vx); ocSetField(item,"vy",p.vy); ocSetField(item,"life",p.life); ocSetField(item,"size",p.size); stdlibArrayAppend(out,item); }
        return box(out);
    }
    if ((op == "clear" || op == "temizle") && argc >= 0) { const std::string name = argc >= 1 ? argumentString(args,0) : std::string("default"); emitters[name].particles.clear(); return box(true); }
    writeStderr("PRCYTERAS: Particle2D."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - lighting-2d
struct OcSegment { double x1,y1,x2,y2; };
std::vector<OcSegment> ocSegments(const Value& value) {
    std::vector<OcSegment> out;
    for (const auto& item : ocValues(value)) out.push_back({numberOf(ocField(item,"x1")),numberOf(ocField(item,"y1")),numberOf(ocField(item,"x2")),numberOf(ocField(item,"y2"))});
    return out;
}
bool ocRayHit(double ox, double oy, double dx, double dy, const OcSegment& s, double& t) {
    const double sx = s.x2 - s.x1, sy = s.y2 - s.y1;
    const double det = dx * -sy - dy * -sx;
    if (std::fabs(det) < 1e-12) return false;
    const double rx = s.x1 - ox, ry = s.y1 - oy;
    const double u = (rx * -sy - ry * -sx) / det;
    const double v = (dx * ry - dy * rx) / det;
    if (u < 0 || v < 0 || v > 1) return false;
    t = u; return true;
}
void* lighting2dCall(const std::string& op, void** args, int argc) {
    if ((op == "visibility" || op == "gorunurluk") && argc >= 3) {
        const double lx = ocNum(args,0), ly = ocNum(args,1); const auto segments = ocSegments(argument(args,2));
        const double radius = argc >= 4 ? std::max(0.0, ocNum(args,3)) : 1000.0;
        std::vector<double> angles;
        for (const auto& s : segments) { angles.push_back(std::atan2(s.y1-ly, s.x1-lx)); angles.push_back(std::atan2(s.y2-ly, s.x2-lx)); }
        for (int i = 0; i < 64; ++i) angles.push_back(i * 6.283185307179586 / 64.0);
        std::vector<double> uniqueAngles;
        for (double angle : angles) { uniqueAngles.push_back(angle - 0.0001); uniqueAngles.push_back(angle); uniqueAngles.push_back(angle + 0.0001); }
        Value polygon = stdlibArray();
        for (const double angle : uniqueAngles) {
            const double dx = std::cos(angle), dy = std::sin(angle); double best = radius;
            for (const auto& s : segments) { double t = 0; if (ocRayHit(lx,ly,dx,dy,s,t)) best = std::min(best, t); }
            Value point = stdlibObject("LightPoint"); ocSetField(point,"x",lx+dx*best); ocSetField(point,"y",ly+dy*best); ocSetField(point,"angle",angle); stdlibArrayAppend(polygon, point);
        }
        return box(polygon);
    }
    if ((op == "attenuation" || op == "zayiflama") && argc >= 2) {
        const double distance = std::max(0.0, ocNum(args,0)), radius = std::max(1e-6, ocNum(args,1));
        return box(std::clamp(1.0 - distance/radius, 0.0, 1.0));
    }
    if ((op == "blend" || op == "karistir") && argc >= 3) {
        const double light = std::clamp(ocNum(args,0),0.0,1.0), base = std::clamp(ocNum(args,1),0.0,1.0), ambient = std::clamp(ocNum(args,2),0.0,1.0);
        return box(std::clamp(base * (ambient + light * (1.0-ambient)), 0.0, 1.0));
    }
    if ((op == "circle" || op == "cember") && argc >= 3) {
        const double cx = ocNum(args,0), cy = ocNum(args,1), r = std::max(0.0, ocNum(args,2)); const int segments = argc >= 4 ? std::clamp(argumentInt(args,3), 8, 256) : 32;
        Value polygon = stdlibArray();
        for (int i = 0; i < segments; ++i) { const double angle = i * 6.283185307179586 / segments; Value point = stdlibObject("LightPoint"); ocSetField(point,"x",cx+std::cos(angle)*r); ocSetField(point,"y",cy+std::sin(angle)*r); stdlibArrayAppend(polygon,point); }
        return box(polygon);
    }
    writeStderr("PRCYTERAS: Lighting2D."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - physics-2d
struct OcBody2D { int id=0, type=0; double x=0,y=0,vx=0,vy=0,mass=1,invMass=1,restitution=0.4,friction=0.2,radius=10,hw=10,hh=10; };
struct OcWorld2D { std::vector<OcBody2D> bodies; double gravity=9.8; int next=1; };
std::map<std::string, OcWorld2D>& ocPhysics2d() { static std::map<std::string, OcWorld2D> worlds; return worlds; }
void* physics2dCall(const std::string& op, void** args, int argc) {
    auto& worlds = ocPhysics2d();
    if ((op == "world" || op == "dunya") && argc >= 1) {
        const std::string name = argumentString(args,0); OcWorld2D world; world.gravity = argc >= 2 ? ocNum(args,1) : 9.8; worlds[name] = world; return box(true);
    }
    if ((op == "add" || op == "ekle") && argc >= 2) {
        const std::string name = argumentString(args,0); auto& world = worlds[name]; OcBody2D body; body.id = world.next++;
        body.type = argumentString(args,1) == "circle" ? 0 : 1; body.x = argc>=3?ocNum(args,2):0; body.y = argc>=4?ocNum(args,3):0;
        body.mass = argc>=5?std::max(0.0001,ocNum(args,4)):1.0; body.invMass = 1.0/body.mass; body.restitution = argc>=6?std::clamp(ocNum(args,5),0.0,1.0):0.4;
        body.radius = argc>=7?std::max(0.0,ocNum(args,6)):10.0; body.hw = argc>=8?ocNum(args,7):10.0; body.hh = argc>=9?ocNum(args,8):10.0;
        world.bodies.push_back(body); return box(static_cast<std::int64_t>(body.id));
    }
    if ((op == "step" || op == "adim") && argc >= 2) {
        const std::string name = argumentString(args,0); auto found = worlds.find(name); if (found == worlds.end()) return box(false);
        auto& world = found->second; const double dt = std::clamp(ocNum(args,1), 0.0, 0.05);
        for (auto& body : world.bodies) { if (body.invMass == 0) continue; body.vy += world.gravity * dt; body.x += body.vx*dt; body.y += body.vy*dt; }
        for (std::size_t i = 0; i < world.bodies.size(); ++i) for (std::size_t j = i+1; j < world.bodies.size(); ++j) {
            auto& a = world.bodies[i]; auto& b = world.bodies[j];
            if (a.type == 0 && b.type == 0) {
                const double dx = b.x-a.x, dy = b.y-a.y; const double radius = a.radius+b.radius; const double distance = std::sqrt(dx*dx+dy*dy);
                if (distance > 0 && distance < radius) {
                    const double nx = dx/distance, ny = dy/distance, overlap = radius-distance;
                    const double total = a.invMass+b.invMass; if (total == 0) continue;
                    a.x -= nx*overlap*(a.invMass/total); a.y -= ny*overlap*(a.invMass/total);
                    b.x += nx*overlap*(b.invMass/total); b.y += ny*overlap*(b.invMass/total);
                    const double rvx = b.vx-a.vx, rvy = b.vy-a.vy; const double normal = rvx*nx+rvy*ny;
                    if (normal < 0) { const double impulse = -(1+std::min(a.restitution,b.restitution))*normal/total;
                        a.vx -= impulse*nx*a.invMass; a.vy -= impulse*ny*a.invMass; b.vx += impulse*nx*b.invMass; b.vy += impulse*ny*b.invMass; }
                }
            } else if (a.type == 1 && b.type == 1) {
                const double dx = b.x-a.x, dy = b.y-a.y, overlapX = a.hw+b.hw-std::fabs(dx), overlapY = a.hh+b.hh-std::fabs(dy);
                if (overlapX > 0 && overlapY > 0) {
                    const double total = a.invMass+b.invMass; if (total == 0) continue;
                    if (overlapX < overlapY) { const double sign = dx < 0 ? -1.0 : 1.0; a.x -= sign*overlapX*(a.invMass/total); b.x += sign*overlapX*(b.invMass/total); std::swap(a.vx,b.vx); }
                    else { const double sign = dy < 0 ? -1.0 : 1.0; a.y -= sign*overlapY*(a.invMass/total); b.y += sign*overlapY*(b.invMass/total); std::swap(a.vy,b.vy); }
                }
            } else {
                const auto& circle = a.type == 0 ? a : b; auto& box = a.type == 1 ? a : b;
                const double nx = std::clamp(circle.x, box.x-box.hw, box.x+box.hw), ny = std::clamp(circle.y, box.y-box.hh, box.y+box.hh);
                const double dx = circle.x-nx, dy = circle.y-ny, distance = std::sqrt(dx*dx+dy*dy);
                if (distance < circle.radius) {
                    const double total = a.invMass+b.invMass; if (total == 0) continue;
                    const double sign = a.type == 0 ? 1.0 : -1.0;
                    const double push = (circle.radius-distance)/total;
                    a.x += sign*dx/(distance>0?distance:1)*push*a.invMass; a.y += sign*dy/(distance>0?distance:1)*push*a.invMass;
                    b.x -= sign*dx/(distance>0?distance:1)*push*b.invMass; b.y -= sign*dy/(distance>0?distance:1)*push*b.invMass;
                }
            }
        }
        return box(true);
    }
    if ((op == "apply_force" || op == "kuvvet") && argc >= 3) {
        auto found = worlds.find(argumentString(args,0)); if (found == worlds.end()) return box(false);
        for (auto& body : found->second.bodies) if (body.id == argumentInt(args,1)) { body.vx += ocNum(args,2)*body.invMass; body.vy += (argc>=4?ocNum(args,3):0.0)*body.invMass; return box(true); }
        return box(false);
    }
    if ((op == "impulse" || op == "itme") && argc >= 3) {
        auto found = worlds.find(argumentString(args,0)); if (found == worlds.end()) return box(false);
        for (auto& body : found->second.bodies) if (body.id == argumentInt(args,1)) { body.vx += ocNum(args,2)*body.invMass; body.vy += (argc>=4?ocNum(args,3):0.0)*body.invMass; return box(true); }
        return box(false);
    }
    if ((op == "body" || op == "govde") && argc >= 2) {
        auto found = worlds.find(argumentString(args,0)); if (found == worlds.end()) return box(Value{});
        for (const auto& body : found->second.bodies) if (body.id == argumentInt(args,1)) {
            Value out = stdlibObject("Body2D"); ocSetField(out,"id",static_cast<std::int64_t>(body.id)); ocSetField(out,"x",body.x); ocSetField(out,"y",body.y);
            ocSetField(out,"vx",body.vx); ocSetField(out,"vy",body.vy); ocSetField(out,"mass",body.mass); return box(out);
        }
        return box(Value{});
    }
    if ((op == "bodies" || op == "govdeler") && argc >= 1) {
        auto found = worlds.find(argumentString(args,0)); if (found == worlds.end()) return box(stdlibArray());
        Value out = stdlibArray();
        for (const auto& body : found->second.bodies) { Value item = stdlibObject("Body2D"); ocSetField(item,"id",static_cast<std::int64_t>(body.id)); ocSetField(item,"x",body.x); ocSetField(item,"y",body.y); ocSetField(item,"vx",body.vx); ocSetField(item,"vy",body.vy); ocSetField(item,"mass",body.mass); stdlibArrayAppend(out,item); }
        return box(out);
    }
    writeStderr("PRCYTERAS: Physics2D."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - vector-db
struct OcVectorEntry { std::string id; std::vector<double> vector; std::string metadata; };
struct OcVectorDb { std::vector<OcVectorEntry> entries; std::mutex mutex; };
std::map<std::string, OcVectorDb>& ocVectorDbs() { static std::map<std::string, OcVectorDb> dbs; return dbs; }
double ocCosine(const std::vector<double>& a, const std::vector<double>& b) {
    const std::size_t size = std::min(a.size(), b.size()); double dot=0, na=0, nb=0;
    for (std::size_t i = 0; i < size; ++i) { dot += a[i]*b[i]; na += a[i]*a[i]; nb += b[i]*b[i]; }
    if (na == 0 || nb == 0) return 0.0; return dot / (std::sqrt(na)*std::sqrt(nb));
}
void* vectorDbCall(const std::string& op, void** args, int argc) {
    auto& dbs = ocVectorDbs();
    if ((op == "open" || op == "ac") && argc >= 1) { dbs[argumentString(args,0)]; return box(true); }
    if ((op == "add" || op == "ekle") && argc >= 3) {
        auto& db = dbs[argumentString(args,0)]; std::lock_guard<std::mutex> lock(db.mutex);
        OcVectorEntry entry; entry.id = argumentString(args,1); entry.vector = ocNumberList(argument(args,2)); entry.metadata = argc >= 4 ? ocJsonText(argument(args,3)) : std::string();
        for (auto& existing : db.entries) if (existing.id == entry.id) { existing = entry; return box(true); }
        db.entries.push_back(entry); return box(true);
    }
    if ((op == "search" || op == "ara") && argc >= 2) {
        auto& db = dbs[argumentString(args,0)]; std::lock_guard<std::mutex> lock(db.mutex);
        const auto query = ocNumberList(argument(args,1)); const std::size_t limit = static_cast<std::size_t>(std::max(0, argc>=3?argumentInt(args,2):5));
        std::vector<std::pair<double, std::size_t>> scores;
        for (std::size_t i = 0; i < db.entries.size(); ++i) scores.push_back({ocCosine(query, db.entries[i].vector), i});
        std::sort(scores.begin(), scores.end(), [](const auto& a, const auto& b){ return a.first > b.first; });
        Value out = stdlibArray();
        for (std::size_t i = 0; i < std::min(limit, scores.size()); ++i) {
            Value item = stdlibObject("VectorMatch"); ocSetField(item,"id",db.entries[scores[i].second].id); ocSetField(item,"score",scores[i].first);
            bool ok=false; Value metadata = db.entries[scores[i].second].metadata.empty() ? stdlibObject("JsonObject") : ocParseJson(db.entries[scores[i].second].metadata, ok);
            ocSetField(item,"metadata",metadata); stdlibArrayAppend(out,item);
        }
        return box(out);
    }
    if ((op == "size" || op == "boyut") && argc >= 1) { auto& db = dbs[argumentString(args,0)]; std::lock_guard<std::mutex> lock(db.mutex); return box(static_cast<std::int64_t>(db.entries.size())); }
    if ((op == "remove" || op == "kaldir") && argc >= 2) {
        auto& db = dbs[argumentString(args,0)]; std::lock_guard<std::mutex> lock(db.mutex); const std::string id = argumentString(args,1);
        db.entries.erase(std::remove_if(db.entries.begin(), db.entries.end(), [&](const OcVectorEntry& e){ return e.id == id; }), db.entries.end()); return box(true);
    }
    if ((op == "clear" || op == "temizle") && argc >= 1) { auto& db = dbs[argumentString(args,0)]; std::lock_guard<std::mutex> lock(db.mutex); db.entries.clear(); return box(true); }
    if ((op == "save" || op == "kaydet") && argc >= 2) {
        auto& db = dbs[argumentString(args,0)]; std::lock_guard<std::mutex> lock(db.mutex); Value document = stdlibArray();
        for (const auto& entry : db.entries) { Value item = stdlibObject("VectorEntry"); ocSetField(item,"id",entry.id); ocSetField(item,"vector",ocNumberArray(entry.vector)); bool ok=false; ocSetField(item,"metadata",entry.metadata.empty()?stdlibObject("JsonObject"):ocParseJson(entry.metadata,ok)); stdlibArrayAppend(document,item); }
        return box(ocWriteFile(argumentString(args,1), ocJsonText(document)));
    }
    if ((op == "load" || op == "yukle") && argc >= 2) {
        bool ok=false; const std::string text = ocReadFile(argumentString(args,1), ok); if (!ok) return box(false);
        Value document; document = ocParseJson(text, ok); if (!ok) return box(false);
        auto& db = dbs[argumentString(args,0)]; std::lock_guard<std::mutex> lock(db.mutex); db.entries.clear();
        for (const auto& item : ocValues(document)) { OcVectorEntry entry; entry.id = ocJsonString(item,"id"); entry.vector = ocNumberList(ocField(item,"vector")); entry.metadata = ocJsonText(ocField(item,"metadata")); db.entries.push_back(entry); }
        return box(true);
    }
    writeStderr("PRCYTERAS: VectorDb."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - neural-net
struct OcNeuralNet {
    std::vector<int> sizes;
    std::vector<std::vector<std::vector<double>>> weights;
    std::vector<std::vector<double>> biases;
    std::vector<std::vector<double>> activations;
};
std::map<std::string, OcNeuralNet>& ocNets() { static std::map<std::string, OcNeuralNet> nets; return nets; }
double ocActivate(double x) { return std::tanh(x); }
double ocActivateDerivative(double y) { return 1.0 - y*y; }
std::vector<double> ocForward(OcNeuralNet& net, const std::vector<double>& input) {
    net.activations.clear(); net.activations.push_back(input);
    std::vector<double> current = input;
    for (std::size_t layer = 0; layer < net.weights.size(); ++layer) {
        std::vector<double> next(net.weights[layer].size(), 0.0);
        for (std::size_t neuron = 0; neuron < net.weights[layer].size(); ++neuron) {
            double sum = net.biases[layer][neuron];
            for (std::size_t i = 0; i < current.size(); ++i) sum += net.weights[layer][neuron][i] * current[i];
            next[neuron] = ocActivate(sum);
        }
        net.activations.push_back(next); current = next;
    }
    return current;
}
void* neuralNetCall(const std::string& op, void** args, int argc) {
    auto& nets = ocNets();
    if ((op == "create" || op == "olustur") && argc >= 2) {
        const std::string name = argumentString(args,0); const auto sizes = ocNumberList(argument(args,1)); if (sizes.size() < 2) return box(false);
        OcNeuralNet net; for (double size : sizes) net.sizes.push_back(std::max(1, static_cast<int>(size)));
        std::mt19937 rng(static_cast<unsigned>(ocNowMillis()));
        std::normal_distribution<double> distribution(0.0, 1.0);
        for (std::size_t layer = 1; layer < net.sizes.size(); ++layer) {
            const double scale = std::sqrt(2.0 / net.sizes[layer-1]);
            std::vector<std::vector<double>> matrix; std::vector<double> bias;
            for (int neuron = 0; neuron < net.sizes[layer]; ++neuron) { std::vector<double> row; for (int i = 0; i < net.sizes[layer-1]; ++i) row.push_back(distribution(rng)*scale); matrix.push_back(row); bias.push_back(0.0); }
            net.weights.push_back(matrix); net.biases.push_back(bias);
        }
        nets[name] = net; return box(true);
    }
    if ((op == "forward" || op == "ileri") && argc >= 2) {
        auto found = nets.find(argumentString(args,0)); if (found == nets.end()) return box(stdlibArray());
        return box(ocNumberArray(ocForward(found->second, ocNumberList(argument(args,1)))));
    }
    if ((op == "train" || op == "egit") && argc >= 3) {
        auto found = nets.find(argumentString(args,0)); if (found == nets.end()) return box(false);
        auto& net = found->second;
        const auto input = ocNumberList(argument(args,1)), target = ocNumberList(argument(args,2));
        const double learningRate = argc >= 4 ? ocNum(args,3) : 0.05;
        if (input.size() != static_cast<std::size_t>(net.sizes.front()) || target.size() != static_cast<std::size_t>(net.sizes.back())) return box(false);
        std::vector<double> output = ocForward(net, input);
        std::vector<std::vector<double>> deltas(net.weights.size());
        for (std::size_t layer = net.weights.size(); layer-- > 0;) {
            deltas[layer].resize(net.weights[layer].size());
            for (std::size_t neuron = 0; neuron < net.weights[layer].size(); ++neuron) {
                const double error = layer + 1 == net.weights.size() ? output[neuron] - target[neuron] : 0.0;
                double sum = 0.0;
                if (layer + 1 < net.weights.size()) for (std::size_t next = 0; next < net.weights[layer+1].size(); ++next) sum += net.weights[layer+1][next][neuron] * deltas[layer+1][next];
                const double signal = layer + 1 == net.weights.size() ? error : sum;
                deltas[layer][neuron] = signal * ocActivateDerivative(net.activations[layer+1][neuron]);
            }
        }
        double loss = 0.0; for (std::size_t i = 0; i < output.size(); ++i) { const double diff = output[i]-target[i]; loss += diff*diff; }
        for (std::size_t layer = 0; layer < net.weights.size(); ++layer) {
            for (std::size_t neuron = 0; neuron < net.weights[layer].size(); ++neuron) {
                for (std::size_t i = 0; i < net.weights[layer][neuron].size(); ++i) net.weights[layer][neuron][i] -= learningRate * deltas[layer][neuron] * net.activations[layer][i];
                net.biases[layer][neuron] -= learningRate * deltas[layer][neuron];
            }
        }
        return box(loss);
    }
    if ((op == "predict" || op == "tahmin") && argc >= 2) {
        auto found = nets.find(argumentString(args,0)); if (found == nets.end()) return box(-1.0);
        const auto output = ocForward(found->second, ocNumberList(argument(args,1)));
        if (output.empty()) return box(-1.0); std::size_t best = 0; for (std::size_t i = 1; i < output.size(); ++i) if (output[i] > output[best]) best = i;
        return box(static_cast<std::int64_t>(best));
    }
    if ((op == "save" || op == "kaydet") && argc >= 2) {
        auto found = nets.find(argumentString(args,0)); if (found == nets.end()) return box(false);
        Value document = stdlibObject("NeuralNet");
        ocSetField(document,"sizes",ocNumberArray(std::vector<double>(found->second.sizes.begin(), found->second.sizes.end())));
        Value weights = stdlibArray();
        for (const auto& layer : found->second.weights) { Value rows = stdlibArray(); for (const auto& row : layer) stdlibArrayAppend(rows, ocNumberArray(row)); stdlibArrayAppend(weights, rows); }
        ocSetField(document,"weights",weights); Value biases = stdlibArray(); for (const auto& layer : found->second.biases) stdlibArrayAppend(biases, ocNumberArray(layer)); ocSetField(document,"biases",biases);
        return box(ocWriteFile(argumentString(args,1), ocJsonText(document)));
    }
    if ((op == "load" || op == "yukle") && argc >= 2) {
        bool ok=false; const std::string text = ocReadFile(argumentString(args,1), ok); if (!ok) return box(false);
        Value document = ocParseJson(text, ok); if (!ok) return box(false);
        OcNeuralNet net; for (double size : ocNumberList(ocField(document,"sizes"))) net.sizes.push_back(static_cast<int>(size));
        for (const auto& layer : ocValues(ocField(document,"weights"))) { std::vector<std::vector<double>> matrix; for (const auto& row : ocValues(layer)) matrix.push_back(ocNumberList(row)); net.weights.push_back(matrix); }
        for (const auto& layer : ocValues(ocField(document,"biases"))) net.biases.push_back(ocNumberList(layer));
        nets[argumentString(args,0)] = net; return box(true);
    }
    writeStderr("PRCYTERAS: NeuralNet."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - neat-ai
struct OcNeatConnection { int from=0, to=0; double weight=0; bool enabled=true; int innovation=0; };
struct OcNeatGenome { std::vector<int> inputs, outputs, hidden; std::vector<OcNeatConnection> connections; };
std::map<std::string, std::vector<OcNeatGenome>>& ocNeatPops() { static std::map<std::string, std::vector<OcNeatGenome>> pops; return pops; }
std::mt19937& ocNeatRng() { static std::mt19937 rng(static_cast<unsigned>(ocNowMillis())); return rng; }
OcNeatGenome ocNeatCreate(int inputs, int outputs) {
    OcNeatGenome genome; std::uniform_real_distribution<double> weight(-1.0, 1.0); int innovation = 0;
    for (int i = 0; i < inputs; ++i) genome.inputs.push_back(i);
    for (int o = 0; o < outputs; ++o) genome.outputs.push_back(inputs + o);
    for (int i = 0; i < inputs; ++i) for (int o = 0; o < outputs; ++o) genome.connections.push_back({i, inputs+o, weight(ocNeatRng()), true, innovation++});
    return genome;
}
double ocNeatEvaluate(const OcNeatGenome& genome, const std::vector<double>& input) {
    std::map<int,double> values; for (std::size_t i = 0; i < genome.inputs.size(); ++i) values[genome.inputs[i]] = i < input.size() ? input[i] : 0.0;
    for (int iteration = 0; iteration < 32; ++iteration) for (const auto& c : genome.connections) if (c.enabled) values[c.to] += values[c.from] * c.weight;
    double sum = 0; for (int output : genome.outputs) sum += std::tanh(values[output]);
    return sum / std::max<std::size_t>(1, genome.outputs.size());
}
void* neatAiCall(const std::string& op, void** args, int argc) {
    auto& pops = ocNeatPops();
    if ((op == "create" || op == "olustur") && argc >= 3) {
        const std::string name = argumentString(args,0); const int inputs = std::max(1, argumentInt(args,1)), outputs = std::max(1, argumentInt(args,2));
        const int population = argc >= 4 ? std::max(1, argumentInt(args,3)) : 20;
        auto& pop = pops[name]; pop.clear(); for (int i = 0; i < population; ++i) pop.push_back(ocNeatCreate(inputs, outputs)); return box(true);
    }
    if ((op == "evaluate" || op == "degerlendir") && argc >= 3) {
        auto found = pops.find(argumentString(args,0)); if (found == pops.end()) return box(-1.0);
        const int index = argumentInt(args,1); if (index < 0 || index >= static_cast<int>(found->second.size())) return box(-1.0);
        return box(ocNeatEvaluate(found->second[static_cast<std::size_t>(index)], ocNumberList(argument(args,2))));
    }
    if ((op == "mutate" || op == "mutasyon") && argc >= 2) {
        auto found = pops.find(argumentString(args,0)); if (found == pops.end()) return box(false);
        const int index = argumentInt(args,1); if (index < 0 || index >= static_cast<int>(found->second.size())) return box(false);
        auto& genome = found->second[static_cast<std::size_t>(index)];
        const double rate = argc >= 3 ? std::clamp(ocNum(args,2), 0.0, 1.0) : 0.1;
        std::uniform_real_distribution<double> chance(0.0, 1.0), weight(-1.0, 1.0);
        for (auto& c : genome.connections) if (chance(ocNeatRng()) < rate) c.weight += weight(ocNeatRng()) * 0.5;
        if (!genome.connections.empty() && chance(ocNeatRng()) < rate) { auto& c = genome.connections[static_cast<std::size_t>(ocNeatRng()() % genome.connections.size())]; c.enabled = !c.enabled; }
        return box(true);
    }
    if ((op == "crossover" || op == "caprazlama") && argc >= 4) {
        auto found = pops.find(argumentString(args,0)); if (found == pops.end()) return box(false);
        const int a = argumentInt(args,1), b = argumentInt(args,2), child = argumentInt(args,3);
        if (a < 0 || b < 0 || a >= static_cast<int>(found->second.size()) || b >= static_cast<int>(found->second.size())) return box(false);
        const auto& parentA = found->second[static_cast<std::size_t>(a)]; const auto& parentB = found->second[static_cast<std::size_t>(b)];
        OcNeatGenome genome = parentA; std::uniform_int_distribution<int> pick(0,1);
        for (std::size_t i = 0; i < genome.connections.size(); ++i) if (i < parentB.connections.size() && pick(ocNeatRng())) genome.connections[i] = parentB.connections[i];
        if (child >= 0 && child < static_cast<int>(found->second.size())) found->second[static_cast<std::size_t>(child)] = genome; else found->second.push_back(genome);
        return box(true);
    }
    if ((op == "population" || op == "populasyon") && argc >= 1) { auto found = pops.find(argumentString(args,0)); return box(static_cast<std::int64_t>(found == pops.end() ? 0 : found->second.size())); }
    if ((op == "genome" || op == "genom") && argc >= 2) {
        auto found = pops.find(argumentString(args,0)); if (found == pops.end()) return box(Value{});
        const int index = argumentInt(args,1); if (index < 0 || index >= static_cast<int>(found->second.size())) return box(Value{});
        Value out = stdlibObject("NeatGenome"); Value list = stdlibArray();
        for (const auto& c : found->second[static_cast<std::size_t>(index)].connections) { Value item = stdlibObject("NeatConnection"); ocSetField(item,"from",static_cast<std::int64_t>(c.from)); ocSetField(item,"to",static_cast<std::int64_t>(c.to)); ocSetField(item,"weight",c.weight); ocSetField(item,"enabled",c.enabled); stdlibArrayAppend(list,item); }
        ocSetField(out,"connections",list); return box(out);
    }
    writeStderr("PRCYTERAS: NeatAi."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}

// ben yaptım opencode - 2026-11-14 - crypto
std::string ocHashRaw(const wchar_t* algorithm, const std::string& data) {
    BCRYPT_ALG_HANDLE handle = nullptr;
    if (BCryptOpenAlgorithmProvider(&handle, algorithm, nullptr, 0) != 0) return {};
    DWORD hashLength = 0, written = 0;
    BCryptGetProperty(handle, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength), &written, 0);
    std::string out(hashLength, '\0');
    const NTSTATUS status = BCryptHash(handle, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
                                       static_cast<ULONG>(data.size()), reinterpret_cast<PUCHAR>(out.data()), hashLength);
    BCryptCloseAlgorithmProvider(handle, 0);
    if (status != 0) return {};
    return out;
}
std::string ocHmacRaw(const wchar_t* algorithm, const std::string& key, const std::string& data) {
    BCRYPT_ALG_HANDLE handle = nullptr;
    if (BCryptOpenAlgorithmProvider(&handle, algorithm, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) return {};
    DWORD hashLength = 0, written = 0;
    BCryptGetProperty(handle, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength), &written, 0);
    std::string out(hashLength, '\0');
    const NTSTATUS status = BCryptHash(handle, reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())), static_cast<ULONG>(key.size()),
                                       reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())), static_cast<ULONG>(data.size()),
                                       reinterpret_cast<PUCHAR>(out.data()), hashLength);
    BCryptCloseAlgorithmProvider(handle, 0);
    if (status != 0) return {};
    return out;
}
std::string ocAesCbc(bool encrypt, const std::string& key, const std::string& iv, const std::string& input) {
    BCRYPT_ALG_HANDLE handle = nullptr; BCRYPT_KEY_HANDLE keyHandle = nullptr;
    if (BCryptOpenAlgorithmProvider(&handle, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return {};
    const wchar_t* mode = BCRYPT_CHAIN_MODE_CBC;
    BCryptSetProperty(handle, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(mode)),
                      static_cast<ULONG>((wcslen(mode) + 1) * sizeof(wchar_t)), 0);
    if (BCryptGenerateSymmetricKey(handle, &keyHandle, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),
                                   static_cast<ULONG>(key.size()), 0) != 0) { BCryptCloseAlgorithmProvider(handle, 0); return {}; }
    std::string ivBlock = iv; ivBlock.resize(16, '\0');
    std::string data = input;
    if (encrypt) { const std::size_t pad = 16 - (data.size() % 16); data.append(pad, static_cast<char>(pad)); }
    std::string out(data.size() + 32, '\0'); ULONG written = 0;
    const NTSTATUS status = encrypt
        ? BCryptEncrypt(keyHandle, reinterpret_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr,
            reinterpret_cast<PUCHAR>(ivBlock.data()), static_cast<ULONG>(ivBlock.size()), reinterpret_cast<PUCHAR>(out.data()),
            static_cast<ULONG>(out.size()), &written, 0)
        : BCryptDecrypt(keyHandle, reinterpret_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr,
            reinterpret_cast<PUCHAR>(ivBlock.data()), static_cast<ULONG>(ivBlock.size()), reinterpret_cast<PUCHAR>(out.data()),
            static_cast<ULONG>(out.size()), &written, 0);
    BCryptDestroyKey(keyHandle); BCryptCloseAlgorithmProvider(handle, 0);
    if (status != 0) return {};
    out.resize(written);
    if (!encrypt) {
        if (out.empty()) return {};
        const std::size_t pad = static_cast<unsigned char>(out.back());
        if (pad == 0 || pad > 16 || pad > out.size()) return {};
        out.resize(out.size() - pad);
    }
    return out;
}
struct OcGcmResult { bool ok = false; std::string cipher, tag; };
OcGcmResult ocAesGcm(bool encrypt, const std::string& key, const std::string& iv, const std::string& input,
                     const std::string& aad, const std::string& tag) {
    OcGcmResult result;
    BCRYPT_ALG_HANDLE handle = nullptr; BCRYPT_KEY_HANDLE keyHandle = nullptr;
    if (BCryptOpenAlgorithmProvider(&handle, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) return result;
    const wchar_t* mode = BCRYPT_CHAIN_MODE_GCM;
    BCryptSetProperty(handle, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(mode)),
                      static_cast<ULONG>((wcslen(mode) + 1) * sizeof(wchar_t)), 0);
    if (BCryptGenerateSymmetricKey(handle, &keyHandle, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),
                                   static_cast<ULONG>(key.size()), 0) != 0) { BCryptCloseAlgorithmProvider(handle, 0); return result; }
    std::string ivBlock = iv; ivBlock.resize(12, '\0'); std::string tagBlock = tag; tagBlock.resize(16, '\0');
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = reinterpret_cast<PUCHAR>(ivBlock.data()); info.cbNonce = static_cast<ULONG>(ivBlock.size());
    info.pbAuthData = aad.empty() ? nullptr : reinterpret_cast<PUCHAR>(const_cast<char*>(aad.data()));
    info.cbAuthData = static_cast<ULONG>(aad.size());
    info.pbTag = reinterpret_cast<PUCHAR>(tagBlock.data()); info.cbTag = 16;
    std::string out(input.size() + 16, '\0'); ULONG written = 0;
    const NTSTATUS status = encrypt
        ? BCryptEncrypt(keyHandle, reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())), static_cast<ULONG>(input.size()), &info,
            nullptr, 0, reinterpret_cast<PUCHAR>(out.data()), static_cast<ULONG>(out.size()), &written, 0)
        : BCryptDecrypt(keyHandle, reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())), static_cast<ULONG>(input.size()), &info,
            nullptr, 0, reinterpret_cast<PUCHAR>(out.data()), static_cast<ULONG>(out.size()), &written, 0);
    if (encrypt) { out.resize(written); result.cipher = out; result.tag = tagBlock.substr(0, 16); result.ok = status == 0; }
    else if (status == 0) { out.resize(written); result.cipher = out; result.ok = true; }
    BCryptDestroyKey(keyHandle); BCryptCloseAlgorithmProvider(handle, 0);
    return result;
}
void ocChaChaBlock(const std::uint32_t input[16], unsigned char out[64]) {
    auto rotate = [](std::uint32_t v, int c) { return (v << c) | (v >> (32 - c)); };
    std::uint32_t x[16]; std::memcpy(x, input, sizeof(x));
    for (int i = 0; i < 10; ++i) {
        auto quarter = [&](int a, int b, int c, int d) {
            x[a] += x[b]; x[d] ^= x[a]; x[d] = rotate(x[d], 16);
            x[c] += x[d]; x[b] ^= x[c]; x[b] = rotate(x[b], 12);
            x[a] += x[b]; x[d] ^= x[a]; x[d] = rotate(x[d], 8);
            x[c] += x[d]; x[b] ^= x[c]; x[b] = rotate(x[b], 7);
        };
        quarter(0,4,8,12); quarter(1,5,9,13); quarter(2,6,10,14); quarter(3,7,11,15);
        quarter(0,5,10,15); quarter(1,6,11,12); quarter(2,7,8,13); quarter(3,4,9,14);
    }
    for (int i = 0; i < 16; ++i) { const std::uint32_t value = x[i] + input[i];
        out[i*4] = static_cast<unsigned char>(value & 0xFF); out[i*4+1] = static_cast<unsigned char>((value >> 8) & 0xFF);
        out[i*4+2] = static_cast<unsigned char>((value >> 16) & 0xFF); out[i*4+3] = static_cast<unsigned char>((value >> 24) & 0xFF); }
}
std::string ocChaCha20(const std::string& key, const std::string& nonce, const std::string& input, std::uint32_t counter) {
    std::uint32_t state[16] = {0x61707865u,0x3320646eu,0x79622d32u,0x6b206574u};
    for (int i = 0; i < 8; ++i) state[4+i] = static_cast<std::uint32_t>(static_cast<unsigned char>(key[i*4])) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(key[i*4+1])) << 8) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(key[i*4+2])) << 16) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(key[i*4+3])) << 24);
    for (int i = 0; i < 3; ++i) state[13+i] = static_cast<std::uint32_t>(static_cast<unsigned char>(nonce[i*4])) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(nonce[i*4+1])) << 8) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(nonce[i*4+2])) << 16) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(nonce[i*4+3])) << 24);
    std::string out(input.size(), '\0'); unsigned char block[64];
    for (std::size_t offset = 0; offset < input.size(); offset += 64) {
        state[12] = counter++;
        ocChaChaBlock(state, block);
        for (std::size_t i = 0; i < 64 && offset + i < input.size(); ++i) out[offset+i] = static_cast<char>(input[offset+i] ^ static_cast<char>(block[i]));
    }
    return out;
}
std::string ocPbkdf2(const std::string& password, const std::string& salt, int iterations, int length) {
    std::string out; std::uint32_t block = 1;
    while (out.size() < static_cast<std::size_t>(length)) {
        std::string message = salt; message.push_back(static_cast<char>((block >> 24) & 0xFF)); message.push_back(static_cast<char>((block >> 16) & 0xFF));
        message.push_back(static_cast<char>((block >> 8) & 0xFF)); message.push_back(static_cast<char>(block & 0xFF));
        std::string u = ocHmacRaw(BCRYPT_SHA256_ALGORITHM, password, message), t = u;
        for (int i = 1; i < iterations; ++i) { u = ocHmacRaw(BCRYPT_SHA256_ALGORITHM, password, u); for (std::size_t k = 0; k < t.size(); ++k) t[k] = static_cast<char>(t[k] ^ u[k]); }
        out += t; ++block;
    }
    out.resize(static_cast<std::size_t>(length));
    return out;
}
std::string ocHexToBytes(const std::string& text) {
    const auto bytes = ocUnhex(text);
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
void* cryptoCall(const std::string& op, void** args, int argc) {
    if ((op == "sha256" || op == "sha_256") && argc >= 1) return box(ocHex(ocHashRaw(BCRYPT_SHA256_ALGORITHM, argumentString(args,0))));
    if ((op == "sha1" || op == "sha_1") && argc >= 1) return box(ocHex(ocHashRaw(BCRYPT_SHA1_ALGORITHM, argumentString(args,0))));
    if ((op == "sha512" || op == "sha_512") && argc >= 1) return box(ocHex(ocHashRaw(BCRYPT_SHA512_ALGORITHM, argumentString(args,0))));
    if ((op == "md5") && argc >= 1) return box(ocHex(ocHashRaw(BCRYPT_MD5_ALGORITHM, argumentString(args,0))));
    if ((op == "hmac_sha256" || op == "hmac") && argc >= 2) return box(ocHex(ocHmacRaw(BCRYPT_SHA256_ALGORITHM, argumentString(args,0), argumentString(args,1))));
    if ((op == "hmac_sha1") && argc >= 2) return box(ocHex(ocHmacRaw(BCRYPT_SHA1_ALGORITHM, argumentString(args,0), argumentString(args,1))));
    if ((op == "hmac_sha512") && argc >= 2) return box(ocHex(ocHmacRaw(BCRYPT_SHA512_ALGORITHM, argumentString(args,0), argumentString(args,1))));
    if ((op == "aes_encrypt" || op == "aes_cbc_encrypt") && argc >= 3) {
        const std::string key = ocHexToBytes(argumentString(args,0)), iv = ocHexToBytes(argumentString(args,1));
        return box(ocHex(ocAesCbc(true, key, iv, argumentString(args,2))));
    }
    if ((op == "aes_decrypt" || op == "aes_cbc_decrypt") && argc >= 3) {
        const std::string key = ocHexToBytes(argumentString(args,0)), iv = ocHexToBytes(argumentString(args,1)), data = ocHexToBytes(argumentString(args,2));
        return box(ocAesCbc(false, key, iv, data));
    }
    if ((op == "aes_gcm_encrypt" || op == "gcm_encrypt") && argc >= 2) {
        const std::string key = ocHexToBytes(argumentString(args,0)), iv = ocHexToBytes(argumentString(args,1));
        const std::string plain = argumentString(args,2), aad = argc >= 4 ? argumentString(args,3) : std::string();
        const auto result = ocAesGcm(true, key, iv, plain, aad, std::string());
        if (!result.ok) return nullptr;
        Value out = stdlibObject("GcmResult"); ocSetField(out,"cipher",ocHex(result.cipher)); ocSetField(out,"tag",ocHex(result.tag)); return box(out);
    }
    if ((op == "aes_gcm_decrypt" || op == "gcm_decrypt") && argc >= 4) {
        const std::string key = ocHexToBytes(argumentString(args,0)), iv = ocHexToBytes(argumentString(args,1));
        const std::string cipher = ocHexToBytes(argumentString(args,2)), tag = ocHexToBytes(argumentString(args,3));
        const std::string aad = argc >= 5 ? argumentString(args,4) : std::string();
        const auto result = ocAesGcm(false, key, iv, cipher, aad, tag);
        return result.ok ? box(result.cipher) : nullptr;
    }
    if ((op == "chacha20" || op == "chacha") && argc >= 3) {
        const std::string key = ocHexToBytes(argumentString(args,0)), nonce = ocHexToBytes(argumentString(args,1));
        return box(ocHex(ocChaCha20(key, nonce, argumentString(args,2), argc >= 4 ? static_cast<std::uint32_t>(argumentInt(args,3)) : 0)));
    }
    if ((op == "pbkdf2" || op == "pbkdf2_sha256") && argc >= 2) {
        const int iterations = argc >= 3 ? std::clamp(argumentInt(args,2), 1, 2000000) : 100000;
        const int length = argc >= 4 ? std::clamp(argumentInt(args,3), 1, 256) : 32;
        return box(ocHex(ocPbkdf2(argumentString(args,0), argumentString(args,1), iterations, length)));
    }
    if ((op == "random_bytes" || op == "rastgele_bayt") && argc >= 1) return box(ocHex(ocRandomBytes(static_cast<std::size_t>(std::clamp(argumentInt(args,0), 0, 1048576)))));
    if ((op == "random_hex" || op == "rastgele_hex") && argc >= 1) return box(ocRandomHex(static_cast<std::size_t>(std::clamp(argumentInt(args,0), 0, 1048576))));
    if ((op == "equal" || op == "esit") && argc >= 2) {
        const std::string a = argumentString(args,0), b = argumentString(args,1);
        unsigned char diff = static_cast<unsigned char>(a.size() != b.size());
        const std::size_t size = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < size; ++i) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
        return box(diff == 0);
    }
    if ((op == "uuid" || op == "guid") && argc == 0) {
        const std::string bytes = ocRandomBytes(16);
        unsigned char b[16]; std::memcpy(b, bytes.data(), 16); b[6] = static_cast<unsigned char>((b[6] & 0x0F) | 0x40); b[8] = static_cast<unsigned char>((b[8] & 0x3F) | 0x80);
        char out[37]; std::snprintf(out, sizeof(out), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
        return box(std::string(out));
    }
    writeStderr("PRCYTERAS: Crypto."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - jwt
std::string ocJwtAlgorithm(const std::string& algorithm) {
    if (algorithm == "HS384" || algorithm == "hs384") return "HS384";
    if (algorithm == "HS512" || algorithm == "hs512") return "HS512";
    return "HS256";
}
std::string ocJwtHmac(const std::string& algorithm, const std::string& secret, const std::string& data) {
    if (algorithm == "HS384") return ocHmacRaw(BCRYPT_SHA384_ALGORITHM, secret, data);
    if (algorithm == "HS512") return ocHmacRaw(BCRYPT_SHA512_ALGORITHM, secret, data);
    return ocHmacRaw(BCRYPT_SHA256_ALGORITHM, secret, data);
}
void* jwtCall(const std::string& op, void** args, int argc) {
    if ((op == "sign" || op == "imzala") && argc >= 2) {
        const std::string algorithm = ocJwtAlgorithm(argc >= 3 ? argumentString(args,2) : std::string("HS256"));
        const std::string header = "{\"alg\":\"" + algorithm + "\",\"typ\":\"JWT\"}";
        const std::string payload = argumentString(args,0), secret = argumentString(args,1);
        const std::string unsignedToken = ocBase64UrlEncode(header) + "." + ocBase64UrlEncode(payload);
        return box(unsignedToken + "." + ocBase64UrlEncode(ocJwtHmac(algorithm, secret, unsignedToken)));
    }
    if ((op == "decode" || op == "coz") && argc >= 1) {
        const std::string token = argumentString(args,0);
        const std::size_t first = token.find('.'), second = first == std::string::npos ? std::string::npos : token.find('.', first+1);
        if (first == std::string::npos || second == std::string::npos) return nullptr;
        bool ok = false; const std::string payload = ocBase64UrlDecode(token.substr(first+1, second-first-1), ok);
        Value out = stdlibObject("JwtDecoded"); ocSetField(out,"header",ocParseJson(ocBase64UrlDecode(token.substr(0,first), ok), ok));
        ocSetField(out,"payload",ocParseJson(payload, ok)); ocSetField(out,"signature",token.substr(second+1)); return box(out);
    }
    if ((op == "verify" || op == "dogrula") && argc >= 2) {
        const std::string token = argumentString(args,0), secret = argumentString(args,1);
        Value result = stdlibObject("JwtResult");
        const std::size_t first = token.find('.'), second = first == std::string::npos ? std::string::npos : token.find('.', first+1);
        if (first == std::string::npos || second == std::string::npos) { ocSetField(result,"valid",false); ocSetField(result,"error",std::string("gecersiz token bicimi")); return box(result); }
        bool ok = false; const Value header = ocParseJson(ocBase64UrlDecode(token.substr(0,first), ok), ok);
        if (!ok) { ocSetField(result,"valid",false); ocSetField(result,"error",std::string("gecersiz baslik")); return box(result); }
        const std::string algorithm = ocJwtAlgorithm(ocJsonString(header,"alg"));
        const std::string unsignedToken = token.substr(0, second);
        const std::string expected = ocBase64UrlEncode(ocJwtHmac(algorithm, secret, unsignedToken));
        if (expected != token.substr(second+1)) { ocSetField(result,"valid",false); ocSetField(result,"error",std::string("imza uyusmuyor")); return box(result); }
        const Value payload = ocParseJson(ocBase64UrlDecode(token.substr(first+1, second-first-1), ok), ok);
        ocSetField(result,"valid",ok); ocSetField(result,"payload",payload);
        const double expiry = numberOf(ocField(payload,"exp"));
        if (expiry > 0 && expiry < static_cast<double>(ocNowMillis()) / 1000.0) { ocSetField(result,"valid",false); ocSetField(result,"error",std::string("suresi dolmus")); }
        return box(result);
    }
    writeStderr("PRCYTERAS: Jwt."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - argon2 (RFC 9106, Argon2id)
struct OcBlake2b {
    std::uint64_t h[8]; unsigned char buffer[128]; std::size_t used = 0; std::uint64_t total = 0; std::size_t outLength = 64;
    static std::uint64_t rotate(std::uint64_t value, int bits) { return (value >> bits) | (value << (64 - bits)); }
    void init(std::size_t length) {
        static const std::uint64_t iv[8] = {0x6a09e667f3bcc908ULL,0xbb67ae8584caa73bULL,0x3c6ef372fe94f82bULL,0xa54ff53a5f1d36f1ULL,
            0x510e527fade682d1ULL,0x9b05688c2b3e6c1fULL,0x1f83d9abfb41bd6bULL,0x5be0cd19137e2179ULL};
        std::memcpy(h, iv, sizeof(h)); h[0] ^= 0x01010000ULL ^ static_cast<std::uint64_t>(length); outLength = length; used = 0; total = 0;
    }
    void compress(const unsigned char* block, bool last) {
        static const unsigned char sigma[12][16] = {
            {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},{14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3},
            {11,8,12,0,5,2,15,13,10,14,3,6,7,1,9,4},{7,9,3,1,13,12,11,14,2,6,5,10,4,0,15,8},
            {9,0,5,7,2,4,10,15,14,1,11,12,6,8,3,13},{2,12,6,10,0,11,8,3,4,13,7,5,15,14,1,9},
            {12,5,1,15,14,13,4,10,0,7,6,3,9,2,8,11},{13,11,7,14,12,1,3,9,5,0,15,4,8,6,2,10},
            {6,15,14,9,11,3,0,8,12,2,13,7,1,4,10,5},{10,2,8,4,7,6,1,5,15,11,9,14,3,12,13,0},
            {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},{14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3}};
        std::uint64_t m[16], v[16];
        for (int i = 0; i < 16; ++i) { m[i] = 0; for (int b = 0; b < 8; ++b) m[i] |= static_cast<std::uint64_t>(block[i*8+b]) << (b*8); }
        std::memcpy(v, h, sizeof(v)); std::memcpy(v+8, h, sizeof(v)/2);
        v[12] ^= total; v[13] ^= 0; if (last) v[14] = ~v[14];
        auto g = [&](int a, int b, int c, int d, std::uint64_t x, std::uint64_t y) {
            v[a] = v[a] + v[b] + x; v[d] = rotate(v[d] ^ v[a], 32); v[c] += v[d]; v[b] = rotate(v[b] ^ v[c], 24);
            v[a] = v[a] + v[b] + y; v[d] = rotate(v[d] ^ v[a], 16); v[c] += v[d]; v[b] = rotate(v[b] ^ v[c], 63);
        };
        for (int round = 0; round < 12; ++round) {
            const auto& s = sigma[round];
            g(0,4,8,12,m[s[0]],m[s[1]]); g(1,5,9,13,m[s[2]],m[s[3]]); g(2,6,10,14,m[s[4]],m[s[5]]); g(3,7,11,15,m[s[6]],m[s[7]]);
            g(0,5,10,15,m[s[8]],m[s[9]]); g(1,6,11,12,m[s[10]],m[s[11]]); g(2,7,8,13,m[s[12]],m[s[13]]); g(3,4,9,14,m[s[14]],m[s[15]]);
        }
        for (int i = 0; i < 8; ++i) h[i] ^= v[i] ^ v[i+8];
    }
    void update(const unsigned char* data, std::size_t size) {
        total += size;
        while (size > 0) {
            const std::size_t take = std::min<std::size_t>(128 - used, size);
            std::memcpy(buffer + used, data, take); used += take; data += take; size -= take;
            if (used == 128) { compress(buffer, false); used = 0; }
        }
    }
    std::string finish() {
        std::memset(buffer + used, 0, 128 - used); total += 128 - used;
        compress(buffer, true);
        std::string out(outLength, '\0');
        for (std::size_t i = 0; i < outLength; ++i) out[i] = static_cast<char>((h[i/8] >> ((i%8)*8)) & 0xFF);
        return out;
    }
};
std::string ocBlake2b(const std::string& data, std::size_t length) {
    OcBlake2b hash; hash.init(length); hash.update(reinterpret_cast<const unsigned char*>(data.data()), data.size()); return hash.finish();
}
std::string ocArgon2HPrime(const std::string& input, std::size_t length) {
    std::string message; const std::uint32_t l = static_cast<std::uint32_t>(length);
    for (int i = 0; i < 4; ++i) message.push_back(static_cast<char>((l >> (i*8)) & 0xFF));
    message += input;
    if (length <= 64) return ocBlake2b(message, length);
    const std::size_t blocks = (length + 31) / 32;
    const std::size_t rounds = blocks - 2;
    std::string v = ocBlake2b(message, 64), out = v.substr(0, 32);
    for (std::size_t i = 0; i < rounds; ++i) { v = ocBlake2b(v, 64); out += v.substr(0, 32); }
    const std::size_t remaining = length - 32 * (rounds + 1);
    if (remaining > 0) { v = ocBlake2b(v, remaining); out += v.substr(0, remaining); }
    return out.substr(0, length);
}
std::uint64_t ocBlaMka(std::uint64_t x, std::uint64_t y) {
    return x + y + 2ULL * (x & 0xFFFFFFFFULL) * (y & 0xFFFFFFFFULL);
}
void ocArgon2G(std::uint64_t* v) {
    auto rotate = [](std::uint64_t value, int bits) { return (value >> bits) | (value << (64 - bits)); };
    auto g = [&](int a, int b, int c, int d) {
        v[a] = ocBlaMka(v[a], v[b]); v[d] = rotate(v[d] ^ v[a], 32);
        v[c] = ocBlaMka(v[c], v[d]); v[b] = rotate(v[b] ^ v[c], 24);
        v[a] = ocBlaMka(v[a], v[b]); v[d] = rotate(v[d] ^ v[a], 16);
        v[c] = ocBlaMka(v[c], v[d]); v[b] = rotate(v[b] ^ v[c], 63);
    };
    g(0,4,8,12); g(1,5,9,13); g(2,6,10,14); g(3,7,11,15);
    g(0,5,10,15); g(1,6,11,12); g(2,7,8,13); g(3,4,9,14);
}
void ocArgon2Permute(std::uint64_t* block) {
    std::uint64_t temp[16];
    for (int i = 0; i < 8; ++i) { std::memcpy(temp, block + i*16, sizeof(temp)); ocArgon2G(temp); std::memcpy(block + i*16, temp, sizeof(temp)); }
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) { temp[2*j] = block[2*i + j*16]; temp[2*j+1] = block[2*i + 1 + j*16]; }
        ocArgon2G(temp);
        for (int j = 0; j < 8; ++j) { block[2*i + j*16] = temp[2*j]; block[2*i + 1 + j*16] = temp[2*j+1]; }
    }
}
std::string ocArgon2id(const std::string& password, const std::string& salt, int passes, int memoryKb, int lanes, std::size_t outLength) {
    passes = std::clamp(passes, 1, 64); memoryKb = std::clamp(memoryKb, 8*lanes, 1048576); lanes = std::clamp(lanes, 1, 8);
    const std::uint32_t p = static_cast<std::uint32_t>(lanes);
    std::uint32_t memory = static_cast<std::uint32_t>(memoryKb);
    memory = (memory / (4 * p)) * (4 * p);
    const std::uint32_t laneLength = memory / p, segmentLength = laneLength / 4;
    auto le32 = [](std::string& out, std::uint32_t value) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((value >> (i*8)) & 0xFF)); };
    std::string h0Input; le32(h0Input, p); le32(h0Input, static_cast<std::uint32_t>(outLength)); le32(h0Input, memory);
    le32(h0Input, static_cast<std::uint32_t>(passes)); le32(h0Input, 0x13); le32(h0Input, 2);
    le32(h0Input, static_cast<std::uint32_t>(password.size())); h0Input += password;
    le32(h0Input, static_cast<std::uint32_t>(salt.size())); h0Input += salt;
    le32(h0Input, 0); le32(h0Input, 0);
    const std::string h0 = ocBlake2b(h0Input, 64);
    std::vector<std::vector<std::uint64_t>> blocks(static_cast<std::size_t>(memory), std::vector<std::uint64_t>(128, 0));
    auto blockIndex = [&](std::uint32_t lane, std::uint32_t index) { return static_cast<std::size_t>(lane) * laneLength + index; };
    auto fillBlock = [&](std::vector<std::uint64_t>& out, const std::vector<std::uint64_t>& prev, const std::vector<std::uint64_t>& ref, bool withXor) {
        std::vector<std::uint64_t> r(128), z(128);
        for (int i = 0; i < 128; ++i) { r[i] = prev[i] ^ ref[i]; z[i] = r[i]; }
        ocArgon2Permute(z.data());
        for (int i = 0; i < 128; ++i) out[i] = (withXor ? out[i] : 0ULL) ^ z[i] ^ r[i];
    };
    for (std::uint32_t lane = 0; lane < p; ++lane) {
        for (std::uint32_t index = 0; index < 2; ++index) {
            std::string input = h0; le32(input, index); le32(input, lane);
            const std::string bytes = ocArgon2HPrime(input, 1024);
            auto& block = blocks[blockIndex(lane, index)];
            for (int i = 0; i < 128; ++i) { std::uint64_t value = 0; for (int b = 0; b < 8; ++b) value |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[i*8+b])) << (b*8); block[i] = value; }
        }
    }
    std::uint64_t zeroBlock[128] = {0}, inputBlock[128] = {0}, addressBlock[128] = {0};
    auto nextAddresses = [&](std::uint64_t counter) {
        std::memcpy(inputBlock, zeroBlock, sizeof(inputBlock)); inputBlock[6] = counter;
        std::vector<std::uint64_t> tmp(inputBlock, inputBlock + 128);
        std::vector<std::uint64_t> zero(128, 0); std::vector<std::uint64_t> out(128, 0); std::vector<std::uint64_t> addrOut(128, 0);
        fillBlock(out, zero, tmp, false); fillBlock(addrOut, zero, out, false);
        std::memcpy(addressBlock, addrOut.data(), sizeof(addressBlock));
    };
    for (std::uint32_t pass = 0; pass < static_cast<std::uint32_t>(passes); ++pass) {
        for (std::uint32_t slice = 0; slice < 4; ++slice) {
            for (std::uint32_t lane = 0; lane < p; ++lane) {
                const bool independent = pass == 0 && slice < 2;
                if (independent) nextAddresses(0);
                for (std::uint32_t index = (pass == 0 && slice == 0) ? 2 : 0; index < segmentLength; ++index) {
                    const std::uint32_t position = slice * segmentLength + index;
                    const std::uint32_t prevIndex = position == 0 ? laneLength - 1 : position - 1;
                    const auto& prev = blocks[blockIndex(lane, prevIndex)];
                    std::uint64_t j1 = 0, j2 = 0;
                    if (independent) {
                        if (index % 128 == 0 && index != 0) nextAddresses(index / 128);
                        j1 = addressBlock[index % 128] & 0xFFFFFFFFULL; j2 = (addressBlock[index % 128] >> 32) & 0xFFFFFFFFULL;
                    } else { j1 = prev[0] & 0xFFFFFFFFULL; j2 = (prev[0] >> 32) & 0xFFFFFFFFULL; }
                    std::uint32_t refLane = static_cast<std::uint32_t>(j2 % p);
                    if (pass == 0 && slice == 0) refLane = lane;
                    const bool sameLane = refLane == lane;
                    std::uint32_t refArea = 0, start = 0;
                    if (pass == 0) {
                        if (slice == 0) { refArea = index - 1; start = 0; }
                        else if (sameLane) { refArea = slice * segmentLength + index - 1; start = 0; }
                        else { refArea = slice * segmentLength + (index == 0 ? static_cast<std::uint32_t>(-1) : 0); start = 0; }
                    } else {
                        if (sameLane) { refArea = laneLength - segmentLength + index - 1; start = (slice + 1) * segmentLength % laneLength; }
                        else { refArea = laneLength - segmentLength + (index == 0 ? static_cast<std::uint32_t>(-1) : 0); start = (slice + 1) * segmentLength % laneLength; }
                    }
                    std::uint64_t relative = j1; relative = (relative * relative) >> 32; relative = refArea - 1 - ((refArea * relative) >> 32);
                    const std::uint32_t refIndex = (start + static_cast<std::uint32_t>(relative)) % laneLength;
                    auto& current = blocks[blockIndex(lane, position)];
                    fillBlock(current, prev, blocks[blockIndex(refLane, refIndex)], pass > 0);
                }
            }
        }
    }
    std::vector<std::uint64_t> finalBlock(128, 0);
    for (std::uint32_t lane = 0; lane < p; ++lane) { const auto& block = blocks[blockIndex(lane, laneLength - 1)]; for (int i = 0; i < 128; ++i) finalBlock[i] ^= block[i]; }
    std::string finalBytes(1024, '\0');
    for (int i = 0; i < 128; ++i) for (int b = 0; b < 8; ++b) finalBytes[i*8+b] = static_cast<char>((finalBlock[i] >> (b*8)) & 0xFF);
    return ocArgon2HPrime(finalBytes, outLength);
}
void* argon2Call(const std::string& op, void** args, int argc) {
    if ((op == "hash" || op == "id" || op == "argon2id") && argc >= 2) {
        const int passes = argc >= 3 ? argumentInt(args,2) : 3;
        const int memory = argc >= 4 ? argumentInt(args,3) : 65536;
        const int lanes = argc >= 5 ? argumentInt(args,4) : 1;
        const int length = argc >= 6 ? argumentInt(args,5) : 32;
        return box(ocHex(ocArgon2id(argumentString(args,0), argumentString(args,1), passes, memory, lanes, static_cast<std::size_t>(std::clamp(length, 4, 128)))));
    }
    if ((op == "verify" || op == "dogrula") && argc >= 3) {
        const std::string expected = argumentString(args,2);
        const int passes = argc >= 4 ? argumentInt(args,3) : 3;
        const int memory = argc >= 5 ? argumentInt(args,4) : 65536;
        const int lanes = argc >= 6 ? argumentInt(args,5) : 1;
        const std::string actual = ocHex(ocArgon2id(argumentString(args,0), argumentString(args,1), passes, memory, lanes, expected.size()/2));
        return box(actual == expected);
    }
    writeStderr("PRCYTERAS: Argon2."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}

// ben yaptım opencode - 2026-11-14 - zip-archive (gercek DEFLATE)
std::uint32_t ocCrc32(const std::string& data) {
    static std::uint32_t table[256]; static bool ready = false;
    if (!ready) { for (std::uint32_t i = 0; i < 256; ++i) { std::uint32_t c = i; for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; } ready = true; }
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const unsigned char byte : data) crc = table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
struct OcBitWriter {
    std::string data; std::uint32_t buffer = 0; int count = 0;
    void writeBits(std::uint32_t value, int bits) {
        buffer |= (value & ((bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1))) << count; count += bits;
        while (count >= 8) { data.push_back(static_cast<char>(buffer & 0xFF)); buffer >>= 8; count -= 8; }
    }
    void writeCode(std::uint32_t code, int bits) { std::uint32_t reversed = 0; for (int i = 0; i < bits; ++i) reversed = (reversed << 1) | ((code >> i) & 1); writeBits(reversed, bits); }
    void align() { if (count > 0) { data.push_back(static_cast<char>(buffer & 0xFF)); buffer = 0; count = 0; } }
};
struct OcBitReader {
    const std::string& data; std::size_t pos = 0; std::uint32_t buffer = 0; int count = 0;
    explicit OcBitReader(const std::string& source) : data(source) {}
    bool read(int bits, std::uint32_t& out) {
        while (count < bits) { if (pos >= data.size()) return false; buffer |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[pos++])) << count; count += 8; }
        out = buffer & ((1u << bits) - 1); buffer >>= bits; count -= bits; return true;
    }
    bool readBit(int& bit) { std::uint32_t value = 0; if (!read(1, value)) return false; bit = static_cast<int>(value); return true; }
    void align() { buffer = 0; count = 0; }
};
struct OcHuffman {
    std::unordered_map<std::uint32_t,int> table; int maxLength = 0;
    void build(const std::vector<int>& lengths) {
        table.clear(); maxLength = 0; std::vector<int> count(16, 0);
        for (const int length : lengths) { if (length > 0) ++count[length]; maxLength = std::max(maxLength, length); }
        std::vector<int> next(16, 0); int code = 0;
        for (int bits = 1; bits <= 15; ++bits) { code = (code + count[bits-1]) << 1; next[bits] = code; }
        for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
            const int length = lengths[symbol]; if (length == 0) continue;
            table[(static_cast<std::uint32_t>(length) << 16) | static_cast<std::uint32_t>(next[length]++)] = static_cast<int>(symbol);
        }
    }
    bool decode(OcBitReader& reader, int& symbol) const {
        std::uint32_t code = 0;
        for (int length = 1; length <= maxLength; ++length) {
            std::uint32_t bit = 0; if (!reader.read(1, bit)) return false;
            code = (code << 1) | bit;
            const auto found = table.find((static_cast<std::uint32_t>(length) << 16) | code);
            if (found != table.end()) { symbol = found->second; return true; }
        }
        return false;
    }
};
int ocLengthSymbol(int length, int& extraBits, int& extraValue) {
    static const int base[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const int extra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    for (int i = 28; i >= 0; --i) if (length >= base[i]) { extraBits = extra[i]; extraValue = length - base[i]; return 257 + i; }
    extraBits = 0; extraValue = 0; return 257;
}
int ocDistanceSymbol(int distance, int& extraBits, int& extraValue) {
    static const int base[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const int extra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    for (int i = 29; i >= 0; --i) if (distance >= base[i]) { extraBits = extra[i]; extraValue = distance - base[i]; return i; }
    extraBits = 0; extraValue = 0; return 0;
}
void ocFixedCode(int symbol, int& code, int& bits) {
    if (symbol <= 143) { code = 0x30 + symbol; bits = 8; }
    else if (symbol <= 255) { code = 0x190 + symbol - 144; bits = 9; }
    else if (symbol <= 279) { code = symbol - 256; bits = 7; }
    else { code = 0xC0 + symbol - 280; bits = 8; }
}
std::string ocDeflate(const std::string& input) {
    OcBitWriter writer; writer.writeBits(1, 1); writer.writeBits(1, 2);
    std::unordered_map<std::uint32_t,int> seen; seen.reserve(input.size());
    auto hashAt = [&](std::size_t i) -> std::uint32_t {
        if (i + 2 >= input.size()) return 0xFFFFFFFFu;
        return (static_cast<std::uint32_t>(static_cast<unsigned char>(input[i])) << 16) |
               (static_cast<std::uint32_t>(static_cast<unsigned char>(input[i+1])) << 8) |
               static_cast<std::uint32_t>(static_cast<unsigned char>(input[i+2]));
    };
    auto emitLiteral = [&](int symbol) { int code = 0, bits = 0; ocFixedCode(symbol, code, bits); writer.writeCode(static_cast<std::uint32_t>(code), bits); };
    std::size_t i = 0;
    while (i < input.size()) {
        int bestLength = 0, bestDistance = 0;
        if (i + 2 < input.size()) {
            const std::uint32_t key = hashAt(i); const auto found = seen.find(key);
            if (found != seen.end() && i > static_cast<std::size_t>(found->second)) {
                const std::size_t candidate = static_cast<std::size_t>(found->second); const std::size_t distance = i - candidate;
                if (distance <= 32768) {
                    std::size_t length = 0; const std::size_t limit = std::min<std::size_t>(258, input.size() - i);
                    while (length < limit && input[candidate + length] == input[i + length]) ++length;
                    if (length >= 3) { bestLength = static_cast<int>(length); bestDistance = static_cast<int>(distance); }
                }
            }
        }
        if (bestLength >= 3) {
            int extraBits = 0, extraValue = 0; const int lengthSymbol = ocLengthSymbol(bestLength, extraBits, extraValue);
            emitLiteral(lengthSymbol); if (extraBits > 0) writer.writeBits(static_cast<std::uint32_t>(extraValue), extraBits);
            const int distanceSymbol = ocDistanceSymbol(bestDistance, extraBits, extraValue);
            writer.writeCode(static_cast<std::uint32_t>(distanceSymbol), 5);
            if (extraBits > 0) writer.writeBits(static_cast<std::uint32_t>(extraValue), extraBits);
            for (int k = 0; k < bestLength; ++k) { if (i + 2 < input.size()) seen[hashAt(i)] = static_cast<int>(i); ++i; }
        } else {
            emitLiteral(static_cast<unsigned char>(input[i]));
            if (i + 2 < input.size()) seen[hashAt(i)] = static_cast<int>(i);
            ++i;
        }
    }
    emitLiteral(256); writer.align();
    return writer.data;
}
bool ocInflate(const std::string& input, std::string& output) {
    OcBitReader reader(input); output.clear(); bool final = false;
    static OcHuffman fixedLiteral, fixedDistance; static bool ready = false;
    if (!ready) {
        std::vector<int> literal(288), distance(32, 5);
        for (int i = 0; i <= 143; ++i) literal[i] = 8;
        for (int i = 144; i <= 255; ++i) literal[i] = 9;
        for (int i = 256; i <= 279; ++i) literal[i] = 7;
        for (int i = 280; i <= 287; ++i) literal[i] = 8;
        fixedLiteral.build(literal); fixedDistance.build(distance); ready = true;
    }
    static const int lengthBase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const int lengthExtra[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const int distanceBase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const int distanceExtra[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    while (!final) {
        std::uint32_t value = 0;
        if (!reader.read(1, value)) return false; final = value != 0;
        if (!reader.read(2, value)) return false; const int type = static_cast<int>(value);
        if (type == 0) {
            reader.align(); if (reader.pos + 4 > input.size()) return false;
            const std::size_t length = static_cast<unsigned char>(input[reader.pos]) | (static_cast<std::size_t>(static_cast<unsigned char>(input[reader.pos+1])) << 8);
            reader.pos += 4;
            if (reader.pos + length > input.size()) return false;
            output.append(input, reader.pos, length); reader.pos += length; reader.buffer = 0; reader.count = 0;
            continue;
        }
        OcHuffman literal, distance;
        if (type == 1) { literal = fixedLiteral; distance = fixedDistance; }
        else if (type == 2) {
            std::uint32_t hlit = 0, hdist = 0, hclen = 0;
            if (!reader.read(5, hlit) || !reader.read(5, hdist) || !reader.read(4, hclen)) return false;
            const int literalCount = static_cast<int>(hlit) + 257, distanceCount = static_cast<int>(hdist) + 1, codeCount = static_cast<int>(hclen) + 4;
            static const int order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            std::vector<int> codeLengths(19, 0);
            for (int i = 0; i < codeCount; ++i) { std::uint32_t bits = 0; if (!reader.read(3, bits)) return false; codeLengths[order[i]] = static_cast<int>(bits); }
            OcHuffman codeHuffman; codeHuffman.build(codeLengths);
            std::vector<int> lengths; lengths.reserve(literalCount + distanceCount);
            while (static_cast<int>(lengths.size()) < literalCount + distanceCount) {
                int symbol = 0; if (!codeHuffman.decode(reader, symbol)) return false;
                if (symbol < 16) lengths.push_back(symbol);
                else if (symbol == 16) { if (lengths.empty()) return false; std::uint32_t extra = 0; if (!reader.read(2, extra)) return false; const int previous = lengths.back(); for (int i = 0; i < 3 + static_cast<int>(extra); ++i) lengths.push_back(previous); }
                else if (symbol == 17) { std::uint32_t extra = 0; if (!reader.read(3, extra)) return false; for (int i = 0; i < 3 + static_cast<int>(extra); ++i) lengths.push_back(0); }
                else { std::uint32_t extra = 0; if (!reader.read(7, extra)) return false; for (int i = 0; i < 11 + static_cast<int>(extra); ++i) lengths.push_back(0); }
            }
            literal.build(std::vector<int>(lengths.begin(), lengths.begin() + literalCount));
            distance.build(std::vector<int>(lengths.begin() + literalCount, lengths.begin() + literalCount + distanceCount));
        } else return false;
        while (true) {
            int symbol = 0; if (!literal.decode(reader, symbol)) return false;
            if (symbol < 256) { output.push_back(static_cast<char>(symbol)); continue; }
            if (symbol == 256) break;
            const int lengthIndex = symbol - 257; if (lengthIndex < 0 || lengthIndex >= 29) return false;
            std::uint32_t extra = 0; if (lengthExtra[lengthIndex] > 0 && !reader.read(lengthExtra[lengthIndex], extra)) return false;
            const int length = lengthBase[lengthIndex] + static_cast<int>(extra);
            int distanceSymbol = 0; if (!distance.decode(reader, distanceSymbol)) return false;
            if (distanceSymbol < 0 || distanceSymbol >= 30) return false;
            extra = 0; if (distanceExtra[distanceSymbol] > 0 && !reader.read(distanceExtra[distanceSymbol], extra)) return false;
            const int distanceValue = distanceBase[distanceSymbol] + static_cast<int>(extra);
            if (distanceValue <= 0 || static_cast<std::size_t>(distanceValue) > output.size()) return false;
            for (int i = 0; i < length; ++i) output.push_back(output[output.size() - static_cast<std::size_t>(distanceValue)]);
        }
    }
    return true;
}
void ocZipAppend16(std::string& out, std::uint32_t value) { out.push_back(static_cast<char>(value & 0xFF)); out.push_back(static_cast<char>((value >> 8) & 0xFF)); }
void ocZipAppend32(std::string& out, std::uint32_t value) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<char>((value >> (i*8)) & 0xFF)); }
void* zipArchiveCall(const std::string& op, void** args, int argc) {
    if ((op == "deflate" || op == "sikistir") && argc >= 1) return box(ocHex(ocDeflate(argumentString(args,0))));
    if ((op == "inflate" || op == "ac") && argc >= 1) {
        const auto bytes = ocUnhex(argumentString(args,0)); std::string output;
        const std::string input(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return ocInflate(input, output) ? box(output) : nullptr;
    }
    if ((op == "crc32") && argc >= 1) return box(static_cast<std::int64_t>(ocCrc32(argumentString(args,0))));
    if ((op == "create" || op == "olustur") && argc >= 1) {
        const std::string path = argumentString(args,0); const Value entries = argc >= 2 ? argument(args,1) : stdlibArray();
        std::string archive, central;
        for (const auto& entry : ocValues(entries)) {
            const std::string name = stringOf(ocField(entry,"name"));
            std::string content = stringOf(ocField(entry,"data"));
            const Value bytesValue = ocField(entry,"bytes");
            if (std::holds_alternative<std::shared_ptr<ListValue>>(bytesValue)) { std::string bytes; for (const auto& item : ocValues(bytesValue)) bytes.push_back(static_cast<char>(static_cast<int>(numberOf(item)) & 0xFF)); content = bytes; }
            const bool compress = argc >= 3 ? truthy(argument(args,2)) : true;
            std::string compressed = compress ? ocDeflate(content) : content;
            if (!compress || compressed.size() >= content.size()) compressed = content;
            const bool stored = compressed.size() == content.size() && (!compress || compressed == content);
            const std::uint32_t crc = ocCrc32(content), offset = static_cast<std::uint32_t>(archive.size());
            const std::uint16_t method = stored ? 0 : 8;
            ocZipAppend32(archive, 0x04034b50); ocZipAppend16(archive, 20); ocZipAppend16(archive, 0); ocZipAppend16(archive, method);
            ocZipAppend16(archive, 0); ocZipAppend16(archive, 0); ocZipAppend32(archive, crc);
            ocZipAppend32(archive, static_cast<std::uint32_t>(compressed.size())); ocZipAppend32(archive, static_cast<std::uint32_t>(content.size()));
            ocZipAppend16(archive, static_cast<std::uint32_t>(name.size())); ocZipAppend16(archive, 0); archive += name; archive += compressed;
            ocZipAppend32(central, 0x02014b50); ocZipAppend16(central, 20); ocZipAppend16(central, 20); ocZipAppend16(central, 0); ocZipAppend16(central, method);
            ocZipAppend16(central, 0); ocZipAppend16(central, 0); ocZipAppend32(central, crc);
            ocZipAppend32(central, static_cast<std::uint32_t>(compressed.size())); ocZipAppend32(central, static_cast<std::uint32_t>(content.size()));
            ocZipAppend16(central, static_cast<std::uint32_t>(name.size())); ocZipAppend16(central, 0); ocZipAppend16(central, 0);
            ocZipAppend16(central, 0); ocZipAppend16(central, 0); ocZipAppend32(central, 0); ocZipAppend32(central, offset); central += name;
        }
        const std::uint32_t centralOffset = static_cast<std::uint32_t>(archive.size()); archive += central;
        const std::uint32_t count = static_cast<std::uint32_t>(ocValues(entries).size());
        ocZipAppend32(archive, 0x06054b50); ocZipAppend16(archive, 0); ocZipAppend16(archive, 0);
        ocZipAppend16(archive, count); ocZipAppend16(archive, count);
        ocZipAppend32(archive, static_cast<std::uint32_t>(central.size())); ocZipAppend32(archive, centralOffset); ocZipAppend16(archive, 0);
        return box(ocWriteFile(path, archive));
    }
    if ((op == "list" || op == "liste") && argc >= 1) {
        bool ok = false; const std::string data = ocReadFile(argumentString(args,0), ok); if (!ok) return box(stdlibArray());
        std::size_t eocd = std::string::npos;
        for (std::size_t i = data.size() >= 22 ? data.size() - 22 : 0; i + 4 <= data.size(); ++i)
            if (static_cast<unsigned char>(data[i]) == 0x50 && static_cast<unsigned char>(data[i+1]) == 0x4b && static_cast<unsigned char>(data[i+2]) == 0x05 && static_cast<unsigned char>(data[i+3]) == 0x06) { eocd = i; break; }
        if (eocd == std::string::npos) return box(stdlibArray());
        auto read16 = [&](std::size_t at) { return static_cast<std::uint32_t>(static_cast<unsigned char>(data[at])) | (static_cast<std::uint32_t>(static_cast<unsigned char>(data[at+1])) << 8); };
        auto read32 = [&](std::size_t at) { std::uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[at+i])) << (i*8); return v; };
        const std::uint32_t count = read16(eocd + 10); std::size_t cursor = read32(eocd + 16);
        Value out = stdlibArray();
        for (std::uint32_t index = 0; index < count && cursor + 46 <= data.size(); ++index) {
            if (read32(cursor) != 0x02014b50) break;
            const std::uint32_t method = read16(cursor + 10), compressedSize = read32(cursor + 20), size = read32(cursor + 24);
            const std::uint32_t nameLength = read16(cursor + 28), extraLength = read16(cursor + 30), commentLength = read16(cursor + 32);
            const std::uint32_t localOffset = read32(cursor + 42);
            const std::string name = data.substr(cursor + 46, nameLength);
            Value entry = stdlibObject("ZipEntry"); ocSetField(entry,"name",name); ocSetField(entry,"size",static_cast<std::int64_t>(size));
            ocSetField(entry,"compressed",static_cast<std::int64_t>(compressedSize)); ocSetField(entry,"method",static_cast<std::int64_t>(method)); ocSetField(entry,"offset",static_cast<std::int64_t>(localOffset));
            stdlibArrayAppend(out, entry); cursor += 46 + nameLength + extraLength + commentLength;
        }
        return box(out);
    }
    if ((op == "extract" || op == "cikar") && argc >= 2) {
        bool ok = false; const std::string data = ocReadFile(argumentString(args,0), ok); if (!ok) return nullptr;
        const std::string wanted = argumentString(args,1);
        const Value entries = valueOf(zipArchiveCall("list", args, 1));
        for (const auto& entry : ocValues(entries)) {
            if (stringOf(ocField(entry,"name")) != wanted) continue;
            const std::size_t local = static_cast<std::size_t>(numberOf(ocField(entry,"offset")));
            if (local + 30 > data.size()) return nullptr;
            auto read16 = [&](std::size_t at) { return static_cast<std::uint32_t>(static_cast<unsigned char>(data[at])) | (static_cast<std::uint32_t>(static_cast<unsigned char>(data[at+1])) << 8); };
            auto read32 = [&](std::size_t at) { std::uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[at+i])) << (i*8); return v; };
            const std::uint32_t nameLength = read16(local + 26), extraLength = read16(local + 28), method = read16(local + 8), compressedSize = read32(local + 18);
            const std::size_t start = local + 30 + nameLength + extraLength;
            if (start + compressedSize > data.size()) return nullptr;
            const std::string payload = data.substr(start, compressedSize);
            if (method == 0) return box(payload);
            if (method == 8) { std::string output; return ocInflate(payload, output) ? box(output) : nullptr; }
            return nullptr;
        }
        return nullptr;
    }
    writeStderr("PRCYTERAS: ZipArchive."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - html
std::string ocHtmlEntityDecode(const std::string& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '&') { out.push_back(text[i]); continue; }
        const std::size_t semi = text.find(';', i + 1);
        if (semi == std::string::npos || semi - i > 12) { out.push_back('&'); continue; }
        const std::string name = text.substr(i + 1, semi - i - 1);
        if (name == "amp") out.push_back('&'); else if (name == "lt") out.push_back('<'); else if (name == "gt") out.push_back('>');
        else if (name == "quot") out.push_back('"'); else if (name == "apos") out.push_back('\''); else if (name == "nbsp") out.push_back(' ');
        else if (!name.empty() && name[0] == '#') {
            const bool hex = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
            unsigned long value = 0; bool valid = true;
            for (std::size_t k = hex ? 2 : 1; k < name.size(); ++k) {
                const char c = name[k]; int digit = -1;
                if (c >= '0' && c <= '9') digit = c - '0'; else if (hex && c >= 'a' && c <= 'f') digit = c - 'a' + 10; else if (hex && c >= 'A' && c <= 'F') digit = c - 'A' + 10; else { valid = false; break; }
                value = value * (hex ? 16 : 10) + static_cast<unsigned long>(digit);
            }
            if (!valid) { out += "&" + name + ";"; i = semi; continue; }
            if (value < 0x80) out.push_back(static_cast<char>(value));
            else if (value < 0x800) { out.push_back(static_cast<char>(0xC0 | (value >> 6))); out.push_back(static_cast<char>(0x80 | (value & 0x3F))); }
            else if (value < 0x10000) { out.push_back(static_cast<char>(0xE0 | (value >> 12))); out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (value & 0x3F))); }
            else { out.push_back(static_cast<char>(0xF0 | (value >> 18))); out.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F))); out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F))); out.push_back(static_cast<char>(0x80 | (value & 0x3F))); }
        } else { out += "&" + name + ";"; i = semi; continue; }
        i = semi;
    }
    return out;
}
std::string ocHtmlEscape(const std::string& text, bool attribute) {
    std::string out;
    for (const char c : text) {
        if (c == '&') out += "&amp;"; else if (c == '<') out += "&lt;"; else if (c == '>') out += "&gt;";
        else if (c == '"' && attribute) out += "&quot;"; else if (c == '\'' && attribute) out += "&#39;"; else out.push_back(c);
    }
    return out;
}
struct OcHtmlNode { std::string type, name, text; std::map<std::string,std::string> attributes; std::vector<OcHtmlNode> children; };
OcHtmlNode ocHtmlParseNode(const std::string& html) {
    OcHtmlNode root; root.type = "root"; std::vector<OcHtmlNode*> stack{&root};
    std::size_t i = 0;
    while (i < html.size()) {
        if (html[i] != '<') {
            const std::size_t next = html.find('<', i); const std::string text = html.substr(i, next == std::string::npos ? std::string::npos : next - i);
            if (!text.empty()) { OcHtmlNode node; node.type = "text"; node.text = ocHtmlEntityDecode(text); stack.back()->children.push_back(node); }
            i = next == std::string::npos ? html.size() : next; continue;
        }
        if (html.compare(i, 4, "<!--") == 0) { const std::size_t end = html.find("-->", i + 4); i = end == std::string::npos ? html.size() : end + 3; continue; }
        const std::size_t close = html.find('>', i + 1); if (close == std::string::npos) break;
        std::string content = html.substr(i + 1, close - i - 1); i = close + 1;
        if (!content.empty() && content[0] == '/') {
            std::string name; for (const char c : content.substr(1)) { if (std::isspace(static_cast<unsigned char>(c))) break; name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c)))); }
            for (std::size_t s = stack.size(); s > 1; --s) if (stack[s-1]->name == name) { stack.resize(s-1); break; }
            continue;
        }
        if (!content.empty() && (content[0] == '!' || content[0] == '?')) continue;
        bool selfClosing = false; if (!content.empty() && content.back() == '/') { selfClosing = true; content.pop_back(); }
        OcHtmlNode node; node.type = "element"; std::size_t p = 0;
        while (p < content.size() && !std::isspace(static_cast<unsigned char>(content[p]))) { node.name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(content[p])))); ++p; }
        while (p < content.size()) {
            while (p < content.size() && std::isspace(static_cast<unsigned char>(content[p]))) ++p;
            std::string key; while (p < content.size() && content[p] != '=' && !std::isspace(static_cast<unsigned char>(content[p]))) key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(content[p++]))));
            while (p < content.size() && std::isspace(static_cast<unsigned char>(content[p]))) ++p;
            std::string value;
            if (p < content.size() && content[p] == '=') { ++p; while (p < content.size() && std::isspace(static_cast<unsigned char>(content[p]))) ++p;
                if (p < content.size() && (content[p] == '"' || content[p] == '\'')) { const char quote = content[p++]; while (p < content.size() && content[p] != quote) value.push_back(content[p++]); if (p < content.size()) ++p; }
                else while (p < content.size() && !std::isspace(static_cast<unsigned char>(content[p]))) value.push_back(content[p++]); }
            if (!key.empty()) node.attributes[key] = ocHtmlEntityDecode(value);
        }
        stack.back()->children.push_back(node); OcHtmlNode* added = &stack.back()->children.back();
        static const char* voidTags[] = {"br","hr","img","input","meta","link","source","area","base","col","embed","param","track","wbr"};
        bool isVoid = false; for (const char* tag : voidTags) if (node.name == tag) { isVoid = true; break; }
        if (!selfClosing && !isVoid) stack.push_back(added);
    }
    return root;
}
Value ocHtmlSerializeNode(const OcHtmlNode& node) {
    Value out = stdlibObject("HtmlNode"); ocSetField(out,"type",node.type);
    if (node.type == "text") { ocSetField(out,"text",node.text); return out; }
    ocSetField(out,"name",node.name); Value attributes = stdlibObject("HtmlAttributes");
    for (const auto& entry : node.attributes) ocSetField(attributes, entry.first, entry.second);
    ocSetField(out,"attributes",attributes); Value children = stdlibArray();
    for (const auto& child : node.children) stdlibArrayAppend(children, ocHtmlSerializeNode(child));
    ocSetField(out,"children",children); return out;
}
bool ocHtmlMatch(const OcHtmlNode& node, const std::string& selector) {
    if (node.type != "element") return false;
    if (!selector.empty() && selector[0] == '#') return node.attributes.count("id") && node.attributes.at("id") == selector.substr(1);
    if (!selector.empty() && selector[0] == '.') { const auto found = node.attributes.find("class"); if (found == node.attributes.end()) return false;
        std::istringstream stream(found->second); std::string token; while (stream >> token) if (token == selector.substr(1)) return true; return false; }
    return node.name == selector;
}
void ocHtmlCollect(const OcHtmlNode& node, const std::string& selector, Value& out) {
    if (ocHtmlMatch(node, selector)) stdlibArrayAppend(out, ocHtmlSerializeNode(node));
    for (const auto& child : node.children) ocHtmlCollect(child, selector, out);
}
void ocHtmlText(const OcHtmlNode& node, std::string& out) {
    if (node.type == "text") { out += node.text; return; }
    for (const auto& child : node.children) ocHtmlText(child, out);
}
std::string ocHtmlStrip(const std::string& html) { const auto root = ocHtmlParseNode(html); std::string out; ocHtmlText(root, out); return out; }
void* htmlCall(const std::string& op, void** args, int argc) {
    if ((op == "escape" || op == "kacis") && argc >= 1) return box(ocHtmlEscape(argumentString(args,0), argc >= 2 ? truthy(argument(args,1)) : false));
    if ((op == "unescape" || op == "coz") && argc >= 1) return box(ocHtmlEntityDecode(argumentString(args,0)));
    if ((op == "parse" || op == "ayristir") && argc >= 1) return box(ocHtmlSerializeNode(ocHtmlParseNode(argumentString(args,0))));
    if ((op == "query" || op == "sorgula") && argc >= 2) {
        const std::string selector = argumentString(args,1); Value out = stdlibArray();
        Value rootValue = argument(args,0);
        if (std::holds_alternative<std::string>(rootValue)) rootValue = ocHtmlSerializeNode(ocHtmlParseNode(stringOf(rootValue)));
        std::function<void(const Value&)> walk = [&](const Value& node) {
            if (stringOf(ocField(node,"type")) == "element") {
                bool match = false; const std::string name = stringOf(ocField(node,"name"));
                if (!selector.empty() && selector[0] == '#') match = ocJsonString(ocField(node,"attributes"),"id") == selector.substr(1);
                else if (!selector.empty() && selector[0] == '.') { std::istringstream stream(ocJsonString(ocField(node,"attributes"),"class")); std::string token; while (stream >> token) if (token == selector.substr(1)) match = true; }
                else match = name == selector;
                if (match) stdlibArrayAppend(out, node);
            }
            for (const auto& child : ocValues(ocField(node,"children"))) walk(child);
        };
        walk(rootValue); return box(out);
    }
    if ((op == "text" || op == "metin") && argc >= 1) {
        const Value rootValue = argument(args,0);
        if (std::holds_alternative<std::string>(rootValue)) return box(ocHtmlStrip(stringOf(rootValue)));
        std::string out; std::function<void(const Value&)> walk = [&](const Value& node) { if (stringOf(ocField(node,"type")) == "text") out += stringOf(ocField(node,"text")); for (const auto& child : ocValues(ocField(node,"children"))) walk(child); }; walk(rootValue); return box(out);
    }
    if ((op == "strip_tags" || op == "etiketleri_temizle") && argc >= 1) return box(ocHtmlStrip(argumentString(args,0)));
    if ((op == "sanitize" || op == "temizle") && argc >= 2) {
        const std::string html = argumentString(args,0); const auto allowed = ocStringList(argument(args,1));
        OcHtmlNode root = ocHtmlParseNode(html);
        std::function<void(OcHtmlNode&)> clean = [&](OcHtmlNode& node) {
            for (auto& child : node.children) {
                if (child.type == "element" && std::find(allowed.begin(), allowed.end(), child.name) == allowed.end()) {
                    OcHtmlNode replacement; replacement.type = "text"; std::string text; ocHtmlText(child, text); replacement.text = text; child = replacement; continue;
                }
                clean(child);
            }
        };
        clean(root); std::string out; std::function<void(const OcHtmlNode&)> write = [&](const OcHtmlNode& node) {
            if (node.type == "text") { out += ocHtmlEscape(node.text, false); return; }
            if (node.type == "element") { out += "<" + node.name; for (const auto& entry : node.attributes) out += " " + entry.first + "=\"" + ocHtmlEscape(entry.second, true) + "\""; out += ">"; }
            for (const auto& child : node.children) write(child);
            if (node.type == "element") out += "</" + node.name + ">";
        };
        for (const auto& child : root.children) write(child); return box(out);
    }
    writeStderr("PRCYTERAS: Html."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}

// ben yaptım opencode - 2026-11-14 - qr-code (gercek QR encoder, byte mode, EC M, v1-10)
std::uint8_t ocQrGfMul(std::uint8_t x, std::uint8_t y) {
    int z = 0;
    for (int i = 7; i >= 0; --i) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return static_cast<std::uint8_t>(z);
}
std::vector<std::uint8_t> ocQrRs(const std::vector<std::uint8_t>& data, int ecLength) {
    std::vector<std::uint8_t> divisor(static_cast<std::size_t>(ecLength), 0);
    divisor[static_cast<std::size_t>(ecLength - 1)] = 1;
    std::uint8_t root = 1;
    for (int i = 0; i < ecLength; ++i) {
        for (int j = 0; j < ecLength; ++j) {
            divisor[static_cast<std::size_t>(j)] = ocQrGfMul(divisor[static_cast<std::size_t>(j)], root);
            if (j + 1 < ecLength) divisor[static_cast<std::size_t>(j)] ^= divisor[static_cast<std::size_t>(j + 1)];
        }
        root = ocQrGfMul(root, 0x02);
    }
    std::vector<std::uint8_t> remainder(static_cast<std::size_t>(ecLength), 0);
    for (const std::uint8_t byte : data) {
        const std::uint8_t factor = byte ^ remainder[0];
        for (std::size_t i = 0; i + 1 < remainder.size(); ++i) remainder[i] = remainder[i + 1];
        remainder.back() = 0;
        for (int i = 0; i < ecLength; ++i) remainder[static_cast<std::size_t>(i)] ^= ocQrGfMul(divisor[static_cast<std::size_t>(i)], factor);
    }
    return remainder;
}
const int* ocQrCapacityTable() {
    static const int table[10] = {14, 26, 42, 62, 84, 106, 122, 152, 180, 213};
    return table;
}
int ocQrDataCodewords(int version) {
    static const int table[10] = {16, 28, 44, 64, 86, 108, 124, 154, 182, 216};
    return table[std::clamp(version, 1, 10) - 1];
}
int ocQrEcPerBlock(int version) {
    static const int table[10] = {10, 16, 26, 18, 24, 16, 18, 22, 22, 26};
    return table[std::clamp(version, 1, 10) - 1];
}
int ocQrBlockCount(int version) {
    static const int table[10] = {1, 1, 1, 2, 2, 4, 4, 4, 5, 5};
    return table[std::clamp(version, 1, 10) - 1];
}
struct OcQrCode {
    int version = 1, size = 21, mask = 0;
    std::vector<std::uint8_t> modules, function;
    explicit OcQrCode(int value) : version(value), size(17 + 4 * value),
        modules(static_cast<std::size_t>(size) * static_cast<std::size_t>(size), 0),
        function(static_cast<std::size_t>(size) * static_cast<std::size_t>(size), 0) {}
    void set(int row, int col, bool dark) {
        if (row < 0 || col < 0 || row >= size || col >= size) return;
        modules[static_cast<std::size_t>(row) * size + col] = dark ? 1 : 0;
        function[static_cast<std::size_t>(row) * size + col] = 1;
    }
    bool get(int row, int col) const { return modules[static_cast<std::size_t>(row) * size + col] != 0; }
    bool reserved(int row, int col) const { return function[static_cast<std::size_t>(row) * size + col] != 0; }
};
std::vector<int> ocQrAlignmentPositions(int version) {
    std::vector<int> positions;
    if (version <= 1) return positions;
    const int count = version / 7 + 2;
    const int step = (version == 32) ? 26 : (version * 4 + count * 2 + 1) / (count * 2 - 2) * 2;
    positions.resize(static_cast<std::size_t>(count));
    positions[0] = 6;
    int pos = 17 + 4 * version - 7;
    for (int i = count - 1; i >= 1; --i, pos -= step) positions[static_cast<std::size_t>(i)] = pos;
    return positions;
}
void ocQrFunctionPatterns(OcQrCode& qr) {
    const int size = qr.size;
    for (int i = 0; i < size; ++i) { qr.set(6, i, i % 2 == 0); qr.set(i, 6, i % 2 == 0); }
    auto finder = [&](int cx, int cy) {
        for (int dy = -4; dy <= 4; ++dy) for (int dx = -4; dx <= 4; ++dx) {
            const int distance = std::max(std::abs(dx), std::abs(dy));
            qr.set(cy + dy, cx + dx, distance != 2 && distance != 4);
        }
    };
    finder(3, 3); finder(size - 4, 3); finder(3, size - 4);
    const std::vector<int> positions = ocQrAlignmentPositions(qr.version);
    for (std::size_t i = 0; i < positions.size(); ++i) for (std::size_t j = 0; j < positions.size(); ++j) {
        if ((i == 0 && j == 0) || (i == 0 && j + 1 == positions.size()) || (i + 1 == positions.size() && j == 0)) continue;
        for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
            qr.set(positions[i] + dy, positions[j] + dx, std::max(std::abs(dx), std::abs(dy)) != 1);
    }
    if (qr.version >= 7) {
        int remainder = qr.version;
        for (int i = 0; i < 12; ++i) remainder = (remainder << 1) ^ ((remainder >> 11) * 0x1F25);
        const int bits = (qr.version << 12) | remainder;
        for (int i = 0; i < 18; ++i) {
            const bool bit = ((bits >> i) & 1) != 0;
            const int a = size - 11 + i % 3, b = i / 3;
            qr.set(b, a, bit); qr.set(a, b, bit);
        }
    }
}
void ocQrDrawFormat(OcQrCode& qr, int mask) {
    const int size = qr.size;
    const int data = mask; // EC seviyesi M = 0
    int remainder = data;
    for (int i = 0; i < 10; ++i) remainder = (remainder << 1) ^ ((remainder >> 9) * 0x537);
    const int bits = ((data << 10) | remainder) ^ 0x5412;
    for (int i = 0; i <= 5; ++i) qr.set(i, 8, ((bits >> i) & 1) != 0);
    qr.set(7, 8, ((bits >> 6) & 1) != 0);
    qr.set(8, 8, ((bits >> 7) & 1) != 0);
    qr.set(8, 7, ((bits >> 8) & 1) != 0);
    for (int i = 9; i < 15; ++i) qr.set(8, 14 - i, ((bits >> i) & 1) != 0);
    for (int i = 0; i < 8; ++i) qr.set(8, size - 1 - i, ((bits >> i) & 1) != 0);
    for (int i = 8; i < 15; ++i) qr.set(size - 15 + i, 8, ((bits >> i) & 1) != 0);
    qr.set(size - 8, 8, true);
}
bool ocQrMaskBit(int mask, int row, int col) {
    switch (mask) {
        case 0: return (row + col) % 2 == 0;
        case 1: return row % 2 == 0;
        case 2: return col % 3 == 0;
        case 3: return (row + col) % 3 == 0;
        case 4: return (row / 2 + col / 3) % 2 == 0;
        case 5: return (row * col) % 2 + (row * col) % 3 == 0;
        case 6: return ((row * col) % 2 + (row * col) % 3) % 2 == 0;
        default: return ((row + col) % 2 + (row * col) % 3) % 2 == 0;
    }
}
std::vector<std::uint8_t> ocQrEncodeData(const std::string& text, int version) {
    const int dataCodewords = ocQrDataCodewords(version);
    std::vector<std::uint8_t> bits;
    auto append = [&](std::uint32_t value, int count) { for (int i = count - 1; i >= 0; --i) bits.push_back(static_cast<std::uint8_t>((value >> i) & 1)); };
    append(4, 4);
    append(static_cast<std::uint32_t>(text.size()), version <= 9 ? 8 : 16);
    for (const unsigned char byte : text) append(byte, 8);
    const int capacity = dataCodewords * 8;
    for (int i = 0; i < 4 && static_cast<int>(bits.size()) < capacity; ++i) bits.push_back(0);
    while (bits.size() % 8 != 0) bits.push_back(0);
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i < bits.size(); i += 8) {
        std::uint8_t byte = 0;
        for (int k = 0; k < 8; ++k) byte = static_cast<std::uint8_t>((byte << 1) | bits[i + static_cast<std::size_t>(k)]);
        out.push_back(byte);
    }
    bool pad = true;
    while (static_cast<int>(out.size()) < dataCodewords) { out.push_back(pad ? 0xEC : 0x11); pad = !pad; }
    return out;
}
std::vector<std::uint8_t> ocQrCodewords(const std::string& text, int version) {
    const int dataCodewords = ocQrDataCodewords(version), ecPerBlock = ocQrEcPerBlock(version), blocks = ocQrBlockCount(version);
    const std::vector<std::uint8_t> data = ocQrEncodeData(text, version);
    const int shortLength = dataCodewords / blocks, numLong = dataCodewords % blocks, numShort = blocks - numLong;
    std::vector<std::vector<std::uint8_t>> dataBlocks, ecBlocks;
    std::size_t offset = 0;
    for (int block = 0; block < blocks; ++block) {
        const int length = shortLength + (block >= numShort ? 1 : 0);
        std::vector<std::uint8_t> chunk(data.begin() + static_cast<std::ptrdiff_t>(offset), data.begin() + static_cast<std::ptrdiff_t>(offset + length));
        offset += static_cast<std::size_t>(length);
        dataBlocks.push_back(chunk);
        ecBlocks.push_back(ocQrRs(chunk, ecPerBlock));
    }
    std::vector<std::uint8_t> out;
    for (int i = 0; i <= shortLength; ++i) for (int block = 0; block < blocks; ++block)
        if (i < static_cast<int>(dataBlocks[static_cast<std::size_t>(block)].size())) out.push_back(dataBlocks[static_cast<std::size_t>(block)][static_cast<std::size_t>(i)]);
    for (int i = 0; i < ecPerBlock; ++i) for (int block = 0; block < blocks; ++block)
        out.push_back(ecBlocks[static_cast<std::size_t>(block)][static_cast<std::size_t>(i)]);
    return out;
}
void ocQrPlaceData(OcQrCode& qr, const std::vector<std::uint8_t>& codewords, int mask) {
    const int size = qr.size;
    const std::size_t total = codewords.size() * 8;
    std::size_t index = 0;
    bool upward = true;
    for (int right = size - 1; right >= 1; right -= 2) {
        if (right == 6) right = 5;
        for (int vertical = 0; vertical < size; ++vertical) {
            const int row = upward ? size - 1 - vertical : vertical;
            for (int column = 0; column < 2; ++column) {
                const int col = right - column;
                if (qr.reserved(row, col)) continue;
                bool bit = index < total && ((codewords[index >> 3] >> (7 - (index & 7))) & 1) != 0;
                if (ocQrMaskBit(mask, row, col)) bit = !bit;
                qr.modules[static_cast<std::size_t>(row) * size + col] = bit ? 1 : 0;
                ++index;
            }
        }
        upward = !upward;
    }
}
int ocQrPenalty(const OcQrCode& qr) {
    const int size = qr.size; int score = 0;
    for (int row = 0; row < size; ++row) {
        int run = 1;
        for (int col = 1; col <= size; ++col) {
            if (col < size && qr.get(row, col) == qr.get(row, col - 1)) ++run;
            else { if (run >= 5) score += 3 + (run - 5); run = 1; }
        }
    }
    for (int col = 0; col < size; ++col) {
        int run = 1;
        for (int row = 1; row <= size; ++row) {
            if (row < size && qr.get(row, col) == qr.get(row - 1, col)) ++run;
            else { if (run >= 5) score += 3 + (run - 5); run = 1; }
        }
    }
    for (int row = 0; row + 1 < size; ++row) for (int col = 0; col + 1 < size; ++col) {
        const bool color = qr.get(row, col);
        if (color == qr.get(row, col + 1) && color == qr.get(row + 1, col) && color == qr.get(row + 1, col + 1)) score += 3;
    }
    static const int patternA[11] = {1,0,1,1,1,0,1,0,0,0,0};
    static const int patternB[11] = {0,0,0,0,1,0,1,1,1,0,1};
    for (int row = 0; row < size; ++row) for (int col = 0; col + 11 <= size; ++col) {
        bool matchA = true, matchB = true;
        for (int i = 0; i < 11; ++i) { const bool value = qr.get(row, col + i); matchA = matchA && value == (patternA[i] != 0); matchB = matchB && value == (patternB[i] != 0); }
        if (matchA) score += 40; if (matchB) score += 40;
    }
    for (int col = 0; col < size; ++col) for (int row = 0; row + 11 <= size; ++row) {
        bool matchA = true, matchB = true;
        for (int i = 0; i < 11; ++i) { const bool value = qr.get(row + i, col); matchA = matchA && value == (patternA[i] != 0); matchB = matchB && value == (patternB[i] != 0); }
        if (matchA) score += 40; if (matchB) score += 40;
    }
    int dark = 0;
    for (const std::uint8_t module : qr.modules) dark += module != 0;
    const int total = size * size;
    const int deviation = std::abs(dark * 100 / total - 50) / 5;
    score += deviation * 10;
    return score;
}
OcQrCode ocQrBuild(const std::string& text, int version, int mask) {
    OcQrCode qr(version);
    ocQrFunctionPatterns(qr);
    ocQrDrawFormat(qr, mask);
    ocQrPlaceData(qr, ocQrCodewords(text, version), mask);
    return qr;
}
OcQrCode ocQrBest(const std::string& text) {
    const int* capacity = ocQrCapacityTable();
    int version = 10;
    for (int candidate = 1; candidate <= 10; ++candidate)
        if (static_cast<int>(text.size()) <= capacity[candidate - 1]) { version = candidate; break; }
    OcQrCode best = ocQrBuild(text, version, 0);
    best.mask = 0;
    int bestScore = ocQrPenalty(best);
    for (int mask = 1; mask < 8; ++mask) {
        OcQrCode candidate = ocQrBuild(text, version, mask);
        const int score = ocQrPenalty(candidate);
        if (score < bestScore) { best = candidate; best.mask = mask; bestScore = score; }
    }
    return best;
}
Value ocQrValue(const OcQrCode& qr) {
    Value out = stdlibObject("QrCode");
    ocSetField(out, "version", static_cast<std::int64_t>(qr.version));
    ocSetField(out, "size", static_cast<std::int64_t>(qr.size));
    ocSetField(out, "mask", static_cast<std::int64_t>(qr.mask));
    Value rows = stdlibArray();
    for (int row = 0; row < qr.size; ++row) {
        std::string line;
        for (int col = 0; col < qr.size; ++col) line.push_back(qr.get(row, col) ? '1' : '0');
        stdlibArrayAppend(rows, line);
    }
    ocSetField(out, "rows", rows);
    return out;
}
std::string ocQrSvg(const OcQrCode& qr, int scale, int border) {
    const int dimension = (qr.size + border * 2) * scale;
    std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" +
        std::to_string(dimension) + "\" height=\"" + std::to_string(dimension) + "\" viewBox=\"0 0 " +
        std::to_string(dimension) + " " + std::to_string(dimension) + "\"><rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/><path d=\"";
    for (int row = 0; row < qr.size; ++row) for (int col = 0; col < qr.size; ++col) if (qr.get(row, col)) {
        out += "M" + std::to_string((col + border) * scale) + " " + std::to_string((row + border) * scale) +
               "h" + std::to_string(scale) + "v" + std::to_string(scale) + "h-" + std::to_string(scale) + "z";
    }
    out += "\" fill=\"#000000\"/></svg>";
    return out;
}
std::string ocQrText(const OcQrCode& qr, int border) {
    std::string out;
    for (int i = 0; i < border; ++i) out += std::string(static_cast<std::size_t>((qr.size + border * 2) * 2), ' ') + "\n";
    for (int row = 0; row < qr.size; ++row) {
        out += std::string(static_cast<std::size_t>(border * 2), ' ');
        for (int col = 0; col < qr.size; ++col) out += qr.get(row, col) ? "##" : "  ";
        out += "\n";
    }
    for (int i = 0; i < border; ++i) out += std::string(static_cast<std::size_t>((qr.size + border * 2) * 2), ' ') + "\n";
    return out;
}
void* qrCodeCall(const std::string& op, void** args, int argc) {
    if ((op == "encode" || op == "olustur") && argc >= 1) {
        const std::string text = argumentString(args, 0);
        if (text.size() > static_cast<std::size_t>(ocQrCapacityTable()[9])) return nullptr;
        return box(ocQrValue(ocQrBest(text)));
    }
    if ((op == "svg") && argc >= 1) {
        const std::string text = argumentString(args, 0);
        if (text.size() > static_cast<std::size_t>(ocQrCapacityTable()[9])) return nullptr;
        return box(ocQrSvg(ocQrBest(text), argc >= 2 ? std::clamp(argumentInt(args, 1), 1, 32) : 4, argc >= 3 ? std::clamp(argumentInt(args, 2), 0, 16) : 4));
    }
    if ((op == "text" || op == "metin") && argc >= 1) {
        const std::string text = argumentString(args, 0);
        if (text.size() > static_cast<std::size_t>(ocQrCapacityTable()[9])) return nullptr;
        return box(ocQrText(ocQrBest(text), argc >= 2 ? std::clamp(argumentInt(args, 1), 0, 8) : 2));
    }
    if ((op == "save_svg" || op == "kaydet") && argc >= 2) {
        const std::string text = argumentString(args, 0);
        if (text.size() > static_cast<std::size_t>(ocQrCapacityTable()[9])) return box(false);
        return box(ocWriteFile(argumentString(args, 1), ocQrSvg(ocQrBest(text), argc >= 3 ? std::clamp(argumentInt(args, 2), 1, 32) : 4, 4)));
    }
    writeStderr("PRCYTERAS: QrCode."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}

// ben yaptım opencode - 2026-11-14 - ag cekirdegi (gercek Winsock/WinINet)
std::once_flag ocWinsockOnce;
void ocWinsockInit() {
    std::call_once(ocWinsockOnce, []() { WSADATA data; WSAStartup(MAKEWORD(2, 2), &data); });
}
struct OcSocketHandle {
    SOCKET handle = INVALID_SOCKET;
    bool udp = false;
    std::mutex mutex;
};
std::mutex& ocSocketRegistryMutex() { static std::mutex mutex; return mutex; }
std::map<int, std::shared_ptr<OcSocketHandle>>& ocSocketRegistry() { static std::map<int, std::shared_ptr<OcSocketHandle>> registry; return registry; }
int ocSocketStore(SOCKET handle, bool udp) {
    std::lock_guard<std::mutex> lock(ocSocketRegistryMutex());
    static int next = 1;
    const int id = next++;
    auto item = std::make_shared<OcSocketHandle>();
    item->handle = handle; item->udp = udp;
    ocSocketRegistry()[id] = item;
    return id;
}
std::shared_ptr<OcSocketHandle> ocSocketFind(int id) {
    std::lock_guard<std::mutex> lock(ocSocketRegistryMutex());
    const auto found = ocSocketRegistry().find(id);
    return found == ocSocketRegistry().end() ? nullptr : found->second;
}
void ocSocketForget(int id) {
    std::lock_guard<std::mutex> lock(ocSocketRegistryMutex());
    const auto found = ocSocketRegistry().find(id);
    if (found != ocSocketRegistry().end()) {
        if (found->second->handle != INVALID_SOCKET) closesocket(found->second->handle);
        found->second->handle = INVALID_SOCKET;
        ocSocketRegistry().erase(found);
    }
}
void ocSocketTimeout(SOCKET handle, int timeoutMs) {
    DWORD value = static_cast<DWORD>(std::max(0, timeoutMs));
    setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&value), sizeof(value));
    setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&value), sizeof(value));
}
std::vector<std::string> ocResolveAll(const std::string& host) {
    ocWinsockInit();
    std::vector<std::string> addresses;
    addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0) return addresses;
    for (addrinfo* entry = result; entry; entry = entry->ai_next) {
        char text[INET6_ADDRSTRLEN] = {0};
        if (entry->ai_family == AF_INET) {
            const auto* address = reinterpret_cast<const sockaddr_in*>(entry->ai_addr);
            inet_ntop(AF_INET, &address->sin_addr, text, sizeof(text));
        } else if (entry->ai_family == AF_INET6) {
            const auto* address = reinterpret_cast<const sockaddr_in6*>(entry->ai_addr);
            inet_ntop(AF_INET6, &address->sin6_addr, text, sizeof(text));
        } else continue;
        const std::string value(text);
        if (std::find(addresses.begin(), addresses.end(), value) == addresses.end()) addresses.push_back(value);
    }
    freeaddrinfo(result);
    return addresses;
}
std::string ocResolveHost(const std::string& host) {
    const auto addresses = ocResolveAll(host);
    return addresses.empty() ? std::string() : addresses.front();
}
bool ocSocketConnect(const std::string& host, int port, int timeoutMs, SOCKET& out) {
    ocWinsockInit();
    addrinfo hints{}; hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0) return false;
    bool connected = false;
    for (addrinfo* entry = result; entry && !connected; entry = entry->ai_next) {
        SOCKET handle = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
        if (handle == INVALID_SOCKET) continue;
        u_long nonBlocking = 1;
        ioctlsocket(handle, FIONBIO, &nonBlocking);
        ::connect(handle, entry->ai_addr, static_cast<int>(entry->ai_addrlen));
        fd_set writable, errors;
        FD_ZERO(&writable); FD_ZERO(&errors);
        FD_SET(handle, &writable); FD_SET(handle, &errors);
        timeval timeout{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
        const int ready = select(0, nullptr, &writable, &errors, &timeout);
        if (ready > 0 && FD_ISSET(handle, &writable)) {
            int error = 0; int length = sizeof(error);
            getsockopt(handle, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length);
            if (error == 0) { nonBlocking = 0; ioctlsocket(handle, FIONBIO, &nonBlocking); out = handle; connected = true; }
        }
        if (!connected) closesocket(handle);
    }
    freeaddrinfo(result);
    return connected;
}
bool ocSocketSendAll(SOCKET handle, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int count = send(handle, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (count <= 0) return false;
        sent += static_cast<std::size_t>(count);
    }
    return true;
}
bool ocSocketReadExact(SOCKET handle, void* buffer, int size, int timeoutMs) {
    int total = 0;
    while (total < size) {
        ocSocketTimeout(handle, timeoutMs);
        const int count = recv(handle, static_cast<char*>(buffer) + total, size - total, 0);
        if (count <= 0) return false;
        total += count;
    }
    return true;
}
std::string ocSocketReadSome(SOCKET handle, int maxBytes, int timeoutMs, bool& ok) {
    std::string out(static_cast<std::size_t>(std::max(0, maxBytes)), '\0');
    ocSocketTimeout(handle, timeoutMs);
    const int count = recv(handle, out.data(), static_cast<int>(out.size()), 0);
    if (count <= 0) { ok = false; return {}; }
    out.resize(static_cast<std::size_t>(count));
    ok = true;
    return out;
}
std::string ocLocalIp() {
    ocWinsockInit();
    SOCKET handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (handle == INVALID_SOCKET) return "127.0.0.1";
    sockaddr_in target{}; target.sin_family = AF_INET; target.sin_port = htons(53); target.sin_addr.s_addr = inet_addr("8.8.8.8");
    std::string result = "127.0.0.1";
    if (connect(handle, reinterpret_cast<sockaddr*>(&target), sizeof(target)) == 0) {
        sockaddr_in local{}; int length = sizeof(local);
        if (getsockname(handle, reinterpret_cast<sockaddr*>(&local), &length) == 0) {
            char text[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &local.sin_addr, text, sizeof(text));
            result = text;
        }
    }
    closesocket(handle);
    return result;
}
std::vector<std::pair<std::string,std::string>> ocHeaderList(const Value& value) {
    std::vector<std::pair<std::string,std::string>> headers;
    if (auto object = ocObjectOf(value))
        for (const auto& entry : object->dynamicFields) headers.push_back({entry.first, stringOf(entry.second)});
    return headers;
}
Value ocHttpResponseValue(const OcHttpResponse& response) {
    Value out = stdlibObject("HttpResponse");
    ocSetField(out, "ok", response.ok);
    ocSetField(out, "status", static_cast<std::int64_t>(response.status));
    ocSetField(out, "body", response.body);
    ocSetField(out, "headers", response.headers);
    ocSetField(out, "error", response.error);
    return out;
}
void* httpCall(const std::string& op, void** args, int argc) {
    if ((op == "request" || op == "istek") && argc >= 2) {
        const std::string method = argumentString(args, 0), url = argumentString(args, 1);
        const std::string body = argc >= 3 ? argumentString(args, 2) : std::string();
        const auto headers = argc >= 4 ? ocHeaderList(argument(args, 3)) : std::vector<std::pair<std::string,std::string>>();
        const int timeout = argc >= 5 ? std::clamp(argumentInt(args, 4), 1000, 300000) : 30000;
        return box(ocHttpResponseValue(ocHttpRequest(method, url, body, headers, timeout)));
    }
    if ((op == "get" || op == "al") && argc >= 1) {
        const auto headers = argc >= 2 ? ocHeaderList(argument(args, 1)) : std::vector<std::pair<std::string,std::string>>();
        return box(ocHttpResponseValue(ocHttpRequest("GET", argumentString(args, 0), std::string(), headers, argc >= 3 ? argumentInt(args, 2) : 30000)));
    }
    if ((op == "post" || op == "gonder") && argc >= 2) {
        auto headers = argc >= 4 ? ocHeaderList(argument(args, 3)) : std::vector<std::pair<std::string,std::string>>();
        if (argc >= 3 && !argumentString(args, 2).empty()) headers.push_back({"Content-Type", argumentString(args, 2)});
        return box(ocHttpResponseValue(ocHttpRequest("POST", argumentString(args, 0), argumentString(args, 1), headers, argc >= 5 ? argumentInt(args, 4) : 30000)));
    }
    if ((op == "put" || op == "koy") && argc >= 2) {
        auto headers = argc >= 4 ? ocHeaderList(argument(args, 3)) : std::vector<std::pair<std::string,std::string>>();
        if (argc >= 3 && !argumentString(args, 2).empty()) headers.push_back({"Content-Type", argumentString(args, 2)});
        return box(ocHttpResponseValue(ocHttpRequest("PUT", argumentString(args, 0), argumentString(args, 1), headers, 30000)));
    }
    if ((op == "delete" || op == "sil") && argc >= 1)
        return box(ocHttpResponseValue(ocHttpRequest("DELETE", argumentString(args, 0), std::string(), argc >= 2 ? ocHeaderList(argument(args, 1)) : std::vector<std::pair<std::string,std::string>>(), 30000)));
    if ((op == "patch") && argc >= 2) {
        auto headers = argc >= 4 ? ocHeaderList(argument(args, 3)) : std::vector<std::pair<std::string,std::string>>();
        if (argc >= 3 && !argumentString(args, 2).empty()) headers.push_back({"Content-Type", argumentString(args, 2)});
        return box(ocHttpResponseValue(ocHttpRequest("PATCH", argumentString(args, 0), argumentString(args, 1), headers, 30000)));
    }
    if ((op == "json" || op == "json_get") && argc >= 1) {
        const auto response = ocHttpRequest("GET", argumentString(args, 0), std::string(), argc >= 2 ? ocHeaderList(argument(args, 1)) : std::vector<std::pair<std::string,std::string>>(), 30000);
        if (!response.ok) return nullptr;
        bool ok = false; const Value parsed = ocParseJson(response.body, ok);
        return ok ? box(parsed) : nullptr;
    }
    if ((op == "post_json" || op == "json_post") && argc >= 2) {
        auto headers = argc >= 4 ? ocHeaderList(argument(args, 3)) : std::vector<std::pair<std::string,std::string>>();
        headers.push_back({"Content-Type", "application/json"});
        const auto response = ocHttpRequest("POST", argumentString(args, 0), argumentString(args, 1), headers, 30000);
        if (!response.ok) return nullptr;
        bool ok = false; const Value parsed = ocParseJson(response.body, ok);
        return ok ? box(parsed) : nullptr;
    }
    if ((op == "encode_url" || op == "url_encode") && argc >= 1) return box(ocUrlEncode(argumentString(args, 0)));
    if ((op == "decode_url" || op == "url_decode") && argc >= 1) return box(ocUrlDecode(argumentString(args, 0)));
    writeStderr("PRCYTERAS: Http."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* socketCall(const std::string& op, void** args, int argc) {
    ocWinsockInit();
    if ((op == "connect" || op == "baglan") && argc >= 2) {
        SOCKET handle = INVALID_SOCKET;
        if (!ocSocketConnect(argumentString(args, 0), argumentInt(args, 1), argc >= 3 ? argumentInt(args, 2) : 5000, handle)) return box(std::int64_t{-1});
        ocSocketTimeout(handle, 30000);
        return box(static_cast<std::int64_t>(ocSocketStore(handle, false)));
    }
    if ((op == "send" || op == "gonder") && argc >= 2) {
        auto socket = ocSocketFind(argumentInt(args, 0)); if (!socket) return box(std::int64_t{-1});
        std::lock_guard<std::mutex> lock(socket->mutex);
        const std::string data = argumentString(args, 1);
        return box(ocSocketSendAll(socket->handle, data) ? static_cast<std::int64_t>(data.size()) : std::int64_t{-1});
    }
    if ((op == "recv" || op == "al") && argc >= 1) {
        auto socket = ocSocketFind(argumentInt(args, 0)); if (!socket) return box(std::string());
        std::lock_guard<std::mutex> lock(socket->mutex);
        bool ok = false;
        return box(ocSocketReadSome(socket->handle, argc >= 2 ? std::clamp(argumentInt(args, 1), 1, 1048576) : 4096, argc >= 3 ? argumentInt(args, 2) : 5000, ok));
    }
    if ((op == "recv_exact" || op == "tam_al") && argc >= 2) {
        auto socket = ocSocketFind(argumentInt(args, 0)); if (!socket) return box(std::string());
        std::lock_guard<std::mutex> lock(socket->mutex);
        const int size = std::clamp(argumentInt(args, 1), 0, 1048576);
        std::string out(static_cast<std::size_t>(size), '\0');
        if (!ocSocketReadExact(socket->handle, out.data(), size, argc >= 3 ? argumentInt(args, 2) : 5000)) return box(std::string());
        return box(out);
    }
    if ((op == "close" || op == "kapat") && argc >= 1) { ocSocketForget(argumentInt(args, 0)); return box(true); }
    if ((op == "listen" || op == "dinle") && argc >= 1) {
        SOCKET handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (handle == INVALID_SOCKET) return box(std::int64_t{-1});
        int reuse = 1; setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(static_cast<u_short>(argumentInt(args, 0)));
        if (bind(handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(handle, argc >= 2 ? argumentInt(args, 1) : 16) != 0) { closesocket(handle); return box(std::int64_t{-1}); }
        u_long nonBlocking = 1; ioctlsocket(handle, FIONBIO, &nonBlocking);
        return box(static_cast<std::int64_t>(ocSocketStore(handle, false)));
    }
    if ((op == "accept" || op == "kabul") && argc >= 1) {
        auto server = ocSocketFind(argumentInt(args, 0)); if (!server) return box(std::int64_t{-1});
        fd_set readable; FD_ZERO(&readable); FD_SET(server->handle, &readable);
        const int timeout = argc >= 2 ? argumentInt(args, 1) : 5000;
        timeval wait{timeout / 1000, (timeout % 1000) * 1000};
        if (select(0, &readable, nullptr, nullptr, &wait) <= 0) return box(std::int64_t{-1});
        SOCKET client = accept(server->handle, nullptr, nullptr);
        if (client == INVALID_SOCKET) return box(std::int64_t{-1});
        u_long blocking = 0; ioctlsocket(client, FIONBIO, &blocking);
        ocSocketTimeout(client, 30000);
        return box(static_cast<std::int64_t>(ocSocketStore(client, false)));
    }
    if ((op == "udp_bind" || op == "udp_ac") && argc >= 1) {
        SOCKET handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (handle == INVALID_SOCKET) return box(std::int64_t{-1});
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(static_cast<u_short>(argumentInt(args, 0)));
        if (bind(handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) { closesocket(handle); return box(std::int64_t{-1}); }
        return box(static_cast<std::int64_t>(ocSocketStore(handle, true)));
    }
    if ((op == "udp_send" || op == "udp_gonder") && argc >= 4) {
        auto socket = ocSocketFind(argumentInt(args, 0)); if (!socket) return box(std::int64_t{-1});
        sockaddr_in target{}; target.sin_family = AF_INET; target.sin_port = htons(static_cast<u_short>(argumentInt(args, 2)));
        const std::string address = argumentString(args, 1);
        if (inet_pton(AF_INET, address.c_str(), &target.sin_addr) != 1) {
            const std::string resolved = ocResolveHost(address);
            if (resolved.empty() || inet_pton(AF_INET, resolved.c_str(), &target.sin_addr) != 1) return box(std::int64_t{-1});
        }
        const std::string data = argumentString(args, 3);
        std::lock_guard<std::mutex> lock(socket->mutex);
        return box(static_cast<std::int64_t>(sendto(socket->handle, data.data(), static_cast<int>(data.size()), 0, reinterpret_cast<sockaddr*>(&target), sizeof(target))));
    }
    if ((op == "udp_recv" || op == "udp_al") && argc >= 1) {
        auto socket = ocSocketFind(argumentInt(args, 0)); if (!socket) return box(std::string());
        const int size = argc >= 2 ? std::clamp(argumentInt(args, 1), 1, 1048576) : 4096;
        const int timeout = argc >= 3 ? argumentInt(args, 2) : 5000;
        fd_set readable; FD_ZERO(&readable); FD_SET(socket->handle, &readable);
        timeval wait{timeout / 1000, (timeout % 1000) * 1000};
        if (select(0, &readable, nullptr, nullptr, &wait) <= 0) return box(std::string());
        std::string out(static_cast<std::size_t>(size), '\0');
        std::lock_guard<std::mutex> lock(socket->mutex);
        const int count = recvfrom(socket->handle, out.data(), size, 0, nullptr, nullptr);
        if (count <= 0) return box(std::string());
        out.resize(static_cast<std::size_t>(count));
        return box(out);
    }
    if ((op == "resolve" || op == "cozumle") && argc >= 1) return box(ocResolveHost(argumentString(args, 0)));
    if ((op == "resolve_all" || op == "tumunu_cozumle") && argc >= 1) return box(ocStringArray(ocResolveAll(argumentString(args, 0))));
    if ((op == "local_ip" || op == "yerel_ip") && argc == 0) return box(ocLocalIp());
    if ((op == "connected" || op == "bagli") && argc >= 1) { auto socket = ocSocketFind(argumentInt(args, 0)); return box(socket && socket->handle != INVALID_SOCKET); }
    writeStderr("PRCYTERAS: Socket."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* dnsCall(const std::string& op, void** args, int argc) {
    if ((op == "resolve" || op == "cozumle") && argc >= 1) return box(ocResolveHost(argumentString(args, 0)));
    if ((op == "resolve_all" || op == "tumunu_cozumle") && argc >= 1) return box(ocStringArray(ocResolveAll(argumentString(args, 0))));
    if ((op == "reverse" || op == "ters") && argc >= 1) {
        ocWinsockInit();
        sockaddr_in address{}; address.sin_family = AF_INET;
        if (inet_pton(AF_INET, argumentString(args, 0).c_str(), &address.sin_addr) != 1) return box(std::string());
        char host[NI_MAXHOST] = {0};
        if (getnameinfo(reinterpret_cast<sockaddr*>(&address), sizeof(address), host, sizeof(host), nullptr, 0, NI_NAMEREQD) != 0) return box(std::string());
        return box(std::string(host));
    }
    if ((op == "ping" || op == "gecikme") && argc >= 1) {
        const std::string host = argumentString(args, 0), address = ocResolveHost(host);
        Value out = stdlibObject("PingResult");
        ocSetField(out, "host", host); ocSetField(out, "ip", address); ocSetField(out, "ok", false); ocSetField(out, "rtt_ms", std::int64_t{-1});
        if (address.empty()) return box(out);
        const std::string ipv4 = address.find(':') == std::string::npos ? address : std::string();
        if (ipv4.empty()) return box(out);
        HANDLE icmp = IcmpCreateFile();
        if (icmp == INVALID_HANDLE_VALUE) return box(out);
        char payload[32] = "PRCYTERAS-PING";
        std::vector<unsigned char> replyBuffer(sizeof(ICMP_ECHO_REPLY) + sizeof(payload) + 16);
        const int timeout = argc >= 2 ? std::clamp(argumentInt(args, 1), 100, 60000) : 2000;
        const DWORD count = IcmpSendEcho(icmp, inet_addr(ipv4.c_str()), payload, sizeof(payload), nullptr,
            replyBuffer.data(), static_cast<DWORD>(replyBuffer.size()), static_cast<DWORD>(timeout));
        if (count > 0) {
            const auto* reply = reinterpret_cast<const ICMP_ECHO_REPLY*>(replyBuffer.data());
            ocSetField(out, "ok", reply->Status == 0);
            ocSetField(out, "rtt_ms", static_cast<std::int64_t>(reply->RoundTripTime));
        }
        IcmpCloseHandle(icmp);
        return box(out);
    }
    if ((op == "lookup" || op == "sorgu") && argc >= 1) {
        const std::string host = argumentString(args, 0);
        Value out = stdlibObject("DnsLookup");
        ocSetField(out, "host", host);
        ocSetField(out, "addresses", ocStringArray(ocResolveAll(host)));
        return box(out);
    }
    writeStderr("PRCYTERAS: Dns."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - downloader (gercek akisli indirme + SHA-256)
OcHttpResponse ocHttpDownload(const std::string& url, const std::string& path,
                              const std::vector<std::pair<std::string,std::string>>& headers, int timeoutMs, std::size_t& bytes, std::string& sha256) {
    OcHttpResponse response;
    std::wstring wideUrl = wide(url);
    const std::size_t schemeEnd = wideUrl.find(L"://");
    if (schemeEnd == std::wstring::npos) { response.error = "gecersiz URL"; return response; }
    const std::wstring scheme = wideUrl.substr(0, schemeEnd);
    std::wstring rest = wideUrl.substr(schemeEnd + 3);
    const std::size_t slash = rest.find(L'/');
    std::wstring hostPort = slash == std::wstring::npos ? rest : rest.substr(0, slash);
    std::wstring object = slash == std::wstring::npos ? L"/" : rest.substr(slash);
    std::wstring host = hostPort;
    INTERNET_PORT port = scheme == L"https" ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    const std::size_t colon = hostPort.rfind(L':');
    if (colon != std::wstring::npos && hostPort.find(L']') == std::wstring::npos) {
        host = hostPort.substr(0, colon);
        try { port = static_cast<INTERNET_PORT>(std::stoi(hostPort.substr(colon + 1))); } catch (...) {}
    }
    const bool secure = scheme == L"https";
    HINTERNET session = InternetOpenW(L"PRCYTERAS/0.11", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!session) { response.error = "InternetOpen basarisiz"; return response; }
    if (timeoutMs > 0) {
        InternetSetOptionW(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
        InternetSetOptionW(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    }
    HINTERNET connection = InternetConnectW(session, host.c_str(), port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!connection) { response.error = "baglanti kurulamadi"; InternetCloseHandle(session); return response; }
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | (secure ? INTERNET_FLAG_SECURE : 0);
    HINTERNET request = HttpOpenRequestW(connection, L"GET", object.c_str(), nullptr, nullptr, nullptr, flags, 0);
    if (!request) { response.error = "istek acilamadi"; InternetCloseHandle(connection); InternetCloseHandle(session); return response; }
    std::wstring headerBlock;
    for (const auto& header : headers) headerBlock += wide(header.first + ": " + header.second + "\r\n");
    if (!HttpSendRequestW(request, headerBlock.empty() ? nullptr : headerBlock.c_str(), headerBlock.empty() ? 0 : static_cast<DWORD>(headerBlock.size()), nullptr, 0)) {
        response.error = "istek gonderilemedi"; InternetCloseHandle(request); InternetCloseHandle(connection); InternetCloseHandle(session); return response;
    }
    DWORD statusCode = 0, statusSize = sizeof(statusCode);
    if (HttpQueryInfoW(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusSize, nullptr)) response.status = static_cast<int>(statusCode);
    std::ofstream output(fs::path(wide(path)), std::ios::binary | std::ios::trunc);
    if (!output) { response.error = "dosya olusturulamadi"; InternetCloseHandle(request); InternetCloseHandle(connection); InternetCloseHandle(session); return response; }
    OcSha256 hash;
    char buffer[65536]; DWORD read = 0; bytes = 0;
    while (InternetReadFile(request, buffer, sizeof(buffer), &read) && read > 0) {
        output.write(buffer, static_cast<std::streamsize>(read));
        hash.update(reinterpret_cast<const unsigned char*>(buffer), read);
        bytes += read;
    }
    output.close();
    unsigned char digest[32]; hash.finish(digest); sha256 = ocHex(digest, sizeof(digest));
    response.ok = response.status >= 200 && response.status < 300 && output.good();
    if (!response.ok && response.error.empty()) response.error = "indirme basarisiz (" + std::to_string(response.status) + ")";
    InternetCloseHandle(request); InternetCloseHandle(connection); InternetCloseHandle(session);
    return response;
}
void* downloaderCall(const std::string& op, void** args, int argc) {
    if ((op == "download" || op == "indir") && argc >= 2) {
        std::size_t bytes = 0; std::string sha256;
        const auto response = ocHttpDownload(argumentString(args, 0), argumentString(args, 1),
            argc >= 3 ? ocHeaderList(argument(args, 2)) : std::vector<std::pair<std::string,std::string>>(),
            argc >= 4 ? std::clamp(argumentInt(args, 3), 1000, 600000) : 60000, bytes, sha256);
        Value out = stdlibObject("DownloadResult");
        ocSetField(out, "ok", response.ok); ocSetField(out, "status", static_cast<std::int64_t>(response.status));
        ocSetField(out, "bytes", static_cast<std::int64_t>(bytes)); ocSetField(out, "sha256", sha256);
        ocSetField(out, "path", argumentString(args, 1)); ocSetField(out, "error", response.error);
        return box(out);
    }
    if ((op == "fetch_text" || op == "metin_al") && argc >= 1) {
        const auto response = ocHttpRequest("GET", argumentString(args, 0), std::string(), argc >= 2 ? ocHeaderList(argument(args, 1)) : std::vector<std::pair<std::string,std::string>>(), 60000);
        return box(response.body);
    }
    if ((op == "verify" || op == "dogrula") && argc >= 2) {
        bool ok = false; const std::string data = ocReadFile(argumentString(args, 0), ok);
        if (!ok) return box(false);
        return box(ocSha256(data) == argumentString(args, 1));
    }
    if ((op == "sha256" || op == "ozet") && argc >= 1) {
        bool ok = false; const std::string data = ocReadFile(argumentString(args, 0), ok);
        return box(ok ? ocSha256(data) : std::string());
    }
    writeStderr("PRCYTERAS: Downloader."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - ftp
struct OcFtpSession { HINTERNET session = nullptr, connection = nullptr; std::mutex mutex; };
std::mutex& ocFtpMutex() { static std::mutex mutex; return mutex; }
std::map<int, std::shared_ptr<OcFtpSession>>& ocFtpSessions() { static std::map<int, std::shared_ptr<OcFtpSession>> sessions; return sessions; }
std::shared_ptr<OcFtpSession> ocFtpFind(int id) {
    std::lock_guard<std::mutex> lock(ocFtpMutex());
    const auto found = ocFtpSessions().find(id);
    return found == ocFtpSessions().end() ? nullptr : found->second;
}
void* ftpCall(const std::string& op, void** args, int argc) {
    if ((op == "connect" || op == "baglan") && argc >= 1) {
        const std::wstring host = wide(argumentString(args, 0));
        const std::wstring user = wide(argc >= 2 ? argumentString(args, 1) : std::string("anonymous"));
        const std::wstring password = wide(argc >= 3 ? argumentString(args, 2) : std::string("anonymous@"));
        const INTERNET_PORT port = static_cast<INTERNET_PORT>(argc >= 4 ? argumentInt(args, 3) : INTERNET_DEFAULT_FTP_PORT);
        const bool passive = argc >= 5 ? truthy(argument(args, 4)) : true;
        HINTERNET session = InternetOpenW(L"PRCYTERAS-FTP/0.11", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
        if (!session) return box(std::int64_t{-1});
        DWORD flags = passive ? INTERNET_FLAG_PASSIVE : 0;
        HINTERNET connection = InternetConnectW(session, host.c_str(), port, user.c_str(), password.c_str(), INTERNET_SERVICE_FTP, flags, 0);
        if (!connection) { InternetCloseHandle(session); return box(std::int64_t{-1}); }
        std::lock_guard<std::mutex> lock(ocFtpMutex());
        static int next = 1; const int id = next++;
        auto item = std::make_shared<OcFtpSession>(); item->session = session; item->connection = connection;
        ocFtpSessions()[id] = item;
        return box(static_cast<std::int64_t>(id));
    }
    auto session = argc >= 1 ? ocFtpFind(argumentInt(args, 0)) : nullptr;
    if ((op == "list" || op == "liste") && argc >= 1) {
        if (!session) return box(stdlibArray());
        std::lock_guard<std::mutex> lock(session->mutex);
        const std::wstring path = wide(argc >= 2 ? argumentString(args, 1) : std::string("*"));
        WIN32_FIND_DATAW data{};
        HINTERNET find = FtpFindFirstFileW(session->connection, path.c_str(), &data, INTERNET_FLAG_RELOAD, 0);
        Value out = stdlibArray();
        if (!find) return box(out);
        do {
            Value entry = stdlibObject("FtpEntry");
            ocSetField(entry, "name", utf8(data.cFileName));
            ocSetField(entry, "size", static_cast<std::int64_t>((static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow));
            ocSetField(entry, "directory", (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
            stdlibArrayAppend(out, entry);
        } while (InternetFindNextFileW(find, &data));
        InternetCloseHandle(find);
        return box(out);
    }
    if ((op == "upload" || op == "yukle") && argc >= 3) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(FtpPutFileW(session->connection, wide(argumentString(args, 1)).c_str(), wide(argumentString(args, 2)).c_str(), FTP_TRANSFER_TYPE_BINARY, 0) != FALSE);
    }
    if ((op == "download" || op == "indir") && argc >= 3) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(FtpGetFileW(session->connection, wide(argumentString(args, 1)).c_str(), wide(argumentString(args, 2)).c_str(), FALSE, FILE_ATTRIBUTE_NORMAL, FTP_TRANSFER_TYPE_BINARY, 0) != FALSE);
    }
    if ((op == "remove" || op == "sil") && argc >= 2) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(FtpDeleteFileW(session->connection, wide(argumentString(args, 1)).c_str()) != FALSE);
    }
    if ((op == "mkdir" || op == "dizin_olustur") && argc >= 2) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(FtpCreateDirectoryW(session->connection, wide(argumentString(args, 1)).c_str()) != FALSE);
    }
    if ((op == "chdir" || op == "dizin_degistir") && argc >= 2) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(FtpSetCurrentDirectoryW(session->connection, wide(argumentString(args, 1)).c_str()) != FALSE);
    }
    if ((op == "close" || op == "kapat") && argc >= 1) {
        std::lock_guard<std::mutex> lock(ocFtpMutex());
        const auto found = ocFtpSessions().find(argumentInt(args, 0));
        if (found != ocFtpSessions().end()) {
            if (found->second->connection) InternetCloseHandle(found->second->connection);
            if (found->second->session) InternetCloseHandle(found->second->session);
            ocFtpSessions().erase(found);
        }
        return box(true);
    }
    writeStderr("PRCYTERAS: Ftp."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}

// ben yaptım opencode - 2026-11-14 - websocket (gercek RFC 6455 istemci/sunucu)
struct OcWebSocket {
    SOCKET handle = INVALID_SOCKET;
    int socketID = 0;
    bool client = true;
    std::mutex mutex;
};
std::mutex& ocWsMutex() { static std::mutex mutex; return mutex; }
std::map<int, std::shared_ptr<OcWebSocket>>& ocWsSessions() { static std::map<int, std::shared_ptr<OcWebSocket>> sessions; return sessions; }
std::shared_ptr<OcWebSocket> ocWsFind(int id) {
    std::lock_guard<std::mutex> lock(ocWsMutex());
    const auto found = ocWsSessions().find(id);
    return found == ocWsSessions().end() ? nullptr : found->second;
}
int ocWsStore(SOCKET handle, int socketID, bool client) {
    std::lock_guard<std::mutex> lock(ocWsMutex());
    static int next = 1; const int id = next++;
    auto item = std::make_shared<OcWebSocket>();
    item->handle = handle; item->socketID = socketID; item->client = client;
    ocWsSessions()[id] = item;
    return id;
}
void ocWsForget(int id) {
    std::shared_ptr<OcWebSocket> item;
    {
        std::lock_guard<std::mutex> lock(ocWsMutex());
        const auto found = ocWsSessions().find(id);
        if (found == ocWsSessions().end()) return;
        item = found->second;
        ocWsSessions().erase(found);
    }
    if (item->socketID > 0) ocSocketForget(item->socketID);
    else if (item->handle != INVALID_SOCKET) closesocket(item->handle);
    item->handle = INVALID_SOCKET;
}
bool ocWsSendFrame(OcWebSocket& ws, int opcode, const std::string& payload) {
    std::string frame;
    frame.push_back(static_cast<char>(0x80 | (opcode & 0x0F)));
    const bool mask = ws.client;
    const std::size_t length = payload.size();
    const unsigned char maskBit = mask ? 0x80 : 0x00;
    if (length < 126) frame.push_back(static_cast<char>(maskBit | length));
    else if (length <= 0xFFFF) {
        frame.push_back(static_cast<char>(maskBit | 126));
        frame.push_back(static_cast<char>((length >> 8) & 0xFF));
        frame.push_back(static_cast<char>(length & 0xFF));
    } else {
        frame.push_back(static_cast<char>(maskBit | 127));
        for (int i = 7; i >= 0; --i) frame.push_back(static_cast<char>((static_cast<std::uint64_t>(length) >> (i * 8)) & 0xFF));
    }
    if (mask) {
        const std::string key = ocRandomBytes(4);
        frame += key;
        for (std::size_t i = 0; i < payload.size(); ++i)
            frame.push_back(static_cast<char>(payload[i] ^ key[i % 4]));
    } else frame += payload;
    return ocSocketSendAll(ws.handle, frame);
}
bool ocWsReadFrame(OcWebSocket& ws, int& opcode, std::string& payload, bool& fin, int timeoutMs) {
    unsigned char header[2];
    if (!ocSocketReadExact(ws.handle, header, 2, timeoutMs)) return false;
    fin = (header[0] & 0x80) != 0;
    opcode = header[0] & 0x0F;
    const bool masked = (header[1] & 0x80) != 0;
    std::uint64_t length = header[1] & 0x7F;
    if (length == 126) {
        unsigned char extended[2];
        if (!ocSocketReadExact(ws.handle, extended, 2, timeoutMs)) return false;
        length = (static_cast<std::uint64_t>(extended[0]) << 8) | extended[1];
    } else if (length == 127) {
        unsigned char extended[8];
        if (!ocSocketReadExact(ws.handle, extended, 8, timeoutMs)) return false;
        length = 0;
        for (int i = 0; i < 8; ++i) length = (length << 8) | extended[i];
    }
    if (length > 16 * 1024 * 1024) return false;
    std::string key;
    if (masked) {
        key.resize(4);
        if (!ocSocketReadExact(ws.handle, key.data(), 4, timeoutMs)) return false;
    }
    payload.resize(static_cast<std::size_t>(length));
    if (length > 0 && !ocSocketReadExact(ws.handle, payload.data(), static_cast<int>(length), timeoutMs)) return false;
    if (masked) for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>(payload[i] ^ key[i % 4]);
    return true;
}
std::string ocWsReceive(OcWebSocket& ws, int timeoutMs, bool& ok) {
    std::string message;
    while (true) {
        int opcode = 0; std::string payload; bool fin = true;
        if (!ocWsReadFrame(ws, opcode, payload, fin, timeoutMs)) { ok = false; return {}; }
        if (opcode == 0x8) { ocWsSendFrame(ws, 0x8, payload.substr(0, 2)); ok = false; return {}; }
        if (opcode == 0x9) { ocWsSendFrame(ws, 0xA, payload); continue; }
        if (opcode == 0xA) continue;
        if (opcode == 0x1 || opcode == 0x2 || opcode == 0x0) {
            message += payload;
            if (fin) { ok = true; return message; }
            continue;
        }
        ok = false; return {};
    }
}
bool ocWsHandshakeClient(SOCKET handle, const std::string& hostHeader, const std::string& path, int timeoutMs) {
    const std::string key = base64Encode(ocRandomBytes(16), false);
    const std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + hostHeader +
        "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + key +
        "\r\nSec-WebSocket-Version: 13\r\n\r\n";
    if (!ocSocketSendAll(handle, request)) return false;
    std::string response;
    char buffer[2048];
    while (response.find("\r\n\r\n") == std::string::npos) {
        ocSocketTimeout(handle, timeoutMs);
        const int count = recv(handle, buffer, sizeof(buffer), 0);
        if (count <= 0) return false;
        response.append(buffer, static_cast<std::size_t>(count));
        if (response.size() > 65536) return false;
    }
    return response.find(" 101 ") != std::string::npos;
}
bool ocWsHandshakeServer(SOCKET handle, int timeoutMs) {
    std::string request;
    char buffer[2048];
    while (request.find("\r\n\r\n") == std::string::npos) {
        ocSocketTimeout(handle, timeoutMs);
        const int count = recv(handle, buffer, sizeof(buffer), 0);
        if (count <= 0) return false;
        request.append(buffer, static_cast<std::size_t>(count));
        if (request.size() > 65536) return false;
    }
    std::string lower = request;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const std::size_t at = lower.find("sec-websocket-key:");
    if (at == std::string::npos) return false;
    std::size_t start = at + 18;
    while (start < request.size() && (request[start] == ' ' || request[start] == '\t')) ++start;
    std::size_t end = request.find("\r\n", start);
    if (end == std::string::npos) end = request.size();
    const std::string key = request.substr(start, end - start);
    const std::string accept = base64Encode(ocHashRaw(BCRYPT_SHA1_ALGORITHM, key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"), false);
    const std::string response = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept + "\r\n\r\n";
    return ocSocketSendAll(handle, response);
}
void* webSocketCall(const std::string& op, void** args, int argc) {
    if ((op == "connect" || op == "baglan") && argc >= 1) {
        std::string url = argumentString(args, 0);
        const int timeout = argc >= 2 ? argumentInt(args, 1) : 10000;
        if (url.rfind("wss://", 0) == 0) { writeStderr("PRCYTERAS: WebSocket wss:// icin TLS katmani henuz yok; ws:// kullanin.\n"); return box(std::int64_t{-1}); }
        if (url.rfind("ws://", 0) != 0) { writeStderr("PRCYTERAS: WebSocket gecersiz URL.\n"); return box(std::int64_t{-1}); }
        std::string rest = url.substr(5);
        const std::size_t slash = rest.find('/');
        std::string hostPort = slash == std::string::npos ? rest : rest.substr(0, slash);
        const std::string path = slash == std::string::npos ? std::string("/") : rest.substr(slash);
        int port = 80; std::string host = hostPort;
        const std::size_t colon = hostPort.rfind(':');
        if (colon != std::string::npos) { host = hostPort.substr(0, colon); try { port = std::stoi(hostPort.substr(colon + 1)); } catch (...) {} }
        SOCKET handle = INVALID_SOCKET;
        if (!ocSocketConnect(host, port, timeout, handle)) return box(std::int64_t{-1});
        ocSocketTimeout(handle, timeout);
        if (!ocWsHandshakeClient(handle, hostPort, path, timeout)) { closesocket(handle); return box(std::int64_t{-1}); }
        return box(static_cast<std::int64_t>(ocWsStore(handle, 0, true)));
    }
    if ((op == "upgrade" || op == "yukselt") && argc >= 1) {
        auto socket = ocSocketFind(argumentInt(args, 0));
        if (!socket) return box(std::int64_t{-1});
        const int timeout = argc >= 2 ? argumentInt(args, 1) : 10000;
        if (!ocWsHandshakeServer(socket->handle, timeout)) return box(std::int64_t{-1});
        return box(static_cast<std::int64_t>(ocWsStore(socket->handle, argumentInt(args, 0), false)));
    }
    if ((op == "wrap" || op == "sarmala") && argc >= 1) {
        auto socket = ocSocketFind(argumentInt(args, 0));
        if (!socket) return box(std::int64_t{-1});
        const bool client = argc >= 2 ? truthy(argument(args, 1)) : true;
        return box(static_cast<std::int64_t>(ocWsStore(socket->handle, argumentInt(args, 0), client)));
    }
    if ((op == "send" || op == "gonder") && argc >= 2) {
        auto ws = ocWsFind(argumentInt(args, 0)); if (!ws) return box(false);
        std::lock_guard<std::mutex> lock(ws->mutex);
        return box(ocWsSendFrame(*ws, 0x1, argumentString(args, 1)));
    }
    if ((op == "send_binary" || op == "ikili_gonder") && argc >= 2) {
        auto ws = ocWsFind(argumentInt(args, 0)); if (!ws) return box(false);
        std::lock_guard<std::mutex> lock(ws->mutex);
        const auto bytes = ocUnhex(argumentString(args, 1));
        const std::string payload(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return box(ocWsSendFrame(*ws, 0x2, payload));
    }
    if ((op == "recv" || op == "al") && argc >= 1) {
        auto ws = ocWsFind(argumentInt(args, 0)); if (!ws) return box(std::string());
        std::lock_guard<std::mutex> lock(ws->mutex);
        bool ok = false;
        return box(ocWsReceive(*ws, argc >= 2 ? argumentInt(args, 1) : 10000, ok));
    }
    if ((op == "ping") && argc >= 1) {
        auto ws = ocWsFind(argumentInt(args, 0)); if (!ws) return box(false);
        std::lock_guard<std::mutex> lock(ws->mutex);
        return box(ocWsSendFrame(*ws, 0x9, argc >= 2 ? argumentString(args, 1) : std::string()));
    }
    if ((op == "close" || op == "kapat") && argc >= 1) { ocWsForget(argumentInt(args, 0)); return box(true); }
    writeStderr("PRCYTERAS: WebSocket."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - web-server (gercek HTTP/1.1 sunucusu)
struct OcWebRoute { std::string method, path; Value response; };
struct OcWebServer { SOCKET listener = INVALID_SOCKET; int port = 0; std::vector<OcWebRoute> routes; std::mutex mutex; };
std::mutex& ocWebServerMutex() { static std::mutex mutex; return mutex; }
std::map<int, std::shared_ptr<OcWebServer>>& ocWebServers() { static std::map<int, std::shared_ptr<OcWebServer>> servers; return servers; }
std::shared_ptr<OcWebServer> ocWebServerFind(int id) {
    std::lock_guard<std::mutex> lock(ocWebServerMutex());
    const auto found = ocWebServers().find(id);
    return found == ocWebServers().end() ? nullptr : found->second;
}
std::string ocHttpReason(int status) {
    switch (status) {
        case 200: return "OK"; case 201: return "Created"; case 204: return "No Content";
        case 301: return "Moved Permanently"; case 302: return "Found"; case 304: return "Not Modified";
        case 400: return "Bad Request"; case 401: return "Unauthorized"; case 403: return "Forbidden";
        case 404: return "Not Found"; case 405: return "Method Not Allowed"; case 500: return "Internal Server Error";
        default: return "OK";
    }
}
bool ocHttpReadRequest(SOCKET handle, std::string& method, std::string& target, std::string& body, std::string& rawHeaders, int timeoutMs) {
    std::string request;
    char buffer[4096];
    while (request.find("\r\n\r\n") == std::string::npos) {
        ocSocketTimeout(handle, timeoutMs);
        const int count = recv(handle, buffer, sizeof(buffer), 0);
        if (count <= 0) return false;
        request.append(buffer, static_cast<std::size_t>(count));
        if (request.size() > 1048576) return false;
    }
    const std::size_t headerEnd = request.find("\r\n\r\n");
    rawHeaders = request.substr(0, headerEnd + 4);
    const std::size_t firstLineEnd = request.find("\r\n");
    const std::string firstLine = request.substr(0, firstLineEnd);
    const std::size_t firstSpace = firstLine.find(' '), secondSpace = firstLine.find(' ', firstSpace + 1);
    if (firstSpace == std::string::npos) return false;
    method = firstLine.substr(0, firstSpace);
    target = secondSpace == std::string::npos ? firstLine.substr(firstSpace + 1) : firstLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    std::string lower = rawHeaders;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::size_t contentLength = 0;
    const std::size_t at = lower.find("content-length:");
    if (at != std::string::npos) { try { contentLength = static_cast<std::size_t>(std::stoull(rawHeaders.substr(at + 15))); } catch (...) { contentLength = 0; } }
    body = request.substr(headerEnd + 4);
    while (body.size() < contentLength) {
        ocSocketTimeout(handle, timeoutMs);
        const int count = recv(handle, buffer, sizeof(buffer), 0);
        if (count <= 0) break;
        body.append(buffer, static_cast<std::size_t>(count));
    }
    if (body.size() > contentLength) body.resize(contentLength);
    return true;
}
void* webServerCall(const std::string& op, void** args, int argc) {
    if ((op == "start" || op == "baslat") && argc >= 1) {
        ocWinsockInit();
        SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET) return box(std::int64_t{-1});
        int reuse = 1; setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(static_cast<u_short>(argumentInt(args, 0)));
        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(listener, 32) != 0) { closesocket(listener); return box(std::int64_t{-1}); }
        u_long nonBlocking = 1; ioctlsocket(listener, FIONBIO, &nonBlocking);
        std::lock_guard<std::mutex> lock(ocWebServerMutex());
        static int next = 1; const int id = next++;
        auto server = std::make_shared<OcWebServer>(); server->listener = listener; server->port = argumentInt(args, 0);
        ocWebServers()[id] = server;
        return box(static_cast<std::int64_t>(id));
    }
    if ((op == "route" || op == "yol") && argc >= 4) {
        auto server = ocWebServerFind(argumentInt(args, 0)); if (!server) return box(false);
        std::lock_guard<std::mutex> lock(server->mutex);
        server->routes.push_back({argumentString(args, 1), argumentString(args, 2), argument(args, 3)});
        return box(true);
    }
    if ((op == "step" || op == "adim") && argc >= 1) {
        auto server = ocWebServerFind(argumentInt(args, 0)); if (!server) return box(Value{});
        const int timeout = argc >= 2 ? argumentInt(args, 1) : 1000;
        fd_set readable; FD_ZERO(&readable); FD_SET(server->listener, &readable);
        timeval wait{timeout / 1000, (timeout % 1000) * 1000};
        if (select(0, &readable, nullptr, nullptr, &wait) <= 0) return box(Value{});
        SOCKET client = accept(server->listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) return box(Value{});
        std::string method, target, body, headers;
        if (!ocHttpReadRequest(client, method, target, body, headers, 5000)) { closesocket(client); return box(Value{}); }
        const std::string path = target.substr(0, target.find('?'));
        Value matched;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(server->mutex);
            for (const auto& route : server->routes) {
                const bool methodOk = route.method == "*" || route.method == method;
                const bool pathOk = route.path == path || route.path == "*" ||
                    (!route.path.empty() && route.path.back() == '*' && path.rfind(route.path.substr(0, route.path.size() - 1), 0) == 0);
                if (methodOk && pathOk) { matched = route.response; found = true; break; }
            }
        }
        int status = found ? 200 : 404;
        std::string responseBody = found ? std::string() : std::string("Not Found");
        std::string contentType = "text/plain; charset=utf-8";
        if (found) {
            if (std::holds_alternative<std::string>(matched)) responseBody = stringOf(matched);
            else if (auto object = ocObjectOf(matched)) {
                status = static_cast<int>(numberOf(ocField(matched, "status")));
                if (status <= 0) status = 200;
                const Value bodyValue = ocField(matched, "body");
                responseBody = std::holds_alternative<std::string>(bodyValue) ? stringOf(bodyValue) : ocJsonText(matched);
                const std::string type = stringOf(ocField(matched, "content_type"));
                if (!type.empty()) contentType = type;
                else if (!std::holds_alternative<std::string>(bodyValue)) contentType = "application/json; charset=utf-8";
            } else responseBody = stringOf(matched);
        }
        const std::string response = "HTTP/1.1 " + std::to_string(status) + " " + ocHttpReason(status) +
            "\r\nContent-Type: " + contentType + "\r\nContent-Length: " + std::to_string(responseBody.size()) +
            "\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n" + responseBody;
        ocSocketSendAll(client, response);
        closesocket(client);
        Value out = stdlibObject("HttpRequestInfo");
        ocSetField(out, "method", method); ocSetField(out, "path", path); ocSetField(out, "target", target);
        ocSetField(out, "body", body); ocSetField(out, "headers", headers);
        ocSetField(out, "status", static_cast<std::int64_t>(status)); ocSetField(out, "matched", found);
        return box(out);
    }
    if ((op == "port" || op == "port_no") && argc >= 1) { auto server = ocWebServerFind(argumentInt(args, 0)); return box(static_cast<std::int64_t>(server ? server->port : 0)); }
    if ((op == "stop" || op == "durdur") && argc >= 1) {
        std::lock_guard<std::mutex> lock(ocWebServerMutex());
        const auto found = ocWebServers().find(argumentInt(args, 0));
        if (found != ocWebServers().end()) { closesocket(found->second->listener); ocWebServers().erase(found); }
        return box(true);
    }
    writeStderr("PRCYTERAS: WebServer."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - oauth2
std::string ocFormEncode(const std::vector<std::pair<std::string,std::string>>& fields) {
    std::string out;
    for (const auto& field : fields) { if (!out.empty()) out += "&"; out += ocUrlEncode(field.first) + "=" + ocUrlEncode(field.second); }
    return out;
}
Value ocTokenValue(const OcHttpResponse& response) {
    Value out = stdlibObject("OAuthToken");
    ocSetField(out, "ok", response.ok);
    ocSetField(out, "status", static_cast<std::int64_t>(response.status));
    ocSetField(out, "error", response.error);
    bool parsedOk = false; const Value document = ocParseJson(response.body, parsedOk);
    if (parsedOk) {
        ocSetField(out, "access_token", ocJsonString(document, "access_token"));
        ocSetField(out, "refresh_token", ocJsonString(document, "refresh_token"));
        ocSetField(out, "token_type", ocJsonString(document, "token_type"));
        ocSetField(out, "expires_in", static_cast<std::int64_t>(numberOf(ocField(document, "expires_in"))));
        ocSetField(out, "scope", ocJsonString(document, "scope"));
        ocSetField(out, "error_description", ocJsonString(document, "error_description"));
        ocSetField(out, "raw", document);
    } else {
        ocSetField(out, "raw", response.body);
    }
    return out;
}
void* oauth2Call(const std::string& op, void** args, int argc) {
    if ((op == "pkce" || op == "kod") && argc == 0) {
        const std::string verifier = ocBase64UrlEncode(ocRandomBytes(32));
        Value out = stdlibObject("PkcePair");
        ocSetField(out, "verifier", verifier);
        ocSetField(out, "challenge", ocBase64UrlEncode(ocSha256Raw(verifier)));
        ocSetField(out, "method", std::string("S256"));
        return box(out);
    }
    if ((op == "auth_url" || op == "yetki_url") && argc >= 1) {
        const Value config = argument(args, 0);
        std::string url = ocJsonString(config, "authorize_url");
        if (url.empty()) return box(std::string());
        std::vector<std::pair<std::string,std::string>> fields;
        fields.push_back({"client_id", ocJsonString(config, "client_id")});
        fields.push_back({"redirect_uri", ocJsonString(config, "redirect_uri")});
        fields.push_back({"response_type", "code"});
        const Value scopes = ocField(config, "scopes");
        const std::string scope = std::holds_alternative<std::string>(scopes) ? stringOf(scopes) : [&]() {
            std::string joined; for (const auto& item : ocValues(scopes)) { if (!joined.empty()) joined += " "; joined += stringOf(item); } return joined;
        }();
        if (!scope.empty()) fields.push_back({"scope", scope});
        const std::string state = ocJsonString(config, "state");
        if (!state.empty()) fields.push_back({"state", state});
        const std::string challenge = ocJsonString(config, "code_challenge");
        if (!challenge.empty()) { fields.push_back({"code_challenge", challenge}); fields.push_back({"code_challenge_method", "S256"}); }
        if (auto extra = ocObjectOf(ocField(config, "params")))
            for (const auto& entry : extra->dynamicFields) fields.push_back({entry.first, stringOf(entry.second)});
        return box(url + (url.find('?') == std::string::npos ? "?" : "&") + ocFormEncode(fields));
    }
    if ((op == "exchange" || op == "degistir") && argc >= 2) {
        const Value config = argument(args, 0);
        std::vector<std::pair<std::string,std::string>> fields;
        fields.push_back({"grant_type", "authorization_code"});
        fields.push_back({"code", argumentString(args, 1)});
        fields.push_back({"client_id", ocJsonString(config, "client_id")});
        const std::string secret = ocJsonString(config, "client_secret");
        if (!secret.empty()) fields.push_back({"client_secret", secret});
        const std::string redirect = ocJsonString(config, "redirect_uri");
        if (!redirect.empty()) fields.push_back({"redirect_uri", redirect});
        const std::string verifier = argc >= 3 ? argumentString(args, 2) : std::string();
        if (!verifier.empty()) fields.push_back({"code_verifier", verifier});
        const std::string tokenUrl = ocJsonString(config, "token_url");
        if (tokenUrl.empty()) return box(Value{});
        return box(ocTokenValue(ocHttpRequest("POST", tokenUrl, ocFormEncode(fields),
            {{"Content-Type", "application/x-www-form-urlencoded"}, {"Accept", "application/json"}}, 30000)));
    }
    if ((op == "refresh" || op == "yenile") && argc >= 2) {
        const Value config = argument(args, 0);
        std::vector<std::pair<std::string,std::string>> fields;
        fields.push_back({"grant_type", "refresh_token"});
        fields.push_back({"refresh_token", argumentString(args, 1)});
        fields.push_back({"client_id", ocJsonString(config, "client_id")});
        const std::string secret = ocJsonString(config, "client_secret");
        if (!secret.empty()) fields.push_back({"client_secret", secret});
        const std::string tokenUrl = ocJsonString(config, "token_url");
        if (tokenUrl.empty()) return box(Value{});
        return box(ocTokenValue(ocHttpRequest("POST", tokenUrl, ocFormEncode(fields),
            {{"Content-Type", "application/x-www-form-urlencoded"}, {"Accept", "application/json"}}, 30000)));
    }
    writeStderr("PRCYTERAS: OAuth2."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-14 - upnp (SSDP + SOAP)
std::vector<std::pair<std::string,std::string>> ocSsdpHeaders(const std::string& text) {
    std::vector<std::pair<std::string,std::string>> headers;
    std::istringstream stream(text);
    std::string line;
    std::getline(stream, line);
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon), value = line.substr(colon + 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        headers.push_back({key, value});
    }
    return headers;
}
std::string ocXmlTag(const std::string& text, const std::string& tag) {
    const std::string open = "<" + tag + ">", close = "</" + tag + ">";
    std::size_t at = text.find(open);
    if (at == std::string::npos) return {};
    at += open.size();
    const std::size_t end = text.find(close, at);
    return end == std::string::npos ? std::string() : text.substr(at, end - at);
}
void* upnpCall(const std::string& op, void** args, int argc) {
    ocWinsockInit();
    if ((op == "discover" || op == "kesfet") && argc >= 0) {
        const int timeout = argc >= 1 ? std::clamp(argumentInt(args, 0), 200, 10000) : 2000;
        SOCKET handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (handle == INVALID_SOCKET) return box(stdlibArray());
        sockaddr_in target{}; target.sin_family = AF_INET; target.sin_port = htons(1900);
        inet_pton(AF_INET, "239.255.255.250", &target.sin_addr);
        const std::string message = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
        sendto(handle, message.data(), static_cast<int>(message.size()), 0, reinterpret_cast<sockaddr*>(&target), sizeof(target));
        Value out = stdlibArray();
        std::vector<std::string> seen;
        const ULONGLONG deadline = GetTickCount64() + static_cast<ULONGLONG>(timeout);
        while (GetTickCount64() < deadline) {
            fd_set readable; FD_ZERO(&readable); FD_SET(handle, &readable);
            const ULONGLONG remaining = deadline - GetTickCount64();
            timeval wait{static_cast<long>(remaining / 1000), static_cast<long>((remaining % 1000) * 1000)};
            if (select(0, &readable, nullptr, nullptr, &wait) <= 0) break;
            char buffer[8192];
            const int count = recvfrom(handle, buffer, sizeof(buffer) - 1, 0, nullptr, nullptr);
            if (count <= 0) continue;
            buffer[count] = 0;
            const std::string text(buffer, static_cast<std::size_t>(count));
            const auto headers = ocSsdpHeaders(text);
            std::string location, server, usn, st;
            for (const auto& header : headers) {
                if (header.first == "location") location = header.second;
                else if (header.first == "server") server = header.second;
                else if (header.first == "usn") usn = header.second;
                else if (header.first == "st") st = header.second;
            }
            if (location.empty() || std::find(seen.begin(), seen.end(), location) != seen.end()) continue;
            seen.push_back(location);
            Value entry = stdlibObject("UpnpDevice");
            ocSetField(entry, "location", location); ocSetField(entry, "server", server);
            ocSetField(entry, "usn", usn); ocSetField(entry, "st", st);
            stdlibArrayAppend(out, entry);
        }
        closesocket(handle);
        return box(out);
    }
    if ((op == "describe" || op == "tanimla") && argc >= 1) {
        const std::string location = argumentString(args, 0);
        const auto response = ocHttpRequest("GET", location, std::string(), {}, 10000);
        Value out = stdlibObject("UpnpDescription");
        ocSetField(out, "ok", response.ok); ocSetField(out, "location", location);
        ocSetField(out, "friendly_name", ocXmlTag(response.body, "friendlyName"));
        ocSetField(out, "manufacturer", ocXmlTag(response.body, "manufacturer"));
        ocSetField(out, "model_name", ocXmlTag(response.body, "modelName"));
        ocSetField(out, "service_type", ocXmlTag(response.body, "serviceType"));
        std::string control = ocXmlTag(response.body, "controlURL");
        if (!control.empty() && control.rfind("http", 0) != 0) {
            const std::size_t schemeEnd = location.find("://");
            if (schemeEnd != std::string::npos) {
                const std::size_t slash = location.find('/', schemeEnd + 3);
                const std::string base = slash == std::string::npos ? location : location.substr(0, slash);
                if (!control.empty() && control.front() != '/') control = "/" + control;
                control = base + control;
            }
        }
        ocSetField(out, "control_url", control);
        ocSetField(out, "body", response.body);
        return box(out);
    }
    if ((op == "add_mapping" || op == "port_ekle") && argc >= 6) {
        const std::string controlUrl = argumentString(args, 0), serviceType = argumentString(args, 1);
        const std::string externalPort = argumentString(args, 2), internalPort = argumentString(args, 3);
        const std::string internalClient = argumentString(args, 4), protocol = argumentString(args, 5);
        const std::string description = argc >= 7 ? argumentString(args, 6) : std::string("PRCYTERAS");
        const std::string lease = argc >= 8 ? argumentString(args, 7) : std::string("0");
        const std::string body = "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:AddPortMapping xmlns:u=\"" + serviceType + "\"><NewRemoteHost></NewRemoteHost><NewExternalPort>" + externalPort + "</NewExternalPort><NewProtocol>" + protocol + "</NewProtocol><NewInternalPort>" + internalPort + "</NewInternalPort><NewInternalClient>" + internalClient + "</NewInternalClient><NewEnabled>1</NewEnabled><NewPortMappingDescription>" + description + "</NewPortMappingDescription><NewLeaseDuration>" + lease + "</NewLeaseDuration></u:AddPortMapping></s:Body></s:Envelope>";
        const auto response = ocHttpRequest("POST", controlUrl, body,
            {{"Content-Type", "text/xml; charset=\"utf-8\""}, {"SOAPAction", "\"" + serviceType + "#AddPortMapping\""}}, 10000);
        Value out = stdlibObject("UpnpResult");
        ocSetField(out, "ok", response.ok && response.body.find("<errorCode>") == std::string::npos);
        ocSetField(out, "status", static_cast<std::int64_t>(response.status));
        ocSetField(out, "error_code", ocXmlTag(response.body, "errorCode"));
        ocSetField(out, "body", response.body);
        return box(out);
    }
    if ((op == "delete_mapping" || op == "port_sil") && argc >= 4) {
        const std::string controlUrl = argumentString(args, 0), serviceType = argumentString(args, 1);
        const std::string externalPort = argumentString(args, 2), protocol = argumentString(args, 3);
        const std::string body = "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:DeletePortMapping xmlns:u=\"" + serviceType + "\"><NewRemoteHost></NewRemoteHost><NewExternalPort>" + externalPort + "</NewExternalPort><NewProtocol>" + protocol + "</NewProtocol></u:DeletePortMapping></s:Body></s:Envelope>";
        const auto response = ocHttpRequest("POST", controlUrl, body,
            {{"Content-Type", "text/xml; charset=\"utf-8\""}, {"SOAPAction", "\"" + serviceType + "#DeletePortMapping\""}}, 10000);
        Value out = stdlibObject("UpnpResult");
        ocSetField(out, "ok", response.ok && response.body.find("<errorCode>") == std::string::npos);
        ocSetField(out, "status", static_cast<std::int64_t>(response.status));
        ocSetField(out, "error_code", ocXmlTag(response.body, "errorCode"));
        ocSetField(out, "body", response.body);
        return box(out);
    }
    if ((op == "external_ip" || op == "dis_ip") && argc >= 2) {
        const std::string controlUrl = argumentString(args, 0), serviceType = argumentString(args, 1);
        const std::string body = "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:GetExternalIPAddress xmlns:u=\"" + serviceType + "\"></u:GetExternalIPAddress></s:Body></s:Envelope>";
        const auto response = ocHttpRequest("POST", controlUrl, body,
            {{"Content-Type", "text/xml; charset=\"utf-8\""}, {"SOAPAction", "\"" + serviceType + "#GetExternalIPAddress\""}}, 10000);
        return box(ocXmlTag(response.body, "NewExternalIPAddress"));
    }
    if ((op == "local_ip" || op == "yerel_ip") && argc == 0) return box(ocLocalIp());
    writeStderr("PRCYTERAS: Upnp."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}

// ben yaptım opencode - 2026-11-15 - sqlite (winsqlite3.dll gercek C API)
struct sqlite3;
struct sqlite3_stmt;
struct OcSqliteApi {
    HMODULE module = nullptr;
    int (*open_v2)(const char*, sqlite3**, int, const char*) = nullptr;
    int (*close)(sqlite3*) = nullptr;
    const char* (*errmsg)(sqlite3*) = nullptr;
    int (*prepare_v2)(sqlite3*, const char*, int, sqlite3_stmt**, const char**) = nullptr;
    int (*step)(sqlite3_stmt*) = nullptr;
    int (*finalize)(sqlite3_stmt*) = nullptr;
    int (*column_count)(sqlite3_stmt*) = nullptr;
    const char* (*column_name)(sqlite3_stmt*, int) = nullptr;
    int (*column_type)(sqlite3_stmt*, int) = nullptr;
    long long (*column_int64)(sqlite3_stmt*, int) = nullptr;
    double (*column_double)(sqlite3_stmt*, int) = nullptr;
    const unsigned char* (*column_text)(sqlite3_stmt*, int) = nullptr;
    int (*column_bytes)(sqlite3_stmt*, int) = nullptr;
    int (*bind_text)(sqlite3_stmt*, int, const char*, int, void(*)(void*)) = nullptr;
    int (*bind_int64)(sqlite3_stmt*, int, long long) = nullptr;
    int (*bind_double)(sqlite3_stmt*, int, double) = nullptr;
    int (*bind_null)(sqlite3_stmt*, int) = nullptr;
    long long (*last_insert_rowid)(sqlite3*) = nullptr;
    int (*changes)(sqlite3*) = nullptr;
    int (*busy_timeout)(sqlite3*, int) = nullptr;
    const char* (*libversion)() = nullptr;
    bool load() {
        module = LoadLibraryW(L"winsqlite3.dll");
        if (!module) module = LoadLibraryW(L"sqlite3.dll");
        if (!module) return false;
        auto get = [&](const char* name) { return GetProcAddress(module, name); };
        open_v2 = reinterpret_cast<decltype(open_v2)>(get("sqlite3_open_v2"));
        close = reinterpret_cast<decltype(close)>(get("sqlite3_close"));
        errmsg = reinterpret_cast<decltype(errmsg)>(get("sqlite3_errmsg"));
        prepare_v2 = reinterpret_cast<decltype(prepare_v2)>(get("sqlite3_prepare_v2"));
        step = reinterpret_cast<decltype(step)>(get("sqlite3_step"));
        finalize = reinterpret_cast<decltype(finalize)>(get("sqlite3_finalize"));
        column_count = reinterpret_cast<decltype(column_count)>(get("sqlite3_column_count"));
        column_name = reinterpret_cast<decltype(column_name)>(get("sqlite3_column_name"));
        column_type = reinterpret_cast<decltype(column_type)>(get("sqlite3_column_type"));
        column_int64 = reinterpret_cast<decltype(column_int64)>(get("sqlite3_column_int64"));
        column_double = reinterpret_cast<decltype(column_double)>(get("sqlite3_column_double"));
        column_text = reinterpret_cast<decltype(column_text)>(get("sqlite3_column_text"));
        column_bytes = reinterpret_cast<decltype(column_bytes)>(get("sqlite3_column_bytes"));
        bind_text = reinterpret_cast<decltype(bind_text)>(get("sqlite3_bind_text"));
        bind_int64 = reinterpret_cast<decltype(bind_int64)>(get("sqlite3_bind_int64"));
        bind_double = reinterpret_cast<decltype(bind_double)>(get("sqlite3_bind_double"));
        bind_null = reinterpret_cast<decltype(bind_null)>(get("sqlite3_bind_null"));
        last_insert_rowid = reinterpret_cast<decltype(last_insert_rowid)>(get("sqlite3_last_insert_rowid"));
        changes = reinterpret_cast<decltype(changes)>(get("sqlite3_changes"));
        busy_timeout = reinterpret_cast<decltype(busy_timeout)>(get("sqlite3_busy_timeout"));
        libversion = reinterpret_cast<decltype(libversion)>(get("sqlite3_libversion"));
        return open_v2 && close && prepare_v2 && step && finalize;
    }
};
OcSqliteApi& ocSqliteApi() {
    static OcSqliteApi api = []() { OcSqliteApi value; value.load(); return value; }();
    return api;
}
struct OcSqliteDb { sqlite3* db = nullptr; std::mutex mutex; };
std::mutex& ocSqliteMutex() { static std::mutex mutex; return mutex; }
std::map<int, std::shared_ptr<OcSqliteDb>>& ocSqliteDbs() { static std::map<int, std::shared_ptr<OcSqliteDb>> dbs; return dbs; }
std::shared_ptr<OcSqliteDb> ocSqliteFind(int id) {
    std::lock_guard<std::mutex> lock(ocSqliteMutex());
    const auto found = ocSqliteDbs().find(id);
    return found == ocSqliteDbs().end() ? nullptr : found->second;
}
void ocSqliteBindParams(sqlite3_stmt* statement, const Value& params) {
    auto& api = ocSqliteApi();
    int index = 1;
    for (const auto& item : ocValues(params)) {
        if (std::holds_alternative<std::string>(item)) {
            const std::string text = stringOf(item);
            api.bind_text(statement, index, text.c_str(), static_cast<int>(text.size()), reinterpret_cast<void(*)(void*)>(static_cast<std::intptr_t>(-1)));
        } else if (std::holds_alternative<bool>(item)) {
            api.bind_int64(statement, index, std::get<bool>(item) ? 1 : 0);
        } else if (std::holds_alternative<std::int64_t>(item) || std::holds_alternative<std::uint64_t>(item)) {
            api.bind_int64(statement, index, static_cast<long long>(numberOf(item)));
        } else if (std::holds_alternative<double>(item)) {
            api.bind_double(statement, index, std::get<double>(item));
        } else {
            api.bind_null(statement, index);
        }
        ++index;
    }
}
Value ocSqliteRows(sqlite3_stmt* statement) {
    auto& api = ocSqliteApi();
    Value rows = stdlibArray();
    const int columns = api.column_count(statement);
    while (api.step(statement) == 100) {
        Value row = stdlibObject("SqliteRow");
        for (int column = 0; column < columns; ++column) {
            const int type = api.column_type(statement, column);
            const std::string name = api.column_name(statement, column) ? api.column_name(statement, column) : "column";
            if (type == 1) ocSetField(row, name, static_cast<std::int64_t>(api.column_int64(statement, column)));
            else if (type == 2) ocSetField(row, name, api.column_double(statement, column));
            else if (type == 3) {
                const unsigned char* text = api.column_text(statement, column);
                const int bytes = api.column_bytes(statement, column);
                ocSetField(row, name, std::string(reinterpret_cast<const char*>(text ? text : reinterpret_cast<const unsigned char*>("")), static_cast<std::size_t>(std::max(0, bytes))));
            } else if (type == 4) {
                const unsigned char* data = api.column_text(statement, column);
                const int bytes = api.column_bytes(statement, column);
                ocSetField(row, name, ocHex(data ? std::string(reinterpret_cast<const char*>(data), static_cast<std::size_t>(std::max(0, bytes))) : std::string()));
            } else ocSetField(row, name, Value{});
        }
        stdlibArrayAppend(rows, row);
    }
    return rows;
}
Value ocSqliteExec(OcSqliteDb& db, const std::string& sql, const Value& params, bool wantRows) {
    auto& api = ocSqliteApi();
    Value result = stdlibObject("SqliteResult");
    if (!db.db) { ocSetField(result, "ok", false); ocSetField(result, "error", std::string("veritabani acik degil")); return result; }
    sqlite3_stmt* statement = nullptr;
    const char* tail = sql.c_str();
    std::size_t offset = 0;
    Value allRows = stdlibArray();
    int totalChanges = 0;
    long long lastId = 0;
    bool ok = true;
    std::string error;
    do {
        statement = nullptr;
        const char* current = sql.c_str() + offset;
        if (*current == 0) break;
        if (api.prepare_v2(db.db, current, -1, &statement, &tail) != 0 || !statement) {
            error = api.errmsg(db.db) ? api.errmsg(db.db) : "SQL hazirlanamadi";
            ok = false;
            break;
        }
        if (tail) offset = static_cast<std::size_t>(tail - sql.c_str()); else offset = sql.size();
        ocSqliteBindParams(statement, params);
        const int columns = api.column_count(statement);
        if (wantRows && columns > 0) {
            const Value rows = ocSqliteRows(statement);
            for (const auto& row : ocValues(rows)) stdlibArrayAppend(allRows, row);
        } else {
            int code = api.step(statement);
            while (code == 100) code = api.step(statement);
            if (code != 101) { ok = false; error = api.errmsg(db.db) ? api.errmsg(db.db) : "SQL calistirilamadi"; }
        }
        totalChanges += api.changes(db.db);
        lastId = static_cast<long long>(api.last_insert_rowid(db.db));
        api.finalize(statement);
    } while (offset < sql.size());
    ocSetField(result, "ok", ok);
    ocSetField(result, "changes", static_cast<std::int64_t>(totalChanges));
    ocSetField(result, "last_insert_id", static_cast<std::int64_t>(lastId));
    ocSetField(result, "rows", allRows);
    ocSetField(result, "count", static_cast<std::int64_t>(ocValues(allRows).size()));
    if (!error.empty()) ocSetField(result, "error", error);
    return result;
}
void* sqliteCall(const std::string& op, void** args, int argc) {
    auto& api = ocSqliteApi();
    if (op == "version" || op == "surum") {
        return box(std::string(api.libversion && api.libversion() ? api.libversion() : "yok"));
    }
    if ((op == "open" || op == "ac") && argc >= 1) {
        if (!api.module && !api.load()) return box(std::int64_t{-1});
        sqlite3* handle = nullptr;
        const int flags = 0x00000002 | 0x00000004 | 0x00010000; // READWRITE | CREATE | FULLMUTEX
        if (api.open_v2(argumentString(args, 0).c_str(), &handle, flags, nullptr) != 0 || !handle) {
            if (handle) api.close(handle);
            return box(std::int64_t{-1});
        }
        if (api.busy_timeout) api.busy_timeout(handle, 5000);
        std::lock_guard<std::mutex> lock(ocSqliteMutex());
        static int next = 1; const int id = next++;
        auto db = std::make_shared<OcSqliteDb>(); db->db = handle;
        ocSqliteDbs()[id] = db;
        return box(static_cast<std::int64_t>(id));
    }
    auto db = argc >= 1 ? ocSqliteFind(argumentInt(args, 0)) : nullptr;
    if ((op == "exec" || op == "calistir") && argc >= 2) {
        if (!db) return box(Value{});
        std::lock_guard<std::mutex> lock(db->mutex);
        return box(ocSqliteExec(*db, argumentString(args, 1), argc >= 3 ? argument(args, 2) : Value{}, false));
    }
    if ((op == "query" || op == "sorgu") && argc >= 2) {
        if (!db) return box(stdlibArray());
        std::lock_guard<std::mutex> lock(db->mutex);
        const Value result = ocSqliteExec(*db, argumentString(args, 1), argc >= 3 ? argument(args, 2) : Value{}, true);
        return box(ocField(result, "rows"));
    }
    if ((op == "run" || op == "yurut") && argc >= 2) {
        if (!db) return box(Value{});
        std::lock_guard<std::mutex> lock(db->mutex);
        return box(ocSqliteExec(*db, argumentString(args, 1), argc >= 3 ? argument(args, 2) : Value{}, true));
    }
    if ((op == "scalar" || op == "tek") && argc >= 2) {
        if (!db) return box(Value{});
        std::lock_guard<std::mutex> lock(db->mutex);
        const Value result = ocSqliteExec(*db, argumentString(args, 1), argc >= 3 ? argument(args, 2) : Value{}, true);
        const auto rows = ocValues(ocField(result, "rows"));
        if (rows.empty()) return box(Value{});
        const auto fields = ocObjectOf(rows.front());
        return box(fields && !fields->dynamicFields.empty() ? fields->dynamicFields.begin()->second : Value{});
    }
    if ((op == "tables" || op == "tablolar") && argc >= 1) {
        if (!db) return box(stdlibArray());
        std::lock_guard<std::mutex> lock(db->mutex);
        const Value result = ocSqliteExec(*db, "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name", Value{}, true);
        Value names = stdlibArray();
        for (const auto& row : ocValues(ocField(result, "rows"))) stdlibArrayAppend(names, ocField(row, "name"));
        return box(names);
    }
    if ((op == "last_insert_id" || op == "son_id") && argc >= 1) {
        if (!db) return box(std::int64_t{0});
        std::lock_guard<std::mutex> lock(db->mutex);
        return box(static_cast<std::int64_t>(api.last_insert_rowid(db->db)));
    }
    if ((op == "changes" || op == "degisiklik") && argc >= 1) {
        if (!db) return box(std::int64_t{0});
        std::lock_guard<std::mutex> lock(db->mutex);
        return box(static_cast<std::int64_t>(api.changes(db->db)));
    }
    if ((op == "close" || op == "kapat") && argc >= 1) {
        std::lock_guard<std::mutex> lock(ocSqliteMutex());
        const auto found = ocSqliteDbs().find(argumentInt(args, 0));
        if (found != ocSqliteDbs().end()) {
            if (found->second->db) api.close(found->second->db);
            ocSqliteDbs().erase(found);
        }
        return box(true);
    }
    writeStderr("PRCYTERAS: Sqlite."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-15 - redis (gercek RESP2 istemcisi)
struct OcRedisConn { SOCKET handle = INVALID_SOCKET; std::mutex mutex; };
std::mutex& ocRedisMutex() { static std::mutex mutex; return mutex; }
std::map<int, std::shared_ptr<OcRedisConn>>& ocRedisConns() { static std::map<int, std::shared_ptr<OcRedisConn>> conns; return conns; }
std::shared_ptr<OcRedisConn> ocRedisFind(int id) {
    std::lock_guard<std::mutex> lock(ocRedisMutex());
    const auto found = ocRedisConns().find(id);
    return found == ocRedisConns().end() ? nullptr : found->second;
}
bool ocRedisReadLine(SOCKET handle, std::string& line, int timeoutMs) {
    line.clear();
    char ch = 0;
    while (true) {
        if (!ocSocketReadExact(handle, &ch, 1, timeoutMs)) return false;
        if (ch == '\r') {
            char next = 0;
            if (!ocSocketReadExact(handle, &next, 1, timeoutMs)) return false;
            return next == '\n';
        }
        line.push_back(ch);
        if (line.size() > 1048576) return false;
    }
}
Value ocRedisParse(SOCKET handle, int timeoutMs, bool& ok) {
    std::string line;
    if (!ocRedisReadLine(handle, line, timeoutMs) || line.empty()) { ok = false; return Value{}; }
    const char type = line[0];
    const std::string rest = line.substr(1);
    if (type == '+') { ok = true; return rest; }
    if (type == '-') {
        ok = false;
        Value error = stdlibObject("RedisError");
        ocSetField(error, "error", rest);
        return error;
    }
    if (type == ':') { ok = true; return static_cast<std::int64_t>(std::strtoll(rest.c_str(), nullptr, 10)); }
    if (type == '$') {
        const long long length = std::strtoll(rest.c_str(), nullptr, 10);
        if (length < 0) { ok = true; return Value{}; }
        std::string data(static_cast<std::size_t>(length), '\0');
        if (!ocSocketReadExact(handle, data.data(), static_cast<int>(length), timeoutMs)) { ok = false; return Value{}; }
        char crlf[2];
        if (!ocSocketReadExact(handle, crlf, 2, timeoutMs)) { ok = false; return Value{}; }
        ok = true;
        return data;
    }
    if (type == '*') {
        const long long count = std::strtoll(rest.c_str(), nullptr, 10);
        if (count < 0) { ok = true; return Value{}; }
        Value array = stdlibArray();
        for (long long i = 0; i < count; ++i) {
            bool itemOk = false;
            const Value item = ocRedisParse(handle, timeoutMs, itemOk);
            if (!itemOk && !std::holds_alternative<std::shared_ptr<Object>>(item)) { ok = false; return Value{}; }
            stdlibArrayAppend(array, item);
        }
        ok = true;
        return array;
    }
    ok = false;
    return Value{};
}
std::string ocRedisCommandText(const std::vector<std::string>& parts) {
    std::string out = "*" + std::to_string(parts.size()) + "\r\n";
    for (const auto& part : parts) out += "$" + std::to_string(part.size()) + "\r\n" + part + "\r\n";
    return out;
}
Value ocRedisRun(OcRedisConn& conn, const std::vector<std::string>& parts, int timeoutMs) {
    std::lock_guard<std::mutex> lock(conn.mutex);
    if (conn.handle == INVALID_SOCKET) return Value{};
    if (!ocSocketSendAll(conn.handle, ocRedisCommandText(parts))) return Value{};
    bool ok = false;
    return ocRedisParse(conn.handle, timeoutMs, ok);
}
void* redisCall(const std::string& op, void** args, int argc) {
    if ((op == "connect" || op == "baglan") && argc >= 1) {
        const int timeout = argc >= 3 ? argumentInt(args, 2) : 5000;
        SOCKET handle = INVALID_SOCKET;
        if (!ocSocketConnect(argumentString(args, 0), argc >= 2 ? argumentInt(args, 1) : 6379, timeout, handle)) return box(std::int64_t{-1});
        ocSocketTimeout(handle, timeout);
        std::lock_guard<std::mutex> lock(ocRedisMutex());
        static int next = 1; const int id = next++;
        auto conn = std::make_shared<OcRedisConn>(); conn->handle = handle;
        ocRedisConns()[id] = conn;
        return box(static_cast<std::int64_t>(id));
    }
    auto conn = argc >= 1 ? ocRedisFind(argumentInt(args, 0)) : nullptr;
    if ((op == "command" || op == "komut") && argc >= 2) {
        if (!conn) return box(Value{});
        std::vector<std::string> parts;
        for (const auto& item : ocValues(argument(args, 1))) parts.push_back(stringOf(item));
        return box(ocRedisRun(*conn, parts, argc >= 3 ? argumentInt(args, 2) : 5000));
    }
    auto simple = [&](std::vector<std::string> parts) { return conn ? ocRedisRun(*conn, parts, 5000) : Value{}; };
    if ((op == "ping") && argc >= 1) return box(simple({"PING"}));
    if ((op == "get" || op == "al") && argc >= 2) return box(simple({"GET", argumentString(args, 1)}));
    if ((op == "set" || op == "yaz") && argc >= 3) {
        std::vector<std::string> parts = {"SET", argumentString(args, 1), argumentString(args, 2)};
        if (argc >= 4 && argumentInt(args, 3) > 0) { parts.push_back("EX"); parts.push_back(std::to_string(argumentInt(args, 3))); }
        return box(simple(parts));
    }
    if ((op == "del" || op == "sil") && argc >= 2) {
        std::vector<std::string> parts = {"DEL"};
        for (int i = 1; i < argc; ++i) parts.push_back(argumentString(args, i));
        return box(simple(parts));
    }
    if ((op == "exists" || op == "var_mi") && argc >= 2) return box(simple({"EXISTS", argumentString(args, 1)}));
    if ((op == "incr" || op == "arttir") && argc >= 2) return box(simple({"INCR", argumentString(args, 1)}));
    if ((op == "expire" || op == "sure") && argc >= 3) return box(simple({"EXPIRE", argumentString(args, 1), std::to_string(argumentInt(args, 2))}));
    if ((op == "ttl") && argc >= 2) return box(simple({"TTL", argumentString(args, 1)}));
    if ((op == "hset") && argc >= 4) return box(simple({"HSET", argumentString(args, 1), argumentString(args, 2), argumentString(args, 3)}));
    if ((op == "hget") && argc >= 3) return box(simple({"HGET", argumentString(args, 1), argumentString(args, 2)}));
    if ((op == "hgetall") && argc >= 2) return box(simple({"HGETALL", argumentString(args, 1)}));
    if ((op == "lpush") && argc >= 3) return box(simple({"LPUSH", argumentString(args, 1), argumentString(args, 2)}));
    if ((op == "lrange") && argc >= 4) return box(simple({"LRANGE", argumentString(args, 1), std::to_string(argumentInt(args, 2)), std::to_string(argumentInt(args, 3))}));
    if ((op == "publish" || op == "yayinla") && argc >= 3) return box(simple({"PUBLISH", argumentString(args, 1), argumentString(args, 2)}));
    if ((op == "select") && argc >= 2) return box(simple({"SELECT", std::to_string(argumentInt(args, 1))}));
    if ((op == "auth") && argc >= 2) return box(simple({"AUTH", argumentString(args, 1)}));
    if ((op == "info") && argc >= 1) return box(simple({"INFO"}));
    if ((op == "dbsize") && argc >= 1) return box(simple({"DBSIZE"}));
    if ((op == "flushdb") && argc >= 1) return box(simple({"FLUSHDB"}));
    if ((op == "close" || op == "kapat") && argc >= 1) {
        std::lock_guard<std::mutex> lock(ocRedisMutex());
        const auto found = ocRedisConns().find(argumentInt(args, 0));
        if (found != ocRedisConns().end()) {
            if (found->second->handle != INVALID_SOCKET) closesocket(found->second->handle);
            ocRedisConns().erase(found);
        }
        return box(true);
    }
    writeStderr("PRCYTERAS: Redis."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-15 - mysql (gercek MySQL protokolu, TLS'siz)
struct OcMySqlConn { SOCKET handle = INVALID_SOCKET; std::mutex mutex; unsigned char sequence = 0; std::string server; };
std::mutex& ocMySqlMutex() { static std::mutex mutex; return mutex; }
std::map<int, std::shared_ptr<OcMySqlConn>>& ocMySqlConns() { static std::map<int, std::shared_ptr<OcMySqlConn>> conns; return conns; }
std::shared_ptr<OcMySqlConn> ocMySqlFind(int id) {
    std::lock_guard<std::mutex> lock(ocMySqlMutex());
    const auto found = ocMySqlConns().find(id);
    return found == ocMySqlConns().end() ? nullptr : found->second;
}
bool ocMyReadPacket(SOCKET handle, std::vector<unsigned char>& payload, unsigned char& sequence, int timeoutMs) {
    unsigned char header[4];
    if (!ocSocketReadExact(handle, header, 4, timeoutMs)) return false;
    const std::size_t length = static_cast<std::size_t>(header[0]) | (static_cast<std::size_t>(header[1]) << 8) | (static_cast<std::size_t>(header[2]) << 16);
    sequence = header[3];
    payload.resize(length);
    return length == 0 || ocSocketReadExact(handle, payload.data(), static_cast<int>(length), timeoutMs);
}
bool ocMyWritePacket(SOCKET handle, unsigned char& sequence, const std::vector<unsigned char>& payload) {
    std::vector<unsigned char> packet(4 + payload.size());
    packet[0] = static_cast<unsigned char>(payload.size() & 0xFF);
    packet[1] = static_cast<unsigned char>((payload.size() >> 8) & 0xFF);
    packet[2] = static_cast<unsigned char>((payload.size() >> 16) & 0xFF);
    packet[3] = sequence++;
    std::copy(payload.begin(), payload.end(), packet.begin() + 4);
    return ocSocketSendAll(handle, std::string(reinterpret_cast<const char*>(packet.data()), packet.size()));
}
void ocMyPut16(std::vector<unsigned char>& out, unsigned int value) { out.push_back(value & 0xFF); out.push_back((value >> 8) & 0xFF); }
void ocMyPut32(std::vector<unsigned char>& out, std::uint32_t value) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>((value >> (i * 8)) & 0xFF)); }
std::string ocMyNullString(const std::vector<unsigned char>& data, std::size_t& pos) {
    const std::size_t start = pos;
    while (pos < data.size() && data[pos] != 0) ++pos;
    std::string out(reinterpret_cast<const char*>(data.data() + start), pos - start);
    if (pos < data.size()) ++pos;
    return out;
}
std::size_t ocMyLenEnc(const std::vector<unsigned char>& data, std::size_t& pos) {
    if (pos >= data.size()) return 0;
    const unsigned char first = data[pos++];
    if (first < 0xFB) return first;
    if (first == 0xFC && pos + 2 <= data.size()) { const std::size_t value = data[pos] | (data[pos + 1] << 8); pos += 2; return value; }
    if (first == 0xFD && pos + 3 <= data.size()) { const std::size_t value = data[pos] | (data[pos + 1] << 8) | (data[pos + 2] << 16); pos += 3; return value; }
    if (first == 0xFE && pos + 8 <= data.size()) { std::size_t value = 0; for (int i = 0; i < 8; ++i) value |= static_cast<std::size_t>(data[pos + i]) << (i * 8); pos += 8; return value; }
    return 0;
}
std::string ocMyLenEncString(const std::vector<unsigned char>& data, std::size_t& pos, bool& isNull) {
    isNull = false;
    if (pos >= data.size()) { isNull = true; return {}; }
    if (data[pos] == 0xFB) { ++pos; isNull = true; return {}; }
    const std::size_t length = ocMyLenEnc(data, pos);
    if (pos + length > data.size()) { isNull = true; return {}; }
    std::string out(reinterpret_cast<const char*>(data.data() + pos), length);
    pos += length;
    return out;
}
void ocMyPutLenEnc(std::vector<unsigned char>& out, std::size_t value) {
    if (value < 0xFB) out.push_back(static_cast<unsigned char>(value));
    else if (value <= 0xFFFF) { out.push_back(0xFC); ocMyPut16(out, static_cast<unsigned int>(value)); }
    else if (value <= 0xFFFFFF) { out.push_back(0xFD); out.push_back(value & 0xFF); out.push_back((value >> 8) & 0xFF); out.push_back((value >> 16) & 0xFF); }
    else { out.push_back(0xFE); for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>((value >> (i * 8)) & 0xFF)); }
}
std::string ocMyNativePassword(const std::string& password, const std::string& scramble) {
    if (password.empty()) return {};
    std::string stage1 = ocHashRaw(BCRYPT_SHA1_ALGORITHM, password);
    std::string stage2 = ocHashRaw(BCRYPT_SHA1_ALGORITHM, stage1);
    std::string combined = scramble.substr(0, 20) + stage2;
    std::string stage3 = ocHashRaw(BCRYPT_SHA1_ALGORITHM, combined);
    std::string out(stage1.size(), '\0');
    for (std::size_t i = 0; i < stage1.size(); ++i) out[i] = static_cast<char>(stage1[i] ^ stage3[i]);
    return out;
}
std::string ocMyCachingSha2(const std::string& password, const std::string& scramble) {
    if (password.empty()) return {};
    std::string stage1 = ocSha256Raw(password);
    std::string stage2 = ocSha256Raw(stage1);
    std::string stage3 = ocSha256Raw(stage2 + scramble.substr(0, 20));
    std::string out(stage1.size(), '\0');
    for (std::size_t i = 0; i < stage1.size(); ++i) out[i] = static_cast<char>(stage1[i] ^ stage3[i]);
    return out;
}
std::string ocMyAuthResponse(const std::string& plugin, const std::string& password, const std::string& scramble) {
    if (plugin == "mysql_native_password") return ocMyNativePassword(password, scramble);
    if (plugin == "caching_sha2_password") return ocMyCachingSha2(password, scramble);
    if (plugin == "mysql_clear_password") return password + '\0';
    return ocMyNativePassword(password, scramble);
}
Value ocMyErrorValue(const std::vector<unsigned char>& payload) {
    Value error = stdlibObject("MySqlError");
    int code = 0;
    std::string message;
    if (payload.size() >= 3) { code = payload[1] | (payload[2] << 8); message.assign(reinterpret_cast<const char*>(payload.data() + 3), payload.size() - 3); }
    ocSetField(error, "error", message); ocSetField(error, "code", static_cast<std::int64_t>(code));
    return error;
}
Value ocMyQueryResult(const std::vector<unsigned char>& first, SOCKET handle, int timeoutMs) {
    if (first.empty()) return Value{};
    if (first[0] == 0xFF) return ocMyErrorValue(first);
    Value out = stdlibObject("MySqlResult");
    if (first[0] == 0x00) {
        std::size_t pos = 1;
        const std::size_t affected = ocMyLenEnc(first, pos);
        const std::size_t lastId = ocMyLenEnc(first, pos);
        ocSetField(out, "ok", true); ocSetField(out, "affected_rows", static_cast<std::int64_t>(affected));
        ocSetField(out, "last_insert_id", static_cast<std::int64_t>(lastId));
        ocSetField(out, "rows", stdlibArray());
        return out;
    }
    std::size_t pos = 0;
    const std::size_t columnCount = ocMyLenEnc(first, pos);
    std::vector<std::string> names;
    for (std::size_t i = 0; i < columnCount; ++i) {
        std::vector<unsigned char> packet; unsigned char sequence = 0;
        if (!ocMyReadPacket(handle, packet, sequence, timeoutMs)) return Value{};
        std::size_t p = 0;
        bool isNull = false;
        ocMyLenEncString(packet, p, isNull); // catalog
        ocMyLenEncString(packet, p, isNull); // schema
        ocMyLenEncString(packet, p, isNull); // table
        ocMyLenEncString(packet, p, isNull); // org table
        names.push_back(ocMyLenEncString(packet, p, isNull));
        ocMyLenEncString(packet, p, isNull);
    }
    std::vector<unsigned char> eofPacket; unsigned char sequence = 0;
    ocMyReadPacket(handle, eofPacket, sequence, timeoutMs);
    Value rows = stdlibArray();
    while (true) {
        std::vector<unsigned char> packet;
        if (!ocMyReadPacket(handle, packet, sequence, timeoutMs)) break;
        if (!packet.empty() && packet[0] == 0xFE && packet.size() < 9) break;
        if (!packet.empty() && packet[0] == 0xFF) { stdlibArrayAppend(rows, ocMyErrorValue(packet)); break; }
        std::size_t p = 0;
        Value row = stdlibObject("MySqlRow");
        for (std::size_t i = 0; i < columnCount; ++i) {
            bool isNull = false;
            const std::string value = ocMyLenEncString(packet, p, isNull);
            ocSetField(row, i < names.size() ? names[i] : ("column" + std::to_string(i)), isNull ? Value{} : Value(value));
        }
        stdlibArrayAppend(rows, row);
    }
    ocSetField(out, "ok", true); ocSetField(out, "columns", ocStringArray(names));
    ocSetField(out, "rows", rows); ocSetField(out, "count", static_cast<std::int64_t>(ocValues(rows).size()));
    return out;
}
Value ocMyCommand(OcMySqlConn& conn, const std::string& sql, int timeoutMs) {
    std::lock_guard<std::mutex> lock(conn.mutex);
    if (conn.handle == INVALID_SOCKET) return Value{};
    std::vector<unsigned char> payload; payload.push_back(0x03); payload.insert(payload.end(), sql.begin(), sql.end());
    conn.sequence = 0;
    if (!ocMyWritePacket(conn.handle, conn.sequence, payload)) return Value{};
    std::vector<unsigned char> first; unsigned char sequence = 0;
    if (!ocMyReadPacket(conn.handle, first, sequence, timeoutMs)) return Value{};
    return ocMyQueryResult(first, conn.handle, timeoutMs);
}
void* mysqlCall(const std::string& op, void** args, int argc) {
    if ((op == "connect" || op == "baglan") && argc >= 1) {
        const std::string host = argumentString(args, 0);
        const int port = argc >= 2 ? argumentInt(args, 1) : 3306;
        const std::string user = argc >= 3 ? argumentString(args, 2) : std::string("root");
        const std::string password = argc >= 4 ? argumentString(args, 3) : std::string();
        const std::string database = argc >= 5 ? argumentString(args, 4) : std::string();
        const int timeout = argc >= 6 ? argumentInt(args, 5) : 5000;
        SOCKET handle = INVALID_SOCKET;
        if (!ocSocketConnect(host, port, timeout, handle)) return box(std::int64_t{-1});
        ocSocketTimeout(handle, timeout);
        std::vector<unsigned char> handshake; unsigned char sequence = 0;
        if (!ocMyReadPacket(handle, handshake, sequence, timeout) || handshake.size() < 32) { closesocket(handle); return box(std::int64_t{-1}); }
        std::size_t pos = 0;
        const unsigned char protocol = handshake[pos++];
        if (protocol != 10) { closesocket(handle); return box(std::int64_t{-1}); }
        const std::string serverVersion = ocMyNullString(handshake, pos);
        pos += 4; // connection id
        std::string auth1(reinterpret_cast<const char*>(handshake.data() + pos), 8); pos += 8;
        ++pos; // filler
        unsigned int capabilities = 0;
        if (pos + 2 <= handshake.size()) { capabilities = handshake[pos] | (handshake[pos + 1] << 8); pos += 2; }
        unsigned char charset = 45;
        if (pos + 1 <= handshake.size()) charset = handshake[pos++];
        pos += 2; // status
        unsigned int upper = 0;
        if (pos + 2 <= handshake.size()) { upper = handshake[pos] | (handshake[pos + 1] << 8); pos += 2; }
        capabilities |= upper << 16;
        unsigned char authLength = 0;
        if (pos < handshake.size()) authLength = handshake[pos++];
        pos += 10; // reserved
        std::string auth2;
        if (pos < handshake.size()) {
            const std::size_t length = std::max<std::size_t>(13, authLength > 8 ? authLength - 8 : 13);
            const std::size_t available = std::min(length, handshake.size() - pos);
            auth2.assign(reinterpret_cast<const char*>(handshake.data() + pos), available);
            pos += available;
            while (!auth2.empty() && auth2.back() == 0) auth2.pop_back();
        }
        std::string plugin;
        if (pos < handshake.size()) plugin = ocMyNullString(handshake, pos);
        if (plugin.empty()) plugin = "mysql_native_password";
        const std::string scramble = auth1 + auth2;
        const std::uint32_t clientFlags = 0x00000001u | 0x00000004u | 0x00000200u | 0x00008000u | 0x00020000u | 0x00080000u | 0x00200000u | (!database.empty() ? 0x00000008u : 0u);
        std::vector<unsigned char> auth;
        ocMyPut32(auth, clientFlags);
        ocMyPut32(auth, 0x01000000u);
        auth.push_back(charset);
        auth.insert(auth.end(), 23, 0);
        auth.insert(auth.end(), user.begin(), user.end()); auth.push_back(0);
        const std::string response = ocMyAuthResponse(plugin, password, scramble);
        auth.push_back(static_cast<unsigned char>(response.size()));
        auth.insert(auth.end(), response.begin(), response.end());
        if (!database.empty()) { auth.insert(auth.end(), database.begin(), database.end()); auth.push_back(0); }
        auth.insert(auth.end(), plugin.begin(), plugin.end()); auth.push_back(0);
        sequence = 1;
        if (!ocMyWritePacket(handle, sequence, auth)) { closesocket(handle); return box(std::int64_t{-1}); }
        bool authenticated = false;
        for (int attempt = 0; attempt < 4 && !authenticated; ++attempt) {
            std::vector<unsigned char> packet;
            if (!ocMyReadPacket(handle, packet, sequence, timeout)) break;
            if (packet.empty()) break;
            if (packet[0] == 0x00) { authenticated = true; break; }
            if (packet[0] == 0xFF) break;
            if (packet[0] == 0xFE && packet.size() > 1) {
                std::size_t p = 1;
                const std::string newPlugin = ocMyNullString(packet, p);
                std::string newScramble(reinterpret_cast<const char*>(packet.data() + p), packet.size() - p);
                while (!newScramble.empty() && newScramble.back() == 0) newScramble.pop_back();
                const std::string newResponse = ocMyAuthResponse(newPlugin, password, newScramble);
                std::vector<unsigned char> answer(newResponse.begin(), newResponse.end());
                if (!ocMyWritePacket(handle, sequence, answer)) break;
                continue;
            }
            if (packet[0] == 0x01) {
                if (packet.size() > 1 && packet[1] == 0x03) continue;
                break; // full auth requires TLS/RSA
            }
        }
        if (!authenticated) { closesocket(handle); return box(std::int64_t{-1}); }
        std::lock_guard<std::mutex> lock(ocMySqlMutex());
        static int next = 1; const int id = next++;
        auto conn = std::make_shared<OcMySqlConn>(); conn->handle = handle; conn->server = serverVersion;
        ocMySqlConns()[id] = conn;
        return box(static_cast<std::int64_t>(id));
    }
    auto conn = argc >= 1 ? ocMySqlFind(argumentInt(args, 0)) : nullptr;
    if ((op == "query" || op == "sorgu") && argc >= 2) {
        if (!conn) return box(Value{});
        return box(ocMyCommand(*conn, argumentString(args, 1), argc >= 3 ? argumentInt(args, 2) : 30000));
    }
    if ((op == "exec" || op == "calistir") && argc >= 2) {
        if (!conn) return box(Value{});
        const Value result = ocMyCommand(*conn, argumentString(args, 1), 30000);
        if (auto object = ocObjectOf(result)) ocSetField(result, "rows", Value{});
        return box(result);
    }
    if ((op == "scalar" || op == "tek") && argc >= 2) {
        if (!conn) return box(Value{});
        const Value result = ocMyCommand(*conn, argumentString(args, 1), 30000);
        const auto rows = ocValues(ocField(result, "rows"));
        if (rows.empty()) return box(Value{});
        const auto fields = ocObjectOf(rows.front());
        return box(fields && !fields->dynamicFields.empty() ? fields->dynamicFields.begin()->second : Value{});
    }
    if ((op == "server" || op == "sunucu") && argc >= 1) return box(conn ? conn->server : std::string());
    if ((op == "close" || op == "kapat") && argc >= 1) {
        std::lock_guard<std::mutex> lock(ocMySqlMutex());
        const auto found = ocMySqlConns().find(argumentInt(args, 0));
        if (found != ocMySqlConns().end()) {
            if (found->second->handle != INVALID_SOCKET) closesocket(found->second->handle);
            ocMySqlConns().erase(found);
        }
        return box(true);
    }
    writeStderr("PRCYTERAS: MySql."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-15 - smtp (gercek SMTP istemcisi)
struct OcSmtpSession { SOCKET handle = INVALID_SOCKET; std::string host; std::mutex mutex; };
std::mutex& ocSmtpMutex() { static std::mutex mutex; return mutex; }
std::map<int, std::shared_ptr<OcSmtpSession>>& ocSmtpSessions() { static std::map<int, std::shared_ptr<OcSmtpSession>> sessions; return sessions; }
std::shared_ptr<OcSmtpSession> ocSmtpFind(int id) {
    std::lock_guard<std::mutex> lock(ocSmtpMutex());
    const auto found = ocSmtpSessions().find(id);
    return found == ocSmtpSessions().end() ? nullptr : found->second;
}
std::string ocSmtpReadReply(SOCKET handle, int timeoutMs) {
    std::string reply;
    while (true) {
        std::string line;
        if (!ocRedisReadLine(handle, line, timeoutMs)) break;
        reply += line + "\n";
        if (line.size() < 4 || line[3] != '-') break;
    }
    return reply;
}
std::string ocSmtpCommand(OcSmtpSession& session, const std::string& command, int timeoutMs) {
    if (!ocSocketSendAll(session.handle, command + "\r\n")) return {};
    return ocSmtpReadReply(session.handle, timeoutMs);
}
std::string ocMimeHeader(const std::string& value) {
    std::string out;
    for (const char ch : value) { if (ch == '\r' || ch == '\n') continue; out.push_back(ch); }
    return out;
}
std::string ocRfc2822Date() {
    SYSTEMTIME st{}; GetSystemTime(&st);
    static const char* days[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* months[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%s, %02d %s %04d %02d:%02d:%02d +0000",
        days[std::min<int>(6, st.wDayOfWeek)], st.wDay, months[std::min<int>(11, std::max<int>(1, st.wMonth) - 1)],
        st.wYear, st.wHour, st.wMinute, st.wSecond);
    return buffer;
}
void* smtpCall(const std::string& op, void** args, int argc) {
    if ((op == "connect" || op == "baglan") && argc >= 1) {
        const std::string host = argumentString(args, 0);
        const int port = argc >= 2 ? argumentInt(args, 1) : 587;
        const int timeout = argc >= 3 ? argumentInt(args, 2) : 10000;
        SOCKET handle = INVALID_SOCKET;
        if (!ocSocketConnect(host, port, timeout, handle)) return box(std::int64_t{-1});
        ocSocketTimeout(handle, timeout);
        const std::string greeting = ocSmtpReadReply(handle, timeout);
        if (greeting.rfind("220", 0) != 0) { closesocket(handle); return box(std::int64_t{-1}); }
        std::lock_guard<std::mutex> lock(ocSmtpMutex());
        static int next = 1; const int id = next++;
        auto session = std::make_shared<OcSmtpSession>(); session->handle = handle; session->host = host;
        ocSmtpSessions()[id] = session;
        return box(static_cast<std::int64_t>(id));
    }
    auto session = argc >= 1 ? ocSmtpFind(argumentInt(args, 0)) : nullptr;
    if ((op == "ehlo" || op == "helo") && argc >= 2) {
        if (!session) return box(std::string());
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(ocSmtpCommand(*session, "EHLO " + ocMimeHeader(argumentString(args, 1)), 10000));
    }
    if ((op == "auth" || op == "kimlik") && argc >= 3) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        const std::string user = argumentString(args, 1), password = argumentString(args, 2);
        const std::string method = argc >= 4 ? argumentString(args, 3) : std::string("LOGIN");
        if (method == "PLAIN") {
            const std::string token = base64Encode(std::string("\0", 1) + user + std::string("\0", 1) + password, false);
            const std::string reply = ocSmtpCommand(*session, "AUTH PLAIN " + token, 10000);
            return box(reply.rfind("235", 0) == 0);
        }
        std::string reply = ocSmtpCommand(*session, "AUTH LOGIN", 10000);
        if (reply.rfind("334", 0) != 0) return box(false);
        reply = ocSmtpCommand(*session, base64Encode(user, false), 10000);
        if (reply.rfind("334", 0) != 0) return box(false);
        reply = ocSmtpCommand(*session, base64Encode(password, false), 10000);
        return box(reply.rfind("235", 0) == 0);
    }
    if ((op == "send" || op == "gonder") && argc >= 5) {
        if (!session) return box(Value{});
        std::lock_guard<std::mutex> lock(session->mutex);
        const std::string from = argumentString(args, 1), to = argumentString(args, 2);
        const std::string subject = argumentString(args, 3), body = argumentString(args, 4);
        const bool html = argc >= 6 ? truthy(argument(args, 5)) : false;
        std::string reply = ocSmtpCommand(*session, "MAIL FROM:<" + ocMimeHeader(from) + ">", 10000);
        if (reply.rfind("250", 0) != 0) return box(reply);
        reply = ocSmtpCommand(*session, "RCPT TO:<" + ocMimeHeader(to) + ">", 10000);
        if (reply.rfind("250", 0) != 0 && reply.rfind("251", 0) != 0) return box(reply);
        reply = ocSmtpCommand(*session, "DATA", 10000);
        if (reply.rfind("354", 0) != 0) return box(reply);
        std::string message;
        message += "From: " + ocMimeHeader(from) + "\r\nTo: " + ocMimeHeader(to) + "\r\nSubject: " + ocMimeHeader(subject) + "\r\n";
        message += "MIME-Version: 1.0\r\nContent-Type: " + std::string(html ? "text/html" : "text/plain") + "; charset=UTF-8\r\n";
        message += "Date: " + ocRfc2822Date() + "\r\n";
        message += "\r\n" + body + "\r\n.\r\n";
        if (!ocSocketSendAll(session->handle, message)) return box(std::string());
        return box(ocSmtpReadReply(session->handle, 15000));
    }
    if ((op == "raw" || op == "ham") && argc >= 2) {
        if (!session) return box(std::string());
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(ocSmtpCommand(*session, argumentString(args, 1), 10000));
    }
    if ((op == "quit" || op == "cik") && argc >= 1) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        ocSmtpCommand(*session, "QUIT", 5000);
        closesocket(session->handle);
        session->handle = INVALID_SOCKET;
        return box(true);
    }
    if ((op == "close" || op == "kapat") && argc >= 1) {
        std::lock_guard<std::mutex> lock(ocSmtpMutex());
        const auto found = ocSmtpSessions().find(argumentInt(args, 0));
        if (found != ocSmtpSessions().end()) {
            if (found->second->handle != INVALID_SOCKET) closesocket(found->second->handle);
            ocSmtpSessions().erase(found);
        }
        return box(true);
    }
    writeStderr("PRCYTERAS: Smtp."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-15 - irc (gercek IRC istemcisi)
struct OcIrcSession { SOCKET handle = INVALID_SOCKET; std::string nick; std::mutex mutex; };
std::mutex& ocIrcMutex() { static std::mutex mutex; return mutex; }
std::map<int, std::shared_ptr<OcIrcSession>>& ocIrcSessions() { static std::map<int, std::shared_ptr<OcIrcSession>> sessions; return sessions; }
std::shared_ptr<OcIrcSession> ocIrcFind(int id) {
    std::lock_guard<std::mutex> lock(ocIrcMutex());
    const auto found = ocIrcSessions().find(id);
    return found == ocIrcSessions().end() ? nullptr : found->second;
}
void* ircCall(const std::string& op, void** args, int argc) {
    if ((op == "connect" || op == "baglan") && argc >= 1) {
        const std::string host = argumentString(args, 0);
        const int port = argc >= 2 ? argumentInt(args, 1) : 6667;
        const int timeout = argc >= 3 ? argumentInt(args, 2) : 10000;
        SOCKET handle = INVALID_SOCKET;
        if (!ocSocketConnect(host, port, timeout, handle)) return box(std::int64_t{-1});
        ocSocketTimeout(handle, timeout);
        std::lock_guard<std::mutex> lock(ocIrcMutex());
        static int next = 1; const int id = next++;
        auto session = std::make_shared<OcIrcSession>(); session->handle = handle;
        ocIrcSessions()[id] = session;
        return box(static_cast<std::int64_t>(id));
    }
    auto session = argc >= 1 ? ocIrcFind(argumentInt(args, 0)) : nullptr;
    auto sendRaw = [&](const std::string& line) { return session && ocSocketSendAll(session->handle, line + "\r\n"); };
    if ((op == "login" || op == "giris") && argc >= 2) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        session->nick = argumentString(args, 1);
        const std::string realName = argc >= 3 ? argumentString(args, 2) : session->nick;
        sendRaw("NICK " + ocMimeHeader(session->nick));
        sendRaw("USER " + ocMimeHeader(session->nick) + " 0 * :" + ocMimeHeader(realName));
        return box(true);
    }
    if ((op == "join" || op == "katil") && argc >= 2) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(sendRaw("JOIN " + ocMimeHeader(argumentString(args, 1))));
    }
    if ((op == "privmsg" || op == "mesaj") && argc >= 3) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(sendRaw("PRIVMSG " + ocMimeHeader(argumentString(args, 1)) + " :" + argumentString(args, 2)));
    }
    if ((op == "raw" || op == "ham") && argc >= 2) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(sendRaw(argumentString(args, 1)));
    }
    if ((op == "read" || op == "oku") && argc >= 1) {
        if (!session) return box(std::string());
        std::lock_guard<std::mutex> lock(session->mutex);
        std::string line;
        if (!ocRedisReadLine(session->handle, line, argc >= 2 ? argumentInt(args, 1) : 5000)) return box(std::string());
        if (line.rfind("PING", 0) == 0) { sendRaw("PONG " + line.substr(5)); return box(line); }
        return box(line);
    }
    if ((op == "pong") && argc >= 2) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(sendRaw("PONG :" + argumentString(args, 1)));
    }
    if ((op == "part") && argc >= 2) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        return box(sendRaw("PART " + ocMimeHeader(argumentString(args, 1))));
    }
    if ((op == "quit" || op == "cik") && argc >= 1) {
        if (!session) return box(false);
        std::lock_guard<std::mutex> lock(session->mutex);
        sendRaw("QUIT :PRCYTERAS");
        closesocket(session->handle); session->handle = INVALID_SOCKET;
        return box(true);
    }
    if ((op == "close" || op == "kapat") && argc >= 1) {
        std::lock_guard<std::mutex> lock(ocIrcMutex());
        const auto found = ocIrcSessions().find(argumentInt(args, 0));
        if (found != ocIrcSessions().end()) {
            if (found->second->handle != INVALID_SOCKET) closesocket(found->second->handle);
            ocIrcSessions().erase(found);
        }
        return box(true);
    }
    writeStderr("PRCYTERAS: Irc."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-15 - webhook (gercek JSON POST)
void* webhookCall(const std::string& op, void** args, int argc) {
    auto post = [&](const std::string& url, const std::string& body, const std::string& contentType, const Value& extra) {
        auto headers = ocHeaderList(extra);
        headers.push_back({"Content-Type", contentType});
        headers.push_back({"User-Agent", "PRCYTERAS-Webhook/1.0"});
        return ocHttpResponseValue(ocHttpRequest("POST", url, body, headers, 15000));
    };
    if ((op == "send" || op == "gonder") && argc >= 2) {
        return box(post(argumentString(args, 0), argumentString(args, 1),
            argc >= 3 ? argumentString(args, 2) : std::string("application/json"),
            argc >= 4 ? argument(args, 3) : Value{}));
    }
    if ((op == "json" || op == "json_gonder") && argc >= 2) {
        return box(post(argumentString(args, 0), ocJsonText(argument(args, 1)), "application/json", Value{}));
    }
    if ((op == "discord") && argc >= 2) {
        Value payload = stdlibObject("DiscordWebhook");
        ocSetField(payload, "content", argumentString(args, 1));
        if (argc >= 3 && !argumentString(args, 2).empty()) ocSetField(payload, "username", argumentString(args, 2));
        return box(post(argumentString(args, 0), ocJsonText(payload), "application/json", Value{}));
    }
    if ((op == "slack") && argc >= 2) {
        Value payload = stdlibObject("SlackWebhook");
        ocSetField(payload, "text", argumentString(args, 1));
        if (argc >= 3 && !argumentString(args, 2).empty()) ocSetField(payload, "channel", argumentString(args, 2));
        return box(post(argumentString(args, 0), ocJsonText(payload), "application/json", Value{}));
    }
    writeStderr("PRCYTERAS: Webhook."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}

// ben yaptım opencode - 2026-11-15 - API istemcileri (gercek HTTPS + JSON)
Value ocJsonIndex(const Value& value, int index) {
    const auto items = ocValues(value);
    if (index < 0) index += static_cast<int>(items.size());
    return index >= 0 && index < static_cast<int>(items.size()) ? items[static_cast<std::size_t>(index)] : Value{};
}
std::string ocJsonQuote(const std::string& text) { return ocJsonText(Value(text)); }
Value ocApiValue(const OcHttpResponse& response) {
    Value out = stdlibObject("ApiResponse");
    ocSetField(out, "ok", response.ok);
    ocSetField(out, "status", static_cast<std::int64_t>(response.status));
    ocSetField(out, "body", response.body);
    bool parsed = false;
    const Value document = ocParseJson(response.body, parsed);
    ocSetField(out, "json", parsed ? document : Value{});
    ocSetField(out, "error", response.error);
    return out;
}
std::string ocMessagesJson(const Value& messages) {
    if (std::holds_alternative<std::string>(messages)) {
        return "[{\"role\":\"user\",\"content\":" + ocJsonQuote(stringOf(messages)) + "}]";
    }
    std::string out = "[";
    bool first = true;
    for (const auto& item : ocValues(messages)) {
        const std::string role = ocJsonString(item, "role");
        const Value content = ocField(item, "content");
        if (!first) out += ",";
        first = false;
        out += "{\"role\":" + ocJsonQuote(role.empty() ? std::string("user") : role) + ",\"content\":" +
               (std::holds_alternative<std::string>(content) ? ocJsonQuote(stringOf(content)) : ocJsonText(content)) + "}";
    }
    return out + "]";
}
std::string ocMessagesText(const Value& messages) {
    if (std::holds_alternative<std::string>(messages)) return stringOf(messages);
    std::string out;
    for (const auto& item : ocValues(messages)) {
        const std::string text = ocJsonString(item, "content");
        if (!text.empty()) { if (!out.empty()) out += "\n"; out += text; }
    }
    return out;
}
struct OcAiDefaults { std::string base, path, authHeader, authPrefix, version; bool ollama = false, google = false, hf = false; };
OcAiDefaults ocAiDefaults(const std::string& provider) {
    if (provider == "openai") return {"https://api.openai.com/v1", "/chat/completions", "Authorization", "Bearer ", ""};
    if (provider == "groq") return {"https://api.groq.com/openai/v1", "/chat/completions", "Authorization", "Bearer ", ""};
    if (provider == "ollama") return {"http://127.0.0.1:11434", "/api/chat", "", "", ""};
    if (provider == "anthropic") return {"https://api.anthropic.com", "/v1/messages", "x-api-key", "", "2023-06-01"};
    if (provider == "google") return {"https://generativelanguage.googleapis.com", "", "", "", ""};
    if (provider == "huggingface") return {"https://api-inference.huggingface.co", "", "Authorization", "Bearer ", ""};
    return {"", "", "", "", ""};
}
Value ocAiChat(const std::string& provider, void** args, int argc) {
    const std::string key = argumentString(args, 0);
    const std::string model = argumentString(args, 1);
    const Value messages = argument(args, 2);
    const double temperature = argc >= 4 ? numberOf(argument(args, 3)) : 0.7;
    const int maxTokens = argc >= 5 ? std::clamp(argumentInt(args, 4), 1, 32768) : 1024;
    OcAiDefaults defaults = ocAiDefaults(provider);
    if (argc >= 6 && !argumentString(args, 5).empty()) defaults.base = argumentString(args, 5);
    std::vector<std::pair<std::string,std::string>> headers;
    if (!defaults.authHeader.empty() && !key.empty()) headers.push_back({defaults.authHeader, defaults.authPrefix + key});
    if (!defaults.version.empty()) headers.push_back({"anthropic-version", defaults.version});
    std::string url = defaults.base;
    std::string body;
    if (provider == "google") {
        url += "/v1beta/models/" + model + ":generateContent?key=" + ocUrlEncode(key);
        body = "{\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":" + ocJsonQuote(ocMessagesText(messages)) +
               "}]}],\"generationConfig\":{\"temperature\":" + std::to_string(temperature) + ",\"maxOutputTokens\":" + std::to_string(maxTokens) + "}}";
    } else if (provider == "huggingface") {
        url += "/models/" + model;
        body = "{\"inputs\":" + ocJsonQuote(ocMessagesText(messages)) + ",\"parameters\":{\"temperature\":" + std::to_string(temperature) +
               ",\"max_new_tokens\":" + std::to_string(maxTokens) + "}}";
    } else if (provider == "ollama") {
        url += defaults.path;
        body = "{\"model\":" + ocJsonQuote(model) + ",\"messages\":" + ocMessagesJson(messages) + ",\"stream\":false,\"options\":{\"temperature\":" +
               std::to_string(temperature) + ",\"num_predict\":" + std::to_string(maxTokens) + "}}";
    } else if (provider == "anthropic") {
        url += defaults.path;
        body = "{\"model\":" + ocJsonQuote(model) + ",\"max_tokens\":" + std::to_string(maxTokens) + ",\"temperature\":" +
               std::to_string(temperature) + ",\"messages\":" + ocMessagesJson(messages) + "}";
    } else {
        url += defaults.path;
        body = "{\"model\":" + ocJsonQuote(model) + ",\"messages\":" + ocMessagesJson(messages) + ",\"temperature\":" +
               std::to_string(temperature) + ",\"max_tokens\":" + std::to_string(maxTokens) + "}";
    }
    headers.push_back({"Content-Type", "application/json"});
    const OcHttpResponse response = ocHttpRequest("POST", url, body, headers, 60000);
    Value out = stdlibObject("AiChatResult");
    ocSetField(out, "ok", response.ok);
    ocSetField(out, "status", static_cast<std::int64_t>(response.status));
    ocSetField(out, "error", response.error);
    bool parsed = false;
    const Value document = ocParseJson(response.body, parsed);
    std::string text;
    if (parsed) {
        if (provider == "google") {
            const Value candidates = ocField(document, "candidates");
            const Value content = ocField(ocJsonIndex(candidates, 0), "content");
            const Value parts = ocField(content, "parts");
            text = stringOf(ocField(ocJsonIndex(parts, 0), "text"));
        }
        else if (provider == "anthropic") text = stringOf(ocField(ocJsonIndex(ocField(document, "content"), 0), "text"));
        else if (provider == "ollama") text = ocJsonString(ocField(document, "message"), "content");
        else if (provider == "huggingface") {
            const Value first = ocJsonIndex(document, 0);
            text = std::holds_alternative<std::string>(first) ? stringOf(first) : ocJsonString(first, "generated_text");
        } else text = ocJsonString(ocField(ocJsonIndex(ocField(document, "choices"), 0), "message"), "content");
    }
    ocSetField(out, "text", text);
    ocSetField(out, "raw", parsed ? document : Value{});
    ocSetField(out, "body", response.body);
    return out;
}
Value ocAiModels(const std::string& provider, void** args, int argc) {
    const std::string key = argumentString(args, 0);
    OcAiDefaults defaults = ocAiDefaults(provider);
    if (argc >= 2 && !argumentString(args, 1).empty()) defaults.base = argumentString(args, 1);
    std::vector<std::pair<std::string,std::string>> headers;
    if (!defaults.authHeader.empty() && !key.empty()) headers.push_back({defaults.authHeader, defaults.authPrefix + key});
    if (!defaults.version.empty()) headers.push_back({"anthropic-version", defaults.version});
    std::string url;
    if (provider == "google") url = defaults.base + "/v1beta/models?key=" + ocUrlEncode(key);
    else if (provider == "ollama") url = defaults.base + "/api/tags";
    else if (provider == "huggingface") url = "https://huggingface.co/api/models?limit=20";
    else if (provider == "anthropic") url = defaults.base + "/v1/models";
    else url = defaults.base + "/models";
    // `ocAiModels` returns a Value. Boxing is only valid at the C ABI boundary.
    return ocApiValue(ocHttpRequest("GET", url, std::string(), headers, 30000));
}
void* openAiCall(const std::string& op, void** args, int argc) {
    if ((op == "chat" || op == "sohbet") && argc >= 3) return box(ocAiChat("openai", args, argc));
    if ((op == "models" || op == "modeller") && argc >= 1) return box(ocAiModels("openai", args, argc));
    writeStderr("PRCYTERAS: OpenAi."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* groqCall(const std::string& op, void** args, int argc) {
    if ((op == "chat" || op == "sohbet") && argc >= 3) return box(ocAiChat("groq", args, argc));
    if ((op == "models" || op == "modeller") && argc >= 1) return box(ocAiModels("groq", args, argc));
    writeStderr("PRCYTERAS: Groq."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* ollamaCall(const std::string& op, void** args, int argc) {
    if ((op == "chat" || op == "sohbet") && argc >= 3) return box(ocAiChat("ollama", args, argc));
    if ((op == "models" || op == "modeller") && argc >= 1) return box(ocAiModels("ollama", args, argc));
    writeStderr("PRCYTERAS: Ollama."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* anthropicCall(const std::string& op, void** args, int argc) {
    if ((op == "chat" || op == "sohbet") && argc >= 3) return box(ocAiChat("anthropic", args, argc));
    if ((op == "models" || op == "modeller") && argc >= 1) return box(ocAiModels("anthropic", args, argc));
    writeStderr("PRCYTERAS: Anthropic."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* googleGenaiCall(const std::string& op, void** args, int argc) {
    if ((op == "chat" || op == "sohbet" || op == "generate") && argc >= 3) return box(ocAiChat("google", args, argc));
    if ((op == "models" || op == "modeller") && argc >= 1) return box(ocAiModels("google", args, argc));
    writeStderr("PRCYTERAS: GoogleGenai."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* huggingFaceCall(const std::string& op, void** args, int argc) {
    if ((op == "chat" || op == "sohbet" || op == "infer") && argc >= 3) return box(ocAiChat("huggingface", args, argc));
    if ((op == "models" || op == "modeller") && argc >= 1) return box(ocAiModels("huggingface", args, argc));
    writeStderr("PRCYTERAS: HuggingFace."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-15 - elevenlabs / stability (gercek ses ve goruntu API)
void* elevenLabsCall(const std::string& op, void** args, int argc) {
    if ((op == "voices" || op == "sesler") && argc >= 1) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.elevenlabs.io/v1/voices", std::string(), {{"xi-api-key", argumentString(args, 0)}}, 30000)));
    }
    if ((op == "models" || op == "modeller") && argc >= 1) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.elevenlabs.io/v1/models", std::string(), {{"xi-api-key", argumentString(args, 0)}}, 30000)));
    }
    if ((op == "speak" || op == "konus") && argc >= 4) {
        const std::string key = argumentString(args, 0), voice = argumentString(args, 1), text = argumentString(args, 2), path = argumentString(args, 3);
        const std::string model = argc >= 5 ? argumentString(args, 4) : std::string("eleven_multilingual_v2");
        const std::string body = "{\"text\":" + ocJsonQuote(text) + ",\"model_id\":" + ocJsonQuote(model) + "}";
        const OcHttpResponse response = ocHttpRequest("POST", "https://api.elevenlabs.io/v1/text-to-speech/" + ocUrlEncode(voice) + "?output_format=mp3_44100_128",
            body, {{"xi-api-key", key}, {"Content-Type", "application/json"}, {"Accept", "audio/mpeg"}}, 120000);
        Value out = ocApiValue(response);
        ocSetField(out, "saved", response.ok && ocWriteFile(path, response.body));
        ocSetField(out, "bytes", static_cast<std::int64_t>(response.body.size()));
        return box(out);
    }
    if ((op == "user" || op == "kullanici") && argc >= 1) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.elevenlabs.io/v1/user", std::string(), {{"xi-api-key", argumentString(args, 0)}}, 30000)));
    }
    writeStderr("PRCYTERAS: ElevenLabs."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* whisperCall(const std::string& op, void** args, int argc) {
    if ((op == "transcribe" || op == "decretele" || op == "transcribe_file") && argc >= 2) {
        const std::string key = argumentString(args, 0);
        const fs::path path(wide(argumentString(args, 1)));
        const std::string language = argc >= 3 ? argumentString(args, 2) : std::string();
        const std::string endpoint = argc >= 4 ? argumentString(args, 3) : "https://api.openai.com/v1/audio/transcriptions";
        const bool httpsEndpoint = endpoint.rfind("https://", 0) == 0;
        const bool loopbackHttp = endpoint.rfind("http://localhost:", 0) == 0 || endpoint.rfind("http://127.0.0.1:", 0) == 0 || endpoint.rfind("http://[::1]:", 0) == 0;
        if (key.empty() || (!httpsEndpoint && !loopbackHttp))
            return box(ocApiValue({false, 0, {}, {}, "API anahtari veya endpoint gecersiz"}));
        std::error_code ec;
        const auto size = fs::file_size(path, ec);
        if (ec || size == 0 || size > 25ULL * 1024 * 1024)
            return box(ocApiValue({false, 0, {}, {}, "ses dosyasi okunamadi veya 25 MiB sinirini asiyor"}));
        std::ifstream input(path, std::ios::binary);
        if (!input) return box(ocApiValue({false, 0, {}, {}, "ses dosyasi acilamadi"}));
        std::string audio(static_cast<std::size_t>(size), '\0');
        input.read(audio.data(), static_cast<std::streamsize>(audio.size()));
        if (!input || static_cast<std::size_t>(input.gcount()) != audio.size())
            return box(ocApiValue({false, 0, {}, {}, "ses dosyasi eksik okundu"}));
        std::string filename = path.filename().string();
        if (filename.empty()) filename = "audio.bin";
        for (char& c : filename) if (c == '"' || c == '\r' || c == '\n' || c == '\\') c = '_';
        std::string mime = "application/octet-stream";
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".mp3" || extension == ".mpga") mime = "audio/mpeg";
        else if (extension == ".wav") mime = "audio/wav";
        else if (extension == ".m4a" || extension == ".mp4") mime = "audio/mp4";
        else if (extension == ".ogg") mime = "audio/ogg";
        else if (extension == ".webm") mime = "audio/webm";
        else if (extension == ".flac") mime = "audio/flac";
        const std::string boundary = "----PRCYTERASWhisper" + ocRandomHex(12);
        std::string body;
        auto field = [&](const std::string& name, const std::string& value) {
            body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + name + "\"\r\n\r\n" + value + "\r\n";
        };
        field("model", "whisper-1");
        if (!language.empty()) field("language", language);
        body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\nContent-Type: " + mime + "\r\n\r\n";
        body += audio;
        body += "\r\n--" + boundary + "--\r\n";
        const OcHttpResponse response = ocHttpRequest("POST", endpoint, body,
            {{"Authorization", "Bearer " + key}, {"Content-Type", "multipart/form-data; boundary=" + boundary}, {"Accept", "application/json"}}, 120000);
        return box(ocApiValue(response));
    }
    writeStderr("PRCYTERAS: Whisper."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* stabilityAiCall(const std::string& op, void** args, int argc) {
    if ((op == "generate" || op == "uret") && argc >= 3) {
        const std::string key = argumentString(args, 0), prompt = argumentString(args, 1), path = argumentString(args, 2);
        const std::string aspect = argc >= 4 ? argumentString(args, 3) : std::string("1:1");
        const std::string boundary = "----PRCYTERAS" + ocRandomHex(8);
        std::string body;
        auto field = [&](const std::string& name, const std::string& value) {
            body += "--" + boundary + "\r\nContent-Disposition: form-data; name=\"" + name + "\"\r\n\r\n" + value + "\r\n";
        };
        field("prompt", prompt); field("output_format", "png"); field("aspect_ratio", aspect);
        body += "--" + boundary + "--\r\n";
        const OcHttpResponse response = ocHttpRequest("POST", "https://api.stability.ai/v2beta/stable-image/generate/core", body,
            {{"Authorization", "Bearer " + key}, {"Content-Type", "multipart/form-data; boundary=" + boundary}, {"Accept", "image/*"}}, 120000);
        Value out = ocApiValue(response);
        ocSetField(out, "saved", response.ok && ocWriteFile(path, response.body));
        ocSetField(out, "bytes", static_cast<std::int64_t>(response.body.size()));
        return box(out);
    }
    if ((op == "balance" || op == "bakiye") && argc >= 1) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.stability.ai/v1/user/balance", std::string(), {{"Authorization", "Bearer " + argumentString(args, 0)}}, 30000)));
    }
    writeStderr("PRCYTERAS: StabilityAi."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-15 - mesajlasma API'leri
void* discordCall(const std::string& op, void** args, int argc) {
    if ((op == "gateway") && argc >= 0) return box(ocApiValue(ocHttpRequest("GET", "https://discord.com/api/v10/gateway", std::string(), {}, 30000)));
    if ((op == "send_webhook" || op == "webhook") && argc >= 2) {
        const std::string body = "{\"content\":" + ocJsonQuote(argumentString(args, 1)) +
            (argc >= 3 && !argumentString(args, 2).empty() ? ",\"username\":" + ocJsonQuote(argumentString(args, 2)) : "") + "}";
        return box(ocApiValue(ocHttpRequest("POST", argumentString(args, 0), body, {{"Content-Type", "application/json"}}, 30000)));
    }
    if ((op == "send" || op == "gonder") && argc >= 3) {
        const std::string body = "{\"content\":" + ocJsonQuote(argumentString(args, 2)) + "}";
        return box(ocApiValue(ocHttpRequest("POST", "https://discord.com/api/v10/channels/" + ocUrlEncode(argumentString(args, 1)) + "/messages", body,
            {{"Authorization", "Bot " + argumentString(args, 0)}, {"Content-Type", "application/json"}}, 30000)));
    }
    if ((op == "me" || op == "kullanici") && argc >= 1) {
        return box(ocApiValue(ocHttpRequest("GET", "https://discord.com/api/v10/users/@me", std::string(), {{"Authorization", "Bot " + argumentString(args, 0)}}, 30000)));
    }
    if ((op == "guilds" || op == "sunucular") && argc >= 1) {
        return box(ocApiValue(ocHttpRequest("GET", "https://discord.com/api/v10/users/@me/guilds", std::string(), {{"Authorization", "Bot " + argumentString(args, 0)}}, 30000)));
    }
    if ((op == "messages" || op == "mesajlar") && argc >= 3) {
        return box(ocApiValue(ocHttpRequest("GET", "https://discord.com/api/v10/channels/" + ocUrlEncode(argumentString(args, 1)) + "/messages?limit=" + std::to_string(std::clamp(argumentInt(args, 2), 1, 100)),
            std::string(), {{"Authorization", "Bot " + argumentString(args, 0)}}, 30000)));
    }
    writeStderr("PRCYTERAS: Discord."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-15 - discord-rpc (gercek named pipe IPC)
struct OcRpcPipe { HANDLE handle = INVALID_HANDLE_VALUE; std::mutex mutex; };
std::mutex& ocRpcMutex() { static std::mutex mutex; return mutex; }
std::map<int, std::shared_ptr<OcRpcPipe>>& ocRpcPipes() { static std::map<int, std::shared_ptr<OcRpcPipe>> pipes; return pipes; }
std::shared_ptr<OcRpcPipe> ocRpcFind(int id) {
    std::lock_guard<std::mutex> lock(ocRpcMutex());
    const auto found = ocRpcPipes().find(id);
    return found == ocRpcPipes().end() ? nullptr : found->second;
}
bool ocRpcWrite(HANDLE handle, int opcode, const std::string& json) {
    std::string frame(8, '\0');
    const std::uint32_t op = static_cast<std::uint32_t>(opcode), length = static_cast<std::uint32_t>(json.size());
    std::memcpy(frame.data(), &op, 4); std::memcpy(frame.data() + 4, &length, 4);
    frame += json;
    DWORD written = 0;
    return WriteFile(handle, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr) && written == frame.size();
}
std::string ocRpcRead(HANDLE handle, int timeoutMs) {
    const ULONGLONG deadline = GetTickCount64() + static_cast<ULONGLONG>(std::max(0, timeoutMs));
    auto readExact = [&](void* buffer, DWORD size) {
        DWORD total = 0;
        while (total < size) {
            DWORD available = 0;
            if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr)) return false;
            if (available == 0) {
                if (GetTickCount64() >= deadline) return false;
                Sleep(10);
                continue;
            }
            DWORD count = 0;
            if (!ReadFile(handle, static_cast<char*>(buffer) + total, size - total, &count, nullptr) || count == 0) return false;
            total += count;
        }
        return true;
    };
    std::uint32_t op = 0, length = 0;
    if (!readExact(&op, 4) || !readExact(&length, 4)) return {};
    if (length > 4 * 1024 * 1024) return {};
    std::string json(length, '\0');
    if (length > 0 && !readExact(json.data(), length)) return {};
    return json;
}
void* discordRpcCall(const std::string& op, void** args, int argc) {
    if ((op == "connect" || op == "baglan") && argc >= 1) {
        for (int index = 0; index < 10; ++index) {
            const std::wstring name = L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring(index);
            HANDLE handle = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (handle == INVALID_HANDLE_VALUE) continue;
            if (!ocRpcWrite(handle, 0, "{\"v\":1,\"client_id\":" + ocJsonQuote(argumentString(args, 0)) + "}")) { CloseHandle(handle); continue; }
            std::lock_guard<std::mutex> lock(ocRpcMutex());
            static int next = 1; const int id = next++;
            auto pipe = std::make_shared<OcRpcPipe>(); pipe->handle = handle;
            ocRpcPipes()[id] = pipe;
            return box(static_cast<std::int64_t>(id));
        }
        return box(std::int64_t{-1});
    }
    auto pipe = argc >= 1 ? ocRpcFind(argumentInt(args, 0)) : nullptr;
    if ((op == "read" || op == "oku") && argc >= 1) {
        if (!pipe) return box(std::string());
        std::lock_guard<std::mutex> lock(pipe->mutex);
        return box(ocRpcRead(pipe->handle, argc >= 2 ? argumentInt(args, 1) : 5000));
    }
    if ((op == "set_activity" || op == "durum") && argc >= 2) {
        if (!pipe) return box(false);
        std::lock_guard<std::mutex> lock(pipe->mutex);
        std::string activity = "{\"details\":" + ocJsonQuote(argumentString(args, 1));
        if (argc >= 3 && !argumentString(args, 2).empty()) activity += ",\"state\":" + ocJsonQuote(argumentString(args, 2));
        if (argc >= 5 && !argumentString(args, 3).empty()) activity += ",\"assets\":{\"large_image\":" + ocJsonQuote(argumentString(args, 3)) + ",\"large_text\":" + ocJsonQuote(argumentString(args, 4)) + "}";
        activity += "}";
        const std::string frame = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string(GetCurrentProcessId()) + ",\"activity\":" + activity + "},\"nonce\":" + ocJsonQuote(ocRandomHex(8)) + "}";
        return box(ocRpcWrite(pipe->handle, 1, frame));
    }
    if ((op == "clear_activity" || op == "temizle") && argc >= 1) {
        if (!pipe) return box(false);
        std::lock_guard<std::mutex> lock(pipe->mutex);
        const std::string frame = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string(GetCurrentProcessId()) + ",\"activity\":null},\"nonce\":" + ocJsonQuote(ocRandomHex(8)) + "}";
        return box(ocRpcWrite(pipe->handle, 1, frame));
    }
    if ((op == "close" || op == "kapat") && argc >= 1) {
        std::lock_guard<std::mutex> lock(ocRpcMutex());
        const auto found = ocRpcPipes().find(argumentInt(args, 0));
        if (found != ocRpcPipes().end()) { if (found->second->handle != INVALID_HANDLE_VALUE) CloseHandle(found->second->handle); ocRpcPipes().erase(found); }
        return box(true);
    }
    writeStderr("PRCYTERAS: DiscordRpc."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* telegramCall(const std::string& op, void** args, int argc) {
    auto call = [&](const std::string& method, const std::string& body) {
        return ocApiValue(ocHttpRequest("POST", "https://api.telegram.org/bot" + argumentString(args, 0) + "/" + method, body, {{"Content-Type", "application/json"}}, 30000));
    };
    if ((op == "get_me" || op == "ben") && argc >= 1) return box(call("getMe", "{}"));
    if ((op == "send" || op == "gonder") && argc >= 3) {
        const std::string body = "{\"chat_id\":" + ocJsonQuote(argumentString(args, 1)) + ",\"text\":" + ocJsonQuote(argumentString(args, 2)) + "}";
        return box(call("sendMessage", body));
    }
    if ((op == "send_photo") && argc >= 3) {
        const std::string body = "{\"chat_id\":" + ocJsonQuote(argumentString(args, 1)) + ",\"photo\":" + ocJsonQuote(argumentString(args, 2)) +
            (argc >= 4 ? ",\"caption\":" + ocJsonQuote(argumentString(args, 3)) : "") + "}";
        return box(call("sendPhoto", body));
    }
    if ((op == "updates" || op == "guncellemeler") && argc >= 1) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.telegram.org/bot" + argumentString(args, 0) + "/getUpdates?offset=" + std::to_string(argc >= 2 ? argumentInt(args, 1) : 0),
            std::string(), {}, 30000)));
    }
    if ((op == "call" || op == "cagir") && argc >= 3) return box(call(argumentString(args, 1), argumentString(args, 2)));
    writeStderr("PRCYTERAS: Telegram."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* whatsAppCall(const std::string& op, void** args, int argc) {
    auto headers = [&](const std::string& token) { return std::vector<std::pair<std::string,std::string>>{{"Authorization", "Bearer " + token}, {"Content-Type", "application/json"}}; };
    if ((op == "send" || op == "gonder") && argc >= 4) {
        const std::string body = "{\"messaging_product\":\"whatsapp\",\"to\":" + ocJsonQuote(argumentString(args, 2)) +
            ",\"type\":\"text\",\"text\":{\"body\":" + ocJsonQuote(argumentString(args, 3)) + "}}";
        return box(ocApiValue(ocHttpRequest("POST", "https://graph.facebook.com/v21.0/" + argumentString(args, 1) + "/messages", body, headers(argumentString(args, 0)), 30000)));
    }
    if ((op == "phone" || op == "telefon") && argc >= 2) {
        return box(ocApiValue(ocHttpRequest("GET", "https://graph.facebook.com/v21.0/" + argumentString(args, 1) + "?fields=display_phone_number,verified_name", std::string(), headers(argumentString(args, 0)), 30000)));
    }
    if ((op == "templates" || op == "sablonlar") && argc >= 2) {
        return box(ocApiValue(ocHttpRequest("GET", "https://graph.facebook.com/v21.0/" + argumentString(args, 1) + "/message_templates", std::string(), headers(argumentString(args, 0)), 30000)));
    }
    writeStderr("PRCYTERAS: WhatsApp."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* twitchCall(const std::string& op, void** args, int argc) {
    auto helixHeaders = [&](const std::string& client, const std::string& token) {
        return std::vector<std::pair<std::string,std::string>>{{"Client-Id", client}, {"Authorization", "Bearer " + token}};
    };
    if ((op == "token" || op == "jeton") && argc >= 2) {
        const std::string url = "https://id.twitch.tv/oauth2/token?client_id=" + ocUrlEncode(argumentString(args, 0)) +
            "&client_secret=" + ocUrlEncode(argumentString(args, 1)) + "&grant_type=client_credentials";
        return box(ocApiValue(ocHttpRequest("POST", url, std::string(), {}, 30000)));
    }
    if ((op == "streams" || op == "yayinlar") && argc >= 3) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.twitch.tv/helix/streams?user_login=" + ocUrlEncode(argumentString(args, 2)), std::string(), helixHeaders(argumentString(args, 0), argumentString(args, 1)), 30000)));
    }
    if ((op == "users" || op == "kullanicilar") && argc >= 3) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.twitch.tv/helix/users?login=" + ocUrlEncode(argumentString(args, 2)), std::string(), helixHeaders(argumentString(args, 0), argumentString(args, 1)), 30000)));
    }
    if ((op == "send_chat" || op == "sohbet_gonder") && argc >= 5) {
        const std::string body = "{\"broadcaster_id\":" + ocJsonQuote(argumentString(args, 2)) + ",\"sender_id\":" + ocJsonQuote(argumentString(args, 3)) +
            ",\"message\":" + ocJsonQuote(argumentString(args, 4)) + "}";
        auto headers = helixHeaders(argumentString(args, 0), argumentString(args, 1));
        headers.push_back({"Content-Type", "application/json"});
        return box(ocApiValue(ocHttpRequest("POST", "https://api.twitch.tv/helix/chat/messages", body, headers, 30000)));
    }
    if ((op == "top_games" || op == "oyunlar") && argc >= 3) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.twitch.tv/helix/games/top?first=" + std::to_string(std::clamp(argumentInt(args, 2), 1, 100)), std::string(), helixHeaders(argumentString(args, 0), argumentString(args, 1)), 30000)));
    }
    writeStderr("PRCYTERAS: Twitch."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* youTubeLiveCall(const std::string& op, void** args, int argc) {
    if ((op == "search" || op == "ara") && argc >= 2) {
        const std::string url = "https://www.googleapis.com/youtube/v3/search?part=snippet&type=video&eventType=live&maxResults=10&q=" +
            ocUrlEncode(argumentString(args, 1)) + "&key=" + ocUrlEncode(argumentString(args, 0));
        return box(ocApiValue(ocHttpRequest("GET", url, std::string(), {}, 30000)));
    }
    if ((op == "videos" || op == "videolar") && argc >= 2) {
        return box(ocApiValue(ocHttpRequest("GET", "https://www.googleapis.com/youtube/v3/videos?part=snippet,liveStreamingDetails&id=" + ocUrlEncode(argumentString(args, 1)) + "&key=" + ocUrlEncode(argumentString(args, 0)), std::string(), {}, 30000)));
    }
    if ((op == "live_chat" || op == "sohbet") && argc >= 2) {
        return box(ocApiValue(ocHttpRequest("GET", "https://www.googleapis.com/youtube/v3/liveChat/messages?part=snippet&liveChatId=" + ocUrlEncode(argumentString(args, 1)) + "&key=" + ocUrlEncode(argumentString(args, 0)), std::string(), {}, 30000)));
    }
    if ((op == "send_chat" || op == "mesaj_gonder") && argc >= 3) {
        const std::string body = "{\"snippet\":{\"liveChatId\":" + ocJsonQuote(argumentString(args, 1)) + ",\"type\":\"textMessageEvent\",\"textMessageDetails\":{\"messageText\":" + ocJsonQuote(argumentString(args, 2)) + "}}}";
        return box(ocApiValue(ocHttpRequest("POST", "https://www.googleapis.com/youtube/v3/liveChat/messages?part=snippet", body,
            {{"Authorization", "Bearer " + argumentString(args, 0)}, {"Content-Type", "application/json"}}, 30000)));
    }
    writeStderr("PRCYTERAS: YouTubeLive."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* slackCall(const std::string& op, void** args, int argc) {
    auto bearer = [&](const std::string& token) { return std::vector<std::pair<std::string,std::string>>{{"Authorization", "Bearer " + token}, {"Content-Type", "application/json"}}; };
    if ((op == "post" || op == "gonder") && argc >= 3) {
        const std::string body = "{\"channel\":" + ocJsonQuote(argumentString(args, 1)) + ",\"text\":" + ocJsonQuote(argumentString(args, 2)) + "}";
        return box(ocApiValue(ocHttpRequest("POST", "https://slack.com/api/chat.postMessage", body, bearer(argumentString(args, 0)), 30000)));
    }
    if ((op == "webhook") && argc >= 2) {
        const std::string body = "{\"text\":" + ocJsonQuote(argumentString(args, 1)) + "}";
        return box(ocApiValue(ocHttpRequest("POST", argumentString(args, 0), body, {{"Content-Type", "application/json"}}, 30000)));
    }
    if ((op == "history" || op == "gecmis") && argc >= 3) {
        return box(ocApiValue(ocHttpRequest("GET", "https://slack.com/api/conversations.history?channel=" + ocUrlEncode(argumentString(args, 1)) + "&limit=" + std::to_string(std::clamp(argumentInt(args, 2), 1, 1000)),
            std::string(), bearer(argumentString(args, 0)), 30000)));
    }
    if ((op == "users" || op == "kullanicilar") && argc >= 1) {
        return box(ocApiValue(ocHttpRequest("GET", "https://slack.com/api/users.list", std::string(), bearer(argumentString(args, 0)), 30000)));
    }
    writeStderr("PRCYTERAS: Slack."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-15 - firebase firestore REST
Value ocFirestoreValue(const Value& value) {
    Value out = stdlibObject("FirestoreValue");
    if (std::holds_alternative<std::string>(value)) ocSetField(out, "stringValue", stringOf(value));
    else if (std::holds_alternative<bool>(value)) ocSetField(out, "booleanValue", std::get<bool>(value));
    else if (std::holds_alternative<std::int64_t>(value)) ocSetField(out, "integerValue", std::to_string(std::get<std::int64_t>(value)));
    else if (std::holds_alternative<std::uint64_t>(value)) ocSetField(out, "integerValue", std::to_string(std::get<std::uint64_t>(value)));
    else if (std::holds_alternative<double>(value)) ocSetField(out, "doubleValue", std::get<double>(value));
    else if (auto object = ocObjectOf(value)) {
        Value fields = stdlibObject("FirestoreFields");
        for (const auto& entry : object->dynamicFields) ocSetField(fields, entry.first, ocFirestoreValue(entry.second));
        ocSetField(out, "mapValue", [&]() { Value map = stdlibObject("FirestoreMap"); ocSetField(map, "fields", fields); return map; }());
    } else if (std::holds_alternative<std::shared_ptr<ListValue>>(value)) {
        Value values = stdlibArray();
        for (const auto& item : ocValues(value)) stdlibArrayAppend(values, ocFirestoreValue(item));
        Value array = stdlibObject("FirestoreArray"); ocSetField(array, "values", values);
        ocSetField(out, "arrayValue", array);
    } else ocSetField(out, "nullValue", Value{});
    return out;
}
Value ocFirestorePlain(const Value& value) {
    if (const Value stringValue = ocField(value, "stringValue"); std::holds_alternative<std::string>(stringValue)) return stringValue;
    if (const Value boolValue = ocField(value, "booleanValue"); std::holds_alternative<bool>(boolValue)) return boolValue;
    if (const Value intValue = ocField(value, "integerValue"); std::holds_alternative<std::string>(intValue)) return static_cast<std::int64_t>(std::strtoll(stringOf(intValue).c_str(), nullptr, 10));
    if (const Value doubleValue = ocField(value, "doubleValue"); std::holds_alternative<double>(doubleValue)) return doubleValue;
    if (const Value mapValue = ocField(value, "mapValue"); auto map = ocObjectOf(mapValue)) {
        Value out = stdlibObject("FirestoreDoc");
        if (auto fields = ocObjectOf(ocField(mapValue, "fields"))) for (const auto& entry : fields->dynamicFields) ocSetField(out, entry.first, ocFirestorePlain(entry.second));
        return out;
    }
    if (const Value arrayValue = ocField(value, "arrayValue"); auto array = ocObjectOf(arrayValue)) {
        Value out = stdlibArray();
        for (const auto& item : ocValues(ocField(arrayValue, "values"))) stdlibArrayAppend(out, ocFirestorePlain(item));
        return out;
    }
    return Value{};
}
void* firebaseCall(const std::string& op, void** args, int argc) {
    auto base = [&](const std::string& project, const std::string& key, const std::string& path) {
        return "https://firestore.googleapis.com/v1/projects/" + ocUrlEncode(project) + "/databases/(default)/documents/" + path + "?key=" + ocUrlEncode(key);
    };
    if ((op == "set" || op == "yaz") && argc >= 4) {
        Value fields = stdlibObject("FirestoreFields");
        if (auto object = ocObjectOf(argument(args, 3))) for (const auto& entry : object->dynamicFields) ocSetField(fields, entry.first, ocFirestoreValue(entry.second));
        Value body = stdlibObject("FirestoreWrite"); ocSetField(body, "fields", fields);
        return box(ocApiValue(ocHttpRequest("PATCH", base(argumentString(args, 0), argumentString(args, 1), argumentString(args, 2)), ocJsonText(body), {{"Content-Type", "application/json"}}, 30000)));
    }
    if ((op == "get" || op == "al") && argc >= 3) {
        const Value response = ocApiValue(ocHttpRequest("GET", base(argumentString(args, 0), argumentString(args, 1), argumentString(args, 2)), std::string(), {}, 30000));
        if (auto object = ocObjectOf(response)) {
            const Value fields = ocField(ocField(response, "json"), "fields");
            Value plain = stdlibObject("FirestoreDoc");
            if (auto map = ocObjectOf(fields)) for (const auto& entry : map->dynamicFields) ocSetField(plain, entry.first, ocFirestorePlain(entry.second));
            ocSetField(response, "data", plain);
        }
        return box(response);
    }
    if ((op == "list" || op == "liste") && argc >= 3) {
        return box(ocApiValue(ocHttpRequest("GET", base(argumentString(args, 0), argumentString(args, 1), argumentString(args, 2)), std::string(), {}, 30000)));
    }
    if ((op == "delete" || op == "sil") && argc >= 3) {
        return box(ocApiValue(ocHttpRequest("DELETE", base(argumentString(args, 0), argumentString(args, 1), argumentString(args, 2)), std::string(), {}, 30000)));
    }
    writeStderr("PRCYTERAS: Firebase."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-15 - supabase postgrest
void* supabaseCall(const std::string& op, void** args, int argc) {
    auto headers = [&](const std::string& key) {
        return std::vector<std::pair<std::string,std::string>>{{"apikey", key}, {"Authorization", "Bearer " + key}, {"Content-Type", "application/json"}, {"Prefer", "return=representation"}};
    };
    auto endpoint = [&](const std::string& url, const std::string& table) { return url + "/rest/v1/" + table; };
    if ((op == "select" || op == "sec") && argc >= 3) {
        const std::string query = argc >= 4 && !argumentString(args, 3).empty() ? "?" + argumentString(args, 3) : std::string();
        return box(ocApiValue(ocHttpRequest("GET", endpoint(argumentString(args, 0), argumentString(args, 2)) + query, std::string(), headers(argumentString(args, 1)), 30000)));
    }
    if ((op == "insert" || op == "ekle") && argc >= 4) {
        return box(ocApiValue(ocHttpRequest("POST", endpoint(argumentString(args, 0), argumentString(args, 2)), ocJsonText(argument(args, 3)), headers(argumentString(args, 1)), 30000)));
    }
    if ((op == "update" || op == "guncelle") && argc >= 5) {
        return box(ocApiValue(ocHttpRequest("PATCH", endpoint(argumentString(args, 0), argumentString(args, 2)) + "?" + argumentString(args, 3), ocJsonText(argument(args, 4)), headers(argumentString(args, 1)), 30000)));
    }
    if ((op == "delete" || op == "sil") && argc >= 4) {
        return box(ocApiValue(ocHttpRequest("DELETE", endpoint(argumentString(args, 0), argumentString(args, 2)) + "?" + argumentString(args, 3), std::string(), headers(argumentString(args, 1)), 30000)));
    }
    if ((op == "rpc") && argc >= 4) {
        return box(ocApiValue(ocHttpRequest("POST", argumentString(args, 0) + "/rest/v1/rpc/" + argumentString(args, 2), ocJsonText(argument(args, 3)), headers(argumentString(args, 1)), 30000)));
    }
    writeStderr("PRCYTERAS: Supabase."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptım opencode - 2026-11-15 - steam web api
void* steamCall(const std::string& op, void** args, int argc) {
    if ((op == "player_summary" || op == "oyuncu") && argc >= 2) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.steampowered.com/ISteamUser/GetPlayerSummaries/v2/?key=" + ocUrlEncode(argumentString(args, 0)) + "&steamids=" + ocUrlEncode(argumentString(args, 1)), std::string(), {}, 30000)));
    }
    if ((op == "owned_games" || op == "oyunlar") && argc >= 2) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.steampowered.com/IPlayerService/GetOwnedGames/v1/?key=" + ocUrlEncode(argumentString(args, 0)) + "&steamid=" + ocUrlEncode(argumentString(args, 1)) + "&include_appinfo=1", std::string(), {}, 30000)));
    }
    if ((op == "news" || op == "haberler") && argc >= 2) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=" + ocUrlEncode(argumentString(args, 1)) + "&count=" + std::to_string(argc >= 3 ? argumentInt(args, 2) : 10) + "&key=" + ocUrlEncode(argumentString(args, 0)), std::string(), {}, 30000)));
    }
    if ((op == "app_details" || op == "uygulama") && argc >= 1) {
        return box(ocApiValue(ocHttpRequest("GET", "https://store.steampowered.com/api/appdetails?appids=" + ocUrlEncode(argumentString(args, 0)) + "&l=turkish", std::string(), {}, 30000)));
    }
    if ((op == "resolve_vanity" || op == "cozumle") && argc >= 2) {
        return box(ocApiValue(ocHttpRequest("GET", "https://api.steampowered.com/ISteamUser/ResolveVanityURL/v1/?key=" + ocUrlEncode(argumentString(args, 0)) + "&vanityurl=" + ocUrlEncode(argumentString(args, 1)), std::string(), {}, 30000)));
    }
    writeStderr("PRCYTERAS: Steam."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}

// ben yaptım codex - 2026-11-15 - Windows clipboard and dialogs
void* clipboardCall(const std::string& op, void** args, int argc) {
    if ((op == "write" || op == "set" || op == "yaz") && argc >= 1) {
        if (!OpenClipboard(nullptr)) return box(false);
        const std::wstring text = wide(argumentString(args,0));
        EmptyClipboard();
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE,(text.size()+1)*sizeof(wchar_t));
        if (!memory) { CloseClipboard(); return box(false); }
        void* data = GlobalLock(memory);
        if (!data) { GlobalFree(memory); CloseClipboard(); return box(false); }
        std::memcpy(data,text.c_str(),(text.size()+1)*sizeof(wchar_t)); GlobalUnlock(memory);
        if (!SetClipboardData(CF_UNICODETEXT,memory)) { GlobalFree(memory); CloseClipboard(); return box(false); }
        CloseClipboard(); return box(true);
    }
    if ((op == "read" || op == "get" || op == "oku") && argc == 0) {
        if (!OpenClipboard(nullptr)) return box(std::string());
        HANDLE memory=GetClipboardData(CF_UNICODETEXT);
        if (!memory) { CloseClipboard(); return box(std::string()); }
        const wchar_t* data=static_cast<const wchar_t*>(GlobalLock(memory));
        if (!data) { CloseClipboard(); return box(std::string()); }
        const std::wstring text(data); GlobalUnlock(memory); CloseClipboard(); return box(utf8(text));
    }
    if ((op == "clear" || op == "temizle") && argc == 0) {
        if (!OpenClipboard(nullptr)) return box(false);
        const BOOL ok=EmptyClipboard(); CloseClipboard(); return box(ok!=FALSE);
    }
    writeStderr("PRCYTERAS: Clipboard."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* winDialogCall(const std::string& op, void** args, int argc) {
    if ((op == "message" || op == "mesaj") && argc >= 1) {
        const std::wstring message=wide(argumentString(args,0));
        const std::wstring title=argc>=2?wide(argumentString(args,1)):L"PRCYTERAS";
        const UINT flags=argc>=3?static_cast<UINT>(argumentInt(args,2)):MB_OK|MB_ICONINFORMATION;
        return box(static_cast<std::int64_t>(MessageBoxW(nullptr,message.c_str(),title.c_str(),flags)));
    }
    if ((op == "open_file" || op == "ac") && argc >= 0) {
        std::vector<wchar_t> path(32768,L'\0'); OPENFILENAMEW d{}; d.lStructSize=sizeof(d); d.lpstrFile=path.data(); d.nMaxFile=static_cast<DWORD>(path.size()); d.lpstrFilter=L"Tum dosyalar\0*.*\0Metin\0*.txt\0"; d.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        std::wstring title;if(argc>=1){title=wide(argumentString(args,0));d.lpstrTitle=title.c_str();}
        return box(GetOpenFileNameW(&d)?utf8(path.data()):std::string());
    }
    if ((op == "save_file" || op == "farkli_kaydet") && argc >= 0) {
        std::vector<wchar_t> path(32768,L'\0'); OPENFILENAMEW d{}; d.lStructSize=sizeof(d); d.lpstrFile=path.data(); d.nMaxFile=static_cast<DWORD>(path.size()); d.lpstrFilter=L"Tum dosyalar\0*.*\0Metin\0*.txt\0"; d.Flags=OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR;
        std::wstring title;if(argc>=1){title=wide(argumentString(args,0));d.lpstrTitle=title.c_str();}
        return box(GetSaveFileNameW(&d)?utf8(path.data()):std::string());
    }
    writeStderr("PRCYTERAS: WinDialog."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptim codex - 2026-11-15 - explicit Win32 desktop screenshot to BMP
bool ocSaveScreenshotBmp(const std::wstring& path, int x, int y, int width, int height) {
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384 || static_cast<std::uint64_t>(width) * height > 64000000ULL) return false;
    HDC screen = GetDC(nullptr);
    if (!screen) return false;
    HDC memory = CreateCompatibleDC(screen);
    HBITMAP bitmap = memory ? CreateCompatibleBitmap(screen, width, height) : nullptr;
    if (!memory || !bitmap) { if (bitmap) DeleteObject(bitmap); if (memory) DeleteDC(memory); ReleaseDC(nullptr, screen); return false; }
    HGDIOBJ previous = SelectObject(memory, bitmap);
    const BOOL copied = previous && previous != HGDI_ERROR && BitBlt(memory, 0, 0, width, height, screen, x, y, SRCCOPY | CAPTUREBLT);
    if (previous && previous != HGDI_ERROR) SelectObject(memory, previous);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 24; info.bmiHeader.biCompression = BI_RGB;
    const std::uint64_t stride = (static_cast<std::uint64_t>(width) * 3 + 3) & ~3ULL;
    const std::uint64_t pixelBytes = stride * static_cast<std::uint64_t>(height);
    if (pixelBytes > std::numeric_limits<DWORD>::max()) { DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(nullptr, screen); return false; }
    info.bmiHeader.biSizeImage = static_cast<DWORD>(pixelBytes);
    std::vector<BYTE> pixels(static_cast<std::size_t>(pixelBytes));
    const int lines = copied ? GetDIBits(memory, bitmap, 0, static_cast<UINT>(height), pixels.data(), &info, DIB_RGB_COLORS) : 0;
    DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(nullptr, screen);
    if (lines != height) return false;
    BITMAPFILEHEADER header{}; header.bfType = 0x4D42; header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    header.bfSize = header.bfOffBits + info.bmiHeader.biSizeImage;
    std::ofstream output(fs::path(path), std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    output.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
    output.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    output.close(); return static_cast<bool>(output);
}
void* screenshotCall(const std::string& op, void** args, int argc) {
    if ((op == "capture" || op == "yakala") && argc == 1) {
        const int x = GetSystemMetrics(SM_XVIRTUALSCREEN), y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        return box(ocSaveScreenshotBmp(wide(argumentString(args, 0)), x, y, width, height));
    }
    if ((op == "capture_region" || op == "bolge_yakala") && argc == 5) {
        const double xValue = numberOf(argument(args, 1)), yValue = numberOf(argument(args, 2));
        const double widthValue = numberOf(argument(args, 3)), heightValue = numberOf(argument(args, 4));
        if (!std::isfinite(xValue) || !std::isfinite(yValue) || !std::isfinite(widthValue) || !std::isfinite(heightValue) ||
            std::floor(xValue) != xValue || std::floor(yValue) != yValue || std::floor(widthValue) != widthValue || std::floor(heightValue) != heightValue ||
            xValue < std::numeric_limits<int>::min() || xValue > std::numeric_limits<int>::max() ||
            yValue < std::numeric_limits<int>::min() || yValue > std::numeric_limits<int>::max() ||
            widthValue < 1 || widthValue > 16384 || heightValue < 1 || heightValue > 16384) return box(false);
        const int x = static_cast<int>(xValue), y = static_cast<int>(yValue);
        const int width = static_cast<int>(widthValue), height = static_cast<int>(heightValue);
        const int virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN), virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN), virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (x < virtualX || y < virtualY || static_cast<std::int64_t>(x) + width > static_cast<std::int64_t>(virtualX) + virtualWidth ||
            static_cast<std::int64_t>(y) + height > static_cast<std::int64_t>(virtualY) + virtualHeight) return box(false);
        return box(ocSaveScreenshotBmp(wide(argumentString(args, 0)), x, y, width, height));
    }
    writeStderr("PRCYTERAS: Screenshot."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
struct PtsGifRecorderState {
    std::wstring path;
    int x = 0, y = 0, width = 0, height = 0, delayCentiseconds = 10;
    std::size_t storedBytes = 0;
    std::vector<std::vector<std::uint8_t>> frames;
    bool active = false;
};
PtsGifRecorderState& ptsGifState() { static PtsGifRecorderState state; return state; }
std::vector<std::uint8_t> ptsGifLzw(const std::vector<std::uint8_t>& pixels) {
    std::vector<std::uint8_t> output;
    std::unordered_map<std::uint32_t, std::uint16_t> dictionary;
    unsigned codeBits = 9, nextCode = 258, bitCount = 0;
    std::uint32_t bitBuffer = 0;
    auto put = [&](unsigned code) {
        bitBuffer |= code << bitCount;
        bitCount += codeBits;
        while (bitCount >= 8) { output.push_back(static_cast<std::uint8_t>(bitBuffer & 0xff)); bitBuffer >>= 8; bitCount -= 8; }
    };
    put(256);
    if (!pixels.empty()) {
        unsigned prefix = pixels[0];
        for (std::size_t i = 1; i < pixels.size(); ++i) {
            const std::uint8_t suffix = pixels[i];
            const std::uint32_t key = (prefix << 8) | suffix;
            const auto found = dictionary.find(key);
            if (found != dictionary.end()) { prefix = found->second; continue; }
            put(prefix);
            if (nextCode < 4096) {
                dictionary.emplace(key, static_cast<std::uint16_t>(nextCode++));
                if (nextCode == (1u << codeBits) && codeBits < 12) ++codeBits;
            } else {
                put(256);
                dictionary.clear(); nextCode = 258; codeBits = 9;
            }
            prefix = suffix;
        }
        put(prefix);
    }
    put(257);
    if (bitCount) output.push_back(static_cast<std::uint8_t>(bitBuffer & 0xff));
    return output;
}
bool ptsGifSave(const PtsGifRecorderState& state) {
    if (state.frames.empty() || state.width < 1 || state.height < 1 || state.width > 4096 || state.height > 4096) return false;
    std::vector<std::uint8_t> gif;
    gif.reserve(state.storedBytes + 1024);
    auto byte = [&](std::uint8_t value) { gif.push_back(value); };
    auto word = [&](unsigned value) { byte(static_cast<std::uint8_t>(value)); byte(static_cast<std::uint8_t>(value >> 8)); };
    const char header[] = "GIF89a"; gif.insert(gif.end(), header, header + 6);
    word(state.width); word(state.height); byte(0xF7); byte(0); byte(0);
    for (unsigned i = 0; i < 256; ++i) {
        byte(static_cast<std::uint8_t>(((i >> 5) & 7) * 255 / 7));
        byte(static_cast<std::uint8_t>(((i >> 2) & 7) * 255 / 7));
        byte(static_cast<std::uint8_t>((i & 3) * 255 / 3));
    }
    const std::uint8_t loop[] = {0x21,0xFF,0x0B,'N','E','T','S','C','A','P','E','2','.','0',0x03,0x01,0x00,0x00,0x00};
    gif.insert(gif.end(), std::begin(loop), std::end(loop));
    for (const auto& frame : state.frames) {
        if (frame.size() != static_cast<std::size_t>(state.width) * state.height) return false;
        byte(0x21); byte(0xF9); byte(0x04); byte(0x04); word(state.delayCentiseconds); byte(0); byte(0);
        byte(0x2C); word(0); word(0); word(state.width); word(state.height); byte(0);
        byte(8);
        const auto compressed = ptsGifLzw(frame);
        for (std::size_t offset = 0; offset < compressed.size();) {
            const std::size_t count = std::min<std::size_t>(255, compressed.size() - offset);
            byte(static_cast<std::uint8_t>(count));
            gif.insert(gif.end(), compressed.begin() + static_cast<std::ptrdiff_t>(offset), compressed.begin() + static_cast<std::ptrdiff_t>(offset + count));
            offset += count;
        }
        byte(0);
    }
    byte(0x3B);
    std::ofstream output(fs::path(state.path), std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(gif.data()), static_cast<std::streamsize>(gif.size()));
    output.close();
    return static_cast<bool>(output);
}
void* gifRecorderCall(const std::string& op, void** args, int argc) {
    auto& state = ptsGifState();
    if ((op == "start" || op == "baslat") && argc == 6) {
        const int x = argumentInt(args, 1), y = argumentInt(args, 2), width = argumentInt(args, 3), height = argumentInt(args, 4);
        const int fps = std::clamp(argumentInt(args, 5), 1, 30);
        if (state.active || argumentString(args, 0).empty() || width < 1 || height < 1 || width > 4096 || height > 4096 || static_cast<std::uint64_t>(width) * height > 16000000ULL) return box(false);
        state.path = wide(argumentString(args, 0)); state.x = x; state.y = y; state.width = width; state.height = height;
        state.delayCentiseconds = std::max(1, static_cast<int>(std::lround(100.0 / fps)));
        state.storedBytes = 0; state.frames.clear(); state.active = true;
        return box(true);
    }
    if ((op == "capture" || op == "kare_ekle") && argc == 0) {
        if (!state.active || state.frames.size() >= 600) return box(false);
        const std::uint64_t count = static_cast<std::uint64_t>(state.width) * state.height;
        if (count > 64ULL * 1024 * 1024 || state.storedBytes > 64ULL * 1024 * 1024 - count) return box(false);
        HDC screen = GetDC(nullptr);
        if (!screen) return box(false);
        HDC memory = CreateCompatibleDC(screen);
        HBITMAP bitmap = memory ? CreateCompatibleBitmap(screen, state.width, state.height) : nullptr;
        if (!memory || !bitmap) { if (bitmap) DeleteObject(bitmap); if (memory) DeleteDC(memory); ReleaseDC(nullptr, screen); return box(false); }
        HGDIOBJ previous = SelectObject(memory, bitmap);
        const BOOL copied = previous && previous != HGDI_ERROR && BitBlt(memory, 0, 0, state.width, state.height, screen, state.x, state.y, SRCCOPY | CAPTUREBLT);
        if (previous && previous != HGDI_ERROR) SelectObject(memory, previous);
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = state.width; info.bmiHeader.biHeight = -state.height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
        std::vector<std::uint8_t> bgra(static_cast<std::size_t>(count) * 4);
        const int lines = copied ? GetDIBits(memory, bitmap, 0, static_cast<UINT>(state.height), bgra.data(), &info, DIB_RGB_COLORS) : 0;
        DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(nullptr, screen);
        if (lines != state.height) return box(false);
        std::vector<std::uint8_t> indices(static_cast<std::size_t>(count));
        for (std::size_t pixel = 0; pixel < indices.size(); ++pixel) {
            const auto* p = bgra.data() + pixel * 4;
            indices[pixel] = static_cast<std::uint8_t>((p[2] & 0xE0) | ((p[1] & 0xE0) >> 3) | (p[0] >> 6));
        }
        state.storedBytes += indices.size(); state.frames.push_back(std::move(indices));
        return box(true);
    }
    if ((op == "save" || op == "kaydet") && argc == 0) {
        const bool wasActive = state.active; state.active = false;
        const bool saved = wasActive && ptsGifSave(state);
        state.frames.clear(); state.storedBytes = 0;
        return box(saved);
    }
    if ((op == "cancel" || op == "iptal") && argc == 0) {
        const bool wasActive = state.active; state.active = false; state.frames.clear(); state.storedBytes = 0;
        return box(wasActive);
    }
    if ((op == "frame_count" || op == "kare_sayisi") && argc == 0) return box(static_cast<std::int64_t>(state.frames.size()));
    writeStderr("PRCYTERAS: GifRecorder."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptim codex - 2026-11-15 - current-user Windows registry values
void* registryCall(const std::string& op, void** args, int argc) {
    if ((op == "exists_key" || op == "anahtar_var_mi") && argc == 1) {
        HKEY key = nullptr;
        const std::wstring path = wide(argumentString(args, 0));
        const LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_QUERY_VALUE, &key);
        if (status == ERROR_SUCCESS) RegCloseKey(key);
        return box(status == ERROR_SUCCESS);
    }
    if ((op == "get" || op == "oku") && argc == 2) {
        HKEY key = nullptr;
        const std::wstring path = wide(argumentString(args, 0));
        const std::wstring name = wide(argumentString(args, 1));
        if (RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return box(Value{});
        DWORD type = 0, bytes = 0;
        LSTATUS status = RegQueryValueExW(key, name.empty() ? nullptr : name.c_str(), nullptr, &type, nullptr, &bytes);
        if (status != ERROR_SUCCESS) { RegCloseKey(key); return box(Value{}); }
        if (type == REG_DWORD && bytes == sizeof(DWORD)) {
            DWORD value = 0; status = RegQueryValueExW(key, name.empty() ? nullptr : name.c_str(), nullptr, &type, reinterpret_cast<BYTE*>(&value), &bytes);
            RegCloseKey(key); return status == ERROR_SUCCESS ? box(static_cast<std::uint64_t>(value)) : box(Value{});
        }
        if (type == REG_QWORD && bytes == sizeof(ULONGLONG)) {
            ULONGLONG value = 0; status = RegQueryValueExW(key, name.empty() ? nullptr : name.c_str(), nullptr, &type, reinterpret_cast<BYTE*>(&value), &bytes);
            RegCloseKey(key); return status == ERROR_SUCCESS ? box(static_cast<std::uint64_t>(value)) : box(Value{});
        }
        if (type != REG_SZ && type != REG_EXPAND_SZ) { RegCloseKey(key); return box(Value{}); }
        std::vector<wchar_t> value(static_cast<std::size_t>(bytes / sizeof(wchar_t)) + 1, L'\0');
        status = RegQueryValueExW(key, name.empty() ? nullptr : name.c_str(), nullptr, &type, reinterpret_cast<BYTE*>(value.data()), &bytes);
        RegCloseKey(key);
        if (status != ERROR_SUCCESS) return box(Value{});
        return box(utf8(std::wstring(value.data())));
    }
    if ((op == "set_string" || op == "yazi_yaz") && argc == 3) {
        HKEY key = nullptr; DWORD disposition = 0;
        const std::wstring path = wide(argumentString(args, 0));
        const std::wstring name = wide(argumentString(args, 1));
        const std::wstring value = wide(argumentString(args, 2));
        if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) return box(false);
        const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
        const LSTATUS status = RegSetValueExW(key, name.empty() ? nullptr : name.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), bytes);
        RegCloseKey(key); return box(status == ERROR_SUCCESS);
    }
    if ((op == "set_dword" || op == "dword_yaz") && argc == 3) {
        const double numeric = numberOf(argument(args, 2));
        if (!std::isfinite(numeric) || numeric < 0 || numeric > std::numeric_limits<DWORD>::max() || std::floor(numeric) != numeric) return box(false);
        HKEY key = nullptr; DWORD disposition = 0;
        const std::wstring path = wide(argumentString(args, 0));
        const std::wstring name = wide(argumentString(args, 1));
        if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) return box(false);
        const DWORD value = static_cast<DWORD>(numeric);
        const LSTATUS status = RegSetValueExW(key, name.empty() ? nullptr : name.c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key); return box(status == ERROR_SUCCESS);
    }
    if ((op == "delete_value" || op == "deger_sil") && argc == 2) {
        HKEY key = nullptr;
        const std::wstring path = wide(argumentString(args, 0));
        const std::wstring name = wide(argumentString(args, 1));
        if (RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return box(false);
        const LSTATUS status = RegDeleteValueW(key, name.empty() ? nullptr : name.c_str());
        RegCloseKey(key); return box(status == ERROR_SUCCESS);
    }
    writeStderr("PRCYTERAS: Registry."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptim codex - 2026-11-15 - Win32 ReadDirectoryChangesW file watcher
struct OcFileWatcher {
    HANDLE directory = INVALID_HANDLE_VALUE;
    HANDLE event = nullptr;
    OVERLAPPED overlapped{};
    std::vector<BYTE> buffer = std::vector<BYTE>(64 * 1024);
    bool recursive = false;
    bool pending = false;
    std::mutex mutex;
};
bool ocStartFileWatch(OcFileWatcher& watcher) {
    if (watcher.directory == INVALID_HANDLE_VALUE || !watcher.event) return false;
    ResetEvent(watcher.event);
    watcher.overlapped = {};
    watcher.overlapped.hEvent = watcher.event;
    const BOOL started = ReadDirectoryChangesW(watcher.directory, watcher.buffer.data(), static_cast<DWORD>(watcher.buffer.size()),
        watcher.recursive ? TRUE : FALSE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES |
        FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
        nullptr, &watcher.overlapped, nullptr);
    watcher.pending = started != FALSE || GetLastError() == ERROR_IO_PENDING;
    return watcher.pending;
}
std::mutex& ocFileWatcherMutex() { static std::mutex mutex; return mutex; }
std::map<int, std::shared_ptr<OcFileWatcher>>& ocFileWatchers() { static std::map<int, std::shared_ptr<OcFileWatcher>> watchers; return watchers; }
std::shared_ptr<OcFileWatcher> ocFindFileWatcher(int id) {
    std::lock_guard<std::mutex> lock(ocFileWatcherMutex());
    const auto found = ocFileWatchers().find(id);
    return found == ocFileWatchers().end() ? nullptr : found->second;
}
void* fileWatcherCall(const std::string& op, void** args, int argc) {
    if ((op == "watch" || op == "izle") && argc >= 1) {
        const std::wstring path = wide(argumentString(args, 0));
        HANDLE directory = CreateFileW(path.c_str(), FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
        if (directory == INVALID_HANDLE_VALUE) return box(std::int64_t{-1});
        auto watcher = std::make_shared<OcFileWatcher>();
        watcher->directory = directory;
        watcher->event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        watcher->recursive = argc >= 2 && truthy(argument(args, 1));
        if (!watcher->event || !ocStartFileWatch(*watcher)) {
            if (watcher->event) CloseHandle(watcher->event);
            CloseHandle(watcher->directory);
            return box(std::int64_t{-1});
        }
        std::lock_guard<std::mutex> lock(ocFileWatcherMutex());
        static int nextId = 1;
        const int id = nextId++;
        ocFileWatchers()[id] = std::move(watcher);
        return box(static_cast<std::int64_t>(id));
    }
    if ((op == "poll" || op == "oku") && argc >= 1) {
        const int id = argumentInt(args, 0);
        auto watcher = ocFindFileWatcher(id);
        if (!watcher) return box(Value{});
        std::lock_guard<std::mutex> watcherLock(watcher->mutex);
        if (watcher->directory == INVALID_HANDLE_VALUE) return box(Value{});
        if (!watcher->pending) return box(std::make_shared<ListValue>());
        const DWORD timeout = static_cast<DWORD>(std::clamp(argc >= 2 ? argumentInt(args, 1) : 0, 0, 60000));
        const DWORD wait = WaitForSingleObject(watcher->event, timeout);
        if (wait != WAIT_OBJECT_0) return box(std::make_shared<ListValue>());
        DWORD bytes = 0;
        const BOOL completed = GetOverlappedResult(watcher->directory, &watcher->overlapped, &bytes, FALSE);
        Value changes = stdlibArray();
        if (completed && bytes > 0) {
            std::size_t offset = 0;
            while (offset + sizeof(FILE_NOTIFY_INFORMATION) <= bytes) {
                const auto* item = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(watcher->buffer.data() + offset);
                const std::size_t chars = item->FileNameLength / sizeof(WCHAR);
                const std::wstring name(item->FileName, chars);
                const char* action = item->Action == FILE_ACTION_ADDED ? "added" :
                    item->Action == FILE_ACTION_REMOVED ? "removed" :
                    item->Action == FILE_ACTION_MODIFIED ? "modified" :
                    item->Action == FILE_ACTION_RENAMED_OLD_NAME ? "renamed_from" :
                    item->Action == FILE_ACTION_RENAMED_NEW_NAME ? "renamed_to" : "unknown";
                Value change = stdlibObject("FileChange");
                ocSetField(change, "action", std::string(action));
                ocSetField(change, "name", utf8(name));
                stdlibArrayAppend(changes, std::move(change));
                if (item->NextEntryOffset == 0 || item->NextEntryOffset > bytes - offset) break;
                offset += item->NextEntryOffset;
            }
        }
        ocStartFileWatch(*watcher);
        return box(std::move(changes));
    }
    if ((op == "close" || op == "kapat") && argc == 1) {
        std::shared_ptr<OcFileWatcher> watcher;
        {
            std::lock_guard<std::mutex> lock(ocFileWatcherMutex());
            const auto found = ocFileWatchers().find(argumentInt(args, 0));
            if (found == ocFileWatchers().end()) return box(false);
            watcher = std::move(found->second);
            ocFileWatchers().erase(found);
        }
        std::lock_guard<std::mutex> watcherLock(watcher->mutex);
        if (watcher->directory != INVALID_HANDLE_VALUE) {
            CancelIoEx(watcher->directory, nullptr);
            if (watcher->event && watcher->pending) WaitForSingleObject(watcher->event, INFINITE);
            CloseHandle(watcher->directory);
            watcher->directory = INVALID_HANDLE_VALUE;
        }
        if (watcher->event) { CloseHandle(watcher->event); watcher->event = nullptr; }
        watcher->pending = false;
        return box(true);
    }
    writeStderr("PRCYTERAS: FileWatcher."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
// ben yaptim codex - 2026-11-15 - XInput controller state via system DLL
struct PtsXInputGamepad { WORD buttons; BYTE leftTrigger; BYTE rightTrigger; SHORT leftX; SHORT leftY; SHORT rightX; SHORT rightY; };
struct PtsXInputState { DWORD packet; PtsXInputGamepad gamepad; };
using PtsXInputGetState = DWORD (WINAPI*)(DWORD, PtsXInputState*);
struct PtsXInputApi { HMODULE module = nullptr; PtsXInputGetState getState = nullptr; };
const PtsXInputApi& ptsXInputApi() {
    static const PtsXInputApi api = [] {
        PtsXInputApi result;
        for (const wchar_t* name : {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"}) {
            result.module = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!result.module) continue;
            result.getState = reinterpret_cast<PtsXInputGetState>(GetProcAddress(result.module, "XInputGetState"));
            if (result.getState) break;
            FreeLibrary(result.module); result.module = nullptr;
        }
        return result;
    }();
    return api;
}
bool ptsReadXInputState(int index, PtsXInputState& state) {
    if (index < 0 || index > 3) return false;
    const auto& api = ptsXInputApi();
    return api.getState && api.getState(static_cast<DWORD>(index), &state) == ERROR_SUCCESS;
}
double ptsNormalizeStick(SHORT value, int deadzone) {
    const int raw = static_cast<int>(value);
    const int magnitude = std::abs(raw);
    if (magnitude <= deadzone) return 0.0;
    const double scaled = static_cast<double>(magnitude - deadzone) / static_cast<double>(32767 - deadzone);
    return std::clamp(raw < 0 ? -scaled : scaled, -1.0, 1.0);
}
void* gamepadCall(const std::string& op, void** args, int argc) {
    if ((op == "connected" || op == "bagli") && argc == 1) {
        PtsXInputState state{}; return box(ptsReadXInputState(argumentInt(args, 0), state));
    }
    if ((op == "state" || op == "durum") && argc == 1) {
        PtsXInputState state{};
        if (!ptsReadXInputState(argumentInt(args, 0), state)) return box(Value{});
        Value result = stdlibObject("GamepadState");
        ocSetField(result, "packet", static_cast<std::uint64_t>(state.packet));
        ocSetField(result, "buttons", static_cast<std::uint64_t>(state.gamepad.buttons));
        ocSetField(result, "left_trigger", static_cast<double>(state.gamepad.leftTrigger) / 255.0);
        ocSetField(result, "right_trigger", static_cast<double>(state.gamepad.rightTrigger) / 255.0);
        ocSetField(result, "left_x", ptsNormalizeStick(state.gamepad.leftX, 7849));
        ocSetField(result, "left_y", ptsNormalizeStick(state.gamepad.leftY, 7849));
        ocSetField(result, "right_x", ptsNormalizeStick(state.gamepad.rightX, 8689));
        ocSetField(result, "right_y", ptsNormalizeStick(state.gamepad.rightY, 8689));
        return box(std::move(result));
    }
    if ((op == "button_down" || op == "tus_basili_mi") && argc == 2) {
        PtsXInputState state{};
        if (!ptsReadXInputState(argumentInt(args, 0), state)) return box(false);
        std::string button = argumentString(args, 1);
        std::transform(button.begin(), button.end(), button.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        static const std::unordered_map<std::string, WORD> masks = {
            {"UP",0x0001},{"DPAD_UP",0x0001},{"DOWN",0x0002},{"DPAD_DOWN",0x0002},
            {"LEFT",0x0004},{"DPAD_LEFT",0x0004},{"RIGHT",0x0008},{"DPAD_RIGHT",0x0008},
            {"START",0x0010},{"BACK",0x0020},{"LEFT_STICK",0x0040},{"RIGHT_STICK",0x0080},
            {"LB",0x0100},{"LEFT_BUMPER",0x0100},{"RB",0x0200},{"RIGHT_BUMPER",0x0200},
            {"A",0x1000},{"B",0x2000},{"X",0x4000},{"Y",0x8000}
        };
        const auto mask = masks.find(button);
        return box(mask != masks.end() && (state.gamepad.buttons & mask->second) != 0);
    }
    writeStderr("PRCYTERAS: Gamepad."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* fontLoaderCall(const std::string& op, void** args, int argc) {
    if ((op == "load" || op == "yukle") && argc == 1) {
        const std::wstring path = wide(argumentString(args, 0));
        return box(!path.empty() && AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr) > 0);
    }
    if ((op == "unload" || op == "kaldir") && argc == 1) {
        const std::wstring path = wide(argumentString(args, 0));
        return box(!path.empty() && RemoveFontResourceExW(path.c_str(), FR_PRIVATE, nullptr) != FALSE);
    }
    writeStderr("PRCYTERAS: FontLoader."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void ptsTrayRemove() {
    if (ptsTrayIconAdded) {
        Shell_NotifyIconW(NIM_DELETE, &ptsTrayIconData);
        ptsTrayIconAdded = false;
    }
    if (ptsTrayOwnedIcon) {
        DestroyIcon(ptsTrayOwnedIcon);
        ptsTrayOwnedIcon = nullptr;
    }
    ptsTrayIconData = {};
}
void* trayIconCall(const std::string& op, void** args, int argc) {
    if ((op == "add" || op == "ekle") && argc == 2) {
        if (!windowHandle || ptsTrayIconAdded) return box(false);
        const std::wstring iconPath = wide(argumentString(args, 0));
        const std::wstring tooltip = wide(argumentString(args, 1));
        HICON icon = nullptr;
        if (!iconPath.empty()) {
            icon = static_cast<HICON>(LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
            if (icon) ptsTrayOwnedIcon = icon;
        }
        if (!icon) icon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
        if (!icon) return box(false);
        ptsTrayIconData = {};
        ptsTrayIconData.cbSize = sizeof(ptsTrayIconData);
        ptsTrayIconData.hWnd = windowHandle;
        ptsTrayIconData.uID = 0x505453;
        ptsTrayIconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        ptsTrayIconData.uCallbackMessage = ptsTrayCallbackMessage;
        ptsTrayIconData.hIcon = icon;
        wcsncpy_s(ptsTrayIconData.szTip, tooltip.c_str(), _TRUNCATE);
        if (!Shell_NotifyIconW(NIM_ADD, &ptsTrayIconData)) {
            ptsTrayRemove();
            return box(false);
        }
        ptsTrayIconAdded = true;
        ptsTrayIconData.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &ptsTrayIconData);
        return box(true);
    }
    if ((op == "set_tooltip" || op == "ipucu_ayarla") && argc == 1) {
        if (!ptsTrayIconAdded) return box(false);
        const std::wstring tooltip = wide(argumentString(args, 0));
        ptsTrayIconData.uFlags = NIF_TIP;
        wcsncpy_s(ptsTrayIconData.szTip, tooltip.c_str(), _TRUNCATE);
        return box(Shell_NotifyIconW(NIM_MODIFY, &ptsTrayIconData) != FALSE);
    }
    if ((op == "balloon" || op == "bildirim") && argc == 3) {
        if (!ptsTrayIconAdded) return box(false);
        const std::wstring title = wide(argumentString(args, 0));
        const std::wstring body = wide(argumentString(args, 1));
        ptsTrayIconData.uFlags = NIF_INFO;
        ptsTrayIconData.dwInfoFlags = NIIF_INFO;
        ptsTrayIconData.uTimeout = static_cast<UINT>(std::clamp(argumentInt(args, 2), 1000, 30000));
        wcsncpy_s(ptsTrayIconData.szInfoTitle, title.c_str(), _TRUNCATE);
        wcsncpy_s(ptsTrayIconData.szInfo, body.c_str(), _TRUNCATE);
        return box(Shell_NotifyIconW(NIM_MODIFY, &ptsTrayIconData) != FALSE);
    }
    if ((op == "poll_event" || op == "olay_al") && argc == 0) {
        std::string event = std::move(ptsTrayLastEvent);
        ptsTrayLastEvent.clear();
        return box(std::move(event));
    }
    if ((op == "remove" || op == "kaldir") && argc == 0) {
        const bool existed = ptsTrayIconAdded;
        ptsTrayRemove();
        return box(existed);
    }
    writeStderr("PRCYTERAS: TrayIcon."); writeStderr(op); writeStderr(" gecersiz veya pencere yok.\n"); return nullptr;
}
bool ocWicContainer(const std::string& format, GUID& container) {
    std::string value = format;
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (value == "png") container = GUID_ContainerFormatPng;
    else if (value == "jpg" || value == "jpeg") container = GUID_ContainerFormatJpeg;
    else if (value == "bmp") container = GUID_ContainerFormatBmp;
    else if (value == "tif" || value == "tiff") container = GUID_ContainerFormatTiff;
    else return false;
    return true;
}
bool ocWicTransform(const std::wstring& inputPath, const std::wstring& outputPath, const std::string& format,
    bool crop, int cropX, int cropY, int outputWidth, int outputHeight) {
    if (inputPath.empty() || outputPath.empty() || outputWidth <= 0 || outputHeight <= 0 ||
        outputWidth > 16384 || outputHeight > 16384 || static_cast<std::uint64_t>(outputWidth) * outputHeight > 64000000ULL) return false;
    GUID container{};
    if (!ocWicContainer(format, container)) return false;
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(com);
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* decoded = nullptr;
    IWICBitmapClipper* clipper = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* converter = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* encodedFrame = nullptr;
    IPropertyBag2* properties = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromFilename(inputPath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &decoded);
    UINT sourceWidth = 0, sourceHeight = 0;
    if (SUCCEEDED(hr)) hr = decoded->GetSize(&sourceWidth, &sourceHeight);
    if (SUCCEEDED(hr) && (sourceWidth == 0 || sourceHeight == 0 || sourceWidth > 16384 || sourceHeight > 16384)) hr = E_INVALIDARG;
    if (SUCCEEDED(hr) && crop && (cropX < 0 || cropY < 0 ||
        static_cast<std::uint64_t>(cropX) + outputWidth > sourceWidth || static_cast<std::uint64_t>(cropY) + outputHeight > sourceHeight)) hr = E_INVALIDARG;
    IWICBitmapSource* source = decoded;
    if (SUCCEEDED(hr) && crop) {
        hr = factory->CreateBitmapClipper(&clipper);
        if (SUCCEEDED(hr)) { WICRect rectangle{cropX, cropY, outputWidth, outputHeight}; hr = clipper->Initialize(source, &rectangle); }
        if (SUCCEEDED(hr)) source = clipper;
    }
    UINT encodedWidth = outputWidth, encodedHeight = outputHeight;
    if (SUCCEEDED(hr) && !crop) { encodedWidth = sourceWidth; encodedHeight = sourceHeight; }
    if (SUCCEEDED(hr) && !crop && (encodedWidth != outputWidth || encodedHeight != outputHeight)) {
        hr = factory->CreateBitmapScaler(&scaler);
        if (SUCCEEDED(hr)) hr = scaler->Initialize(source, outputWidth, outputHeight, WICBitmapInterpolationModeFant);
        if (SUCCEEDED(hr)) { source = scaler; encodedWidth = outputWidth; encodedHeight = outputHeight; }
    }
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) hr = converter->Initialize(source, GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(hr)) hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(outputPath.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = factory->CreateEncoder(container, nullptr, &encoder);
    if (SUCCEEDED(hr)) hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&encodedFrame, &properties);
    if (SUCCEEDED(hr)) hr = encodedFrame->Initialize(properties);
    if (SUCCEEDED(hr)) hr = encodedFrame->SetSize(encodedWidth, encodedHeight);
    WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat24bppBGR;
    if (SUCCEEDED(hr)) hr = encodedFrame->SetPixelFormat(&pixelFormat);
    if (SUCCEEDED(hr) && !IsEqualGUID(pixelFormat, GUID_WICPixelFormat24bppBGR)) hr = WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
    if (SUCCEEDED(hr)) hr = encodedFrame->WriteSource(converter, nullptr);
    if (SUCCEEDED(hr)) hr = encodedFrame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();
    if (properties) properties->Release();
    if (encodedFrame) encodedFrame->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();
    if (converter) converter->Release();
    if (scaler) scaler->Release();
    if (clipper) clipper->Release();
    if (decoded) decoded->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    if (uninitialize) CoUninitialize();
    return SUCCEEDED(hr);
}
void* imageEditCall(const std::string& op, void** args, int argc) {
    if ((op == "resize" || op == "boyutlandir") && argc == 4) {
        const int width = argumentInt(args, 2), height = argumentInt(args, 3);
        std::string format = fs::path(wide(argumentString(args, 1))).extension().string();
        if (!format.empty() && format.front() == '.') format.erase(format.begin());
        return box(ocWicTransform(wide(argumentString(args, 0)), wide(argumentString(args, 1)), format, false, 0, 0, width, height));
    }
    if ((op == "crop" || op == "kirp") && argc == 6) {
        const int x = argumentInt(args, 2), y = argumentInt(args, 3), width = argumentInt(args, 4), height = argumentInt(args, 5);
        std::string format = fs::path(wide(argumentString(args, 1))).extension().string();
        if (!format.empty() && format.front() == '.') format.erase(format.begin());
        return box(ocWicTransform(wide(argumentString(args, 0)), wide(argumentString(args, 1)), format, true, x, y, width, height));
    }
    if ((op == "convert" || op == "donustur") && argc == 3) {
        const std::wstring input = wide(argumentString(args, 0));
        const std::wstring output = wide(argumentString(args, 1));
        IWICImagingFactory* factory = nullptr; IWICBitmapDecoder* decoder = nullptr; IWICBitmapFrameDecode* frame = nullptr;
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED); const bool uninitialize = SUCCEEDED(com);
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromFilename(input.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
        if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
        UINT width = 0, height = 0; if (SUCCEEDED(hr)) hr = frame->GetSize(&width, &height);
        if (frame) frame->Release(); if (decoder) decoder->Release(); if (factory) factory->Release(); if (uninitialize) CoUninitialize();
        if (FAILED(hr)) return box(false);
        return box(ocWicTransform(input, output, argumentString(args, 2), false, 0, 0, static_cast<int>(width), static_cast<int>(height)));
    }
    writeStderr("PRCYTERAS: ImageEdit."); writeStderr(op); writeStderr(" gecersiz.\n"); return nullptr;
}
void* stdlibCall(const std::string& module, const std::string& op, void** args, int argc) {
    if (PTS_MODULE_ENABLED(Clipboard) && (module == "Clipboard" || module == "clipboard")) return clipboardCall(op,args,argc);
    if (PTS_MODULE_ENABLED(WinDialog) && (module == "WinDialog" || module == "win-dialog")) return winDialogCall(op,args,argc);
    if (PTS_MODULE_ENABLED(Screenshot) && (module == "Screenshot" || module == "screenshot")) return screenshotCall(op,args,argc);
    if (PTS_MODULE_ENABLED(GifRecorder) && (module == "GifRecorder" || module == "gif-recorder" || module == "gifrecorder")) return gifRecorderCall(op,args,argc);
    if (PTS_MODULE_ENABLED(Registry) && (module == "Registry" || module == "registry")) return registryCall(op,args,argc);
    if (PTS_MODULE_ENABLED(FileWatcher) && (module == "FileWatcher" || module == "file-watcher" || module == "filewatcher")) return fileWatcherCall(op,args,argc);
    if (PTS_MODULE_ENABLED(Gamepad) && (module == "Gamepad" || module == "gamepad")) return gamepadCall(op,args,argc);
    if (PTS_MODULE_ENABLED(FontLoader) && (module == "FontLoader" || module == "font-loader" || module == "fontloader")) return fontLoaderCall(op,args,argc);
    if (PTS_MODULE_ENABLED(ImageEdit) && (module == "ImageEdit" || module == "image-edit" || module == "imageedit")) return imageEditCall(op,args,argc);
    if (PTS_MODULE_ENABLED(TrayIcon) && (module == "TrayIcon" || module == "tray-icon" || module == "trayicon")) return trayIconCall(op,args,argc);
    if (PTS_MODULE_ENABLED(Json) && (module == "Json" || module == "json")) return jsonCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Base64) && (module == "Base64" || module == "base64")) return base64Call(op, args, argc);
    if (PTS_MODULE_ENABLED(Regex) && (module == "Regex" || module == "regex")) return regexCall(op, args, argc);
    if (PTS_MODULE_ENABLED(DateTime) && (module == "DateTime" || module == "datetime")) return dateTimeCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Toml) && (module == "Toml" || module == "toml")) return tomlCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Yaml) && (module == "Yaml" || module == "yaml")) return yamlCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Xml) && (module == "Xml" || module == "xml")) return xmlCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Dotenv) && (module == "Dotenv" || module == "dotenv")) return dotenvCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Uuid) && (module == "Uuid" || module == "uuid")) return uuidCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Logger) && (module == "Logger" || module == "logger")) return loggerCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Stats) && (module == "Stats" || module == "stats")) return statsCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Fuzzy) && (module == "Fuzzy" || module == "fuzzy" || module == "fuzzy-search" || module == "fuzzy_search")) return fuzzyCall(op, args, argc);
    // ben yaptım opencode - 2026-11-14 - yeni 100 kutuphane yonlendirmesi
    if (PTS_MODULE_ENABLED(LinAlg) && (module == "LinAlg" || module == "linalg" || module == "linear-algebra")) return linAlgCall(op, args, argc);
    if (PTS_MODULE_ENABLED(FixedPoint) && (module == "FixedPoint" || module == "fixed-point" || module == "fixedpoint")) return fixedPointCall(op, args, argc);
    if (PTS_MODULE_ENABLED(BigInt) && (module == "BigInt" || module == "bigint" || module == "big-int")) return bigIntCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Decimal) && (module == "Decimal" || module == "decimal")) return decimalCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Noise) && (module == "Noise" || module == "noise")) return noiseCall(op, args, argc);
    if (PTS_MODULE_ENABLED(BinaryPack) && (module == "BinaryPack" || module == "binary-pack" || module == "binarypack")) return binaryPackCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Profiler) && (module == "Profiler" || module == "profiler")) return profilerCall(op, args, argc);
    if (PTS_MODULE_ENABLED(UnitTest) && (module == "UnitTest" || module == "unit-test" || module == "unittest")) return unitTestCall(op, args, argc);
    if (PTS_MODULE_ENABLED(CliArgs) && (module == "CliArgs" || module == "cli-args" || module == "cliargs")) return cliArgsCall(op, args, argc);
    if (PTS_MODULE_ENABLED(TerminalUi) && (module == "TerminalUi" || module == "terminal-ui" || module == "terminalui")) return terminalUiCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Pathfinding) && (module == "Pathfinding" || module == "pathfinding")) return pathfindingCall(op, args, argc);
    if (PTS_MODULE_ENABLED(NavMesh3D) && (module == "NavMesh3D" || module == "navmesh3d" || module == "nav-mesh-3d")) return navMesh3DCall(op, args, argc);
    if (PTS_MODULE_ENABLED(BehaviorTree) && (module == "BehaviorTree" || module == "behavior-tree" || module == "behaviortree")) return behaviorTreeCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Ecs) && (module == "Ecs" || module == "ecs")) return ecsCall(op, args, argc);
    if (PTS_MODULE_ENABLED(RateLimiter) && (module == "RateLimiter" || module == "rate-limiter" || module == "ratelimiter")) return rateLimiterCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Scheduler) && (module == "Scheduler" || module == "scheduler")) return schedulerCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Localization) && (module == "Localization" || module == "localization")) return localizationCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Tween) && (module == "Tween" || module == "tween")) return tweenCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Camera2D) && (module == "Camera2D" || module == "camera2d" || module == "camera-2d")) return camera2dCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Particle2D) && (module == "Particle2D" || module == "particle2d" || module == "particle-2d")) return particle2dCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Lighting2D) && (module == "Lighting2D" || module == "lighting2d" || module == "lighting-2d")) return lighting2dCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Physics2D) && (module == "Physics2D" || module == "physics2d" || module == "physics-2d")) return physics2dCall(op, args, argc);
    if (PTS_MODULE_ENABLED(VectorDb) && (module == "VectorDb" || module == "vector-db" || module == "vectordb")) return vectorDbCall(op, args, argc);
    if (PTS_MODULE_ENABLED(NeuralNet) && (module == "NeuralNet" || module == "neural-net" || module == "neuralnet")) return neuralNetCall(op, args, argc);
    if (PTS_MODULE_ENABLED(NeatAi) && (module == "NeatAi" || module == "neat-ai" || module == "neatai")) return neatAiCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Crypto) && (module == "Crypto" || module == "crypto")) return cryptoCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Jwt) && (module == "Jwt" || module == "jwt")) return jwtCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Argon2) && (module == "Argon2" || module == "argon2")) return argon2Call(op, args, argc);
    if (PTS_MODULE_ENABLED(ZipArchive) && (module == "ZipArchive" || module == "zip-archive" || module == "ziparchive" || module == "zip")) return zipArchiveCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Html) && (module == "Html" || module == "html")) return htmlCall(op, args, argc);
    if (PTS_MODULE_ENABLED(QrCode) && (module == "QrCode" || module == "qr-code" || module == "qrcode")) return qrCodeCall(op, args, argc);
    // ben yaptım opencode - 2026-11-14 - ag kutuphanesi yonlendirmesi
    if (PTS_MODULE_ENABLED(Http) && (module == "Http" || module == "http")) return httpCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Socket) && (module == "Socket" || module == "socket")) return socketCall(op, args, argc);
    if (PTS_MODULE_ENABLED(WebSocket) && (module == "WebSocket" || module == "websocket" || module == "web-socket")) return webSocketCall(op, args, argc);
    if (PTS_MODULE_ENABLED(WebServer) && (module == "WebServer" || module == "web-server" || module == "webserver")) return webServerCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Downloader) && (module == "Downloader" || module == "downloader")) return downloaderCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Ftp) && (module == "Ftp" || module == "ftp" || module == "ftp-client")) return ftpCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Dns) && (module == "Dns" || module == "dns" || module == "dns-lookup")) return dnsCall(op, args, argc);
    if (PTS_MODULE_ENABLED(OAuth2) && (module == "OAuth2" || module == "oauth2")) return oauth2Call(op, args, argc);
    if (PTS_MODULE_ENABLED(Upnp) && (module == "Upnp" || module == "upnp")) return upnpCall(op, args, argc);
    // ben yaptım opencode - 2026-11-15 - veri servisleri yonlendirmesi
    if (PTS_MODULE_ENABLED(Sqlite) && (module == "Sqlite" || module == "sqlite")) return sqliteCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Redis) && (module == "Redis" || module == "redis")) return redisCall(op, args, argc);
    if (PTS_MODULE_ENABLED(MySql) && (module == "MySql" || module == "mysql")) return mysqlCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Smtp) && (module == "Smtp" || module == "smtp" || module == "smtp-mail")) return smtpCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Irc) && (module == "Irc" || module == "irc" || module == "irc-client")) return ircCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Webhook) && (module == "Webhook" || module == "webhook")) return webhookCall(op, args, argc);
    // Packages below have native implementations in this runtime. Keep this
    // dispatch table explicit so an import cannot silently resolve to a stub.
    if (PTS_MODULE_ENABLED(OpenAi) && (module == "OpenAi" || module == "openai")) return openAiCall(op, args, argc);
    if (PTS_MODULE_ENABLED(GoogleGenai) && (module == "GoogleGenai" || module == "google-genai")) return googleGenaiCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Anthropic) && (module == "Anthropic" || module == "anthropic")) return anthropicCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Ollama) && (module == "Ollama" || module == "ollama")) return ollamaCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Groq) && (module == "Groq" || module == "groq")) return groqCall(op, args, argc);
    if (PTS_MODULE_ENABLED(HuggingFace) && (module == "HuggingFace" || module == "huggingface")) return huggingFaceCall(op, args, argc);
    if (PTS_MODULE_ENABLED(ElevenLabs) && (module == "ElevenLabs" || module == "elevenlabs")) return elevenLabsCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Whisper) && (module == "Whisper" || module == "whisper-pts" || module == "whisper")) return whisperCall(op, args, argc);
    if (PTS_MODULE_ENABLED(StabilityAi) && (module == "StabilityAi" || module == "stability-ai")) return stabilityAiCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Discord) && (module == "Discord" || module == "discord")) return discordCall(op, args, argc);
    if (PTS_MODULE_ENABLED(DiscordRpc) && (module == "DiscordRpc" || module == "discord-rpc")) return discordRpcCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Telegram) && (module == "Telegram" || module == "telegram")) return telegramCall(op, args, argc);
    if (PTS_MODULE_ENABLED(WhatsApp) && (module == "WhatsApp" || module == "whatsapp-cloud")) return whatsAppCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Twitch) && (module == "Twitch" || module == "twitch-chat")) return twitchCall(op, args, argc);
    if (PTS_MODULE_ENABLED(YouTubeLive) && (module == "YouTubeLive" || module == "youtube-live")) return youTubeLiveCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Slack) && (module == "Slack" || module == "slack-bot")) return slackCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Firebase) && (module == "Firebase" || module == "firebase")) return firebaseCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Supabase) && (module == "Supabase" || module == "supabase")) return supabaseCall(op, args, argc);
    if (PTS_MODULE_ENABLED(Steam) && (module == "Steam" || module == "steamworks")) return steamCall(op, args, argc);
    writeStderr("PRCYTERAS: bilinmeyen standart kutuphane modulu: ");
    writeStderr(module);
    writeStderr("\n");
    return nullptr;
}
}

extern "C" {
PTS_RUNTIME_API(pts_bootstrap) void pts_bootstrap() { }
PTS_RUNTIME_API(pts_global_env) void* pts_global_env() { return globalEnv(); }
PTS_RUNTIME_API(pts_env_create) void* pts_env_create(void* parent, void* self) { auto* env=new Environment();env->parent=static_cast<Environment*>(parent);if(self)env->self=std::get<std::shared_ptr<Object>>(valueOf(self));return env; }
PTS_RUNTIME_API(pts_arg) void* pts_arg(void** args,int index) { return args && index>=0 ? args[index] : nullptr; }
PTS_RUNTIME_API(pts_as_int) std::int64_t pts_as_int(void* value) { return static_cast<std::int64_t>(numberOf(valueOf(value))); }
PTS_RUNTIME_API(pts_as_uint) std::uint64_t pts_as_uint(void* value) {
    const Value v = valueOf(value);
    if (const auto* item = std::get_if<std::uint64_t>(&v)) return *item;
    if (const auto* item = std::get_if<std::int64_t>(&v)) return static_cast<std::uint64_t>(*item);
    if (const auto* item = std::get_if<double>(&v)) return static_cast<std::uint64_t>(std::max(0.0, *item));
    return 0;
}
PTS_RUNTIME_API(pts_as_float) double pts_as_float(void* value) { return numberOf(valueOf(value)); }
PTS_RUNTIME_API(pts_as_bool) bool pts_as_bool(void* value) { return truthy(valueOf(value)); }
PTS_RUNTIME_API(pts_int_div) std::int64_t pts_int_div(std::int64_t a,std::int64_t b) { if(b==0)return 0;if(a==std::numeric_limits<std::int64_t>::min()&&b==-1)return a;return a/b; }
PTS_RUNTIME_API(pts_int_mod) std::int64_t pts_int_mod(std::int64_t a,std::int64_t b) { if(b==0||(a==std::numeric_limits<std::int64_t>::min()&&b==-1))return 0;return a%b; }
PTS_RUNTIME_API(pts_uint_div) std::uint64_t pts_uint_div(std::uint64_t a,std::uint64_t b) { return b==0?0:a/b; }
PTS_RUNTIME_API(pts_uint_mod) std::uint64_t pts_uint_mod(std::uint64_t a,std::uint64_t b) { return b==0?0:a%b; }
PTS_RUNTIME_API(pts_env_set) void pts_env_set(void* envHandle,const char* name,void* value) { if(envHandle)static_cast<Environment*>(envHandle)->values[name]=valueOf(value); }
PTS_RUNTIME_API(pts_env_get) void* pts_env_get(void* envHandle,const char* name) { auto* env=static_cast<Environment*>(envHandle);const std::string_view key=name?name:"";for(auto* e=env;e;e=e->parent){if(key=="self"&&e->self)return box(e->self);auto it=e->values.find(key);if(it!=e->values.end())return box(it->second);if(e->self){const Value* f=objectField(*e->self,name);if(f)return box(*f);}}return nullptr; }
PTS_RUNTIME_API(pts_null) void* pts_null() { return nullptr; }
PTS_RUNTIME_API(pts_int) void* pts_int(std::int64_t value) { return box(value); }
PTS_RUNTIME_API(pts_box_pin) void pts_box_pin(void* value) { temporaryValuesState().pin(value); }
PTS_RUNTIME_API(pts_box_unpin) void pts_box_unpin(void* value) { temporaryValuesState().unpin(value); }
PTS_RUNTIME_API(pts_box_copy) void* pts_box_copy(void* value) { return box(valueOf(value)); }
PTS_RUNTIME_API(pts_uint) void* pts_uint(std::uint64_t value) { return box(value); }
PTS_RUNTIME_API(pts_float) void* pts_float(double value) { return box(value); }
PTS_RUNTIME_API(pts_bool) void* pts_bool(bool value) { return box(value); }
PTS_RUNTIME_API(pts_string) void* pts_string(const char* value) { return box(std::string(value?value:"")); }
PTS_RUNTIME_API(pts_to_string) void* pts_to_string(void* value) { return box(stringOf(valueOf(value))); }
PTS_RUNTIME_API(pts_print) void pts_print(void** args,int argc) { for(int i=0;i<argc;++i)writeStdout(stringOf(argument(args,i)));writeStdout("\n"); }
PTS_RUNTIME_API(pts_color) void* pts_color(const char* value) { return box(std::string(value?value:"")); }
PTS_RUNTIME_API(pts_color_call) void* pts_color_call(const char* name,void** args,int argc) { return box(colorCall(name?name:"",args,argc)); }
PTS_RUNTIME_API(pts_color_count) std::uint64_t pts_color_count() { return sizeof(namedPalette) / sizeof(namedPalette[0]); }
PTS_RUNTIME_API(pts_color_generated_count) std::uint64_t pts_color_generated_count() { return 10000; }
PTS_RUNTIME_API(pts_binary) void* pts_binary(const char* op,void* a,void* b) { return box(binary(op?op:"",valueOf(a),valueOf(b))); }
PTS_RUNTIME_API(pts_unary) void* pts_unary(const char* op,void* a) { return box(unary(op?op:"",valueOf(a))); }
PTS_RUNTIME_API(pts_truthy) bool pts_truthy(void* value) { return truthy(valueOf(value)); }
PTS_RUNTIME_API(pts_get_member) void* pts_get_member(void* object,const char* name) { auto v=valueOf(object);if(auto o=std::get_if<std::shared_ptr<Object>>(&v)){const Value* field=objectField(**o,name?name:"");if(field)return box(*field);}return nullptr; }
PTS_RUNTIME_API(pts_set_member) void pts_set_member(void* object,const char* name,void* value) { auto v=valueOf(object);if(auto o=std::get_if<std::shared_ptr<Object>>(&v))setObjectField(**o,name?name:"",valueOf(value)); }
PTS_RUNTIME_API(pts_get_member_index) void* pts_get_member_index(void* object,std::int64_t index) { if(index<0)return nullptr;return boxedObjectFieldAt(object,static_cast<std::size_t>(index)); }
PTS_RUNTIME_API(pts_set_member_index) void pts_set_member_index(void* object,std::int64_t index,void* value) { if(index<0)return;setObjectFieldAt(object,static_cast<std::size_t>(index),value); }
PTS_RUNTIME_API(pts_get_member_i64) std::int64_t pts_get_member_i64(void* object,std::int64_t index) { if(index<0)return 0;const Value* field=objectFieldAtHandle(object,static_cast<std::size_t>(index));return field?static_cast<std::int64_t>(numberOf(*field)):0; }
PTS_RUNTIME_API(pts_get_member_u64) std::uint64_t pts_get_member_u64(void* object,std::int64_t index) { if(index<0)return 0;const Value* field=objectFieldAtHandle(object,static_cast<std::size_t>(index));if(!field)return 0;if(const auto* value=std::get_if<std::uint64_t>(field))return *value;if(const auto* value=std::get_if<std::int64_t>(field))return static_cast<std::uint64_t>(*value);return static_cast<std::uint64_t>(std::max(0.0,numberOf(*field))); }
PTS_RUNTIME_API(pts_get_member_f64) double pts_get_member_f64(void* object,std::int64_t index) { if(index<0)return 0.0;const Value* field=objectFieldAtHandle(object,static_cast<std::size_t>(index));return field?numberOf(*field):0.0; }
PTS_RUNTIME_API(pts_get_member_bool) bool pts_get_member_bool(void* object,std::int64_t index) { if(index<0)return false;const Value* field=objectFieldAtHandle(object,static_cast<std::size_t>(index));return field?truthy(*field):false; }
PTS_RUNTIME_API(pts_set_member_i64) void pts_set_member_i64(void* object,std::int64_t index,std::int64_t value) { if(index>=0)setObjectFieldAtValue(object,static_cast<std::size_t>(index),Value{value}); }
PTS_RUNTIME_API(pts_set_member_u64) void pts_set_member_u64(void* object,std::int64_t index,std::uint64_t value) { if(index>=0)setObjectFieldAtValue(object,static_cast<std::size_t>(index),Value{value}); }
PTS_RUNTIME_API(pts_set_member_f64) void pts_set_member_f64(void* object,std::int64_t index,double value) { if(index>=0)setObjectFieldAtValue(object,static_cast<std::size_t>(index),Value{value}); }
PTS_RUNTIME_API(pts_set_member_bool) void pts_set_member_bool(void* object,std::int64_t index,bool value) { if(index>=0)setObjectFieldAtValue(object,static_cast<std::size_t>(index),Value{value}); }
PTS_RUNTIME_API(pts_list_new) void* pts_list_new(bool flow) { auto list=std::make_shared<ListValue>();list->flow=flow;return box(list); }
static bool listIndex(const Value& indexValue,std::size_t& wanted) {
    if(const auto* value=std::get_if<std::int64_t>(&indexValue)) {
        if(*value<0)return false;
        wanted=static_cast<std::size_t>(*value);
        return true;
    }
    if(const auto* value=std::get_if<std::uint64_t>(&indexValue)) {
        if(*value>static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))return false;
        wanted=static_cast<std::size_t>(*value);
        return true;
    }
    return false;
}
static bool liveListElement(const ListValue& list,std::size_t wanted,std::size_t& physical) {
    std::size_t visible=0;
    for(std::size_t i=0;i<list.values.size();++i) {
        if(i<list.active.size()&&!list.active[i])continue;
        if(visible++==wanted){physical=i;return true;}
    }
    return false;
}
PTS_RUNTIME_API(pts_index_get) void* pts_index_get(void* handle,void* index) {
    const Value listValue=valueOf(handle),indexValue=valueOf(index);
    const auto* list=std::get_if<std::shared_ptr<ListValue>>(&listValue);
    std::size_t wanted=0,physical=0;
    if(!list||!listIndex(indexValue,wanted)||!liveListElement(**list,wanted,physical)) {
        writeStderr("PRCYTERAS calisma zamani hatasi: liste indisi gecersiz veya sinir disi.\n");
        return nullptr;
    }
    return box((*list)->values[physical]);
}
PTS_RUNTIME_API(pts_index_set) void pts_index_set(void* handle,void* index,void* replacement) {
    const Value listValue=valueOf(handle),indexValue=valueOf(index);
    const auto* list=std::get_if<std::shared_ptr<ListValue>>(&listValue);
    std::size_t wanted=0,physical=0;
    if(!list||!listIndex(indexValue,wanted)||!liveListElement(**list,wanted,physical)) {
        writeStderr("PRCYTERAS calisma zamani hatasi: liste indisi gecersiz veya sinir disi.\n");
        return;
    }
    (*list)->values[physical]=valueOf(replacement);
}
// ben yaptım codex - 2026-11-12 - Zig/Clang noreturn öznitelik uyumluluğu.
[[noreturn]] PTS_RUNTIME_API(pts_array_bounds_fail) void pts_array_bounds_fail(std::uint64_t index,std::uint64_t length) {
    writeStderr("PRCYTERAS calisma zamani hatasi: sabit dizi indisi "+std::to_string(index)+
        " sinir disinda (uzunluk "+std::to_string(length)+").\n");
    std::abort();
}
PTS_RUNTIME_API(pts_file_call) void* pts_file_call(const char* name,void** args,int argc) {
    const std::string op=name?name:"";
    auto pathAt=[&](int i){return fs::path(wide(argumentString(args,i)));};
    auto report=[&](const std::error_code& ec){if(ec){writeStderr("PRCYTERAS File.");writeStderr(op);writeStderr(" hatasi: ");writeStderr(ec.message());writeStderr("\n");}};
    if((op=="exists"||op=="var_mi")&&argc==1){std::error_code ec;const bool result=fs::exists(pathAt(0),ec);report(ec);return box(!ec&&result);}
    if((op=="is_file"||op=="dosya_mi")&&argc==1){const DWORD attributes=GetFileAttributesW(windowsApiPath(pathAt(0)).c_str());return box(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY)==0);}
    if((op=="is_directory"||op=="dizin_mi")&&argc==1){const DWORD attributes=GetFileAttributesW(windowsApiPath(pathAt(0)).c_str());return box(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY)!=0);}
    if((op=="read_text"||op=="oku")&&argc==1){const fs::path p=pathAt(0);std::ifstream input(p,std::ios::binary);if(!input){writeStderr("PRCYTERAS File.read_text: dosya okunamadi.\n");return nullptr;}std::string text((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());if(input.bad()){writeStderr("PRCYTERAS File.read_text: okuma sirasinda hata.\n");return nullptr;}return box(std::move(text));}
    if((op=="write_text"||op=="yaz")&&argc==2){const fs::path p=pathAt(0);const std::string text=argumentString(args,1);std::ofstream output(p,std::ios::binary|std::ios::trunc);if(!output){writeStderr("PRCYTERAS File.write_text: dosya olusturulamadi.\n");return box(false);}output.write(text.data(),static_cast<std::streamsize>(text.size()));output.close();if(!output){writeStderr("PRCYTERAS File.write_text: yazma sirasinda hata.\n");return box(false);}return box(true);}
    if((op=="append_text"||op=="ekle")&&argc==2){const fs::path p=pathAt(0);const std::string text=argumentString(args,1);std::ofstream output(p,std::ios::binary|std::ios::app);if(!output){writeStderr("PRCYTERAS File.append_text: dosya acilamadi.\n");return box(false);}output.write(text.data(),static_cast<std::streamsize>(text.size()));output.close();if(!output){writeStderr("PRCYTERAS File.append_text: yazma sirasinda hata.\n");return box(false);}return box(true);}
    if((op=="create_directory"||op=="dizin_olustur")&&argc==1){const fs::path p=pathAt(0);std::error_code ec;fs::create_directories(p,ec);report(ec);return box(!ec&&fs::is_directory(p,ec)&&!ec);}
    if((op=="delete_file"||op=="sil_dosya")&&argc==1){const fs::path p=pathAt(0);std::error_code ec;const auto status=fs::symlink_status(p,ec);if(ec||status.type()==fs::file_type::not_found||status.type()==fs::file_type::directory)return box(false);const bool result=fs::remove(p,ec);report(ec);return box(!ec&&result);}
    if((op=="remove_empty_directory"||op=="bos_dizin_sil")&&argc==1){const fs::path p=pathAt(0);std::error_code ec;const auto status=fs::symlink_status(p,ec);if(ec||status.type()!=fs::file_type::directory)return box(false);const bool result=fs::remove(p,ec);report(ec);return box(!ec&&result);}
    if((op=="remove_tree"||op=="dizin_agacini_sil")&&argc==1){
        std::error_code ec;const fs::path absolute=fs::absolute(pathAt(0),ec).lexically_normal();if(ec){report(ec);return box(false);}if(absolute.empty()||absolute==absolute.root_path())return box(false);
        const fs::path current=fs::current_path(ec).lexically_normal();if(ec){report(ec);return box(false);}
        auto sameOrAncestor=[](const fs::path& parent,const fs::path& child){auto p=parent.begin(),c=child.begin();for(;p!=parent.end();++p,++c){if(c==child.end())return false;const std::wstring a=p->wstring(),b=c->wstring();if(CompareStringOrdinal(a.c_str(),static_cast<int>(a.size()),b.c_str(),static_cast<int>(b.size()),TRUE)!=CSTR_EQUAL)return false;}return true;};
        if(sameOrAncestor(absolute,current))return box(false);
        const auto status=fs::symlink_status(absolute,ec);if(ec||status.type()!=fs::file_type::directory)return box(false);
        const auto removed=fs::remove_all(absolute,ec);report(ec);return box(!ec&&removed>0);
    }
    if((op=="copy"||op=="kopyala")&&(argc==2||argc==3)){std::error_code ec;const auto options=argc==3&&truthy(argument(args,2))?fs::copy_options::overwrite_existing:fs::copy_options::none;const bool result=fs::copy_file(pathAt(0),pathAt(1),options,ec);report(ec);return box(!ec&&result);}
    if((op=="move"||op=="tasi")&&argc==2){std::error_code ec;fs::rename(pathAt(0),pathAt(1),ec);report(ec);return box(!ec);}
    if((op=="size"||op=="boyut")&&argc==1){std::error_code ec;const auto result=fs::file_size(pathAt(0),ec);report(ec);return ec?box(std::int64_t{-1}):box(static_cast<std::uint64_t>(result));}
    if((op=="list"||op=="listele")&&argc==1){std::error_code ec;std::vector<std::string> names;const fs::path p=pathAt(0);for(fs::directory_iterator it(p,ec),end;!ec&&it!=end;it.increment(ec))names.push_back(utf8(it->path().filename().wstring()));report(ec);std::sort(names.begin(),names.end());auto list=std::make_shared<ListValue>();for(auto& item:names){list->values.emplace_back(std::move(item));list->active.push_back(true);}return box(std::move(list));}
    if(op=="list_safe"&&argc==1){
        const fs::path p=pathAt(0);
        const fs::path apiDirectory=windowsApiPath(p);const DWORD rootAttributes=GetFileAttributesW(apiDirectory.c_str());
        if(rootAttributes==INVALID_FILE_ATTRIBUTES||(rootAttributes&FILE_ATTRIBUTE_DIRECTORY)==0)return box(std::make_shared<ListValue>());
        std::wstring pattern=apiDirectory.wstring();if(pattern.empty()||(pattern.back()!=L'\\'&&pattern.back()!=L'/'))pattern.push_back(L'\\');pattern.push_back(L'*');
        WIN32_FIND_DATAW data{};HANDLE search=FindFirstFileW(pattern.c_str(),&data);std::vector<std::string> names;
        if(search!=INVALID_HANDLE_VALUE){do{if(std::wcscmp(data.cFileName,L".")==0||std::wcscmp(data.cFileName,L"..")==0)continue;if((data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0&&(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==0)continue;names.push_back(utf8(data.cFileName));}while(FindNextFileW(search,&data));FindClose(search);}
        std::sort(names.begin(),names.end());auto list=std::make_shared<ListValue>();for(auto& item:names){list->values.emplace_back(std::move(item));list->active.push_back(true);}return box(std::move(list));
    }
    if((op=="scan_reset"||op=="tarama_sifirla")&&argc==1){
        scannedDirectoryIdsState().clear();scanRootFinalPathState().clear();scanRootVolumeSerial=0;BY_HANDLE_FILE_INFORMATION info{};std::wstring finalPath;
        if(!getPathIdentityAndFinal(pathAt(0),true,info,finalPath))return box(false);
        scanRootVolumeSerial=info.dwVolumeSerialNumber;scanRootFinalPathState()=std::move(finalPath);return box(true);
    }
    if((op=="scan_enter_directory"||op=="tarama_dizini_kaydet")&&argc==1){
        if(scanRootFinalPathState().empty())return box(false);BY_HANDLE_FILE_INFORMATION info{};std::wstring finalPath;
        if(!getPathIdentityAndFinal(pathAt(0),true,info,finalPath)||info.dwVolumeSerialNumber!=scanRootVolumeSerial||finalPath.size()<scanRootFinalPathState().size())return box(false);
        if(CompareStringOrdinal(finalPath.c_str(),static_cast<int>(scanRootFinalPathState().size()),scanRootFinalPathState().c_str(),static_cast<int>(scanRootFinalPathState().size()),TRUE)!=CSTR_EQUAL)return box(false);
        const std::uint64_t fileId=(static_cast<std::uint64_t>(info.nFileIndexHigh)<<32)|info.nFileIndexLow;
        const std::string key=std::to_string(info.dwVolumeSerialNumber)+":"+std::to_string(fileId);
        return box(scannedDirectoryIdsState().insert(key).second);
    }
    if((op=="drives"||op=="suruculer")&&argc==0){auto list=std::make_shared<ListValue>();const DWORD mask=GetLogicalDrives();for(unsigned int i=0;i<26;++i)if(mask&(1U<<i)){std::string root;root.push_back(static_cast<char>('A'+i));root+=":\\";list->values.emplace_back(std::move(root));list->active.push_back(true);}return box(std::move(list));}
    if((op=="walk"||op=="recursive_list"||op=="agac_listele")&&(argc==1||argc==2)){
        const fs::path root=pathAt(0);std::error_code ec;const auto rootStatus=fs::symlink_status(root,ec);
        if(ec||rootStatus.type()!=fs::file_type::directory)return box(std::make_shared<ListValue>());
        const double requested=argc==2?numberOf(argument(args,1)):100000.0;
        const std::size_t limit=static_cast<std::size_t>(std::clamp(requested,1.0,1000000.0));
        auto list=std::make_shared<ListValue>();
        fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,ec),end;
        while(!ec&&it!=end&&list->values.size()<limit){
            const auto status=it->symlink_status(ec);if(ec){ec.clear();it.increment(ec);continue;}
            const DWORD attributes=GetFileAttributesW(it->path().c_str());
            const bool reparse=attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0;
            if(reparse){if(it->is_directory(ec))it.disable_recursion_pending();it.increment(ec);continue;}
            if(status.type()==fs::file_type::regular) {list->values.emplace_back(utf8(it->path().wstring()));list->active.push_back(true);}
            it.increment(ec);
        }
        if(ec)report(ec);
        return box(std::move(list));
    }
    if(op=="hashes"&&argc==1){
        const LPCWSTR algorithmNames[3]={BCRYPT_MD5_ALGORITHM,BCRYPT_SHA1_ALGORITHM,BCRYPT_SHA256_ALGORITHM};
        BCRYPT_ALG_HANDLE algorithms[3]{};BCRYPT_HASH_HANDLE hashes[3]{};DWORD hashLengths[3]{};
        std::vector<std::vector<UCHAR>> objects(3),digests(3);DWORD received=0;bool okay=true;
        for(int i=0;i<3&&okay;++i){
            okay=BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithms[i],algorithmNames[i],nullptr,0));DWORD objectLength=0;
            if(okay)okay=BCRYPT_SUCCESS(BCryptGetProperty(algorithms[i],BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectLength),sizeof(objectLength),&received,0))&&BCRYPT_SUCCESS(BCryptGetProperty(algorithms[i],BCRYPT_HASH_LENGTH,reinterpret_cast<PUCHAR>(&hashLengths[i]),sizeof(hashLengths[i]),&received,0));
            if(okay){objects[i].resize(objectLength);digests[i].resize(hashLengths[i]);okay=BCRYPT_SUCCESS(BCryptCreateHash(algorithms[i],&hashes[i],objects[i].data(),objectLength,nullptr,0,0));}
        }
        std::ifstream input(windowsApiPath(pathAt(0)),std::ios::binary);okay=okay&&static_cast<bool>(input);std::array<char,65536> chunk{};
        while(okay&&input){input.read(chunk.data(),static_cast<std::streamsize>(chunk.size()));const auto count=input.gcount();if(count>0)for(int i=0;i<3&&okay;++i)okay=BCRYPT_SUCCESS(BCryptHashData(hashes[i],reinterpret_cast<PUCHAR>(chunk.data()),static_cast<ULONG>(count),0));}
        okay=okay&&input.eof();for(int i=0;i<3;++i)if(okay)okay=BCRYPT_SUCCESS(BCryptFinishHash(hashes[i],digests[i].data(),hashLengths[i],0));
        for(int i=0;i<3;++i){if(hashes[i])BCryptDestroyHash(hashes[i]);if(algorithms[i])BCryptCloseAlgorithmProvider(algorithms[i],0);}
        auto result=std::make_shared<ListValue>();if(!okay)return box(std::move(result));
        for(const auto& digest:digests){std::ostringstream hex;hex<<std::hex<<std::setfill('0');for(UCHAR byte:digest)hex<<std::setw(2)<<static_cast<unsigned int>(byte);result->values.emplace_back(hex.str());result->active.push_back(true);}
        return box(std::move(result));
    }
    if((op=="sha256"||op=="hash_sha256"||op=="sha256_hesapla")&&argc==1){
        BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
        if(!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)))return nullptr;
        DWORD objectLength=0,hashLength=0,received=0;
        if(!BCRYPT_SUCCESS(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectLength),sizeof(objectLength),&received,0))||
           !BCRYPT_SUCCESS(BCryptGetProperty(algorithm,BCRYPT_HASH_LENGTH,reinterpret_cast<PUCHAR>(&hashLength),sizeof(hashLength),&received,0))){BCryptCloseAlgorithmProvider(algorithm,0);return nullptr;}
        std::vector<UCHAR> object(objectLength),digest(hashLength);
        if(!BCRYPT_SUCCESS(BCryptCreateHash(algorithm,&hash,object.data(),objectLength,nullptr,0,0))){BCryptCloseAlgorithmProvider(algorithm,0);return nullptr;}
        std::ifstream input(pathAt(0),std::ios::binary);std::array<char,65536> chunk{};bool okay=static_cast<bool>(input);
        while(okay&&input){input.read(chunk.data(),static_cast<std::streamsize>(chunk.size()));const auto count=input.gcount();if(count>0&&!BCRYPT_SUCCESS(BCryptHashData(hash,reinterpret_cast<PUCHAR>(chunk.data()),static_cast<ULONG>(count),0)))okay=false;}
        okay=okay&&input.eof()&&BCRYPT_SUCCESS(BCryptFinishHash(hash,digest.data(),hashLength,0));
        BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);if(!okay)return nullptr;
        std::ostringstream hex;hex<<std::hex<<std::setfill('0');for(UCHAR byte:digest)hex<<std::setw(2)<<static_cast<unsigned int>(byte);return box(hex.str());
    }
    if((op=="quarantine"||op=="karantinaya_al")&&argc==2){
        const fs::path source=pathAt(0),directory=pathAt(1);std::error_code ec;const auto status=fs::symlink_status(windowsApiPath(source),ec);
        if(ec||status.type()!=fs::file_type::regular)return nullptr;
        fs::create_directories(directory,ec);if(ec){report(ec);return nullptr;}
        const fs::path nativeSource=windowsApiPath(source);const DWORD attributes=GetFileAttributesW(nativeSource.c_str());if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return nullptr;
        const std::wstring base=source.filename().wstring()+L".prcyteras-quarantine";fs::path target=directory/base;
        for(unsigned int suffix=1;fs::exists(windowsApiPath(target),ec)&&suffix<100000;++suffix)target=directory/(base+L"."+std::to_wstring(suffix));
        const fs::path nativeTarget=windowsApiPath(target);if(ec||!MoveFileExW(nativeSource.c_str(),nativeTarget.c_str(),MOVEFILE_COPY_ALLOWED|MOVEFILE_WRITE_THROUGH))return nullptr;
        return box(utf8(target.wstring()));
    }
    if((op=="restore"||op=="karantinadan_geri_yukle")&&argc==2){
        const fs::path source=pathAt(0),target=pathAt(1);std::error_code ec;const auto status=fs::symlink_status(windowsApiPath(source),ec);
        if(ec||status.type()!=fs::file_type::regular||fs::exists(windowsApiPath(target),ec)||ec)return box(false);
        const fs::path nativeSource=windowsApiPath(source),nativeTarget=windowsApiPath(target);const DWORD attributes=GetFileAttributesW(nativeSource.c_str());if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))return box(false);
        return box(MoveFileExW(nativeSource.c_str(),nativeTarget.c_str(),MOVEFILE_COPY_ALLOWED|MOVEFILE_WRITE_THROUGH)!=0);
    }
    if((op=="current_directory"||op=="calisma_dizini")&&argc==0){std::error_code ec;const auto p=fs::current_path(ec);report(ec);return ec?nullptr:box(utf8(p.wstring()));}
    if((op=="absolute"||op=="tam_yol")&&argc==1){std::error_code ec;const auto p=fs::absolute(pathAt(0),ec);report(ec);return ec?nullptr:box(utf8(p.lexically_normal().wstring()));}
    if((op=="filename"||op=="dosya_adi")&&argc==1)return box(utf8(pathAt(0).filename().wstring()));
    if((op=="extension"||op=="uzanti")&&argc==1)return box(utf8(pathAt(0).extension().wstring()));
    if((op=="parent"||op=="ust_dizin")&&argc==1)return box(utf8(pathAt(0).parent_path().wstring()));
    return nullptr;
}
PTS_RUNTIME_API(pts_console_call) void* pts_console_call(const char* name,void** args,int argc) {
    const std::string op=name?name:"";
    if(op=="read_line"||op=="satir_oku"||op=="sor"){
        ensureConsoleInputUtf8();
        if(argc>0){writeStdout(argumentString(args,0));std::fflush(stdout);}
        std::string line;int ch=0;while((ch=std::fgetc(stdin))!=EOF&&ch!='\n')line.push_back(static_cast<char>(ch));if(ch==EOF&&line.empty())return nullptr;return box(std::move(line));
    }
    if(op=="write"||op=="yaz") {if(argc>0)writeStdout(argumentString(args,0));std::fflush(stdout);return box(true);}
    if(op=="print"||op=="yazdir") {if(argc>0)writeStdout(argumentString(args,0));writeStdout("\n");return box(true);}
    return nullptr;
}
PTS_RUNTIME_API(pts_math_call) void* pts_math_call(const char* name,void** args,int argc) {
    const std::string op = name ? name : "";
    const double a = argc > 0 ? numberOf(argument(args, 0)) : 0.0;
    const double b = argc > 1 ? numberOf(argument(args, 1)) : 0.0;
    if ((op == "pi" || op == "PI") && argc == 0) return box(3.141592653589793238462643383279502884);
    if ((op == "e" || op == "E") && argc == 0) return box(2.718281828459045235360287471352662498);
    if ((op == "tau" || op == "TAU") && argc == 0) return box(6.283185307179586476925286766559005768);
    if ((op == "random" || op == "rastgele") && argc == 0) return box(std::uniform_real_distribution<double>(0.0, 1.0)(randomEngineState()));
    if (argc == 1) {
        if (op == "abs" || op == "mutlak") {
            const Value value = argument(args, 0);
            if (const auto* i = std::get_if<std::int64_t>(&value)) return box(*i == INT64_MIN ? INT64_MAX : std::llabs(*i));
            if (const auto* u = std::get_if<std::uint64_t>(&value)) return box(*u);
            return box(std::fabs(a));
        }
        if (op == "sqrt" || op == "karekok") return box(std::sqrt(a));
        if (op == "sin") return box(std::sin(a));
        if (op == "cos") return box(std::cos(a));
        if (op == "tan") return box(std::tan(a));
        if (op == "asin") return box(std::asin(a));
        if (op == "acos") return box(std::acos(a));
        if (op == "atan") return box(std::atan(a));
        if (op == "exp") return box(std::exp(a));
        if (op == "log" || op == "ln") return box(std::log(a));
        if (op == "log10") return box(std::log10(a));
        if (op == "floor" || op == "taban") return box(std::floor(a));
        if (op == "ceil" || op == "tavan") return box(std::ceil(a));
        if (op == "round" || op == "yuvarla") return box(std::round(a));
        if (op == "radians" || op == "radyan") return box(a * (3.14159265358979323846 / 180.0));
        if (op == "degrees" || op == "derece") return box(a * (180.0 / 3.14159265358979323846));
    }
    if (argc == 2) {
        if (op == "pow" || op == "kuvvet") return box(std::pow(a, b));
        if (op == "atan2") return box(std::atan2(a, b));
        if (op == "min" || op == "minimum") return box(std::min(a, b));
        if (op == "max" || op == "maksimum") return box(std::max(a, b));
        if (op == "fmod" || op == "kalan") return box(b == 0.0 ? 0.0 : std::fmod(a, b));
        if (op == "random_range" || op == "rastgele_aralik") return box(std::uniform_real_distribution<double>(a, b)(randomEngineState()));
    }
    if (argc == 3) {
        const double c = numberOf(argument(args, 2));
        if (op == "clamp" || op == "sinirla") return box(std::clamp(a, b, c));
        if (op == "lerp" || op == "dogrusal_aradeger") return box(a + (b - a) * c);
    }
    writeStderr("PRCYTERAS: Math.");writeStderr(op);writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");
    return nullptr;
}
PTS_RUNTIME_API(pts_system_call) void* pts_system_call(const char* name,void** args,int argc) {
    const std::string op = name ? name : "";
    if ((op == "processes" || op == "surecler") && argc == 0) {
        auto list=std::make_shared<ListValue>();HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        if(snapshot==INVALID_HANDLE_VALUE)return box(std::move(list));
        PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);
        if(Process32FirstW(snapshot,&entry))do{
            auto object=std::make_shared<Object>();object->type="ProcessInfo";
            object->dynamicFields["pid"]=static_cast<std::uint64_t>(entry.th32ProcessID);
            object->dynamicFields["name"]=utf8(entry.szExeFile);
            std::wstring imagePath;HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
            if(process){std::vector<wchar_t> buffer(32768);DWORD length=static_cast<DWORD>(buffer.size());if(QueryFullProcessImageNameW(process,0,buffer.data(),&length))imagePath.assign(buffer.data(),length);CloseHandle(process);}
            object->dynamicFields["path"]=utf8(imagePath);list->values.emplace_back(std::move(object));list->active.push_back(true);
        }while(Process32NextW(snapshot,&entry));
        CloseHandle(snapshot);return box(std::move(list));
    }
    if ((op == "terminate_process" || op == "surec_sonlandir") && argc == 1) {
        const double numeric=numberOf(argument(args,0));if(!std::isfinite(numeric)||numeric<5.0||numeric>MAXDWORD||std::floor(numeric)!=numeric)return box(false);
        const std::uint64_t raw=static_cast<std::uint64_t>(numeric);if(raw==GetCurrentProcessId())return box(false);
        HANDLE process=OpenProcess(PROCESS_TERMINATE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,static_cast<DWORD>(raw));
        if(!process)return box(false);
        const bool stopped=TerminateProcess(process,1)!=0;
        if(stopped)WaitForSingleObject(process,2000);CloseHandle(process);return box(stopped);
    }
    if ((op == "startup_entries" || op == "baslangic_girdileri") && argc == 0) {
        auto list=std::make_shared<ListValue>();
        struct Spec{const wchar_t* scope;HKEY root;const wchar_t* path;REGSAM view;};
        const Spec specs[]={{L"HKCU_RUN",HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0},
            {L"HKCU_RUNONCE",HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",0},
            {L"HKLM_RUN",HKEY_LOCAL_MACHINE,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",KEY_WOW64_64KEY},
            {L"HKLM_RUNONCE",HKEY_LOCAL_MACHINE,L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",KEY_WOW64_64KEY},
            {L"HKLM_RUN32",HKEY_LOCAL_MACHINE,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",KEY_WOW64_32KEY},
            {L"HKLM_RUNONCE32",HKEY_LOCAL_MACHINE,L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",KEY_WOW64_32KEY}};
        for(const auto& spec:specs){HKEY key=nullptr;if(RegOpenKeyExW(spec.root,spec.path,0,KEY_READ|spec.view,&key)!=ERROR_SUCCESS)continue;
            DWORD values=0,maxName=0,maxData=0;RegQueryInfoKeyW(key,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&values,&maxName,&maxData,nullptr,nullptr);
            std::vector<wchar_t> valueName(static_cast<std::size_t>(maxName)+2);std::vector<BYTE> data(static_cast<std::size_t>(maxData)+sizeof(wchar_t));
            for(DWORD i=0;i<values;++i){DWORD nameLength=static_cast<DWORD>(valueName.size()-1),dataLength=static_cast<DWORD>(data.size()),type=0;
                if(RegEnumValueW(key,i,valueName.data(),&nameLength,nullptr,&type,data.data(),&dataLength)!=ERROR_SUCCESS)continue;
                std::wstring command;if((type==REG_SZ||type==REG_EXPAND_SZ)&&dataLength>=sizeof(wchar_t)){const auto* chars=reinterpret_cast<const wchar_t*>(data.data());std::size_t n=dataLength/sizeof(wchar_t);while(n&&chars[n-1]==L'\0')--n;command.assign(chars,n);}
                auto object=std::make_shared<Object>();object->type="StartupEntry";object->dynamicFields["scope"]=utf8(spec.scope);object->dynamicFields["name"]=utf8(std::wstring(valueName.data(),nameLength));object->dynamicFields["command"]=utf8(command);list->values.emplace_back(std::move(object));list->active.push_back(true);
            }RegCloseKey(key);
        }return box(std::move(list));
    }
    if ((op == "remove_startup_entry" || op == "baslangic_girdisini_sil") && argc == 2) {
        const std::string scope=argumentString(args,0);HKEY root=nullptr;const wchar_t* path=nullptr;REGSAM view=0;
        if(scope=="HKCU_RUN"){root=HKEY_CURRENT_USER;path=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";}
        else if(scope=="HKCU_RUNONCE"){root=HKEY_CURRENT_USER;path=L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";}
        else if(scope=="HKLM_RUN"){root=HKEY_LOCAL_MACHINE;path=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";view=KEY_WOW64_64KEY;}
        else if(scope=="HKLM_RUNONCE"){root=HKEY_LOCAL_MACHINE;path=L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";view=KEY_WOW64_64KEY;}
        else if(scope=="HKLM_RUN32"){root=HKEY_LOCAL_MACHINE;path=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";view=KEY_WOW64_32KEY;}
        else if(scope=="HKLM_RUNONCE32"){root=HKEY_LOCAL_MACHINE;path=L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce";view=KEY_WOW64_32KEY;}
        if(!root)return box(false);HKEY key=nullptr;if(RegOpenKeyExW(root,path,0,KEY_SET_VALUE|view,&key)!=ERROR_SUCCESS)return box(false);
        const std::wstring valueName=wide(argumentString(args,1));const LSTATUS result=RegDeleteValueW(key,valueName.c_str());RegCloseKey(key);return box(result==ERROR_SUCCESS);
    }
    if ((op == "services" || op == "servisler") && argc == 0) {
        auto list=std::make_shared<ListValue>();SC_HANDLE manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_ENUMERATE_SERVICE);if(!manager)return box(std::move(list));
        DWORD needed=0,count=0,resume=0;EnumServicesStatusExW(manager,SC_ENUM_PROCESS_INFO,SERVICE_WIN32,SERVICE_STATE_ALL,nullptr,0,&needed,&count,&resume,nullptr);
        if(GetLastError()==ERROR_MORE_DATA&&needed){std::vector<BYTE> buffer(static_cast<std::size_t>(needed)+4096);resume=0;
            do{needed=0;count=0;const BOOL ok=EnumServicesStatusExW(manager,SC_ENUM_PROCESS_INFO,SERVICE_WIN32,SERVICE_STATE_ALL,buffer.data(),static_cast<DWORD>(buffer.size()),&needed,&count,&resume,nullptr);
                auto* entries=reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
                for(DWORD i=0;i<count;++i){auto object=std::make_shared<Object>();object->type="ServiceInfo";object->dynamicFields["name"]=utf8(entries[i].lpServiceName?entries[i].lpServiceName:L"");object->dynamicFields["display_name"]=utf8(entries[i].lpDisplayName?entries[i].lpDisplayName:L"");const DWORD state=entries[i].ServiceStatusProcess.dwCurrentState;object->dynamicFields["state"]=static_cast<std::uint64_t>(state);const char* stateName=state==SERVICE_STOPPED?"STOPPED":state==SERVICE_START_PENDING?"START_PENDING":state==SERVICE_STOP_PENDING?"STOP_PENDING":state==SERVICE_RUNNING?"RUNNING":state==SERVICE_CONTINUE_PENDING?"CONTINUE_PENDING":state==SERVICE_PAUSE_PENDING?"PAUSE_PENDING":state==SERVICE_PAUSED?"PAUSED":"UNKNOWN";object->dynamicFields["state_name"]=std::string(stateName);list->values.emplace_back(std::move(object));list->active.push_back(true);}
                if(ok)break;if(GetLastError()!=ERROR_MORE_DATA||!needed)break;buffer.resize(static_cast<std::size_t>(needed)+4096);
            }while(resume!=0);
        }CloseServiceHandle(manager);return box(std::move(list));
    }
    if ((op == "stop_service" || op == "servisi_durdur" || op == "remove_service" || op == "servisi_kaldir") && argc == 1) {
        const bool remove=(op=="remove_service"||op=="servisi_kaldir");SC_HANDLE manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);if(!manager)return box(false);
        const std::wstring serviceName=wide(argumentString(args,0));SC_HANDLE service=OpenServiceW(manager,serviceName.c_str(),SERVICE_QUERY_STATUS|SERVICE_STOP|(remove?DELETE:0));if(!service){CloseServiceHandle(manager);return box(false);}
        SERVICE_STATUS_PROCESS status{};DWORD bytes=0;bool okay=QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,reinterpret_cast<LPBYTE>(&status),sizeof(status),&bytes)!=0;
        if(okay&&status.dwCurrentState!=SERVICE_STOPPED){SERVICE_STATUS ignored{};ControlService(service,SERVICE_CONTROL_STOP,&ignored);
            const ULONGLONG deadline=GetTickCount64()+5000;do{Sleep(100);okay=QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,reinterpret_cast<LPBYTE>(&status),sizeof(status),&bytes)!=0;}while(okay&&status.dwCurrentState!=SERVICE_STOPPED&&GetTickCount64()<deadline);
            okay=okay&&status.dwCurrentState==SERVICE_STOPPED;
        }
        if(okay&&remove)okay=DeleteService(service)!=0;CloseServiceHandle(service);CloseServiceHandle(manager);return box(okay);
    }
    if ((op == "args" || op == "arguments" || op == "argumanlar") && argc == 0) {
        int count = 0;
        LPWSTR* values = CommandLineToArgvW(GetCommandLineW(), &count);
        auto list = std::make_shared<ListValue>();
        if (values) {
            for (int i = 1; i < count; ++i) { list->values.emplace_back(utf8(values[i])); list->active.push_back(true); }
            LocalFree(values);
        }
        return box(std::move(list));
    }
    if ((op == "cwd" || op == "current_directory" || op == "calisma_dizini") && argc == 0) {
        std::error_code ec;
        const fs::path path = fs::current_path(ec);
        return ec ? nullptr : box(utf8(path.wstring()));
    }
    if ((op == "executable_path" || op == "program_yolu") && argc == 0) {
        std::wstring path(32768, L'\0');
        const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (size == 0 || size >= path.size()) return nullptr;
        path.resize(size);
        return box(utf8(path));
    }
    if ((op == "cpu_count" || op == "islemci_sayisi") && argc == 0)
        return box(static_cast<std::uint64_t>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
    if ((op == "unix_time" || op == "epoch_seconds" || op == "unix_zamani") && argc == 0) {
        const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return box(static_cast<std::int64_t>(seconds));
    }
    if ((op == "sleep_ms" || op == "bekle_ms") && argc == 1) {
        const double ms = numberOf(argument(args, 0));
        if (ms > 0.0) Sleep(static_cast<DWORD>(std::min(ms, static_cast<double>(MAXDWORD))));
        return nullptr;
    }
    if ((op == "chdir" || op == "set_current_directory" || op == "calisma_dizinini_ayarla") && argc == 1)
        return box(SetCurrentDirectoryW(wide(argumentString(args, 0)).c_str()) != 0);
    if ((op == "env" || op == "get_env" || op == "ortam") && argc == 1) {
        const std::wstring key = wide(argumentString(args, 0));
        const DWORD needed = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
        if (needed == 0) return nullptr;
        std::wstring value(needed, L'\0');
        const DWORD written = GetEnvironmentVariableW(key.c_str(), value.data(), needed);
        if (written == 0 || written >= needed) return nullptr;
        value.resize(written);
        return box(utf8(value));
    }
    if ((op == "set_env" || op == "set_environment" || op == "ortam_ayarla") && argc == 2) {
        const std::wstring key = wide(argumentString(args, 0));
        const std::wstring value = wide(argumentString(args, 1));
        return box(SetEnvironmentVariableW(key.c_str(), value.c_str()) != 0);
    }
    writeStderr("PRCYTERAS: System.");writeStderr(op);writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");
    return nullptr;
}
PTS_RUNTIME_API(pts_string_call) void* pts_string_call(const char* name,void** args,int argc) {
    const std::string op=name?name:"";const std::string source=argc>0?argumentString(args,0):std::string();
    auto boundaries=[](const std::string& text){std::vector<std::size_t> result{0};for(std::size_t i=0;i<text.size();){const auto c=static_cast<unsigned char>(text[i]);std::size_t step=(c&0x80)==0?1:(c&0xE0)==0xC0?2:(c&0xF0)==0xE0?3:(c&0xF8)==0xF0?4:1;i=std::min(text.size(),i+step);result.push_back(i);}return result;};
    if((op=="length"||op=="size"||op=="uzunluk")&&argc==1){std::int64_t count=0;for(unsigned char c:source)if((c&0xC0)!=0x80)++count;return box(count);}
    if((op=="is_empty"||op=="empty"||op=="bos_mu")&&argc==1)return box(source.empty());
    if((op=="contains"||op=="icerir")&&argc==2)return box(source.find(argumentString(args,1))!=std::string::npos);
    if((op=="starts_with"||op=="baslar_mi")&&argc==2){const std::string part=argumentString(args,1);return box(source.size()>=part.size()&&source.compare(0,part.size(),part)==0);}
    if((op=="ends_with"||op=="biter_mi")&&argc==2){const std::string part=argumentString(args,1);return box(source.size()>=part.size()&&source.compare(source.size()-part.size(),part.size(),part)==0);}
    if((op=="substring"||op=="substr"||op=="alt_metin")&&(argc==2||argc==3)){
        const auto points=boundaries(source);const auto total=static_cast<std::int64_t>(points.size()-1);std::int64_t start=static_cast<std::int64_t>(numberOf(argument(args,1)));if(start<0)start=std::max<std::int64_t>(0,total+start);start=std::clamp<std::int64_t>(start,0,total);
        std::int64_t end=total;if(argc==3){const auto count=static_cast<std::int64_t>(numberOf(argument(args,2)));end=std::clamp<std::int64_t>(start+std::max<std::int64_t>(0,count),start,total);}
        return box(source.substr(points[static_cast<std::size_t>(start)],points[static_cast<std::size_t>(end)]-points[static_cast<std::size_t>(start)]));
    }
    if((op=="replace"||op=="degistir")&&argc==3){const std::string from=argumentString(args,1),to=argumentString(args,2);if(from.empty())return box(source);std::string result;std::size_t begin=0,at=0;while((at=source.find(from,begin))!=std::string::npos){result.append(source,begin,at-begin);result+=to;begin=at+from.size();}result.append(source,begin,std::string::npos);return box(std::move(result));}
    if((op=="trim"||op=="kirp")&&argc==1){std::size_t first=0,last=source.size();while(first<last&&std::isspace(static_cast<unsigned char>(source[first])))++first;while(last>first&&std::isspace(static_cast<unsigned char>(source[last-1])))--last;return box(source.substr(first,last-first));}
    if((op=="lower"||op=="lowercase"||op=="kucuk_harf")&&argc==1){std::wstring text=wide(source);if(!text.empty())CharLowerBuffW(text.data(),static_cast<DWORD>(text.size()));return box(utf8(text));}
    if((op=="upper"||op=="uppercase"||op=="buyuk_harf")&&argc==1){std::wstring text=wide(source);if(!text.empty())CharUpperBuffW(text.data(),static_cast<DWORD>(text.size()));return box(utf8(text));}
    if((op=="to_int"||op=="parse_int"||op=="tamsayi")&&argc==1){try{std::size_t used=0;const auto value=std::stoll(source,&used,10);if(used!=source.size())return nullptr;return box(static_cast<std::int64_t>(value));}catch(...){return nullptr;}}
    if((op=="to_float"||op=="parse_float"||op=="ondalik")&&argc==1){try{std::size_t used=0;const double value=std::stod(source,&used);if(used!=source.size()||!std::isfinite(value))return nullptr;return box(value);}catch(...){return nullptr;}}
    if((op=="split"||op=="bol")&&argc==2){const std::string separator=argumentString(args,1);auto list=std::make_shared<ListValue>();auto add=[&](std::string part){list->values.emplace_back(std::move(part));list->active.push_back(true);};
        if(separator.empty()){const auto points=boundaries(source);for(std::size_t i=1;i<points.size();++i)add(source.substr(points[i-1],points[i]-points[i-1]));}
        else {std::size_t begin=0,at=0;while((at=source.find(separator,begin))!=std::string::npos){add(source.substr(begin,at-begin));begin=at+separator.size();}add(source.substr(begin));}
        return box(std::move(list));
    }
    if((op=="join"||op=="birlestir")&&argc==2){const std::string separator=argumentString(args,1);const Value v=argument(args,0);const auto* list=std::get_if<std::shared_ptr<ListValue>>(&v);if(!list)return nullptr;std::string result;bool first=true;for(std::size_t i=0;i<(*list)->values.size();++i){if(i<(*list)->active.size()&&!(*list)->active[i])continue;if(!first)result+=separator;first=false;result+=stringOf((*list)->values[i]);}return box(std::move(result));}
    writeStderr("PRCYTERAS: String.");writeStderr(op);writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");return nullptr;
}
PTS_RUNTIME_API(pts_iter_begin) void* pts_iter_begin(void* handle) { auto v=valueOf(handle);if(auto list=std::get_if<std::shared_ptr<ListValue>>(&v))return new Iterator{*list,0};return nullptr; }
PTS_RUNTIME_API(pts_iter_next) void* pts_iter_next(void* iteratorHandle) { auto* it=static_cast<Iterator*>(iteratorHandle);if(!it)return nullptr;while(it->index<it->list->values.size()){auto i=it->index++;if(it->list->active[i])return box(it->list->values[i]);}return nullptr; }
PTS_RUNTIME_API(pts_register_function) void pts_register_function(const char* name,void* function) { functionsState()[name]=reinterpret_cast<Function>(function); }
PTS_RUNTIME_API(pts_register_class) void pts_register_class(const char* name) { classesState().try_emplace(name); }
PTS_RUNTIME_API(pts_class_field) void pts_class_field(const char* type,const char* name,void* value,int bits,bool isUnsigned,int nativeKind) { auto& info=classesState()[type];const std::string fieldName=name?name:"";auto found=info.fieldIndices.find(fieldName);std::size_t index=0;if(found==info.fieldIndices.end()){index=info.fieldOrder.size();info.fieldIndices.emplace(fieldName,index);info.fieldOrder.push_back(fieldName);info.defaults.emplace_back();info.integerFieldTypes.emplace_back();info.nativeFieldKinds.emplace_back(0);}else index=found->second;if(bits>=4&&bits<=64){const IntegerFieldType fieldType{bits,isUnsigned};info.integerFields[fieldName]=fieldType;info.integerFieldTypes[index]=fieldType;}info.nativeFieldKinds[index]=nativeKind;info.defaults[index]=normalizeIntegerField(info,fieldName,valueOf(value)); }
PTS_RUNTIME_API(pts_register_method) void pts_register_method(const char* type,const char* name,void* function) { classesState()[type].methods[name]=reinterpret_cast<Function>(function); }
PTS_RUNTIME_API(pts_make) void* pts_make(const char* type,void** args,int argc) { auto object=std::make_shared<Object>();object->type=type?type:"";auto it=classesState().find(object->type);if(it!=classesState().end()){object->fields=it->second.defaults;object->nativeFields.resize(object->fields.size());for(int i=0;i<argc&&i<static_cast<int>(it->second.fieldOrder.size());++i){const std::size_t index=static_cast<std::size_t>(i);object->fields[index]=normalizeIntegerField(it->second,it->second.fieldOrder[index],argument(args,i));}for(std::size_t i=0;i<object->fields.size();++i)syncNativeField(*object,i,object->fields[i]);auto init=it->second.methods.find("init");if(init!=it->second.methods.end()){auto* env=new Environment();env->self=object;env->parent=&globalEnvironmentState();init->second(env,nullptr,0);}}return box(object); }
PTS_RUNTIME_API(pts_native_data) void* pts_native_data(void* objectHandle) { const Value value=valueOf(objectHandle);const auto* object=std::get_if<std::shared_ptr<Object>>(&value);return object&&*object&&!(*object)->nativeFields.empty()?static_cast<void*>((*object)->nativeFields.data()):nullptr; }
PTS_RUNTIME_API(pts_call_global) void* pts_call_global(const char* name,void* env,void** args,int argc) { return callFunction(name?name:"",env,args,argc); }
PTS_RUNTIME_API(pts_call_method) void* pts_call_method(void* receiver,const char* name,void** args,int argc) { return callMethod(valueOf(receiver),name?name:"",args,argc); }
PTS_RUNTIME_API(pts_engine_call) void* pts_engine_call(const char* name,void** args,int argc) { return engineCall(name?name:"",args,argc); }
PTS_RUNTIME_API(pts_engine3d_call) void* pts_engine3d_call(const char* name,void** args,int argc) { return engine3DCall(name?name:"",args,argc); }
PTS_RUNTIME_API(pts_ui_call) void* pts_ui_call(const char* name,void** args,int argc) { if(name&&std::string(name)=="draw_text")return engineCall("draw_text",args,argc);return nullptr; }
PTS_RUNTIME_API(pts_random_call) void* pts_random_call(const char* name,void** args,int argc) { std::string n=name?name:"";if(n=="int_range")return box(std::uniform_int_distribution<int>(argumentInt(args,0),argumentInt(args,1))(randomEngineState()));if(n=="chance")return box(std::bernoulli_distribution(numberOf(argument(args,0)))(randomEngineState()));return nullptr; }
PTS_RUNTIME_API(pts_arena_call) void* pts_arena_call(const char* name,void** args,int argc) {
    std::lock_guard<std::mutex> arenaGuard(arenaMutexState());
    const std::string op=name?name:"";
    auto numericAt=[&](int index,long double& value)->bool {
        const Value input=argument(args,index);
        if(const auto* p=std::get_if<std::int64_t>(&input)){value=static_cast<long double>(*p);return true;}
        if(const auto* p=std::get_if<std::uint64_t>(&input)){value=static_cast<long double>(*p);return true;}
        if(const auto* p=std::get_if<double>(&input)){if(!std::isfinite(*p))return false;value=static_cast<long double>(*p);return true;}
        return false;
    };
    auto unsignedAt=[&](int index,std::uint64_t& value)->bool {
        const Value input=argument(args,index);
        if(const auto* p=std::get_if<std::uint64_t>(&input)){value=*p;return true;}
        if(const auto* p=std::get_if<std::int64_t>(&input)){if(*p<0)return false;value=static_cast<std::uint64_t>(*p);return true;}
        if(const auto* p=std::get_if<double>(&input)){if(!std::isfinite(*p)||*p<0||std::trunc(*p)!=*p||*p>=std::ldexp(1.0,64))return false;value=static_cast<std::uint64_t>(*p);return true;}
        return false;
    };
    auto arenaAt=[&](int index)->ArenaValue* {
        std::uint64_t id=0;
        if(!unsignedAt(index,id)||id==0||id>=arenasState().size())return nullptr;
        return arenasState()[static_cast<std::size_t>(id)].get();
    };
    if(op=="create"&&argc==1) {
        std::uint64_t capacity=0;
        if(!unsignedAt(0,capacity)||capacity==0||capacity>512ull*1024ull*1024ull||arenasState().size()>=100000)return box(std::int64_t{-1});
        try {
            if(arenasState().empty())arenasState().emplace_back(nullptr);
            auto arena=std::make_unique<ArenaValue>();
            arena->capacity=static_cast<std::size_t>(capacity);
            arena->storage.resize(arena->capacity+ArenaValue::maxAlignment-1);
            const auto address=reinterpret_cast<std::uintptr_t>(arena->storage.data());
            arena->baseOffset=static_cast<std::size_t>((ArenaValue::maxAlignment-(address&(ArenaValue::maxAlignment-1)))&(ArenaValue::maxAlignment-1));
            arenasState().push_back(std::move(arena));
            return box(static_cast<std::uint64_t>(arenasState().size()-1));
        } catch(const std::bad_alloc&) { return box(std::int64_t{-1}); }
    }
    if(op=="destroy"&&argc==1) {
        std::uint64_t id=0;
        if(!unsignedAt(0,id)||id==0||id>=arenasState().size()||!arenasState()[static_cast<std::size_t>(id)])return box(false);
        arenasState()[static_cast<std::size_t>(id)].reset();return box(true);
    }
    ArenaValue* arena=argc>0?arenaAt(0):nullptr;
    if(op=="reset"&&argc==1&&arena){arena->used=0;return box(true);}
    if(op=="used"&&argc==1&&arena)return box(static_cast<std::uint64_t>(arena->used));
    if(op=="capacity"&&argc==1&&arena)return box(static_cast<std::uint64_t>(arena->capacity));
    if(op=="alloc"&&(argc==2||argc==3)&&arena) {
        std::uint64_t size=0,alignment=1;
        if(!unsignedAt(1,size)||(argc==3&&!unsignedAt(2,alignment))||alignment==0||alignment>4096||(alignment&(alignment-1))!=0)return box(std::int64_t{-1});
        if(arena->used>std::numeric_limits<std::size_t>::max()-(static_cast<std::size_t>(alignment)-1))return box(std::int64_t{-1});
        const std::size_t start=(arena->used+static_cast<std::size_t>(alignment)-1)&~(static_cast<std::size_t>(alignment)-1);
        if(start>arena->capacity||size>arena->capacity-start)return box(std::int64_t{-1});
        const std::size_t count=static_cast<std::size_t>(size);
        if(count==0)return box(static_cast<std::uint64_t>(start));
        std::fill(arena->data()+start,arena->data()+start+count,std::uint8_t{0});
        arena->used=start+count;
        return box(static_cast<std::uint64_t>(start));
    }
    if((op=="read_bytes"&&argc==3)&&arena) {
        std::uint64_t offset=0,count=0;
        if(!unsignedAt(1,offset)||!unsignedAt(2,count)||offset>arena->used||count>arena->used-offset)return nullptr;
        return box(std::string(reinterpret_cast<const char*>(arena->data()+static_cast<std::size_t>(offset)),static_cast<std::size_t>(count)));
    }
    if(op=="write_bytes"&&argc==3&&arena) {
        std::uint64_t offset=0;
        const std::string bytes=argumentString(args,2);
        if(!unsignedAt(1,offset)||offset>arena->used||bytes.size()>arena->used-offset)return box(false);
        if(!bytes.empty())std::memcpy(arena->data()+static_cast<std::size_t>(offset),bytes.data(),bytes.size());
        return box(true);
    }
    const bool writing=op.rfind("write_",0)==0;
    const bool reading=op.rfind("read_",0)==0;
    if((writing||reading)&&arena&&(argc==(writing?3:2))) {
        const std::string type=op.substr(writing?6:5);
        std::size_t width=0;bool signedInteger=false,isFloat=false;
        if(type=="i8"||type=="u8"){width=1;signedInteger=type[0]=='i';}
        else if(type=="i16"||type=="u16"){width=2;signedInteger=type[0]=='i';}
        else if(type=="i32"||type=="u32"){width=4;signedInteger=type[0]=='i';}
        else if(type=="i64"||type=="u64"){width=8;signedInteger=type[0]=='i';}
        else if(type=="f32"){width=4;isFloat=true;}
        else if(type=="f64"){width=8;isFloat=true;}
        else return writing?box(false):nullptr;
        std::uint64_t offset=0;
        if(!unsignedAt(1,offset)||offset>arena->used||width>arena->used-offset)return writing?box(false):nullptr;
        auto* destination=arena->data()+static_cast<std::size_t>(offset);
        if(reading) {
            if(isFloat&&width==4){float value=0;std::memcpy(&value,destination,sizeof(value));return box(static_cast<double>(value));}
            if(isFloat){double value=0;std::memcpy(&value,destination,sizeof(value));return box(value);}
            std::uint64_t raw=0;std::memcpy(&raw,destination,width);
            if(signedInteger){
                if(width<8){const unsigned bits=static_cast<unsigned>(width*8);const std::uint64_t sign=std::uint64_t{1}<<(bits-1);if(raw&sign)raw|=(~std::uint64_t{0})<<bits;}
                std::int64_t value=0;std::memcpy(&value,&raw,sizeof(value));return box(value);
            }
            return box(raw);
        }
        if(isFloat) {
            long double value=0;
            if(!numericAt(2,value))return box(false);
            if(width==4){const float narrowed=static_cast<float>(value);if(!std::isfinite(narrowed))return box(false);std::memcpy(destination,&narrowed,sizeof(narrowed));}
            else {const double narrowed=static_cast<double>(value);if(!std::isfinite(narrowed))return box(false);std::memcpy(destination,&narrowed,sizeof(narrowed));}
            return box(true);
        }
        const unsigned bits=static_cast<unsigned>(width*8);
        std::uint64_t raw=0;
        if(signedInteger) {
            const Value input=argument(args,2);std::int64_t value=0;
            if(const auto* p=std::get_if<std::int64_t>(&input))value=*p;
            else if(const auto* p=std::get_if<std::uint64_t>(&input)){if(*p>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))return box(false);value=static_cast<std::int64_t>(*p);}
            else if(const auto* p=std::get_if<double>(&input)){if(!std::isfinite(*p)||std::trunc(*p)!=*p||*p< -std::ldexp(1.0,63)||*p>=std::ldexp(1.0,63))return box(false);value=static_cast<std::int64_t>(*p);}
            else return box(false);
            if(bits<64){const std::int64_t minimum=-(std::int64_t{1}<<(bits-1));const std::int64_t maximum=(std::int64_t{1}<<(bits-1))-1;if(value<minimum||value>maximum)return box(false);}
            raw=static_cast<std::uint64_t>(value);
        } else {
            const Value input=argument(args,2);std::uint64_t value=0;
            if(const auto* p=std::get_if<std::uint64_t>(&input))value=*p;
            else if(const auto* p=std::get_if<std::int64_t>(&input)){if(*p<0)return box(false);value=static_cast<std::uint64_t>(*p);}
            else if(const auto* p=std::get_if<double>(&input)){if(!std::isfinite(*p)||std::trunc(*p)!=*p||*p<0||*p>=std::ldexp(1.0,64))return box(false);value=static_cast<std::uint64_t>(*p);}
            else return box(false);
            if(bits<64&&value>((std::uint64_t{1}<<bits)-1))return box(false);
            raw=value;
        }
        std::memcpy(destination,&raw,width);return box(true);
    }
    if((op=="used"||op=="capacity"||op=="reset"||op=="alloc"||op=="read_bytes"||op=="write_bytes"||writing||reading))return writing?box(false):nullptr;
    writeStderr("PRCYTERAS: Arena.");writeStderr(op);writeStderr(" icin parametre sayisi veya islem adi gecersiz.\n");
    return nullptr;
}
PTS_RUNTIME_API(pts_stdlib_call) void* pts_stdlib_call(const char* module,const char* op,void** args,int argc) { return stdlibCall(module?module:"",op?op:"",args,argc); }
PTS_RUNTIME_API(pts_call_namespace) void* pts_call_namespace(const char* name,void* env,void** args,int argc) { return callFunction(name?name:"",env,args,argc); }
}
