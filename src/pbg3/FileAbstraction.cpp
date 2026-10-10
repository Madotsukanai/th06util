#include "pbg3/FileAbstraction.hpp"

namespace th06
{
FileAbstraction::FileAbstraction()
{
    handle = INVALID_HANDLE_VALUE;
    access = 0;
}

i32 FileAbstraction::Open(char *filename, char *mode)
{
    int creationDisposition;
    i32 goToEnd = FALSE;

    this->Close();

    char *curMode;
    for (curMode = mode; *curMode != '\0'; curMode += 1)
    {
        if (*curMode == 'r')
        {
            this->access = GENERIC_READ;
            creationDisposition = OPEN_EXISTING;
            break;
        }
        else if (*curMode == 'w')
        {
            DeleteFileA(filename);
            this->access = GENERIC_WRITE;
            creationDisposition = OPEN_ALWAYS;
            break;
        }
        else if (*curMode == 'a')
        {
            goToEnd = true;
            this->access = GENERIC_WRITE;
            creationDisposition = OPEN_ALWAYS;
            break;
        }
    }

    if (*curMode == '\0')
    {
        return 0;
    }
    this->handle = CreateFileA(filename, this->access, FILE_SHARE_READ, NULL, creationDisposition,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    if (this->handle == INVALID_HANDLE_VALUE)
    {
        wchar_t *fallbackW = NULL;
        char *fallbackA = NULL;

        if (strstr(filename, "CM") || strstr(filename, "cm"))
        {
            fallbackW = L"\x7D05\x9B54\x90F7\x0043\x004D\x002E\x0064\x0061\x0074";
            fallbackA = "KOUMAKYO_CM.dat";
        }
        else if (strstr(filename, "ED") || strstr(filename, "ed"))
        {
            fallbackW = L"\x7D05\x9B54\x90F7\x0045\x0044\x002E\x0044\x0041\x0054";
            fallbackA = "KOUMAKYO_ED.dat";
        }
        else if (strstr(filename, "IN") || strstr(filename, "in"))
        {
            fallbackW = L"\x7D05\x9B54\x90F7\x0049\x004E\x002E\x0064\x0061\x0074";
            fallbackA = "KOUMAKYO_IN.dat";
        }
        else if (strstr(filename, "MD") || strstr(filename, "md"))
        {
            fallbackW = L"\x7D05\x9B54\x90F7\x004D\x0044\x002E\x0064\x0061\x0074";
            fallbackA = "KOUMAKYO_MD.dat";
        }
        else if (strstr(filename, "ST") || strstr(filename, "st"))
        {
            fallbackW = L"\x7D05\x9B54\x90F7\x0053\x0054\x002E\x0064\x0061\x0074";
            fallbackA = "KOUMAKYO_ST.dat";
        }
        else if (strstr(filename, "TL") || strstr(filename, "tl"))
        {
            fallbackW = L"\x7D05\x9B54\x90F7\x0054\x004C\x002E\x0064\x0061\x0074";
            fallbackA = "KOUMAKYO_TL.dat";
        }

        if (fallbackW != NULL)
        {
            this->handle = CreateFileW(fallbackW, this->access, FILE_SHARE_READ, NULL, creationDisposition,
                                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        }
        if (this->handle == INVALID_HANDLE_VALUE && fallbackA != NULL)
        {
            this->handle = CreateFileA(fallbackA, this->access, FILE_SHARE_READ, NULL, creationDisposition,
                                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        }
    }

    if (this->handle == INVALID_HANDLE_VALUE)
        return 0;

    if (goToEnd)
    {
        SetFilePointer(this->handle, 0, NULL, FILE_END);
    }
    return 1;
}

void FileAbstraction::Close()
{
    if (this->handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(this->handle);
        this->handle = INVALID_HANDLE_VALUE;
        this->access = 0;
    }
}

i32 FileAbstraction::Read(u8 *data, u32 dataLen, u32 *numBytesRead)
{
    if (this->access != GENERIC_READ)
    {
        return FALSE;
    }

    return ReadFile(this->handle, data, dataLen, reinterpret_cast<DWORD *>(numBytesRead), NULL);
}

i32 FileAbstraction::Write(u8 *data, u32 dataLen, u32 *outWritten)
{
    if (this->access != GENERIC_WRITE)
    {
        return FALSE;
    }

    return WriteFile(this->handle, data, dataLen, reinterpret_cast<DWORD *>(outWritten), NULL);
}

i32 FileAbstraction::ReadByte()
{
    u8 data;
    u32 outBytesRead;

    if (this->Read(&data, 1, &outBytesRead) == FALSE)
    {
        return -1;
    }
    else
    {
        if (outBytesRead == 0)
        {
            return -1;
        }
        return data;
    }
}

i32 FileAbstraction::WriteByte(u32 b)
{
    u8 outByte;
    u32 outBytesWritten;

    outByte = b;
    if (this->Write(&outByte, 1, &outBytesWritten) == FALSE)
    {
        return -1;
    }
    else
    {
        if (outBytesWritten == 0)
        {
            return -1;
        }
        return b;
    }
}

i32 FileAbstraction::Seek(u32 amount, u32 seekFrom)
{
    if (this->handle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    SetFilePointer(this->handle, amount, NULL, seekFrom);
    return 1;
}

u32 FileAbstraction::Tell()
{
    if (this->handle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    return SetFilePointer(this->handle, 0, NULL, FILE_CURRENT);
}

u32 FileAbstraction::GetSize()
{
    if (this->handle == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    return GetFileSize(this->handle, NULL);
}

u8 *FileAbstraction::ReadWholeFile(u32 maxSize)
{
    if (this->access != GENERIC_READ)
    {
        return NULL;
    }

    u32 dataLen = this->GetSize();
    u32 outDataLen;
    if (dataLen <= maxSize)
    {
        u8 *data = reinterpret_cast<u8 *>(LocalAlloc(LPTR, dataLen));
        if (data != NULL)
        {
            u32 oldLocation = this->Tell();

            if (this->Seek(oldLocation, FILE_BEGIN) != 0)
            {
                if (this->Read(data, dataLen, &outDataLen) == 0)
                {
                    LocalFree(data);
                    return NULL;
                }
                this->Seek(oldLocation, FILE_BEGIN);
                return data;
            }
        }
    }
    return NULL;
}

FileAbstraction::~FileAbstraction()
{
    this->Close();
}
}; // namespace th06
