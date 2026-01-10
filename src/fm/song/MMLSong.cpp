#include "MMLSong.h"
#include "./../../common/klib/Kstring.h"
#include "mml_constants.h"
#include <algorithm>
#include <format>

std::string fm::MMLSong::to_string(void) const {
	std::string result{ std::format("#song {}\n", index) };
	result += std::format("t{}\n\n", tempo.to_tempo_string());

	for (const auto& ch : channels)
		result += ch.to_string() + "\n";

	return result;
}

smf::MidiFile fm::MMLSong::to_midi(int p_bpm, const std::vector<int>& p_global_transpose) {
	smf::MidiFile l_midi;
	l_midi.setTicksPerQuarterNote(60);
	l_midi.addTempo(0, 0, static_cast<double>(p_bpm) / static_cast<double>(60));

	int songtransp{ channels.at(0).get_song_transpose() };

	int max_ticks{ 0 };
	max_ticks = std::max(max_ticks, channels.at(0).add_midi_track(l_midi, p_global_transpose.at(0) + songtransp));
	max_ticks = std::max(max_ticks, channels.at(1).add_midi_track(l_midi, p_global_transpose.at(1) + + songtransp));
	max_ticks = std::max(max_ticks, channels.at(2).add_midi_track(l_midi, p_global_transpose.at(2) + songtransp));
	channels.at(3).add_midi_track(l_midi, 0, max_ticks);

	return l_midi;
}

fm::MMLChannel fm::MMLSong::get_channel_of_type(fm::ChannelType p_chan_type) const {

	const auto get_channel_index = [*this](fm::ChannelType p_type) -> std::size_t {
		for (std::size_t i{ 0 }; i < channels.size(); ++i)
			if (channels[i].channel_type == p_type)
				return i;
		throw std::runtime_error(std::format("Song {} is missing a channel", index));
		};

	return channels[get_channel_index(p_chan_type)];
}

void fm::MMLSong::sort(void) {

	std::vector<MMLChannel> l_new_chans;

	l_new_chans.push_back(get_channel_of_type(fm::ChannelType::sq1));
	l_new_chans.push_back(get_channel_of_type(fm::ChannelType::sq2));
	l_new_chans.push_back(get_channel_of_type(fm::ChannelType::tri));
	l_new_chans.push_back(get_channel_of_type(fm::ChannelType::noise));

	channels = l_new_chans;
}
