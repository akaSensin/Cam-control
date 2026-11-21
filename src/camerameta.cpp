/*
	Cam Control - made by sensin
	Camera metadata layer - see camerameta.h.
*/

#include "camerameta.h"
#include "log.h"

#include "main.h"
#include "natives.h"

#include <windows.h>
#include <psapi.h>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cmath>
#include <map>

namespace
{
	// Camera metadata hashes the game uses for the player ped. These are stable
	// across builds; the vehicle ones are read out of the vehicle model info.
	const unsigned int HASH_PED_THIRD     = 0xFBE36564u; // follow ped camera
	const unsigned int HASH_PED_FIRST     = 0xA70102CAu; // first person shooter camera
	const unsigned int HASH_PED_FIRST_ALT = 0x02DFF9A0u; // alternate first person shooter camera
	const unsigned int HASH_PED_FIRST_AIM = 0xEC22D348u; // first person ped aim camera
	const unsigned int HASH_PED_AIM       = 0xBB570E9Eu; // third person aiming camera

	// All 18 camFollowPedCameraMetadata entries from cameras.ymt so on-foot
	// third-person FOV, distance and pivot apply across walk, sprint, stealth,
	// cover, swim and other ped follow states rather than only the default one.
	const unsigned int kFollowPedExtraCams[] =
	{
		0x3E704326u, 0xB2C08BFDu, 0xEAF6855Au, 0x73F590F4u,
		0xDCE65055u, 0x2B67AB1Cu, 0x6007AD8Bu, 0xCDD88AD1u,
		0x8DC07186u, 0x9084C50Fu, 0x5820D1AFu, 0x6481B7E1u,
		0x5DBBFB6Eu, 0xA38DB056u, 0x16B702A3u, 0x41D72A2Eu,
		0x0F152D30u
	};

	// -----------------------------------------------------------------------
	// Weapon aiming cameras.
	//
	// The game does not use one aiming camera for everything: it looks up the
	// weapon in your hands in its own weapons.meta and switches to a camera
	// named there. Most weapons name nothing and fall back to the plain aiming
	// camera, which is why pistols, SMG, rifles and the rest are one group and
	// cannot be separated.
	//
	// These hashes are the names in weapons.meta / weaponcomponents.meta run
	// through the same joaat the game uses, and each one was checked against the
	// camera list in cameras.ymt.
	//
	// Two offsets, not one. The aiming cameras are one metadata type and the
	// two sniper cameras are another, with a shorter prefix before BaseFov -
	// 0x30 against 0x24 (three fewer 4-byte refs). The 0x30 side is the offset
	// the mod's on-foot aiming slider has always used; 0x24 is the first person
	// camera's offset, reached by the same arithmetic.
	// -----------------------------------------------------------------------
	const int OFF_FOV_TP_AIM = 0x30;
	const int OFF_FOV_FP_AIM = 0x24;

	struct WeaponCam
	{
		unsigned int hash;
		int offFov;
		float Settings::*fov; // 0 in the setting means "leave the game alone"
		const char *label;
		// Writing BaseFov is ignored by the game while this camera's "use weapon
		// fov setting" flag is still set - the weapon's own FOV then wins. Only
		// set for the cameras whose flag offset is verified (see
		// LAYOUT::pedAimUseWeaponFov and CAMERA-RESEARCH.md).
		bool clearWeaponFovFlag;
	};

	const WeaponCam kWeaponCams[] =
	{
		{ HASH_PED_AIM, OFF_FOV_TP_AIM, &Settings::weaponFovDefault, "weapons, default", true },
		{ 0xD2B17AE4u,  OFF_FOV_TP_AIM, &Settings::weaponFovDefault, "weapons, default (cover)", true },
		{ 0xE3348412u,  OFF_FOV_TP_AIM, &Settings::weaponFovDefault, "weapons, default (run & gun)", true },
		{ 0xF6156810u,  OFF_FOV_TP_AIM, &Settings::weaponFovDefault, "weapons, default (cinematic)", true },
		{ 0x70F748ECu,  OFF_FOV_TP_AIM, &Settings::weaponFovHip,     "weapons, hip fire", true },
		{ 0x7EA4617Au,  OFF_FOV_TP_AIM, &Settings::weaponFovHip,     "weapons, hip fire (cover)", true },
		{ 0x31009595u,  OFF_FOV_TP_AIM, &Settings::weaponFovHip,     "weapons, hip fire (run & gun)", true },
		{ 0xAA882A2Du,  OFF_FOV_TP_AIM, &Settings::weaponFovHip,     "weapons, hip fire (cinematic)", true },
		{ 0xE135DF75u,  OFF_FOV_TP_AIM, &Settings::weaponFovRpg,     "weapons, rpg", true },
		{ 0x24D03E82u,  OFF_FOV_TP_AIM, &Settings::weaponFovRpg,     "weapons, rpg (cover)", true },
		{ 0xED55CCAFu,  OFF_FOV_TP_AIM, &Settings::weaponFovRpg,     "weapons, rpg (run & gun)", true },
		{ 0x053D22F3u,  OFF_FOV_TP_AIM, &Settings::weaponFovShotgun, "weapons, shotgun", true },
		{ 0xC1ED5912u,  OFF_FOV_TP_AIM, &Settings::weaponFovShotgun, "weapons, shotgun (cover)", true },
		{ 0x0368F2CBu,  OFF_FOV_TP_AIM, &Settings::weaponFovShotgun, "weapons, shotgun (run & gun)", true },
		{ 0x64C2433Fu,  OFF_FOV_TP_AIM, &Settings::weaponFovShotgun, "weapons, shotgun (cinematic)", true },
		{ 0xB3B84A58u,  OFF_FOV_TP_AIM, &Settings::weaponFovThrown,  "weapons, thrown", true },
		{ 0x35470B7Cu,  OFF_FOV_TP_AIM, &Settings::weaponFovThrown,  "weapons, thrown (cover)", true },
		{ 0xE3F934B6u,  OFF_FOV_TP_AIM, &Settings::weaponFovThrown,  "weapons, thrown (run & gun)", true },
		{ 0x8CB470DFu,  OFF_FOV_TP_AIM, &Settings::weaponFovMelee,   "weapons, melee", true },
		{ 0xDAF92A81u,  OFF_FOV_TP_AIM, &Settings::weaponFovMelee,   "weapons, melee (cover)", true },
		{ 0xBBAEB699u,  OFF_FOV_TP_AIM, &Settings::weaponFovDigiscan, "weapons, digiscanner", true },
		{ 0x7225EF59u,  OFF_FOV_TP_AIM, &Settings::weaponFovJerry,   "weapons, jerry can", true },
		{ 0xEC22D348u,  OFF_FOV_FP_AIM, &Settings::weaponFovSniper,  "weapons, sniper", false },
		{ 0x8C40FB35u,  OFF_FOV_FP_AIM, &Settings::weaponFovSniperLow, "weapons, sniper low zoom", false },
		{ 0x64E68B46u,  OFF_FOV_FP_AIM, &Settings::weaponFovScopeOther, "weapons, scope/aim 1", false },
		{ 0xD4CFC0B3u,  OFF_FOV_FP_AIM, &Settings::weaponFovScopeOther, "weapons, scope/aim 2", false },
		{ 0xA5E49969u,  OFF_FOV_FP_AIM, &Settings::weaponFovScopeOther, "weapons, scope/aim 3", false },
		{ 0x195F4C0Cu,  OFF_FOV_FP_AIM, &Settings::weaponFovScopeOther, "weapons, scope/aim 4", false },
		{ 0xD5F08A9Eu,  OFF_FOV_FP_AIM, &Settings::weaponFovScopeOther, "weapons, scope/aim 5", false },
		{ 0xB68EA067u,  OFF_FOV_FP_AIM, &Settings::weaponFovScopeOther, "weapons, scope/aim 6", false },
		{ 0x3672868Au,  OFF_FOV_FP_AIM, &Settings::weaponFovScopeOther, "weapons, scope/aim 7", false },
	};

	// Parachute follow cameras (camFollowParachuteCameraMetadata, BaseFov at +0x30).
	const unsigned int kParachuteCams[] = { 0xBBA95D43u, 0x4CAF9332u };

