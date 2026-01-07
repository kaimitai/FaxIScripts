#include "MMLSongCollection.h"
#include "./../fm_constants.h"

fm::MMLSongCollection::MMLSongCollection(int p_bpm) :
	bpm{ p_bpm }
{
}


void fm::MMLSongCollection::extract_bytecode_collection(MScriptLoader& p_loader) {

	for (std::size_t i{ 0 }; i < p_loader.get_song_count(); ++i) {
		auto song{ extract_bytecode_song(p_loader, i) };
		int tempo = determine_tempo(song);

		fm::MMLSong bytesong;
		bytesong.tempo = tempo;
		bytesong.index = static_cast<int>(i + 1);

		for (std::size_t i{ 0 }; i < 4; ++i) {
			fm::MMLChannel bytechannel(tempo, &bpm);
			bytechannel.name = fm::c::CHANNEL_LABELS.at(i);

			auto chandata{ normalize_bytecode_channel(song.at(i)) };

			bytechannel.parse_bytecode(chandata.instrs, chandata.start, chandata.jump_targets);

			bytesong.channels.push_back(bytechannel);
		}

		songs.push_back(bytesong);
	}
}

int fm::MMLSongCollection::determine_tempo(const std::vector<BytecodeChannel>& p_song) const {
	std::set<int> Qs{ candidate_q() };
	std::map<int, int> lcounts; // map from tick length -> usage count

	for (const auto& ch : p_song) {
		for (const auto& instr : ch.instrs) {
			if (instr.second.opcode_byte >= fm::c::HEX_NOTELENGTH_MIN &&
				instr.second.opcode_byte < fm::c::HEX_NOTELENGTH_END)
				++lcounts[static_cast<int>(instr.second.opcode_byte - fm::c::HEX_NOTELENGTH_MIN)];
			else if (instr.second.opcode_byte == fm::c::MSCRIPT_OPCODE_SET_LENGTH)
				++lcounts[static_cast<int>(instr.second.operand.value())];

		}
	}

	// let's calculate and find the best Q
	int bestQ = -1;
	int bestScore = -1;

	for (int Q : Qs) {
		int score = 0;

		for (auto& [D, count] : lcounts) {
			Fraction r(D, 4 * Q); // normalized automatically

			if (fm::c::ALLOWED_FRACTIONS.contains(r)) {
				score += count;
			}
		}

		if (score > bestScore) {
			bestScore = score;
			bestQ = Q;
		}
		/*
		else if (score == bestScore &&
			Q > bestQ) {
			bestQ = Q;
		}
		*/
	}

	return bpm / bestQ;
}

std::set<int> fm::MMLSongCollection::candidate_q(void) const {
	std::set<int> qs;

	int q_min = bpm / 300; // max tempo we allow is 300
	int q_max = bpm / 30;  // min tempo we allow is 30

	for (int q = 1; q * q <= bpm; ++q) {
		if (bpm % q != 0) continue;

		int d1 = q;
		int d2 = bpm / q;

		auto consider = [&](int Q) {
			if (Q >= q_min && Q <= q_max)
				qs.insert(Q);
			};

		consider(d1);
		if (d2 != d1) consider(d2);
	}

	return qs;
}

// this gets rid of all ROM offsets and replaces them with instruction indexes
// for both the jump targets and entrypoing
fm::NormalizedBytecodeChannel fm::MMLSongCollection::normalize_bytecode_channel(
	const fm::BytecodeChannel& ch) const {

	std::vector<fm::MusicInstruction> instrs;
	// ROM offset to instruction index
	std::map<std::size_t, std::size_t> offsettoindex;

	// map all ROM offsets to instruction idx and store them in the result vec
	for (const auto& kv : ch.instrs) {
		std::size_t idx{ instrs.size() };
		offsettoindex[kv.first] = idx;
		instrs.push_back(kv.second);
	}

	// patch all jump targets
	for (auto& ins : instrs) {
		if (ins.jump_target.has_value())
			ins.jump_target = offsettoindex.at(ins.jump_target.value());
	}

	// recalc jump targets: ROM offset -> instruction idx
	std::set<std::size_t> jump_targets;
	for (std::size_t n : ch.jump_targets)
		jump_targets.insert(offsettoindex.at(n));

	// same for entrypoint: ROM offset -> instruction idx on return
	return fm::NormalizedBytecodeChannel(
		instrs,
		offsettoindex.at(ch.start),
		jump_targets
	);
}

fm::BytecodeChannel fm::MMLSongCollection::extract_bytecode_channel(fm::MScriptLoader& p_loader, std::size_t p_song_no,
	std::size_t p_channel_no) {
	p_loader.parse_channel(p_song_no, p_channel_no);
	return fm::BytecodeChannel(p_loader.get_channel_offset(p_song_no, p_channel_no),
		p_loader.m_instrs, p_loader.m_jump_targets);
}

std::vector<fm::BytecodeChannel> fm::MMLSongCollection::extract_bytecode_song(MScriptLoader& p_loader,
	std::size_t p_song_no) {
	std::vector<fm::BytecodeChannel> result;

	for (std::size_t i{ 0 }; i < 4; ++i)
		result.push_back(extract_bytecode_channel(p_loader, p_song_no, i));

	return result;
}

std::string fm::MMLSongCollection::to_string(void) const {
	std::string result;

	for (const auto& song : songs) {
		result += song.to_string() + "\n";
	}

	return result;
}
