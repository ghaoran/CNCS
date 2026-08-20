#pragma once

enum Tab {
    VISUAL,
    COMBAT,
    WORLD,
    SYSTEM
};

struct TabItem
{
    Tab id;
    std::string label;
};

static const TabItem tabs[] =
{
    { Tab::VISUAL, "视觉" },
    { Tab::COMBAT, "战斗" },
    { Tab::WORLD,  "世界" },
    { Tab::SYSTEM, "系统" }
};

class Menu {
public:
    ~Menu() = default;
    Menu(const Menu&) = delete;
    Menu(Menu&&) = delete;
    Menu& operator=(const Menu&) = delete;
    Menu& operator=(Menu&&) = delete;

    static bool Init();
    static void Render();

    static void RenderStartupHelp();

    static ImVec2 GetPos();
    static ImVec2 GetSize();
private:
    Menu() {};

    static Menu& GetInstance()
    {
        static Menu i{};
        return i;
    }

    bool InitImpl();
    void RenderImpl();
    void RenderStartupHelpImpl();

    void SetupStyles();
private:
    bool isSetup = true;

    ImVec2 pos;
    ImVec2 size;
    
    ImFont* font_icons;
};