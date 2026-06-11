cmake_minimum_required(VERSION 3.20)

project(FaxIScripts)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp

    # common
    src/common/klib/Kfile.cpp
    src/common/klib/Kstring.cpp

    src/common/midifile/Binasc.cpp
    src/common/midifile/MidiEvent.cpp
    src/common/midifile/MidiEventList.cpp
    src/common/midifile/MidiFile.cpp
    src/common/midifile/MidiMessage.cpp
    src/common/midifile/Options.cpp

    src/common/pugixml/pugixml.cpp

    # fb
    src/fb/BScriptLoader.cpp
    src/fb/BScriptOpcode.cpp
    src/fb/BScriptReader.cpp
    src/fb/BScriptWriter.cpp

    # fe
    src/fe/Config.cpp
    src/fe/xml/Xml_helper.cpp

    # fi
    src/fi/AsmReader.cpp
    src/fi/AsmReaderStaticLinker.cpp
    src/fi/AsmWriter.cpp
    src/fi/FaxString.cpp
    src/fi/IScriptLoader.cpp
    src/fi/Opcode.cpp
    src/fi/Shop.cpp
    src/fi/cli/Cli.cpp

    # fm
    src/fm/MMLReader.cpp
    src/fm/MMLWriter.cpp
    src/fm/MScriptLoader.cpp
    src/fm/MusicOpcode.cpp
    src/fm/fm_util.cpp

    src/fm/song/Fraction.cpp
    src/fm/song/LilyPond.cpp
    src/fm/song/MMLChannel.cpp
    src/fm/song/MMLSong.cpp
    src/fm/song/MMLSongCollection.cpp
    src/fm/song/Parser.cpp
    src/fm/song/Tokenizer.cpp

    # fv
    src/fv/MiscWriter.cpp

    # mantra
    src/mantra/Mantra.cpp
    src/mantra/MantraCli.cpp
    src/mantra/mantra_math.cpp
)

add_executable(faxiscripts ${SOURCES})

target_include_directories(faxiscripts PRIVATE
    src
    src/common
    src/common/klib
    src/common/midifile
    src/common/pugixml

    src/fb

    src/fe
    src/fe/xml

    src/fi
    src/fi/cli

    src/fm
    src/fm/song

    src/fv

    src/mantra
)
