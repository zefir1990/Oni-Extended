// ======================================================================
// test_oni_quick_save.c
//
// Standalone unit tests for the quick_save.dat format in
// Oni_QuickSave.c (#112). No framework needed — compile and run directly:
//
//   cc -Wall -Wextra -DUUmSDL=1 \
//      -IBungieFrameWork/BFW_Headers -IOniProj/OniGameSource \
//      -I/opt/homebrew/include \
//      tests/test_oni_quick_save.c OniProj/OniGameSource/Oni_QuickSave.c \
//      -o /tmp/test_oni_quick_save
//   /tmp/test_oni_quick_save
//
// (The BFW/SDL include flags are what BFW.h needs; -I/opt/homebrew/include
// is where SDL2 lives on this machine — `sdl2-config --cflags` names it.)
//
// Covers the missing/garbage/empty/truncated cases that must never be
// parsed into a save, the version and swap-code guards, the count guard
// that stops a crafted file overrunning the fixed arrays, the full-field
// round trip at both empty and maximum population, and the resolver-backed
// wrappers via a stubbed ONiBundlePath_ResolveStateFile.
// ======================================================================
#include "Oni_QuickSave.h"
#include "ONi_BundlePath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg)								\
	do {												\
		if (cond) {										\
			g_pass++;									\
		} else {										\
			g_fail++;									\
			printf("FAIL %s (%s:%d)\n", msg, __FILE__, __LINE__);	\
		}												\
	} while (0)

// Stub of the bundle-path resolver: resolve into the test scratch dir so the
// wrapper functions are covered without touching the real App Support dir.
static char g_scratch[1024];

UUtError
ONiBundlePath_ResolveStateFile(
	const char				*filename,
	char					*outPath,
	size_t					outPathSize)
{
	snprintf(outPath, outPathSize, "%s/%s", g_scratch, filename);
	return UUcError_None;
}

// The header is private to the module, but its prefix is the public
// contract: version, swap code, level number, save point, then the player
// record (whose type is public), then the counts. Expressing the count
// offset in public types keeps the crafted-count test honest without
// duplicating the layout.
#define HEADER_VERSION_OFFSET			(0)
#define HEADER_SWAP_CODE_OFFSET			(sizeof(UUtUns32))
#define HEADER_CHARACTER_COUNT_OFFSET	\
	(2 * sizeof(UUtUns32) + 2 * sizeof(UUtInt32) + sizeof(ONtQuickSave_Player))

static void PatchUns32(const char *inPath, long inOffset, UUtUns32 inValue)
{
	FILE *file = fopen(inPath, "r+b");

	if (NULL == file) {
		printf("could not patch %s\n", inPath);
		return;
	}

	if (0 != fseek(file, inOffset, SEEK_SET)) {
		printf("could not seek in %s\n", inPath);
	}

	if (1 != fwrite(&inValue, sizeof(inValue), 1, file)) {
		printf("could not write to %s\n", inPath);
	}

	fclose(file);
}

static UUtUns32 ReadUns32(const char *inPath, long inOffset)
{
	UUtUns32 value = 0;
	FILE *file = fopen(inPath, "rb");

	if (NULL == file) {
		printf("could not read %s\n", inPath);
		return 0;
	}

	if (0 != fseek(file, inOffset, SEEK_SET)) {
		printf("could not seek in %s\n", inPath);
	}

	if (1 != fread(&value, sizeof(value), 1, file)) {
		printf("could not read from %s\n", inPath);
	}

	fclose(file);
	return value;
}

static void FillCharacter(ONtQuickSave_Character *outCharacter, UUtUns32 inIndex)
{
	snprintf(outCharacter->name, sizeof(outCharacter->name), "enemy_%u", inIndex);
	outCharacter->ordinal = inIndex % 3;
	outCharacter->position.x = (float) inIndex + 0.5f;
	outCharacter->position.y = (float) inIndex * 2.0f;
	outCharacter->position.z = (float) inIndex * 3.0f;
	outCharacter->facing = (float) inIndex * 0.25f;
	outCharacter->hitPoints = 100u + inIndex;
	outCharacter->alive = (0 == (inIndex % 2)) ? UUcTrue : UUcFalse;
}

