//
// Created by Stalker7274 on 07.11.2026.
//

#include "AppModule.h"

#include <algorithm>
#include <utility>

namespace
{
    struct Entry
    {
        QString name;
        AppModule::Factory factory;
    };

    std::vector<Entry>& Registry()
    {
        static std::vector<Entry> entries;
        return entries;
    }

    std::vector<Entry> Sorted()
    {
        std::vector<Entry> entries = Registry();
        std::ranges::sort(entries, {}, &Entry::name);
        return entries;
    }
}

bool AppModule::Register(const char* name, Factory factory)
{
    Registry().push_back({QString::fromLatin1(name), factory});
    return true;
}

std::vector<std::unique_ptr<AppModule>> AppModule::CreateAll()
{
    std::vector<std::unique_ptr<AppModule>> modules;
    for (const Entry& entry : Sorted())
        modules.push_back(entry.factory());
    return modules;
}

QStringList AppModule::GetNames()
{
    QStringList names;
    for (const Entry& entry : Sorted())
        names.append(entry.name);
    return names;
}
