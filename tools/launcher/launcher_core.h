#ifndef LTLAUNCH_CORE_H
#define LTLAUNCH_CORE_H

#include <string>
#include <vector>

namespace ltlaunch {

// Everything one launch needs
struct LaunchSpec
{
    std::string m_sGameDir; // The working directory, holding the game's modules and configs
    std::string m_sDataDir; // The folder holding the game's archives, when not m_sGameDir (a standard install)
    std::string m_sEngineDir; // The folder holding Lithtech.exe
    std::vector<std::string> m_Rez; // The necessary .rez files, passed with -rez (aka SHOGO.REZ)
    std::vector<std::string> m_EngineRez; // Necessary too, but the engine opens them itself (NOLF2's)
    std::vector<std::string> m_OptionalRez; // Passed with -rez when present
};

// An addon built as its own copy of the game's modules, in a folder inside the game folder.
// It runs from that folder and loads the game's archives from the one above
struct AddOn
{
    std::string m_sLabel; // The checkbox
    std::string m_sButton; // The Launch button while it's ticked
    std::string m_sDir; // The modules' folder, inside the game folder or a full path (empty for a game without one)
    std::string m_sSplashRez; // The archive holding its banner
    std::string m_sSplashPath;
    std::vector<std::string> m_Rez; // Loaded after the game's own
    std::vector<std::string> m_OptionalRez; // Loaded after those when present
};

// What sets one game's launcher apart
struct GameProfile
{
    std::string m_sCaption; // The window's caption
    std::string m_sTitle;
    std::string m_sName; // Its button in the combined launcher's game list
    std::string m_sKey; // Its folder beside the combined launcher, and its name in that launcher's launcher.ini
    std::string m_sSplashRez; // The archive holding the banner
    std::string m_sSplashPath; // The banner's path inside it
    LaunchSpec m_Spec;
    AddOn m_AddOn;
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

bool IsAbsolutePath(const std::string &sPath);

// Whether two paths name the same folder
bool SameDir(const std::string &a, const std::string &b);

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

// The folder beside LTCallisto holding a folder for each game
extern const char *kGamesFolder;

// LTCallisto: Lithtech.exe beside it and each game in a folder of its own in kGamesFolder, as the build lays them out.
// Only the games whose folders are there, and Shogo when none are
std::vector<GameProfile> CombinedProfiles(const std::string &sExeDir);

// The per-game launchers: Lithtech.exe and the game both in the launcher's own folder
GameProfile ShogoProfile(const std::string &sExeDir);
GameProfile Blood2Profile(const std::string &sExeDir);
GameProfile NOLF2Profile(const std::string &sExeDir);

// Every archive the game can't start without
std::vector<std::string> RequiredRez(const LaunchSpec &spec);

// The folder the game's archives are read from
std::string DataDir(const LaunchSpec &spec);

// An archive's -rez argument: its name in the game folder, or its full path in another data folder
std::string RezArg(const LaunchSpec &spec, const std::string &sRez);

// The first file that isn't in sDir, or an empty string when all are there
std::string FirstMissing(const std::string &sDir, const std::vector<std::string> &names);

// The addon's folder, which it runs from
std::string AddOnDir(const GameProfile &game);

// Whether the game has an addon and its modules are built
bool HasAddOn(const GameProfile &game);

// Where players put their own archives and levels inside the game folder
extern const char *kCustomFolder;

// The .rez files added in the Custom folder
std::vector<std::string> AvailableRez(const LaunchSpec &spec);

// The launcher's own settings, kept in launcher.ini beside it
struct LauncherSettings
{
    bool m_bAlwaysRez = false;
    std::vector<std::string> m_AlwaysRez;
    bool m_bAddOn = false; // The addon's checkbox
    std::string m_sDataDir; // The chosen data folder, empty for the game folder
};

// False when the file can't be read, leaving the defaults
bool LoadSettings(const std::string &sPath, LauncherSettings &settings);

// False when the file can't be written
bool SaveSettings(const std::string &sPath, const LauncherSettings &settings);

// The combined launcher's selected game, by its m_sKey, kept in its own launcher.ini (each game's settings are in its folder)
std::string LoadSelectedGame(const std::string &sPath);
bool SaveSelectedGame(const std::string &sPath, const std::string &sKey);

// The engine's argument list. The caller sets the working directory to m_sGameDir
// The engine opens m_EngineRez itself from there, so another data folder passes them too, and itself as GameDataDir
std::vector<std::string> BuildCommandLine(const LaunchSpec &spec);

// Joins the arguments into one command line, quoting any with spaces
std::string QuoteArgs(const std::vector<std::string> &args);

} // namespace ltlaunch

#endif
