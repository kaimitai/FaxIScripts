#include "AsmReader.h"
#include "fi_constants.h"
#include "Opcode.h"
#include "./../common/klib/Kfile.h"
#include <algorithm>
#include <format>
#include <cctype>
#include <stdexcept>
#include <set>
#include <string>

void fi::AsmReader::read_asm_file(const std::string& p_filename) {
	auto l_lines{ klib::file::read_file_as_strings(p_filename) };
	fi::SectionType currentSection{ fi::SectionType::Defines };

	for (auto& line : l_lines) {
		line = trim(strip_comment(line));

		if (line == c::SECTION_DEFINES) {
			currentSection = fi::SectionType::Defines;
		}
		else if (line == c::SECTION_STRINGS) {
			currentSection = fi::SectionType::Strings;
		}
		else if (line == c::SECTION_SHOPS) {
			currentSection = fi::SectionType::Shops;
		}
		else if (line == c::SECTION_ISCRIPT) {
			currentSection = fi::SectionType::IScript;
		}
		else if (!line.empty())
			m_sections[currentSection].push_back(line);
	}

	parse_section_strings();
	parse_section_defines();
	parse_section_shops();
	parse_section_iscript();
}

void fi::AsmReader::parse_section_strings(void) {
	const auto& lines = m_sections.at(SectionType::Strings);
	std::map<std::size_t, std::string> temp;
	std::size_t max_index{ 0 };

	for (const auto& line : lines) {

		size_t colon_pos = line.find(':');
		if (colon_pos == std::string::npos || colon_pos < 2 || line[0] != '$') {
			throw std::runtime_error("Malformed string line: " + line);
		}

		std::size_t index{
			static_cast<std::size_t>(parse_numeric(line.substr(0, colon_pos)))
		};

		size_t quote_start = line.find('"', colon_pos);
		size_t quote_end = line.rfind('"');

		if (quote_start == std::string::npos || quote_end == quote_start) {
			throw std::runtime_error("Malformed string content: " + line);
		}

		std::string value = line.substr(quote_start + 1, quote_end - quote_start - 1);
		if (value.empty()) {
			throw std::runtime_error(std::format("Error: string index ${:02x} is empty. All strings must contain data.", index));
		}

		temp[index] = value;
		max_index = std::max(max_index, index);
	}

	for (std::size_t i{ 1 }; i <= max_index; ++i) {
		if (!temp.contains(i)) {
			throw std::runtime_error(std::format("Missing string index ${:02x}. All strings must be present and non-empty.", i));
		}
		m_strings.push_back(temp[i]);
	}
}

void fi::AsmReader::parse_section_defines() {
	const auto& lines = m_sections.at(SectionType::Defines);

	for (const auto& line : lines) {
		// Must start with "define "
		if (!line.starts_with("define ")) {
			throw std::runtime_error("Malformed define line: " + line);
		}

		// Strip "define " prefix
		std::string rest = line.substr(7);
		size_t space_pos = rest.find(' ');
		if (space_pos == std::string::npos) {
			throw std::runtime_error("Malformed define line (missing value): " + line);
		}

		std::string key = trim(rest.substr(0, space_pos));
		std::string value_token = trim(rest.substr(space_pos + 1));

		if (key.empty() || value_token.empty()) {
			throw std::runtime_error("Empty key or value in define: " + line);
		}
		if (m_defines.contains(key)) {
			throw std::runtime_error("Duplicate define key: " + key);
		}

		std::size_t value = static_cast<std::size_t>(parse_numeric(value_token));
		m_defines[key] = value;
	}

}

void fi::AsmReader::parse_section_shops() {
	const auto& lines = m_sections.at(SectionType::Shops);
	std::map<std::size_t, Shop> result;

	for (const auto& line : lines) {
		size_t colon_pos = line.find(':');
		if (colon_pos == std::string::npos || colon_pos < 2) {
			throw std::runtime_error("Malformed shop line: " + line);
		}

		std::size_t index = static_cast<std::size_t>(parse_numeric(line.substr(0, colon_pos)));
		std::string rest = trim(line.substr(colon_pos + 1));

		Shop shop;
		size_t pos = 0;
		while ((pos = rest.find('(', pos)) != std::string::npos) {
			size_t end = rest.find(')', pos);
			if (end == std::string::npos) {
				throw std::runtime_error("Unclosed item group in shop line: " + line);
			}

			std::string group = trim(rest.substr(pos + 1, end - pos - 1));
			size_t space = group.find(' ');
			if (space == std::string::npos) {
				throw std::runtime_error("Malformed item-price pair: " + group);
			}

			std::string item_token = group.substr(0, space);
			std::string price_token = group.substr(space + 1);

			byte item{
				static_cast<byte>(resolve_token(item_token))
			};

			uint16_t price{
				static_cast<uint16_t>(resolve_token(price_token))
			};

			shop.m_entries.push_back(fi::ShopEntry(item, price));
			pos = end + 1;
		}

		if (shop.m_entries.empty()) {
			throw std::runtime_error(std::format("Shop ${:02x} has no items.", index));
		}
		if (result.contains(index)) {
			throw std::runtime_error(std::format("Duplicate shop index ${:02x}.", index));
		}

		result[index] = std::move(shop);
	}

	m_shops = std::move(result);
}


