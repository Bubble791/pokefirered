#include "global.h"
#include "m4a.h"
#include "dns.h"
#include "rtc.h"
#include "field_weather.h"
#include "fieldmap.h"
#include "palette.h"
#include "shop.h"
#include "overworld.h"
#include "dynamic_pals.h"
#include "constants/map_types.h"


EWRAM_DATA u8 gLastRecordedFadeCoeff = 0;
EWRAM_DATA u16 gLastRecordedFadeColour = 0;
EWRAM_DATA bool8 gWindowsLitUp = 0;
EWRAM_DATA bool8 gDontFadeWhite = 0;
EWRAM_DATA bool8 gIgnoredDNSPalIndices[16][32] = {0};

static void BlendFadedPalettes(u32 selectedPalettes, u8 coeff, u32 color);
static void BlendFadedPalette(u16 palOffset, u16 numEntries, u8 coeff, u32 blendColor);
static u16 FadeColourForDNS(struct PlttData* blend, u8 coeff, s8 r, s8 g, s8 b);
static void FadeOverworldBackground(u32 selectedPalettes, u8 coeff, u32 color, bool8 palFadeActive);
static bool8 IsDate1BeforeDate2(u32 y1, u32 m1, u32 d1, u32 y2, u32 m2, u32 d2);
static bool8 IsLeapYear(u32 year);

const struct SpecificTilesetFade gSpecificTilesetFades[] =
{
	//These Palette Town ones have been left in as examples. Feel free to remove.
	{ //Palette Town - Player's Door
		.tilesetPointer = 0x82D4AAC, //Tileset 1
		.paletteNumToFade = 8,
		.paletteIndicesToFade =
		{
			{8,  RGB(31, 31, 20)},
			{9,  RGB(31, 31, 11)},
			{10, RGB(31, 31, 10)},
			TILESET_PAL_FADE_END
		},
	},
	{ //Palette Town - Oak's Lab Windows
		.tilesetPointer = 0x82D4AAC, //Tileset 1
		.paletteNumToFade = 9,
		.paletteIndicesToFade =
		{
			{8,  RGB(31, 31, 20)},
			{9,  RGB(31, 31, 14)},
			{10, RGB(31, 30, 0)},
			TILESET_PAL_FADE_END
		},
	},
	{ //Palette Town - Oak's Lab Door
		.tilesetPointer = 0x82D4AAC, //Tileset 1
		.paletteNumToFade = 10,
		.paletteIndicesToFade =
		{
			{8,  RGB(31, 31, 20)},
			{9,  RGB(31, 31, 14)},
			{10, RGB(31, 30, 0)},
			TILESET_PAL_FADE_END
		},
	},
};

