#include "Xml_helper.h"
#include "Xml_constants.h"
#include <stdexcept>

using byte = unsigned char;

void fe::xml::load_configuration(const std::string& p_config_xml,
	const std::string& p_region_name,
	std::map<std::string, std::size_t>& p_constants,
	std::map<std::string, std::pair<std::size_t, std::size_t>>& p_pointers,
	std::map<std::string, std::vector<byte>>& p_sets,
	std::map<std::string, std::map<byte, std::string>>& p_byte_maps) {
	pugi::xml_document l_doc;
	if (!l_doc.load_file(p_config_xml.c_str()))
		throw std::runtime_error("Could not load xml file " + p_config_xml);

	auto n_root{ l_doc.child(c::TAG_CONFIG_ROOT) };

	// constants
	auto n_consts{ n_root.child(c::TAG_CONSTANTS) };
	for (auto n_const{ n_consts.child(c::TAG_CONSTANT) }; n_const;
		n_const = n_const.next_sibling(c::TAG_CONSTANT)) {

		if (!n_const.attribute(c::TAG_REGION) ||
			region_match(p_region_name, n_const.attribute(c::TAG_REGION).as_string())) {

			std::string l_name{ n_const.attribute(c::ATTR_NAME).as_string() };

			if (p_constants.find(l_name) == end(p_constants))
				p_constants.insert(std::make_pair(l_name,
					parse_numeric(n_const.attribute(c::ATTR_VALUE).as_string())
				));
		}
	}

	// pointers
	auto n_ptrs{ n_root.child(c::TAG_POINTERS) };
	for (auto n_ptr{ n_ptrs.child(c::TAG_POINTER) }; n_ptr;
		n_ptr = n_ptr.next_sibling(c::TAG_POINTER)) {

		if (!n_ptr.attribute(c::TAG_REGION) ||
			region_match(p_region_name, n_ptr.attribute(c::TAG_REGION).as_string())) {

			std::string l_name{ n_ptr.attribute(c::ATTR_NAME).as_string() };

			if (p_pointers.find(l_name) == end(p_pointers))
				p_pointers.insert(std::make_pair(l_name,
					std::make_pair(
						parse_numeric(n_ptr.attribute(c::ATTR_VALUE).as_string()),
						parse_numeric(n_ptr.attribute(c::ATTR_ZERO_ADDR).as_string())
					)
				));
		}
	}

	// sets
	auto n_sets{ n_root.child(c::TAG_SETS) };
	for (auto n_set{ n_sets.child(c::TAG_SET) }; n_set;
		n_set = n_set.next_sibling(c::TAG_SET)) {

		if (!n_set.attribute(c::TAG_REGION) ||
			region_match(p_region_name, n_set.attribute(c::TAG_REGION).as_string())) {

			std::string l_name{ n_set.attribute(c::ATTR_NAME).as_string() };

			if (p_sets.find(l_name) == end(p_sets))
				p_sets.insert(std::make_pair(l_name,
					parse_byte_list(n_set.attribute(c::ATTR_VALUES).as_string())
				));
		}
	}

	// maps byte -> string (labels, char maps etc)
	auto n_bmaps{ n_root.child(c::TAG_BYTE_TO_STR_MAPS) };
	for (auto n_bmap{ n_bmaps.child(c::TAG_BYTE_TO_STR_MAP) }; n_bmap;
		n_bmap = n_bmap.next_sibling(c::TAG_BYTE_TO_STR_MAP)) {

		if (!n_bmap.attribute(c::TAG_REGION) ||
			region_match(p_region_name, n_bmap.attribute(c::TAG_REGION).as_string())) {

			std::string l_name{ n_bmap.attribute(c::ATTR_NAME).as_string() };

			if (p_sets.find(l_name) == end(p_sets)) {
				std::map<byte, std::string> l_tmp_bmap;

				for (auto n_entry{ n_bmap.child(c::TAG_ENTRY) }; n_entry;
					n_entry = n_entry.next_sibling(c::TAG_ENTRY)) {
					l_tmp_bmap.insert(std::make_pair(
						parse_numeric_byte(n_entry.attribute(c::ATTR_BYTE).as_string()),
						n_entry.attribute(c::ATTR_STRING).as_string()
					));
				}

				p_byte_maps.insert(std::make_pair(l_name, l_tmp_bmap));
			}
		}
	}
}

