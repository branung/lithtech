#include "de_message.h"

#include <vector>

namespace
{
    // Reserved up front so the common case never allocates
    std::vector<CDEPendingSends::SEntry>& Entries()
    {
        static std::vector<CDEPendingSends::SEntry> s_Entries;
        if (s_Entries.capacity() == 0)
            s_Entries.reserve(8);
        return s_Entries;
    }
}

void CDEPendingSends::Add(const SEntry& entry)
{
    if (!entry.m_pMsg)
        return;

    Entries().push_back(entry);
}

bool CDEPendingSends::Take(ILTMessage_Write* pMsg, SEntry& entryOut)
{
    std::vector<SEntry>& entries = Entries();

    // Searched back to front since messages usually end in reverse order
    for (std::vector<SEntry>::reverse_iterator it = entries.rbegin();
         it != entries.rend(); ++it)
    {
        if (it->m_pMsg != pMsg)
            continue;

        entryOut = *it;
        entries.erase((it + 1).base());
        return true;
    }

    return false;
}

void CDEPendingSends::Clear()
{
    Entries().clear();
}

uint32 CDEPendingSends::Count()
{
    return (uint32)Entries().size();
}
