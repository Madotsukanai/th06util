#include "Backup.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "Supervisor.hpp"
#include "inttypes.hpp"
#include "utils.hpp"

namespace th06
{

#pragma pack(push, 1)
struct ZipLocalHeader
{
    u32 signature;
    u16 versionNeeded;
    u16 flags;
    u16 compressionMethod;
    u16 modTime;
    u16 modDate;
    u32 crc32;
    u32 compressedSize;
    u32 uncompressedSize;
    u16 fileNameLength;
    u16 extraFieldLength;
};

struct ZipCentralDirHeader
{
    u32 signature;
    u16 versionMadeBy;
    u16 versionNeeded;
    u16 flags;
    u16 compressionMethod;
    u16 modTime;
    u16 modDate;
    u32 crc32;
    u32 compressedSize;
    u32 uncompressedSize;
    u16 fileNameLength;
    u16 extraFieldLength;
    u16 fileCommentLength;
    u16 diskNumberStart;
    u16 internalFileAttributes;
    u32 externalFileAttributes;
    u32 relativeOffsetOfLocalHeader;
};

struct ZipEndOfCentralDir
{
    u32 signature;
    u16 diskNumber;
    u16 startDiskNumber;
    u16 totalEntriesDisk;
    u16 totalEntries;
    u32 centralDirSize;
    u32 centralDirOffset;
    u16 commentLength;
};
#pragma pack(pop)

static u32 CalculateCrc32(const u8 *data, size_t length)
{
    static u32 table[256];
    static bool haveTable = false;
    if (!haveTable)
    {
        for (u32 i = 0; i < 256; i++)
        {
            u32 c = i;
            for (int j = 0; j < 8; j++)
            {
                c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        haveTable = true;
    }

    u32 crc = 0xFFFFFFFFL;
    for (size_t i = 0; i < length; i++)
    {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFL;
}

static void GetDosDateTime(u16 *dosTime, u16 *dosDate)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    if (dosTime)
    {
        *dosTime = (u16)(((st.wHour & 0x1F) << 11) | ((st.wMinute & 0x3F) << 5) | ((st.wSecond / 2) & 0x1F));
    }
    if (dosDate)
    {
        int year = st.wYear >= 1980 ? st.wYear - 1980 : 0;
        *dosDate = (u16)(((year & 0x7F) << 9) | ((st.wMonth & 0x0F) << 5) | (st.wDay & 0x1F));
    }
}

struct BackupFileItem
{
    char fullPath[MAX_PATH];
    char zipPath[MAX_PATH];
};

struct WrittenEntry
{
    char zipPath[MAX_PATH];
    u32 crc32;
    u32 size;
    u32 localHeaderOffset;
};

void CreateGameBackup(bool force)
{
    if (g_Backup != 1)
    {
        return;
    }

    static DWORD s_lastBackupTick = 0;
    DWORD now = GetTickCount();
    if (!force && s_lastBackupTick != 0 && (now - s_lastBackupTick) < 3000)
    {
        return;
    }

    char baseDir[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, baseDir, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
    {
        strcpy(baseDir, ".");
    }
    else
    {
        char *slash = strrchr(baseDir, '\\');
        if (!slash)
        {
            slash = strrchr(baseDir, '/');
        }
        if (slash)
        {
            *slash = '\0';
        }
        else
        {
            strcpy(baseDir, ".");
        }
    }

    char backupDir[MAX_PATH];
    _snprintf(backupDir, sizeof(backupDir), "%s\\backup", baseDir);
    CreateDirectoryA(backupDir, NULL);

    SYSTEMTIME st;
    GetLocalTime(&st);

    char zipPath[MAX_PATH];
    _snprintf(zipPath, sizeof(zipPath), "%s\\%04d%02d%02d_%02d%02d%02d.zip", backupDir, st.wYear, st.wMonth, st.wDay,
              st.wHour, st.wMinute, st.wSecond);

    if (GetFileAttributesA(zipPath) != INVALID_FILE_ATTRIBUTES)
    {
        _snprintf(zipPath, sizeof(zipPath), "%s\\%04d%02d%02d_%02d%02d%02d_%03d.zip", backupDir, st.wYear, st.wMonth,
                  st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        int counter = 1;
        while (GetFileAttributesA(zipPath) != INVALID_FILE_ATTRIBUTES)
        {
            _snprintf(zipPath, sizeof(zipPath), "%s\\%04d%02d%02d_%02d%02d%02d_%03d_%d.zip", backupDir, st.wYear,
                      st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, counter++);
        }
    }

    BackupFileItem *files = NULL;
    int filesCount = 0;
    int filesCap = 0;

    auto addFile = [&](const char *full, const char *zip) {
        if (filesCount >= filesCap)
        {
            filesCap = (filesCap == 0) ? 16 : filesCap * 2;
            files = (BackupFileItem *)realloc(files, sizeof(BackupFileItem) * filesCap);
        }
        strncpy(files[filesCount].fullPath, full, MAX_PATH - 1);
        files[filesCount].fullPath[MAX_PATH - 1] = '\0';
        strncpy(files[filesCount].zipPath, zip, MAX_PATH - 1);
        files[filesCount].zipPath[MAX_PATH - 1] = '\0';
        filesCount++;
    };

    char scorePath[MAX_PATH];
    _snprintf(scorePath, sizeof(scorePath), "%s\\score.dat", baseDir);
    DWORD attr = GetFileAttributesA(scorePath);
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        addFile(scorePath, "score.dat");
    }

    char replayPattern[MAX_PATH];
    _snprintf(replayPattern, sizeof(replayPattern), "%s\\replay\\*", baseDir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(replayPattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }
            char filePath[MAX_PATH];
            _snprintf(filePath, sizeof(filePath), "%s\\replay\\%s", baseDir, fd.cFileName);
            char zipRelPath[MAX_PATH];
            _snprintf(zipRelPath, sizeof(zipRelPath), "replay/%s", fd.cFileName);
            addFile(filePath, zipRelPath);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    if (filesCount == 0)
    {
        if (files)
        {
            free(files);
        }
        th06::utils::LogDebug("CreateGameBackup: No score.dat or replay files to backup");
        return;
    }

    FILE *fout = fopen(zipPath, "wb");
    if (!fout)
    {
        if (files)
        {
            free(files);
        }
        th06::utils::LogDebug("CreateGameBackup: Failed to create zip: %s", zipPath);
        return;
    }

    u16 dosTime = 0, dosDate = 0;
    GetDosDateTime(&dosTime, &dosDate);

    WrittenEntry *writtenEntries = (WrittenEntry *)malloc(sizeof(WrittenEntry) * filesCount);
    int writtenCount = 0;

    for (int i = 0; i < filesCount; i++)
    {
        FILE *fin = fopen(files[i].fullPath, "rb");
        if (!fin)
        {
            continue;
        }
        fseek(fin, 0, SEEK_END);
        long fileSize = ftell(fin);
        fseek(fin, 0, SEEK_SET);

        u8 *buffer = NULL;
        if (fileSize > 0)
        {
            buffer = (u8 *)malloc(fileSize);
            if (buffer)
            {
                fread(buffer, 1, fileSize, fin);
            }
        }
        fclose(fin);

        u32 crc = (fileSize > 0 && buffer) ? CalculateCrc32(buffer, fileSize) : 0;
        u32 localOffset = (u32)ftell(fout);

        u16 nameLen = (u16)strlen(files[i].zipPath);

        ZipLocalHeader lh;
        memset(&lh, 0, sizeof(lh));
        lh.signature = 0x04034b50;
        lh.versionNeeded = 20;
        lh.flags = 0x0800;
        lh.compressionMethod = 0;
        lh.modTime = dosTime;
        lh.modDate = dosDate;
        lh.crc32 = crc;
        lh.compressedSize = (u32)fileSize;
        lh.uncompressedSize = (u32)fileSize;
        lh.fileNameLength = nameLen;
        lh.extraFieldLength = 0;

        fwrite(&lh, 1, sizeof(lh), fout);
        fwrite(files[i].zipPath, 1, nameLen, fout);
        if (fileSize > 0 && buffer)
        {
            fwrite(buffer, 1, fileSize, fout);
            free(buffer);
        }

        strncpy(writtenEntries[writtenCount].zipPath, files[i].zipPath, MAX_PATH - 1);
        writtenEntries[writtenCount].zipPath[MAX_PATH - 1] = '\0';
        writtenEntries[writtenCount].crc32 = crc;
        writtenEntries[writtenCount].size = (u32)fileSize;
        writtenEntries[writtenCount].localHeaderOffset = localOffset;
        writtenCount++;
    }

    u32 cdOffset = (u32)ftell(fout);

    for (int i = 0; i < writtenCount; i++)
    {
        u16 nameLen = (u16)strlen(writtenEntries[i].zipPath);

        ZipCentralDirHeader cdh;
        memset(&cdh, 0, sizeof(cdh));
        cdh.signature = 0x02014b50;
        cdh.versionMadeBy = 20;
        cdh.versionNeeded = 20;
        cdh.flags = 0x0800;
        cdh.compressionMethod = 0;
        cdh.modTime = dosTime;
        cdh.modDate = dosDate;
        cdh.crc32 = writtenEntries[i].crc32;
        cdh.compressedSize = writtenEntries[i].size;
        cdh.uncompressedSize = writtenEntries[i].size;
        cdh.fileNameLength = nameLen;
        cdh.extraFieldLength = 0;
        cdh.fileCommentLength = 0;
        cdh.diskNumberStart = 0;
        cdh.internalFileAttributes = 0;
        cdh.externalFileAttributes = 0x20;
        cdh.relativeOffsetOfLocalHeader = writtenEntries[i].localHeaderOffset;

        fwrite(&cdh, 1, sizeof(cdh), fout);
        fwrite(writtenEntries[i].zipPath, 1, nameLen, fout);
    }

    u32 cdSize = (u32)ftell(fout) - cdOffset;

    ZipEndOfCentralDir eocd;
    memset(&eocd, 0, sizeof(eocd));
    eocd.signature = 0x06054b50;
    eocd.diskNumber = 0;
    eocd.startDiskNumber = 0;
    eocd.totalEntriesDisk = (u16)writtenCount;
    eocd.totalEntries = (u16)writtenCount;
    eocd.centralDirSize = cdSize;
    eocd.centralDirOffset = cdOffset;
    eocd.commentLength = 0;

    fwrite(&eocd, 1, sizeof(eocd), fout);
    fclose(fout);

    free(files);
    free(writtenEntries);

    s_lastBackupTick = now;
    th06::utils::LogDebug("CreateGameBackup: Saved backup to %s (%d files)", zipPath, writtenCount);

    if (g_BackupKeepCount > 0)
    {
        PruneOldBackups(backupDir, g_BackupKeepCount);
    }
}

struct BackupZipEntry
{
    char fileName[MAX_PATH];
    char fullPath[MAX_PATH];
    FILETIME writeTime;
};

static bool IsBackupZipName(const char *name)
{

    size_t len = strlen(name);
    if (len < 19)
    {
        return false;
    }
    if (_stricmp(name + len - 4, ".zip") != 0)
    {
        return false;
    }
    for (int i = 0; i < 8; i++)
    {
        if (name[i] < '0' || name[i] > '9')
        {
            return false;
        }
    }
    if (name[8] != '_')
    {
        return false;
    }
    for (int i = 9; i < 15; i++)
    {
        if (name[i] < '0' || name[i] > '9')
        {
            return false;
        }
    }
    return true;
}

static int CompareBackupZipEntries(const void *a, const void *b)
{
    const BackupZipEntry *ea = (const BackupZipEntry *)a;
    const BackupZipEntry *eb = (const BackupZipEntry *)b;
    int cmp = strcmp(ea->fileName, eb->fileName);
    if (cmp != 0)
    {
        return cmp;
    }
    return CompareFileTime(&ea->writeTime, &eb->writeTime);
}

void PruneOldBackups(const char *backupDir, int keepCount)
{
    if (backupDir == NULL || keepCount <= 0)
    {
        return;
    }

    char searchPattern[MAX_PATH];
    _snprintf(searchPattern, sizeof(searchPattern), "%s\\*.zip", backupDir);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return;
    }

    BackupZipEntry *list = NULL;
    int count = 0;
    int cap = 0;

    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }

        if (!IsBackupZipName(fd.cFileName))
        {
            continue;
        }

        if (count >= cap)
        {
            cap = (cap == 0) ? 32 : cap * 2;
            list = (BackupZipEntry *)realloc(list, sizeof(BackupZipEntry) * cap);
        }

        strncpy(list[count].fileName, fd.cFileName, MAX_PATH - 1);
        list[count].fileName[MAX_PATH - 1] = '\0';
        _snprintf(list[count].fullPath, sizeof(list[count].fullPath), "%s\\%s", backupDir, fd.cFileName);
        list[count].writeTime = fd.ftLastWriteTime;
        count++;
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);

    if (list != NULL)
    {
        if (count > keepCount)
        {
            qsort(list, count, sizeof(BackupZipEntry), CompareBackupZipEntries);
            int toDelete = count - keepCount;
            for (int i = 0; i < toDelete; i++)
            {
                SetFileAttributesA(list[i].fullPath, FILE_ATTRIBUTE_NORMAL);
                DeleteFileA(list[i].fullPath);
                th06::utils::LogDebug("PruneOldBackups: Deleted old backup %s", list[i].fullPath);
            }
        }
        free(list);
    }
}

} // namespace th06