std::string fi::AsmReader::strip_comment(const std::string& line) const {
	size_t pos = line.find(';');
	if (pos != std::string::npos)
		return line.substr(0, pos);
	else
		return line;
}

std::string fi::AsmReader::trim(const std::string& line) const {
	size_t start = 0;
	while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
		++start;
	}

	size_t end = line.size();
	while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1]))) {
		--end;
	}

	return line.substr(start, end - start);
}

int fi::AsmReader::parse_numeric(const std::string& token) const {
	if (token.empty()) throw std::runtime_error("Empty index token");

	int base = 10;
	std::string digits;

	if (token[0] == '$') {
		base = 16;
		digits = token.substr(1);
	}
	else if (token.starts_with("0x") || token.starts_with("0X")) {
		base = 16;
		digits = token.substr(2);
	}
	else {
		digits = token;
	}

	size_t pos;
	int value = std::stoi(digits, &pos, base);
	if (pos != digits.size()) {
		throw std::runtime_error("Invalid numeric token: " + token);
	}
	return value;
}

std::size_t fi::AsmReader::resolve_token(const std::string& token) const {
	auto it = m_defines.find(token);
	if (it != m_defines.end()) {
		return it->second;
	}
	return static_cast<std::size_t>(parse_numeric(token));
}

// the fun part, where we parse the asm instructions and calculate all offsets
// before we convert to bytes in the final pass and take home the w
void fi::AsmReader::parse_section_iscript(void) {
	// start by calculating the shop offsets and code offset
	// we are not emitting any bytes in the first pass
	// we will do this with 0-relative offsets in the first pass
	// and recalculate later

	// make an opcode mnemonic reverse lookup
	std::map<std::string, byte> mnemonics;
	for (const auto& opc : fi::opcodes) {
		mnemonics.insert(std::make_pair(
			opc.second.name, opc.first
		));
	}

	std::map<std::size_t, std::size_t> l_shop_ptrs;

	std::size_t offset{ 0 };
	for (const auto& kv : m_shops) {
		l_shop_ptrs.insert(std::make_pair(kv.first, offset));
		offset += kv.second.byte_size();
	}

	std::map<std::string, std::size_t> label_offsets;
	std::map<std::string, std::set<std::size_t>> jump_labels;
	std::map<std::size_t, std::size_t> ep_offsets;
	// TODO: could keep offset inside the instr-struct. consider
	std::map<std::size_t, std::size_t> offset_to_instruction;

	m_instructions.clear();

	// and so it begins...
	for (std::string line : m_sections.at(fi::SectionType::IScript)) {

		// if label - extract and store
		if (contains_label(line)) {
			std::string label{ extract_label(line) };
			auto iter = label_offsets.find(label);
			if (iter == end(label_offsets))
				label_offsets.insert(std::make_pair(label, offset));
			else
				throw std::runtime_error("Multiple definitions for " + label);
		}
		// if entrypoint - extract entrypoint no and update ptr table map
		else if (contains_entrypoint(line)) {
			auto entry_no{ extract_entrypoint(line) };
			if (ep_offsets.find(entry_no) == end(ep_offsets))
				ep_offsets[entry_no] = offset;
			else
				throw std::runtime_error(std::format("Multiple definitions for entrypoint {}", entry_no));
		}
		// if textbox - grab the byte, make a pseudo-instruction and advance
		else if (contains_textbox(line)) {
			byte textbox_no{ extract_textbox(line) };

			offset_to_instruction[offset++] = m_instructions.size();
			m_instructions.push_back(fi::Instruction(
				fi::Instruction_type::Directive,
				textbox_no,
				1
			));
		}
		// else it must be an opcode
		else {
			// can't be empty as long as we did our pre-processing correctly
			auto tokens{ split_whitespace(line) };

			const std::string& mnemo{ tokens[0] };
			if (!mnemonics.contains(mnemo)) {
				throw std::runtime_error("Unknown opcode: " + mnemo);
			}

			byte opcode_byte = mnemonics[mnemo];
			std::optional<uint16_t> arg;
			std::optional<uint16_t> target_address;

			const fi::Opcode& op = fi::opcodes.at(opcode_byte);

			// let's validate the params first
			std::size_t expected_token_count{ 1 }; // the opcode itself
			if (op.arg_type != fi::ArgType::None)
				expected_token_count += 1;
			if (op.flow == fi::Flow::Jump ||
				op.flow == fi::Flow::Read)
				expected_token_count += 1;

			if (expected_token_count != tokens.size())
				throw std::runtime_error(
					std::format("Opcode '{}' expects {} arguments, got {} (@line: \"{}\")",
						mnemo, expected_token_count - 1, tokens.size() - 1, line)
				);

			// based on the template we calculate everything
			// and generate the instruction. we already know shop
			// offsets but not necessarily labels
			std::size_t current_token{ 1 };

			if (op.arg_type != fi::ArgType::None)
				arg = static_cast<uint16_t>(resolve_token(tokens.at(current_token++)));

			if (op.flow == fi::Flow::Read)
				target_address = static_cast<uint16_t>(
					l_shop_ptrs.at(resolve_token(tokens.at(current_token++)))
					);
			else if (op.flow == fi::Flow::Jump) {
				// labels! may or may not be known at this time
				// might as well defer everything to the second pass
				auto label{ tokens.at(current_token++) };
				jump_labels[label].insert(m_instructions.size());
			}

			// finally emit the instruction
			offset_to_instruction[offset] = m_instructions.size();
			offset += op.size();
			m_instructions.push_back(fi::Instruction(
				fi::Instruction_type::OpCode,
				opcode_byte,
				op.size() + 1, // TODO: Make explicit
				arg,
				target_address
			));
		}
	}

	// intermediate pass - patch all previously unresolved labels
	for (const auto& kv : jump_labels) {
		auto iter{ label_offsets.find(kv.first) };
		if (iter == end(label_offsets))
			throw std::runtime_error("Unresolved label: " + kv.first);
		else {
			for (std::size_t instr_no : kv.second)
				m_instructions[instr_no].jump_target = iter->second;
		}
	}

}