std::vector<fe::RegionDefinition> fe::xml::load_region_defs(const std::string& p_config_xml_file) {
	pugi::xml_document l_doc;
	if (!l_doc.load_file(p_config_xml_file.c_str()))
		throw std::runtime_error("Could not load xml file " + p_config_xml_file);

	std::vector<fe::RegionDefinition> result;

	auto n_root{ l_doc.child(c::TAG_CONFIG_ROOT) };
	auto n_regions{ n_root.child(c::TAG_REGIONS) };


	for (auto n_region{ n_regions.child(c::TAG_REGION) }; n_region;
		n_region = n_region.next_sibling(c::TAG_REGION)) {
		fe::RegionDefinition l_region;

		l_region.m_name = n_region.attribute(c::ATTR_NAME).as_string();
		if (n_region.attribute(c::ATTR_FILE_SIZE))
			l_region.m_filesize = parse_numeric(n_region.attribute(c::ATTR_FILE_SIZE).as_string());

		for (auto n_sig{ n_region.child(c::TAG_SIGNATURE) }; n_sig;
			n_sig = n_sig.next_sibling(c::TAG_SIGNATURE)) {
			l_region.m_defs.push_back(std::make_pair(
				parse_numeric(n_sig.attribute(c::ATTR_OFFSET).as_string()),
				parse_byte_list(n_sig.attribute(c::ATTR_VALUES).as_string())
			));
		}

		result.push_back(l_region);
	}

	return result;
}

std::string fe::xml::join_bytes(const std::vector<byte>& p_bytes, bool p_hex) {
	if (p_bytes.empty())
		return std::string();
	else {

		std::string result = (p_hex ? byte_to_hex(p_bytes[0]) : std::to_string(p_bytes[0]));

		for (size_t i = 1; i < p_bytes.size(); ++i)
			result += "," + (p_hex ? byte_to_hex(p_bytes[i]) : std::to_string(p_bytes[i]));

		return result;
	}
}

std::string  fe::xml::trim_whitespace(const std::string& p_value) {
	std::size_t start = 0;
	while (start < p_value.size() && std::isspace(static_cast<unsigned char>(p_value[start]))) ++start;

	std::size_t end = p_value.size();
	while (end > start && std::isspace(static_cast<unsigned char>(p_value[end - 1])))
		--end;

	return p_value.substr(start, end - start);
}

byte fe::xml::parse_numeric_byte(const std::string& p_token) {
	return static_cast<byte>(parse_numeric(p_token));
}

std::size_t fe::xml::parse_numeric(const std::string& p_token) {
	std::string value = trim_whitespace(p_token);
	if (value.empty())
		throw std::runtime_error("Empty value");

	int base = 10;
	std::string number = value;

	if (value.size() > 2 && (value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))) {
		base = 16;
		number = value.substr(2);
	}
	else if (value[0] == '$') {
		base = 16;
		number = value.substr(1);
	}

	unsigned long result = 0;
	for (char c : number) {
		if (!std::isxdigit(static_cast<unsigned char>(c))) {
			throw std::runtime_error("Invalid digit in token: " + p_token);
		}

		int digit = std::isdigit(c) ? c - '0' : std::toupper(c) - 'A' + 10;
		result = result * base + digit;
		// if (result > 0xFF) throw std::runtime_error("Value exceeds byte range: " + p_token);
	}

	return result;
}

std::vector<std::string> fe::xml::split_bytes(const std::string& p_values) {
	std::vector<std::string> l_result;
	std::size_t start = 0;

	while (start < p_values.size()) {
		std::size_t end = p_values.find(',', start);
		if (end == std::string::npos) end = p_values.size();

		l_result.push_back(trim_whitespace(p_values.substr(start, end - start)));
		start = end + 1;
	}

	return l_result;
}

std::string fe::xml::byte_to_hex(byte p_byte) {
	constexpr char hex_chars[] = "0123456789abcdef";
	std::string out = "0x";
	out += hex_chars[(p_byte >> 4) & 0xF];
	out += hex_chars[p_byte & 0xF];
	return out;
}

// parse comma - separated string (takes decimals, hex, prefixed by 0x, 0X or $, trims whitespace) into vector of bytes
std::vector<byte> fe::xml::parse_byte_list(const std::string& input) {
	std::vector<byte> result;
	for (const auto& token : split_bytes(input)) {
		result.push_back(parse_numeric_byte(token));
	}
	return result;
}

bool fe::xml::region_match(const std::string& current_region, const std::string& region_list) {
	size_t start = 0;
	size_t end;

	while ((end = region_list.find(',', start)) != std::string::npos) {
		std::string token = region_list.substr(start, end - start);
		token = trim_whitespace(token);
		if (token == current_region) return true;
		start = end + 1;
	}

	// Handle final token
	std::string token = region_list.substr(start);
	token = trim_whitespace(token);
	return token == current_region;
}
