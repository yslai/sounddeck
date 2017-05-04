#include <cstdio>
#include <cstring>
#include <alsa/asoundlib.h>
#include "DeckLinkAPI.h"

#include "common.h"

class IDeckLinkGLScreenPreviewHelper_0001 :
	public IDeckLinkGLScreenPreviewHelper {
public:
	DUMMY_IUNKNOWN;
    HRESULT InitializeGL(void)
	{
		return E_FAIL;
	}
    HRESULT PaintGL(void)
	{
		return E_FAIL;
	}
    HRESULT SetFrame(IDeckLinkVideoFrame *theFrame)
	{
		return E_FAIL;
	}
    HRESULT Set3DPreviewFormat(BMD3DPreviewFormat previewFormat)
	{
		return E_FAIL;
	}
};

extern "C" {

	IDeckLinkGLScreenPreviewHelper *
	CreateOpenGLScreenPreviewHelper_0001(void)
	{
		return new IDeckLinkGLScreenPreviewHelper_0001();
	}

	IDeckLinkGLScreenPreviewHelper *
	CreateOpenGLScreenPreviewHelper(void)
	{
		return CreateOpenGLScreenPreviewHelper_0001();
	}

}
