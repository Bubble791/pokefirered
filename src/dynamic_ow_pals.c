#include "global.h"
#include "m4a.h"
#include "dns.h"
#include "rtc.h"
#include "field_weather.h"
#include "field_effect.h"
#include "fieldmap.h"
#include "palette.h"
#include "sprite.h"
#include "overworld.h"
#include "graphics.h"
#include "dynamic_pals.h"
#include "util.h"
#include "task.h"
#include "event_object_movement.h"
#include "gba/io_reg.h"
#include "constants/weather.h"
#include "constants/map_types.h"

EWRAM_DATA struct PalRef sPalRefs[16] ={0};

static u16 TintColor(u16 color);
static u8 GetPalTypeByPalTag(u16 palTag);
static u8 FindPalTag(u16 PalTag);
static u8 PalRefIncreaseCount(u8 palSlot);
static void BrightenReflection(u8 palSlot);
static u8 AddPalTag(u16 palTag);
static void MaskPaletteIfFadingIn(u8 palSlot);

u8 AddPalRef(u8 type, u16 palTag)
{
    int i;
	for (i = 0; i < 16; i++)
	{
		if (sPalRefs[i].Type == PalTypeUnused)
		{
			sPalRefs[i].Type = type;
			sPalRefs[i].PalTag = palTag;
			return i;
		}
	}

	return 0xFF; //No more space
}

u8 FindPalRef(u8 type, u16 palTag)
{
    int i;
	for (i = 0; i < 16; i++)
	{
		if (sPalRefs[i].Type == type && sPalRefs[i].PalTag == palTag)
			return i;
	}

	return 0xFF; // not found
}

u8 GetPalTypeByPaletteOffset(u16 offset)
{
    u8 palSlot;
	if (&gPlttBufferUnfaded[offset] < &gPlttBufferUnfaded[0x200])
		return 0;

	palSlot = (u32) (&gPlttBufferUnfaded[offset] - &gPlttBufferUnfaded[0x200]) / 16;

	return sPalRefs[palSlot].Type;
}

u8 GetFadeTypeByWeather(u8 weather)
{
	switch (weather)
	{
		case WEATHER_NONE:
		case 1:
		case WEATHER_SUNNY:
		case WEATHER_SNOW:
		case 7:
		case WEATHER_SANDSTORM:
		default:
			return 0; //Normal fade

		case 3:
		case 5:
		case WEATHER_SHADE:
		case 13:
			return 1; //Fade and darken

		case 6:
		case 9:
		case 10:
		case 14:
			return 2; //Fade and brighten

		case WEATHER_DROUGHT:
			return 3; //Harsh sunlight
	}
}


u8 GetFadeTypeForCurrentWeather(void)
{
	return GetFadeTypeByWeather(gWeatherPtr->currWeather);
}

static u16 TintColor(u16 color)
{
	switch (gUnknown_2036E28)
	{
		case 1:
		case 3:
			TintPalette_GrayScale(&color, 1);
			break;
		case 2:
			TintPalette_SepiaTone(&color, 1);
			break;
	}

	return color;
}


u8 PaletteNeedsFogBrightening(u8 palSlot) // hook at 0x7A748
{
	palSlot = palSlot - 16;

	return sPalRefs[palSlot].Type == PalTypeReflection
		#ifdef FADE_NPCS_IN_FOG
		|| sPalRefs[palSlot].Type == PalTypeNPC
		#endif
		|| sPalRefs[palSlot].Type == PalTypeAnimation;
}

static u8 GetPalTypeByPalTag(u16 palTag)
{
	if (palTag >= 0x1000 && palTag <= 0x1010)
		return PalTypeAnimation;

	if (palTag == 0x1200)
		return PalTypeWeather;

	return PalTypeOther;
}

static u8 FindPalTag(u16 palTag)
{
	int i;
	if (gReservedSpritePaletteCount >= 16)
		return 0xFF;

	for (i = gReservedSpritePaletteCount; i < 16; ++i)
	{
		if (sSpritePaletteTags[i] == palTag)
			return i;
	}

	return 0xFF; //Not found
}

u8 FindPalette(u16 palTag) //Hook at 0x80089E8 via r1
{
	if (FuncIsActiveTask(Task_WeatherMain) || palTag == 0x1200)
		return FindPalRef(GetPalTypeByPalTag(palTag), palTag); // 0x1200 is for weather sprites

	return FindPalTag(palTag);
}

