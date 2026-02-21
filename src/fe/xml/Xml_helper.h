#ifndef FE_XML_HELPER_H
#define FE_XML_HELPER_H

#include <map>
#include <string>
#include <optional>
#include <utility>
#include <vector>
#include "./../../common/pugixml/pugixml.hpp"
#include "./../../common/pugixml/pugiconfig.hpp"

using byte = unsigned char;

namespace fe {

	struct RegionDefinition {
		std::optional<std::size_t> m_filesize;
		std::string m_name;

		// vector of pairs {ROM offset -> vector of values matching at that offset}
		std::vector<std::pair<std::size_t, std::vector<byte>>> m_defs;
	};

	namespace xml {

		std::vector<RegionDefinition> load_region_defs(const std::string& p_xml_file,
			bool p_throw_on_file_not_exists = true);
		void load_configuration(const std::string& p_config_xml,
			const std::string& p_region_name,
			std::map<std::string, std::size_t>& p_constants,
			std::map<std::string, std::pair<std::size_t, std::size_t>>& p_pointers,
			std::map<std::string, std::vector<byte>>& p_sets,
			std::map<std::string, std::map<byte, std::string>>& p_byte_maps,
			bool p_throw_on_file_not_exists = true);

		// utility
		std::string join_bytes(const std::vector<byte>& p_bytes, bool p_hex = false);
		std::vector<byte> parse_byte_list(const std::string& input);
		std::string trim_whitespace(const std::string& p_value);
		std::vector<std::string> split_bytes(const std::string& p_values);
		std::size_t parse_numeric(const std::string& p_token);
		byte parse_numeric_byte(const std::string& p_token);
		std::string byte_to_hex(byte p_byte);

		bool region_match(const std::string& current_region, const std::string& region_list);
	}

}

#endif
