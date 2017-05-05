#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <vector>
#include <deque>
#include <map>
#include <string>
#include <dlfcn.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include "DeckLinkAPI.h"

#include "common.h"

namespace {

    static const unsigned int bmd_display_mode[][5] = {
        {720,  486,  bmdModeNTSC,        30000, 1001},
        {720,  576,  bmdModePAL,         25000, 1000},

        {1920, 1080, bmdModeHD1080p2398, 24000, 1001},
        {1920, 1080, bmdModeHD1080p24,   24000, 1000},
        {1920, 1080, bmdModeHD1080p25,   25000, 1000},
        {1920, 1080, bmdModeHD1080p2997, 30000, 1001},
        {1920, 1080, bmdModeHD1080p30,   30000, 1000},
        {1920, 1080, bmdModeHD1080i50,   50000, 1000},
        {1920, 1080, bmdModeHD1080i5994, 60000, 1001},
        {1920, 1080, bmdModeHD1080i6000, 60000, 1000},
        {1920, 1080, bmdModeHD1080p50,   50000, 1000},
        {1920, 1080, bmdModeHD1080p5994, 60000, 1001},
        {1920, 1080, bmdModeHD1080p6000, 60000, 1000},

        {1280, 720,  bmdModeHD720p50,    50000, 1000},
        {1280, 720,  bmdModeHD720p5994,  60000, 1001},
        {1280, 720,  bmdModeHD720p60,    60000, 1000},

        {0}
    };

    void *lib_api = NULL;

    void load_lib_api(void)
    {
#ifdef PATH_A
        if (lib_api == NULL) {
            lib_api = dlopen(PATH_A, RTLD_NOW | RTLD_GLOBAL);
        }
#endif // PATH_A
    }

}

class SoundDeckLinkDisplayMode :
    public IDeckLinkDisplayMode {
protected:
    long _width;
    long _height;
    std::string _name;
    BMDDisplayMode _display_mode;
    std::pair<BMDTimeValue, BMDTimeScale> _frame_rate;
    BMDFieldDominance _field_dominance;
    BMDDisplayModeFlags _flags;
public:
    DUMMY_IUNKNOWN;
    SoundDeckLinkDisplayMode(long width, long height,
                              std::string name,
                              BMDDisplayMode display_mode,
                              std::pair<BMDTimeValue, BMDTimeScale>
                              frame_rate,
                              BMDFieldDominance field_dominance,
                              BMDDisplayModeFlags flags)
        : _width(width), _height(height), _name(name),
          _display_mode(display_mode), _frame_rate(frame_rate),
          _field_dominance(field_dominance), _flags(flags)
    {
    }
    HRESULT GetName(const char **name)
    {
        *name = strdup(_name.c_str());
        return S_OK;
    }
    BMDDisplayMode GetDisplayMode(void)
    {
        return _display_mode;
    }
    long GetWidth(void)
    {
        return _width;
    }
    long GetHeight(void)
    {
        return _height;
    }
    HRESULT GetFrameRate(BMDTimeValue *frameDuration,
                         BMDTimeScale *timeScale)
    {
        if (frameDuration != NULL && timeScale != NULL) {
            *frameDuration = _frame_rate.first;
            *timeScale = _frame_rate.second;
            return S_OK;
        }
        else {
            return E_FAIL;
        }
    }
    BMDFieldDominance GetFieldDominance(void)
    {
        return _field_dominance;
    }
    BMDDisplayModeFlags GetFlags(void)
    {
        return _flags;
    }
};

