

#define STRICT
#include "zwave.hpp"
#include "GameManager.hpp"
#include "Supervisor.hpp"
#include "dxutil.hpp"
#include "utils.hpp"
#include <cstdio>
#include <dsound.h>
#include <dxerr8.h>
#include <mmsystem.h>
#include <windows.h>

static inline HRESULT ZwaveLogErr(const char *file, int line, HRESULT hr, const char *str)
{
    FILE *_f = fopen("log.txt", "at");
    if (_f)
    {
        fprintf(_f, "BGM error %s(%d): %s hr=0x%08x\n", file, line, str, (unsigned)hr);
        fclose(_f);
    }
    return hr;
}
#undef DXTRACE_ERR
#define DXTRACE_ERR(str, hr) ZwaveLogErr(__FILE__, __LINE__, hr, str)

namespace th06
{

CSoundManager::CSoundManager()
{
    m_pDS = NULL;
}

CSoundManager::~CSoundManager()
{
    SAFE_RELEASE(m_pDS);
}

HRESULT CSoundManager::Initialize(HWND hWnd, DWORD dwCoopLevel, DWORD dwPrimaryChannels, DWORD dwPrimaryFreq,
                                  DWORD dwPrimaryBitRate)
{
    HRESULT hr;
    LPDIRECTSOUNDBUFFER pDSBPrimary = NULL;

    SAFE_RELEASE(m_pDS);

    if (FAILED(hr = DirectSoundCreate8(NULL, &m_pDS, NULL)))
        return DXTRACE_ERR(TEXT("DirectSoundCreate8"), hr);

    if (FAILED(hr = m_pDS->SetCooperativeLevel(hWnd, dwCoopLevel)))
        return DXTRACE_ERR(TEXT("SetCooperativeLevel"), hr);

    SetPrimaryBufferFormat(dwPrimaryChannels, dwPrimaryFreq, dwPrimaryBitRate);

    return S_OK;
}

HRESULT CSoundManager::SetPrimaryBufferFormat(DWORD dwPrimaryChannels, DWORD dwPrimaryFreq, DWORD dwPrimaryBitRate)
{
    HRESULT hr;
    LPDIRECTSOUNDBUFFER pDSBPrimary = NULL;

    if (m_pDS == NULL)
        return CO_E_NOTINITIALIZED;

    DSBUFFERDESC dsbd;
    ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
    dsbd.dwSize = sizeof(DSBUFFERDESC);
    dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
    dsbd.dwBufferBytes = 0;
    dsbd.lpwfxFormat = NULL;

    if (FAILED(hr = m_pDS->CreateSoundBuffer(&dsbd, &pDSBPrimary, NULL)))
        return DXTRACE_ERR(TEXT("CreateSoundBuffer"), hr);

    WAVEFORMATEX wfx;
    ZeroMemory(&wfx, sizeof(WAVEFORMATEX));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = (WORD)dwPrimaryChannels;
    wfx.nSamplesPerSec = dwPrimaryFreq;
    wfx.wBitsPerSample = (WORD)dwPrimaryBitRate;
    wfx.nBlockAlign = wfx.wBitsPerSample / 8 * wfx.nChannels;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    if (FAILED(hr = pDSBPrimary->SetFormat(&wfx)))
        return DXTRACE_ERR(TEXT("SetFormat"), hr);

    SAFE_RELEASE(pDSBPrimary);

    return S_OK;
}

HRESULT CSoundManager::CreateStreaming(CStreamingSound **ppStreamingSound, LPTSTR strWaveFileName,
                                       DWORD dwCreationFlags, GUID guid3DAlgorithm, DWORD dwNotifyCount,
                                       DWORD dwNotifySize, HANDLE hNotifyEvent)
{
    HRESULT hr;

    if (m_pDS == NULL)
        return CO_E_NOTINITIALIZED;

    LPDIRECTSOUNDBUFFER pDSBuffer = NULL;
    DWORD dwDSBufferSize;
    CWaveFile *pWaveFile = NULL;
    DSBPOSITIONNOTIFY *aPosNotify = NULL;
    LPDIRECTSOUNDNOTIFY pDSNotify = NULL;

    pWaveFile = new CWaveFile();
    pWaveFile->Open(strWaveFileName, NULL, WAVEFILE_READ);

    dwDSBufferSize = dwNotifySize * dwNotifyCount;

    DSBUFFERDESC dsbd;
    ZeroMemory(&dsbd, sizeof(DSBUFFERDESC));
    dsbd.dwSize = sizeof(DSBUFFERDESC);
    dsbd.dwFlags = dwCreationFlags | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2 |
                   DSBCAPS_CTRLVOLUME | DSBCAPS_LOCSOFTWARE;
    dsbd.dwBufferBytes = dwDSBufferSize;
    dsbd.guid3DAlgorithm = guid3DAlgorithm;
    dsbd.lpwfxFormat = pWaveFile->m_pwfx;

    if (FAILED(hr = m_pDS->CreateSoundBuffer(&dsbd, &pDSBuffer, NULL)))
    {
        return E_FAIL;
    }

    if (FAILED(hr = pDSBuffer->QueryInterface(IID_IDirectSoundNotify, (VOID **)&pDSNotify)))
    {
        return E_FAIL;
    }

    aPosNotify = new DSBPOSITIONNOTIFY[dwNotifyCount];
    if (aPosNotify == NULL)
        return E_OUTOFMEMORY;

    for (DWORD i = 0; i < dwNotifyCount; i++)
    {
        aPosNotify[i].dwOffset = (dwNotifySize * i) + dwNotifySize - 1;
        aPosNotify[i].hEventNotify = hNotifyEvent;
    }

    if (FAILED(hr = pDSNotify->SetNotificationPositions(dwNotifyCount, aPosNotify)))
    {
        SAFE_RELEASE(pDSNotify);
        SAFE_DELETE_ARRAY(aPosNotify);
        return E_FAIL;
    }

    SAFE_RELEASE(pDSNotify);
    SAFE_DELETE_ARRAY(aPosNotify);

    *ppStreamingSound = new CStreamingSound(pDSBuffer, dwDSBufferSize, pWaveFile, dwNotifySize);

    return S_OK;
}

CSound::CSound(LPDIRECTSOUNDBUFFER *apDSBuffer, DWORD dwDSBufferSize, DWORD dwNumBuffers, CWaveFile *pWaveFile)
{
    DWORD i;

    m_apDSBuffer = new LPDIRECTSOUNDBUFFER[dwNumBuffers];
    for (i = 0; i < dwNumBuffers; i++)
        m_apDSBuffer[i] = apDSBuffer[i];

    m_dwDSBufferSize = dwDSBufferSize;
    m_dwNumBuffers = dwNumBuffers;
    m_pWaveFile = pWaveFile;

    FillBufferWithSound(m_apDSBuffer[0], FALSE);

    for (i = 0; i < dwNumBuffers; i++)
        m_apDSBuffer[i]->SetCurrentPosition(0);
}

CSound::~CSound()
{
    for (DWORD i = 0; i < m_dwNumBuffers; i++)
    {
        SAFE_RELEASE(m_apDSBuffer[i]);
    }

    SAFE_DELETE_ARRAY(m_apDSBuffer);
    SAFE_DELETE(m_pWaveFile);
}

HRESULT CSound::FillBufferWithSound(LPDIRECTSOUNDBUFFER pDSB, BOOL bRepeatWavIfBufferLarger)
{
    HRESULT hr;
    VOID *pDSLockedBuffer = NULL;
    DWORD dwDSLockedBufferSize = 0;
    DWORD dwWavDataRead = 0;

    if (pDSB == NULL)
        return CO_E_NOTINITIALIZED;

    if (FAILED(hr = RestoreBuffer(pDSB, NULL)))
        return DXTRACE_ERR(TEXT("RestoreBuffer"), hr);

    if (FAILED(hr = pDSB->Lock(0, m_dwDSBufferSize, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, NULL, 0L)))
        return DXTRACE_ERR(TEXT("Lock"), hr);

    m_pWaveFile->ResetFile(false);

    if (FAILED(hr = m_pWaveFile->Read((BYTE *)pDSLockedBuffer, dwDSLockedBufferSize, &dwWavDataRead)))
        return DXTRACE_ERR(TEXT("Read"), hr);

    if (dwWavDataRead == 0)
    {

        FillMemory((BYTE *)pDSLockedBuffer, dwDSLockedBufferSize,
                   (BYTE)(m_pWaveFile->m_pwfx->wBitsPerSample == 8 ? 128 : 0));
    }
    else if (dwWavDataRead < dwDSLockedBufferSize)
    {

        if (bRepeatWavIfBufferLarger)
        {

            DWORD dwReadSoFar = dwWavDataRead;
            while (dwReadSoFar < dwDSLockedBufferSize)
            {

                if (FAILED(hr = m_pWaveFile->ResetFile(false)))
                    return DXTRACE_ERR(TEXT("ResetFile"), hr);

                hr = m_pWaveFile->Read((BYTE *)pDSLockedBuffer + dwReadSoFar, dwDSLockedBufferSize - dwReadSoFar,
                                       &dwWavDataRead);
                if (FAILED(hr))
                    return DXTRACE_ERR(TEXT("Read"), hr);

                dwReadSoFar += dwWavDataRead;
            }
        }
        else
        {

            FillMemory((BYTE *)pDSLockedBuffer + dwWavDataRead, dwDSLockedBufferSize - dwWavDataRead,
                       (BYTE)(m_pWaveFile->m_pwfx->wBitsPerSample == 8 ? 128 : 0));
        }
    }

    pDSB->Unlock(pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);

    return S_OK;
}

HRESULT CSound::RestoreBuffer(LPDIRECTSOUNDBUFFER pDSB, BOOL *pbWasRestored)
{
    HRESULT hr;

    if (pDSB == NULL)
        return CO_E_NOTINITIALIZED;
    if (pbWasRestored)
        *pbWasRestored = FALSE;

    DWORD dwStatus;
    if (FAILED(hr = pDSB->GetStatus(&dwStatus)))
        return DXTRACE_ERR(TEXT("GetStatus"), hr);

    if (dwStatus & DSBSTATUS_BUFFERLOST)
    {

        do
        {
            hr = pDSB->Restore();
            if (hr == DSERR_BUFFERLOST)
                Sleep(10);
        } while (hr = pDSB->Restore());

        if (pbWasRestored != NULL)
            *pbWasRestored = TRUE;

        return S_OK;
    }
    else
    {
        return S_FALSE;
    }
}

LPDIRECTSOUNDBUFFER CSound::GetFreeBuffer()
{
    BOOL bIsPlaying = FALSE;

    if (m_apDSBuffer == NULL)
        return FALSE;

    DWORD i;
    for (i = 0; i < m_dwNumBuffers; i++)
    {
        if (m_apDSBuffer[i])
        {
            DWORD dwStatus = 0;
            m_apDSBuffer[i]->GetStatus(&dwStatus);
            if ((dwStatus & DSBSTATUS_PLAYING) == 0)
                break;
        }
    }

    if (i != m_dwNumBuffers)
        return m_apDSBuffer[i];
    else
        return m_apDSBuffer[rand() % m_dwNumBuffers];
}

LPDIRECTSOUNDBUFFER CSound::GetBuffer(DWORD dwIndex)
{
    if (m_apDSBuffer == NULL)
        return NULL;
    if (dwIndex >= m_dwNumBuffers)
        return NULL;

    return m_apDSBuffer[dwIndex];
}

HRESULT CSound::Play(DWORD dwPriority, DWORD dwFlags)
{
    HRESULT hr;
    BOOL bRestored;

    if (m_apDSBuffer == NULL)
        return CO_E_NOTINITIALIZED;

    LPDIRECTSOUNDBUFFER pDSB = GetFreeBuffer();

    if (pDSB == NULL)
        return DXTRACE_ERR(TEXT("GetFreeBuffer"), E_FAIL);

    if (FAILED(hr = RestoreBuffer(pDSB, &bRestored)))
        return DXTRACE_ERR(TEXT("RestoreBuffer"), hr);

    if (bRestored)
    {

        if (FAILED(hr = FillBufferWithSound(pDSB, FALSE)))
            return DXTRACE_ERR(TEXT("FillBufferWithSound"), hr);

        Reset();
    }

    this->m_dwIsFadingOut = 0;
    this->m_dwCurFadeoutProgress = 0;
    this->m_dwTotalFadeout = 0;

    return pDSB->Play(0, dwPriority, dwFlags);
}

HRESULT CSound::Stop()
{
    if (m_apDSBuffer == NULL)
        return CO_E_NOTINITIALIZED;

    HRESULT hr = 0;

    utils::DebugPrint2("CSound::Stop ");

    for (DWORD i = 0; i < m_dwNumBuffers; i++)
    {
        utils::DebugPrint2("%d ", i);
        hr |= m_apDSBuffer[i]->Stop();
    }

    utils::DebugPrint2("\n");

    this->m_dwIsFadingOut = 0;

    return hr;
}

HRESULT CSound::Reset()
{
    if (m_apDSBuffer == NULL)
        return CO_E_NOTINITIALIZED;

    HRESULT hr = 0;

    for (DWORD i = 0; i < m_dwNumBuffers; i++)
        hr |= m_apDSBuffer[i]->SetCurrentPosition(0);

    return hr;
}

CStreamingSound::CStreamingSound(LPDIRECTSOUNDBUFFER pDSBuffer, DWORD dwDSBufferSize, CWaveFile *pWaveFile,
                                 DWORD dwNotifySize)
    : CSound(&pDSBuffer, dwDSBufferSize, 1, pWaveFile)
{
    m_dwLastPlayPos = 0;
    m_dwPlayProgress = 0;
    m_dwNotifySize = dwNotifySize;
    m_dwNextWriteOffset = 0;
    m_bFillNextNotificationWithSilence = FALSE;
}

CStreamingSound::~CStreamingSound()
{
}

HRESULT CStreamingSound::UpdateFadeOut()
{
    if (this->m_dwIsFadingOut != 0)
    {
        this->m_dwCurFadeoutProgress = this->m_dwCurFadeoutProgress - 1;
        if (this->m_dwCurFadeoutProgress <= 0)
        {
            this->m_dwIsFadingOut = 0;
            this->m_apDSBuffer[0]->Stop();
            return 1;
        }

        int baseVol = 0;
        if (th06::g_CustomBgmVolume <= 0)
            baseVol = -10000;
        else if (th06::g_CustomBgmVolume < 100)
            baseVol = (100 - th06::g_CustomBgmVolume) * -40;

        int vol = ((this->m_dwCurFadeoutProgress * 5000) / this->m_dwTotalFadeout) - 5000;
        vol += baseVol;
        if (vol < -10000)
            vol = -10000;

        HRESULT res = this->m_apDSBuffer[0]->SetVolume(vol);
    }
    return 0;
}

HRESULT CStreamingSound::HandleWaveStreamNotification(BOOL bLoopedPlay)
{

    HRESULT hr;
    DWORD dwCurrentPlayPos;
    DWORD dwPlayDelta;
    DWORD dwBytesWrittenToBuffer;
    VOID *pDSLockedBuffer;
    VOID *pDSLockedBuffer2;
    DWORD dwDSLockedBufferSize;
    DWORD dwDSLockedBufferSize2;

    if (m_apDSBuffer == NULL || m_pWaveFile == NULL)
        return CO_E_NOTINITIALIZED;

    BOOL bRestored;
    if (FAILED(hr = RestoreBuffer(m_apDSBuffer[0], &bRestored)))
    {
        utils::DebugPrint2("error : RetoreBuffer in HandleWaveStreamNotification\n");
        return DXTRACE_ERR(TEXT("RestoreBuffer"), hr);
    }

    if (bRestored)
    {

        if (FAILED(hr = FillBufferWithSound(m_apDSBuffer[0], FALSE)))
        {
            utils::DebugPrint2("error : FillBufferWithSound in HandleWaveStreamNotification\n");
            return DXTRACE_ERR(TEXT("FillBufferWithSound"), hr);
        }
        return S_OK;
    }

    pDSLockedBuffer = NULL;
    pDSLockedBuffer2 = NULL;
    if (FAILED(hr = m_apDSBuffer[0]->Lock(m_dwNextWriteOffset, m_dwNotifySize, &pDSLockedBuffer, &dwDSLockedBufferSize,
                                          &pDSLockedBuffer2, &dwDSLockedBufferSize2, 0L)))
    {
        utils::DebugPrint2("error : Buffer->Lock in HandleWaveStreamNotification\n");
        return DXTRACE_ERR(TEXT("Lock"), hr);
    }

    if (pDSLockedBuffer2 != NULL)
        return E_UNEXPECTED;

    if (!m_bFillNextNotificationWithSilence)
    {

        int speed = (g_ReplaySpeed > 1 && g_GameManager.isInReplay && !g_GameManager.demoMode) ? g_ReplaySpeed : 1;
        if (speed > 1)
        {
            DWORD blockAlign = m_pWaveFile->m_pwfx->nBlockAlign;
            if (blockAlign == 0)
                blockAlign = 4;
            DWORD outFrames = dwDSLockedBufferSize / blockAlign;

            DWORD srcBytes = dwDSLockedBufferSize * (DWORD)speed;
            BYTE *scratch = (BYTE *)malloc(srcBytes);
            DWORD totalRead = 0;
            dwBytesWrittenToBuffer = 0;
            if (scratch != NULL)
            {

                while (totalRead < srcBytes)
                {
                    DWORD got = 0;
                    hr = m_pWaveFile->Read(scratch + totalRead, srcBytes - totalRead, &got);
                    if (FAILED(hr))
                    {
                        free(scratch);
                        return DXTRACE_ERR(TEXT("Read"), hr);
                    }
                    totalRead += got;
                    if (got == 0 || totalRead >= srcBytes)
                        break;

                    if (totalRead < srcBytes)
                        m_pWaveFile->ResetFile(true);
                }

                BYTE *dst = (BYTE *)pDSLockedBuffer;
                for (DWORD f = 0; f < outFrames; f++)
                {
                    DWORD srcFrame = f * (DWORD)speed + ((DWORD)speed - 1);
                    DWORD srcOff = srcFrame * blockAlign;
                    if (srcOff + blockAlign <= totalRead)
                        memcpy(dst + f * blockAlign, scratch + srcOff, blockAlign);
                    else
                        memset(dst + f * blockAlign, 0, blockAlign);
                }
                dwBytesWrittenToBuffer = outFrames * blockAlign;
                free(scratch);
            }
            else
            {

                hr = m_pWaveFile->Read((BYTE *)pDSLockedBuffer, dwDSLockedBufferSize, &dwBytesWrittenToBuffer);
                if (FAILED(hr))
                    return DXTRACE_ERR(TEXT("Read"), hr);
            }
        }
        else
        {

            if (FAILED(hr = m_pWaveFile->Read((BYTE *)pDSLockedBuffer, dwDSLockedBufferSize, &dwBytesWrittenToBuffer)))
            {
                utils::DebugPrint2("error : m_pWaveFile->Read in HandleWaveStreamNotification\n");
                return DXTRACE_ERR(TEXT("Read"), hr);
            }
        }
    }
    else
    {

        FillMemory(pDSLockedBuffer, dwDSLockedBufferSize, (BYTE)(m_pWaveFile->m_pwfx->wBitsPerSample == 8 ? 128 : 0));
        dwBytesWrittenToBuffer = dwDSLockedBufferSize;
    }

    if (dwBytesWrittenToBuffer < dwDSLockedBufferSize)
    {
        if (!bLoopedPlay)
        {

            FillMemory((BYTE *)pDSLockedBuffer + dwBytesWrittenToBuffer, dwDSLockedBufferSize - dwBytesWrittenToBuffer,
                       (BYTE)(m_pWaveFile->m_pwfx->wBitsPerSample == 8 ? 128 : 0));

            m_bFillNextNotificationWithSilence = TRUE;
        }
        else
        {

            DWORD dwReadSoFar = dwBytesWrittenToBuffer;
            while (dwReadSoFar < dwDSLockedBufferSize)
            {

                if (FAILED(hr = m_pWaveFile->ResetFile(true)))
                {
                    utils::DebugPrint2("error : m_pWaveFile->ResetFile in HandleWaveStreamNotification\n");
                    return DXTRACE_ERR(TEXT("ResetFile"), hr);
                }

                if (FAILED(hr = m_pWaveFile->Read((BYTE *)pDSLockedBuffer + dwReadSoFar,
                                                  dwDSLockedBufferSize - dwReadSoFar, &dwBytesWrittenToBuffer)))
                {
                    utils::DebugPrint2("error : m_pWaveFile->Read(+) in HandleWaveStreamNotification\n");
                    return DXTRACE_ERR(TEXT("Read"), hr);
                }

                dwReadSoFar += dwBytesWrittenToBuffer;
            }
        }
    }

    m_apDSBuffer[0]->Unlock(pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);

    if (FAILED(hr = m_apDSBuffer[0]->GetCurrentPosition(&dwCurrentPlayPos, NULL)))
    {
        utils::DebugPrint2("error : m_apDSBuffer[0]->GetCurrentPosition in HandleWaveStreamNotification\n");
        return DXTRACE_ERR(TEXT("GetCurrentPosition"), hr);
    }

    if (dwCurrentPlayPos < m_dwLastPlayPos)
        dwPlayDelta = (m_dwDSBufferSize - m_dwLastPlayPos) + dwCurrentPlayPos;
    else
        dwPlayDelta = dwCurrentPlayPos - m_dwLastPlayPos;

    {
        int speed = (g_ReplaySpeed > 1 && g_GameManager.isInReplay && !g_GameManager.demoMode) ? g_ReplaySpeed : 1;
        m_dwPlayProgress += dwPlayDelta * (DWORD)speed;
    }
    m_dwLastPlayPos = dwCurrentPlayPos;

    if (m_bFillNextNotificationWithSilence)
    {

        if (m_dwPlayProgress >= m_pWaveFile->GetSize())
        {
            m_apDSBuffer[0]->Stop();
        }
    }

    m_dwNextWriteOffset += dwDSLockedBufferSize;
    m_dwNextWriteOffset %= m_dwDSBufferSize;

    return S_OK;
}

HRESULT CStreamingSound::Reset()
{
    HRESULT hr;

    if (m_apDSBuffer[0] == NULL || m_pWaveFile == NULL)
        return CO_E_NOTINITIALIZED;

    m_dwLastPlayPos = 0;
    m_dwPlayProgress = 0;
    m_dwNextWriteOffset = 0;
    m_bFillNextNotificationWithSilence = FALSE;

    BOOL bRestored;
    if (FAILED(hr = RestoreBuffer(m_apDSBuffer[0], &bRestored)))
        return DXTRACE_ERR(TEXT("RestoreBuffer"), hr);

    if (bRestored)
    {

        if (FAILED(hr = FillBufferWithSound(m_apDSBuffer[0], FALSE)))
            return DXTRACE_ERR(TEXT("FillBufferWithSound"), hr);
    }

    m_pWaveFile->ResetFile(false);

    return m_apDSBuffer[0]->SetCurrentPosition(0L);
}

void CStreamingSound::SeekToByte(DWORD byteOffset)
{
    if (!m_pWaveFile || !m_apDSBuffer || !m_apDSBuffer[0])
        return;

    m_apDSBuffer[0]->Stop();

    m_pWaveFile->SeekToByte(byteOffset);

    int speed = (g_ReplaySpeed > 1 && g_GameManager.isInReplay && !g_GameManager.demoMode) ? g_ReplaySpeed : 1;

    i32 savedLoopStart = m_pWaveFile->m_loopStartPoint;
    m_pWaveFile->m_loopStartPoint = 0;
    VOID *pData;
    DWORD dwSize;
    if (SUCCEEDED(m_apDSBuffer[0]->Lock(0, m_dwDSBufferSize, &pData, &dwSize, NULL, NULL, DSBLOCK_ENTIREBUFFER)))
    {
        if (speed > 1)
        {

            DWORD blockAlign = m_pWaveFile->m_pwfx ? m_pWaveFile->m_pwfx->nBlockAlign : 4;
            if (blockAlign == 0)
                blockAlign = 4;
            DWORD outFrames = dwSize / blockAlign;
            DWORD srcBytes = dwSize * (DWORD)speed;
            BYTE *scratch = (BYTE *)malloc(srcBytes);
            if (scratch != NULL)
            {
                DWORD totalRead = 0;
                while (totalRead < srcBytes)
                {
                    DWORD got = 0;
                    m_pWaveFile->Read(scratch + totalRead, srcBytes - totalRead, &got);
                    totalRead += got;
                    if (got == 0 || totalRead >= srcBytes)
                        break;
                    m_pWaveFile->ResetFile(false);
                }
                BYTE *dst = (BYTE *)pData;
                for (DWORD f = 0; f < outFrames; f++)
                {
                    DWORD srcOff = (f * (DWORD)speed + ((DWORD)speed - 1)) * blockAlign;
                    if (srcOff + blockAlign <= totalRead)
                        memcpy(dst + f * blockAlign, scratch + srcOff, blockAlign);
                    else
                        memset(dst + f * blockAlign, 0, blockAlign);
                }
                free(scratch);
            }
            else
            {

                memset(pData, 0, dwSize);
            }
        }
        else
        {
            DWORD dwRead = 0;
            m_pWaveFile->Read((BYTE *)pData, dwSize, &dwRead);
            while (dwRead < dwSize)
            {
                m_pWaveFile->ResetFile(false);
                DWORD dwRead2 = 0;
                m_pWaveFile->Read((BYTE *)pData + dwRead, dwSize - dwRead, &dwRead2);
                if (dwRead2 == 0)
                {
                    memset((BYTE *)pData + dwRead, 0, dwSize - dwRead);
                    break;
                }
                dwRead += dwRead2;
            }
        }
        m_apDSBuffer[0]->Unlock(pData, dwSize, NULL, 0);
    }
    m_pWaveFile->m_loopStartPoint = savedLoopStart;

    m_apDSBuffer[0]->SetCurrentPosition(0L);
    m_dwLastPlayPos = 0;
    m_dwPlayProgress = byteOffset;
    m_dwNextWriteOffset = 0;
    m_bFillNextNotificationWithSilence = FALSE;
    m_apDSBuffer[0]->Play(0, 0, DSBPLAY_LOOPING);
}

CWaveFile::CWaveFile()
{
    m_pwfx = NULL;
    m_hmmio = NULL;
    m_dwSize = 0;
    m_bIsReadingFromMemory = FALSE;
    m_loopEndPoint = 0;
    m_loopStartPoint = 0;
}

CWaveFile::~CWaveFile()
{
    Close();

    if (!m_bIsReadingFromMemory)
        SAFE_DELETE_ARRAY(m_pwfx);
}

HRESULT CWaveFile::Open(LPTSTR strFileName, WAVEFORMATEX *pwfx, DWORD dwFlags)
{
    HRESULT hr;

    m_dwFlags = dwFlags;
    m_bIsReadingFromMemory = FALSE;

    if (m_dwFlags == WAVEFILE_READ)
    {
        if (strFileName == NULL)
            return E_INVALIDARG;
        SAFE_DELETE_ARRAY(m_pwfx);

        MMIOINFO mmioInfo;

        ZeroMemory(&mmioInfo, sizeof(mmioInfo));
        m_hmmio = mmioOpen(strFileName, &mmioInfo, MMIO_ALLOCBUF | MMIO_READ);
        if (NULL == m_hmmio)
        {
            switch (mmioInfo.wErrorRet)
            {
            case MMIOERR_PATHNOTFOUND:
                utils::DebugPrint2("The directory specification is incorrect. \n");
                break;
            case MMIOERR_ACCESSDENIED:
                utils::DebugPrint2("The file is protected and cannot be opened. \n");
                break;
            case MMIOERR_SHARINGVIOLATION:
                utils::DebugPrint2("The file is being used by another application and is unavailable. \n");
                break;
            case MMIOERR_TOOMANYOPENFILES:
                utils::DebugPrint2("too Meny Open Files \n");
                break;
            case MMIOERR_INVALIDFILE:
                utils::DebugPrint2(
                    "Another failure condition occurred. This is the default error for an open-file failure. \n");
                break;
            }
            utils::DebugPrint2("error : mmioOpen in CWaveFile::Open()\n");
            return E_FAIL;
        }

        if (FAILED(hr = ReadMMIO()))
        {

            mmioClose(m_hmmio, 0);
            utils::DebugPrint2("error : ReadOpen in CWaveFile::Open()\n");
            return E_FAIL;
        }

        if (FAILED(hr = ResetFile(false)))
        {
            utils::DebugPrint2("error : ResetFile in CWaveFile::Open()\n");
            return E_FAIL;
        }

        m_dwSize = m_ck.cksize;
    }
    return hr;
}

HRESULT CWaveFile::OpenFromMemory(BYTE *pbData, ULONG ulDataSize, WAVEFORMATEX *pwfx, DWORD dwFlags)
{
    m_pwfx = pwfx;
    m_ulDataSize = ulDataSize;
    m_pbData = pbData;
    m_pbDataCur = m_pbData;
    m_bIsReadingFromMemory = TRUE;

    if (dwFlags != WAVEFILE_READ)
        return E_NOTIMPL;

    return S_OK;
}

HRESULT CWaveFile::ReadMMIO()
{
    MMCKINFO ckIn;
    PCMWAVEFORMAT pcmWaveFormat;

    m_pwfx = NULL;

    if ((0 != mmioDescend(m_hmmio, &m_ckRiff, NULL, 0)))
        return DXTRACE_ERR(TEXT("mmioDescend"), E_FAIL);

    if ((m_ckRiff.ckid != FOURCC_RIFF) || (m_ckRiff.fccType != mmioFOURCC('W', 'A', 'V', 'E')))
        return DXTRACE_ERR_NOMSGBOX(TEXT("mmioFOURCC"), E_FAIL);

    ckIn.ckid = mmioFOURCC('f', 'm', 't', ' ');
    if (0 != mmioDescend(m_hmmio, &ckIn, &m_ckRiff, MMIO_FINDCHUNK))
        return DXTRACE_ERR(TEXT("mmioDescend"), E_FAIL);

    if (ckIn.cksize < (LONG)sizeof(PCMWAVEFORMAT))
        return DXTRACE_ERR(TEXT("sizeof(PCMWAVEFORMAT)"), E_FAIL);

    if (mmioRead(m_hmmio, (HPSTR)&pcmWaveFormat, sizeof(pcmWaveFormat)) != sizeof(pcmWaveFormat))
        return DXTRACE_ERR(TEXT("mmioRead"), E_FAIL);

    if (pcmWaveFormat.wf.wFormatTag == WAVE_FORMAT_PCM)
    {
        m_pwfx = (WAVEFORMATEX *)new CHAR[sizeof(WAVEFORMATEX)];
        if (NULL == m_pwfx)
            return DXTRACE_ERR(TEXT("m_pwfx"), E_FAIL);

        memcpy(m_pwfx, &pcmWaveFormat, sizeof(pcmWaveFormat));
        m_pwfx->cbSize = 0;
    }
    else
    {

        WORD cbExtraBytes = 0L;
        if (mmioRead(m_hmmio, (CHAR *)&cbExtraBytes, sizeof(WORD)) != sizeof(WORD))
            return DXTRACE_ERR(TEXT("mmioRead"), E_FAIL);

        m_pwfx = (WAVEFORMATEX *)new CHAR[sizeof(WAVEFORMATEX) + cbExtraBytes];
        if (NULL == m_pwfx)
            return DXTRACE_ERR(TEXT("new"), E_FAIL);

        memcpy(m_pwfx, &pcmWaveFormat, sizeof(pcmWaveFormat));
        m_pwfx->cbSize = cbExtraBytes;

        if (mmioRead(m_hmmio, (CHAR *)(((BYTE *)&(m_pwfx->cbSize)) + sizeof(WORD)), cbExtraBytes) != cbExtraBytes)
        {
            SAFE_DELETE_ARRAY(m_pwfx);
            return DXTRACE_ERR(TEXT("mmioRead"), E_FAIL);
        }
    }

    if (0 != mmioAscend(m_hmmio, &ckIn, 0))
    {
        SAFE_DELETE_ARRAY(m_pwfx);
        return DXTRACE_ERR(TEXT("mmioAscend"), E_FAIL);
    }

    return S_OK;
}

DWORD CWaveFile::GetSize()
{
    return m_dwSize;
}

HRESULT CWaveFile::ResetFile(bool loop)
{
    if (m_bIsReadingFromMemory)
    {
        m_pbDataCur = m_pbData;
    }
    else
    {
        if (m_hmmio == NULL)
        {
            utils::DebugPrint2("error : m_hmmio\t== NULL in CWaveFile::ResetFile\n");
            return CO_E_NOTINITIALIZED;
        }

        if (m_dwFlags == WAVEFILE_READ)
        {

            if (-1 == mmioSeek(m_hmmio, m_ckRiff.dwDataOffset + sizeof(FOURCC), SEEK_SET))
            {
                utils::DebugPrint2("error : mmioSeek in CWaveFile::ResetFile\n");
                return DXTRACE_ERR(TEXT("mmioSeek"), E_FAIL);
            }

            m_ck.ckid = mmioFOURCC('d', 'a', 't', 'a');
            MMRESULT res = mmioDescend(m_hmmio, &m_ck, &m_ckRiff, MMIO_FINDCHUNK);
            if (res != 0)
            {
                utils::DebugPrint2("error : mmioDescend in CWaveFile::ResetFile\n");
                return DXTRACE_ERR(TEXT("mmioDescend"), E_FAIL);
            }

            if (m_loopEndPoint > 0)
            {
                m_ck.cksize = m_loopEndPoint;
            }
            if (loop && this->m_loopStartPoint > 0)
            {

                if (mmioSeek(this->m_hmmio, this->m_loopStartPoint, SEEK_CUR) == -1)
                {
                    utils::DebugPrint2("error : mmioSeek loopStart in CWaveFile::ResetFile\n");
                    return E_FAIL;
                }
                this->m_ck.cksize = this->m_ck.cksize - this->m_loopStartPoint;
            }
        }
    }

    return S_OK;
}

HRESULT CWaveFile::Read(BYTE *pBuffer, DWORD dwSizeToRead, DWORD *pdwSizeRead)
{
    if (m_bIsReadingFromMemory)
    {
        if (m_pbDataCur == NULL)
            return CO_E_NOTINITIALIZED;
        if (pdwSizeRead != NULL)
            *pdwSizeRead = 0;

        if ((BYTE *)(m_pbDataCur + dwSizeToRead) > (BYTE *)(m_pbData + m_ulDataSize))
        {
            dwSizeToRead = m_ulDataSize - (DWORD)(m_pbDataCur - m_pbData);
        }

        CopyMemory(pBuffer, m_pbDataCur, dwSizeToRead);

        if (pdwSizeRead != NULL)
            *pdwSizeRead = dwSizeToRead;

        return S_OK;
    }
    else
    {
        MMIOINFO mmioinfoIn;

        if (m_hmmio == NULL)
            return CO_E_NOTINITIALIZED;
        if (pBuffer == NULL || pdwSizeRead == NULL)
            return E_INVALIDARG;

        if (pdwSizeRead != NULL)
            *pdwSizeRead = 0;

        if (0 != mmioGetInfo(m_hmmio, &mmioinfoIn, 0))
        {
            utils::DebugPrint2("error :\t%s(%s)\n", __FILE__, 1060);
            return DXTRACE_ERR(TEXT("mmioGetInfo"), E_FAIL);
        }

        UINT cbDataIn = dwSizeToRead;
        if (cbDataIn > m_ck.cksize)
            cbDataIn = m_ck.cksize;

        m_ck.cksize -= cbDataIn;

        for (DWORD cT = 0; cT < cbDataIn; cT++)
        {

            if (mmioinfoIn.pchNext == mmioinfoIn.pchEndRead)
            {
                if (0 != mmioAdvance(m_hmmio, &mmioinfoIn, MMIO_READ))
                {

                    utils::DebugPrint2("error :\t%s(%s)\n", __FILE__, 1075);
                    return DXTRACE_ERR(TEXT("mmioAdvance"), E_FAIL);
                }

                if (mmioinfoIn.pchNext == mmioinfoIn.pchEndRead)
                {

                    utils::DebugPrint2("error :\t%s(%s)\n", __FILE__, 1079);
                    return DXTRACE_ERR(TEXT("mmioinfoIn.pchNext"), E_FAIL);
                }
            }

            *((BYTE *)pBuffer + cT) = *((BYTE *)mmioinfoIn.pchNext);
            mmioinfoIn.pchNext++;
        }

        if (0 != mmioSetInfo(m_hmmio, &mmioinfoIn, 0))
        {
            utils::DebugPrint2("error :\t%s(%s)\n", __FILE__, 1088);
            return DXTRACE_ERR(TEXT("mmioSetInfo"), E_FAIL);
        }

        if (pdwSizeRead != NULL)
            *pdwSizeRead = cbDataIn;

        return S_OK;
    }
}

HRESULT CWaveFile::Close()
{
    if (m_dwFlags == WAVEFILE_READ)
    {
        mmioClose(m_hmmio, 0);
        m_hmmio = NULL;
    }
    return S_OK;
}
}; // namespace th06
