#include "global.h"
#include "gflib.h"
#include "link.h"
#include "link_rfu.h"
#include "load_save.h"
#include "m4a.h"
#include "rtc.h"

static int WriteCommand(u8 value);
static int WriteData(u8 value);
static u8 ReadData();
static void EnableGpioPortRead();
static void DisableGpioPortRead();


u8 sRTCFrameCount;
struct SiiRtcInfo sRtc;
struct Clock gClock;
EWRAM_DATA u8 sRTCProbeResult = 0;
EWRAM_DATA u16 sRTCSavedIme = 0;
EWRAM_DATA u16 sRTCErrorStatus = 0;
EWRAM_DATA bool8 sLocked = 0;

static const struct SiiRtcInfo sRtcDummy = {.year = 0, .month = MONTH_JAN, .day = 1}; // 2000 Jan 1

static const s32 sNumDaysInMonths[12] =
{
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
};

void RtcDisableInterrupts(void)
{
	sRTCSavedIme = REG_IME;
	REG_IME = 0;
}

void RtcRestoreInterrupts(void)
{
	REG_IME = sRTCSavedIme;
}

u32 ConvertBcdToBinary(u8 bcd)
{
	if (bcd > 0x9F)
		return 0xFF;

	if ((bcd & 0xF) <= 9)
		return (10 * ((bcd >> 4) & 0xF)) + (bcd & 0xF);
	else
		return 0xFF;
}

static bool8 IsLeapYear(u32 year)
{
	if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
		return TRUE;

	return FALSE;
}

void RtcInit(void)
{
	sRTCErrorStatus = 0;

	RtcDisableInterrupts();
	SiiRtcUnprotect();
	sRTCProbeResult = SiiRtcProbe();
	RtcRestoreInterrupts();

	if ((sRTCProbeResult & 0xF) != 1)
	{
		sRTCErrorStatus = 1;
		return;
	}

	if (sRTCProbeResult & 0xF0)
		sRTCErrorStatus = 2;
	else
		sRTCErrorStatus = 0;

	RtcGetRawInfo(&sRtc);
	sRTCErrorStatus = RtcCheckInfo(&sRtc);
}

u16 RtcGetErrorStatus(void)
{
	return sRTCErrorStatus;
}

void RtcGetInfo(struct SiiRtcInfo *rtc)
{
	if (sRTCErrorStatus & 0xff0)
		*rtc = sRtcDummy;
	else
		RtcGetRawInfo(rtc);
}

void RtcGetDateTime(struct SiiRtcInfo *rtc)
{
	RtcDisableInterrupts();
	SiiRtcGetDateTime(rtc);
	RtcRestoreInterrupts();
}

void RtcGetStatus(struct SiiRtcInfo *rtc)
{
	RtcDisableInterrupts();
	SiiRtcGetStatus(rtc);
	RtcRestoreInterrupts();
}

void RtcGetRawInfo(struct SiiRtcInfo *rtc)
{
	RtcGetStatus(rtc);
	RtcGetDateTime(rtc);
}

u16 RtcCheckInfo(struct SiiRtcInfo *rtc)
{
	u16 errorFlags = 0;
	s32 year;
	s32 month;
	s32 value;

	if (rtc->status & SIIRTCINFO_POWER)
		errorFlags |= RTC_ERR_POWER_FAILURE;

	if (!(rtc->status & SIIRTCINFO_24HOUR))
		errorFlags |= RTC_ERR_12HOUR_CLOCK;

	year = ConvertBcdToBinary(rtc->year);

	if (year == 0xFF)
		errorFlags |= RTC_ERR_INVALID_YEAR;

	month = ConvertBcdToBinary(rtc->month);

	if (month == 0xFF || month == 0 || month > 12)
		errorFlags |= RTC_ERR_INVALID_MONTH;

	value = ConvertBcdToBinary(rtc->day);

	if (value == 0xFF)
		errorFlags |= RTC_ERR_INVALID_DAY;

	if (month == MONTH_FEB)
	{
		if (value > IsLeapYear(year) + sNumDaysInMonths[month - 1])
			errorFlags |= RTC_ERR_INVALID_DAY;
	}
	else
	{
		if (value > sNumDaysInMonths[month - 1])
			errorFlags |= RTC_ERR_INVALID_DAY;
	}

	value = ConvertBcdToBinary(rtc->hour);

	if (value > 24)
		errorFlags |= RTC_ERR_INVALID_HOUR;

	value = ConvertBcdToBinary(rtc->minute);

	if (value > 60)
		errorFlags |= RTC_ERR_INVALID_MINUTE;

	value = ConvertBcdToBinary(rtc->second);

	if (value > 60)
		errorFlags |= RTC_ERR_INVALID_SECOND;

	return errorFlags;
}

