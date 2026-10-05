#include "SPILock.h"
#include "configuration.h"
#include <lofs/LoFS.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#if LOBBS_EXTRA_QSPI
#include <CustomLFS_QSPIFlash.h>
static CustomLFS_QSPIFlash lobfsQspiFlash;
#endif

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
#include <SD.h>
#include <SPI.h>

#ifdef SDCARD_USE_SPI1
extern SPIClass SPI_HSPI;
#define SDHandler SPI_HSPI
#else
#define SDHandler SPI
#endif

#ifndef SD_SPI_FREQUENCY
#define SD_SPI_FREQUENCY 4000000U
#endif
#endif

LoFS::Mount LoFS::mounts[4];
int LoFS::mountCount = 0;
bool LoFS::begun = false;

static bool lobfsSdPresent()
{
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        concurrency::LockGuard g(spiLock);
        SDHandler.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
        if (SD.begin(SDCARD_CS, SDHandler, SD_SPI_FREQUENCY))
            cardType = SD.cardType();
    }
    return cardType != CARD_NONE;
#else
    return false;
#endif
}

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
static const char *convertToSDMode(const char *modeStr)
{
    return modeStr;
}
static const char *convertToSDMode(uint8_t mode)
{
    return (mode == 0) ? "r" : "w";
}
#else
static uint8_t convertToSDMode(const char *modeStr)
{
    return (strcmp(modeStr, "r") == 0) ? FILE_READ : FILE_WRITE;
}
static uint8_t convertToSDMode(uint8_t mode)
{
    return (mode == 0) ? FILE_READ : FILE_WRITE;
}
#endif
#endif

void LoFS::begin()
{
    if (begun)
        return;
    mountCount = 0;

    mounts[mountCount++] = Mount{"flash", Backend::Flash, true};

#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
    if (lobfsSdPresent())
        mounts[mountCount++] = Mount{"sd", Backend::Sd, true};
#endif

#if LOBBS_EXTRA_QSPI
    concurrency::LockGuard g(spiLock);
    if (lobfsQspiFlash.begin())
        mounts[mountCount++] = Mount{"extra", Backend::Extra, true};
#endif

    begun = true;
}

LoFS::Mount *LoFS::findByName(const char *name, size_t len)
{
    for (int i = 0; i < mountCount; i++) {
        if (strncmp(mounts[i].name, name, len) == 0 && mounts[i].name[len] == '\0' && mounts[i].present)
            return &mounts[i];
    }
    return nullptr;
}

bool LoFS::mountPresent(const char *name)
{
    return findByName(name, strlen(name)) != nullptr;
}

void LoFS::eachPresentMount(void (*fn)(void *ctx, const char *name), void *ctx)
{
    if (!fn)
        return;
    for (int i = 0; i < mountCount; i++) {
        if (mounts[i].present)
            fn(ctx, mounts[i].name);
    }
}

bool LoFS::resolve(const char *filepath, Resolved &out)
{
    out = {};
    if (!filepath || filepath[0] != '/')
        return false;

    if (filepath[1] == '\0') {
        out.kind = PathKind::VirtualRoot;
        return true;
    }

    const char *p = filepath + 1;
    const char *slash = strchr(p, '/');
    size_t nameLen = slash ? (size_t)(slash - p) : strlen(p);
    if (nameLen == 0 || nameLen >= 16)
        return false;

    Mount *m = findByName(p, nameLen);
    if (!m)
        return false;

    out.backend = m->backend;
    if (!slash || slash[1] == '\0') {
        out.kind = PathKind::MountRoot;
        out.rel = "/";
        return true;
    }

    out.rel = slash;
    out.kind = PathKind::Normal;
    return true;
}

const char *LoFS::backendPath(const Resolved &r)
{
    return r.backend == Backend::Sd ? r.rel + 1 : r.rel;
}

bool LoFS::isMountPoint(const char *path)
{
    if (!path)
        return false;
    if (strcmp(path, "/") == 0)
        return true;
    Resolved r;
    if (!resolve(path, r))
        return false;
    return r.kind == PathKind::MountRoot;
}

