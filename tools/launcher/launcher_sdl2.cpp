#include "launcher_core.h"
#include "launcher_splash.h"

#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#include <fstream>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../runtime/ui/src/stb_truetype.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

// The launcher's name and version
const char *kLauncherName    = "LithLauncher";
const char *kLauncherVersion = "0.01";
const char *kGameTitle       = "Shogo: Mobile Armor Division";
const char *kSplashRez       = "SHOGO.REZ";
const char *kSplashPath      = "INTERFACE\\SPLASH.PCX";

// Launcher color palette (0xRRGGBB)
const Uint32 kFace   = 0xF0F0F0;
const Uint32 kWindow = 0xFFFFFF;
const Uint32 kBorder = 0x7A7A7A;
const Uint32 kButton = 0xE1E1E1;
const Uint32 kButtonDown = 0xCCE4F7;
const Uint32 kBorderDown = 0x005499;
const Uint32 kText   = 0x000000;
const Uint32 kAlert  = 0xA00000;
const Uint32 kGrayTxt = 0x6D6D6D;
const Uint32 kCaption = 0x0078D7;
const Uint32 kCapText = 0xFFFFFF;

const int kWinW = 640, kWinH = 470;

struct Rect
{
    int x, y, w, h;
    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(int a, int b, int c, int d) : x(a), y(b), w(c), h(d) {}
};

bool In(const Rect &r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

const Rect kPlayButton(406, 12, 222, 26);
const Rect kQuitButton(406, 330, 222, 26);
const Rect kDisplayButton(406, 52, 222, 26);

const Rect kDialog((kWinW - 440) / 2, (kWinH - 320) / 2, 440, 320);
const Rect kDialogCaption(kDialog.x + 1, kDialog.y + 1, kDialog.w - 2, 22);
const Rect kDialogClose(kDialogCaption.x + kDialogCaption.w - 21, kDialogCaption.y + 2, 18, 18);
const Rect kDialogOK(kDialog.x + kDialog.w - 176, kDialog.y + kDialog.h - 34, 80, 24);
const Rect kDialogCancel(kDialog.x + kDialog.w - 88, kDialog.y + kDialog.h - 34, 80, 24);

SDL_Renderer *g_pRen  = 0;
SDL_Texture  *g_pFont = 0;

const int   kFontFirst = 32, kFontCount = 95;
const float kFontPx    = 16.0f;

stbtt_bakedchar g_Glyphs[kFontCount];
int g_nAscent = 0, g_nLineH = 0;

// Drawing

void SetColor(Uint32 c)
{
    SDL_SetRenderDrawColor(g_pRen, (Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c, 255);
}

void Fill(const Rect &r, Uint32 c)
{
    SDL_Rect sr = { r.x, r.y, r.w, r.h };
    SetColor(c);
    SDL_RenderFillRect(g_pRen, &sr);
}

void Frame(const Rect &r, Uint32 c)
{
    SDL_Rect sr = { r.x, r.y, r.w, r.h };
    SetColor(c);
    SDL_RenderDrawRect(g_pRen, &sr);
}

int GlyphIndex(int ch)
{
    if (ch < kFontFirst || ch >= kFontFirst + kFontCount) ch = '?';
    return ch - kFontFirst;
}

int TextW(const std::string &s)
{
    float w = 0.0f;
    for (size_t i = 0; i < s.size(); ++i) w += g_Glyphs[GlyphIndex((unsigned char)s[i])].xadvance;
    return (int)(w + 0.5f);
}

void Text(int x, int y, const std::string &s, Uint32 c, int nMaxW = 100000)
{
    if (!g_pFont) return;
    SDL_SetTextureColorMod(g_pFont, (Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c);
    float pen = (float)x;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const stbtt_bakedchar &b = g_Glyphs[GlyphIndex((unsigned char)s[i])];
        if (pen + b.xadvance > (float)(x + nMaxW)) break;
        SDL_Rect src = { b.x0, b.y0, b.x1 - b.x0, b.y1 - b.y0 };
        if (src.w > 0 && src.h > 0)
        {
            SDL_Rect dst = { (int)(pen + 0.5f) + (int)b.xoff, y + g_nAscent + (int)b.yoff,
                             src.w, src.h };
            SDL_RenderCopy(g_pRen, g_pFont, &src, &dst);
        }
        pen += b.xadvance;
    }
}

void TextC(const Rect &r, const std::string &s, Uint32 c)
{
    Text(r.x + (r.w - TextW(s)) / 2, r.y + (r.h - g_nLineH) / 2, s, c, r.w);
}

// Tahoma from the system's fonts. Has fallback attempts on Linux in case it's not available.
bool ReadFontFile(std::vector<unsigned char> &data)
{
    std::vector<std::string> paths;
#ifdef _WIN32
    const char *pWinDir = SDL_getenv("WINDIR");
    paths.push_back(std::string(pWinDir ? pWinDir : "C:\\Windows") + "\\Fonts\\tahoma.ttf");
#else
    paths.push_back("/usr/share/wine/fonts/tahoma.ttf");
    paths.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    paths.push_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
    paths.push_back("/usr/share/fonts/dejavu/DejaVuSans.ttf");
#endif
    for (size_t i = 0; i < paths.size(); ++i)
    {
        std::ifstream in(paths[i].c_str(), std::ios::binary | std::ios::ate);
        if (!in.good()) continue;
        const std::streamsize nSize = in.tellg();
        if (nSize <= 0) continue;
        data.resize((size_t)nSize);
        in.seekg(0);
        if (in.read((char *)&data[0], nSize)) return true;
    }
    return false;
}

SDL_Texture *MakeFontAtlas()
{
    std::vector<unsigned char> ttf;
    if (!ReadFontFile(ttf)) return 0;

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, &ttf[0], stbtt_GetFontOffsetForIndex(&ttf[0], 0))) return 0;
    const float scale = stbtt_ScaleForPixelHeight(&info, kFontPx);
    int nAscent, nDescent, nGap;
    stbtt_GetFontVMetrics(&info, &nAscent, &nDescent, &nGap);
    g_nAscent = (int)(nAscent * scale + 0.5f);
    g_nLineH  = (int)((nAscent - nDescent) * scale + 0.5f);

    const int w = 512, h = 256;
    std::vector<unsigned char> alpha((size_t)w * h);
    if (stbtt_BakeFontBitmap(&ttf[0], 0, kFontPx, &alpha[0], w, h,
                             kFontFirst, kFontCount, g_Glyphs) <= 0)
        return 0;

    SDL_Surface *pSurf = SDL_CreateRGBSurface(0, w, h, 32,
                                              0x00FF0000, 0x0000FF00, 0x000000FF,
                                              0xFF000000);
    if (!pSurf) return 0;
    SDL_LockSurface(pSurf);
    for (int y = 0; y < h; ++y)
    {
        Uint32 *pRow = (Uint32 *)((Uint8 *)pSurf->pixels + y * pSurf->pitch);
        for (int x = 0; x < w; ++x)
            pRow[x] = ((Uint32)alpha[(size_t)y * w + x] << 24) | 0x00FFFFFFu;
    }
    SDL_UnlockSurface(pSurf);
    SDL_Texture *pTex = SDL_CreateTextureFromSurface(g_pRen, pSurf);
    SDL_FreeSurface(pSurf);
    if (pTex) SDL_SetTextureBlendMode(pTex, SDL_BLENDMODE_BLEND);
    return pTex;
}

