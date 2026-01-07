#ifndef FM_MML_SONG_H
#define FM_MML_SONG_H

#include <string>
#include <optional>
#include <vector>
#include "MMLChannel.h"
#include "./../../common/midifile/MidiFile.h"

namespace fm {

	struct MMLSong {

		std::vector<fm::MMLChannel> channels;
		int index{ 0 }, tempo{ 0 };

	public:
		MMLSong(void) = default;
		std::string to_string(void) const;
		smf::MidiFile to_midi(int p_bpm);
	};

}

#endif
