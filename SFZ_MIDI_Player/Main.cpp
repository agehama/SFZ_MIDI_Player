﻿#include <Siv3D.hpp> // OpenSiv3D v0.6.4
#include <SamplePlayer.hpp>
#include <Utility.hpp>
#include <SFZLoader.hpp>
#include <PianoRoll.hpp>
#include <MIDILoader.hpp>
#include <SampleSource.hpp>

#define LAYOUT_HORIZONTAL

void Main()
{
#ifdef LAYOUT_HORIZONTAL
	Window::Resize(1280, 720);
	const auto [pianorollArea, keyboardArea] = SplitUpDown(Scene::Rect(), 0.8);
#else
	Window::Resize(1280, 1280);
	const auto [keyboardArea, pianorollArea] = SplitLeftRight(Scene::Rect(), 0.1);
#endif

	const auto data = LoadSfz(U"sound/Grand Piano, Kawai.sfz");

	SamplePlayer player{ keyboardArea };
	player.loadData(data);

	PianoRoll pianoRoll{ pianorollArea, 300 };

	std::shared_ptr<SamplerAudioStream> audioStream = std::make_shared<SamplerAudioStream>(pianoRoll, player);
	Audio audio{ audioStream };
	audio.play();

	Optional<MidiData> midiData;

	DragDrop::AcceptFilePaths(true);

	while (System::Update())
	{
		if (DragDrop::HasNewFilePaths())
		{
			const auto filepath = DragDrop::GetDroppedFilePaths().front();

			if (U"mid" == FileSystem::Extension(filepath.path))
			{
				midiData = LoadMidi(filepath.path);
				player.addEvents(midiData.value());
				pianoRoll.playRestart();
				audioStream->restart();
			}
			else if (U"sfz" == FileSystem::Extension(filepath.path))
			{
				pianoRoll.pause();
				audio.pause();

				Console << U"読み込み中：" << filepath.path;
				player.loadData(LoadSfz(filepath.path));
				Console << U"読み込み完了";

				pianoRoll.resume();
				audio.play();
			}
		}

		if (KeySpace.down())
		{
			if (pianoRoll.isPlaying())
			{
				pianoRoll.pause();
				audio.pause();
			}
			else
			{
				pianoRoll.resume();
				audio.play();
			}
		}

		if (KeyHome.down())
		{
			pianoRoll.playRestart();
			audioStream->restart();
		}

		if (midiData)
		{
			pianoRoll.updateTick(midiData.value());

#ifdef LAYOUT_HORIZONTAL
			pianoRoll.drawHorizontal(player.keyMin(), player.keyMax(), midiData.value());
			player.drawHorizontal(pianoRoll, midiData.value());
#else
			pianoRoll.drawVertical(player.keyMin(), player.keyMax(), midiData.value());
			player.drawVertical2(pianoRoll, midiData.value());
#endif
		}
	}
}