class SoundDeckLinkDisplayModeIterator :
    public IDeckLinkDisplayModeIterator {
protected:
    std::vector<SoundDeckLinkDisplayMode> _display_mode;
    std::vector<SoundDeckLinkDisplayMode>::iterator _iterator;
public:
    DUMMY_IUNKNOWN;
    SoundDeckLinkDisplayModeIterator(bool not_empty = true)
    {
        if (not_empty) {
        for (const unsigned int (*p)[5] = bmd_display_mode;
             (*p)[0] != 0; p++) {
            const BMDFieldDominance field_dominance =
                (*p)[2] == bmdModeNTSC ||
                (*p)[2] == bmdModeNTSC2398 ||
                (*p)[2] == bmdModePAL ?
                bmdLowerFieldFirst :
                (*p)[2] == bmdModeHD1080i50 ||
                (*p)[2] == bmdModeHD1080i5994 ||
                (*p)[2] == bmdModeHD1080i6000 ?
                bmdUpperFieldFirst :
                bmdProgressiveFrame;

            char name[11];

            if ((*p)[2] == bmdModeNTSC) {
                snprintf(name, 11, "NTSC");
            }
            else if ((*p)[2] == bmdModePAL) {
                snprintf(name, 11, "PAL");
            }
            else {
                snprintf(name, 11, "%d%c%.4g", (*p)[1],
                         field_dominance == bmdProgressiveFrame ?
                         'p' : 'i',
                         static_cast<double>((*p)[3]) /
                         static_cast<double>((*p)[4]));
            }
            _display_mode.push_back(SoundDeckLinkDisplayMode(
                (*p)[0], (*p)[1], name, (*p)[2],
                std::pair<BMDTimeValue, BMDTimeScale>
                ((*p)[4], (*p)[3]),
                field_dominance,
                (*p)[2] == bmdModeNTSC ||
                (*p)[2] == bmdModeNTSC2398 ||
                (*p)[2] == bmdModeNTSCp ||
                (*p)[2] == bmdModePAL ||
                (*p)[2] == bmdModePALp ?
                bmdDisplayModeColorspaceRec601 :
                bmdDisplayModeColorspaceRec709));
        }
        }
        _iterator = _display_mode.begin();
    }
    HRESULT Next(IDeckLinkDisplayMode **deckLinkDisplayMode)
    {
        if (_iterator != _display_mode.end()) {
            *deckLinkDisplayMode =
                new SoundDeckLinkDisplayMode(*_iterator);
            _iterator++;
            return S_OK;
        }
        else {
            return E_FAIL;
        }
    }
};

class SoundDeckLinkAttributes : public IDeckLinkAttributes {
protected:
    std::map<BMDDeckLinkAttributeID, bool> _flag;
    std::map<BMDDeckLinkAttributeID, int64_t> _int;
    std::map<BMDDeckLinkAttributeID, double> _float;
    std::map<BMDDeckLinkAttributeID, std::string> _string;
public:
    DUMMY_IUNKNOWN;
    SoundDeckLinkAttributes
    (bool supports_internal_keying = false,
     bool supports_external_keying = false,
     bool supports_hd_keying = true,
     // Set to 16, since Resolve will ask for 16 channels regardless
     int64_t maximum_audio_channels = 16,
     bool supports_input_format_detection = true,
     bool has_serial_port = false,
     int64_t number_of_subdevices = 1,
     int64_t subdevice_index = 0,
     bool supports_full_duplex = true,
     int64_t persistent_id = 0,
     int64_t topological_id = 0,
     int64_t video_output_connections = (1 << 6) - 1,
     int64_t audio_output_connections = (1 << 5) - 1,
     int64_t video_input_connections = 0,
     int64_t audio_input_connections = 0)
        : IDeckLinkAttributes()
    {
        _int[BMDDeckLinkMaximumAudioChannels] =
            maximum_audio_channels;
        _flag[BMDDeckLinkSupportsInternalKeying] =
            supports_internal_keying;
        _flag[BMDDeckLinkSupportsExternalKeying] =
            supports_external_keying;
        _flag[BMDDeckLinkSupportsHDKeying] = supports_hd_keying;
        _int[BMDDeckLinkMaximumAudioChannels] =
            maximum_audio_channels;
        _flag[BMDDeckLinkSupportsInputFormatDetection] =
            supports_input_format_detection;
        _flag[BMDDeckLinkHasSerialPort] = has_serial_port;
        _int[BMDDeckLinkNumberOfSubDevices] = number_of_subdevices;
        _int[BMDDeckLinkSubDeviceIndex] = subdevice_index;
        _flag[BMDDeckLinkSupportsFullDuplex] = supports_full_duplex;
        _int[BMDDeckLinkPersistentID] = persistent_id;
        _int[BMDDeckLinkTopologicalID] = topological_id;
        _int[BMDDeckLinkVideoOutputConnections] =
            video_output_connections;
        _int[BMDDeckLinkAudioOutputConnections] =
            audio_output_connections;
        _int[BMDDeckLinkVideoInputConnections] =
            video_input_connections;
        _int[BMDDeckLinkAudioInputConnections] =
            audio_input_connections;
    }