const struct DNSPalFade gDNSNightFadingByTime[24][6] =
{
	[0]  = {
			{RGB(1, 0, 17), 6},		//12:00 AM
			{RGB(1, 0, 17), 6},		//12:10 AM
			{RGB(1, 0, 17), 6},		//12:20 AM
			{RGB(1, 0, 17), 6},		//12:30 AM
			{RGB(1, 0, 17), 6},		//12:40 AM
			{RGB(1, 0, 17), 6},		//12:50 AM
		   },

	[1]  = {
			{RGB(1, 0, 17), 6},		//1:00 AM
			{RGB(1, 0, 17), 6},		//1:10 AM
			{RGB(1, 0, 17), 6},		//1:20 AM
			{RGB(1, 0, 17), 6},		//1:30 AM
			{RGB(1, 0, 17), 6},		//1:40 AM
			{RGB(1, 0, 17), 6},		//1:50 AM
		   },

	[2]  = {
			{RGB(1, 0, 17), 6},		//2:00 AM
			{RGB(1, 0, 17), 6},		//2:10 AM
			{RGB(1, 0, 17), 6},		//2:20 AM
			{RGB(1, 0, 17), 6},		//2:30 AM
			{RGB(1, 0, 17), 6},		//2:40 AM
			{RGB(1, 0, 17), 6},		//2:50 AM
		   },

	[3]  = {
			{RGB(1, 0, 17), 5},		//3:00 AM
			{RGB(1, 0, 17), 5},		//3:10 AM
			{RGB(1, 0, 17), 5},		//3:20 AM
			{RGB(1, 0, 17), 5},		//3:30 AM
			{RGB(1, 0, 17), 5},		//3:40 AM
			{RGB(1, 0, 17), 5},		//3:50 AM
		   },

	[4]  = {
			{RGB(0, 6, 16), 4},		//4:00 AM
			{RGB(0, 7, 16), 4},		//4:10 AM
			{RGB(0, 8, 16), 4},		//4:20 AM
			{RGB(0, 9, 16), 4},		//4:30 AM
			{RGB(0, 10, 16), 4},	//4:40 AM
			{RGB(0, 11, 16), 4},	//4:50 AM
		   },

	[5]  = {
			{RGB(0, 12, 16), 3},	//5:00 AM
			{RGB(0, 13, 16), 3},	//5:10 AM
			{RGB(0, 14, 16), 3},	//5:20 AM
			{RGB(0, 15, 16), 3},	//5:30 AM
			{RGB(0, 16, 16), 3},	//5:40 AM
			{RGB(0, 17, 16), 3},	//5:50 AM
		   },

	[6]  = {
			{RGB(0, 18, 16), 2},	//6:00 AM
			{RGB(0, 19, 16), 2},	//6:10 AM
			{RGB(0, 20, 16), 2},	//6:20 AM
			{RGB(0, 21, 16), 2},	//6:30 AM
			{RGB(0, 22, 16), 2},	//6:40 AM
			{RGB(0, 23, 16), 2},	//6:50 AM
		   },

	[7]  = {
			{RGB(0, 24, 16), 1},	//7:00 AM
			{RGB(0, 24, 16), 1},	//7:10 AM
			{RGB(0, 24, 16), 1},	//7:20 AM
			{RGB(0, 24, 16), 1},	//7:30 AM
			{RGB(0, 24, 16), 1},	//7:40 AM
			{RGB(0, 24, 16), 1},	//7:50 AM
		   },

//Day has no fade

	[17]  = {
			{RGB(19, 0, 10), 1},	//5:00 PM
			{RGB(19, 0, 10), 1},	//5:10 PM
			{RGB(19, 0, 10), 1},	//5:20 PM
			{RGB(19, 0, 10), 1},	//5:30 PM
			{RGB(19, 0, 10), 1},	//5:40 PM
			{RGB(19, 0, 10), 1},	//5:50 PM
		   },


	[18]  = {
			{RGB(18, 0, 11), 2},	//6:00 PM
			{RGB(17, 0, 12), 2},	//6:10 PM
			{RGB(16, 0, 13), 2},	//6:20 PM
			{RGB(15, 0, 14), 2},	//6:30 PM
			{RGB(14, 0, 15), 2},	//6:40 PM
			{RGB(13, 0, 16), 2},	//6:50 PM
		   },

	[19]  = {
			{RGB(12, 0, 17), 3},	//7:00 PM
			{RGB(11, 0, 17), 3},	//7:10 PM
			{RGB(10, 0, 18), 3},	//7:20 PM
			{RGB(9, 0, 18), 3},		//7:30 PM
			{RGB(8, 0, 19), 3},		//7:40 PM
			{RGB(7, 0, 19), 3},		//7:50 PM
		   },

	[20]  = {
			{RGB(5, 0, 21), 3},		//8:00 PM
			{RGB(5, 0, 21), 3},		//8:10 PM
			{RGB(5, 0, 21), 3},		//8:20 PM
			{RGB(5, 0, 21), 3},		//8:30 PM
			{RGB(5, 0, 21), 3},		//8:40 PM
			{RGB(5, 0, 21), 3},		//8:50 PM
		   },

	[21]  = {
			{RGB(5, 0, 21), 4},		//9:00 PM
			{RGB(5, 0, 21), 4},		//9:10 PM
			{RGB(5, 0, 21), 4},		//9:20 PM
			{RGB(4, 0, 20), 4},		//9:30 PM
			{RGB(4, 0, 20), 4},		//9:40 PM
			{RGB(4, 0, 20), 4},		//9:50 PM
		   },

	[22]  = {
			{RGB(3, 0, 19), 5},		//10:00 PM
			{RGB(3, 0, 19), 5},		//10:10 PM
			{RGB(3, 0, 19), 5},		//10:20 PM
			{RGB(2, 0, 18), 5},		//10:30 PM
			{RGB(2, 0, 18), 5},		//10:40 PM
			{RGB(2, 0, 18), 5},		//10:50 PM
		   },

	[23]  = {
			{RGB(1, 0, 17), 5},		//11:00 PM
			{RGB(1, 0, 17), 5},		//11:10 PM
			{RGB(1, 0, 17), 5},		//11:20 PM
			{RGB(1, 0, 17), 5},		//11:30 PM
			{RGB(1, 0, 17), 5},		//11:40 PM
			{RGB(1, 0, 17), 5},		//11:50 PM
		   },
};


