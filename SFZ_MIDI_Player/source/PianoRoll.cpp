#pragma once
#include <PianoRoll.hpp>
#include <MIDILoader.hpp>
#include <Utility.hpp>

void PianoRoll::drawVertical(int keyMin, int keyMax, const Optional<MidiData>& midiDataOpt) const
{
	const double bottomY = m_area.y + m_area.h;
	const double unitHeight = m_area.h / (keyMax - keyMin + 1);

	const Color bgColor(19, 19, 22);
	const Color measureLineColor(214, 214, 214);
	const Color measureFontColor(214, 214, 214);
	m_area.draw(bgColor);

	if (!midiDataOpt)
	{
		return;
	}

	const auto& midiData = midiDataOpt.value();

	const double widthTick = (midiData.resolution() / 480.0) * m_area.w / m_drawScale;

	const double leftTick = m_currentTick;
	const double rightTick = leftTick + widthTick;
	{
		const auto measures = midiData.getMeasures();

		for (const auto& measure : measures)
		{
			const uint32 beginTick = measure.globalTick;
			const uint32 endTick = beginTick + measure.widthOfTicks();

			if (endTick < leftTick || rightTick < beginTick)
			{
				continue;
			}

			// 小節の区切り線
			{
				const double x = Math::Map(beginTick, leftTick, rightTick, m_area.x, m_area.x + m_area.w);
				const Line line(x, m_area.y, x, m_area.y + m_area.h);
				DrawDotLine(line, m_unitLength, m_interval, 1, measureLineColor);

				m_font(measure.measureIndex).draw(line.begin + Vec2(16, 8), measureFontColor);
			}

			// 拍の区切り線
			//for (const auto& beat : measure.beats)
			//{
			//	if (0 < beat.localTick)
			//	{
			//		const uint32 tick = beginTick + beat.localTick;
			//		const double x = Math::Map(tick, leftTick, rightTick, m_area.x, m_area.x + m_area.w);
			//		const Line line(x, m_area.y, x, m_area.y + m_area.h);
			//		line.draw(2.0, Color(71, 74, 74, 128));
			//	}
			//}
		}
	}

	{
		const auto tracks = midiData.notes();
		for (const auto& [i, track] : Indexed(tracks))
		{
			const HSV hsv(360.0 * i / 16.0, 0.5, 0.53);
			const HSV darker(hsv.h, hsv.s, hsv.v * 0.5);

			if (i == 10)
			{
				continue;
			}

			for (const auto& note : track.notes())
			{
				const uint32 beginTick = note.tick;
				const uint32 endTick = note.tick + note.gate;

				if (endTick < leftTick || rightTick < beginTick)
				{
					continue;
				}

				const double x0 = Math::Map(beginTick, leftTick, rightTick, m_area.x, m_area.x + m_area.w);
				const double x1 = Math::Map(endTick, leftTick, rightTick, m_area.x, m_area.x + m_area.w);

				//const int octaveAbs = static_cast<int>(floor(note.key / 12.0));
				//const int noteIndex = note.key - octaveAbs * 12;
				const int keyIndex = note.key - keyMin;
				const double currentY = bottomY - unitHeight * (keyIndex + 1);

				const RectF rect(x0, currentY, x1 - x0, unitHeight);
				rect.draw(hsv);
				rect.drawFrame(2, darker);
			}
		}
	}
}

void PianoRoll::drawHorizontal(int keyMin, int keyMax, const Optional<MidiData>& midiDataOpt) const
{
	const double leftX = m_area.x;
	const double unitWidth = m_area.w / (keyMax - keyMin + 1);

	const Color bgColor(19, 19, 22);
	const Color measureLineColor(214, 214, 214);
	const Color measureFontColor(214, 214, 214);
	m_area.draw(bgColor);

	if (!midiDataOpt)
	{
		return;
	}

	const auto& midiData = midiDataOpt.value();

	const double heightTick = (midiData.resolution() / 480.0) * m_area.h / m_drawScale;

	const double bottomTick = m_currentTick;
	const double topTick = bottomTick + heightTick;
	{
		const auto measures = midiData.getMeasures();

		for (const auto& measure : measures)
		{
			const uint32 beginTick = measure.globalTick;
			const uint32 endTick = beginTick + measure.widthOfTicks();

			if (endTick < bottomTick || topTick < beginTick)
			{
				continue;
			}

			// 小節の区切り線
			{
				const double y = Math::Map(beginTick, bottomTick, topTick, m_area.y + m_area.h, m_area.y);
				const Line line(m_area.x, y, m_area.x + m_area.w, y);
				DrawDotLine(line.movedBy(Vec2(0, 1)), m_unitLength, m_interval, 1, Palette::Black);
				DrawDotLine(line, m_unitLength, m_interval, 1, measureLineColor);

				const auto measureNumber = measure.measureIndex + 1;
				m_font(measureNumber).draw(Arg::bottomLeft = line.begin + Vec2(14, -8), Palette::Black);
				m_font(measureNumber).draw(Arg::bottomLeft = line.begin + Vec2(14, -10), measureFontColor);
			}

			// 拍の区切り線
			//for (const auto& beat : measure.beats)
			//{
			//	if (0 < beat.localTick)
			//	{
			//		const uint32 tick = beginTick + beat.localTick;
			//		const double y = Math::Map(tick, bottomTick, topTick, m_area.y + m_area.h, m_area.y);
			//		const Line line(m_area.x, y, m_area.x + m_area.w, y);
			//		line.draw(2.0, Color(71, 74, 74, 128));
			//	}
			//}
		}
	}

	{
		const auto tracks = midiData.notes();
		for (const auto& [i, track] : Indexed(tracks))
		{
			const HSV hsv(360.0 * i / 16.0, 0.5, 0.53);
			const HSV darker(hsv.h, hsv.s, hsv.v * 0.5);

			if (i == 10)
			{
				continue;
			}

			for (const auto& note : track.notes())
			{
				const uint32 beginTick = note.tick;
				const uint32 endTick = note.tick + note.gate;

				if (endTick < bottomTick || topTick < beginTick)
				{
					continue;
				}

				const double y0 = Math::Map(beginTick, bottomTick, topTick, m_area.y + m_area.h, m_area.y);
				const double y1 = Math::Map(endTick, bottomTick, topTick, m_area.y + m_area.h, m_area.y);

				//const int octaveAbs = static_cast<int>(floor(note.key / 12.0));
				//const int noteIndex = note.key - octaveAbs * 12;
				const int keyIndex = note.key - keyMin;
				const double currentX = leftX + unitWidth * keyIndex;

				const RectF rect(currentX, y1, unitWidth, y0 - y1);
				rect.draw(hsv);
				rect.drawFrame(2, darker);
			}
		}
	}
}

void PianoRoll::updateTick(const MidiData& midiData)
{
	m_currentTick = midiData.secondsToTicks2(m_watch.sF());
	m_lastTick = m_currentTick;
}

void PianoRoll::playRestart()
{
	m_watch.restart();
	mIsPlaying = true;
}

void PianoRoll::pause()
{
	m_watch.pause();
	mIsPlaying = false;
}

void PianoRoll::resume()
{
	m_watch.resume();
	mIsPlaying = true;
}

void PianoRoll::progress(size_t sampleCount)
{
	if (mIsPlaying)
	{
		m_currentTime += 1.0 * sampleCount / Wave::DefaultSampleRate;
	}
}
