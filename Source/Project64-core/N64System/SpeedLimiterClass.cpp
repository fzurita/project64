/****************************************************************************
*                                                                           *
* Project64 - A Nintendo 64 emulator.                                       *
* http://www.pj64-emu.com/                                                  *
* Copyright (C) 2012 Project64. All rights reserved.                        *
*                                                                           *
* License:                                                                  *
* GNU/GPLv2 http://www.gnu.org/licenses/gpl-2.0.html                        *
*                                                                           *
****************************************************************************/

#include "stdafx.h"
#include "Project64-Core/N64System/SpeedLimiterClass.h"

#include <Common/Util.h>

// ---------------------------------------------------

const uint32_t CSpeedLimiter::m_DefaultSpeed = 60;
const uint32_t CSpeedLimiter::m_AverageSamples = 3;

// ---------------------------------------------------

CSpeedLimiter::CSpeedLimiter() :
m_Frames(0),
m_Speed(m_DefaultSpeed),
m_BaseSpeed(m_DefaultSpeed)
{
}

CSpeedLimiter::~CSpeedLimiter()
{
}

void CSpeedLimiter::SetHertz(uint32_t Hertz)
{
    m_Speed = Hertz;
    m_BaseSpeed = Hertz;
    FixSpeedRatio();
}

void CSpeedLimiter::FixSpeedRatio()
{
    m_MicroSecondsPerFrame = 1000000 / m_Speed;
    m_Frames = 0;
}

bool CSpeedLimiter::Timer_Process(uint32_t * FrameRate)
{
    static bool hasBeenReset = false;
    static HighResTimeStamp StartFPSTime;
    static uint32_t lastMicrosecondsPerFrame = 0;
    static int64_t sleepTimes[m_AverageSamples];
    static uint32_t sleepTimesIndex = 0;
    static uint32_t lastFrameRateSet = 0;
    HighResTimeStamp CurrentFPSTime;
    CurrentFPSTime.SetToNow();

    //if this is the first time or we are resuming from pause
    if (StartFPSTime.GetMicroSeconds() == 0 || !hasBeenReset || lastMicrosecondsPerFrame != m_MicroSecondsPerFrame)
    {
        StartFPSTime = CurrentFPSTime;
        m_LastTime = CurrentFPSTime;
        m_Frames = 0;
        lastFrameRateSet = 0;
        hasBeenReset = true;
    }
    else
    {
        ++m_Frames;
    }

    lastMicrosecondsPerFrame = m_MicroSecondsPerFrame;

    int64_t totalElapsedGameTime = m_MicroSecondsPerFrame * m_Frames;
    int64_t elapsedRealTime = CurrentFPSTime.GetMicroSeconds() - StartFPSTime.GetMicroSeconds();
    int64_t sleepTime = totalElapsedGameTime - elapsedRealTime;

    //Reset if the sleep needed is an unreasonable value
    float speedFactor = static_cast<float>(m_Speed) / m_BaseSpeed;
    static const int64_t minSleepNeeded = -50000;
    static const int64_t maxSleepNeeded = 50000;
    if (sleepTime < minSleepNeeded || sleepTime > (maxSleepNeeded*speedFactor))
    {
        hasBeenReset = false;
    }

    sleepTimes[sleepTimesIndex%m_AverageSamples] = sleepTime;
    sleepTimesIndex++;

    uint32_t elementsForAverage = sleepTimesIndex > m_AverageSamples ? m_AverageSamples : sleepTimesIndex;

    // compute the average sleepTime
    double sum = 0;
    for (uint32_t index = 0; index < elementsForAverage; index++)
    {
        sum += sleepTimes[index];
    }

    int64_t averageSleepUs = static_cast<int64_t>(sum / elementsForAverage);

    // Sleep for the average to smooth out jitter
    if (averageSleepUs > 0 && averageSleepUs < maxSleepNeeded*speedFactor)
    {
        pjutil::Sleep(static_cast<uint32_t>(averageSleepUs / 1000));
    }

    float fpsInterval = static_cast<float>(CurrentFPSTime.GetMicroSeconds() - m_LastTime.GetMicroSeconds());
    if (fpsInterval >= 1000000)
    {
        /* Output FPS */
        if (FrameRate != NULL) { *FrameRate = static_cast<uint32_t>(std::round((m_Frames - lastFrameRateSet)/(fpsInterval/1.0e6))); }

        m_LastTime = CurrentFPSTime;
        lastFrameRateSet = m_Frames;
        return true;
    }
    return false;
}

void CSpeedLimiter::AlterSpeed( const ESpeedChange SpeedChange )
{
	int32_t SpeedFactor = 1;
	if (SpeedChange == DECREASE_SPEED) { SpeedFactor = -1; }

	if (m_Speed >= m_DefaultSpeed)
	{
		m_Speed += 10 * SpeedFactor;
	}
	else if (m_Speed >= 15)
	{
		m_Speed += 5 * SpeedFactor;
	}
	else if ((m_Speed > 1 && SpeedChange == DECREASE_SPEED) || SpeedChange == INCREASE_SPEED)
	{
		m_Speed += 1 * SpeedFactor;
	}

	SpeedChanged(m_Speed);
	FixSpeedRatio();
}

void CSpeedLimiter::SetSpeed(int Speed)
{
    if (Speed < 1)
    {
        Speed = 1;
    }
    m_Speed = Speed;
    SpeedChanged(m_Speed);
    FixSpeedRatio();
}

int CSpeedLimiter::GetSpeed(void) const
{
    return m_Speed;
}

int CSpeedLimiter::GetBaseSpeed(void) const
{
    return m_BaseSpeed;
}
