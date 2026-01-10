#include "Parser.h"
#include "./mml_constants.h"
#include "./../fm_util.h"
#include <format>
#include <stdexcept>

fm::Parser::Parser(const std::vector<fm::Token>& toks) :
	tokens(toks), index(0)
{
}

fm::MMLSongCollection fm::Parser::parse(int p_bpm) {
	return parse_all_songs(p_bpm);
}

// --- parse all songs in the file ---
fm::MMLSongCollection fm::Parser::parse_all_songs(int p_bpm) {
	fm::MMLSongCollection coll(p_bpm);

	while (!at_end()) {
		// Look for "#song"
		if (check_song_header()) {
			coll.songs.push_back(parse_single_song(&coll.bpm));
			continue;
		}
		// Ignore anything outside songs
		advance();
	}

	return coll;
}

bool fm::Parser::check_song_header(void) const {
	return check(TokenType::Directive) && peek().text == fm::c::DIRECTIVE_SONG;
}

const fm::Token& fm::Parser::Parser::peek() const {
	return tokens[index];
}

const fm::Token& fm::Parser::peek_next() const {
	if (index + 1 < tokens.size())
		return tokens[index + 1];

	return tokens.back(); // safe fallback: EOF token
}

const fm::Token& fm::Parser::advance() {
	if (!at_end())
		index++;
	return tokens[index - 1];
}

bool fm::Parser::at_end() const {
	return peek().type == TokenType::EndOfFile;
}

bool fm::Parser::check(TokenType type) const {
	return !at_end() && peek().type == type;
}

bool fm::Parser::match(TokenType type) {
	if (check(type)) {
		advance();
		return true;
	}
	return false;
}

fm::MMLSong fm::Parser::parse_single_song(int* p_bpm) {
	fm::MMLSong song;

	// consume "#song"
	advance();

	// read song number
	if (check(TokenType::Number)) {
		song.index = advance().number;
	}

	// parse until next #song or EOF
	while (!at_end() && !check_song_header()) {

		if (check(TokenType::TempoSet)) {
			auto l_tempo{ parse_tempo_set_event() };
			auto& tmpevent{ std::get<TempoSetEvent>(l_tempo) };
			song.tempo = tmpevent.tempo;
			continue;
		}

		// song-level directive?
		if (check(TokenType::Directive)) {
			const Token& d{ peek() };

			// channel directive?
			if (is_channel_name(d.text)) {
				advance();

				song.channels.push_back(parse_channel(d.text,
					song.tempo, p_bpm));
				continue;
			}

			// unknown directive at song level -> skip
			advance();
			continue;
		}

		// ignore anything else
		advance();
	}

	return song;
}

bool fm::Parser::is_channel_name(const std::string& s) const {
	return s == c::DIRECTIVE_SQ1 ||
		s == c::DIRECTIVE_SQ2 ||
		s == c::DIRECTIVE_TRIANGLE ||
		s == c::DIRECTIVE_NOISE;
}

fm::MMLChannel fm::Parser::parse_channel(const std::string& name,
	fm::Fraction p_song_tempo, int* p_bpm) {
	fm::MMLChannel ch(p_song_tempo, p_bpm);
	ch.channel_type = string_to_channel_type(name);

	// expect '{'
	if (!check(TokenType::Brace) || peek().text != "{") {
		// error recovery: skip until next brace or next song
		while (!at_end() && !(check(TokenType::Brace) && peek().text == "{"))
			advance();
	}

	advance(); // consume '{'

	while (!at_end()) {

		// end of channel
		if (check(TokenType::Brace) && peek().text == "}") {
			advance(); // consume '}'
			break;
		}

		// --- event dispatch ---
		if (check(TokenType::Note)) {
			ch.events.push_back(parse_note_event());
			continue;
		}

		if (check(TokenType::Percussion)) {
			ch.events.push_back(parse_percussion_event());
			continue;
		}

		if (check(TokenType::Rest)) {
			ch.events.push_back(parse_rest_event());
			continue;
		}

		if (check(TokenType::Length)) {
			ch.events.push_back(parse_length_event());
			continue;
		}

		if (check(TokenType::TempoSet)) {
			ch.events.push_back(parse_tempo_set_event());
			continue;
		}

		if (check(TokenType::VolumeSet)) {
			ch.events.push_back(parse_volume_set_event());
			continue;
		}

		if (check(TokenType::OctaveShift)) {
			ch.events.push_back(parse_octave_shift_event());
			continue;
		}

		if (check(TokenType::OctaveSet)) {
			ch.events.push_back(parse_octave_set_event());
			continue;
		}

		if (check(TokenType::Tie)) {
			throw std::runtime_error("Tie '&' cannot appear without a preceding note");
		}

		if (check(TokenType::LabelDef)) {
			ch.events.push_back(parse_label_event());
			continue;
		}
		if (check(TokenType::SquareBracket)) {
			ch.events.push_back(parse_loop_event());
			continue;
		}

		if (check(TokenType::Identifier)) {
			ch.events.push_back(parse_identifier_event());
			continue;
		}

		if (check(TokenType::SongTranspose)) {
			ch.events.push_back(parse_song_transpose_event());
			continue;
		}

		if (check(TokenType::ChannelTranspose)) {
			ch.events.push_back(parse_channel_transpose_event());
			continue;
		}

		// unknown token -> skip
		advance();
	}


	return ch;
}

