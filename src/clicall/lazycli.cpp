#include<string>

namespace lazy {

#ifdef _WIN32
#include <windows.h>

    // GBK转UTF-8
    std::string gbkToUtf8(const std::string& gbkStr) {
        if (gbkStr.empty()) {
            return "";
        }

        // 1. 将GBK字符串转换为宽字符（UTF-16）
        int wideCharLen = MultiByteToWideChar(
            CP_ACP,           // 源代码页（GBK）
            0,                // 标志
            gbkStr.c_str(),   // 源字符串
            static_cast<int>(gbkStr.length()), // 实际长度，不包括终止符
            nullptr,          // 目标缓冲区（nullptr用于获取所需长度）
            0                 // 目标缓冲区大小
        );

        if (wideCharLen == 0) {
            return ""; // 转换失败
        }

        // 分配宽字符缓冲区
        std::wstring wideStr(wideCharLen, 0);

        // 执行实际转换
        int result = MultiByteToWideChar(
            CP_ACP,
            0,
            gbkStr.c_str(),
            static_cast<int>(gbkStr.length()),
            &wideStr[0],
            wideCharLen
        );

        if (result == 0) {
            return ""; // 转换失败
        }

        // 2. 将宽字符（UTF-16）转换为UTF-8
        int utf8Len = WideCharToMultiByte(
            CP_UTF8,          // 目标代码页（UTF-8）
            0,                // 标志
            wideStr.c_str(),  // 源宽字符串
            static_cast<int>(wideStr.length()), // 实际长度，不包括终止符
            nullptr,          // 目标缓冲区（nullptr用于获取所需长度）
            0,                // 目标缓冲区大小
            nullptr,          // 默认字符
            nullptr           // 用于指示是否使用了默认字符的指针
        );

        if (utf8Len == 0) {
            return ""; // 转换失败
        }

        // 分配UTF-8字符串缓冲区
        std::string utf8Str(utf8Len, 0);

        // 执行实际转换
        result = WideCharToMultiByte(
            CP_UTF8,
            0,
            wideStr.c_str(),
            static_cast<int>(wideStr.length()),
            &utf8Str[0],
            utf8Len,
            nullptr,
            nullptr
        );

        if (result == 0) {
            return ""; // 转换失败
        }

        return utf8Str;
    }
#endif
    class SafeProcess {
    public:
        FILE* fp = nullptr;
        SafeProcess(const std::string& command) {
            fp = popen(command.c_str(), "r");
        }

        ~SafeProcess() {
            if (fp) {
                // 获取文件描述符并杀掉对应进程组，防止 Broken Pipe
                // 简单处理也可以直接 pclose
                pclose(fp);
            }
        }

    };

    std::string run(const std::string &cmd) {
        std::string res;
        // auto pipe = popen(cmd.c_str(), "r");
        SafeProcess pipe(cmd);
        if (!pipe.fp) return res;

        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe.fp) != nullptr) { res += buffer; }

#ifdef _WIN32
        res = gbkToUtf8(res);
#endif

        return res;
    }
}