	// All 36 drive-by / vehicle aiming cameras (camThirdPersonVehicleAimCameraMetadata,
	// BaseFov at +0x30, ShouldApplyWeaponFov at [96]). Covers cars, bikes, boats, helis.
	const unsigned int kVehicleAimCams[] =
	{
		0x0F146894u, 0x7754E31Bu, 0xEF9F5C37u, 0x278C8472u, 0x6B93C9A6u, 0x39734112u,
		0x39AB2557u, 0xD4C3B230u, 0xA04B7AC4u, 0x256DCF16u, 0xDAAAB0E7u, 0xE8D518AEu,
		0x1558FC4Fu, 0xD3A61169u, 0x173B2184u, 0xEEC79C4Eu, 0xF3E480E8u, 0x69A4906Du,
		0x56167B1Fu, 0xA31656D7u, 0xF299070Cu, 0x44B71D7Eu, 0x4A63C33Cu, 0x64180620u,
		0xE80B3288u, 0x0B935A12u, 0x0EFCE525u, 0x441DDE23u, 0xDDDBB8DDu, 0x4A7B4A81u,
		0x606D9C80u, 0x41A9AC4Du, 0x758614F4u, 0x9C94CC04u, 0xA00B2152u, 0xC2A9183Eu
	};

	// Field offsets inside the live metadata object.
	//
	// These are the newest-build layout. The on-foot ones come from the camera
	// metadata functions the game itself uses to read the values, so they are
	// stable; the pivots do move between builds, which is why this layer checks
	// the metadata hash before writing and refuses anything it did not ask for.
	struct Layout
	{
		int pedTpFov;
		int pedTpDistance;
		int pedTpPivotX;
		int pedFpFov;
		int pedAimFov;
		int pedAimDistance;
		int pedAimPivotX;
		int pedAimUseWeaponFov;
		int vehTpFov;
		int vehTpDistance;
		int vehTpPivotScale;
		int vehFpFov;
		int vehFpOffsetZ;
		int modelThirdHash;
		int modelFirstHash;

		// camFirstPersonShooterCameraMetadata extras.
		// Verified against live game memory (build 3407 / v102):
		//   MinPitch = 0x54 (84), MaxPitch = 0x58 (88),
		//   ShouldDisplayReticule = 0x66 (102),
		//   MaxFov (slider upper bound) = 0x228 (552), AimFov = 0x22C (556).
		// Auto-verified / resolved in ResolveFpsOffsets() before writing.
		int fpsAimFov;      // "AimFov"  - the FOV used while aiming in first person
		int fpsReticule;    // "ShouldDisplayReticule" - show the reticle unarmed
		int fpsMaxFov;      // upper FOV bound for the game's first-person FOV slider
		int fpsMinPitch;    // "MinPitch" (-60 default)
		int fpsMaxPitch;    // "MaxPitch" (+60 default)
	};

	const Layout LAYOUT =
	{
		48, 360, 256,   // on foot, third person
		36,             // on foot, first person (BaseFov)
		48, 360, 248, 827, // on foot, aiming (last one = useWeaponFovSetting flag)
		48, 360, 188,   // vehicle, third person
		84, 104,        // vehicle, first person (mounted)
		1232, 1244,     // CVehicleModelInfo camera hashes
		556, 102,       // first person extras: AimFov (0x22C), ShouldDisplayReticule (0x66)
		552, 84, 88     // first person MaxFov (0x228), MinPitch (0x54), MaxPitch (0x58)
	};

	// One camera to write this frame. Any offset left at -1 is skipped.
	struct WritePlan
	{
		unsigned int hash;
		int offFov;      float fov;
		int offDistance; float distance;
		int offPivot;    float pivot;
		int offFlag;     bool flag;
		const char *label;
		bool recordLastWrite;
	};

	WritePlan PlainPlan(unsigned int hash, int offFov, float fov, const char *label, bool recordLastWrite = true)
	{
		WritePlan p = { hash, offFov, fov, -1, 0.0f, -1, 0.0f, -1, false, label, recordLastWrite };
		return p;
	}

	float AsFloat(unsigned int bits)
	{
		float f;
		memcpy(&f, &bits, sizeof(f));
		return f;
	}
	bool g_available = false;
	bool g_tried = false;
	void *g_metadataPool = nullptr;
	unsigned long g_writeCount = 0;
	unsigned long g_lastReport = 0;

	// Set by FindMetadata: true when the match came from the ref node's nameHash
	// rather than the object's own hash field.
	bool g_lastMatchByRef = false;

	// The metadata object the current write is aimed at. Kept only so the
	// range-checked writers can report a byte offset in the log instead of a
	// raw address the reader cannot map back to a field.
	unsigned char *g_currentMetaBase = nullptr;

	// Last FOV written and the camera it went to - see CamMeta::LastWriteInfo.
	bool g_haveLastWrite = false;
	float g_lastWriteFov = 0.0f;
	unsigned int g_lastWriteHash = 0;

	int HexVal(char c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		return -1;
	}

	// Parses an IDA-style signature ("48 8D 0D ? ? ? ?") into bytes + wildcards.
	bool ParseSignature(const char *ida, unsigned char *bytes, bool *wild, int &len)
	{
		len = 0;
		const char *p = ida;
		while (*p)
		{
			while (*p == ' ')
				p++;
			if (!*p)
				break;

			if (*p == '?')
			{
				bytes[len] = 0;
				wild[len] = true;
				len++;
				p++;
			}
			else
			{
				int hi = HexVal(p[0]);
				int lo = HexVal(p[1]);
				if (hi < 0 || lo < 0)
					return false;

				bytes[len] = (unsigned char)((hi << 4) | lo);
				wild[len] = false;
				len++;
				p += 2;
			}

			if (len >= 96)
				break;
		}
		return len > 0;
	}

	uintptr_t FindPattern(const char *ida)
	{
		unsigned char bytes[96];
		bool wild[96];
		int len = 0;

		if (!ParseSignature(ida, bytes, wild, len))
			return 0;

		HMODULE module = GetModuleHandleW(nullptr);
		if (!module)
			return 0;

		MODULEINFO info{};
		if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
			return 0;

		unsigned char *base = (unsigned char *)info.lpBaseOfDll;
		const SIZE_T size = info.SizeOfImage;
		if (size < (SIZE_T)len)
			return 0;

		const SIZE_T last = size - (SIZE_T)len;
		for (SIZE_T i = 0; i <= last; i++)
		{
			bool ok = true;
			for (int j = 0; j < len; j++)
			{
				if (!wild[j] && base[i + j] != bytes[j])
				{
					ok = false;
					break;
				}
			}
			if (ok)
				return (uintptr_t)(base + i);
		}
		return 0;
	}

	// Resolves a RIP-relative operand: target = instr + instrLen + disp32.
	uintptr_t ResolveRipRelative(uintptr_t instr, int instrLen, int dispOffset)
	{
		const int32_t rel = *(int32_t *)(instr + dispOffset);
		return (uintptr_t)(instr + instrLen + rel);
	}

	// Walks the game's camera metadata pool looking for one hash.
	void *FindMetadata(unsigned int hash)
	{
		if (!g_metadataPool || hash == 0)
			return nullptr;

		// Always reflects the most recent call, so a failed lookup cannot leave a
		// stale "matched by ref" behind for the next verification.
		g_lastMatchByRef = false;

		// rage::pgCollection<camMetadataRef*>: { T* data; u16 count; u16 size; }
		unsigned char *pool = (unsigned char *)g_metadataPool;
		void **data = *(void ***)pool;
		const unsigned short count = *(unsigned short *)(pool + 8);

		if (!data || count == 0 || count > 4096)
			return nullptr;

		for (unsigned short i = 0; i < count; i++)
		{
			unsigned char *ref = (unsigned char *)data[i];
			int guard = 0;

			// camMetadataRef: nameHash at +0x0, pData at +0x8, pNext at +0x10
			//
			// The ref's own nameHash is the reliable key. Most metadata objects
			// also carry the hash at pData+0x8, but not all of them do -
			// camFirstPersonShooterCameraMetadata keeps a float (1.0) there
			// instead, which is why the first-person camera could never be found
			// by the object hash alone.
			while (ref && guard++ < 4096)
			{
				void *pData = *(void **)(ref + 0x8);
				if (pData)
				{
					const unsigned int key = *(unsigned int *)((unsigned char *)pData + 0x8);
					if (key == hash)
					{
						g_lastMatchByRef = false;
						return pData;
					}

					if (*(unsigned int *)ref == hash)
					{
						g_lastMatchByRef = true;
						return pData;
					}
				}
				ref = *(unsigned char **)(ref + 0x10);
			}
		}
		return nullptr;
	}