fm::MmlEvent fm::Parser::parse_label_event() {
	Token t = advance(); // label name
	LabelEvent ev;

	ev.name = t.text;
	return ev;
}

fm::MmlEvent fm::Parser::parse_identifier_event() {
	Token t = advance(); // identifier name

	const std::string& idname{ t.text };

	if (idname == c::OPCODE_JSR) {
		Token reflabel = advance();
		if (reflabel.type != TokenType::LabelRef)
			throw std::runtime_error("JSR not followed by label name");

		JSREvent ev;
		ev.label_name = reflabel.text;
		return ev;
	}
	else if (idname == c::OPCODE_RETURN) {
		ReturnEvent ev{};
		return ev;
	}
	else if (idname == c::OPCODE_LOOPIF) {
		LoopIfEvent ev{};
		ev.iterations = consume_number(c::OPCODE_LOOPIF, 0, 255);
		return ev;
	}
	else if (idname == c::OPCODE_NOP) {
		return NOPEvent{};
	}
	else if (idname == c::OPCODE_START) {
		return StartEvent{};
	}
	else if (idname == c::OPCODE_END) {
		return EndEvent{};
	}
	else if (idname == c::OPCODE_RESTART) {
		return RestartEvent{};
	}
	else if (idname == c::OPCODE_ENVELOPE) {
		EnvelopeEvent ev{};
		ev.value = consume_number(c::OPCODE_ENVELOPE, 0, 2);
		return ev;
	}
	else if (idname == c::OPCODE_DETUNE) {
		DetuneEvent ev{};
		ev.value = consume_number(c::OPCODE_DETUNE, 0, 255);
		return ev;
	}
	else if (idname == c::OPCODE_EFFECT) {
		EffectEvent ev{};
		ev.value = consume_number(c::OPCODE_EFFECT, 0, 255);
		return ev;
	}
	else if (idname == c::OPCODE_PULSE) {
		PulseEvent ev{};
		ev.duty_cycle = consume_number(std::format(" {} (Duty Cycle)", c::OPCODE_PULSE), 0, 3);
		ev.length_counter = consume_number(std::format(" {} (Length Counter Halt / Envelope Loop)", c::OPCODE_PULSE), 0, 1);
		ev.constant_volume = consume_number(std::format(" {} (Constant Volume)", c::OPCODE_PULSE), 0, 1);
		ev.volume_period = consume_number(std::format(" {} (Volume or Envelope Period)", c::OPCODE_PULSE), 0, 15);
		return ev;
	}

	throw std::runtime_error(std::format("Unknown identifier {}", idname));
}

int fm::Parser::consume_number(const std::string& p_label, int p_min, int p_max) {
	Token tok = advance();

	if (tok.type != TokenType::Number || tok.number < p_min || tok.number > p_max)
		throw std::runtime_error(
			std::format("Argument for {} must be a number between {} and {}",
				p_label, p_min, p_max)
		);

	return tok.number;
}

