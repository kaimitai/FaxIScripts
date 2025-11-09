# FaxIScripts - Assembler and Disassembler for scripting in Faxanadu (NES)

Welcome to the FaxIScripts code repository and release page. The code is standard C++20, and the project files were created using Microsoft Visual Studio Community 2022. You can compile the application from source, or get the latest precompiled Windows x64 build under the [repository releases](https://github.com/kaimitai/FaxIScripts/releases/).

IScripts are scripts used inside the Faxanadu (NES) game engine, and is surprisingly expressive. The aim of this assembler is to extract IScript code to a human-readable format reminiscent of assembly code. We aim to stay at the highest possible layer of abstraction without losing any extracted information.

The scripting layer contains strings, shop data and actual code. The strings are stored in a separate section, but the shop data and code live together in one combined section. The shop data gets moved to its own section in our assembly files, and any opcode referencing a shop uses its index - which is only resolved to an actual address during linking. This provides a zero-cost abstraction - no extra bytes, no layout penalties.

There are two ROM regions we can use when patching, and the users can choose between different patching modes.

Make sure to read the [documentation](./docs/doc.md) for a detailed overview of the syntax and structure of the assembly files we will be editing, as well as a list of all available opcodes.

<hr>

## Assembler Capabilities
The assembler is currently only compatible with the US version of Faxanadu. It has the following features:

* Byte-fidelity with respect to size and content will be retained when extracting and patching content; for both strings and script code
* When extracting an asm-file from ROM, the constant defines will be populated automatically and used in the code
* Extracted asm-files can show shop contents and string values in comments wherever they are used as operands
* Strict-mode; where we don't patch a ROM if we spend more bytes than the original ROM did, tightly packed in one section
* Extended ROM-mode; where we put the shop data in one section and the script code in the other free section
* Smart static linker mode; The shop data and code stream starts within the first safe region, and if we overflow the static linker redirects code to the second region while patching all required labels, jumps, pointer table entries and instruction offsets. This is done without inserting a synthetic jump-node.

<hr>

## How it works

The application can disassemble - that is extract - the scripting layer data in one assembly-file - from a Faxanadu NES rom. This file can then be modified by the user via our internal assembly language.

A command-line instruction will extract and disassemble the scripting layer data from ROM.

The command `faxiscripts extract "Faxanadu (U).nes" faxanadu.asm` will extract this data from file "Faxanadu (U).nes" and write it to file faxanadu.asm.

Another instruction will pack your data and assemble your code, and patch it back to ROM.

The command `faxiscripts build faxanadu.asm "Faxanadu (U).nes"` will patch "Faxanadu (U).nes" with the code from faxanadu.asm as long as the code was valid.

The asm-files may look a little daunting at first, but I am sure it will be very manageable for most people who have an interest in editing IScripts. The documentation is detailed and contains concrete examples you can follow.

There is little static code analysis available for the time being, but before actually patching the ROM we ensure the code is good by trying to traverse all code paths from all entry points to verify that the code the game can potentially use can actually be parsed.

<hr>

### Roadmap

This assembler was built over the course of a few nights, and hasn't been thoroughly tested yet, so there could be bugs. We prioritize fixing those.

* Incorporate the application with [Echoes of Eolis](https://github.com/kaimitai/faxedit/) in some fashion. This is my graphical data editor for Faxanadu, and currently shows hard coded labels for the scripts - labels that describe the scripts as they were used in the original game. I would like to dynamically parse scripts there and show descriptive labels or tooltips for scripts that have been edited.
* We will consider allowing users to inline actual strings and shops as operands in commands. It would improve editability but there are concerns regarding data duplication that need to be considered first. In addition, there are hard coded string index references embedded in the game code, which we need to identify and handle specially.
* We might do more static analysis to help users identify problems in their code
* I would like to add an option to sort the instruction stream by entry point (pointer table index) to the extent that it is possible. This takes some care to get right in the general case. 

<hr>

### Version History

* 2025-11-09: version 0.1
  * Initial release

<hr>

### Credits

Special thanks to the following contributors and fellow digital archaeologists:

[ChipX86/Christian Hammond](http://chipx86.com/) - For entirely mapping out the IScript language in his [Faxanadu disassembly](https://chipx86.com/faxanadu/) - and for coining the term IScript (Interaction Script) itself. This project would not have existed without his resources.
