#ifndef KLIB_KSTRING_H
#define KLIB_KSTRING_H

#include <map>
#include <string>
#include <vector>

namespace klib {

	namespace str {

		std::string strip_comment(const std::string& line, char p_comment_char = ';');
		bool str_begins_with(const std::string& p_line, const std::string& p_start);
		std::vector<std::string> split_whitespace(const std::string& p_line);
		std::vector<std::string> split_string(const std::string& input, char delim);

		std::map<std::string, std::string> extract_keyval_str(const std::string& p_str,
			char p_super_delim = ',', char p_delim = '=');

		std::string trim(const std::string& str);
		std::string to_lower(const std::string& str);

		int parse_numeric(const std::string& token);
	}

}

#endif