fm::MmlEvent fm::Parser::parse_tempo_set_event() {
	Token t = advance();

	TempoSetEvent ev{};
	std::string s = t.text;   // "nnn", "nnn+a/b", or "nnn.dddd"

	int num = 0;
	int den = 1;

	// ------------------------------------------------------------
	// Case 1: integer + fraction: "120+1/4"
	// ------------------------------------------------------------
	if (s.find('+') != std::string::npos) {
		auto plusPos = s.find('+');
		std::string intPart = s.substr(0, plusPos);
		std::string fracPart = s.substr(plusPos + 1); // "a/b"

		// integer part
		int i = std::stoi(intPart);

		// fraction part
		auto slashPos = fracPart.find('/');
		int a = std::stoi(fracPart.substr(0, slashPos));
		int b = std::stoi(fracPart.substr(slashPos + 1));

		// tempo = i + a/b = (i*b + a) / b
		num = i * b + a;
		den = b;
	}

	// ------------------------------------------------------------
	// Case 2: decimal: "120.75"
	// ------------------------------------------------------------
	else if (s.find('.') != std::string::npos) {
		auto dotPos = s.find('.');
		std::string intPart = s.substr(0, dotPos);
		std::string decPart = s.substr(dotPos + 1);

		int i = std::stoi(intPart);
		int d = std::stoi(decPart);
		int scale = 1;

		for (size_t k = 0; k < decPart.size(); ++k)
			scale *= 10;

		// tempo = i + d/scale = (i*scale + d) / scale
		num = i * scale + d;
		den = scale;
	}

	// ------------------------------------------------------------
	// Case 3: integer only: "120"
	// ------------------------------------------------------------
	else {
		num = std::stoi(s);
		den = 1;
	}

	// fraction is reduced in constructor
	ev.tempo = fm::Fraction(num, den);
	return ev;
}

fm::MmlEvent fm::Parser::parse_volume_set_event() {
	Token t = advance();

	VolumeSetEvent ev{};

	ev.volume = t.number; // tokenizer already parsed it
	return ev;
}

fm::MmlEvent fm::Parser::parse_octave_shift_event() {
	Token t = advance();

	OctaveShiftEvent ev{};
	ev.amount = (t.text == "<") ? -1 : +1;

	return ev;
}

fm::MmlEvent fm::Parser::parse_octave_set_event() {
	Token t = advance();

	OctaveSetEvent ev{};

	ev.octave = t.number; // tokenizer already parsed it
	return ev;
}

fm::MmlEvent fm::Parser::parse_percussion_event(void) {
	Token t = advance(); // e.g. p1 or p3*5

	PercussionEvent ev{ };

	const std::string& s = t.text;
	std::size_t i{ 0 };

	int p{ 0 }, rep{ 0 };

	while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
		p *= 10;
		p += s[i++] - '0';
	}

	++i;

	if (i < s.size()) {
		while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
			rep *= 10;
			rep += s[i++] - '0';
		}
	}
	else
		rep = 1;

	ev.perc_no = p;
	ev.repeat = rep;

	return ev;
}

fm::MmlEvent fm::Parser::parse_rest_event() {
	Token t = advance(); // e.g. "r4." or "r16.." or "r~13"
	RestEvent ev{};

	const std::string& s = t.text;
	int i = 1; // skip 'r'

	// --- RAW TICKS: r~13 ---
	if (i < (int)s.size() && s[i] == c::RAW_DELIM) {
		i++; // skip '~'

		int ticks = 0;
		while (i < (int)s.size() && std::isdigit((unsigned char)s[i])) {
			ticks = ticks * 10 + (s[i] - '0');
			i++;
		}

		ev.raw = ticks;
		return ev;
	}

	// --- MUSICAL LENGTH ---
	int start = i;
	while (i < (int)s.size() && std::isdigit((unsigned char)s[i]))
		i++;

	int length = -1;
	if (i > start)
		length = std::stoi(s.substr(start, i - start));

	// --- DOTS ---
	int dots = 0;
	while (i < (int)s.size() && s[i] == '.') {
		dots++;
		i++;
	}

	ev.dots = dots;

	if (length > 0) {
		ev.length = length;
	}

	return ev;
}