    HRESULT GetFlag(BMDDeckLinkAttributeID cfgID, bool *value)
    {
        if (_flag.find(cfgID) != _flag.end()) {
            *value = _flag[cfgID];
            return S_OK;
        }
        else {
            return E_INVALIDARG;
        }
    }
    HRESULT GetInt(BMDDeckLinkAttributeID cfgID, int64_t *value)
    {
        if (_int.find(cfgID) != _int.end()) {
            *value = _int[cfgID];
            return S_OK;
        }
        else {
            return E_INVALIDARG;
        }
    }
    HRESULT GetFloat(BMDDeckLinkAttributeID cfgID,
                     double *value)
    {
        if (_float.find(cfgID) != _float.end()) {
            *value = _float[cfgID];
            return S_OK;
        }
        else {
            return E_INVALIDARG;
        }
    }
    HRESULT GetString(BMDDeckLinkAttributeID cfgID,
                      const char **value)
    {
        if (_string.find(cfgID) != _string.end()) {
            *value = _string[cfgID].c_str();
            return S_OK;
        }
        else {
            return E_INVALIDARG;
        }
    }
};

class SoundDeckLinkOutput : public IDeckLinkOutput {
protected:
    class callback_arg_t {
    public:
        SoundDeckLinkOutput *_this;
        pthread_cond_t _cond;
        pthread_mutex_t _mutex;
        bool _stop;
        callback_arg_t(SoundDeckLinkOutput *this_)
            : _this(this_), _stop(false)
        {
            pthread_cond_init(&_cond, NULL);
            pthread_mutex_init(&_mutex, NULL);
        }
    };
    std::pair<BMDTimeValue, BMDTimeScale> _frame_rate;
    IDeckLinkVideoOutputCallback *_frame_completion;
    IDeckLinkScreenPreviewCallback *_screen_preview;
    std::deque<std::pair<BMDTimeValue, IDeckLinkVideoFrame *> > _frame_buffer;
    IDeckLinkMemoryAllocator *_allocator;
    callback_arg_t _callback_arg;
    pthread_t _callback_thread;
    bool _callback_thread_alive;
    struct timespec _playback_start;
    size_t _channel_count;
    size_t _channel_count_physical;
    size_t _sample_width_byte;
    std::string _alsa_device;
    snd_pcm_t *_alsa_pcm;
    snd_pcm_hw_params_t *_alsa_hw_params;
    static void *callback_thread(void *arg)
    {
        class callback_arg_t *c =
            reinterpret_cast<class callback_arg_t *>(arg);

        while (true) {
            pthread_mutex_lock(&c->_mutex);

            if (c->_stop) {
                while (!c->_this->_frame_buffer.empty()) {
                    c->_this->_frame_buffer.front().second->
                        Release();
                    c->_this->_frame_buffer.pop_front();
                }
                pthread_mutex_unlock(&c->_mutex);
                return NULL;
            }

            if (c->_this->_frame_completion != NULL &&
                c->_this->_frame_buffer.size() >= 2 &&
                c->_this->_frame_buffer.front().first <
                rint(c->_this->time_elapsed
                     (c->_this->_playback_start,
                      c->_this->_frame_rate.second))) {
                c->_this->_frame_completion->
                    ScheduledFrameCompleted
                    (c->_this->_frame_buffer.front().second,
                     bmdOutputFrameCompleted);
                c->_this->_frame_buffer.front().second->Release();
                c->_this->_frame_buffer.pop_front();
            }

            if (c->_this->_screen_preview != NULL &&
                c->_this->_frame_buffer.size() >= 1) {
                c->_this->_screen_preview->DrawFrame
                    (c->_this->_frame_buffer.front().second);
            }

            struct timespec abstime;

            clock_gettime(CLOCK_REALTIME, &abstime);

            double frame_ns = 1e+9 * c->_this->_frame_rate.first /
                c->_this->_frame_rate.second;

            while (abstime.tv_nsec + frame_ns >= 1e+9) {
                abstime.tv_sec++;
                frame_ns -= 1e+9;
            }
            abstime.tv_nsec += frame_ns;
            pthread_cond_timedwait(&c->_cond, &c->_mutex, &abstime);
            pthread_mutex_unlock(&c->_mutex);
        };

        return NULL;
    }
    double time_elapsed(struct timespec start,
                        BMDTimeScale time_scale)
    {
        struct timespec current;

        clock_gettime(CLOCK_REALTIME, &current);

        const double dsec = static_cast<double>(current.tv_sec) -
            static_cast<double>(start.tv_sec);
        const double dnsec = static_cast<double>(current.tv_nsec) -
            static_cast<double>(start.tv_nsec);

        return (dsec + dnsec / 1e+9) * time_scale;
    }
    uint32_t alsa_write(void *buffer, uint32_t sample_frame_count)
    {
        if (_alsa_pcm == NULL) {
            return 0;
        }

        const snd_pcm_channel_area_t *area;
        snd_pcm_uframes_t offset;

        if (snd_pcm_state(_alsa_pcm) == SND_PCM_STATE_XRUN) {
            snd_pcm_prepare(_alsa_pcm);
        }

        const size_t channel_step = _channel_count * _sample_width_byte;
        const size_t channel_step_physical =
            _channel_count_physical * _sample_width_byte;

        int alsa_status = 0;

        unsigned char *o =
            new unsigned char[channel_step * sample_frame_count];

        for (uint32_t i = 0; i < sample_frame_count; i++) {
            memcpy(o + i * channel_step_physical,
                   reinterpret_cast<unsigned char *>(buffer) +
                   i * channel_step, channel_step_physical);
        }

        alsa_status = snd_pcm_writei(_alsa_pcm, o, sample_frame_count);

        if (alsa_status < 0) {
            snd_pcm_prepare(_alsa_pcm);
            alsa_status =
                snd_pcm_writei(_alsa_pcm, o, sample_frame_count);
        }

        delete [] o;

        return alsa_status > 0 ? alsa_status : 0;
    }
public:
    DUMMY_IUNKNOWN;
    SoundDeckLinkOutput(IDeckLinkOutput *forward = NULL)
        : _frame_completion(NULL),
          _screen_preview(NULL), _allocator(NULL),
          _callback_arg(this), _callback_thread_alive(false),
          _channel_count(0), _channel_count_physical(0),
          _sample_width_byte(0), _alsa_pcm(NULL)
    {
    }
    SoundDeckLinkOutput(std::string alsa_device)
        : _frame_completion(NULL),
          _screen_preview(NULL), _allocator(NULL),
          _callback_arg(this), _callback_thread_alive(false),
          _channel_count(0), _channel_count_physical(0),
          _sample_width_byte(0),
          _alsa_device(alsa_device), _alsa_pcm(NULL)
    {
    }
    ~SoundDeckLinkOutput()
    {
        if (_callback_thread_alive) {
            pthread_mutex_lock(&_callback_arg._mutex);
            _callback_arg._stop = true;
            pthread_mutex_unlock(&_callback_arg._mutex);
        }
        if (_alsa_pcm != NULL) {
            snd_pcm_close(_alsa_pcm);
        }
    }
    HRESULT DoesSupportVideoMode(BMDDisplayMode displayMode,
                                 BMDPixelFormat pixelFormat,
                                 BMDVideoOutputFlags flags,
                                 BMDDisplayModeSupport *result,
                                 IDeckLinkDisplayMode **
                                 resultDisplayMode)
    {
        return S_OK;
    }
    HRESULT GetDisplayModeIterator(IDeckLinkDisplayModeIterator **
                                   iterator)
    {
        *iterator = new SoundDeckLinkDisplayModeIterator();
        return S_OK;
    }
    HRESULT SetScreenPreviewCallback(IDeckLinkScreenPreviewCallback *
                                     previewCallback)
    {
        _screen_preview = previewCallback;
        return S_OK;
    }
    HRESULT EnableVideoOutput(BMDDisplayMode displayMode,
                              BMDVideoOutputFlags flags)
    {
        clock_gettime(CLOCK_REALTIME, &_playback_start);
        for (const unsigned int (*p)[5] = bmd_display_mode;
             (*p)[0] != 0; p++) {
            if ((*p)[2] == displayMode) {
                _frame_rate = std::pair<BMDTimeValue, BMDTimeScale>
                    ((*p)[4], (*p)[3]);
                return S_OK;
            }
        }
        return E_FAIL;
    }
    HRESULT DisableVideoOutput(void)
    {
        return S_OK;
    }
    HRESULT
    SetVideoOutputFrameMemoryAllocator(IDeckLinkMemoryAllocator *
                                       theAllocator)
    {
        _allocator = theAllocator;
        return S_OK;
    }
    HRESULT
    CreateVideoFrame(int32_t width, int32_t height, int32_t rowBytes,
                     BMDPixelFormat pixelFormat, BMDFrameFlags flags,
                     IDeckLinkMutableVideoFrame **outFrame)
    {
        return E_FAIL;
    }
    HRESULT CreateAncillaryData(BMDPixelFormat pixelFormat,
                                IDeckLinkVideoFrameAncillary **
                                outBuffer)
    {
        return E_FAIL;
    }
    HRESULT DisplayVideoFrameSync(IDeckLinkVideoFrame *theFrame)
    {
        pthread_mutex_lock(&_callback_arg._mutex);

        BMDTimeValue display_time = 0;

        if (!_frame_buffer.empty()) {
            display_time = _frame_buffer.front().first;
            _frame_buffer.front().second->Release();
            _frame_buffer.pop_front();
        }
        theFrame->AddRef();
        _frame_buffer.push_front
            (std::pair<BMDTimeValue, IDeckLinkVideoFrame *>
             (display_time, theFrame));
        pthread_mutex_unlock(&_callback_arg._mutex);

        return S_OK;
    }
    HRESULT ScheduleVideoFrame(IDeckLinkVideoFrame *theFrame,
                               BMDTimeValue displayTime,
                               BMDTimeValue displayDuration,
                               BMDTimeScale timeScale)
    {
        if (_frame_completion != NULL || _screen_preview != NULL) {
            pthread_mutex_lock(&_callback_arg._mutex);
            // This is needed to prevent segfault from the caller
            // deallocating the frame while in our frame buffer
            theFrame->AddRef();
            _frame_buffer.push_back
                (std::pair<BMDTimeValue, IDeckLinkVideoFrame *>
                 (displayTime + displayDuration, theFrame));
            pthread_mutex_unlock(&_callback_arg._mutex);
        }
        return S_OK;
    }
    HRESULT
    SetScheduledFrameCompletionCallback(IDeckLinkVideoOutputCallback *
                                        theCallback)
    {
        _frame_completion = theCallback;
        return S_OK;
    }
    HRESULT GetBufferedVideoFrameCount(uint32_t *bufferedFrameCount)
    {
        pthread_mutex_lock(&_callback_arg._mutex);
        *bufferedFrameCount = _frame_buffer.size();
        pthread_mutex_unlock(&_callback_arg._mutex);
        return S_OK;
    }
    HRESULT EnableAudioOutput(BMDAudioSampleRate sampleRate,
                              BMDAudioSampleType sampleType,
                              uint32_t channelCount,
                              BMDAudioOutputStreamType streamType)
    {
        _channel_count = channelCount;

        int alsa_status =
            snd_pcm_open(&_alsa_pcm, _alsa_device.c_str(),
                         SND_PCM_STREAM_PLAYBACK, 0);

        if (_alsa_pcm == NULL) {
            return E_FAIL;
        }

        snd_pcm_hw_params_alloca(&_alsa_hw_params);
        alsa_status = snd_pcm_hw_params_any(_alsa_pcm,
                                            _alsa_hw_params);

        if (alsa_status != 0) {
            return E_FAIL;
        }

        alsa_status = snd_pcm_hw_params_set_access
            (_alsa_pcm, _alsa_hw_params,
             SND_PCM_ACCESS_RW_INTERLEAVED);

        if (alsa_status != 0) {
            return E_FAIL;
        }

        alsa_status =
            snd_pcm_hw_params_set_rate_near(_alsa_pcm,
                                            _alsa_hw_params,
                                            &sampleRate, NULL);

        if (alsa_status != 0) {
            return E_FAIL;
        }

        switch (sampleType) {
        case bmdAudioSampleType16bitInteger:
            alsa_status = snd_pcm_hw_params_set_format
                (_alsa_pcm, _alsa_hw_params, SND_PCM_FORMAT_S16_LE);
            _sample_width_byte = 2;
            break;
        case bmdAudioSampleType32bitInteger:
            alsa_status = snd_pcm_hw_params_set_format
                (_alsa_pcm, _alsa_hw_params, SND_PCM_FORMAT_S32_LE);
            _sample_width_byte = 4;
            break;
        default:
            return E_FAIL;
        }

        if (alsa_status != 0) {
            return E_FAIL;
        }

        for (unsigned int c = channelCount; c > 0; c--) {
            alsa_status =
                snd_pcm_hw_params_set_channels(_alsa_pcm,
                                               _alsa_hw_params,
                                               c);
            if (alsa_status == 0) {
                _channel_count_physical = c;
                break;
            }
        }
        alsa_status = snd_pcm_hw_params(_alsa_pcm, _alsa_hw_params);

        if (alsa_status != 0) {
            return E_FAIL;
        }

        return S_OK;
    }
    HRESULT DisableAudioOutput(void)
    {
        if (_alsa_pcm == NULL) {
            return E_FAIL;
        }
        snd_pcm_drain(_alsa_pcm);
        snd_pcm_close(_alsa_pcm);
        return S_OK;
    }
    HRESULT WriteAudioSamplesSync(void *buffer,
                                  uint32_t sampleFrameCount,
                                  uint32_t *sampleFramesWritten)
    {
        *sampleFramesWritten = alsa_write(buffer, sampleFrameCount);
        return S_OK;
    }
    HRESULT BeginAudioPreroll(void)
    {
        return S_OK;
    }
    HRESULT EndAudioPreroll(void)
    {
        return S_OK;
    }
    HRESULT ScheduleAudioSamples(void *buffer,
                                 uint32_t sampleFrameCount,
                                 BMDTimeValue streamTime,
                                 BMDTimeScale timeScale,
                                 uint32_t *sampleFramesWritten)
    {
        *sampleFramesWritten = alsa_write(buffer, sampleFrameCount);
        return S_OK;
    }
    HRESULT
    GetBufferedAudioSampleFrameCount(uint32_t *
                                     bufferedSampleFrameCount)
    {
        return E_FAIL;
    }
    HRESULT FlushBufferedAudioSamples(void)
    {
        return E_FAIL;
    }
    HRESULT SetAudioCallback(IDeckLinkAudioOutputCallback *
                             theCallback)
    {
        return S_OK;
    }
    HRESULT StartScheduledPlayback(BMDTimeValue playbackStartTime,
                                   BMDTimeScale timeScale,
                                   double playbackSpeed)
    {
        if (!_callback_thread_alive) {
            pthread_create(&_callback_thread, NULL,
                           &SoundDeckLinkOutput::callback_thread,
                           &_callback_arg);
            _callback_thread_alive = true;
        }
        else if (_alsa_pcm != NULL) {
            snd_pcm_prepare(_alsa_pcm);
        }
        return S_OK;
    }
    HRESULT StopScheduledPlayback(BMDTimeValue stopPlaybackAtTime,
                                  BMDTimeValue *actualStopTime,
                                  BMDTimeScale timeScale)
    {
        if (_alsa_pcm != NULL) {
            snd_pcm_drain(_alsa_pcm);
            snd_pcm_drop(_alsa_pcm);
        }
        return S_OK;
    }
    HRESULT IsScheduledPlaybackRunning(bool *active)
    {
        *active = true;
        return S_OK;
    }
    HRESULT GetScheduledStreamTime(BMDTimeScale desiredTimeScale,
                                   BMDTimeValue *streamTime,
                                   double *playbackSpeed)
    {
        *streamTime =
            rint(time_elapsed(_playback_start, desiredTimeScale));
        return S_OK;
    }
    HRESULT GetReferenceStatus(BMDReferenceStatus *referenceStatus)
    {
        return E_FAIL;
    }
    HRESULT GetHardwareReferenceClock(BMDTimeScale desiredTimeScale,
                                      BMDTimeValue *hardwareTime,
                                      BMDTimeValue *timeInFrame,
                                      BMDTimeValue *ticksPerFrame)
    {
        return E_FAIL;
    }
    HRESULT
    GetFrameCompletionReferenceTimestamp(IDeckLinkVideoFrame *
                                         theFrame,
                                         BMDTimeScale
                                         desiredTimeScale,
                                         BMDTimeValue *
                                         frameCompletionTimestamp)
    {
        return E_FAIL;
    }
};

