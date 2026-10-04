#ifndef LTLAUNCH_CORE_H
#define LTLAUNCH_CORE_H

#include <string>
#include <vector>

namespace ltlaunch {

// Everything one launch needs
struct LaunchSpec
{
    std::string m_sGameDir; // The working directory (aka the /Shogo folder)
    std::string m_sEngineDir; // It's parent, holding Lithtech.exe
    std::vector<std::string> m_Rez; // The necessary .rez files (aka SHOGO.REZ)
};

std::string JoinPath(const std::string &dir, const std::string &leaf);

bool FileExists(const std::string &sPath);

LaunchSpec ShogoLaunch(const std::string &sExeDir);

// The first rez file that isn't in the game directory, or an empty string when all are there
std::string FirstMissing(const LaunchSpec &spec);

// The engine's argument list. The caller sets the working directory to m_sGameDir
std::vector<std::string> BuildCommandLine(const LaunchSpec &spec);

// Joins the arguments into one command line, quoting any with spaces
std::string QuoteArgs(const std::vector<std::string> &args);

} // namespace ltlaunch

#endif