fm::MmlEvent fm::Parser::parse_length_event(void) {
	Token t = advance(); // e.g. "l4", "l16", "l4.", "l8.."
	LengthEvent ev{};

	const std::string& s = t.text;
	int i = 0;

	bool raw{ false };
	if (!t.text.empty() && s.at(i) == c::RAW_DELIM) {
		++i;
		raw = true;
	}


	// --- MUSICAL OR RAW LENGTH DIGITS ---
	int start = i;
	while (i < (int)s.size() && std::isdigit((unsigned char)s[i]))
		i++;

	if (i == start)
		throw std::runtime_error("Missing length after 'l'");

	int length = std::stoi(s.substr(start, i - start));
	if (length <= 0)
		throw std::runtime_error("Default length must be > 0");

	if (raw)
		ev.raw = length;
	else
		ev.length = length;

	// --- DOT COUNT ---
	if (!raw) {
		int dot_count = 0;
		while (i < (int)s.size() && s[i] == '.')
			dot_count++, i++;

		ev.dots = dot_count;
	}

	return ev;
}

fm::MmlEvent fm::Parser::parse_loop_event(void) {
	Token t = advance();

	if (t.text == "]")
		return parse_end_loop_or_pop_addr_event();
	else
		return parse_begin_loop_or_push_addr_event();
}

fm::MmlEvent fm::Parser::parse_begin_loop_or_push_addr_event(void) {
	std::size_t end_idx{ find_loop_end_token(index) };

	if (end_idx < tokens.size() - 1 &&
		tokens[end_idx + 1].type == TokenType::Number) {
		BeginLoopEvent ev{};
		ev.iterations = tokens[end_idx + 1].number;
		return ev;
	}
	else {
		PushAddrEvent ev{};
		return ev;
	}
}

std::size_t fm::Parser::find_loop_end_token(std::size_t p_index) const {
	int depth{ 0 };
	for (std::size_t i{ p_index };
		i < tokens.size(); ++i) {

		if (tokens[i].type == TokenType::SquareBracket &&
			tokens[i].text == "[")
			++depth;
		else if (tokens[i].type == TokenType::SquareBracket &&
			tokens[i].text == "]") {
			if (depth == 0)
				return i;
			else
				--depth;
		}

	}

	throw std::runtime_error("Matching end-loop token not found");
}

fm::MmlEvent fm::Parser::parse_end_loop_or_pop_addr_event(void) {
	if (match(TokenType::Number))
		return EndLoopEvent{};
	else
		return PopAddrEvent{};
}

fm::MmlEvent fm::Parser::parse_note_event() {
	Token t = advance();
	const std::string& s = t.text;

	NoteEvent ev{};

	// --- pitch letter ---
	ev.pitch = fm::util::note_string_to_pitch(s);

	int i = 1;

	// --- accidental ---
	if (i < s.size() && (s[i] == '+' || s[i] == '-'))
		i++;

	// --- NEW: raw tick literal ---
	if (i < s.size() && s[i] == c::RAW_DELIM) {
		i++; // skip '~'

		int ticks = 0;
		while (i < s.size() && std::isdigit((unsigned char)s[i])) {
			ticks = ticks * 10 + (s[i] - '0');
			i++;
		}

		ev.raw = ticks;
		return ev;
	}

	// --- musical duration digits ---
	int start = i;
	while (i < s.size() && std::isdigit((unsigned char)s[i]))
		i++;

	int length = -1;
	if (i > start)
		length = std::stoi(s.substr(start, i - start));

	// --- dots ---
	int dots = 0;
	while (i < s.size() && s[i] == '.') {
		dots++;
		i++;
	}

	ev.dots = dots;

	if (length > 0) {
		ev.length = length;
	}

	// parser does NOT fold ties; VM will handle it
	if (match(TokenType::Tie))
		ev.tie_to_next = true;

	return ev;
}

fm::MmlEvent fm::Parser::parse_song_transpose_event(void) {
	Token t = advance();
	SongTransposeEvent ev{};
	ev.semitones = t.number;
	return ev;
}

fm::MmlEvent fm::Parser::parse_channel_transpose_event(void) {
	Token t = advance();
	ChannelTransposeEvent ev{};
	ev.semitones = t.number;
	return ev;
}

fm::ChannelType fm::Parser::string_to_channel_type(const std::string& str) const {
	if (str == c::CHANNEL_NAMES[0])
		return fm::ChannelType::sq1;
	else if (str == c::CHANNEL_NAMES[1])
		return fm::ChannelType::sq2;
	else if (str == c::CHANNEL_NAMES[2])
		return fm::ChannelType::tri;
	else if (str == c::CHANNEL_NAMES[3])
		return fm::ChannelType::noise;
	else
		throw std::runtime_error(std::format("Invalid channel name directive: {}", str));
}