	// True when `meta` is the metadata we asked for. Accepts either the object's
	// own hash or the ref node's, because not every camera stores it in the
	// object - see FindMetadata.
	bool VerifyMeta(void *meta, unsigned int hash)
	{
		if (!meta)
			return false;
		if (g_lastMatchByRef)
			return true;
		return *(unsigned int *)((unsigned char *)meta + 0x8) == hash;
	}

	// A pointer is two 32-bit words: a garbage low half and a small high half
	// such as 0x00007FF7. Read as a float that high half is ~4.6e-41, a denormal
	// tiny number - which sails straight through a plain range check. Writing a
	// float over either half corrupts the pointer, and that is what takes the
	// script down. So an offset is only touched when nothing in its 8-byte window
	// looks like a pointer half.
	bool LooksLikePointerHalf(unsigned int raw)
	{
		if (raw == 0)
			return false;

		// 0x00007FF7 style: a plausible module/heap address high half.
		if (raw >= 0x00007F00u && raw <= 0x0000FFFFu)
			return true;

		const float f = AsFloat(raw);
		if (!(f == f)) // NaN bits - not an address half
			return false;

		const float a = fabsf(f);
		return a != 0.0f && a < 1.0e-30f; // denormal: pointer garbage
	}

	// Original values captured before our first write to each (address), so
	// disabling the mod, turning off metadata/aiming, or resetting a 0-based
	// option back to 0 restores the game's original memory cleanly.
	struct OriginalSlot
	{
		int size; // 1 or 4
		unsigned int bits;
	};

	std::map<uintptr_t, OriginalSlot> g_originals;

	void RecordOriginal(uintptr_t address, int size, unsigned int bits)
	{
		if (g_originals.find(address) == g_originals.end())
		{
			OriginalSlot slot = { size, bits };
			g_originals[address] = slot;
		}
	}

	void RestoreField(uintptr_t address)
	{
		std::map<uintptr_t, OriginalSlot>::iterator it = g_originals.find(address);
		if (it == g_originals.end())
			return;

		const OriginalSlot slot = it->second;
		g_originals.erase(it);

		if (address < 0x10000)
			return;
		if (IsBadWritePtr((void *)address, (UINT_PTR)slot.size))
			return;

		if (slot.size == 4)
			*(unsigned int *)address = slot.bits;
		else if (slot.size == 1)
			*(unsigned char *)address = (unsigned char)(slot.bits & 0xFFu);
	}

	void RestoreAllInternal()
	{
		for (std::map<uintptr_t, OriginalSlot>::const_iterator it = g_originals.begin(); it != g_originals.end(); ++it)
		{
			const uintptr_t address = it->first;
			const OriginalSlot &slot = it->second;
			if (address < 0x10000 || IsBadWritePtr((void *)address, (UINT_PTR)slot.size))
				continue;

			if (slot.size == 4)
				*(unsigned int *)address = slot.bits;
			else if (slot.size == 1)
				*(unsigned char *)address = (unsigned char)(slot.bits & 0xFFu);
		}
		g_originals.clear();
		g_haveLastWrite = false;
	}

	// Defined below; the range-checked writers sit above it so the verification
	// logic stays together.
	void WriteFloat(uintptr_t address, float value);

	// Writes a float ONLY if the field already holds something that could
	// plausibly be that kind of value.
	bool WriteFloatChecked(uintptr_t address, float value, float lo, float hi, const char *label)
	{
		if (address < 0x10000 || IsBadReadPtr((void *)address, sizeof(float)))
			return false;

		const unsigned int raw = *(const unsigned int *)address;
		const float current = AsFloat(raw);
		const bool plausible = !LooksLikePointerHalf(raw) &&
			(current == current) && current >= lo && current <= hi;

		if (!plausible)
		{
			// Reported once per label, not once per frame.
			static std::map<const char *, bool> warned;
			if (!warned[label])
			{
				warned[label] = true;
				Log::Line("metadata: '%s' - the field at +0x%X reads %.6g (0x%08X), which is not a "
					"plausible value for it. Not writing, so nothing else is damaged. "
					"The offset has moved on this game build.",
					label, (unsigned)(address - (uintptr_t)g_currentMetaBase), current, raw);
			}
			return false;
		}

		RecordOriginal(address, 4, raw);
		WriteFloat(address, value);
		return true;
	}

	// Same idea for a boolean: the byte must already be 0 or 1.
	bool WriteBoolChecked(uintptr_t address, bool value, const char *label)
	{
		if (address < 0x10000 || IsBadReadPtr((void *)address, 1))
			return false;

		const unsigned char current = *(unsigned char *)address;
		if (current > 1)
		{
			static std::map<const char *, bool> warned;
			if (!warned[label])
			{
				warned[label] = true;
				Log::Line("metadata: '%s' - the byte at +0x%X reads %u, not a 0/1 flag. "
					"Not writing.", label,
					(unsigned)(address - (uintptr_t)g_currentMetaBase), (unsigned)current);
			}
			return false;
		}

		if (IsBadWritePtr((void *)address, 1))
			return false;

		RecordOriginal(address, 1, (unsigned int)current);
		*(unsigned char *)address = value ? 1 : 0;
		g_writeCount++;
		return true;
	}

	void WriteFloat(uintptr_t address, float value)
	{
		if (address < 0x10000)
			return;
		if (value != value) // NaN guard
			return;
		if (IsBadWritePtr((void *)address, sizeof(float)))
			return;

		*(float *)address = value;
		g_writeCount++;
	}

	// Which metadata object the camera on screen is using, or 0 when it cannot be
	// worked out. Shared by the writer and the dumper so they cannot disagree.
	unsigned int ResolveActiveHash(int veh, bool inVeh, bool firstPerson)
	{
		if (!inVeh)
			return firstPerson ? HASH_PED_FIRST : HASH_PED_THIRD;

		if (veh == 0)
			return 0;

		unsigned char *handle = getScriptHandleBaseAddress(veh);
		if (!handle || IsBadReadPtr(handle + 0x20, sizeof(uintptr_t)))
			return 0;

		const uintptr_t modelInfo = *(uintptr_t *)(handle + 0x20);
		if (!modelInfo)
			return 0;

		const int offset = firstPerson ? LAYOUT.modelFirstHash : LAYOUT.modelThirdHash;
		if (IsBadReadPtr((void *)(modelInfo + offset), sizeof(unsigned int)))
			return 0;

		const unsigned int hash = *(unsigned int *)(modelInfo + offset);
		if (hash != 0 && FindMetadata(hash) != nullptr)
			return hash;

		// Fallback to the default vehicle camera if CVehicleModelInfo shifted on a
		// newer build and the read hash is not in the metadata pool.
		return firstPerson ? 0x2D96A816u : 0x623275C3u;
	}

	// Lists every metadata object the pool currently holds. Called the first time
	// a lookup fails, so the log shows whether the camera we wanted is in there
	// under a different hash, or is simply not loaded at that moment.
	void LogPoolHashes()
	{
		if (!g_metadataPool)
		{
			Log::Line("meta: metadata pool pointer is null");
			return;
		}

		unsigned char *pool = (unsigned char *)g_metadataPool;
		void **data = *(void ***)pool;
		const unsigned short count = *(unsigned short *)(pool + 8);

		Log::Line("meta: pool %p holds %u ref(s) - listing every hash present",
			g_metadataPool, (unsigned)count);

		if (!data || count == 0 || count > 4096)
		{
			Log::Line("meta: pool contents look wrong (data %p, count %u)", data, (unsigned)count);
			return;
		}

		for (unsigned short i = 0; i < count; i++)
		{
			unsigned char *ref = (unsigned char *)data[i];
			int guard = 0;

			while (ref && guard++ < 64)
			{
				void *pData = *(void **)(ref + 0x8);
				if (pData)
				{
					const unsigned int key = *(unsigned int *)((unsigned char *)pData + 0x8);
					const unsigned int refKey = *(unsigned int *)ref;
					// Both are printed: for most cameras they agree, but the
					// first-person camera carries a float where the object hash
					// would be, so only the ref key identifies it.
					Log::Line("meta:   [%u] data=%p hash=0x%08X ref=0x%08X",
						(unsigned)i, pData, key, refKey);
				}
				ref = *(unsigned char **)(ref + 0x10);
			}
		}
	}

