#ifndef FM_PARSER_H
#define FM_PARSER_H

#include <vector>
#include <string>
#include "Tokenizer.h"
#include "MMLSong.h"
#include "MMLChannel.h"
#include "MmlEvent.h"   // variant + event structs
#include "MMLSongCollection.h"

namespace fm {

	class Parser {
		const std::vector<Token>& tokens;
		std::size_t index = 0;

		// --- token navigation ---
		const Token& peek() const;
		const Token& peek_next() const;
		const Token& advance();
		bool at_end() const;

		bool check(TokenType type) const;
		bool match(TokenType type);

		// --- helpers ---
		bool check_song_header() const;
		bool is_channel_name(const std::string& s) const;

		// --- parsing ---
		fm::MMLSongCollection parse_all_songs(int p_bpm);
		fm::MMLSong parse_single_song(int* p_bpm);
		fm::MMLChannel parse_channel(const std::string& name,
			int p_song_tempo, int* p_bpm);

		// --- event parsers ---
		MmlEvent parse_note_event(void);
		MmlEvent parse_rest_event();
		MmlEvent parse_length_event(void);
		MmlEvent parse_tempo_set_event(void);
		MmlEvent parse_volume_set_event(void);
		MmlEvent parse_octave_shift_event(void);
		MmlEvent parse_octave_set_event(void);
		MmlEvent parse_label_event(void);
		MmlEvent parse_identifier_event(void);
		MmlEvent parse_song_transpose_event(void);
		MmlEvent parse_channel_transpose_event(void);

		// infinite or finite loops
		MmlEvent parse_loop_event(void);
		MmlEvent parse_end_loop_or_pop_addr_event(void);
		MmlEvent parse_begin_loop_or_push_addr_event(void);
		std::size_t find_loop_end_token(std::size_t index) const;

		int consume_number(const std::string& p_label, int p_min, int p_max);

	public:
		Parser(const std::vector<Token>& toks);
		fm::MMLSongCollection parse(int bpm);
	};

}

#endif
