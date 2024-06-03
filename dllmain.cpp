// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include <Windows.h>
#include <gl\GL.h>
#pragma comment(lib, "opengl32.lib")
#include "geom.h"

class CPlayer
{
private:
	BYTE _pad0[0x4];
public:
	vec head; //0x04
	vec vel; //0x10
private:
	BYTE _pad1[0xC];
public:
	vec pos; //0x28
	vec rot;
private:
	BYTE _pad2[0xAC];
public:
	DWORD health; //0xEC
private:
	BYTE _pad3[0x114];
public:
	BYTE shooting;
	CHAR nick[0x20]; //0x205
private:
	BYTE _pad4[0xE7];
public:
	DWORD team; //0x30C
};
class CPlayerList
{
public:
	CPlayer* player[0x20];
};
class CGame //ac_client.exe+18AC00
{
public:
	CPlayer* local;
	CPlayerList* remote;
private:
	DWORD _pad0;
public:
	DWORD count;
};

void ClampAngles(vec* a)
{
	if (a->y > 88.f) a->y = 88.f;
	if (a->y < -88.f) a->y = -88.f;
	while (a->x < 0.f) a->x += 360.f;
	while (a->x >= 360.f) a->x -= 360.f;
}
vec LookAt(vec v)
{
	vec a(0.f, 0.f, 0.f);
	a.x = ::atanf(v.x / v.y) * -57.2957795f;
	a.y = ::atanf(v.z / ::sqrtf(v.x*v.x + v.y*v.y)) * -57.2957795f;
	if (v.y < 0.f) a.x += 180.f;
	ClampAngles(&a);
	return a;
}

void* SwapProtectedPointer(void** pPtr, void* pNew)
{
	DWORD protect;
	VirtualProtect(pPtr, sizeof(void*), PAGE_READWRITE, &protect);
	void* orig = *pPtr;
	*pPtr = pNew;
	VirtualProtect(pPtr, sizeof(void*), protect, &protect);
	return orig;
}

void fill_rectangle(float x1, float y1, float x2, float y2, unsigned char r, unsigned char g, unsigned char b)
{
	glBegin(GL_POLYGON);
	glColor3ub(r, g, b);
	glVertex2f(x1, y1);
	glVertex2f(x2, y1);
	glVertex2f(x2, y2);
	glVertex2f(x1, y2);
	glEnd();
}

class CFont
{
private:
	unsigned int base;
public:
	CFont(int size)
	{
		this->base = glGenLists(96);
		HDC hdc = wglGetCurrentDC();
		HFONT hFont = CreateFontA(-size, NULL, NULL, NULL, FW_MEDIUM, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FF_DONTCARE | DEFAULT_PITCH, "Consolas");
		HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

		wglUseFontBitmapsA(hdc, 32, 96, this->base);
		SelectObject(hdc, hOldFont);
		DeleteObject(hFont);
	}
	void print(float x, float y, const char* text, unsigned char r, unsigned char g, unsigned char b)
	{
		glColor3ub(r, g, b);
		glRasterPos2f(x, y);
		glPushAttrib(GL_LIST_BIT);
		glListBase(this->base - 32);
		glCallLists(strlen(text), GL_UNSIGNED_BYTE, text);
		glPopAttrib();
	}
};

bool g_bMenuVisible = true;
bool g_bWallhackPlayers = false;
bool g_bWallhackMapEnts = false;
bool g_bAutomaticAiming = false;
bool g_bAutomaticTriger = false;

