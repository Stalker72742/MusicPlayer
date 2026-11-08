//
// Created by Stalker7274 on 05.11.2026.
//

#pragma once

#include <QString>

/// @brief Writes a text report of what the process holds in memory, for hunting leaks by hand.
///
/// The report covers the process totals, the address space by kind and by module, the Windows heaps (live blocks
/// against free memory they keep), the QML JS heap, live QML objects by type, decoded images, the app's own data
/// and the child processes. Numbers that changed since the previous dump of this run carry the difference.
///
/// A dump only reads: nothing is freed or collected. It holds the heap locks while walking them and runs on the
/// calling thread, so the UI stops for a moment. Call it on the main thread.
class MemoryDump
{
public:
    /// @brief Writes a new report. Only the latest 50 reports are kept.
    /// @param reason A note put at the top of the report, e.g. what triggered it.
    /// @return The path of the report; empty if it could not be written.
    static QString Write(const QString& reason = {});

    /// @brief The folder of the reports: `memory-dumps` in the app's cache folder.
    static QString GetDirectory();
};
