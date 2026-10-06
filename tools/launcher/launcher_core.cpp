#include "launcher_core.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace ltlaunch {

std::string JoinPath(const std::string &dir, const std::string &leaf)
{
    if (dir.empty()) return leaf;
    char last = dir[dir.size() - 1];
    if (last == '/' || last == '\\') return dir + leaf;
#ifdef _WIN32
    return dir + "\\" + leaf;
#else
    return dir + "/" + leaf;
#endif
}

bool FileExists(const std::string &sPath)
{
    std::ifstream f(sPath.c_str());
    return f.good();
}

LaunchSpec ShogoLaunch(const std::string &sExeDir)
{
    LaunchSpec spec;
    spec.m_sEngineDir = sExeDir;
    spec.m_sGameDir = JoinPath(sExeDir, "Shogo");
    spec.m_Rez.push_back("SHOGO.REZ");
    spec.m_Rez.push_back("SOUND.REZ");
    return spec;
}

std::string FirstMissing(const LaunchSpec &spec)
{
    for (size_t i = 0; i < spec.m_Rez.size(); ++i)
        if (!FileExists(JoinPath(spec.m_sGameDir, spec.m_Rez[i])))
            return spec.m_Rez[i];
    return std::string();
}

std::string ReadTextFile(const std::string &sPath)
{
    std::ifstream in(sPath.c_str(), std::ios::binary);
    std::ostringstream os;
    os << in.rdbuf();
    return os.str();
}

bool WriteTextFile(const std::string &sPath, const std::string &sText)
{
    std::ofstream out(sPath.c_str(), std::ios::binary);
    out << sText;
    return !out.fail();
}

namespace {

bool LargerFirst(const WindowSize &a, const WindowSize &b)
{
    if (a.m_nWidth != b.m_nWidth) return a.m_nWidth > b.m_nWidth;
    return a.m_nHeight > b.m_nHeight;
}

bool EqualNoCase(const std::string &a, const std::string &b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

bool ReadWord(const std::string &line, size_t &i, std::string &out)
{
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i >= line.size()) return false;
    if (line[i] == '"')
    {
        const size_t end = line.find('"', i + 1);
        if (end == std::string::npos) return false;
        out = line.substr(i + 1, end - i - 1);
        i = end + 1;
    }
    else
    {
        size_t end = line.find_first_of(" \t\r", i);
        if (end == std::string::npos) end = line.size();
        out = line.substr(i, end - i);
        i = end;
    }
    return true;
}

bool ParseConfigLine(const std::string &line, std::string &sName, std::string &sValue)
{
    size_t i = line.find_first_not_of(" \t");
    if (i == std::string::npos) return false;
    if (line[i] == '+' || line[i] == '-') ++i;
    return ReadWord(line, i, sName) && ReadWord(line, i, sValue);
}

std::string LineEnding(const std::string &sCfg)
{
    const bool bHasLF   = sCfg.find('\n') != std::string::npos;
    const bool bHasCRLF = sCfg.find("\r\n") != std::string::npos;
    return (!bHasLF || bHasCRLF) ? "\r\n" : "\n";
}

size_t FindValue(const std::vector<ConfigValue> &values, const std::string &sName)
{
    for (size_t k = 0; k < values.size(); ++k)
        if (EqualNoCase(values[k].m_sName, sName)) return k;
    return values.size();
}

// "Name" "Value"
std::string QuotedLine(const ConfigValue &value)
{
    return "\"" + value.m_sName + "\" \"" + value.m_sValue + "\"";
}

}

std::vector<WindowSize> DistinctSizes(const std::vector<WindowSize> &sizes)
{
    std::vector<WindowSize> out;
    for (size_t i = 0; i < sizes.size(); ++i)
    {
        if (sizes[i].m_nWidth <= 0 || sizes[i].m_nHeight <= 0) continue;
        bool bDup = false;
        for (size_t j = 0; j < out.size() && !bDup; ++j)
            bDup = out[j].m_nWidth == sizes[i].m_nWidth && out[j].m_nHeight == sizes[i].m_nHeight;
        if (!bDup) out.push_back(sizes[i]);
    }
    std::sort(out.begin(), out.end(), LargerFirst);
    return out;
}

std::string SizeText(const WindowSize &m)
{
    std::ostringstream os;
    os << m.m_nWidth << " x " << m.m_nHeight;
    return os.str();
}

bool GetConfigValue(const std::string &sCfg, const std::string &sName, std::string &sValue)
{
    std::istringstream in(sCfg);
    std::string line, sLineName, sLineValue;
    bool bFound = false;
    while (std::getline(in, line))
    {
        if (ParseConfigLine(line, sLineName, sLineValue) && EqualNoCase(sLineName, sName))
        {
            sValue = sLineValue;
            bFound = true;
        }
    }
    return bFound;
}

std::string SetConfigValues(const std::string &sCfg, const std::vector<ConfigValue> &values)
{
    const std::string sEol = LineEnding(sCfg);
    std::vector<bool> written(values.size(), false);
    std::ostringstream os;
    std::istringstream in(sCfg);
    std::string line, sLineName, sLineValue;
    while (std::getline(in, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        if (ParseConfigLine(line, sLineName, sLineValue))
        {
            const size_t k = FindValue(values, sLineName);
            if (k < values.size())
            {
                line = QuotedLine(values[k]);
                written[k] = true;
            }
        }
        os << line << sEol;
    }

    for (size_t k = 0; k < values.size(); ++k)
        if (!written[k]) os << QuotedLine(values[k]) << sEol;
    return os.str();
}

std::vector<std::string> BuildCommandLine(const LaunchSpec &spec)
{
    std::vector<std::string> args;
#ifdef _WIN32
    args.push_back(JoinPath(spec.m_sEngineDir, "Lithtech.exe"));
#else
    args.push_back(JoinPath(spec.m_sEngineDir, "Lithtech"));
#endif
    for (size_t i = 0; i < spec.m_Rez.size(); ++i)
    {
        args.push_back("-rez");
        args.push_back(spec.m_Rez[i]);
    }
    return args;
}

std::string QuoteArgs(const std::vector<std::string> &args)
{
    std::ostringstream os;
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (i) os << ' ';
        if (args[i].find(' ') != std::string::npos) os << '"' << args[i] << '"';
        else os << args[i];
    }
    return os.str();
}

} // namespace ltlaunch