class SoundDeckLinkConfiguration : public IDeckLinkConfiguration {
public:
    DUMMY_IUNKNOWN;
    HRESULT SetFlag(BMDDeckLinkConfigurationID cfgID, bool value)
    {
        return S_OK;
    }
    HRESULT GetFlag(BMDDeckLinkConfigurationID cfgID, bool *value)
    {
        return S_OK;
    }
    HRESULT SetInt(BMDDeckLinkConfigurationID cfgID, int64_t value)
    {
        return S_OK;
    }
    HRESULT GetInt(BMDDeckLinkConfigurationID cfgID, int64_t *value)
    {
        return S_OK;
    }
    HRESULT SetFloat(BMDDeckLinkConfigurationID cfgID, double value)
    {
        return S_OK;
    }
    HRESULT GetFloat(BMDDeckLinkConfigurationID cfgID, double *value)
    {
        return S_OK;
    }
    HRESULT SetString(BMDDeckLinkConfigurationID cfgID, const char *value)
    {
        return S_OK;
    }
    HRESULT GetString(BMDDeckLinkConfigurationID cfgID, const char **value)
    {
        return S_OK;
    }
    HRESULT WriteConfigurationToPreferences(void)
    {
        return S_OK;
    }
};

class SoundDeckLink : public IDeckLink {
protected:
    std::string _alsa_device;
    std::string _model_display_name;
public:
    DUMMY_IUNKNOWN_REFERENCE;
    HRESULT QueryInterface(REFIID id, void **outputInterface)
    {
        static const size_t size_iid = 16;

        if (memcmp(&id, &IID_IDeckLinkAttributes, size_iid) == 0) {
            *outputInterface = new SoundDeckLinkAttributes();
            return S_OK;
        }
        if (memcmp(&id, &IID_IDeckLinkOutput, size_iid) == 0) {
            *outputInterface = new SoundDeckLinkOutput(_alsa_device);
            return S_OK;
        }
        if (memcmp(&id, &IID_IDeckLinkConfiguration, size_iid) == 0) {
            *outputInterface = new SoundDeckLinkConfiguration();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    SoundDeckLink(std::pair<std::string, std::string> alsa_device)
        : _alsa_device(alsa_device.first),
          _model_display_name(alsa_device.second)
    {
    }
    HRESULT GetModelName(const char **modelName)
    {
        *modelName = strdup(_model_display_name.c_str());
        return S_OK;
    }
    HRESULT GetDisplayName(const char **displayName)
    {
        *displayName = strdup(_model_display_name.c_str());
        return S_OK;
    }
};

class SoundDeckLinkAPIInformation :
    public IDeckLinkAPIInformation {
protected:
    IDeckLinkAPIInformation *_forward;
    unsigned int _api_version_int;
    std::string _api_version_str;
    void load_bmd(void)
    {
        load_lib_api();

        IDeckLinkAPIInformation *(*create_api_information_f)(void) =
            reinterpret_cast<IDeckLinkAPIInformation *(*)(void)>
            (dlsym(lib_api, "CreateDeckLinkAPIInformationInstance_0001"));

        if (create_api_information_f != NULL) {
            _forward = create_api_information_f();
        }
    }
public:
    DUMMY_IUNKNOWN;
    SoundDeckLinkAPIInformation(void)
        : _forward(NULL), _api_version_int(0x0a090000),
          _api_version_str("10.9")
    {
#ifdef PATH_A
        load_bmd();
#endif // PATH_A
    }
    HRESULT GetFlag(BMDDeckLinkAPIInformationID cfgID, bool *value)
    {
        return E_FAIL;
    }
    HRESULT GetInt(BMDDeckLinkAPIInformationID cfgID, int64_t *value)
    {
        if (_forward != NULL) {
            return _forward->GetInt(cfgID, value);
        }
        else if (cfgID == BMDDeckLinkAPIVersion) {
            *value = _api_version_int;
            return S_OK;
        }
        else {
            return E_FAIL;
        }
    }
    HRESULT GetFloat(BMDDeckLinkAPIInformationID cfgID,
                     double *value)
    {
        return E_FAIL;
    }
    HRESULT GetString(BMDDeckLinkAPIInformationID cfgID,
                      const char **value)
    {
        if (_forward != NULL) {
            return _forward->GetString(cfgID, value);
        }
        else if (cfgID == BMDDeckLinkAPIVersion) {
            *value = strdup(_api_version_str.c_str());
            return S_OK;
        }
        else {
            return E_FAIL;
        }
    }
};

class SoundDeckLinkIterator : public IDeckLinkIterator {
protected:
    std::vector<std::pair<std::string, std::string> > _alsa_device;
    std::vector<std::pair<std::string, std::string> >::const_iterator
    _iterator_alsa_device;
    size_t _count;
    IDeckLinkIterator *_iterator_bmd;
    void enumerate_alsa_dev(void)
    {
        snd_pcm_info_t *pcm_info;
        snd_ctl_card_info_t *card_info;

        snd_ctl_card_info_alloca(&card_info);
        snd_pcm_info_alloca(&pcm_info);

        int card = -1;

        while (!(snd_card_next(&card) < 0 || card < 0)) {
            snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
            snd_ctl_t *alsa_ctl;
            // "hw:" + 11 characters max for int + '\0'
            char card_name[15];

            snprintf(card_name, 15, "hw:%d", card);
            if (snd_ctl_open(&alsa_ctl, card_name, 0) < 0) {
                continue;
            }
            if (snd_ctl_card_info(alsa_ctl, card_info) < 0) {
                snd_ctl_close(alsa_ctl);
                continue;
            }

            int dev = -1;

            while (!(snd_ctl_pcm_next_device(alsa_ctl, &dev),
                     dev < 0)) {
                snd_pcm_info_set_device(pcm_info, dev);
                snd_pcm_info_set_subdevice(pcm_info, 0);
                snd_pcm_info_set_stream(pcm_info, stream);

                if (snd_ctl_pcm_info(alsa_ctl, pcm_info) < 0) {
                    continue;
                }

                // "hw:" + 2 x 11 characters max for int + ',' + '\0'
                char card_dev_name[27];

                snprintf(card_dev_name, 27, "hw:%d,%d", card, dev);

                _alsa_device.push_back(
                    std::pair<std::string, std::string>(
                        card_dev_name,
                        snd_ctl_card_info_get_name(card_info) +
                        std::string(" ") +
                        snd_pcm_info_get_name(pcm_info)));
            }
            snd_ctl_close(alsa_ctl);
        }
        _iterator_alsa_device = _alsa_device.begin();
    }
    void load_bmd(void)
    {
        load_lib_api();

        IDeckLinkIterator *(*create_iterator_f)(void) =
            reinterpret_cast<IDeckLinkIterator *(*)(void)>
            (dlsym(lib_api, "CreateDeckLinkIteratorInstance_0002"));

        if (create_iterator_f != NULL) {
            _iterator_bmd = create_iterator_f();
        }
    }
public:
    DUMMY_IUNKNOWN_REFERENCE;
    HRESULT QueryInterface(REFIID id, void **outputInterface)
    {
        static const size_t size_iid = 16;

        if (memcmp(&id, &IID_IDeckLinkAPIInformation, size_iid) == 0) {
            *outputInterface = new SoundDeckLinkAPIInformation();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    SoundDeckLinkIterator(void)
        : IDeckLinkIterator(), _count(0), _iterator_bmd(NULL)
    {
        enumerate_alsa_dev();
#ifdef PATH_A
        load_bmd();
#endif // PATH_A
    }
    HRESULT Next(IDeckLink **deckLinkInstance)
    {
        if (_iterator_alsa_device != _alsa_device.end()) {
            *deckLinkInstance =
                new SoundDeckLink(*_iterator_alsa_device);
            _iterator_alsa_device++;
            return S_OK;
        }
        else if (_iterator_bmd != NULL) {
            return _iterator_bmd->Next(deckLinkInstance);
        }
        else {
            return S_FALSE;
        }
    }
};

class SoundDeckLinkVideoConversion :
    public IDeckLinkVideoConversion {
protected:
public:
    DUMMY_IUNKNOWN;
    HRESULT ConvertFrame(IDeckLinkVideoFrame *srcFrame,
                         IDeckLinkVideoFrame *dstFrame)
    {
        return S_OK;
    }
};

class SoundDeckLinkDiscovery : public IDeckLinkDiscovery {
protected:
public:
    DUMMY_IUNKNOWN;
    HRESULT
    InstallDeviceNotifications(IDeckLinkDeviceNotificationCallback *
                               deviceNotificationCallback)
    {
        return S_OK;
    }
    HRESULT UninstallDeviceNotifications(void)
    {
        return S_OK;
    }
};

extern "C" {

    IDeckLinkIterator *CreateDeckLinkIteratorInstance_0002(void)
    {
        return new SoundDeckLinkIterator();
    }

    IDeckLinkAPIInformation *
    CreateDeckLinkAPIInformationInstance_0001(void)
    {
        return new SoundDeckLinkAPIInformation();
    }

    IDeckLinkVideoConversion *
    CreateVideoConversionInstance_0001(void)
    {
        return new SoundDeckLinkVideoConversion();
    }

    IDeckLinkDiscovery *CreateDeckLinkDiscoveryInstance_0001(void)
    {
        return new SoundDeckLinkDiscovery();
    }

}
