#include "MMLSongCollection.h"
#include "./../fm_constants.h"
#include "mml_constants.h"
#include "Fraction.h"

fm::MMLSongCollection::MMLSongCollection(int p_bpm) :
	bpm{ p_bpm }
{
}


void fm::MMLSongCollection::extract_bytecode_collection(MScriptLoader& p_loader) {

	for (std::size_t i{ 0 }; i < p_loader.get_song_count(); ++i) {
		auto song{ extract_bytecode_song(p_loader, i) };
		fm::Fraction tempo = determine_tempo(song);

		fm::MMLSong bytesong;
		bytesong.tempo = tempo;
		bytesong.index = static_cast<int>(i + 1);

		for (std::size_t i{ 0 }; i < 4; ++i) {
			fm::MMLChannel bytechannel(tempo, &bpm);

			bytechannel.channel_type = c::CHANNEL_TYPES[i];

			auto chandata{ normalize_bytecode_channel(song.at(i)) };

			bytechannel.parse_bytecode(chandata.instrs, chandata.start, chandata.jump_targets);

			bytesong.channels.push_back(bytechannel);
		}

		songs.push_back(bytesong);
	}
}

fm::Fraction fm::MMLSongCollection::determine_tempo(
	const std::vector<BytecodeChannel>& p_song) const
{
	// 1. Build histogram of tick lengths
	std::map<int, int> lcounts;

	for (const auto& ch : p_song) {
		for (const auto& instr : ch.instrs) {
			int D = -1;

			if (instr.second.opcode_byte >= fm::c::HEX_NOTELENGTH_MIN &&
				instr.second.opcode_byte < fm::c::HEX_NOTELENGTH_END)
			{
				D = instr.second.opcode_byte - fm::c::HEX_NOTELENGTH_MIN;
			}
			else if (instr.second.opcode_byte == fm::c::MSCRIPT_OPCODE_SET_LENGTH) {
				D = instr.second.operand.value();
			}

			if (D >= 0)
				++lcounts[D];
		}
	}

	// Allowed tempo range
	const int T_min = 30 / 4;
	const int T_max = 300 / 4;

	// 2. Generate candidate tempos
	std::map<fm::Fraction, int> scores;

	for (auto& [D, count] : lcounts) {
		for (const fm::Fraction& f : fm::c::ALLOWED_FRACTIONS) {

			// T = (3600 * f) / D
			fm::Fraction T = fm::Fraction(3600 * 4, 1) * f / fm::Fraction(D, 1);

			// Filter tempo range
			double Td = T.to_double();
			if (Td < T_min || Td > T_max)
				continue;

			// Initialize score bucket
			scores.try_emplace(T, 0);
		}
	}

	// 3. Score each candidate tempo
	for (auto& [T, score] : scores) {
		for (auto& [D, count] : lcounts) {
			for (const fm::Fraction& f : fm::c::ALLOWED_FRACTIONS) {

				// expected ticks = (3600/T) * f
				fm::Fraction expected = fm::Fraction(3600 * 4, 1) * f / T;

				if (expected.is_integer() && expected.extract_whole() == D)
					score += count;
			}
		}
	}

	// 4. Pick best tempo
	fm::Fraction bestT(120 / 4, 1); // fallback
	int bestScore = -1;

	for (auto& [T, score] : scores) {
		if (score > bestScore) {
			bestScore = score;
			bestT = T;
		}
	}

	return bestT * Fraction(4, 1);
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

std::vector<byte> fm::MMLSongCollection::to_bytecode(const fe::Config& p_config) {
	std::vector<byte> result;
	std::vector<std::size_t> ptr_table;
	std::size_t instr_offset{ 0 };
	std::vector<fm::MusicInstruction> instrs;

	for (std::size_t i{ 0 }; i < songs.size(); ++i)
		for (std::size_t c{ 0 }; c < songs[i].channels.size(); ++c) {
			auto c_instrs{ songs[i].channels[c].to_bytecode() };
			ptr_table.push_back(c_instrs.entrypt_idx + instr_offset);

			for (auto& instr : c_instrs.instrs) {
				if (instr.jump_target.has_value())
					instr.jump_target = instr.jump_target.value() + instr_offset;
				instrs.push_back(instr);
			}

			instr_offset += c_instrs.instrs.size();
		}

	// set byte offsets for all instructions
	std::size_t byte_offset{ 0 };
	for (std::size_t i{ 0 }; i < instrs.size(); ++i) {
		instrs[i].byte_offset = byte_offset;
		byte_offset += instrs[i].size();
	}

	// the instructions and jump targets are relative to 0
	// next we set all byte offsets relative to the bank
	auto music_ptr{ p_config.pointer(c::ID_MUSIC_PTR) };

	// diff between our 0-addr @ start of music data and ptr zero addr
	const uint16_t PTR_DELTA{ static_cast<uint16_t>(music_ptr.first + 2 * ptr_table.size()
		- music_ptr.second) };

	// add this delta to all jump targets and then make our ptr table entries
	for (auto& instr : instrs)
		instr.byte_offset = instr.byte_offset.value() + PTR_DELTA;

	// update all jump targets now that all byte offsets are known
	// turn instr index into byte offsets
	for (std::size_t i{ 0 }; i < instrs.size(); ++i) {
		if (instrs[i].jump_target.has_value())
			instrs[i].jump_target = instrs.at(instrs[i].jump_target.value()).byte_offset.value();
	}

	// update ptr table values in the same way
	for (std::size_t& n : ptr_table)
		n = instrs.at(n).byte_offset.value();

	// emit bytes - ptr table
	for (std::size_t n : ptr_table) {
		result.push_back(static_cast<byte>(n % 256));
		result.push_back(static_cast<byte>(n / 256));
	}
	// emit bytes - instructions
	for (const auto& instr : instrs) {
		auto bytes{ instr.get_bytes() };
		result.insert(end(result), begin(bytes), end(bytes));
	}

	return result;
}