const char *LoFS::mountNameForPath(const char *path)
{
    if (!path || path[0] != '/')
        return nullptr;
    if (path[1] == '\0')
        return nullptr;
    const char *p = path + 1;
    const char *slash = strchr(p, '/');
    size_t nameLen = slash ? (size_t)(slash - p) : strlen(p);
    Mount *m = findByName(p, nameLen);
    return m ? m->name : nullptr;
}

bool LoFS::isDirectory(const char *path)
{
    Resolved r;
    if (!resolve(path, r))
        return false;
    if (r.kind == PathKind::VirtualRoot || r.kind == PathKind::MountRoot)
        return true;

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
    File f;
#else
    File f(FSCom);
#endif
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        f = SD.open(bp, FILE_O_READ);
#endif
    } else if (r.backend == Backend::Extra) {
#if LOBBS_EXTRA_QSPI
        f = lobfsQspiFlash.open(bp, FILE_O_READ);
#endif
    } else {
        f = FSCom.open(bp, FILE_O_READ);
    }
#else
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        f = SD.open(bp, FILE_O_READ);
#endif
    } else if (r.backend == Backend::Extra) {
#if LOBBS_EXTRA_QSPI
        f = lobfsQspiFlash.open(bp, FILE_O_READ);
#endif
    } else {
        f = FSCom.open(bp, FILE_O_READ);
    }
#endif
    if (!f)
        return false;
    bool isDir = f.isDirectory();
    f.close();
    return isDir;
}

bool LoFS::refuseMountPointMutation(const char *filepath)
{
    return isMountPoint(filepath);
}

File LoFS::open(const char *filepath, uint8_t mode)
{
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
    const char *modeStr = (mode == 0) ? FILE_O_READ : FILE_O_WRITE;
    return open(filepath, modeStr);
#else
    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot) {
        return File(FSCom);
    }

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.open(bp, convertToSDMode(mode));
#endif
    }
    if (r.backend == Backend::Extra) {
#if LOBBS_EXTRA_QSPI
        return lobfsQspiFlash.open(bp, mode);
#endif
    }
    return FSCom.open(bp, mode);
#endif
}

File LoFS::open(const char *filepath, const char *mode)
{
    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot) {
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
        return File();
#else
        return File(FSCom);
#endif
    }

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
        return SD.open(bp, mode);
#else
        return SD.open(bp, convertToSDMode(mode));
#endif
#endif
    }
    if (r.backend == Backend::Extra) {
#if LOBBS_EXTRA_QSPI
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
        return lobfsQspiFlash.open(bp, mode);
#else
        uint8_t m = (strcmp(mode, "r") == 0) ? 0 : 1;
        return lobfsQspiFlash.open(bp, m);
#endif
#endif
    }
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
    return FSCom.open(bp, mode);
#else
    uint8_t flashMode = (mode && strcmp(mode, "r") == 0) ? 0 : 1;
    return FSCom.open(bp, flashMode);
#endif
}

bool LoFS::exists(const char *filepath)
{
    Resolved r;
    if (!resolve(filepath, r))
        return false;
    if (r.kind == PathKind::VirtualRoot)
        return true;
    if (r.kind == PathKind::MountRoot)
        return true;

    const char *bp = backendPath(r);
    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.exists(bp);
#endif
    }
    if (r.backend == Backend::Extra) {
#if LOBBS_EXTRA_QSPI
        return lobfsQspiFlash.exists(bp);
#endif
    }
    return FSCom.exists(bp);
}

bool LoFS::mkdir(const char *filepath)
{
    if (refuseMountPointMutation(filepath))
        return false;

    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot)
        return false;

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.mkdir(bp);
#endif
    }
    if (r.backend == Backend::Extra) {
#if LOBBS_EXTRA_QSPI
        return lobfsQspiFlash.mkdir(bp);
#endif
    }
    return FSCom.mkdir(bp);
}