	// Resolves the metadata for a plan, verifies the hash it came back with, then
	// writes the fields the plan asks for. Reports once per camera when the
	// object is missing, so the log explains a slider that does nothing.
	void ApplyPlan(const WritePlan &p)
	{
		if (p.hash == 0)
			return;

		void *meta = FindMetadata(p.hash);
		if (!meta)
		{
			// The pool is dumped once, not once per missing camera. With the
			// weapon cameras several can be absent at the same time (mission
			// cameras load on demand), and ten copies of a 1600 line listing
			// answer nothing.
			static bool reportedPool = false;
			if (!reportedPool)
			{
				reportedPool = true;
				LogPoolHashes();
			}

			const unsigned long now = GetTickCount();
			if (now - g_lastReport > 5000)
			{
				g_lastReport = now;
				Log::Line("metadata: no live metadata object for hash 0x%X (%s) - "
					"its sliders idle for this camera", p.hash, p.label);
			}
			return;
		}

		const uintptr_t m = (uintptr_t)meta;

		// Never write into anything that is not the metadata we asked for.
		if (!VerifyMeta(meta, p.hash))
			return;

		g_currentMetaBase = (unsigned char *)meta;

		if (p.offFov >= 0)
		{
			if (WriteFloatChecked(m + p.offFov, p.fov, 1.0f, 180.0f, p.label) && p.recordLastWrite)
			{
				g_haveLastWrite = true;
				g_lastWriteFov = p.fov;
				g_lastWriteHash = p.hash;
			}
		}
		if (p.offDistance >= 0)
			WriteFloatChecked(m + p.offDistance, p.distance, -10.0f, 50.0f, p.label);
		if (p.offPivot >= 0)
			WriteFloatChecked(m + p.offPivot, p.pivot, -20.0f, 20.0f, p.label);

		if (p.offFlag >= 0)
			WriteBoolChecked(m + p.offFlag, p.flag, p.label);
	}

	// Resolves the byte offset of ShouldApplyWeaponFov inside an aiming
	// camera metadata object (camThirdPersonPedAimCameraMetadata and its cover,
	// melee, and assisted-aim subclasses). Checks LAYOUT.pedAimUseWeaponFov first
	// and scans [0x220..0x480] for the 6-bool block [93..98]
	// {ShouldDisplayReticule, 0, PersistReticule, ShouldApplyWeaponFov, 1, 0}.
	int ResolveWeaponFovFlagOffsetFor(void *meta, unsigned int hash)
	{
		static std::map<unsigned int, int> cache;
		std::map<unsigned int, int>::const_iterator it = cache.find(hash);
		if (it != cache.end())
			return it->second;

		if (!meta)
			return LAYOUT.pedAimUseWeaponFov;

		const unsigned char *m = (const unsigned char *)meta;
		if (!IsBadReadPtr(m + 0x220, 0x280))
		{
			const int def = LAYOUT.pedAimUseWeaponFov;
			if (m[def - 3] <= 1 && m[def - 2] == 0 && m[def - 1] <= 1 &&
				m[def] <= 1 && m[def + 1] == 1 && m[def + 2] == 0)
			{
				cache[hash] = def;
				return def;
			}

			for (int off = 0x220; off <= 0x480; off++)
			{
				if (m[off - 3] <= 1 && m[off - 2] == 0 && m[off - 1] <= 1 &&
					m[off] == 1 && m[off + 1] == 1 && m[off + 2] == 0)
				{
					cache[hash] = off;
					Log::Line("weapons: auto-located ShouldApplyWeaponFov for 0x%X at +0x%X (%d)", hash, off, off);
					return off;
				}
			}
		}

		cache[hash] = LAYOUT.pedAimUseWeaponFov;
		return LAYOUT.pedAimUseWeaponFov;
	}

	int ResolveWeaponFovFlagOffset()
	{
		void *meta = FindMetadata(HASH_PED_AIM);
		if (!VerifyMeta(meta, HASH_PED_AIM))
			return LAYOUT.pedAimUseWeaponFov;
		return ResolveWeaponFovFlagOffsetFor(meta, HASH_PED_AIM);
	}

	// Writes the per-weapon aiming FOVs. Every entry left at 0 is skipped (and
	// restored if previously touched), so a fresh install writes none of them.
	void ApplyWeaponFovs(const Settings &s)
	{
		for (int i = 0; i < (int)(sizeof(kWeaponCams) / sizeof(kWeaponCams[0])); i++)
		{
			const WeaponCam &w = kWeaponCams[i];
			const float fov = s.*(w.fov);
			if (fov <= 0.0f)
			{
				// Restore per-weapon cameras returned to 0 (HASH_PED_AIM is owned by
				// the main aiming plan while metaAiming is enabled).
				if (w.hash != HASH_PED_AIM)
				{
					void *meta = FindMetadata(w.hash);
					if (VerifyMeta(meta, w.hash))
					{
						RestoreField((uintptr_t)meta + w.offFov);
						if (w.clearWeaponFovFlag)
						{
							const int flagOff = ResolveWeaponFovFlagOffsetFor(meta, w.hash);
							RestoreField((uintptr_t)meta + flagOff);
						}
					}
				}
				continue;
			}

			ApplyPlan(PlainPlan(w.hash, w.offFov, fov, w.label, false));

			// The game prefers the weapon's OWN fov while this flag is set, so
			// the BaseFov written above would be ignored. Clearing it is what
			// makes the per-weapon sliders do anything at all; WriteBoolChecked
			// reads the byte first, so an offset that moved on another build is
			// refused instead of overwriting a neighbour.
			if (w.clearWeaponFovFlag)
			{
				void *meta = FindMetadata(w.hash);
				if (VerifyMeta(meta, w.hash))
				{
					const int flagOff = ResolveWeaponFovFlagOffsetFor(meta, w.hash);
					g_currentMetaBase = (unsigned char *)meta;
					WriteBoolChecked((uintptr_t)meta + flagOff, false, w.label);
				}
			}

			// Reported once per camera, so the log answers "is this slider
			// being written at all" without spamming a line per frame.
			static std::map<const char *, bool> logged;
			if (!logged[w.label])
			{
				logged[w.label] = true;
				Log::Line("weapons: %s fov %.1f -> 0x%X%s", w.label, fov, w.hash,
					w.clearWeaponFovFlag ? " (weapon fov flag cleared)" : "");
			}
		}
	}

	void RestoreAiming()
	{
		void *aimMeta = FindMetadata(HASH_PED_AIM);
		if (VerifyMeta(aimMeta, HASH_PED_AIM))
		{
			const int flagOff = ResolveWeaponFovFlagOffsetFor(aimMeta, HASH_PED_AIM);
			const uintptr_t m = (uintptr_t)aimMeta;
			RestoreField(m + LAYOUT.pedAimFov);
			RestoreField(m + LAYOUT.pedAimDistance);
			RestoreField(m + LAYOUT.pedAimPivotX);
			RestoreField(m + flagOff);
		}
		for (int i = 0; i < (int)(sizeof(kWeaponCams) / sizeof(kWeaponCams[0])); i++)
		{
			const WeaponCam &w = kWeaponCams[i];
			void *meta = FindMetadata(w.hash);
			if (VerifyMeta(meta, w.hash))
			{
				RestoreField((uintptr_t)meta + w.offFov);
				if (w.clearWeaponFovFlag)
				{
					const int flagOff = ResolveWeaponFovFlagOffsetFor(meta, w.hash);
					RestoreField((uintptr_t)meta + flagOff);
				}
			}
		}
	}

	struct FpsOffsets
	{
		int minPitch;
		int maxPitch;
		int reticule;
		int maxFov;
		int aimFov;
	};

