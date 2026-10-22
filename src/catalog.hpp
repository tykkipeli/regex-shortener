#pragma once
#include "automata.hpp"
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sys/file.h>
#include <unistd.h>
namespace rx {
// An index is a hint, never an equivalence certificate. Every lookup is
// verified against a freshly constructed automaton before substitution.
struct LanguageCatalog {
    std::map<std::string, R> entries;
    size_t rejected = 0;
    static std::string hex(const std::string &s) {
        const char *h = "0123456789abcdef";
        std::string out;
        out.reserve(s.size() * 2);
        for (unsigned char c : s) {
            out += h[c >> 4];
            out += h[c & 15];
        }
        return out;
    }
    static std::string unhex(const std::string &s) {
        if (s.size() % 82 || s.empty() || s.size() > 4096 * 82)
            return {};
        auto digit = [](char c) {
            return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        };
        std::string out;
        for (size_t i = 0; i < s.size(); i += 2) {
            int a = digit(s[i]), b = digit(s[i + 1]);
            if (a < 0 || b < 0)
                return {};
            out += char(a * 16 + b);
        }
        return out;
    }
    void remember(const std::string &k, R r) {
        auto i = entries.find(k);
        if (i == entries.end()) {
            if (entries.size() < 20000)
                entries.emplace(k, r);
        } else if (better(r, i->second))
            i->second = r;
    }
    R find(const std::string &k) const {
        auto i = entries.find(k);
        return i == entries.end() ? R{} : i->second;
    }
    void load(const std::string &path) {
        std::ifstream in(path);
        std::string line;
        size_t count = 0;
        while (count++ < 25000 && std::getline(in, line)) {
            if (line.empty() || line[0] == '#')
                continue;
            auto tab = line.find('\t');
            if (tab == std::string::npos || line.size() > 400000) {
                ++rejected;
                continue;
            }
            std::string k = unhex(line.substr(0, tab));
            if (k.empty()) {
                ++rejected;
                continue;
            }
            try {
                remember(k, Parser(line.substr(tab + 1), false).parse());
            } catch (const std::exception &) {
                ++rejected;
            }
        }
    }
    void seed(const std::string &path, const Limits &limits) {
        std::ifstream in(path);
        std::string line;
        Budget budget(10);
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#')
                continue;
            try {
                R r = Parser(line, false).parse();
                remember(key(compile(r, limits, &budget)), r);
            } catch (const std::exception &) {
                ++rejected;
            }
        }
    }
    void save(const std::string &path) {
        namespace fs = std::filesystem;
        fs::path file(path);
        if (!file.parent_path().empty())
            fs::create_directories(file.parent_path());
        std::string lockName = path + ".lock";
        int fd = ::open(lockName.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (fd < 0)
            throw std::runtime_error("Katalog-Sperre kann nicht geoeffnet werden");
        struct Guard {
            int fd;
            ~Guard() {
                flock(fd, LOCK_UN);
                ::close(fd);
            }
        } guard{fd};
        if (flock(fd, LOCK_EX) < 0)
            throw std::runtime_error("Katalog kann nicht gesperrt werden");
        load(path);
        std::string temporary = path + ".tmp." + std::to_string(::getpid());
        {
            std::ofstream out(temporary, std::ios::trunc);
            if (!out)
                throw std::runtime_error("Katalog kann nicht geschrieben werden");
            out << "# regex-shortener catalog v1: best known, not generally proven minimal\n";
            for (const auto &[k, r] : entries)
                out << hex(k) << '\t' << r->s << '\n';
            out.flush();
            if (!out)
                throw std::runtime_error("Katalog konnte nicht vollstaendig geschrieben werden");
        }
        fs::rename(temporary, file);
    }
};
} // namespace rx
