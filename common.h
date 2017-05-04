#ifndef COMMON_H_
#define COMMON_H_

#define DUMMY_IUNKNOWN_REFERENCE							\
	protected:												\
	size_t _ref;											\
	public:													\
	virtual ULONG STDMETHODCALLTYPE AddRef(void)			\
	{														\
		return ++_ref;										\
	}														\
	virtual ULONG STDMETHODCALLTYPE Release(void)			\
	{														\
		return --_ref;										\
	}

#define DUMMY_IUNKNOWN						\
	HRESULT STDMETHODCALLTYPE				\
	QueryInterface(REFIID iid, LPVOID *ppv)	\
	{										\
		return E_NOINTERFACE;				\
	}										\
	DUMMY_IUNKNOWN_REFERENCE

#endif // COMMON_H_