	FpsOffsets ResolveFpsOffsets(void *meta)
	{
		static bool resolved = false;
		static FpsOffsets off =
		{
			LAYOUT.fpsMinPitch, LAYOUT.fpsMaxPitch, LAYOUT.fpsReticule,
			LAYOUT.fpsMaxFov, LAYOUT.fpsAimFov
		};
		if (resolved || !meta)
			return off;

		const unsigned char *m = (const unsigned char *)meta;
		if (!IsBadReadPtr(m, 0x2C0))
		{
			// 1. MinPitch (-60) / MaxPitch (60) + ShouldDisplayReticule (+0x12)
			for (int o = 0x48; o <= 0x70; o += 4)
			{
				const float f0 = AsFloat(*(const unsigned int *)(m + o));
				const float f1 = AsFloat(*(const unsigned int *)(m + o + 4));
				if (fabsf(f0 - (-60.0f)) < 0.01f && fabsf(f1 - 60.0f) < 0.01f)
				{
					off.minPitch = o;
					off.maxPitch = o + 4;
					// 4 consecutive bools sit at o + 0x10..o + 0x13: {0, 1, *, 1}
					if (m[o + 0x10] <= 1 && m[o + 0x11] <= 1 && m[o + 0x12] <= 1 && m[o + 0x13] <= 1)
						off.reticule = o + 0x12;
					break;
				}
			}

			// 2. [180, 3, 60, 55 (MaxFov), 42 (AimFov)]
			for (int o = 0x1B0; o <= 0x280; o += 4)
			{
				const float f0 = AsFloat(*(const unsigned int *)(m + o));
				const float f1 = AsFloat(*(const unsigned int *)(m + o + 4));
				const float f2 = AsFloat(*(const unsigned int *)(m + o + 8));
				const float f3 = AsFloat(*(const unsigned int *)(m + o + 12));
				const float f4 = AsFloat(*(const unsigned int *)(m + o + 16));
				if (fabsf(f0 - 180.0f) < 0.01f && fabsf(f1 - 3.0f) < 0.01f &&
					fabsf(f2 - 60.0f) < 0.01f && f3 > 1.0f && f3 < 180.0f &&
					f4 > 1.0f && f4 < 180.0f)
				{
					off.maxFov = o + 12;
					off.aimFov = o + 16;
					break;
				}
			}

			Log::Line("metadata: fps offsets resolved - minPitch=+0x%X maxPitch=+0x%X reticle=+0x%X maxFov=+0x%X aimFov=+0x%X",
				off.minPitch, off.maxPitch, off.reticule, off.maxFov, off.aimFov);
			resolved = true;
		}
		return off;
	}

	// Options that live on camFirstPersonShooterCameraMetadata.
	// Everything goes through range-checked writers and restores cleanly when
	// set back to 0 / Default.
	void ApplyFirstPersonExtras(const Settings &s)
	{
		void *meta = FindMetadata(HASH_PED_FIRST);
		if (!VerifyMeta(meta, HASH_PED_FIRST))
			return;

		const FpsOffsets off = ResolveFpsOffsets(meta);
		const uintptr_t m = (uintptr_t)meta;
		g_currentMetaBase = (unsigned char *)meta;

		// Write the slider's upper FOV bound too so the game's Pause Menu
		// first-person FOV slider cannot clamp or override pedFpFov.
		if (s.pedFpFov > 0.0f)
			WriteFloatChecked(m + off.maxFov, s.pedFpFov, 1.0f, 180.0f, "first person max fov");
		else
			RestoreField(m + off.maxFov);

		// An FOV is never below 1 or above 180, so anything else in this field
		// means the layout moved.
		RestoreField(m + off.aimFov);

		// Also mirror to the alternate first-person shooter metadata
		// referenced by camGameplayDirectorMetadata.
		void *altMeta = FindMetadata(HASH_PED_FIRST_ALT);
		if (VerifyMeta(altMeta, HASH_PED_FIRST_ALT))
		{
			const uintptr_t am = (uintptr_t)altMeta;
			g_currentMetaBase = (unsigned char *)altMeta;
			if (s.pedFpFov > 0.0f)
			{
				WriteFloatChecked(am + LAYOUT.pedFpFov, s.pedFpFov, 1.0f, 180.0f, "first person alt fov");
				WriteFloatChecked(am + off.maxFov, s.pedFpFov, 1.0f, 180.0f, "first person alt max fov");
			}
			else
			{
				RestoreField(am + LAYOUT.pedFpFov);
				RestoreField(am + off.maxFov);
			}
			RestoreField(am + off.aimFov);
		}

		if (s.pedFpReticle == 1)
			WriteBoolChecked(m + off.reticule, true, "first person reticle");
		else if (s.pedFpReticle == 2)
			WriteBoolChecked(m + off.reticule, false, "first person reticle");
		else
			RestoreField(m + off.reticule);

		// Metadata MinPitch/MaxPitch default to -60/+60; widen them if the user
		// asks for a tilt limit past 60 deg so the native clamp can reach it.
		if (s.pedFpTiltUp > 60.0f)
			WriteFloatChecked(m + off.maxPitch, s.pedFpTiltUp, 1.0f, 90.0f, "first person max pitch");
		else
			RestoreField(m + off.maxPitch);

		if (s.pedFpTiltDown > 60.0f)
			WriteFloatChecked(m + off.minPitch, -s.pedFpTiltDown, -90.0f, -1.0f, "first person min pitch");
		else
			RestoreField(m + off.minPitch);
	}

	void ApplyExtraCameras(const Settings &s)
	{
		// 1. Parachute cameras (camFollowParachuteCameraMetadata).
		for (int i = 0; i < (int)(sizeof(kParachuteCams) / sizeof(kParachuteCams[0])); i++)
		{
			const unsigned int h = kParachuteCams[i];
			void *meta = FindMetadata(h);
			if (!VerifyMeta(meta, h))
				continue;

			const uintptr_t m = (uintptr_t)meta;
			g_currentMetaBase = (unsigned char *)meta;
			if (s.parachuteFov > 0.0f)
				WriteFloatChecked(m + LAYOUT.pedTpFov, s.parachuteFov, 1.0f, 180.0f, "parachute fov");
			else
				RestoreField(m + LAYOUT.pedTpFov);

			if (s.parachuteDistance > 0.0f)
				WriteFloatChecked(m + LAYOUT.pedTpDistance, s.parachuteDistance, -10.0f, 50.0f, "parachute distance");
			else
				RestoreField(m + LAYOUT.pedTpDistance);
		}

		// 2. Drive-by / vehicle aiming cameras (36 camThirdPersonVehicleAimCameraMetadata).
		for (int i = 0; i < (int)(sizeof(kVehicleAimCams) / sizeof(kVehicleAimCams[0])); i++)
		{
			const unsigned int h = kVehicleAimCams[i];
			void *meta = FindMetadata(h);
			if (!VerifyMeta(meta, h))
				continue;

			const uintptr_t m = (uintptr_t)meta;
			const int flagOff = ResolveWeaponFovFlagOffsetFor(meta, h);
			g_currentMetaBase = (unsigned char *)meta;
			if (s.vehAimFov > 0.0f)
			{
				WriteFloatChecked(m + OFF_FOV_TP_AIM, s.vehAimFov, 1.0f, 180.0f, "vehicle aim fov");
				WriteBoolChecked(m + flagOff, false, "vehicle aim weapon fov flag");
			}
			else
			{
				RestoreField(m + OFF_FOV_TP_AIM);
				RestoreField(m + flagOff);
			}
		}
	}
}

namespace CamMeta
{
	void Init()
	{
		if (g_tried)
			return;
		g_tried = true;

		g_available = false;
		g_metadataPool = nullptr;

		Log::Line("metadata: game version %d - locating the camera metadata pool",
			(int)getGameVersion());

		// 48 8D 94 24 40 01 00 00   lea rdx, [rsp+140]
		// 48 8D 0D ?? ?? ?? ??      lea rcx, [camera metadata pool]
		const uintptr_t match = FindPattern("48 8D 94 24 40 01 00 00 48 8D 0D ? ? ? ?");
		if (match)
		{
			const uintptr_t lea = match + 8;
			g_metadataPool = (void *)ResolveRipRelative(lea, 7, 3);
			Log::Line("metadata: camera metadata pool at 0x%llX",
				(unsigned long long)(uintptr_t)g_metadataPool);
			g_available = true;
		}
		else
		{
			Log::Line("metadata: WARNING - the camera metadata pool was not found on this "
				"build. FOV / distance / pivot will do nothing; everything else still works.");
		}

		Log::Line("metadata: layer %s", g_available ? "active" : "disabled");
	}

	bool Available() { return g_available; }

