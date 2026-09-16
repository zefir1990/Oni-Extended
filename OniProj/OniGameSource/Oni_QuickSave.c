// ======================================================================
// Oni_QuickSave.c
//
// Reader/writer for quick_save.dat (#112). Oni_QuickSave.h states the
// format's contract; this file is the only place that knows its layout.
//
// The file is user-writable state, so every failure path returns UUcFalse
// rather than asserting: missing, empty, truncated, wrong version, wrong
// swap code, or counts that would overrun the fixed arrays.
//
// Layout: a fixed header (identity, player, record counts) followed by
// four variable-length record runs in a fixed order. Only the live
// records are written, so the file stays proportional to its contents
// rather than to the array maxima.
// ======================================================================
#include "Oni_QuickSave.h"
#include "ONi_BundlePath.h"

#include <stdio.h>

#define ONcQuickSave_Version	(1)
#define ONcQuickSave_SwapCode	(UUm4CharToUns32('Q', 'S', 'A', 'V'))
#define ONcQuickSave_FileName	"quick_save.dat"
#define ONcQuickSave_PathLength	(1024)

typedef struct ONtQuickSave_Header
{
	UUtUns32			version;
	UUtUns32			swapCode;
	UUtInt32			levelNumber;
	UUtInt32			savePoint;
	ONtQuickSave_Player	player;
	UUtUns32			characterCount;
	UUtUns32			doorCount;
	UUtUns32			geometryQuadCount;
	UUtUns32			corpseCount;
	UUtUns32			droppedCount;
} ONtQuickSave_Header;

static UUtBool ONiQuickSave_WriteRecords(FILE *inFile, const void *inRecords, size_t inRecordSize, UUtUns32 inCount)
{
	if (0 == inCount) {
		return UUcTrue;
	}

	return (inCount == fwrite(inRecords, inRecordSize, inCount, inFile));
}

static UUtBool ONiQuickSave_ReadRecords(FILE *inFile, void *outRecords, size_t inRecordSize, UUtUns32 inCount)
{
	if (0 == inCount) {
		return UUcTrue;
	}

	return (inCount == fread(outRecords, inRecordSize, inCount, inFile));
}

// A count read off disk indexes a fixed array, so it is validated before
// any read happens. Without this a crafted file overruns the record.
static UUtBool ONiQuickSave_CountsFit(const ONtQuickSave_Header *inHeader)
{
	return (inHeader->characterCount <= ONcQuickSave_MaxCharacters)
		&& (inHeader->doorCount <= ONcQuickSave_MaxDoors)
		&& (inHeader->geometryQuadCount <= ONcQuickSave_MaxGeometryQuads)
		&& (inHeader->corpseCount <= ONcQuickSave_MaxCorpses);
}

void ONrQuickSave_Clear(ONtQuickSave *outSave)
{
	if (NULL == outSave) {
		return;
	}

	UUrMemory_Clear(outSave, sizeof(*outSave));
}

UUtBool ONrQuickSave_WriteToPath(const char *inPath, const ONtQuickSave *inSave)
{
	ONtQuickSave_Header header;
	FILE *file;
	UUtBool succeeded = UUcFalse;

	if ((NULL == inPath) || (NULL == inSave)) {
		return UUcFalse;
	}

	UUrMemory_Clear(&header, sizeof(header));
	header.version = ONcQuickSave_Version;
	header.swapCode = ONcQuickSave_SwapCode;
	header.levelNumber = inSave->levelNumber;
	header.savePoint = inSave->savePoint;
	header.player = inSave->player;
	header.characterCount = inSave->characterCount;
	header.doorCount = inSave->doorCount;
	header.geometryQuadCount = inSave->geometryQuadCount;
	header.corpseCount = inSave->corpseCount;
	header.droppedCount = inSave->droppedCount;

	file = fopen(inPath, "wb");
	if (NULL == file) {
		return UUcFalse;
	}

	if (1 == fwrite(&header, sizeof(header), 1, file)
		&& ONiQuickSave_WriteRecords(file, inSave->characters, sizeof(ONtQuickSave_Character), header.characterCount)
		&& ONiQuickSave_WriteRecords(file, inSave->doors, sizeof(ONtQuickSave_Door), header.doorCount)
		&& ONiQuickSave_WriteRecords(file, inSave->geometryQuads, sizeof(ONtQuickSave_GeometryQuad), header.geometryQuadCount)
		&& ONiQuickSave_WriteRecords(file, inSave->corpses, sizeof(ONtQuickSave_Corpse), header.corpseCount)) {
		succeeded = UUcTrue;
	}

	fclose(file);
	return succeeded;
}

UUtBool ONrQuickSave_ReadFromPath(const char *inPath, ONtQuickSave *outSave)
{
	ONtQuickSave_Header header;
	FILE *file;
	UUtBool succeeded = UUcFalse;

	if ((NULL == inPath) || (NULL == outSave)) {
		return UUcFalse;
	}

	file = fopen(inPath, "rb");
	if (NULL == file) {
		return UUcFalse;
	}

	UUrMemory_Clear(&header, sizeof(header));
	if (1 != fread(&header, sizeof(header), 1, file)) { goto exit; }
	if (ONcQuickSave_Version != header.version) { goto exit; }
	if (ONcQuickSave_SwapCode != header.swapCode) { goto exit; }
	if (!ONiQuickSave_CountsFit(&header)) { goto exit; }

	ONrQuickSave_Clear(outSave);
	outSave->levelNumber = header.levelNumber;
	outSave->savePoint = header.savePoint;
	outSave->player = header.player;
	outSave->characterCount = header.characterCount;
	outSave->doorCount = header.doorCount;
	outSave->geometryQuadCount = header.geometryQuadCount;
	outSave->corpseCount = header.corpseCount;
	outSave->droppedCount = header.droppedCount;

	if (!ONiQuickSave_ReadRecords(file, outSave->characters, sizeof(ONtQuickSave_Character), header.characterCount)) { goto exit; }
	if (!ONiQuickSave_ReadRecords(file, outSave->doors, sizeof(ONtQuickSave_Door), header.doorCount)) { goto exit; }
	if (!ONiQuickSave_ReadRecords(file, outSave->geometryQuads, sizeof(ONtQuickSave_GeometryQuad), header.geometryQuadCount)) { goto exit; }
	if (!ONiQuickSave_ReadRecords(file, outSave->corpses, sizeof(ONtQuickSave_Corpse), header.corpseCount)) { goto exit; }

	succeeded = UUcTrue;

exit:
	fclose(file);
	return succeeded;
}

UUtBool ONrQuickSave_Write(const ONtQuickSave *inSave)
{
	char path[ONcQuickSave_PathLength];

	if (UUcError_None != ONiBundlePath_ResolveStateFile(ONcQuickSave_FileName, path, sizeof(path))) {
		return UUcFalse;
	}

	return ONrQuickSave_WriteToPath(path, inSave);
}

UUtBool ONrQuickSave_Read(ONtQuickSave *outSave)
{
	char path[ONcQuickSave_PathLength];

	if (UUcError_None != ONiBundlePath_ResolveStateFile(ONcQuickSave_FileName, path, sizeof(path))) {
		return UUcFalse;
	}

	return ONrQuickSave_ReadFromPath(path, outSave);
}

// ======================================================================
