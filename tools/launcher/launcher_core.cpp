#include "launcher_core.h"

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