void FadeDayNightPalettes(void)
{
	u32 palsToFade;
	bool8 inOverworld, fadePalettes;
    u8 paletteIndex;

	switch (gMapHeader.mapType) { //Save time by not calling the function
		case MAP_TYPE_TOWN: //Try to force a jump table to manually placing these values
		case MAP_TYPE_CITY:
		case MAP_TYPE_ROUTE:
		case MAP_TYPE_UNDERWATER:
		case 6:
		case 7:
		default:
			inOverworld = FuncIsActiveTask(Task_WeatherMain);
			fadePalettes = inOverworld || gInShop;

			if (fadePalettes)
			{
				u8 coeff = gDNSNightFadingByTime[gClock.hour][gClock.minute / 10].amount;
				u16 colour = gDNSNightFadingByTime[gClock.hour][gClock.minute / 10].colour;
				bool8 palFadeActive = gPaletteFade.active || gWeatherPtr->palProcessingState == WEATHER_PAL_STATE_SCREEN_FADING_IN;

				if (inOverworld)
					palsToFade = OW_DNS_PAL_FADE;
				else
					palsToFade = OW_DNS_PAL_FADE & ~(OBG_SHI(11)); //Used by shop

				if (gLastRecordedFadeCoeff != coeff
				||  gLastRecordedFadeColour != colour) //Only fade the background if colour should change
				{
					bool8 hardFade = gLastRecordedFadeCoeff == 0xFF; //Set to 0xFF next so check up here

					if (!palFadeActive)
						apply_map_tileset1_tileset2_palette(gMapHeader.mapLayout);

					gWindowsLitUp = FALSE;
					FadeOverworldBackground(palsToFade, coeff, colour, palFadeActive); //Load/remove the palettes to fade once during the day and night
					gLastRecordedFadeCoeff = coeff;
					gLastRecordedFadeColour = colour;

					//The weather fading needs to be reloaded when the tileset palette is reloaded
					if (!palFadeActive)
					{
						for (paletteIndex = 0; paletteIndex < 13; paletteIndex++)
							ApplyWeatherGammaShiftToPal(paletteIndex);
					}

					if (hardFade) //Changed routes and part of the tileset was reloaded
						DmaCopy16(3, gPlttBufferFaded, (void *)PLTT, PLTT_SIZE / 2);
				}

				if (coeff == 0)
					break; //Don't bother fading a null fade

				palsToFade = (palsToFade & ~OW_DNS_BG_PAL_FADE) >> 16;
				BlendFadedPalettes(palsToFade, coeff, colour);
			}
			break;
		case MAP_TYPE_INDOOR: //No fading in these areas
		case MAP_TYPE_UNDERGROUND:
		case 0:
		case MAP_TYPE_SECRET_BASE:
			gLastRecordedFadeCoeff = 0;
			break;
	}
}

static void BlendFadedPalettes(u32 selectedPalettes, u8 coeff, u32 color)
{
	u16 paletteOffset;

	for (paletteOffset = 256; selectedPalettes; paletteOffset += 16)
	{
		if (selectedPalettes & 1)
		{
			switch (GetPalTypeByPaletteOffset(paletteOffset)) {
				case 0:
					//if (paletteOffset < 256) //The background
					//	BlendFadedPalette(paletteOffset, 16, coeff, color);
					//break;
				case 5: //Fade everything except Poke pics
					break;
				default:
					BlendFadedPalette(paletteOffset, 16, coeff, color);
			}
		}
		selectedPalettes >>= 1;
	}
}