void Button(const Rect &r, const std::string &sLabel, bool bPressed = false)
{
    Fill(r, bPressed ? kButtonDown : kButton);
    Frame(r, bPressed ? kBorderDown : kBorder);
    TextC(bPressed ? Rect(r.x + 1, r.y + 1, r.w, r.h) : r, sLabel, kText);
}

// Starts the engine with the game directory as its working directory
bool Spawn(const std::vector<std::string> &args, const std::string &sCwd,
           std::string &sError)
{
#ifdef _WIN32
    std::string sCmd = ltlaunch::QuoteArgs(args);
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    std::vector<char> cmd(sCmd.begin(), sCmd.end()); cmd.push_back('\0');
    if (!CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, 0, NULL,
                        sCwd.c_str(), &si, &pi))
    { sError = "Could not start " + args[0]; return false; }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
#else
    const pid_t pid = fork();
    if (pid < 0) { sError = "Could not start " + args[0]; return false; }
    if (pid == 0)
    {
        if (chdir(sCwd.c_str()) != 0) _exit(127);
        std::vector<const char *> cargv;
        for (size_t i = 0; i < args.size(); ++i) cargv.push_back(args[i].c_str());
        cargv.push_back(0);
        execv(cargv[0], (char *const *)&cargv[0]);
        _exit(127);
    }
    return true;
#endif
}

struct App
{
    ltlaunch::LaunchSpec spec;
    std::string sStatus;
    bool bDisplayOpen = false;
    bool bQuit = false;

    const Rect *pPressed = 0;
    bool bPressedOver = false;

    SDL_Texture *pSplash = 0;
    bool bSplashTried = false;

    // Refuses a launch whose archives aren't there.
    void DoPlay()
    {
        if (!ltlaunch::FirstMissing(spec).empty()) return;

        std::string sError;
        if (!Spawn(ltlaunch::BuildCommandLine(spec), spec.m_sGameDir, sError))
        { sStatus = sError; return; }
        sStatus.clear();
    }

