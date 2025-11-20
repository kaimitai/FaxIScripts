#include "IScriptLoader.h"
#include "fi_constants.h"
#include <algorithm>
#include <format>

fi::IScriptLoader::IScriptLoader(const std::vector<byte>& p_rom) :
	rom{ p_rom }
{
}

void fi::IScriptLoader::parse_rom(const fe::Config& p_config) {
	// parse strings
	parse_strings(p_config);

	// parse IScript blob
	ptr_table.clear();

	// extract vals we need from config
	std::size_t l_iscript_count{ p_config.constant(c::ID_ISCRIPT_COUNT) };
	auto l_iscript_ptr{ p_config.pointer(c::ID_ISCRIPT_PTR_LO) };

	for (std::size_t i{ 0 }; i < l_iscript_count; ++i)
		ptr_table.push_back(static_cast<std::size_t>(rom.at(l_iscript_ptr.first + i))
			+ 256 * static_cast<std::size_t>(rom.at(l_iscript_ptr.first + l_iscript_count + i))
			+ l_iscript_ptr.second);

	for (const auto offset : ptr_table)
		parse_blob_from_entrypoint(offset, l_iscript_ptr.second, true);
}

void fi::IScriptLoader::parse_strings(const fe::Config& p_config) {
	m_strings.clear();
	std::string encodedstring;

	const auto& lc_char_map{ p_config.bmap(c::ID_STRING_CHAR_MAP) };

	for (std::size_t i{ p_config.constant(c::ID_STRING_DATA_START) };
		i < p_config.constant(c::ID_STRING_DATA_END) && m_strings.size() < 255;
		++i) {

		if (rom.at(i) == 0xff) {
			m_strings.push_back(encodedstring);
			encodedstring.clear();
		}
		else {
			byte b{ rom.at(i) };
			auto iter{ lc_char_map.find(b) };
			if (iter == end(lc_char_map))
				encodedstring += std::format("<${:02x}>", b);
			else
				encodedstring += iter->second;
		}

	}
}

byte fi::IScriptLoader::read_byte(size_t& offset) const {
	if (offset >= rom.size()) throw std::out_of_range("ROM read out of bounds");
	return rom[offset++];
}

uint16_t fi::IScriptLoader::read_short(size_t& offset) const {
	uint8_t lo = read_byte(offset);
	uint8_t hi = read_byte(offset);
	return static_cast<uint16_t>(hi << 8 | lo);
}

std::string fi::IScriptLoader::to_hex(size_t value) {
	return std::format("0x{:x}", value);
}

void fi::IScriptLoader::parse_blob_from_entrypoint(size_t offset,
	size_t zeroaddr, bool at_entrypoint) {
	if (m_instructions.find(offset) != end(m_instructions))
		return;

	size_t cursor = offset;

	if (at_entrypoint)
		m_instructions.insert(std::make_pair(offset,
			fi::Instruction({ Instruction_type::Directive, read_byte(cursor), 1 })
		));

	while (cursor < rom.size()) {
		if (m_instructions.find(cursor) != end(m_instructions))
			return;

		size_t instr_offset = cursor;
		uint8_t opcode_byte = read_byte(cursor);

		auto it = opcodes.find(opcode_byte);
		if (it == opcodes.end()) {
			throw std::runtime_error("Unknown opcode " + to_hex(opcode_byte) +
				" at offset " + to_hex(instr_offset));
		}

		const Opcode& op = it->second;
		std::optional<uint16_t> arg;

		if (op.arg_type == ArgType::Byte) {
			arg = read_byte(cursor);
		}
		else if (op.arg_type == ArgType::Short) {
			arg = read_short(cursor);
		}

		std::optional<std::size_t> target_addr;

		// track jump targets, extract shops
		if (op.flow == Flow::Jump || op.flow == Flow::Read) {
			target_addr = static_cast<std::size_t>(read_short(cursor))
				+ zeroaddr;

			if (op.flow == Flow::Jump)
				m_jump_targets.insert(target_addr.value());
			else {
				const auto& shop_iter{ m_shop_addresses.find(target_addr.value()) };

				if (shop_iter == end(m_shop_addresses)) {
					// new shop, parse it and assign index
					fi::Shop newshop;
					std::size_t shop_offset{ target_addr.value() };
					while (rom.at(shop_offset) != 0xff) {
						newshop.add_entry(rom.at(shop_offset),
							rom.at(shop_offset + 1),
							rom.at(shop_offset + 2));
						shop_offset += 3;
					}

					arg = static_cast<uint16_t>(m_shops.size());
					m_shop_addresses[target_addr.value()] = static_cast<std::size_t>(arg.value());
					m_shops.push_back(newshop);

				}
				else {
					// already seen shop, use its index
					arg = static_cast<uint16_t>(shop_iter->second);
				}
			}
		}

		m_instructions.insert(
			std::make_pair(instr_offset,
				fi::Instruction(fi::Instruction_type::OpCode, opcode_byte, it->second.size(),
					arg, target_addr)));

		if (op.flow == Flow::Jump) {
			// parse all branches recursively, but store and restore cursors
			std::size_t temp_cursor{ cursor };
			cursor = target_addr.value();

			parse_blob_from_entrypoint(cursor, zeroaddr, false);

			cursor = temp_cursor;
		}

		if (op.ends_stream)
			break;

	}
}