static void BlendFadedPalette(u16 palOffset, u16 numEntries, u8 coeff, u32 blendColor)
{
	u16 i;
	u16 ignoreOffset = palOffset / 16;
	bool8 dontFadeWhite = gDontFadeWhite && !gMain.inBattle;
    struct PlttData *data1;
    struct PlttData* data2;
    s8 r,g,b;

	for (i = 0; i < numEntries; ++i)
	{
		u16 index = i + palOffset;
		if (gPlttBufferFaded[index] == RGB_BLACK) continue; //Don't fade black

		if (gIgnoredDNSPalIndices[ignoreOffset][i]) continue; //Don't fade this index.

		//Fixes an issue with pre-battle mugshots
		if (dontFadeWhite && gPlttBufferFaded[index] == RGB_WHITE) continue;

		data1 = (struct PlttData*) &gPlttBufferFaded[index];
		r = data1->r;
		g = data1->g;
		b = data1->b;
		data2 = (struct PlttData*) &blendColor;
		((u16*) PLTT)[index] = FadeColourForDNS(data2, coeff, r, g, b);
	}
}

static void BlendFadedUnfadedPalette(u16 palOffset, u16 numEntries, u8 coeff, u32 blendColor, bool8 palFadeActive)
{
	u16 i;
    u16 index;
	u16 ignoreOffset = palOffset / 16;
    s8 r,g,b;
    struct PlttData *data1;
    struct PlttData* data2;

	for (i = 0; i < numEntries; ++i)
	{
		index = i + palOffset;
		if (gPlttBufferUnfaded[index] == RGB_BLACK) continue; //Don't fade black

		if (gIgnoredDNSPalIndices[ignoreOffset][i]) continue; //Don't fade this index.

		data1 = (struct PlttData*) &gPlttBufferUnfaded[index];
		data2 = (struct PlttData*) &blendColor;
		r = data1->r;
		g = data1->g;
		b = data1->b;

		gPlttBufferUnfaded[index] = FadeColourForDNS(data2, coeff, r, g, b);

		if (!palFadeActive)
			gPlttBufferFaded[index] = FadeColourForDNS(data2, coeff, r, g, b);
	}
}

static u16 FadeColourForDNS(struct PlttData* blend, u8 coeff, s8 r, s8 g, s8 b)
{
	return ((r + (((blend->r - r) * coeff) >> 4)) << 0)
		 | ((g + (((blend->g - g) * coeff) >> 4)) << 5)
		 | ((b + (((blend->b - b) * coeff) >> 4)) << 10);

/*
	u8 coeffMax = 128;

	return (((r * (coeffMax - coeff) + (((r * blend->r) >> 5) * coeff)) >> 8) << 0)
		 | (((g * (coeffMax - coeff) + (((g * blend->g) >> 5) * coeff)) >> 8) << 5)
		 | (((b * (coeffMax - coeff) + (((b * blend->b) >> 5) * coeff)) >> 8) << 10);
*/
}

//This function gets called once when the game transitions to a new fade colour
static void FadeOverworldBackground(u32 selectedPalettes, u8 coeff, u32 color, bool8 palFadeActive)
{
	u32 i, j, row, column;
    u16 paletteOffset;

	if (IsNightTime())
	{
		if (!gWindowsLitUp)
		{
			for (i = 0; i < ARRAY_COUNT(gSpecificTilesetFades); ++i)
			{
				if ((u32) gMapHeader.mapLayout->primaryTileset == gSpecificTilesetFades[i].tilesetPointer
				||  (u32) gMapHeader.mapLayout->secondaryTileset == gSpecificTilesetFades[i].tilesetPointer)
				{
					row = gSpecificTilesetFades[i].paletteNumToFade;

					if (row == 11 && FuncIsActiveTask(Task_BuyMenu))
						continue; //Don't fade palette 11 in shop menu

					for (j = 0; gSpecificTilesetFades[i].paletteIndicesToFade[j].index != 0xFF; ++j)
					{
						column = gSpecificTilesetFades[i].paletteIndicesToFade[j].index;
						gPlttBufferUnfaded[row * 16 + column] = gSpecificTilesetFades[i].paletteIndicesToFade[j].colour;
						if (!palFadeActive)
							gPlttBufferFaded[row * 16 + column] = gSpecificTilesetFades[i].paletteIndicesToFade[j].colour;
						gIgnoredDNSPalIndices[row][column] = TRUE;
					}
				}
			}

			gWindowsLitUp = TRUE;
		}
	}
	else
	{
		if (!palFadeActive)
			apply_map_tileset1_tileset2_palette(gMapHeader.mapLayout);
		memset(gIgnoredDNSPalIndices, 0, sizeof(bool8) * 16 * 32); //Don't ignore colours during day
		gWindowsLitUp = FALSE;
	}

	for (paletteOffset = 0; paletteOffset < 256; paletteOffset += 16) //Only background colours
	{
		if (selectedPalettes & 1)
		{
			BlendFadedUnfadedPalette(paletteOffset, 16, coeff, color, palFadeActive);
		}
		selectedPalettes >>= 1;
	}
}