    bool Down(const Rect &r) const { return pPressed == &r && bPressedOver; }

    const Rect *ButtonAt(int x, int y) const
    {
        static const Rect *const kMain[] = { &kPlayButton, &kDisplayButton, &kQuitButton };
        static const Rect *const kDlg[]  = { &kDialogOK, &kDialogCancel, &kDialogClose };
        const Rect *const *pList = bDisplayOpen ? kDlg : kMain;
        for (int i = 0; i < 3; ++i)
            if (In(*pList[i], x, y)) return pList[i];
        return 0;
    }

    void Press(const Rect *p)
    {
        if (p == &kPlayButton) DoPlay();
        else if (p == &kDisplayButton) bDisplayOpen = true;
        else if (p == &kQuitButton) bQuit = true;
        else bDisplayOpen = false;   // OK, Cancel and the close box
    }

    void Draw()
    {
        Fill(Rect(0, 0, kWinW, kWinH), kFace);

        const Rect title(12, 12, 380, 22);
        Fill(title, kWindow);
        Frame(title, kBorder);
        Text(title.x + 4, title.y + (title.h - g_nLineH) / 2, kGameTitle, kText, title.w - 8);

        const Rect panel(12, 40, 380, 290);
        Fill(panel, kWindow);
        Frame(panel, kBorder);

        std::vector<bool> present;
        bool bMissing = false;
        for (size_t i = 0; i < spec.m_Rez.size(); ++i)
        {
            present.push_back(ltlaunch::FileExists(ltlaunch::JoinPath(spec.m_sGameDir, spec.m_Rez[i])));
            bMissing = bMissing || !present.back();
        }
        if (bMissing)
        {
            DrawDataMissing(panel, present);
            bSplashTried = false; // loaded again once the archives are back
        }
        else
            DrawSplash(panel);

        Button(kPlayButton, "Launch", Down(kPlayButton));
        Button(kDisplayButton, "Display...", Down(kDisplayButton));
        Button(kQuitButton, "Quit", Down(kQuitButton));

        // A refused or failed launch
        Text(12, 366, sStatus, kAlert, kWinW - 24);

        // The footer
        const std::string sLine = std::string(kLauncherName) + " v" + kLauncherVersion;
        Text((kWinW - TextW(sLine)) / 2, 440, sLine, kText);

        if (bDisplayOpen) DrawDisplayDialog();
    }

    void LoadSplash()
    {
        bSplashTried = true;
        if (pSplash) { SDL_DestroyTexture(pSplash); pSplash = 0; }

        std::vector<unsigned char> raw;
        ltlaunch::Image img;
        if (!ltlaunch::ReadRezEntry(ltlaunch::JoinPath(spec.m_sGameDir, kSplashRez), kSplashPath, raw) ||
            !ltlaunch::DecodePcx(raw, img))
            return;

        pSplash = SDL_CreateTexture(g_pRen, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC,
                                    img.m_nWidth, img.m_nHeight);
        if (!pSplash) return;
        SDL_UpdateTexture(pSplash, 0, &img.m_RGB[0], img.m_nWidth * 3);
        SDL_SetTextureScaleMode(pSplash, SDL_ScaleModeLinear);
    }

    void DrawSplash(const Rect &panel)
    {
        if (!bSplashTried) LoadSplash();
        if (!pSplash) return;

        int nW = 0, nH = 0;
        SDL_QueryTexture(pSplash, 0, 0, &nW, &nH);
        const Rect in(panel.x + 1, panel.y + 1, panel.w - 2, panel.h - 2);
        int w = in.w, h = nH * in.w / nW;
        if (h > in.h) { h = in.h; w = nW * in.h / nH; }
        Fill(in, 0x000000);
        SDL_Rect dst = { in.x + (in.w - w) / 2, in.y + (in.h - h) / 2, w, h };
        SDL_RenderCopy(g_pRen, pSplash, 0, &dst);
    }