	// Every pool entry whose +0x24 or +0x30 reads as a plausible field of view.
	// Camera metadata carries an FOV; the other 1500-odd entries in the pool do
	// not, so this is what turns "1609 hashes" into a short list of cameras.
	void ListHashes()
	{
		if (!g_available)
		{
			Log::Line("list: metadata layer is unavailable on this build");
			return;
		}

		unsigned char *pool = (unsigned char *)g_metadataPool;
		void **data = *(void ***)pool;
		const unsigned short count = *(unsigned short *)(pool + 8);

		if (!data || count == 0 || count > 4096)
		{
			Log::Line("list: the pool looks wrong (data %p, count %u)", data, (unsigned)count);
			return;
		}

		Log::Line("list: ==== %u ref(s) in the pool - listing the camera-like ones ====",
			(unsigned)count);

		int shown = 0;
		for (unsigned short i = 0; i < count; i++)
		{
			unsigned char *ref = (unsigned char *)data[i];
			int guard = 0;

			while (ref && guard++ < 64)
			{
				unsigned char *pData = *(unsigned char **)(ref + 0x8);
				if (pData)
				{
					const unsigned int key = *(unsigned int *)(pData + 0x8);
					const float f24 = AsFloat(*(unsigned int *)(pData + 0x24));
					const float f30 = AsFloat(*(unsigned int *)(pData + 0x30));

					const bool ok24 = (f24 == f24) && f24 > 1.0f && f24 < 179.0f;
					const bool ok30 = (f30 == f30) && f30 > 1.0f && f30 < 179.0f;

					if (ok24 || ok30)
					{
						Log::Line("list:   hash=0x%08X  +0x24=%8.3f  +0x30=%8.3f", key, f24, f30);
						shown++;
					}
				}
				ref = *(unsigned char **)(ref + 0x10);
			}
		}

		Log::Line("list: ==== %d camera-like entr(ies) ====", shown);
		Log::Line("list: change ONE field of view in the game's own camera settings, run this "
			"again, and the hash whose value moved is that camera");
	}

	const char *Status()
	{
		return g_available ? "metadata: active" : "metadata: unavailable";
	}

	bool LastWriteInfo(float &fov, unsigned int &hash)
	{
		if (!g_haveLastWrite)
			return false;

		fov = g_lastWriteFov;
		hash = g_lastWriteHash;
		return true;
	}

	void Reset()
	{
		g_writeCount = 0;
		g_lastReport = 0;
	}

	// Baseline for diff mode: the raw words of the last dump, and which camera it
	// was. Comparing two dumps taken in the SAME session is the only reliable way
	// to attribute a single game setting - a dump from another session differs in
	// every pointer and in every value the player moved in between.
	unsigned int g_baseHash = 0;
	std::map<int, unsigned int> g_base;

	// Probe bookkeeping: the object/offset being probed, what was there before,
	// and whether a refusal has already been reported for it.
	void *g_probeMeta = nullptr;
	int g_probeOffset = -1;
	unsigned int g_probeOriginal = 0;
	bool g_probeSeen = false;
	int g_probeWarnedOffset = -2;

	// Reports a refused probe once per offset. Without this a probe that does
	// nothing looks like the tool is broken.
	void WarnProbe(int offset, const char *why)
	{
		if (g_probeWarnedOffset == offset)
			return;

		g_probeWarnedOffset = offset;
		Log::Line("probe: cannot write +0x%03X - %s", offset, why);
	}

	// Writes the whole live metadata object of the camera on screen to the log,
	// as offset / raw bits / float, and - when this camera was dumped before in
	// this session - a list of just the offsets that changed since.
	//
	// This is how new field offsets are found without guessing: dump, change ONE
	// setting in the game's own camera menu, dump again. The offset that moved is
	// that setting's field. Nothing is copied from anyone; the answer comes from
	// the game itself.
	void Dump(int ped, int veh, bool inVeh, bool firstPerson)
	{
		(void)ped;

		if (!g_available)
		{
			Log::Line("dump: metadata layer is unavailable on this build - nothing to dump");
			return;
		}

		const unsigned int hash = ResolveActiveHash(veh, inVeh, firstPerson);
		if (hash == 0)
		{
			Log::Line("dump: could not work out the hash of the camera on screen");
			return;
		}

		void *meta = FindMetadata(hash);
		if (!VerifyMeta(meta, hash))
		{
			Log::Line("dump: no verified metadata object for hash 0x%X (%s, %s)",
				hash, inVeh ? "vehicle" : "on foot", firstPerson ? "first person" : "third person");
			return;
		}

		const unsigned char *m = (const unsigned char *)meta;

		// 0x0 .. 0x600. The fields we know of top out inside this range, and
		// reading past the allocation would buy nothing.
		const int kBytes = 0x600;

		Log::Line("dump: ==== camera metadata 0x%X  %s, %s ====", hash,
			inVeh ? "vehicle" : "on foot", firstPerson ? "first person" : "third person");

		// ---- full dump ---------------------------------------------------
		Log::Line("dump: offset  raw       float         | offset  raw       float");
		for (int off = 0; off < kBytes; off += 0x10)
		{
			char line[320];
			int n = 0;

			for (int i = 0; i < 4; i++)
			{
				const int o = off + i * 4;
				const unsigned int raw = *(const unsigned int *)(m + o);
				const float f = AsFloat(raw);

				char cell[80];
				if (f == f && f > -1.0e7f && f < 1.0e7f)
					_snprintf_s(cell, sizeof(cell), _TRUNCATE, "+0x%03X   %08X  %12.5f |", o, raw, f);
				else
					_snprintf_s(cell, sizeof(cell), _TRUNCATE, "+0x%03X   %08X  %12s |", o, raw, "-");

				n += _snprintf_s(line + n, sizeof(line) - n, _TRUNCATE, "%s", cell);
			}

			Log::Line("dump: %s", line);
		}

		// ---- what changed since the last dump of this camera --------------
		if (g_baseHash == hash && !g_base.empty())
		{
			Log::Line("dump: ---- changes since the previous dump of this camera ----");

			int changed = 0;
			for (int off = 0; off < kBytes; off += 4)
			{
				const unsigned int raw = *(const unsigned int *)(m + off);

				std::map<int, unsigned int>::const_iterator it = g_base.find(off);
				if (it == g_base.end() || it->second == raw)
					continue;

				Log::Line("dump: CHANGED +0x%03X  %08X %12.5f   ->   %08X %12.5f",
					off, it->second, AsFloat(it->second), raw, AsFloat(raw));
				changed++;
			}

			Log::Line("dump: %d offset(s) changed - the game setting you moved is among these",
				changed);
		}
		else
		{
			Log::Line("dump: baseline captured. Now change ONE setting in the game's own "
				"camera menu, then dump again - the change list will name its offset.");
		}

		// ---- this dump becomes the baseline for the next one --------------
		g_base.clear();
		for (int off = 0; off < kBytes; off += 4)
			g_base[off] = *(const unsigned int *)(m + off);
		g_baseHash = hash;

		Log::Line("dump: ==== end ====");
	}