static void UpdateClockFromRtc(struct SiiRtcInfo *rtc)
{
	gClock.year = ConvertBcdToBinary(rtc->year) + 2000; //Base year is 2000
	gClock.month = ConvertBcdToBinary(rtc->month);
	gClock.day = ConvertBcdToBinary(rtc->day);
	gClock.dayOfWeek = ConvertBcdToBinary(rtc->dayOfWeek);
	gClock.hour = ConvertBcdToBinary(rtc->hour);
	gClock.minute = ConvertBcdToBinary(rtc->minute);
	gClock.second = ConvertBcdToBinary(rtc->second);
}

void RtcCalcLocalTime(void)
{
	if (sRTCFrameCount == 0)
	{
		RtcInit();
		//u8 prevSecond = gClock.second;
		UpdateClockFromRtc(&sRtc);
		
		/*if (prevSecond == gClock.second
		&& !ScriptContext2_IsEnabled())
		//Probably needs an overworld check and a minute/hour/day etc check too
		//Also the below value should be ~70 for it to work normally
		//Really best to use a custom task that runs in the overworld for this
		{
			ScriptContext2_Enable();
			ScriptContext1_SetupScript(SystemScript_StopZooming);
		}*/
	}
	else if (sRTCFrameCount >= 60) //Update once a second
	{
		sRTCFrameCount = 0;
		return;
	}

	++sRTCFrameCount;
}

void ForceClockUpdate(void)
{
	sRTCFrameCount = 0;
}

//extern vu16 GPIOPortDirection;

void SiiRtcUnprotect(void)
{
	EnableGpioPortRead();
	sLocked = FALSE;
}

void SiiRtcProtect(void)
{
	DisableGpioPortRead();
	sLocked = TRUE;
}

u8 SiiRtcProbe(void)
{
	u8 errorCode;
	struct SiiRtcInfo rtc;

	if (!SiiRtcGetStatus(&rtc))
		return 0;

	errorCode = 0;

	if ((rtc.status & (SIIRTCINFO_POWER | SIIRTCINFO_24HOUR)) == SIIRTCINFO_POWER
	 || (rtc.status & (SIIRTCINFO_POWER | SIIRTCINFO_24HOUR)) == 0)
	{
		// The RTC is in 12-hour mode. Reset it and switch to 24-hour mode.

		// Note that the conditions are redundant and equivalent to simply
		// "(rtc.status & SIIRTCINFO_24HOUR) == 0". It's possible that this
		// was also intended to handle resetting the clock after power failure
		// but a mistake was made.

		if (!SiiRtcReset())
			return 0;

		errorCode++;
	}

	SiiRtcGetTime(&rtc);

	if (rtc.second & TEST_MODE)
	{
		// The RTC is in test mode. Reset it to leave test mode.

		if (!SiiRtcReset())
			return (errorCode << 4) & 0xF0;

		errorCode++;
	}

	return (errorCode << 4) | 1;
}

bool8 SiiRtcReset(void)
{
	u8 result;
	struct SiiRtcInfo rtc;

	/*if (sLocked == TRUE)
		return FALSE;

	sLocked = TRUE;

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI | CS_HI;

	GPIO_PORT_DIRECTION = DIR_ALL_OUT;

	WriteCommand(CMD_RESET | WR);

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI;

	sLocked = FALSE;*/

	rtc.status = SIIRTCINFO_24HOUR;

	result = SiiRtcSetStatus(&rtc);

	return result;
}

bool8 SiiRtcGetStatus(struct SiiRtcInfo *rtc)
{
	u8 statusData;

	if (sLocked == TRUE)
		return FALSE;

	sLocked = TRUE;

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI | CS_HI;

	GPIO_PORT_DIRECTION = DIR_ALL_OUT;

	WriteCommand(CMD_STATUS | RD);

	GPIO_PORT_DIRECTION = DIR_0_OUT | DIR_1_IN | DIR_2_OUT;

	statusData = ReadData();

	rtc->status = (statusData & (STATUS_POWER | STATUS_24HOUR))
				| ((statusData & STATUS_INTAE) >> 3)
				| ((statusData & STATUS_INTME) >> 2)
				| ((statusData & STATUS_INTFE) >> 1);

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI;

	sLocked = FALSE;

	return TRUE;
}

bool8 SiiRtcSetStatus(struct SiiRtcInfo *rtc)
{
	u8 statusData;

	if (sLocked == TRUE)
		return FALSE;

	sLocked = TRUE;

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI | CS_HI;

	statusData = STATUS_24HOUR
			   | ((rtc->status & SIIRTCINFO_INTAE) << 3)
			   | ((rtc->status & SIIRTCINFO_INTME) << 2)
			   | ((rtc->status & SIIRTCINFO_INTFE) << 1);

	GPIO_PORT_DIRECTION = DIR_ALL_OUT;

	WriteCommand(CMD_STATUS | WR);

	WriteData(statusData);

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI;

	sLocked = FALSE;

	return TRUE;
}