void apply_map_tileset_palette(struct Tileset const* tileset, u16 destOffset, u16 size)
{
	u16 black = RGB_BLACK;

	if (tileset)
	{
		if (tileset->isSecondary == FALSE)
		{
			LoadPalette(&black, destOffset, 2);
			LoadPalette(((u16*)tileset->palettes) + 1, destOffset + 1, size - 2);
			sub_80598CC(destOffset + 1, (size - 2) >> 1);
		}
		else if (tileset->isSecondary == TRUE)
		{
			LoadPalette(((u16*) tileset->palettes) + (NUM_PALS_IN_PRIMARY * 16), destOffset, size);
			sub_80598CC(destOffset, size >> 1);
		}
		else
		{
			LoadCompressedPalette((u32*) tileset->palettes, destOffset, size);
			sub_80598CC(destOffset, size >> 1);
		}

		memset(gIgnoredDNSPalIndices, 0, sizeof(bool8) * 16 * 32);
		gLastRecordedFadeCoeff = 0xFF; //So the colours can be reloaded on map re-entry
		gLastRecordedFadeColour = 0;
	}
}

void DNSBattleBGPalFade(void)
{
    u16 i, palOffset,index,color;
	u8 coeff = gDNSNightFadingByTime[gClock.hour][gClock.minute / 10].amount;
	u32 blendColor = gDNSNightFadingByTime[gClock.hour][gClock.minute / 10].colour;
	u8 selectedPalettes = BATTLE_DNS_PAL_FADE & 0x1C;
    s8 r,b,g;
    struct PlttData *data2;
    struct PlttData* data1;

	switch (GetCurrentMapType()) {
		case 0:			//No fading in these areas
		case MAP_TYPE_UNDERGROUND:
		case MAP_TYPE_INDOOR:
		case MAP_TYPE_SECRET_BASE:
			return;
	}

	for (palOffset = 0; selectedPalettes; palOffset += 16)
	{
		if (selectedPalettes & 1)
		{
			for (i = 0; i < 16; ++i)
			{
				index = i + palOffset;
				data1 = (struct PlttData*) &gPlttBufferUnfaded[index];
				r = data1->r;
				g = data1->g;
				b = data1->b;
				data2 = (struct PlttData *)&blendColor;
				color = FadeColourForDNS(data2, coeff, r, g, b);

				gPlttBufferUnfaded[index] = color;
				gPlttBufferFaded[index] = color;
			}
		}
		selectedPalettes >>= 1;
	}
}


bool8 IsDayTime(void)
{
	return gClock.hour >= TIME_MORNING_START && gClock.hour < TIME_NIGHT_START;
}

bool8 IsOnlyDayTime(void)
{
	return gClock.hour >= TIME_DAY_START && gClock.hour < TIME_EVENING_START;
}

bool8 IsNightTime(void)
{
	return gClock.hour >= TIME_NIGHT_START || gClock.hour < TIME_MORNING_START;
}

bool8 IsMorning(void)
{
	return gClock.hour >= TIME_MORNING_START && gClock.hour < TIME_DAY_START;
}

bool8 IsEvening(void)
{
	return gClock.hour >= TIME_EVENING_START && gClock.hour < TIME_NIGHT_START;
}

static bool8 IsDate1BeforeDate2(u32 y1, u32 m1, u32 d1, u32 y2, u32 m2, u32 d2)
{
	return y1 < y2 ? TRUE : (y1 == y2 ? (m1 < m2 ? TRUE : (m1 == m2 ? d1 < d2 : FALSE)) : FALSE);
}

