#include<string>

namespace lazy {
    std::string run(const std::string &cmd) {
        std::string res;
        auto pipe = popen(cmd.c_str(), "r");
        if (!pipe) return res;

        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) { res += buffer; }
        pclose(pipe);

        return res;
    }
}
