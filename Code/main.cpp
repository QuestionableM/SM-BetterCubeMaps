#include "Utils/StaticVariable.hpp"

#include <MinHook.h>

#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

#include <string>

#pragma comment(lib, "User32.lib")

//The memory which holds the cube map size
static StaticVariable<std::uint32_t, 0x9D9D92> g_cubeMapSize;

//Override values
constexpr std::uint32_t g_overrideTextureSize = 1024;
constexpr float g_fOverrideTextureSize = float(g_overrideTextureSize);

//Min hook specific stuff
static bool ms_mhInitialized = false;
static bool ms_mhHooksAttached = false;

#define DEFINE_HOOK(address, detour, original) \
	MH_CreateHook((LPVOID)(v_mod_base + address), (LPVOID)detour, (LPVOID*)&original)

#define EASY_HOOK(address, func_name) \
	DEFINE_HOOK(address, h_ ## func_name, o_ ## func_name)

#define CALLER_ADDRESS std::uintptr_t(_ReturnAddress()) - std::uintptr_t(GetModuleHandle(NULL))

static void (*o_updateViewportsAndScissorRects)(void*, float, float, float, float) = nullptr;
void h_updateViewportsAndScissorRects(
	void* device,
	float top_left_x,
	float top_left_y,
	float width,
	float height)
{
	if (CALLER_ADDRESS == 0x6D311A)
		width = height = g_fOverrideTextureSize;

	o_updateViewportsAndScissorRects(device, top_left_x, top_left_y, width, height);
}

static void (*o_initRenderTargetView)(void*, std::uint64_t, std::uint64_t, std::uint64_t, const std::string&) = nullptr;
void h_initRenderTargetView(
	void* rts,
	std::uint64_t width,
	std::uint64_t height,
	std::uint64_t a4,
	const std::string& name)
{
	switch (CALLER_ADDRESS)
	{
	case 0x6CBF77:
		__fallthrough;
	case 0x6CC030:
		__fallthrough;
	case 0x6CC100:
		__fallthrough;
	case 0x6CC1DB:
		width = height = std::uint64_t(g_overrideTextureSize);
		break;
	default:
		break;
	}

	o_initRenderTargetView(rts, width, height, a4, name);
}

static void* (*o_createTextureResource)(
	void*, std::uint32_t, std::uint32_t, std::int32_t, std::int32_t, std::int32_t, std::uint64_t, const std::string&) = nullptr;
void* h_createTextureResource(
	void* res_mgr,
	std::uint32_t width,
	std::uint32_t height,
	std::int32_t a4,
	std::int32_t a5,
	std::int32_t weird1,
	std::uint64_t weird2,
	const std::string& resource_name)
{
	switch (CALLER_ADDRESS)
	{
	case 0x6CC29A:
		__fallthrough;
	case 0x6CC372:
		width = height = g_overrideTextureSize;
		break;
	default:
		break;
	}

	return o_createTextureResource(res_mgr, width, height, a4, a5, weird1, weird2, resource_name);
}

void process_attach()
{
	if (MH_Initialize() != MH_OK)
	{
		MessageBoxA(NULL, "Couldn't initialize MinHook", "BetterCubemaps", MB_ICONERROR);
		return;
	}

	ms_mhInitialized = true;
	g_cubeMapSize = g_overrideTextureSize;

	const std::uintptr_t v_mod_base = std::uintptr_t(GetModuleHandle(NULL));
	if (EASY_HOOK(0x909790, updateViewportsAndScissorRects) != MH_OK) return;
	if (EASY_HOOK(0x9D95E0, initRenderTargetView) != MH_OK) return;
	if (EASY_HOOK(0x58AB30, createTextureResource) != MH_OK) return;

	ms_mhHooksAttached = MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
}

void process_detach(HMODULE hmod)
{
	if (ms_mhInitialized)
	{
		if (ms_mhHooksAttached)
			MH_DisableHook(MH_ALL_HOOKS);

		MH_Uninitialize();
	}

	FreeLibrary(hmod);
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		process_attach();
		break;
	case DLL_PROCESS_DETACH:
		process_detach(hModule);
		break;
	}

	return TRUE;
}