#ifndef MML_SONGCOLLECTION_H
#define MML_SONGCOLLECTION_H

#include "MMLSong.h"
#include "./../MScriptLoader.h"
#include "./../MusicOpcode.h"
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace fm {

	// ROM offsets
	struct BytecodeChannel {
		std::size_t start;
		std::map<std::size_t, fm::MusicInstruction> instrs;
		std::set<std::size_t> jump_targets;
	};

	// instruction offsets
	struct NormalizedBytecodeChannel {
		std::vector<fm::MusicInstruction> instrs;
		std::size_t start;
		std::set<std::size_t> jump_targets;
	};

	struct MMLSongCollection {
		int bpm; // ticks per minute - 3600 NTSC, 3000 PAL
		std::vector<fm::MMLSong> songs;

		void extract_bytecode_collection(MScriptLoader& p_loader);

		MMLSongCollection(int p_bpm);
		std::string to_string(void) const;

	private:
		// turn bytecode into mml via an MML loader
		fm::NormalizedBytecodeChannel normalize_bytecode_channel(
			const fm::BytecodeChannel& ch) const;
		BytecodeChannel extract_bytecode_channel(MScriptLoader& p_loader, std::size_t p_song_no,
			std::size_t p_channel_no);
		std::vector<BytecodeChannel> extract_bytecode_song(MScriptLoader& p_loader, std::size_t p_song_no);

		int determine_tempo(const std::vector<BytecodeChannel>& p_song) const;
		std::set<int> candidate_q(void) const;
	};

}

#endif
