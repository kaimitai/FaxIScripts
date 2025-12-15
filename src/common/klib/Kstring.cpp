#include "Kstring.h"
#include <cctype>
#include <stdexcept>

int klib::str::parse_numeric(const std::string& token) {
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

	int value{ 0 };

	try {
		value = std::stoi(digits, &pos, base);
	}
	catch (const std::invalid_argument&) {
		throw std::runtime_error("Invalid numeric token: '" + token + "' - not a valid number or define.");
	}
	catch (const std::out_of_range&) {
		throw std::runtime_error("Numeric token out of range: '" + token + "'");
	}

	if (pos != digits.size()) {
		throw std::runtime_error("Invalid numeric token: " + token);
	}
	return value;
}

std::string klib::str::strip_comment(const std::string& line, char p_comment_char) {
	size_t pos = line.find(p_comment_char);
	if (pos != std::string::npos)
		return line.substr(0, pos);
	else
		return line;
}

bool klib::str::str_begins_with(const std::string& p_line,
	const std::string& p_start) {
	if (p_start.size() > p_line.size())
		return false;
	else
		for (std::size_t i{ 0 }; i < p_start.size(); ++i)
			if (p_start[i] != p_line[i])
				return false;

	return true;
}

std::vector<std::string> klib::str::split_whitespace(const std::string& line) {
	std::vector<std::string> tokens;
	std::size_t i = 0;
	while (i < line.size()) {
		// Skip leading whitespace, there really shouldn't be any
		while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
			++i;
		}

		if (i >= line.size()) break;

		// Handle quoted string
		if (line[i] == '"') {
			std::size_t start = i;
			++i;
			while (i < line.size() && line[i] != '"') {
				++i;
			}
			if (i < line.size()) ++i; // include closing quote
			tokens.emplace_back(line.substr(start, i - start));
		}
		else {
			// Handle regular token
			std::size_t start = i;
			while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
				++i;
			}
			tokens.emplace_back(line.substr(start, i - start));
		}
	}
	return tokens;
}

std::string klib::str::trim(const std::string& line) {
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

std::string klib::str::to_lower(const std::string& str) {
	std::string result;

	for (char c : str)
		result.push_back(std::tolower(c));

	return result;
}

std::vector<std::string> klib::str::split_string(const std::string& input, char delim) {
	std::vector<std::string> result;

	if (input.empty()) {
		return result; // 0 entries if string is empty
	}

	size_t start = 0;
	size_t pos = input.find(delim);

	while (pos != std::string::npos) {
		result.emplace_back(input.substr(start, pos - start));
		start = pos + 1;
		pos = input.find(delim, start);
	}

	// Add the last segment (or the whole string if no delimiter was found)
	result.emplace_back(input.substr(start));

	return result;
}

std::map<std::string, std::string> klib::str::extract_keyval_str(const std::string& p_str,
	char p_super_delim, char p_delim) {
	std::map<std::string, std::string> result;

	auto kvs{ split_string(trim(p_str), p_super_delim) };

	for (const auto& kvstring : kvs) {
		std::string str{ trim(kvstring) };
		auto kv{ split_string(trim(str), p_delim) };

		if (kv.empty())
			throw std::exception("Empty key-value pair");
		else if (kv.size() > 2)
			throw std::runtime_error("Invalid key-value pair for arg " + kv[0]);
		else {
			std::string arg{ trim(kv[0]) };
			std::string val{ trim(kv[1]) };

			if (arg.empty())
				throw std::runtime_error("Empty argument name");
			else if (result.find(arg) != end(result))
				throw std::runtime_error("Argument " + arg + " redefined");
			else
				result.insert(std::make_pair(arg, val));
		}
	}

	return result;
}