static void FillCorpse(ONtQuickSave_Corpse *outCorpse, UUtUns32 inIndex)
{
	UUtUns32 part;

	snprintf(outCorpse->name, sizeof(outCorpse->name), "dynamic-corpse");
	snprintf(outCorpse->characterClassName, sizeof(outCorpse->characterClassName), "class_%u", inIndex);

	for (part = 0; part < ONcQuickSave_CorpsePartCount; part++) {
		outCorpse->matricies[part].m[0][0] = (float) inIndex + (float) part;
		outCorpse->matricies[part].m[3][2] = (float) part - (float) inIndex;
	}

	outCorpse->boundingBoxMin.x = -1.0f;
	outCorpse->boundingBoxMin.y = -2.0f;
	outCorpse->boundingBoxMin.z = -3.0f;
	outCorpse->boundingBoxMax.x = 1.0f;
	outCorpse->boundingBoxMax.y = 2.0f;
	outCorpse->boundingBoxMax.z = 3.0f;
}

static void FillSave(ONtQuickSave *outSave, UUtUns32 inCharacterCount, UUtUns32 inDoorCount, UUtUns32 inQuadCount, UUtUns32 inCorpseCount)
{
	UUtUns32 itr;

	ONrQuickSave_Clear(outSave);
	outSave->levelNumber = 7;
	outSave->savePoint = 2;
	outSave->droppedCount = 0;

	strcpy(outSave->player.name, "konoko");
	outSave->player.continueFlags = 1;
	outSave->player.hitPoints = 175;
	outSave->player.maxHitPoints = 200;
	outSave->player.position.x = 10.5f;
	outSave->player.position.y = -20.25f;
	outSave->player.position.z = 30.125f;
	outSave->player.facing = 1.75f;
	outSave->player.ammo = 12;
	outSave->player.cell = 3;
	outSave->player.shieldRemaining = 4;
	outSave->player.invisibilityRemaining = 5;
	outSave->player.hypo = 6;
	outSave->player.keys = 7;
	outSave->player.hasLsi = 1;
	strcpy(outSave->player.weaponSave[0].name, "w1_tap");
	outSave->player.weaponSave[0].ammo = 40;
	strcpy(outSave->player.weaponSave[2].name, "w7_scc");
	outSave->player.weaponSave[2].ammo = 9;

	outSave->characterCount = inCharacterCount;
	for (itr = 0; itr < inCharacterCount; itr++) {
		FillCharacter(outSave->characters + itr, itr);
	}

	outSave->doorCount = inDoorCount;
	for (itr = 0; itr < inDoorCount; itr++) {
		outSave->doors[itr].id = 1000u + itr;
		outSave->doors[itr].flags = itr % 4;
		outSave->doors[itr].state = itr % 3;
	}

	outSave->geometryQuadCount = inQuadCount;
	for (itr = 0; itr < inQuadCount; itr++) {
		outSave->geometryQuads[itr].index = itr * 5u;
		outSave->geometryQuads[itr].flags = 0xDEAD0000u + itr;
	}

	outSave->corpseCount = inCorpseCount;
	for (itr = 0; itr < inCorpseCount; itr++) {
		FillCorpse(outSave->corpses + itr, itr);
	}
}