CFont* g_pFont = nullptr;
CGame* g_pGame = nullptr;
DWORD* g_p_gamemode = nullptr;
/*
"ac_client.exe"+7BEC5: 0F BF 46 04                    -  movsx eax,word ptr [esi+04]
"ac_client.exe"+7BEC9: 66 0F 6E C0                    -  movd xmm0,eax
"ac_client.exe"+7BECD: 0F 5B C0                       -  cvtdq2ps xmm0,xmm0
"ac_client.exe"+7BED0: F3 0F 11 04 8D 18 21 59 00     -  movss [ecx*4+ac_client.exe+192118],xmm0
"ac_client.exe"+7BED9: 8B 0D 74 08 59 00              -  mov ecx,[ac_client.exe+190874]
"ac_client.exe"+7BEDF: 47                             -  inc edi
"ac_client.exe"+7BEE0: 83 C2 18                       -  add edx,18
"ac_client.exe"+7BEE3: 89 54 24 38                    -  mov [esp+38],edx
"ac_client.exe"+7BEE7: 3B F9                          -  cmp edi,ecx
"ac_client.exe"+7BEE9: 0F 8C 71 FF FF FF              -  jl ac_client.exe+7BE60
"ac_client.exe"+7BEEF: A1 F8 AB 58 00                 -  mov eax,[ac_client.exe+18ABF8] // <--- gamemode
"ac_client.exe"+7BEF4: C7 05 38 BB 56 00 FF FF FF FF  -  mov [ac_client.exe+16BB38],FFFFFFFF
"ac_client.exe"+7BEFE: C7 05 90 BC 56 00 FF FF FF FF  -  mov [ac_client.exe+16BC90],FFFFFFFF
"ac_client.exe"+7BF08: C7 05 4C F2 57 00 00 00 00 00  -  mov [ac_client.exe+17F24C],00000000
"ac_client.exe"+7BF12: 85 C0                          -  test eax,eax
"ac_client.exe"+7BF14: 78 05                          -  js ac_client.exe+7BF1B
"ac_client.exe"+7BF16: 83 F8 16                       -  cmp eax,16
"ac_client.exe"+7BF19: 7C 05                          -  jl ac_client.exe+7BF20
"ac_client.exe"+7BF1B: 83 F8 FF                       -  cmp eax,-01
"ac_client.exe"+7BF1E: 75 30                          -  jne ac_client.exe+7BF50
"ac_client.exe"+7BF20: 85 C0                          -  test eax,eax
*/
bool m_teammode(void)
{
	return (*g_p_gamemode==0 || *g_p_gamemode==4 || *g_p_gamemode==5 || *g_p_gamemode==7 || *g_p_gamemode==11 || *g_p_gamemode==13 || *g_p_gamemode==14 || *g_p_gamemode==16 || *g_p_gamemode==17 || *g_p_gamemode==20 || *g_p_gamemode==21);
}
char* g_p_ocull = nullptr;
/*
ac_client.exe+BA890 - 55                    - push ebp
"ac_client.exe"+BA891: 8B EC                          -  mov ebp,esp
"ac_client.exe"+BA893: 83 E4 F8                       -  and esp,-08
"ac_client.exe"+BA896: 83 EC 14                       -  sub esp,14
"ac_client.exe"+BA899: 80 3D 9E BA 56 00 00           -  cmp byte ptr [ac_client.exe+16BA9E],00 // <--- ocull
"ac_client.exe"+BA8A0: 0F 28 D9                       -  movaps xmm3,xmm1
"ac_client.exe"+BA8A3: 53                             -  push ebx
"ac_client.exe"+BA8A4: 56                             -  push esi
"ac_client.exe"+BA8A5: 57                             -  push edi
"ac_client.exe"+BA8A6: F3 0F 11 5C 24 1C              -  movss [esp+1C],xmm3
"ac_client.exe"+BA8AC: F3 0F 11 44 24 14              -  movss [esp+14],xmm0
"ac_client.exe"+BA8B2: 75 22                          -  jne ac_client.exe+BA8D6 //if not ocull then inlined void disableraytable() { odist = 1e16f; loopi(NUMRAYS=0x200) rdist[i] = 1e16f; }
"ac_client.exe"+BA8B4: B9 00 02 00 00                 -  mov ecx,00000200
"ac_client.exe"+BA8B9: C7 05 58 C9 56 00 CA 1B 0E 5A  -  mov [ac_client.exe+16C958],nvoglv32.DLL+C41BCA = 0x5A0E1BCA //1e16f
"ac_client.exe"+BA8C3: B8 CA 1B 0E 5A                 -  mov eax,nvoglv32.DLL+C41BCA = 0x5A0E1BCA //1e16f
"ac_client.exe"+BA8C8: BF 90 2F 58 00                 -  mov edi,ac_client.exe+182F90
"ac_client.exe"+BA8CD: F3 AB                          - repe  stosd 
"ac_client.exe"+BA8CF: 5F                             -  pop edi
"ac_client.exe"+BA8D0: 5E                             -  pop esi
"ac_client.exe"+BA8D1: 5B                             -  pop ebx
"ac_client.exe"+BA8D2: 8B E5                          -  mov esp,ebp
"ac_client.exe"+BA8D4: 5D                             -  pop ebp
"ac_client.exe"+BA8D5: C3                             -  ret
*/
struct traceresult_s
{
	vec end;
	bool collided;
};
typedef void(__fastcall* fnTraceLine)(vec from, vec to, CPlayer* pTracer, bool CheckPlayers, traceresult_s* tr, bool SkipTags);
fnTraceLine g_p_TraceLine;
/*
"ac_client.exe"+109010: 55                       -  push ebp
"ac_client.exe"+109011: 8B EC                    -  mov ebp,esp
"ac_client.exe"+109013: 83 E4 F8                 -  and esp,-08
"ac_client.exe"+109016: 83 EC 38                 -  sub esp,38
"ac_client.exe"+109019: F3 0F 10 7D 08           -  movss xmm7,[ebp+08]
"ac_client.exe"+10901E: F3 0F 7E 45 08           -  movq xmm0,[ebp+08]
"ac_client.exe"+109023: 8B 45 10                 -  mov eax,[ebp+10]
"ac_client.exe"+109026: 56                       -  push esi
"ac_client.exe"+109027: 88 54 24 0B              -  mov [esp+0B],dl
"ac_client.exe"+10902B: F3 0F 2C D7              -  cvttss2si edx,xmm7
"ac_client.exe"+10902F: 57                       -  push edi
"ac_client.exe"+109030: 8B 7D 20                 -  mov edi,[ebp+20]
"ac_client.exe"+109033: 89 4C 24 14              -  mov [esp+14],ecx
"ac_client.exe"+109037: C6 47 0C 00              -  mov byte ptr [edi+0C],00
"ac_client.exe"+10903B: 66 0F D6 07              -  movq [edi],xmm0
"ac_client.exe"+10903F: F3 0F 10 05 14 BF 55 00  -  movss xmm0,[ac_client.exe+15BF14] //9999.f
"ac_client.exe"+109047: 89 47 08                 -  mov [edi+08],eax
"ac_client.exe"+10904A: F3 0F 11 44 24 10        -  movss [esp+10],xmm0
"ac_client.exe"+109050: F3 0F 11 05 24 47 5A 00  -  movss [ac_client.exe+1A4724],xmm0
"ac_client.exe"+109058: 83 FA 02                 -  cmp edx,02
"ac_client.exe"+10905B: 0F 8C 03 09 00 00        -  jl ac_client.exe+109964
"ac_client.exe"+109061: F3 0F 10 55 0C           -  movss xmm2,[ebp+0C]
"ac_client.exe"+109066: F3 0F 2C CA              -  cvttss2si ecx,xmm2
*/
bool CanHit(vec from, vec to)
{
	traceresult_s tr;
	g_p_TraceLine(from, to, /*g_pGame->local*/ nullptr, false, &tr, false);
	__asm add esp, 0x20;
	return !tr.collided;
}

