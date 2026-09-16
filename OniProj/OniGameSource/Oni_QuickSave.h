// ======================================================================
// Oni_QuickSave.h
//
// On-disk format for mid-level quick save / quick load (#112). Owns
// "quick_save.dat", living wherever ONiBundlePath_ResolveStateFile puts
// it (cwd for the bare-binary workflow, App Support for the .app).
//
// Records are fixed-size and pointer-free by construction, because every
// one of them has to survive ONrLevel_Unload: identity is always a name,
// an ordinal or an ID, never an address. That is also why this module
// does not reuse ONtContinue — see Oni_QuickSave.c for the bridging.
//
// Kept free of engine deps beyond BFW base types so the TU compiles
// standalone for ../../tests/test_oni_quick_save.c.
// ======================================================================
#ifndef ONI_QUICK_SAVE_H
#define ONI_QUICK_SAVE_H

#include "BFW.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ONcQuickSave_PlayerNameLength			(64)
#define ONcQuickSave_WeaponNameLength			(128)
#define ONcQuickSave_CharacterNameLength		(32)
#define ONcQuickSave_CharacterClassNameLength	(128)
#define ONcQuickSave_CorpseNameLength			(32)
#define ONcQuickSave_CorpsePartCount			(19)
#define ONcQuickSave_WeaponSlotCount			(3)

#define ONcQuickSave_MaxCharacters				(128)
#define ONcQuickSave_MaxDoors					(256)
#define ONcQuickSave_MaxGeometryQuads			(1024)
#define ONcQuickSave_MaxCorpses					(128)

typedef struct ONtQuickSave_Point
{
	float	x;
	float	y;
	float	z;
} ONtQuickSave_Point;

typedef struct ONtQuickSave_Matrix
{
	float	m[4][3];
} ONtQuickSave_Matrix;

typedef struct ONtQuickSave_Weapon
{
	char		name[ONcQuickSave_WeaponNameLength];
	UUtUns32	ammo;
} ONtQuickSave_Weapon;

typedef struct ONtQuickSave_Player
{
	char				name[ONcQuickSave_PlayerNameLength];
	UUtUns32			continueFlags;
	UUtUns32			hitPoints;
	UUtUns32			maxHitPoints;
	ONtQuickSave_Point	position;
	float				facing;
	UUtUns32			ammo;
	UUtUns32			cell;
	UUtUns32			shieldRemaining;
	UUtUns32			invisibilityRemaining;
	UUtUns32			hypo;
	UUtUns32			keys;
	UUtUns32			hasLsi;
	ONtQuickSave_Weapon	weaponSave[ONcQuickSave_WeaponSlotCount];
} ONtQuickSave_Player;

// Ordinal disambiguates same-named characters, which shipped levels spawn
// repeatedly on purpose ("respawn1" up to three times, "WH_Striker_C" four).
// It is the creation-order rank among characters sharing this name.
typedef struct ONtQuickSave_Character
{
	char				name[ONcQuickSave_CharacterNameLength];
	UUtUns32			ordinal;
	ONtQuickSave_Point	position;
	float				facing;
	UUtUns32			hitPoints;
	UUtBool				alive;
} ONtQuickSave_Character;

typedef struct ONtQuickSave_Door
{
	UUtUns32	id;
	UUtUns32	flags;
	UUtUns32	state;
} ONtQuickSave_Door;

typedef struct ONtQuickSave_GeometryQuad
{
	UUtUns32	index;
	UUtUns32	flags;
} ONtQuickSave_GeometryQuad;

// The character class travels as a name, never as the ONtCharacterClass*
// that ONtCorpse_Data actually holds, because that pointer dies with
// template memory on unload. Same approach the level importer takes.
typedef struct ONtQuickSave_Corpse
{
	char				name[ONcQuickSave_CorpseNameLength];
	char				characterClassName[ONcQuickSave_CharacterClassNameLength];
	ONtQuickSave_Matrix	matricies[ONcQuickSave_CorpsePartCount];
	ONtQuickSave_Point	boundingBoxMin;
	ONtQuickSave_Point	boundingBoxMax;
} ONtQuickSave_Corpse;

typedef struct ONtQuickSave
{
	UUtInt32					levelNumber;
	UUtInt32					savePoint;
	ONtQuickSave_Player			player;
	UUtUns32					characterCount;
	UUtUns32					doorCount;
	UUtUns32					geometryQuadCount;
	UUtUns32					corpseCount;
	UUtUns32					droppedCount;
	ONtQuickSave_Character		characters[ONcQuickSave_MaxCharacters];
	ONtQuickSave_Door			doors[ONcQuickSave_MaxDoors];
	ONtQuickSave_GeometryQuad	geometryQuads[ONcQuickSave_MaxGeometryQuads];
	ONtQuickSave_Corpse			corpses[ONcQuickSave_MaxCorpses];
} ONtQuickSave;

// Zeroes the whole record. Callers must do this before filling in a save,
// so that struct padding stays deterministic on disk.
void ONrQuickSave_Clear(ONtQuickSave *outSave);

// Path-explicit forms (unit-testable without the resolver). Write returns
// UUcFalse on any failure. Read returns UUcFalse on a missing, truncated,
// short, wrong-version or wrong-swap-code file, and on counts that would
// overrun the fixed arrays; *outSave is only meaningful when it returns
// UUcTrue.
UUtBool ONrQuickSave_WriteToPath(const char *inPath, const ONtQuickSave *inSave);
UUtBool ONrQuickSave_ReadFromPath(const char *inPath, ONtQuickSave *outSave);

// Resolver-backed forms used by the engine.
UUtBool ONrQuickSave_Write(const ONtQuickSave *inSave);
UUtBool ONrQuickSave_Read(ONtQuickSave *outSave);

#ifdef __cplusplus
}
#endif

// ======================================================================
#endif /* ONI_QUICK_SAVE_H */