bool LoFS::remove(const char *filepath)
{
    if (refuseMountPointMutation(filepath))
        return false;

    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot || r.kind == PathKind::MountRoot)
        return false;

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.remove(bp);
#endif
    }
    if (r.backend == Backend::Extra) {
#if LOBBS_EXTRA_QSPI
        return lobfsQspiFlash.remove(bp);
#endif
    }
    return FSCom.remove(bp);
}

bool LoFS::rename(const char *oldfilepath, const char *newfilepath)
{
    if (refuseMountPointMutation(oldfilepath) || refuseMountPointMutation(newfilepath))
        return false;

    Resolved oldR;
    Resolved newR;
    if (!resolve(oldfilepath, oldR) || !resolve(newfilepath, newR))
        return false;
    if (oldR.kind == PathKind::VirtualRoot || newR.kind == PathKind::VirtualRoot)
        return false;
    if (oldR.backend != newR.backend)
        return false;

    const char *oldBp = backendPath(oldR);
    const char *newBp = backendPath(newR);

    concurrency::LockGuard g(spiLock);
    if (oldR.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.rename(oldBp, newBp);
#endif
    }
    if (oldR.backend == Backend::Extra) {
#if LOBBS_EXTRA_QSPI
        return lobfsQspiFlash.rename(oldBp, newBp);
#endif
    }
    return FSCom.rename(oldBp, newBp);
}

static bool lobfsEachDirEntry(File &dir, void *ctx, LoFS::ListCallback fn)
{
    while (true) {
        File file = dir.openNextFile();
        if (!file)
            break;

        std::string pathFromFile = file.name();
        bool isDir = file.isDirectory();
        file.close();

        size_t lastSlash = pathFromFile.rfind('/');
        std::string entryName = (lastSlash != std::string::npos) ? pathFromFile.substr(lastSlash + 1) : pathFromFile;

        if (entryName == "." || entryName == "..")
            continue;

        if (!fn(ctx, entryName.c_str(), isDir))
            return false;
    }
    return true;
}

bool LoFS::list(const char *dirpath, void *ctx, ListCallback fn)
{
    if (!dirpath || !fn)
        return false;

    Resolved r;
    if (!resolve(dirpath, r))
        return false;

    if (r.kind == PathKind::VirtualRoot) {
        for (int i = 0; i < mountCount; i++) {
            if (!mounts[i].present)
                continue;
            if (!fn(ctx, mounts[i].name, true))
                return false;
        }
        return true;
    }

    File dir = open(dirpath, FILE_O_READ);
    if (!dir)
        return false;
    if (!dir.isDirectory()) {
        dir.close();
        return false;
    }

    bool ok = lobfsEachDirEntry(dir, ctx, fn);
    dir.close();
    return ok;
}

bool LoFS::stat(const char *filepath, uint32_t *sizeOut, bool *isDirOut)
{
    if (sizeOut)
        *sizeOut = 0;
    if (isDirOut)
        *isDirOut = false;

    Resolved r;
    if (!resolve(filepath, r))
        return false;
    if (r.kind == PathKind::VirtualRoot || r.kind == PathKind::MountRoot) {
        if (isDirOut)
            *isDirOut = true;
        return true;
    }

    File f = open(filepath, FILE_O_READ);
    if (!f)
        return false;
    if (isDirOut)
        *isDirOut = f.isDirectory();
    if (sizeOut && !f.isDirectory())
        *sizeOut = (uint32_t)f.size();
    f.close();
    return true;
}

