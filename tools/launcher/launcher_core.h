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

struct WindowSize
{
    int m_nWidth, m_nHeight;
};

struct ConfigValue
{
    std::string m_sName, m_sValue;
};

std::string JoinPath(const std::string &dir, const std::string &leaf);

bool FileExists(const std::string &sPath);

// A whole file, or an empty string when it can't be read
std::string ReadTextFile(const std::string &sPath);

// Replaces a file (false when it can't be written)
bool WriteTextFile(const std::string &sPath, const std::string &sText);

// Distinct sizes only, largest first
std::vector<WindowSize> DistinctSizes(const std::vector<WindowSize> &sizes);

std::string SizeText(const WindowSize &m);

// A variable's value in a config file's text
// A line may be "Name" "Value" or Name Value with a + or - in front
bool GetConfigValue(const std::string &sCfg, const std::string &sName, std::string &sValue);

std::string SetConfigValues(const std::string &sCfg, const std::vector<ConfigValue> &values);

LaunchSpec ShogoLaunch(const std::string &sExeDir);

// The first rez file that isn't in the game directory, or an empty string when all are there
std::string FirstMissing(const LaunchSpec &spec);

// The engine's argument list. The caller sets the working directory to m_sGameDir
std::vector<std::string> BuildCommandLine(const LaunchSpec &spec);

// Joins the arguments into one command line, quoting any with spaces
std::string QuoteArgs(const std::vector<std::string> &args);

} // namespace ltlaunch

#endif