typedef DWORD(__cdecl* fnGL_SwapWindow)(DWORD, DWORD);
fnGL_SwapWindow g_orig_GL_SwapWindow;
typedef DWORD(__fastcall* fnRenderModel)(void*, void*, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
fnRenderModel g_orig_RenderMapEntity;
fnRenderModel g_orig_RenderPlayer;
/*
"ac_client.exe"+64636: F3 0F 10 47 18        -  movss xmm0,[edi+18]
"ac_client.exe"+6463B: F3 0F 11 04 24        -  movss [esp],xmm0
"ac_client.exe"+64640: F3 0F 10 47 24        -  movss xmm0,[edi+24]
"ac_client.exe"+64645: 57                    -  push edi
"ac_client.exe"+64646: FF 77 28              -  push [edi+28]
"ac_client.exe"+64649: 51                    -  push ecx
"ac_client.exe"+6464A: F3 0F 11 04 24        -  movss [esp],xmm0
"ac_client.exe"+6464F: 8B CD                 -  mov ecx,ebp
"ac_client.exe"+64651: FF 77 10              -  push [edi+10]
"ac_client.exe"+64654: FF 77 0C              -  push [edi+0C]
"ac_client.exe"+64657: FF 50 14              -  call dword ptr [eax+14] //this calls TWO render model functions: mapents and players
"ac_client.exe"+6465A: F7 47 0C 00 80 00 00  -  test [edi+0C],00008000
"ac_client.exe"+64661: 74 12                 -  je ac_client.exe+64675
"ac_client.exe"+64663: 68 01 02 00 00        -  push 00000201
"ac_client.exe"+64668: FF D6                 -  call esi //OPENGL32.glDepthFunc
"ac_client.exe"+6466A: 68 E2 0B 00 00        -  push 00000BE2
"ac_client.exe"+6466F: FF 15 90 52 52 00     -  call dword ptr [ac_client.exe+125290] //OPENGL32.glDisable
"ac_client.exe"+64675: 5E                    -  pop esi
"ac_client.exe"+64676: 5F                    -  pop edi
"ac_client.exe"+64677: 5D                    -  pop ebp
"ac_client.exe"+64678: 5B                    -  pop ebx
"ac_client.exe"+64679: 83 C4 10              -  add esp,10
ac_client.exe+6467C - C3                    - ret 
*/
DWORD __fastcall hook_RenderPlayer(void* ecx, void* edx, DWORD param1, DWORD param2, DWORD param3, DWORD param4, DWORD param5, DWORD param6, DWORD param7, DWORD param8, DWORD param9, DWORD param10, DWORD param11)
{
	if (g_bWallhackPlayers)
	{
		*g_p_ocull = 0;
		glDisable(GL_DEPTH_TEST);
	}
	DWORD result = g_orig_RenderPlayer(ecx, edx, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11);
	
	if (g_bWallhackPlayers) glEnable(GL_DEPTH_TEST);
	return result;
}
DWORD __fastcall hook_RenderMapEntity(void* ecx, void* edx, DWORD param1, DWORD param2, DWORD param3, DWORD param4, DWORD param5, DWORD param6, DWORD param7, DWORD param8, DWORD param9, DWORD param10, DWORD param11)
{
	if (g_bWallhackMapEnts)
	{
		*g_p_ocull = 0;
		glDisable(GL_DEPTH_TEST);
	}
	DWORD result = g_orig_RenderMapEntity(ecx, edx, param1, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11);
	
	if (g_bWallhackMapEnts) glEnable(GL_DEPTH_TEST);
	return result;
}

DWORD __cdecl hook_GL_SwapWindow(DWORD param1, DWORD param2)
{
	CPlayer* loc = g_pGame->local;
	vec bestdiff(999.f, 999.f, 0.f);
	vec dist(999.f, 999.f, 999.f);

	for (DWORD cn = 0; cn < g_pGame->count; ++cn)
	{
		CPlayer* ent = g_pGame->remote->player[cn];

		if (ent == nullptr) continue;
		if (ent->health == NULL || ent->health > 100) continue;
		if (ent->team == loc->team && m_teammode()) continue;

		vec diff = loc->head;
		diff.sub(ent->head);
		diff.z += 1.f;

		vec view = LookAt(diff);
		diff = view.sub(loc->rot);

		if (!CanHit(loc->head, ent->head)) continue;

		if (diff.magnitude() < bestdiff.magnitude())
		{
			bestdiff = diff;

			dist = loc->head;
			dist.sub(ent->head);
		}
	}

	float mag = bestdiff.magnitude();

	if (g_bAutomaticTriger)
	{
		if (mag < 80.f / dist.magnitude())
			loc->shooting = true;
		else
			loc->shooting = false;
	}

	if (g_bAutomaticAiming && mag < 30.f && GetAsyncKeyState(2))
	{
		bestdiff.mul(mag / 30.f);

		loc->rot.add(bestdiff);
		ClampAngles(&loc->rot);
	}

	if (GetAsyncKeyState(VK_F5) & 1) g_bMenuVisible = !g_bMenuVisible;

	if (g_bMenuVisible)
	{
		if (!g_pFont) g_pFont = new CFont(14);

		const float posx = 200.;
		const float posy = 300.;
		const float vspace = 30.;
		const float hspace = 340.;

		g_pFont->print(posx + 10, posy + vspace * 1, "[F5] Toggle Menu Visibility", 100, 50, 250);
		g_pFont->print(posx + 10, posy + vspace * 2, "[F6] Wallhack Players -", 100, 50, 250);
		g_pFont->print(posx + 10, posy + vspace * 3, "[F7] Wallhack MapEnts -", 100, 50, 250);
		g_pFont->print(posx + 10, posy + vspace * 4, "[F8] Automatic Aiming -", 100, 50, 250);
		g_pFont->print(posx + 10, posy + vspace * 5, "[F9] Automatic Triger -", 100, 50, 250);
		g_pFont->print(posx + 10, posy + vspace * 6, "   ~ Cheat Created by Segy", 100, 50, 250);

		if (g_bWallhackPlayers)
			g_pFont->print(posx + 10 + hspace, posy + vspace * 2, "ON", 120, 250, 90);
		else
			g_pFont->print(posx + 10 + hspace, posy + vspace * 2, "OFF", 250, 80, 50);

		if (g_bWallhackMapEnts)
			g_pFont->print(posx + 10 + hspace, posy + vspace * 3, "ON", 120, 250, 90);
		else
			g_pFont->print(posx + 10 + hspace, posy + vspace * 3, "OFF", 250, 80, 50);

		if (g_bAutomaticAiming)
			g_pFont->print(posx + 10 + hspace, posy + vspace * 4, "ON", 120, 250, 90);
		else
			g_pFont->print(posx + 10 + hspace, posy + vspace * 4, "OFF", 250, 80, 50);

		if (g_bAutomaticTriger)
			g_pFont->print(posx + 10 + hspace, posy + vspace * 5, "ON", 120, 250, 90);
		else
			g_pFont->print(posx + 10 + hspace, posy + vspace * 5, "OFF", 250, 80, 50);

		if (GetAsyncKeyState(VK_F6) & 1) g_bWallhackPlayers = !g_bWallhackPlayers;
		if (GetAsyncKeyState(VK_F7) & 1) g_bWallhackMapEnts = !g_bWallhackMapEnts;
		if (GetAsyncKeyState(VK_F8) & 1) g_bAutomaticAiming = !g_bAutomaticAiming;
		if (GetAsyncKeyState(VK_F9) & 1) g_bAutomaticTriger = !g_bAutomaticTriger;

		fill_rectangle(posx, posy, posx + 405, posy + 15 + vspace * 6, 30, 30, 30);
	}
	return g_orig_GL_SwapWindow(param1, param2);
}
/*
"SDL2.dll"+BF500: F6 43 30 02                    -  test byte ptr [ebx+30],02
"SDL2.dll"+BF504: 74 3B                          -  je SDL2.dll+BF541
"SDL2.dll"+BF506: 8B 80 C8 02 00 00              -  mov eax,[eax+000002C8]
"SDL2.dll"+BF50C: 89 04 24                       -  mov [esp],eax
"SDL2.dll"+BF50F: E8 EC 8D FB FF                 -  call SDL2.dll+78300
"SDL2.dll"+BF514: 39 C3                          -  cmp ebx,eax
"SDL2.dll"+BF516: 75 18                          -  jne SDL2.dll+BF530
"SDL2.dll"+BF518: A1 C8 DC AA 59                 -  mov eax,[SDL2.dll+11DCC8]
"SDL2.dll"+BF51D: 89 5C 24 04                    -  mov [esp+04],ebx
"SDL2.dll"+BF521: 89 04 24                       -  mov [esp],eax
"SDL2.dll"+BF524: FF 90 C8 00 00 00              -  call dword ptr [eax+000000C8] //hooking this pointer
"SDL2.dll"+BF52A: 83 C4 08                       -  add esp,08 //GDI32.SwapBuffers and return from it, then return one more time
"SDL2.dll"+BF52D: 5B                             -  pop ebx
"SDL2.dll"+BF52E: C3                             -  ret 
"SDL2.dll"+BF52F: 90                             -  nop 
"SDL2.dll"+BF530: C7 44 24 10 80 54 AA 59        -  mov [esp+10],SDL2.dll+115480 //"The specified window has not been made current"
"SDL2.dll"+BF538: 83 C4 08                       -  add esp,08
"SDL2.dll"+BF53B: 5B                             -  pop ebx
"SDL2.dll"+BF53C: E9 7F 32 F4 FF                 -  jmp SDL2.dll+27C0
"SDL2.dll"+BF541: C7 44 24 10 00 54 AA 59        -  mov [esp+10],SDL2.dll+115400 //"The specified window isn't an OpenGL window"
"SDL2.dll"+BF549: 83 C4 08                       -  add esp,08
*/
void SetupHooks(void)
{
	char* hSDL = (char*)GetModuleHandleA("SDL2.dll");
	char* eax = *(char**)(hSDL + 0x11DCC8);
	DWORD* fnptr = (DWORD*)(eax + 0xC8);

	g_orig_GL_SwapWindow = (fnGL_SwapWindow)*fnptr;
	*fnptr = (DWORD)hook_GL_SwapWindow;

	g_orig_RenderPlayer = (fnRenderModel)SwapProtectedPointer((void**)0x54ADE4, hook_RenderPlayer);
	g_orig_RenderMapEntity = (fnRenderModel)SwapProtectedPointer((void**)0x54AC40, hook_RenderMapEntity);

	g_p_ocull = (char*)0x56BA9E;
	g_pGame = (CGame*)0x58AC00;
	g_p_TraceLine = (fnTraceLine)0x509010;
	g_p_gamemode = (DWORD*)0x58ABF8;
}
BOOL APIENTRY entry( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		SetupHooks();
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}