static u8 PalRefIncreaseCount(u8 palSlot)
{
	sPalRefs[palSlot].Count++;
	return palSlot;
}

void PalRefDecreaseCount(u8 palSlot)
{
	if (sPalRefs[palSlot].Count != 0)
		sPalRefs[palSlot].Count--;
	if (sPalRefs[palSlot].Count == 0)
	{
		sPalRefs[palSlot].Type = 0;
		sPalRefs[palSlot].PalTag = 0;
	}
}

void ClearAllPalRefs(void)
{
	int fill = 0;
	CpuSet(&fill, sPalRefs, 32 | CpuSetFill);
}

void ClearAllPalettes(void) //Hook at 0x5F574 via r0
{
	int fill = 0;
	CpuSet(&fill, &gPlttBufferUnfaded[16 * 16], 256 | CpuSetFill);
}

static void BrightenReflection(u8 palSlot)
{
	u8 R, G, B;
	u16 color;
	int i;
	u16* pal = &gPlttBufferFaded[palSlot * 16 + 16 * 16];

	for (i = 0; i < 16; ++i)
	{
		color = pal[i];
		R = Red(color) + 5;
		G = Green(color) + 5;
		B = Blue(color) + 10;
		if (R > 31) R = 31;
		if (G > 31) G = 31;
		if (B > 31) B = 31;
		pal[i] = RGB(R, G, B);
	}

	CpuSet(pal, &gPlttBufferUnfaded[palSlot * 16 + 16 * 16], 16);
}

static u8 AddPalTag(u16 palTag)
{
	int i;
	if (gReservedSpritePaletteCount >= 16)
		return 0xFF;

	for (i = gReservedSpritePaletteCount; i < 16; ++i)
	{
		if (sSpritePaletteTags[i] == 0xFFFF)
		{
			sSpritePaletteTags[i] = palTag;
			return i;
		}
	}

	return 0xFF; // no more space
}

u8 FindOrLoadPalette(struct SpritePalette* pal) //Hook at 0x8928 via r1
{
	u8 palSlot;
	u16 palTag = pal->tag;

	if (FuncIsActiveTask(Task_WeatherMain)  || palTag == 0x1200) //0x1200 is for weather sprites
	{
		palSlot = FindPalRef(GetPalTypeByPalTag(palTag), palTag);
		if (palSlot != 0xFF)
			return palSlot;

		palSlot = AddPalRef(GetPalTypeByPalTag(palTag), palTag);
		if (palSlot == 0xFF)
			return 0xFF;
	}
	else
	{
		palSlot = FindPalTag(palTag);
		if (palSlot != 0xFF)
			return palSlot;

		palSlot = AddPalTag(palTag);
		if (palSlot == 0xFF)
			return 0xFF;
	}

	DoLoadSpritePalette(pal->data, palSlot * 16);
	return palSlot;
}

static void MaskPaletteIfFadingIn(u8 palSlot) //Prevent the palette from flashing briefly before fading starts
{
	u16 fadeColor;
	u8 fadeState = gWeatherPtr->palProcessingState;
	u8 aboutToFadeIn = gWeatherPtr->unknown_6CA;

	if (fadeState == 1 && aboutToFadeIn)
	{
		fadeColor = gWeatherPtr->fadeDestColor;
		CpuSet(&fadeColor, &gPlttBufferFaded[palSlot * 16 + 16 * 16], 16 | CpuSetFill);
	}
}

u8 GetPalSlotMisc(u32 OBJData)
{
	u8 palSlot;
	u16 palTag = *(u16*)(OBJData + 2);
	if (palTag == 0xFFFF)
		return 0xFF;

	palSlot = FindPalette(palTag);
	if (palSlot != 0xFF)
		return PalRefIncreaseCount(palSlot);

	if (palTag != 0x1200)
		return 0xFF;

	//Load the rain palette
	palSlot = AddPalRef(PalTypeWeather, palTag);
	if (palSlot == 0xFF)
		return 0xFF;

	DoLoadSpritePalette(gUnknown_83C2CE0, palSlot * 16);
	sub_8083598(palSlot);
	MaskPaletteIfFadingIn(palSlot);
	return PalRefIncreaseCount(palSlot);
}

