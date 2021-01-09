#ifndef GUARD_DYNAMIC_PALS_H
#define GUARD_DYNAMIC_PALS_H

#include "global.h"

#define PalTypeUnused 0
#define PalTypeNPC 1
#define PalTypeAnimation 2
#define PalTypeWeather 3
#define PalTypeReflection 4
#define PalTypeOther 5

#define Red(Color)		((Color) & 31)
#define Green(Color)	((Color >> 5) & 31)
#define Blue(Color)		((Color >> 10) & 31)

#define FOG_FADE_COLOUR TintColor(RGB(28, 31, 28))
#define CpuSetFill (1 << 24)

struct PalRef
{
	u8 Type;
	u8 Count;
	u16 PalTag;
};


u8 AddPalRef(u8 type, u16 palTag);
u8 FindPalRef(u8 type, u16 palTag);
u8 GetPalTypeByPaletteOffset(u16 offset);
u8 GetFadeTypeByWeather(u8 weather);
void PalRefDecreaseCount(u8 palSlot);
void ClearAllPalRefs(void);
u8 GetPalSlotMisc(u32 OBJData);
u8 FindOrLoadNPCPalette(u16 palTag);
u8 FindOrCreateReflectionPalette(u8 palSlotNPC);
void FogBrightenPalettes(u16 brightenIntensity);
void FogBrightenAndFade(u8 palSlot, u8 fadeIntensity, u16 fadeColor);
u8 GetDarkeningTypeBySlot(u8 palSlot);

//Hooked In Functions
u8 PaletteNeedsFogBrightening(u8 palSlot);
u8 FindPalette(u16 palTag);
void ClearAllPalettes(void);
u8 FindOrLoadPalette(struct SpritePalette* pal);

void LoadCloudOrSandstormPalette(u16* pal);
#endif // GUARD_DYNAMIC_PALS_H