	// Dev aid. Writes one float into the live metadata of the camera in use, so
	// a field can be identified by watching the camera react instead of reading
	// someone else's notes.
	//
	// The write only happens while the menu is open AND the probe is enabled, so
	// closing the menu - or turning the probe off - restores the byte that was
	// there before. Nothing here can persist once the menu is closed.
	void Probe(const Settings &s, int ped, int veh, bool inVeh, bool firstPerson, bool menuOpen)
	{
		(void)ped;
		const bool active = s.probeEnabled && menuOpen;

		// Nothing to do while the probe is neither running nor pending an undo.
		if (!active && !g_probeSeen)
			return;

		// ---- put it back when the probe stops ----------------------------
		if (!active)
		{
			if (g_probeMeta && g_probeOffset >= 0)
			{
				unsigned char *restore = (unsigned char *)g_probeMeta + g_probeOffset;
				if (!IsBadWritePtr(restore, sizeof(float)))
				{
					*(unsigned int *)restore = g_probeOriginal;
					Log::Line("probe: +0x%03X restored to %g", g_probeOffset, AsFloat(g_probeOriginal));
				}
			}
			g_probeSeen = false;
			g_probeMeta = nullptr;
			return;
		}

		if (!g_available)
		{
			WarnProbe(s.probeOffset, "the metadata layer is unavailable on this build");
			return;
		}

		const unsigned int hash = ResolveActiveHash(veh, inVeh, firstPerson);
		if (hash == 0)
		{
			WarnProbe(s.probeOffset, "could not work out the hash of the camera on screen");
			return;
		}

		void *meta = FindMetadata(hash);
		if (!VerifyMeta(meta, hash))
		{
			WarnProbe(s.probeOffset, "no verified metadata object for this camera");
			return;
		}

		// ---- write ---------------------------------------------------------
		// Keep the write inside the object we know the size of.
		if (s.probeOffset < 0 || (s.probeOffset + (int)sizeof(float)) > 0x600)
		{
			WarnProbe(s.probeOffset, "offset is outside the metadata object");
			return;
		}

		unsigned char *target = (unsigned char *)meta + s.probeOffset;
		if (IsBadWritePtr(target, sizeof(float)))
		{
			WarnProbe(s.probeOffset, "address is not writable");
			return;
		}

		// Remember what was there when this offset/object is first probed,
		// restoring any previously probed slot first if the user switched camera
		// or changed the offset while probing.
		if (!g_probeSeen || g_probeOffset != s.probeOffset || g_probeMeta != meta)
		{
			if (g_probeSeen && g_probeMeta && g_probeOffset >= 0)
			{
				unsigned char *prev = (unsigned char *)g_probeMeta + g_probeOffset;
				if (!IsBadWritePtr(prev, sizeof(float)))
					*(unsigned int *)prev = g_probeOriginal;
			}

			g_probeMeta = meta;
			g_probeOffset = s.probeOffset;
			g_probeSeen = true;
			g_probeOriginal = *(unsigned int *)target;

			Log::Line("probe: +0x%03X was %08X (%g) - writing %g while the menu is open",
				s.probeOffset, g_probeOriginal, AsFloat(g_probeOriginal), s.probeValue);
		}

		*(float *)target = s.probeValue;
	}

	// ----------------------------------------------------------------------
	// Automatic field sweep.
	//
	// Perturbs one offset at a time and measures the camera's answer through
	// natives - the only thing that matters is whether the picture changed and
	// how. That tells the option apart without any human watching:
	//
	//     position moved      -> an origin / distance field
	//     rotation changed    -> a pitch, yaw or clamp field
	//     FOV changed         -> a fov field
	//     value bouncing      -> shake
	//     nothing at all      -> padding, or a field the camera is not using
	//
	// Every value is restored, and it only runs while the menu is open, so
	// closing the menu always ends it.
	// ----------------------------------------------------------------------
	const int SWEEP_BYTES = 0x600;
	const int SWEEP_HOLD_FRAMES = 20;   // long enough for the camera to lerp

	bool g_sweepActive = false;
	int  g_sweepOffset = 0;
	int  g_sweepNext = 0;               // where the next run resumes
	int  g_sweepLastBeat = -1;          // last offset the heartbeat was logged at
	unsigned int g_sweepLastHash = 0;

	// One-shot message for the on-screen notification. The sweep takes minutes
	// and the player cannot watch the log or the clock, so the mod says what
	// happened instead of leaving them guessing.
	char g_msgBuf[160];
	bool g_msgPending = false;

	void PostMessage(const char *fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		_vsnprintf_s(g_msgBuf, sizeof(g_msgBuf), _TRUNCATE, fmt, args);
		va_end(args);
		g_msgPending = true;
	}

	int  g_sweepPhase = 0;              // 0 write, 1 measure
	int  g_sweepFrame = 0;
	unsigned int g_sweepOriginal = 0;
	void *g_sweepMeta = nullptr;
	float g_sweepPrePos[3] = { 0.0f, 0.0f, 0.0f };
	float g_sweepPreRot[3] = { 0.0f, 0.0f, 0.0f };
	float g_sweepPreFov = 0.0f;
	int  g_sweepHits = 0;

	void RestoreSweepPending()
	{
		if (g_sweepPhase == 1 && g_sweepMeta && g_sweepOffset >= 0 && g_sweepOffset <= SWEEP_BYTES - 4)
		{
			unsigned char *restore = (unsigned char *)g_sweepMeta + g_sweepOffset;
			if (!IsBadWritePtr(restore, sizeof(float)))
				*(unsigned int *)restore = g_sweepOriginal;
		}
		g_sweepPhase = 0;
		g_sweepMeta = nullptr;
	}

	void SweepSample(float pos[3], float rot[3], float &fov)
	{
		Vector3 p = CAM::GET_GAMEPLAY_CAM_COORD();
		Vector3 r = CAM::GET_GAMEPLAY_CAM_ROT(2);
		pos[0] = p.x; pos[1] = p.y; pos[2] = p.z;
		rot[0] = r.x; rot[1] = r.y; rot[2] = r.z;
		fov = CAM::GET_GAMEPLAY_CAM_FOV();
	}

