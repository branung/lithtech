#ifndef LTLAUNCH_CORE_H
#define LTLAUNCH_CORE_H

#include <string>
#include <vector>

namespace ltlaunch {

// Everything one launch needs
struct LaunchSpec
{
    std::string m_sGameDir; // The working directory, holding the game's archives
    std::string m_sEngineDir; // The folder holding Lithtech.exe
    std::vector<std::string> m_Rez; // The necessary .rez files, passed with -rez (aka SHOGO.REZ)
    std::vector<std::string> m_EngineRez; // Necessary too, but the engine opens them itself (NOLF2's)
    std::vector<std::string> m_OptionalRez; // Passed with -rez when present
};

// What sets one game's launcher apart
struct GameProfile
{
    std::string m_sCaption; // The window's caption
    std::string m_sTitle;
    std::string m_sSplashRez; // The archive holding the banner
    std::string m_sSplashPath; // The banner's path inside it
    bool m_bTitleBox = false; // The game box above the banner (LithLauncher's)
    LaunchSpec m_Spec;
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

bool DirExists(const std::string &sPath);

std::vector<std::string> ListFiles(const std::string &sDir, const std::string &sExt);

// Whether a list holds a name (compared without case)
bool ContainsNoCase(const std::vector<std::string> &list, const std::string &sName);

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

// LithLauncher: Lithtech.exe beside it and the game in its Shogo folder
GameProfile LithLauncherProfile(const std::string &sExeDir);

// The per-game launchers: Lithtech.exe and the game both in the launcher's own folder
GameProfile ShogoProfile(const std::string &sExeDir);
GameProfile Blood2Profile(const std::string &sExeDir);
GameProfile NOLF2Profile(const std::string &sExeDir);

// Every archive the game can't start without
std::vector<std::string> RequiredRez(const LaunchSpec &spec);

// The first rez file that isn't in the game directory, or an empty string when all are there
std::string FirstMissing(const LaunchSpec &spec);

// Where players put their own archives and levels inside the game folder
extern const char *kCustomFolder;

// The .rez files added in the Custom folder
std::vector<std::string> AvailableRez(const LaunchSpec &spec);

// The launcher's own settings, kept in launcher.ini beside it
struct LauncherSettings
{
    bool m_bAlwaysRez = false;
    std::vector<std::string> m_AlwaysRez;
};

// False when the file can't be read, leaving the defaults
bool LoadSettings(const std::string &sPath, LauncherSettings &settings);

// False when the file can't be written
bool SaveSettings(const std::string &sPath, const LauncherSettings &settings);

// The engine's argument list. The caller sets the working directory to m_sGameDir
std::vector<std::string> BuildCommandLine(const LaunchSpec &spec);

// Joins the arguments into one command line, quoting any with spaces
std::string QuoteArgs(const std::vector<std::string> &args);

} // namespace ltlaunch

#endif