    // Names each archive the game needs, whether it was found, and the folder they belong in
    void DrawDataMissing(const Rect &panel, const std::vector<bool> &present)
    {
        const Rect in(panel.x + 3, panel.y + 3, panel.w - 6, panel.h - 6);
        const int nMaxW = in.w - 8;
        int y = in.y + 12;

        const std::string sHead = "GAME DATA MISSING";
        const int hx = in.x + (in.w - TextW(sHead)) / 2;
        Text(hx, y, sHead, kAlert);
        Text(hx + 1, y, sHead, kAlert);
        y += g_nLineH + 10;

        Text(in.x + 4, y, "This game's archives were not found. Copy them into:", kText, nMaxW);
        y += g_nLineH + 4;

        std::string sDir = spec.m_sGameDir;
        while (!sDir.empty())
        {
            size_t n = 1;
            while (n < sDir.size() && TextW(sDir.substr(0, n + 1)) <= nMaxW) ++n;
            if (n < sDir.size())
            {
                const size_t cut = sDir.find_last_of("\\/", n - 1);
                if (cut != std::string::npos && cut > 0) n = cut + 1;
            }
            Text(in.x + 4, y, sDir.substr(0, n), kText, nMaxW);
            sDir.erase(0, n);
            y += g_nLineH;
        }
        y += 8;

        for (size_t i = 0; i < spec.m_Rez.size() && y + g_nLineH <= in.y + in.h; ++i)
        {
            Text(in.x + 12, y, present[i] ? "found" : "missing", present[i] ? kGrayTxt : kAlert);
            Text(in.x + 84, y, spec.m_Rez[i], present[i] ? kGrayTxt : kText, in.w - 88);
            y += g_nLineH;
        }
    }

    void DrawDisplayDialog()
    {
        Fill(kDialog, kFace);
        Frame(kDialog, kBorder);
        Fill(kDialogCaption, kCaption);
        Text(kDialogCaption.x + 6, kDialogCaption.y + (kDialogCaption.h - g_nLineH) / 2,
             "Display Settings", kCapText, kDialogCaption.w - 32);
        Button(kDialogClose, "x", Down(kDialogClose));
        Button(kDialogOK, "OK", Down(kDialogOK));
        Button(kDialogCancel, "Cancel", Down(kDialogCancel));
    }
};

} // namespace

int LauncherMain()
{
#ifdef _WIN32
    SDL_SetMainReady();
#endif

    // A click that activates the window still reaches the buttons
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    // Lithtech.exe and the game directory are found from where the launcher runs
    char *pBasePath = SDL_GetBasePath();
    const std::string sExeDir = pBasePath ? pBasePath : ".";
    if (pBasePath) SDL_free(pBasePath);

    App app;
    app.spec = ltlaunch::ShogoLaunch(sExeDir);

    SDL_Window *pWnd = SDL_CreateWindow(kLauncherName,
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        kWinW, kWinH, SDL_WINDOW_SHOWN);
    if (!pWnd) { SDL_Quit(); return 1; }
    g_pRen = SDL_CreateRenderer(pWnd, -1, SDL_RENDERER_SOFTWARE);
    if (!g_pRen) { SDL_DestroyWindow(pWnd); SDL_Quit(); return 1; }
    g_pFont = MakeFontAtlas();
    if (!g_pFont)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kLauncherName,
                                 "Could not load the Tahoma font.", pWnd);
        SDL_DestroyRenderer(g_pRen);
        SDL_DestroyWindow(pWnd);
        SDL_Quit();
        return 1;
    }

    bool bRedraw = true;
    while (!app.bQuit)
    {
        if (bRedraw)
        {
            app.Draw();
            SDL_RenderPresent(g_pRen);
            bRedraw = false;
        }

        SDL_Event e;
        if (!SDL_WaitEvent(&e)) break;
        do
        {
            if (e.type == SDL_QUIT)
                app.bQuit = true;
            else if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_EXPOSED ||
                                                   e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED))
                bRedraw = true;
            else if (e.type == SDL_KEYDOWN && app.bDisplayOpen &&
                     (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_RETURN ||
                      e.key.keysym.sym == SDLK_KP_ENTER))
            {
                app.bDisplayOpen = false;
                app.pPressed = 0;
                bRedraw = true;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
            {
                app.pPressed = app.ButtonAt(e.button.x, e.button.y);
                app.bPressedOver = true;
                if (app.pPressed) bRedraw = true;
            }
            else if (e.type == SDL_MOUSEMOTION && app.pPressed)
            {
                const bool bOver = In(*app.pPressed, e.motion.x, e.motion.y);
                if (bOver != app.bPressedOver) { app.bPressedOver = bOver; bRedraw = true; }
            }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT && app.pPressed)
            {
                const Rect *p = app.pPressed;
                app.pPressed = 0;
                if (In(*p, e.button.x, e.button.y)) app.Press(p);
                bRedraw = true;
            }
        } while (SDL_PollEvent(&e));
    }

    if (app.pSplash) SDL_DestroyTexture(app.pSplash);
    if (g_pFont) SDL_DestroyTexture(g_pFont);
    SDL_DestroyRenderer(g_pRen);
    SDL_DestroyWindow(pWnd);
    SDL_Quit();
    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return LauncherMain();
}
#else
int main()
{
    return LauncherMain();
}
#endif
