#pragma once

// Probes which CPU slices a Mach-O dylib on disk actually contains, so
// arch advice can be based on the INSTALLED core rather than the
// manifest's core_arch (which describes the distributed artifact — a
// locally installed universal/native core supersedes it; see the
// native-arm transition). Header-only like host_arch.h.

#include <QFile>
#include <QString>
#include <QtEndian>

#include <mach-o/fat.h>
#include <mach-o/loader.h>

#include "host_arch.h"

namespace DylibArch {

// True if the Mach-O file at `path` contains a slice for `cputype`
// (CPU_TYPE_ARM64 / CPU_TYPE_X86_64). False on read/parse failure —
// callers treat "unknown" as "trust the manifest".
inline bool fileContainsArch(const QString& path, cpu_type_t cputype)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    quint32 magic_raw = 0;
    if (f.read(reinterpret_cast<char*>(&magic_raw), 4) != 4)
        return false;

    const quint32 magic = qFromLittleEndian(magic_raw);
    if (magic == MH_MAGIC_64) {
        // Thin 64-bit image: cputype is the next little-endian word.
        quint32 ct = 0;
        if (f.read(reinterpret_cast<char*>(&ct), 4) != 4)
            return false;
        return static_cast<cpu_type_t>(qFromLittleEndian(ct)) == cputype;
    }

    const quint32 magic_be = qFromBigEndian(magic_raw);
    if (magic_be == FAT_MAGIC || magic_be == FAT_MAGIC_64) {
        // Universal binary: fat header fields are big-endian on disk.
        quint32 nfat_raw = 0;
        if (f.read(reinterpret_cast<char*>(&nfat_raw), 4) != 4)
            return false;
        const quint32 nfat = qFromBigEndian(nfat_raw);
        if (nfat > 32) // arbitrary sanity bound; real files have 2-3
            return false;
        const qint64 arch_size = (magic_be == FAT_MAGIC_64)
            ? sizeof(fat_arch_64) : sizeof(fat_arch);
        for (quint32 i = 0; i < nfat; i++) {
            quint32 ct_raw = 0; // cputype is the first field of both structs
            if (f.seek(8 + static_cast<qint64>(i) * arch_size) &&
                f.read(reinterpret_cast<char*>(&ct_raw), 4) == 4 &&
                static_cast<cpu_type_t>(qFromBigEndian(ct_raw)) == cputype)
                return true;
        }
        return false;
    }

    return false;
}

// True if `path` has a slice matching the RUNNING process's arch
// (i.e. the core can be dlopen'd natively by this process).
inline bool fileContainsHostArch(const QString& path)
{
    return fileContainsArch(
        path, HostArch::isArm64() ? CPU_TYPE_ARM64 : CPU_TYPE_X86_64);
}

} // namespace DylibArch
