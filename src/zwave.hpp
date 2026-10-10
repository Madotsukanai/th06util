

#ifndef DSUTIL_H
#define DSUTIL_H

#include "inttypes.hpp"
#include <dsound.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <windows.h>

namespace th06
{

class CSoundManager;
class CSound;
class CStreamingSound;
class CWaveFile;

#define WAVEFILE_READ 1
#define WAVEFILE_WRITE 2

#define DSUtil_StopSound(s)                                                                                            \
    {                                                                                                                  \
        if (s)                                                                                                         \
            s->Stop();                                                                                                 \
    }
#define DSUtil_PlaySound(s)                                                                                            \
    {                                                                                                                  \
        if (s)                                                                                                         \
            s->Play(0, 0);                                                                                             \
    }
#define DSUtil_PlaySoundLooping(s)                                                                                     \
    {                                                                                                                  \
        if (s)                                                                                                         \
            s->Play(0, DSBPLAY_LOOPING);                                                                               \
    }

class CSoundManager
{
  protected:
    LPDIRECTSOUND8 m_pDS;

  public:
    CSoundManager();
    ~CSoundManager();

    HRESULT Initialize(HWND hWnd, DWORD dwCoopLevel, DWORD dwPrimaryChannels, DWORD dwPrimaryFreq,
                       DWORD dwPrimaryBitRate);
    inline LPDIRECTSOUND GetDirectSound()
    {
        return m_pDS;
    }
    HRESULT SetPrimaryBufferFormat(DWORD dwPrimaryChannels, DWORD dwPrimaryFreq, DWORD dwPrimaryBitRate);

    HRESULT CreateStreaming(CStreamingSound **ppStreamingSound, LPTSTR strWaveFileName, DWORD dwCreationFlags,
                            GUID guid3DAlgorithm, DWORD dwNotifyCount, DWORD dwNotifySize, HANDLE hNotifyEvent);
};

class CSound
{
  protected:
    LPDIRECTSOUNDBUFFER *m_apDSBuffer;
    DWORD m_dwDSBufferSize;

  public:
    CWaveFile *m_pWaveFile;

  protected:
    DWORD m_dwNumBuffers;

  public:
    i32 m_dwCurFadeoutProgress;
    i32 m_dwTotalFadeout;
    DWORD m_dwIsFadingOut;

  protected:
    HRESULT RestoreBuffer(LPDIRECTSOUNDBUFFER pDSB, BOOL *pbWasRestored);

  public:
    CSound(LPDIRECTSOUNDBUFFER *apDSBuffer, DWORD dwDSBufferSize, DWORD dwNumBuffers, CWaveFile *pWaveFile);
    virtual ~CSound();

    HRESULT FillBufferWithSound(LPDIRECTSOUNDBUFFER pDSB, BOOL bRepeatWavIfBufferLarger);
    LPDIRECTSOUNDBUFFER GetFreeBuffer();
    LPDIRECTSOUNDBUFFER GetBuffer(DWORD dwIndex);

    HRESULT Play(DWORD dwPriority, DWORD dwFlags);
    HRESULT Stop();
    HRESULT Reset();
};

class CStreamingSound : public CSound
{
  protected:
    DWORD m_dwLastPlayPos;
    DWORD m_dwPlayProgress;
    DWORD m_dwNotifySize;
    DWORD m_dwNextWriteOffset;
    BOOL m_bFillNextNotificationWithSilence;

  public:
    DWORD GetPlayProgress() const
    {
        return m_dwPlayProgress;
    }

    CStreamingSound(LPDIRECTSOUNDBUFFER pDSBuffer, DWORD dwDSBufferSize, CWaveFile *pWaveFile, DWORD dwNotifySize);
    ~CStreamingSound();

    HRESULT UpdateFadeOut();
    HRESULT HandleWaveStreamNotification(BOOL bLoopedPlay);
    HRESULT Reset();
    void SeekToByte(DWORD byteOffset);
};

class CWaveFile
{
  public:
    WAVEFORMATEX *m_pwfx;
    HMMIO m_hmmio;
    MMCKINFO m_ck;
    MMCKINFO m_ckRiff;
    DWORD m_dwSize;
    MMIOINFO m_mmioinfoOut;
    DWORD m_dwFlags;
    BOOL m_bIsReadingFromMemory;
    BYTE *m_pbData;
    BYTE *m_pbDataCur;
    ULONG m_ulDataSize;

    i32 m_loopStartPoint;

    i32 m_loopEndPoint;

  protected:
    HRESULT ReadMMIO();

  public:
    CWaveFile();
    ~CWaveFile();

    HRESULT Open(LPTSTR strFileName, WAVEFORMATEX *pwfx, DWORD dwFlags);
    HRESULT OpenFromMemory(BYTE *pbData, ULONG ulDataSize, WAVEFORMATEX *pwfx, DWORD dwFlags);
    HRESULT Close();

    HRESULT Read(BYTE *pBuffer, DWORD dwSizeToRead, DWORD *pdwSizeRead);

    DWORD GetSize();
    HRESULT ResetFile(bool loop);
    void SeekToByte(DWORD byteOffset)
    {
        DWORD savedSize = m_dwSize;
        ResetFile(false);
        m_dwSize = savedSize;
        if (byteOffset > m_ck.cksize)
            byteOffset = m_ck.cksize;
        m_ck.cksize -= byteOffset;
        if (!m_bIsReadingFromMemory && m_hmmio)
            mmioSeek(m_hmmio, (LONG)byteOffset, SEEK_CUR);
        else if (m_bIsReadingFromMemory && m_pbData)
            m_pbDataCur = m_pbData + byteOffset;
    }
    WAVEFORMATEX *GetFormat()
    {
        return m_pwfx;
    };
};
}; // namespace th06

#endif