bool LoFS::copy(const char *src, const char *dst)
{
    if (refuseMountPointMutation(src) || refuseMountPointMutation(dst))
        return false;
    if (exists(dst))
        return false;

    Resolved srcR;
    Resolved dstR;
    if (!resolve(src, srcR) || !resolve(dst, dstR))
        return false;
    if (srcR.kind == PathKind::VirtualRoot || srcR.kind == PathKind::MountRoot)
        return false;
    if (dstR.kind == PathKind::VirtualRoot || dstR.kind == PathKind::MountRoot)
        return false;

    File srcFile = open(src, FILE_O_READ);
    if (!srcFile)
        return false;
    if (srcFile.isDirectory()) {
        srcFile.close();
        return false;
    }

    File dstFile = open(dst, FILE_O_WRITE);
    if (!dstFile) {
        srcFile.close();
        return false;
    }

    unsigned char buffer[128];
    bool ok = true;
    while (true) {
        size_t n = 0;
        {
            concurrency::LockGuard g(spiLock);
            n = srcFile.read(buffer, sizeof(buffer));
        }
        if (n == 0)
            break;
        size_t w = 0;
        {
            concurrency::LockGuard g(spiLock);
            w = dstFile.write(buffer, n);
        }
        if (w != n) {
            ok = false;
            break;
        }
    }

    {
        concurrency::LockGuard g(spiLock);
        dstFile.flush();
        dstFile.close();
        srcFile.close();
    }

    if (!ok)
        remove(dst);
    return ok;
}

uint64_t LoFS::totalBytes(const char *mountRoot)
{
    Resolved r;
    if (!resolve(mountRoot, r) || r.kind != PathKind::MountRoot)
        return 0;
    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.totalBytes();
#endif
    }
    if (r.backend == Backend::Extra) {
#if LOBBS_EXTRA_QSPI
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
        return lobfsQspiFlash.totalBytes();
#endif
#endif
    }
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
    return FSCom.totalBytes();
#else
    return 0;
#endif
}

uint64_t LoFS::usedBytes(const char *mountRoot)
{
    Resolved r;
    if (!resolve(mountRoot, r) || r.kind != PathKind::MountRoot)
        return 0;
    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.usedBytes();
#endif
    }
    if (r.backend == Backend::Extra) {
#if LOBBS_EXTRA_QSPI
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
        return lobfsQspiFlash.usedBytes();
#endif
#endif
    }
#if defined(ARCH_ESP32) || defined(ARCH_RP2040) || defined(ARCH_PORTDUINO)
    return FSCom.usedBytes();
#else
    return 0;
#endif
}

uint64_t LoFS::freeBytes(const char *mountRoot)
{
    uint64_t total = totalBytes(mountRoot);
    uint64_t used = usedBytes(mountRoot);
    if (total == 0)
        return 0;
    return total - used;
}

bool LoFS::rmdir(const char *filepath, bool recursive)
{
    if (refuseMountPointMutation(filepath))
        return false;

    if (!exists(filepath))
        return true;

    if (recursive) {
        File dir = open(filepath, FILE_O_READ);
        if (!dir)
            return false;

        if (!dir.isDirectory()) {
            dir.close();
            return remove(filepath);
        }

        bool result = true;
        while (true) {
            File file = dir.openNextFile();
            if (!file)
                break;

            std::string pathFromFile = file.name();
            bool isDir = file.isDirectory();
            file.close();

            size_t lastSlash = pathFromFile.rfind('/');
            std::string entryName = (lastSlash != std::string::npos) ? pathFromFile.substr(lastSlash + 1) : pathFromFile;

            if (entryName == "." || entryName == "..")
                continue;

            std::string fullPath = std::string(filepath) + "/" + entryName;

            if (isDir) {
                if (!rmdir(fullPath.c_str(), true))
                    result = false;
            } else {
                if (!remove(fullPath.c_str()))
                    result = false;
            }
        }
        dir.close();

        if (!result)
            return false;
    }

    Resolved r;
    if (!resolve(filepath, r) || r.kind == PathKind::VirtualRoot || r.kind == PathKind::MountRoot)
        return false;

    const char *bp = backendPath(r);

    concurrency::LockGuard g(spiLock);
    if (r.backend == Backend::Sd) {
#if defined(HAS_SDCARD) && !defined(SDCARD_USE_SOFT_SPI)
        return SD.rmdir(bp);
#endif
    }
    if (r.backend == Backend::Extra) {
#if LOBBS_EXTRA_QSPI
        return lobfsQspiFlash.rmdir(bp);
#endif
    }
    return FSCom.rmdir(bp);
}