bool fi::AsmReader::contains_label(const std::string& p_line) const {
	std::size_t colonPos = p_line.find(':');
	if (colonPos == std::string::npos) return false;

	std::string label = p_line.substr(0, colonPos);
	return !label.empty() && label.find(' ') == std::string::npos;
}

std::string fi::AsmReader::extract_label(const std::string& p_line) const {
	std::size_t colonPos = p_line.find(':');
	return p_line.substr(0, colonPos);
}

bool fi::AsmReader::contains_entrypoint(const std::string& p_line) const {
	return str_begins_with(p_line, c::DIRECTIVE_ENTRYPOINT);
}

std::size_t fi::AsmReader::extract_entrypoint(const std::string& p_line) const {
	std::size_t spacePos = p_line.find(' ');
	if (spacePos == std::string::npos || spacePos + 1 >= p_line.size()) {
		throw std::runtime_error("Missing entrypoint index: " + p_line);
	}

	std::string indexStr = p_line.substr(spacePos + 1);
	indexStr = trim(indexStr);
	return resolve_token(indexStr);
}

bool fi::AsmReader::contains_textbox(const std::string& p_line) const {
	return str_begins_with(p_line, c::PSEUDO_OPCODE_TEXTBOX);
}

byte fi::AsmReader::extract_textbox(const std::string& p_line) const {
	std::size_t spacePos = p_line.find(' ');
	if (spacePos == std::string::npos || spacePos + 1 >= p_line.size()) {
		throw std::runtime_error("Missing textbox value: " + p_line);
	}

	std::string indexStr = p_line.substr(spacePos + 1);
	indexStr = trim(indexStr);
	return static_cast<byte>(resolve_token(indexStr));
}

bool fi::AsmReader::str_begins_with(const std::string& p_line,
	const std::string& p_start) const {
	if (p_start.size() > p_line.size())
		return false;
	else
		for (std::size_t i{ 0 }; i < p_start.size(); ++i)
			if (p_start[i] != p_line[i])
				return false;

	return true;
}

std::vector<std::string> fi::AsmReader::split_whitespace(const std::string& line) const {
	std::vector<std::string> tokens;
	std::size_t i = 0;
	while (i < line.size()) {
		// Skip leading whitespace
		while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
			++i;
		}

		// Find start of token
		std::size_t start = i;
		while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
			++i;
		}

		// Add token if found
		if (start < i) {
			tokens.emplace_back(line.substr(start, i - start));
		}
	}
	return tokens;
}

std::vector<byte> fi::AsmReader::get_bytes(void) const {
	std::vector<byte> result;

	for (const auto& kv : m_shops) {
		auto shopbytes{ kv.second.to_bytes() };
		result.insert(end(result), begin(shopbytes), end(shopbytes));
	}

	for (const auto& instr : m_instructions) {
		auto instrbytes{ instr.get_bytes() };
		result.insert(end(result), begin(instrbytes), end(instrbytes));
	}

	klib::file::write_bytes_to_file(result, "c:/temp/iscript.bin");

	return result;
}
