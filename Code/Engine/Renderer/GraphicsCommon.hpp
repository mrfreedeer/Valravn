#pragma  once
#include "Engine/Core/ErrorWarningAssert.hpp"

#undef OPAQUE
#define DX_SAFE_RELEASE(dxObject)			\
{											\
	if (( dxObject) != nullptr)				\
	{										\
		(dxObject)->Release();				\
		(dxObject) = nullptr;				\
	}										\
}

inline void ThrowIfFailed(long hr, char const* errorMsg) {
	if (hr < 0) {
		ERROR_AND_DIE(errorMsg);
	}
}




enum class BlendMode
{
	ALPHA = 1,
	ADDITIVE,
	OPAQUE
};

enum class CullMode {
	NONE = 1,
	FRONT,
	BACK,
	NUM_CULL_MODES
};

enum class FillMode {
	SOLID = 1,
	WIREFRAME,
	NUM_FILL_MODES
};

enum class WindingOrder {
	CLOCKWISE = 1,
	COUNTERCLOCKWISE
};


enum class DepthTest // Transformed directly to DX11 (if standard changes, unexpected behavior might result) check when changing to > DX11
{
	NEVER = 1,
	LESS = 2,
	EQUAL = 3,
	LESSEQUAL = 4,
	GREATER = 5,
	NOTEQUAL = 6,
	GREATEREQUAL = 7,
	ALWAYS = 8,
};

enum class SamplerMode
{
	POINTCLAMP,
	POINTWRAP,
	BILINEARCLAMP,
	BILINEARWRAP,
	SHADOWMAPS,
};

enum class TopologyType {// Transformed directly to DX12 (if standard changes, unexpected behavior might result) check when changing to > DX12
	TOPOLOGY_TYPE_UNDEFINED = 0,
	TOPOLOGY_TYPE_POINT = 1,
	TOPOLOGY_TYPE_LINE = 2,
	TOPOLOGY_TYPE_TRIANGLE = 3,
	TOPOLOGY_TYPE_PATCH = 4
};

/*
* Since SRV UAV AND CBV share heap, the start and end of each needs to be managed
*/
constexpr unsigned int SRV_UAV_CBV_DEFAULT_SIZE = 4096;
constexpr unsigned int CBV_HANDLE_START = 0;
constexpr unsigned int CBV_HANDLE_END = (SRV_UAV_CBV_DEFAULT_SIZE / 8) * 3 - 1;
constexpr unsigned int CBV_DESCRIPTORS_AMOUNT = CBV_HANDLE_END - CBV_HANDLE_START + 1;

constexpr unsigned int SRV_HANDLE_START = CBV_HANDLE_END + 1;
constexpr unsigned int SRV_HANDLE_END = SRV_HANDLE_START + (SRV_UAV_CBV_DEFAULT_SIZE / 2) - 1;
constexpr unsigned int SRV_DESCRIPTORS_AMOUNT = SRV_HANDLE_END - SRV_HANDLE_START + 1;

constexpr unsigned int UAV_HANDLE_START = SRV_HANDLE_END + 1;
constexpr unsigned int UAV_HANDLE_END = SRV_UAV_CBV_DEFAULT_SIZE - 1;
constexpr unsigned int UAV_DESCRIPTORS_AMOUNT = UAV_HANDLE_END - UAV_HANDLE_START + 1;