bool8 SiiRtcGetDateTime(struct SiiRtcInfo *rtc)
{
	u8 i;

	if (sLocked == TRUE)
		return FALSE;

	sLocked = TRUE;

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI | CS_HI;

	GPIO_PORT_DIRECTION = DIR_ALL_OUT;

	WriteCommand(CMD_DATETIME | RD);

	GPIO_PORT_DIRECTION = DIR_0_OUT | DIR_1_IN | DIR_2_OUT;

	for (i = 0; i < DATETIME_BUF_LEN; i++)
		DATETIME_BUF(rtc, i) = ReadData();

	INFO_BUF(rtc, OFFSET_HOUR) &= 0x7F;

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI;

	sLocked = FALSE;

	return TRUE;
}

bool8 SiiRtcSetDateTime(struct SiiRtcInfo *rtc)
{
	u8 i;

	if (sLocked == TRUE)
		return FALSE;

	sLocked = TRUE;

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI | CS_HI;

	GPIO_PORT_DIRECTION = DIR_ALL_OUT;

	WriteCommand(CMD_DATETIME | WR);

	for (i = 0; i < DATETIME_BUF_LEN; i++)
		WriteData(DATETIME_BUF(rtc, i));

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI;

	sLocked = FALSE;

	return TRUE;
}

bool8 SiiRtcGetTime(struct SiiRtcInfo *rtc)
{
	u8 i;

	if (sLocked == TRUE)
		return FALSE;

	sLocked = TRUE;

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI | CS_HI;

	GPIO_PORT_DIRECTION = DIR_ALL_OUT;

	WriteCommand(CMD_TIME | RD);

	GPIO_PORT_DIRECTION = DIR_0_OUT | DIR_1_IN | DIR_2_OUT;

	for (i = 0; i < TIME_BUF_LEN; i++)
		TIME_BUF(rtc, i) = ReadData();

	INFO_BUF(rtc, OFFSET_HOUR) &= 0x7F;

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI;

	sLocked = FALSE;

	return TRUE;
}

bool8 SiiRtcSetTime(struct SiiRtcInfo *rtc)
{
	u8 i;

	if (sLocked == TRUE)
		return FALSE;

	sLocked = TRUE;

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI | CS_HI;

	GPIO_PORT_DIRECTION = DIR_ALL_OUT;

	WriteCommand(CMD_TIME | WR);

	for (i = 0; i < TIME_BUF_LEN; i++)
		WriteData(TIME_BUF(rtc, i));

	GPIO_PORT_DATA = SCK_HI;
	GPIO_PORT_DATA = SCK_HI;

	sLocked = FALSE;

	return TRUE;
}


static int WriteCommand(u8 value)
{
	u8 i;
	u8 temp;

	for (i = 0; i < 8; i++)
	{
		temp = ((value >> (7 - i)) & 1);
		GPIO_PORT_DATA = (temp << 1) | CS_HI;
		GPIO_PORT_DATA = (temp << 1) | CS_HI;
		GPIO_PORT_DATA = (temp << 1) | CS_HI;
		GPIO_PORT_DATA = (temp << 1) | SCK_HI | CS_HI;
	}

	// control reaches end of non-void function
	return 0;
}

static int WriteData(u8 value)
{
	u8 i;
	u8 temp;

	for (i = 0; i < 8; i++)
	{
		temp = ((value >> i) & 1);
		GPIO_PORT_DATA = (temp << 1) | CS_HI;
		GPIO_PORT_DATA = (temp << 1) | CS_HI;
		GPIO_PORT_DATA = (temp << 1) | CS_HI;
		GPIO_PORT_DATA = (temp << 1) | SCK_HI | CS_HI;
	}

	// control reaches end of non-void function
	return 0;
}

static u8 ReadData()
{
	u8 i;
	u8 temp;
	u8 value = 0;

	for (i = 0; i < 8; i++)
	{
		GPIO_PORT_DATA = CS_HI;
		GPIO_PORT_DATA = CS_HI;
		GPIO_PORT_DATA = CS_HI;
		GPIO_PORT_DATA = CS_HI;
		GPIO_PORT_DATA = CS_HI;
		GPIO_PORT_DATA = SCK_HI | CS_HI;

		temp = ((GPIO_PORT_DATA & SIO_HI) >> 1);
		value = (value >> 1) | (temp << 7); // UB: accessing uninitialized var
	}

	return value;
}

static void EnableGpioPortRead()
{
	GPIO_PORT_READ_ENABLE = 1;
}

static void DisableGpioPortRead()
{
	GPIO_PORT_READ_ENABLE = 0;
}