static bool8 IsLeapYear(u32 year)
{
	return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

u32 MathMax(u32 num1, u32 num2)
{
	if (num1 > num2)
		return num1;

	return num2;
}

u32 GetMinuteDifference(u32 startYear, u8 startMonth, u8 startDay, u8 startHour, u8 startMin, u32 endYear, u8 endMonth, u8 endDay, u8 endHour, u8 endMin)
{
    u32 days,hours;
	if (startYear > endYear
	|| (startYear == endYear && startMonth > endMonth)
	|| (startYear == endYear && startMonth == endMonth && startDay > endDay)
	|| (startYear == endYear && startMonth == endMonth && startDay == endDay && startHour > endHour)
	|| (startYear == endYear && startMonth == endMonth && startDay == endDay && startHour == endHour && startMin > endMin))
		return 0;

	days = GetDayDifference(startYear, startMonth, startDay, endYear, endMonth, endDay);

	if (days >= 0xFFFFFFFF / 24 / 60)
		return 0xFFFFFFFF; //Max minutes
	else
	{
		hours = GetHourDifference(startYear, startMonth, startDay, startHour, endYear, endMonth, endDay, endHour);

		if (startMin > endMin)
			return (MathMax(1, hours) - 1) * 60 + ((endMin + 60) - startMin);
		else
			return hours * 60 + (endMin - startMin);
	}
}

u32 GetHourDifference(u32 startYear, u8 startMonth, u8 startDay, u8 startHour, u32 endYear, u8 endMonth, u8 endDay, u8 endHour)
{
    u32 days;

	if (startYear > endYear
	|| (startYear == endYear && startMonth > endMonth)
	|| (startYear == endYear && startMonth == endMonth && startDay > endDay)
	|| (startYear == endYear && startMonth == endMonth && startDay == endDay && startHour > endHour))
		return 0;

	days = GetDayDifference(startYear, startMonth, startDay, endYear, endMonth, endDay);

	if (days >= 0xFFFFFFFF / 24)
		return 0xFFFFFFFF; //Max hours

	if (startHour > endHour)
		return (days - 1) * 24 + ((endHour + 24) - startHour);
	else //startHour <= endHour
		return days * 24 + (endHour - startHour);
}

u32 GetDayDifference(u32 startYear, u8 startMonth, u8 startDay, u32 endYear, u8 endMonth, u8 endDay)
{
	const u16 cumDays[] = {0,31,59,90,120,151,181,212,243,273,304,334}; //Cumulative Days by month
	const u16 leapcumDays[] = {0,31,60,91,121,152,182,213,244,274,305,335}; //Cumulative Days by month for leap year
	u32 totdays = 0;
    u32 year;

	if (!IsDate1BeforeDate2(startYear, startMonth, startDay, endYear, endMonth, endDay))
		return 0;

	if (startYear == endYear)
	{
		if (IsLeapYear(startYear))
			return (leapcumDays[endMonth - 1] + endDay) - (leapcumDays[startMonth - 1] + startDay);
		else
			return (cumDays[endMonth-1] + endDay) - (cumDays[startMonth - 1] + startDay);
	}

	if (IsLeapYear(startYear))
		totdays = totdays + 366 - (leapcumDays[startMonth - 1] + startDay);
	else
		totdays = totdays + 365 - (cumDays[startMonth - 1] + startDay);

	year = startYear + 1;
	while (year < endYear)
	{
		if (IsLeapYear(year))
			totdays = totdays + 366;
		else
			totdays = totdays + 365;

		year = year + 1;
	}

    if (IsLeapYear(endYear))
        totdays = totdays + (leapcumDays[endMonth - 1] + endDay);
    else
        totdays = totdays + (cumDays[endMonth - 1] + endDay);

    return totdays;
}

u32 GetMonthDifference(u32 startYear, u8 startMonth, u32 endYear, u8 endMonth)
{
	if (startYear > endYear
	|| (startYear == endYear && startMonth > endMonth))
		return 0;

	else if (endMonth >= startMonth)
		return GetYearDifference(startYear, endYear) * 12 + (endMonth - startMonth);

	else
		return (MathMax(1, GetYearDifference(startYear, endYear)) - 1) * 12 + ((endMonth + 12) - startMonth);
}

u32 GetYearDifference(u32 startYear, u32 endYear)
{
	if (startYear >= endYear)
		return 0;

	return endYear - startYear;
}

/*u8 GetPalTypeByPaletteOffset(u16 offset)
{
	if (&gPlttBufferUnfaded[offset] < gPlttBufferFaded)
		return 0;

	u8 palSlot = (u32) (&gPlttBufferUnfaded[offset] - gPlttBufferFaded) / 16;
	return sPalRefs[palSlot].Type;
}*/