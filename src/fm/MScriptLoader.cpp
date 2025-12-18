#include "MScriptLoader.h"
#include "fm_constants.h"
#include "fm_util.h"

fm::MScriptLoader::MScriptLoader(const std::vector<byte>& p_rom) :
	m_rom{ p_rom }
{
}

void fm::MScriptLoader::parse_rom(const fe::Config& p_config) {
	auto l_music_ptr{ p_config.pointer(c::ID_MUSIC_PTR) };
	// 4 channels per tune
	auto l_music_count{ 4 * p_config.constant(c::ID_MUSIC_COUNT) };

	m_ptr_table.clear();
	m_instrs.clear();
	m_opcodes.clear();
	m_chan_pitch_offsets.clear();

	// extract ptr table
	for (std::size_t i{ 0 }; i < l_music_count; ++i)
		m_ptr_table.push_back(static_cast<std::size_t>(m_rom.at(l_music_ptr.first + 2 * i))
			+ 256 * static_cast<std::size_t>(m_rom.at(l_music_ptr.first + 2 * i + 1))
			+ l_music_ptr.second);

	// generate opcodes
	m_opcodes = parse_opcode_map(p_config.bmap(c::ID_MSCRIPT_OPCODES));

	// extract the channel pitch offsets
	std::size_t l_chan_pitch_offset{ p_config.constant(c::ID_CHAN_PITCH_OFFSET) };
	for (std::size_t i{ 0 }; i < 4; ++i)
		m_chan_pitch_offsets.push_back(
			static_cast<int8_t>(m_rom.at(l_chan_pitch_offset + i))
		);

	for (std::size_t ep : m_ptr_table)
		parse_blob_from_entrypoint(ep, l_music_ptr.second);
}

void fm::MScriptLoader::parse_blob_from_entrypoint(size_t offset, size_t zeroaddr) {
	std::size_t cursor = offset;

	while (cursor < m_rom.size()) {
		if (m_instrs.find(cursor) != end(m_instrs))
			return;

		std::size_t instr_offset{ cursor };

		byte opcode_byte{ read_byte(cursor) };

		const auto& opcode{
			fm::util::decode_opcode_byte(opcode_byte, m_opcodes)
		};

		std::optional<byte> arg;
		std::optional<std::size_t> target_addr;

		if (opcode.m_argtype == fm::AudioArgType::Byte)
			arg = read_byte(cursor);

		if (opcode.m_flow == fm::AudioFlow::Jump) {
			target_addr = read_short(cursor) + zeroaddr;
			m_jump_targets.insert(target_addr.value());
		}

		m_instrs.insert(
			std::make_pair(instr_offset,
				fm::MusicInstruction(opcode_byte, arg, target_addr)));

		if (opcode.m_flow == fm::AudioFlow::Jump) {
			// parse all branches recursively, but store and restore cursors
			std::size_t temp_cursor{ cursor };
			cursor = target_addr.value();

			parse_blob_from_entrypoint(cursor, zeroaddr);

			cursor = temp_cursor;
		}

		if (opcode.m_flow == fm::AudioFlow::End)
			break;
	}
}

byte fm::MScriptLoader::read_byte(std::size_t& offset) const {
	return m_rom.at(offset++);
}

uint16_t fm::MScriptLoader::read_short(std::size_t& offset) const {
	byte lo = read_byte(offset);
	byte hi = read_byte(offset);
	return static_cast<uint16_t>(hi << 8 | lo);
}