u8 FindOrLoadNPCPalette(u16 palTag)
{
	u8 palSlot = FindPalRef(PalTypeNPC, palTag);
	if (palSlot != 0xFF)
		return PalRefIncreaseCount(palSlot);

	palSlot = AddPalRef(PalTypeNPC, palTag);
	if (palSlot == 0xFF)
		return PalRefIncreaseCount(0);

	PatchObjectPalette(palTag, palSlot);
	FogBrightenPalettes(12);
	MaskPaletteIfFadingIn(palSlot);
	return PalRefIncreaseCount(palSlot);
}

u8 FindOrCreateReflectionPalette(u8 palSlotNPC)
{
	u16 palTag = sPalRefs[palSlotNPC].PalTag;
	u8 palSlot = FindPalRef(PalTypeReflection, palTag);
	if (palSlot != 0xFF)
		return PalRefIncreaseCount(palSlot);

	palSlot = AddPalRef(PalTypeReflection, palTag);
	if (palSlot == 0xFF)
		return PalRefIncreaseCount(0);

	PatchObjectPalette(palTag, palSlot);
	BlendPalettes(gBitTable[(palSlot + 16)], 6, RGB(12, 20, 27)); //Make it blueish
	BrightenReflection(palSlot); //And a little brighter
	sub_8083598(palSlot);
	MaskPaletteIfFadingIn(palSlot);
	return PalRefIncreaseCount(palSlot);
}


void FogBrightenPalettes(u16 brightenIntensity)
{
	int i;
	u8 currWeather = gWeatherPtr->currWeather;
	if (GetFadeTypeByWeather(currWeather) != 2)
		return; //Only brighten if there is fog weather and not underwater

	if (gWeatherPtr->palProcessingState != 3)
		return; // don't brighten while fading

	for (i = 16; i < 32; i++)
	{
		if (PaletteNeedsFogBrightening(i))
		{
			if (currWeather == 14)
				FogBrightenAndFade(i, 0x1, RGB(15, 16, 14));
			else
				BlendPalette(i * 16, 16, brightenIntensity, FOG_FADE_COLOUR);
		}
	}
}

void FogBrightenAndFade(u8 palSlot, u8 fadeIntensity, u16 fadeColor)
{
	u8 R, G, B;
	u16 color;
	int i;
	u8 brightenIntensity = REG_BLDALPHA & 0xff;

	u16 brightenColor = FOG_FADE_COLOUR;

	for (i = 0; i < 16; i++)
	{
		color = gPlttBufferUnfaded[palSlot * 16 + i];
		R = Red(color);
		G = Green(color);
		B = Blue(color);

		R += (Red(brightenColor) - R) *  brightenIntensity / 16;
		G += (Green(brightenColor) - G) *  brightenIntensity / 16;
		B += (Blue(brightenColor) - B) *  brightenIntensity / 16;

		R += (Red(fadeColor) - R) *  fadeIntensity / 16;
		G += (Green(fadeColor) - G) *  fadeIntensity / 16;
		B += (Blue(fadeColor) - B) *  fadeIntensity / 16;

		gPlttBufferFaded[palSlot * 16 + i] = RGB(R, G, B);
	}
}

void LoadCloudOrSandstormPalette(u16* pal) //Hook at 0x7ABC0 via r1
{
	u8 palSlot;
	palSlot = AddPalRef(PalTypeWeather, 0x1200);
	if (palSlot == 0xFF)
		return;

	DoLoadSpritePalette(pal, palSlot * 16);
	sub_8083598(palSlot);
	MaskPaletteIfFadingIn(palSlot);
}

u8 GetDarkeningTypeBySlot(u8 palSlot) //Replaces table at 0x3C2CC0
{
	u8 type;
	if (palSlot < 13)
		return 1;

	if (palSlot < 16)
		return 0;

	type = sPalRefs[palSlot - 16].Type;
	switch (type) {
		case PalTypeNPC:
		case PalTypeWeather:
			return 2;
		case PalTypeAnimation:
		case PalTypeReflection:
			return 1;
		default:
			return 0;
	}
}

extern const u16 HailstormWeatherPal[];

void LoadPaletteForOverworldSandstorm(void)
{
	/*if (gWeatherPtr->currWeather == WEATHER_STEADY_SNOW) //"Snow"
		LoadCustomWeatherSpritePalette(HailstormWeatherPal);
	else*/
        LoadCustomWeatherSpritePalette(gSandstormWeatherPalette);
}
