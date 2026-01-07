#include "MMLSong.h"
#include "./../../common/klib/Kstring.h"
#include "mml_constants.h"
#include <format>

std::string fm::MMLSong::to_string(void) const {
	std::string result{ std::format("#song {}\n", index) };
	result += std::format("#tempo {}\n\n", tempo);

	for (const auto& ch : channels)
		result += ch.to_string() + "\n";

	return result;
}

smf::MidiFile fm::MMLSong::to_midi(int p_bpm) {
	smf::MidiFile l_midi;
	l_midi.setTicksPerQuarterNote(60);
	l_midi.addTempo(0, 0, 60);

	int songtransp{ channels.at(0).get_song_transpose() };

	channels.at(0).add_midi_track(l_midi, -12 + songtransp);
	channels.at(1).add_midi_track(l_midi, -12 + songtransp);
	channels.at(2).add_midi_track(l_midi, 12 + songtransp);

	return l_midi;
}
