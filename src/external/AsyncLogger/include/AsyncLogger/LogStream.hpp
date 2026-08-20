#pragma once
#include <string>
#include <memory>
#include <format>

namespace al
{
    class LogStream
    {
    public:
        LogStream() = default;
        LogStream(const std::string& name, bool enabled = true);
        virtual ~LogStream() = default;

        void SetEnabled(bool enabled) { m_Enabled = enabled; }
        bool Enabled() const { return m_Enabled; }
        const std::string& Name() const { return m_Name; }

    private:
        std::string m_Name;
        bool m_Enabled = true;
    };
}