int main(void)
{
	char					path[1100];
	char					cmd[1200];
	FILE					*file;
	ONtQuickSave			save;
	ONtQuickSave			loaded;
	UUtUns32				itr;

	snprintf(g_scratch, sizeof(g_scratch), "/tmp/oni-qs-test-%d", (int)getpid());
	snprintf(cmd, sizeof(cmd), "mkdir -p %s", g_scratch);
	if (system(cmd) != 0) { printf("could not create scratch dir\n"); return 1; }

	ONrQuickSave_Clear(&save);
	ONrQuickSave_Clear(&loaded);

	snprintf(path, sizeof(path), "%s/quick_save.dat", g_scratch);

	// --- ReadFromPath: missing file / NULL ---
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read missing file");
	CHECK(ONrQuickSave_ReadFromPath(NULL, &loaded) == UUcFalse, "read NULL path");
	CHECK(ONrQuickSave_ReadFromPath(path, NULL) == UUcFalse, "read NULL out");
	CHECK(ONrQuickSave_WriteToPath(NULL, &save) == UUcFalse, "write NULL path");
	CHECK(ONrQuickSave_WriteToPath(path, NULL) == UUcFalse, "write NULL save");

	// --- Round trip, empty population (counts of zero must be legal) ---
	FillSave(&save, 0, 0, 0, 0);
	CHECK(ONrQuickSave_WriteToPath(path, &save) == UUcTrue, "write empty population");
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcTrue, "read empty population");
	CHECK(loaded.levelNumber == 7, "empty level number");
	CHECK(loaded.savePoint == 2, "empty save point");
	CHECK(loaded.characterCount == 0, "empty character count");
	CHECK(loaded.doorCount == 0, "empty door count");
	CHECK(loaded.geometryQuadCount == 0, "empty quad count");
	CHECK(loaded.corpseCount == 0, "empty corpse count");
	CHECK(0 == strcmp(loaded.player.name, "konoko"), "empty player name");
	CHECK(loaded.player.hitPoints == 175, "empty player hit points");

	// --- Round trip, full population at every array maximum ---
	FillSave(&save,
		ONcQuickSave_MaxCharacters,
		ONcQuickSave_MaxDoors,
		ONcQuickSave_MaxGeometryQuads,
		ONcQuickSave_MaxCorpses);
	CHECK(ONrQuickSave_WriteToPath(path, &save) == UUcTrue, "write full population");
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcTrue, "read full population");
	CHECK(loaded.characterCount == ONcQuickSave_MaxCharacters, "full character count");
	CHECK(loaded.doorCount == ONcQuickSave_MaxDoors, "full door count");
	CHECK(loaded.geometryQuadCount == ONcQuickSave_MaxGeometryQuads, "full quad count");
	CHECK(loaded.corpseCount == ONcQuickSave_MaxCorpses, "full corpse count");

	CHECK(0 == strcmp(loaded.player.weaponSave[0].name, "w1_tap"), "full weapon 0 name");
	CHECK(loaded.player.weaponSave[0].ammo == 40, "full weapon 0 ammo");
	CHECK(loaded.player.weaponSave[2].ammo == 9, "full weapon 2 ammo");
	CHECK(0 == strcmp(loaded.player.weaponSave[1].name, ""), "full weapon 1 empty");

	for (itr = 0; itr < ONcQuickSave_MaxCharacters; itr++) {
		char expected[64];
		snprintf(expected, sizeof(expected), "enemy_%u", itr);
		if (0 != strcmp(loaded.characters[itr].name, expected)
			|| (loaded.characters[itr].ordinal != (itr % 3))
			|| (loaded.characters[itr].hitPoints != (100u + itr))
			|| (loaded.characters[itr].position.x != ((float) itr + 0.5f))
			|| (loaded.characters[itr].facing != ((float) itr * 0.25f))
			|| (loaded.characters[itr].alive != ((0 == (itr % 2)) ? UUcTrue : UUcFalse))) {
			break;
		}
	}
	CHECK(itr == ONcQuickSave_MaxCharacters, "every character round trips");

	for (itr = 0; itr < ONcQuickSave_MaxDoors; itr++) {
		if ((loaded.doors[itr].id != (1000u + itr))
			|| (loaded.doors[itr].flags != (itr % 4))
			|| (loaded.doors[itr].state != (itr % 3))) {
			break;
		}
	}
	CHECK(itr == ONcQuickSave_MaxDoors, "every door round trips");

	for (itr = 0; itr < ONcQuickSave_MaxGeometryQuads; itr++) {
		if ((loaded.geometryQuads[itr].index != (itr * 5u))
			|| (loaded.geometryQuads[itr].flags != (0xDEAD0000u + itr))) {
			break;
		}
	}
	CHECK(itr == ONcQuickSave_MaxGeometryQuads, "every quad round trips");

	CHECK(0 == strcmp(loaded.corpses[3].characterClassName, "class_3"), "corpse class name");
	CHECK(0 == strcmp(loaded.corpses[3].name, "dynamic-corpse"), "corpse name");
	CHECK(loaded.corpses[5].matricies[2].m[0][0] == 7.0f, "corpse matrix element");
	CHECK(loaded.corpses[5].matricies[2].m[3][2] == -3.0f, "corpse matrix tail");
	CHECK(loaded.corpses[9].boundingBoxMin.z == -3.0f, "corpse bbox min");
	CHECK(loaded.corpses[9].boundingBoxMax.z == 3.0f, "corpse bbox max");

	// --- File is proportional to contents, not to array maxima ---
	{
		long full_size;
		long empty_size;

		file = fopen(path, "rb");
		fseek(file, 0, SEEK_END);
		full_size = ftell(file);
		fclose(file);

		FillSave(&save, 0, 0, 0, 0);
		CHECK(ONrQuickSave_WriteToPath(path, &save) == UUcTrue, "rewrite empty");
		file = fopen(path, "rb");
		fseek(file, 0, SEEK_END);
		empty_size = ftell(file);
		fclose(file);

		CHECK(empty_size < full_size, "empty file is smaller than full");
	}

	// --- Version mismatch is rejected, not misparsed ---
	FillSave(&save, 4, 2, 3, 1);
	CHECK(ONrQuickSave_WriteToPath(path, &save) == UUcTrue, "write for version test");
	PatchUns32(path, HEADER_VERSION_OFFSET, 99);
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "version mismatch rejected");
	PatchUns32(path, HEADER_VERSION_OFFSET, 1);
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcTrue, "restored version accepted");

	// --- Swap code mismatch is rejected. The expected value is read back
	// from the file the writer produced rather than duplicated here, so the
	// test cannot drift from the constant the module actually uses.
	{
		UUtUns32 swap_code = ReadUns32(path, HEADER_SWAP_CODE_OFFSET);

		CHECK(swap_code != 0, "written swap code is not zero");
		PatchUns32(path, HEADER_SWAP_CODE_OFFSET, swap_code ^ 0xFFFFFFFFu);
		CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "swap code mismatch rejected");
		PatchUns32(path, HEADER_SWAP_CODE_OFFSET, swap_code);
		CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcTrue, "restored swap code accepted");
	}

	// --- A crafted count must not overrun the fixed arrays ---
	PatchUns32(path, HEADER_CHARACTER_COUNT_OFFSET, ONcQuickSave_MaxCharacters + 1);
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "oversized character count rejected");
	PatchUns32(path, HEADER_CHARACTER_COUNT_OFFSET, 0xFFFFFFFFu);
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "huge character count rejected");
	PatchUns32(path, HEADER_CHARACTER_COUNT_OFFSET, 4);
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcTrue, "restored character count accepted");

	// --- Empty file ---
	file = fopen(path, "wb");
	fclose(file);
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read empty file");

	// --- Truncated header ---
	file = fopen(path, "wb");
	fputs("QS", file);
	fclose(file);
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read truncated header");

	// --- Truncated record run: header is intact, the characters are not ---
	FillSave(&save, ONcQuickSave_MaxCharacters, 0, 0, 0);
	CHECK(ONrQuickSave_WriteToPath(path, &save) == UUcTrue, "write for truncation test");
	{
		long file_size;
		long keep;
		char *buffer;

		file = fopen(path, "rb");
		fseek(file, 0, SEEK_END);
		file_size = ftell(file);
		fclose(file);

		keep = file_size - 4;
		buffer = (char *) malloc((size_t) keep);
		file = fopen(path, "rb");
		if (fread(buffer, 1, (size_t) keep, file) != (size_t) keep) { printf("short read\n"); }
		fclose(file);
		file = fopen(path, "wb");
		if (fwrite(buffer, 1, (size_t) keep, file) != (size_t) keep) { printf("short write\n"); }
		fclose(file);
		free(buffer);

		CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "truncated record run rejected");
	}

	// --- Garbage file content ---
	file = fopen(path, "wb");
	for (itr = 0; itr < 4096; itr++) { fputc('m', file); }
	fclose(file);
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read garbage");

	// --- ExistsAtPath: the header predicate, paired against ReadFromPath ---
	remove(path);
	CHECK(ONrQuickSave_ExistsAtPath(NULL) == UUcFalse, "exists NULL path");
	CHECK(ONrQuickSave_ExistsAtPath(path) == UUcFalse, "exists missing file");
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read missing file, paired");

	file = fopen(path, "wb");
	fclose(file);
	CHECK(ONrQuickSave_ExistsAtPath(path) == UUcFalse, "exists empty file");
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read empty file, paired");

	file = fopen(path, "wb");
	fputs("QS", file);
	fclose(file);
	CHECK(ONrQuickSave_ExistsAtPath(path) == UUcFalse, "exists truncated header");
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read truncated header, paired");

	file = fopen(path, "wb");
	for (itr = 0; itr < 4096; itr++) { fputc('m', file); }
	fclose(file);
	CHECK(ONrQuickSave_ExistsAtPath(path) == UUcFalse, "exists garbage");
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read garbage, paired");

	FillSave(&save, 4, 2, 3, 1);
	CHECK(ONrQuickSave_WriteToPath(path, &save) == UUcTrue, "write for exists test");
	CHECK(ONrQuickSave_ExistsAtPath(path) == UUcTrue, "exists valid file");
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcTrue, "read valid file, paired");

	PatchUns32(path, HEADER_VERSION_OFFSET, 99);
	CHECK(ONrQuickSave_ExistsAtPath(path) == UUcFalse, "exists rejects version mismatch");
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read rejects version mismatch, paired");
	PatchUns32(path, HEADER_VERSION_OFFSET, 1);

	{
		UUtUns32 swap_code = ReadUns32(path, HEADER_SWAP_CODE_OFFSET);

		PatchUns32(path, HEADER_SWAP_CODE_OFFSET, swap_code ^ 0xFFFFFFFFu);
		CHECK(ONrQuickSave_ExistsAtPath(path) == UUcFalse, "exists rejects swap code mismatch");
		CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read rejects swap code mismatch, paired");
		PatchUns32(path, HEADER_SWAP_CODE_OFFSET, swap_code);
	}

	PatchUns32(path, HEADER_CHARACTER_COUNT_OFFSET, ONcQuickSave_MaxCharacters + 1);
	CHECK(ONrQuickSave_ExistsAtPath(path) == UUcFalse, "exists rejects oversized count");
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read rejects oversized count, paired");
	PatchUns32(path, HEADER_CHARACTER_COUNT_OFFSET, 4);
	CHECK(ONrQuickSave_ExistsAtPath(path) == UUcTrue, "exists accepts restored file");
	CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcTrue, "read accepts restored file, paired");

	// A file with an intact header and a short record run is the one place
	// the two predicates must disagree: ExistsAtPath is header-only by
	// design, because its caller only needs to know whether to offer a
	// load, and paying for the record run to answer that is the cost this
	// predicate exists to avoid. Pinned so it cannot silently become strict.
	{
		long file_size;
		long keep;
		char *buffer;

		file = fopen(path, "rb");
		fseek(file, 0, SEEK_END);
		file_size = ftell(file);
		fclose(file);

		keep = file_size - 4;
		buffer = (char *) malloc((size_t) keep);
		file = fopen(path, "rb");
		if (fread(buffer, 1, (size_t) keep, file) != (size_t) keep) { printf("short read\n"); }
		fclose(file);
		file = fopen(path, "wb");
		if (fwrite(buffer, 1, (size_t) keep, file) != (size_t) keep) { printf("short write\n"); }
		fclose(file);
		free(buffer);

		CHECK(ONrQuickSave_ExistsAtPath(path) == UUcTrue, "exists accepts short record run");
		CHECK(ONrQuickSave_ReadFromPath(path, &loaded) == UUcFalse, "read rejects short record run");
	}

	// --- Resolver-backed wrappers (via the stub above) ---
	remove(path);
	CHECK(ONrQuickSave_Read(&loaded) == UUcFalse, "wrapper read missing");
	CHECK(ONrQuickSave_Exists() == UUcFalse, "wrapper exists missing");
	FillSave(&save, 2, 1, 1, 1);
	CHECK(ONrQuickSave_Write(&save) == UUcTrue, "wrapper write");
	CHECK(ONrQuickSave_Exists() == UUcTrue, "wrapper exists");
	CHECK(ONrQuickSave_Read(&loaded) == UUcTrue, "wrapper read");
	CHECK(loaded.levelNumber == 7, "wrapper level number");
	CHECK(loaded.characterCount == 2, "wrapper character count");
	CHECK(loaded.doorCount == 1, "wrapper door count");
	CHECK(0 == strcmp(loaded.characters[1].name, "enemy_1"), "wrapper character name");

	// --- Unwritable path fails soft ---
	CHECK(ONrQuickSave_WriteToPath("/nonexistent-dir-xyz/quick_save.dat", &save) == UUcFalse, "write to bad path");

	// --- Clear zeroes the record ---
	ONrQuickSave_Clear(&loaded);
	CHECK(loaded.characterCount == 0, "clear character count");
	CHECK(loaded.levelNumber == 0, "clear level number");
	CHECK(0 == strcmp(loaded.player.name, ""), "clear player name");
	ONrQuickSave_Clear(NULL);

	snprintf(cmd, sizeof(cmd), "rm -rf %s", g_scratch);
	if (system(cmd) != 0) { printf("could not remove scratch dir\n"); }

	printf("test_oni_quick_save: %d passed, %d failed\n", g_pass, g_fail);
	return (g_fail == 0) ? 0 : 1;
}
