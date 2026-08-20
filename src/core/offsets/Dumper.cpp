#include "Dumper.hpp"

#include "core/engine/Engine.hpp"

bool Dumper::Init() {
    return GetInstance().InitImpl();
}

bool Dumper::InitImpl() {
    auto process = Engine::GetProcess();
    auto client = Engine::GetClient();
    auto engine = Engine::GetEngine();

    DWORD64 temp = 0;

    // client.dll

    // View Matrix
    if (!(temp = Scan(offsets::signatures::viewMatrix, client))) {
        LOGF(FATAL, "未找到关键偏移");
        return false;
    }

    offsets::viewMatrix = temp - client.base;
    LOGF(VERBOSE, "找到关键偏移 0x{:X}", offsets::viewMatrix);

    // Global Variables
    if (!(temp = Scan(offsets::signatures::globalVars, client))) {
        LOGF(FATAL, "未找到关键偏移");
        return false;
    }

    offsets::globalVars = temp - client.base;
    LOGF(VERBOSE, "找到关键偏移 0x{:X}", offsets::globalVars);

    // Entity List
    if (!(temp = Scan(offsets::signatures::entityList, client))) {
        LOGF(FATAL, "未找到关键偏移");
        return false;
    }

    offsets::entityList = temp - client.base;
    LOGF(VERBOSE, "找到关键偏移 0x{:X}", offsets::entityList);

    // Local Player Controller
    if (!(temp = Scan(offsets::signatures::localPlayerController, client))) {
        LOGF(FATAL, "未找到关键偏移");
        return false;
    }

    offsets::localPlayerController = temp - client.base;
    LOGF(VERBOSE, "找到关键偏移 0x{:X}", offsets::localPlayerController);

    // C4
    if (!(temp = Scan(offsets::signatures::plantedC4, client))) {
        LOGF(FATAL, "未找到关键偏移");
        return false;
    }

    offsets::plantedC4 = temp - client.base;
    LOGF(VERBOSE, "找到关键偏移 0x{:X}", offsets::plantedC4);

    // C4 carrier pointer
    if (!(temp = Scan(offsets::signatures::weaponC4, client))) {
        LOGF(FATAL, "未找到关键偏移");
        return false;
    }

    offsets::weaponC4 = temp - client.base;
    LOGF(VERBOSE, "找到关键偏移 0x{:X}", offsets::weaponC4);

    // engine2.dll

    // Build Number
    if (!(temp = Scan(offsets::signatures::buildNumber, engine))) {
        LOGF(FATAL, "未找到关键偏移");
        return false;
    }

    offsets::buildNumber = temp - engine.base;
    LOGF(VERBOSE, "找到关键偏移 0x{:X}", offsets::buildNumber);

    LOGF(INFO, "偏移转储成功...");

    return true;
}

DWORD64 Dumper::Scan(const std::string sig, ProcessModule module) {
    auto process = Engine::GetProcess();

    if (!process)
        return 0;

    DWORD offsets = 0;
    DWORD64 address = 0;
    std::vector<DWORD64> list;

    list = ScanMemory(sig, module.base, module.base + 0x4000000);

    if (!list.size())
        return 0;

    if (!process->read_raw(list.at(0) + 3, &offsets, sizeof(DWORD)))
        return 0;

    address = list.at(0) + offsets + 7;
    return address;
}

std::vector<WORD> Dumper::StrSigToArray(const std::string& sig) {
    std::istringstream iss(sig);
    std::vector<WORD> bytes;
    std::string byte_str;

    while (iss >> byte_str) {
        if (byte_str == "??" || byte_str == "?")
            bytes.push_back(256);
        else
            bytes.push_back(static_cast<WORD>(std::stoul(byte_str, nullptr, 16)));
    }
    return bytes;
}

void Dumper::GetNextArray(std::vector<short>& next, const std::vector<WORD>& signature)
{
    const size_t size = signature.size();
    for (size_t i = 0; i < size; i++)
        next[signature[i]] = (short)i;
}

void Dumper::ScanBlock(byte* buffer, const std::vector<short>& next, const std::vector<WORD>& signature, DWORD64 start, DWORD size, std::vector<DWORD64>& result)
{
    auto process = Engine::GetProcess();

    if (!process->read_raw(start, buffer, size))
        return;

    const int length = static_cast<int>(signature.size());
    const int sz = static_cast<int>(size);

    for (int i = 0, j, k; i < sz;)
    {
        j = i; k = 0;

        for (; k < length && j < sz && (signature[k] == buffer[j] || signature[k] == 256); k++, j++);

        if (k == length)
            result.push_back(start + i);

        if ((i + length) >= sz)
            return;

        int Num = next[buffer[i + length]];
        if (Num == -1)
            i += (length - next[256]);
        else
            i += (length - Num);
    }
}

std::vector<DWORD64> Dumper::ScanMemory(const std::string& sig, DWORD64 start, DWORD64 end, int number)
{
    std::vector<DWORD64> result;
    std::vector<short> next(260, -1);

    auto process = Engine::GetProcess();

    if (!process)
        return result;

    auto signature = StrSigToArray(sig);
    if (!signature.size())
        return result;

    GetNextArray(next, signature);

    std::vector<byte> buffer(MAX_BLOCK_SIZE);

    // 内核模式: 按固定块扫描整个 [start, end) 范围。read_raw 已路由到内核驱动，
    // 未映射/不可读的块会在 ScanBlock 里因 read_raw 返回 false 被跳过。
    // 相邻块重叠 (signature.size()-1) 字节，避免签名恰好跨越块边界而漏检。
    const DWORD64 overlap = signature.size() > 0 ? (DWORD64)(signature.size() - 1) : 0;
    for (DWORD64 cur = start; cur < end && (int)result.size() < number; ) {
        const DWORD64 remaining = end - cur;
        const DWORD chunk = (remaining < (DWORD64)MAX_BLOCK_SIZE)
            ? (DWORD)remaining
            : (DWORD)MAX_BLOCK_SIZE;
        ScanBlock(buffer.data(), next, signature, cur, chunk, result);

        if (chunk <= overlap)
            break;  // 防止 chunk - overlap 下溢导致死循环
        cur += chunk - overlap;
    }
    return result;
}