	void Sweep(Settings &s, int ped, int veh, bool inVeh, bool firstPerson, bool menuOpen)
	{
		(void)menuOpen;

		// Stopping: put back whatever the sweep was touching and report.
		if (!s.sweepEnabled)
		{
			if (g_sweepActive)
			{
				RestoreSweepPending();
				g_sweepActive = false;
				// Hand the idle animations back.
				if (ped != 0 && ENTITY::DOES_ENTITY_EXIST(ped))
				{
					PED::SET_PED_CAN_PLAY_AMBIENT_ANIMS(ped, TRUE);
					PED::SET_PED_CAN_PLAY_AMBIENT_BASE_ANIMS(ped, TRUE);
				}
				g_sweepNext = g_sweepOffset;
				Log::Line("sweep: paused at +0x%03X after %d hit(s)", g_sweepOffset, g_sweepHits);
				PostMessage("Cam Control dev: sweep PAUSED at +0x%03X (%d hits) - turn it back on to resume",
					g_sweepOffset, g_sweepHits);
			}
			return;
		}

		// Idle animations keep the character shifting, and the camera drifts with
		// him - which reads as a false hit on whatever offset is being tested, and
		// forces the player to sit on a key to interrupt them. Switching the
		// ambient animations off for the duration keeps the subject still.
		if (ped != 0 && ENTITY::DOES_ENTITY_EXIST(ped))
		{
			const BOOL toggle = s.sweepDisableIdleAnims ? FALSE : TRUE;
			PED::SET_PED_CAN_PLAY_AMBIENT_ANIMS(ped, toggle);
			PED::SET_PED_CAN_PLAY_AMBIENT_BASE_ANIMS(ped, toggle);
		}

		if (!g_available)
			return;

		const unsigned int hash = ResolveActiveHash(veh, inVeh, firstPerson);
		if (hash == 0)
			return;

		void *meta = FindMetadata(hash);
		if (!VerifyMeta(meta, hash))
			return;

		if (!g_sweepActive)
		{
			g_sweepActive = true;

			// Resume where the last pass stopped, unless the camera changed - a
			// different camera is a different object, so it starts fresh.
			if (g_sweepLastHash == hash && g_sweepNext > 0)
				g_sweepOffset = g_sweepNext;
			else
			{
				g_sweepOffset = (s.sweepFrom >= 0) ? (s.sweepFrom & ~3) : 0;
				g_sweepHits = 0;
			}
			g_sweepLastHash = hash;
			g_sweepMeta = meta;
			g_sweepPhase = 0;
			g_sweepFrame = 0;

			Log::Line("sweep: ==== start at +0x%03X  hash 0x%X  %s, %s ====", g_sweepOffset, hash,
				inVeh ? "vehicle" : "on foot", firstPerson ? "first person" : "third person");
			Log::Line("sweep: runs until switched off - the log reports progress");
			PostMessage("Cam Control dev: sweep STARTED at +0x%03X", g_sweepOffset);
			return;
		}

		// If the player switched cameras mid-sweep, restore the old one first.
		if (g_sweepMeta != meta)
		{
			RestoreSweepPending();
			g_sweepMeta = meta;
			g_sweepLastHash = hash;
			g_sweepOffset = (s.sweepFrom >= 0) ? (s.sweepFrom & ~3) : 0;
			g_sweepHits = 0;
		}

		if (g_sweepOffset > SWEEP_BYTES - 4)
		{
			RestoreSweepPending();
			Log::Line("sweep: ==== done - %d hit(s) across this camera ====", g_sweepHits);
			PostMessage("Cam Control dev: sweep DONE - %d offsets moved the camera. See the log.",
				g_sweepHits);
			g_sweepActive = false;
			g_sweepNext = 0;   // a fresh pass starts from SweepFrom
			s.sweepEnabled = false;
			if (ped != 0 && ENTITY::DOES_ENTITY_EXIST(ped))
			{
				PED::SET_PED_CAN_PLAY_AMBIENT_ANIMS(ped, TRUE);
				PED::SET_PED_CAN_PLAY_AMBIENT_BASE_ANIMS(ped, TRUE);
			}
			return;
		}

		unsigned char *base = (unsigned char *)meta + g_sweepOffset;

		// Heartbeat, so it is obvious the sweep is still working. Logged once per
		// 128-byte block, not once per frame.
		if ((g_sweepOffset & ~0x7F) != (g_sweepLastBeat & ~0x7F))
		{
			g_sweepLastBeat = g_sweepOffset;
			Log::Line("sweep: ... at +0x%03X  (%d hit(s) so far)", g_sweepOffset, g_sweepHits);
		}

		if (IsBadWritePtr(base, sizeof(float)))
		{
			g_sweepOffset += 4;
			g_sweepPhase = 0;
			return;
		}

		const unsigned int original = *(unsigned int *)base;
		const float originalF = AsFloat(original);

		// Skip anything that is not a plausible float, and anything whose 8-byte
		// window looks like a pointer: those are addresses, hashes or padding, and
		// writing a float over them is how a sweep crashes the game.
		const unsigned int neighbour =
			(g_sweepOffset + 8 <= SWEEP_BYTES) ? *(const unsigned int *)(base + 4) : 0u;

		if (!(originalF == originalF) || originalF < -1.0e6f || originalF > 1.0e6f ||
			LooksLikePointerHalf(original) || LooksLikePointerHalf(neighbour))
		{
			g_sweepOffset += 4;
			g_sweepPhase = 0;
			return;
		}

		if (g_sweepPhase == 0)
		{
			float pos[3], rot[3], fov;
			SweepSample(pos, rot, fov);
			for (int i = 0; i < 3; i++) { g_sweepPrePos[i] = pos[i]; g_sweepPreRot[i] = rot[i]; }
			g_sweepPreFov = fov;

			g_sweepOriginal = original;
			g_sweepMeta = meta;

			// Perturb relative to what is already there, so the size suits the
			// field: doubling a 0.9 distance is visible, and turning a 0 flag
			// into 1 exercises a toggle.
			float test = (originalF > -0.001f && originalF < 0.001f) ? 1.0f : originalF * 2.0f;
			if (test > 1000.0f) test = 1000.0f;
			if (test < -1000.0f) test = -1000.0f;

			*(float *)base = test;
			g_sweepPhase = 1;
			g_sweepFrame = 0;
			return;
		}

		// Phase 1: let the camera settle, then measure.
		if (++g_sweepFrame < SWEEP_HOLD_FRAMES)
			return;

		float pos[3], rot[3], fov;
		SweepSample(pos, rot, fov);

		float dPos = 0.0f, dRot = 0.0f;
		for (int i = 0; i < 3; i++)
		{
			const float dp = pos[i] - g_sweepPrePos[i];
			const float dr = rot[i] - g_sweepPreRot[i];
			dPos += dp * dp;
			dRot += dr * dr;
		}
		dPos = sqrtf(dPos);
		dRot = sqrtf(dRot);
		const float dFov = fabsf(fov - g_sweepPreFov);

		// Report only what a player would notice. The thresholds are a little
		// above the drift a standing character produces, so a small sway from the
		// idle pose is not mistaken for a field.
		const bool moved = dPos > 0.05f;
		const bool rotated = dRot > 1.0f;
		const bool fovChanged = dFov > 0.25f;

		if (moved || rotated || fovChanged)
		{
			g_sweepHits++;
			Log::Line("sweep: +0x%03X  %.4g -> %.4g | %s%s%s pos %.3f  rot %.2f  fov %.2f",
				g_sweepOffset, originalF, AsFloat(*(unsigned int *)base),
				moved ? "MOVES " : "", rotated ? "ROTATES " : "", fovChanged ? "FOV " : "",
				dPos, dRot, dFov);
		}

		// Restore and move on.
		*(unsigned int *)base = g_sweepOriginal;
		g_sweepOffset += 4;
		g_sweepPhase = 0;
	}

	void RestoreAll()
	{
		RestoreSweepPending();
		RestoreAllInternal();
	}

	void Tick(const Settings &s, int ped, int veh, bool inVeh, bool firstPerson)
	{
		if (!g_available)
			return;

		if (!s.experimentalMeta)
		{
			if (!g_originals.empty())
				RestoreAllInternal();
			return;
		}

		ApplyExtraCameras(s);

		if (inVeh)
		{
			const unsigned int hash = ResolveActiveHash(veh, true, firstPerson);
			if (hash == 0)
				return;

			if (firstPerson)
			{
				WritePlan p = PlainPlan(hash, LAYOUT.vehFpFov, s.vehFpFov, "vehicle, first person");
				p.offPivot = LAYOUT.vehFpOffsetZ; // reused as the view Z offset here
				p.pivot = s.vehFpOffsetZ;
				ApplyPlan(p);
			}
			else
			{
				WritePlan p = PlainPlan(hash, LAYOUT.vehTpFov, s.vehTpFov, "vehicle, third person");
				p.offDistance = LAYOUT.vehTpDistance;
				p.distance = s.vehTpDistance;
				p.offPivot = LAYOUT.vehTpPivotScale;
				p.pivot = s.vehTpPivotScale;
				ApplyPlan(p);
			}
			return;
		}

		if (ped == 0)
			return;

		if (firstPerson)
		{
			ApplyPlan(PlainPlan(HASH_PED_FIRST, LAYOUT.pedFpFov, s.pedFpFov, "on foot, first person", true));
			ApplyFirstPersonExtras(s);
			return;
		}

		const bool aimingNow = (CAM::IS_AIM_CAM_ACTIVE() != 0);

		WritePlan p = PlainPlan(HASH_PED_THIRD, LAYOUT.pedTpFov, s.pedTpFov, "on foot, third person", !aimingNow);
		p.offDistance = LAYOUT.pedTpDistance;
		p.distance = s.pedTpDistance;
		p.offPivot = LAYOUT.pedTpPivotX;
		p.pivot = s.pedTpPivotX;
		ApplyPlan(p);

		// Mirror to the remaining camFollowPedCameraMetadata entries (sprint,
		// stealth, cover, swim, etc.) so on-foot FOV/distance/pivot persist
		// across all on-foot movement states.
		for (int i = 0; i < (int)(sizeof(kFollowPedExtraCams) / sizeof(kFollowPedExtraCams[0])); i++)
		{
			WritePlan ep = p;
			ep.hash = kFollowPedExtraCams[i];
			ep.recordLastWrite = false;
			if (FindMetadata(ep.hash) != nullptr)
				ApplyPlan(ep);
		}

		// Aiming switches the game to a different camera with its own metadata, so
		// the FOV would snap back to vanilla down the sights. Optional - when off,
		// restore any previously written aiming fields so vanilla aiming resumes.
		if (s.metaAiming)
		{
			const bool weaponsOwnsDefault = (s.weaponFovDefault > 0.0f);
			WritePlan aim = PlainPlan(HASH_PED_AIM, LAYOUT.pedAimFov,
				(s.pedAimFov > 0.0f) ? s.pedAimFov : s.pedTpFov,
				"on foot, aiming", aimingNow);

			if (weaponsOwnsDefault)
				aim.offFov = -1;

			aim.offDistance = LAYOUT.pedAimDistance;
			aim.distance = s.pedAimDistance > 0.0f ? s.pedAimDistance : s.pedTpDistance;
			aim.offPivot = LAYOUT.pedAimPivotX;
			aim.pivot = s.pedAimPivotX != 0.0f ? s.pedAimPivotX : s.pedTpPivotX;
			aim.offFlag = ResolveWeaponFovFlagOffset();
			aim.flag = false;
			ApplyPlan(aim);

			// The game switches to a different camera per weapon class while
			// aiming, so each of those carries its own FOV to set.
			ApplyWeaponFovs(s);
		}
		else
		{
			RestoreAiming();
		}
	}

	// Read once per tick by the caller and shown in game, then cleared.
	const char *TakeMessage()
	{
		if (!g_msgPending)
			return nullptr;

		g_msgPending = false;
		return g_msgBuf;
	}
}
