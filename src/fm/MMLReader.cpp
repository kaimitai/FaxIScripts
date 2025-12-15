#include "MMLReader.h"
#include "fm_constants.h"
#include "./../common/klib/Kfile.h"
#include "./../common/klib/Kstring.h"
#include <format>
#include <stdexcept>

fm::MMLReader::MMLReader(const fe::Config& p_config) {
	m_opcodes = parse_xml_map(p_config.bmap(c::ID_MSCRIPT_OPCODES));
}

// we employ the same strategy as for iscript, but this is less comples
// we still need to resolve jump targets and ptr table entries however
void fm::MMLReader::read_mml_file(const std::string& p_filename,
	const fe::Config& p_config) {
	auto music_ptr{ p_config.pointer(c::ID_MUSIC_PTR) };
	std::vector<std::string> tokens;
	{
		auto l_lines{ klib::file::read_file_as_strings(p_filename) };

		for (auto& line : l_lines) {
			line = klib::str::strip_comment(line);
			auto linetokens{ klib::str::split_whitespace(line) };
			for (const auto& token : linetokens)
				tokens.push_back(klib::str::to_lower(klib::str::trim(token)));
		}
	}

	// make an opcode mnemonic reverse lookup
	std::map<std::string, byte> mnemonics;
	for (std::size_t i{ 0 }; i < m_opcodes.size(); ++i) {
		mnemonics.insert(std::make_pair(
			klib::str::to_lower(m_opcodes[i].m_mnemonic), static_cast<byte>(i)
		));
	}

	// clear out our data
	m_ptr_table.clear();
	m_instructions.clear();

	// prepare structures we need for bookkeeping

	// map string label to instruction index
	std::map<std::string, std::size_t> label_to_instr_idx;
	// map from ptr table index to instruction index
	std::map<std::size_t, std::size_t> ptr_to_instr_index;
	// map from label to all instruction indexes having it as target
	std::map<std::string, std::set<std::size_t>> jump_labels;
	// byte offset relative to the start of data
	std::size_t offset{ 0 };

	// we now have all our tokens whitespace-trimmed, and can get to work
	for (std::size_t tokenno{ 0 }; tokenno < tokens.size();) {
		std::string token = tokens[tokenno];

		if (token == klib::str::to_lower(c::ID_MSCRIPT_ENTRYPOINT)) {
			while (++tokenno < tokens.size() && is_entrypoint(tokens[tokenno])) {
				const auto& entrypttoken = tokens[tokenno];
				auto entrypt = parse_entrypoint(entrypttoken);
				std::size_t entryidx{ 4 * entrypt.first + entrypt.second };
				if (ptr_to_instr_index.contains(entryidx))
					throw std::runtime_error(std::format("Multiple definitions for entrypoint {}", entrypttoken));
				ptr_to_instr_index.insert({ entryidx, m_instructions.size() });
			}
		}
		else if (is_label_definition(token)) {
			std::string label{ get_label(token) };

			if (label_to_instr_idx.contains(label))
				throw std::runtime_error(std::format("Label redefinition: {}", label));
			else
				label_to_instr_idx.insert(std::make_pair(
					label,
					m_instructions.size()
				));
			++tokenno;
		}
		else if (tokenno < tokens.size()) {
			if (!mnemonics.contains(token))
				throw std::runtime_error(std::format("Invalid opcode: ", token));

			byte opcodeno{ mnemonics.at(token) };
			const auto& opcode{ m_opcodes.at(opcodeno) };

			std::optional<byte> arg;

			if (opcode.m_argtype == fm::AudioArgType::Byte) {
				arg = klib::str::parse_numeric(tokens.at(++tokenno));
			}
			else if (opcode.m_argtype == fm::AudioArgType::Address) {
				jump_labels[tokens.at(++tokenno)].insert(m_instructions.size());
			}

			m_instructions.push_back(fm::MusicInstruction(
				opcodeno,
				arg,
				std::nullopt,
				offset
			));

			offset += opcode.size();
			++tokenno;
		}
	}

	// diff between our 0-addr @ start of music data and ptr zero addr
	const uint16_t PTR_DELTA{ static_cast<uint16_t>(music_ptr.first + 2 * ptr_to_instr_index.size()
		- music_ptr.second) };

	for (const auto& kv : jump_labels) {
		std::size_t jump_instr{ label_to_instr_idx.at(kv.first) };
		std::size_t jump_offset{ m_instructions.at(jump_instr).byte_offset.value() };

		for (const auto instrind : kv.second)
			m_instructions.at(instrind).jump_target = jump_offset + PTR_DELTA;
	}

	// similarly for ptr table entries
	for (const auto& kv : ptr_to_instr_index) {
		m_ptr_table[kv.first] = m_instructions.at(kv.second).byte_offset.value() + PTR_DELTA;
	}

	// final sanity check
	if (m_ptr_table.size() % 4 != 0) {
		throw std::runtime_error("Four channels must be defined for each song");
	}
	else {
		for (std::size_t i{ 0 }; i < m_ptr_table.size(); ++i)
			if (!m_ptr_table.contains(i))
				throw std::runtime_error(
					std::format("Channel {} not defined for song {}",
						c::CHANNEL_LABELS.at(i % 4), i / 4)
				);
	}
}

bool fm::MMLReader::is_label_definition(const std::string& p_token) {
	return !p_token.empty() &&
		p_token.back() == ':';
}

std::string fm::MMLReader::get_label(const std::string& p_token) {
	return p_token.substr(0, p_token.size() - 1);
}

bool fm::MMLReader::is_entrypoint(const std::string& p_token) const {
	auto split{ klib::str::split_string(p_token, '.') };
	if (split.size() != 2)
		return false;
	else {
		try {
			int dummy{ klib::str::parse_numeric(split[0]) };

			for (const auto& s : c::CHANNEL_LABELS)
				if (klib::str::to_lower(s) == split[1])
					return true;
			return false;
		}
		catch (...) {
			return false;
		}
	}
}

std::pair<std::size_t, std::size_t> fm::MMLReader::parse_entrypoint(const std::string& p_token) const {
	auto split{ klib::str::split_string(p_token, '.') };

	std::size_t l_chan_no{ 0 };
	for (std::size_t i{ 0 }; i < c::CHANNEL_LABELS.size(); ++i)
		if (klib::str::to_lower(c::CHANNEL_LABELS[i]) == split[1]) {
			l_chan_no = i;
			break;
		}

	return std::make_pair(
		static_cast<std::size_t>(klib::str::parse_numeric(split[0])),
		l_chan_no);
}

std::vector<byte> fm::MMLReader::get_bytes(void) const {
	std::vector<byte> result;

	for (std::size_t i{ 0 }; i < m_ptr_table.size(); ++i) {
		result.push_back(static_cast<byte>(m_ptr_table.at(i) % 256));
		result.push_back(static_cast<byte>(m_ptr_table.at(i) / 256));
	}
	for (const auto& instr : m_instructions) {
		auto bytes{ instr.get_bytes(m_opcodes) };
		result.insert(end(result), begin(bytes), end(bytes));

		if (bytes.size() != m_opcodes.at(instr.opcode_byte).size())
			throw std::runtime_error("madda");
	}

	return result;
}
