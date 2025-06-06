/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/archive.h"
#include "common/debug.h"
#include "common/file.h"
#include "common/macresman.h"
#include "common/substream.h"
#include "common/textconsole.h"
#include "common/config-manager.h"

#include "groovie/resource.h"
#include "groovie/groovie.h"

namespace Groovie {

// ResMan

Common::SeekableReadStream *ResMan::open(uint32 fileRef) {
	// Get the information about the resource
	ResInfo resInfo;
	if (!getResInfo(fileRef, resInfo)) {
		return nullptr;
	}

	debugC(1, kDebugResource, "Groovie::Resource: Opening resource %d", fileRef);
	return open(resInfo);
}

Common::SeekableReadStream *ResMan::open(const ResInfo &resInfo) {
	// Do we know the name of the required GJD?
	if (resInfo.gjd >= _gjds.size()) {
		// Is this a raw vob file? (TLC DVD)
		if (resInfo.gjd >= 1000) {
			Common::Path filename = Common::Path(Common::String::format("VOB%u.VOB", resInfo.offset));
			if (!Common::File::exists(filename)) {
				return nullptr;
			}
			Common::File *vobFile = new Common::File();
			vobFile->open(filename);
			return vobFile;
		}

		error("Groovie::Resource: Unknown GJD %d", resInfo.gjd);
		return nullptr;
	}

	debugC(1, kDebugResource, "Groovie::Resource: Opening resource (%s, %d, %d, %d)", _gjds[resInfo.gjd].toString().c_str(), resInfo.offset, resInfo.size, resInfo.disks);

	// Does it exist?
	if (!Common::File::exists(_gjds[resInfo.gjd])) {
		error("Groovie::Resource: %s not found (resInfo.disks: %d)", _gjds[resInfo.gjd].toString().c_str(), resInfo.disks);
		return nullptr;
	}

	// Open the pack file
	Common::File *gjdFile = new Common::File();
	if (!gjdFile->open(_gjds[resInfo.gjd])) {
		delete gjdFile;
		error("Groovie::Resource: Couldn't open %s", _gjds[resInfo.gjd].toString().c_str());
		return nullptr;
	}

	// Save the used gjd file (except xmi and gamwav)
	if (resInfo.gjd < 19) {
		_lastGjd = resInfo.gjd;
	}

	// Returning the resource substream
	Common::SeekableSubReadStream *file = new Common::SeekableSubReadStream(gjdFile, resInfo.offset, resInfo.offset + resInfo.size, DisposeAfterUse::YES);
	if (ConfMan.getBool("dump_resources")) {
		dumpResource(file, Common::Path(resInfo.filename), false);
	}
	return file;
}

Common::String ResMan::getGjdName(const ResInfo &resInfo) {
	if (resInfo.gjd >= _gjds.size()) {
		if (resInfo.gjd >= 1000) {
			return Common::String::format("VOB%u.VOB", resInfo.offset);
		}

		error("Groovie::Resource: Unknown GJD %d", resInfo.gjd);
	}

	return _gjds[resInfo.gjd].baseName();
}

void ResMan::dumpResource(const Common::String &fileName) {
	uint32 fileRef = getRef(fileName);
	dumpResource(fileRef, Common::Path(fileName));
}

void ResMan::dumpResource(uint32 fileRef, const Common::Path &fileName) {
	Common::SeekableReadStream *inFile = open(fileRef);
	dumpResource(inFile, fileName);
}

void ResMan::dumpResource(Common::SeekableReadStream *inFile, const Common::Path &fileName, bool dispose) {
	Common::DumpFile outFile;
	outFile.open(fileName);

	int64 totalSize = inFile->size();
	byte *data = new byte[totalSize];
	inFile->read(data, totalSize);

	outFile.write(data, totalSize);
	outFile.flush();

	delete[] data;

	if (dispose)
		delete inFile;
	else
		inFile->seek(0);

	outFile.close();
}

// ResMan_t7g

static const char t7g_gjds[][0x15] = {"at", "b", "ch", "d", "dr", "fh", "ga", "hdisk", "htbd", "intro", "jhek", "k", "la", "li", "mb", "mc", "mu", "n", "p", "xmi", "gamwav"};

ResMan_t7g::ResMan_t7g(Common::MacResManager *macResFork) : _macResFork(macResFork) {
	for (int i = 0; i < 0x15; i++) {
		// Prepare the filename
		Common::String filename = t7g_gjds[i];
		filename += ".gjd";

		// Handle the special case of Mac's hdisk.gjd
		if (_macResFork && i == 7)
			filename = "T7GData";

		// Append it to the list of GJD files
		_gjds.push_back(Common::Path(filename));
	}
}

uint32 ResMan_t7g::getRef(Common::String name) {
	// Get the name of the RL file
	Common::String rlFileName(t7g_gjds[_lastGjd]);
	rlFileName += ".rl";

	Common::SeekableReadStream *rlFile = nullptr;

	if (_macResFork) {
		// Open the RL file from the resource fork
		rlFile = _macResFork->getResource(rlFileName);
	} else {
		// Open the RL file
		rlFile = SearchMan.createReadStreamForMember(Common::Path(rlFileName));
	}

	if (!rlFile)
		error("Groovie::Resource: Couldn't open %s", rlFileName.c_str());

	uint32 resNum;
	bool found = false;
	for (resNum = 0; !found && !rlFile->err() && !rlFile->eos(); resNum++) {
		// Read the resource name
		char readname[12];
		rlFile->read(readname, 12);

		// Test whether it's the resource we're searching
		Common::String resname(readname, 12);
		if (resname.hasPrefix(name.c_str())) {
			debugC(2, kDebugResource, "Groovie::Resource: Resource %12s matches %s", readname, name.c_str());
			found = true;
		}

		// Skip the rest of resource information
		rlFile->read(readname, 8);
	}

	// Close the RL file
	delete rlFile;

	// Verify we really found the resource
	if (!found) {
		error("Groovie::Resource: Couldn't find resource %s in %s", name.c_str(), rlFileName.c_str());
		return (uint32)-1;
	}

	return (_lastGjd << 10) | (resNum - 1);
}

bool ResMan_t7g::getResInfo(uint32 fileRef, ResInfo &resInfo) {
	// Calculate the GJD and the resource number
	resInfo.gjd = fileRef >> 10;
	uint16 resNum = fileRef & 0x3FF;

	// Get the name of the RL file
	Common::String rlFileName(t7g_gjds[resInfo.gjd]);
	rlFileName += ".rl";

	Common::SeekableReadStream *rlFile = nullptr;

	if (_macResFork) {
		// Open the RL file from the resource fork
		rlFile = _macResFork->getResource(rlFileName);
	} else {
		// Open the RL file
		rlFile = SearchMan.createReadStreamForMember(Common::Path(rlFileName));
	}

	if (!rlFile)
		error("Groovie::Resource: Couldn't open %s", rlFileName.c_str());

	// Seek to the position of the desired resource
	rlFile->seek(resNum * 20);
	if (rlFile->eos()) {
		delete rlFile;
		error("Groovie::Resource: Invalid resource number: 0x%04X (%s)", resNum, rlFileName.c_str());
	}

	// Read the resource name
	char resname[13];
	rlFile->read(resname, 12);
	resname[12] = 0;
	debugC(2, kDebugResource, "Groovie::Resource: Resource name: %12s", resname);
	resInfo.filename = resname;

	// Read the resource information
	resInfo.offset = rlFile->readUint32LE();
	resInfo.size = rlFile->readUint32LE();

	// Close the resource RL file
	delete rlFile;

	return true;
}


// ResMan_v2

ResMan_v2::ResMan_v2() {
	Common::File indexfile;

	// Open the GJD index file
	if (!indexfile.open("gjd.gjd")) {
		error("Groovie::Resource: Couldn't open gjd.gjd");
		return;
	}

	Common::String line = indexfile.readLine();
	while (!indexfile.eos() && !line.empty()) {
		// Get the name before the space
		Common::String filename;
		for (const char *cur = line.c_str(); *cur != ' '; cur++) {
			filename += *cur;
		}

		// Append it to the list of GJD files
		if (!filename.empty()) {
			_gjds.push_back(Common::Path(filename));
		}

		// Read the next line
		line = indexfile.readLine();
	}

	// Close the GJD index file
	indexfile.close();
}

uint32 ResMan_v2::getRef(Common::String name) {
	// Open the RL file
	Common::File rlFile;
	if (!rlFile.open("dir.rl")) {
		error("Groovie::Resource: Couldn't open dir.rl");
		return (uint32)-1;
	}

	// resources are always in lowercase
	name.toLowercase();
	uint32 resNum;
	bool found = false;
	for (resNum = 0; !found && !rlFile.err() && !rlFile.eos(); resNum++) {
		// Seek past metadata
		rlFile.seek(14, SEEK_CUR);

		// Read the resource name
		char readname[18];
		rlFile.read(readname, 18);

		// Test whether it's the resource we're searching
		Common::String resname(readname, 18);
		if (resname.hasPrefixIgnoreCase(name.c_str())) {
			debugC(2, kDebugResource, "Groovie::Resource: Resource %18s matches %s", readname, name.c_str());
			found = true;
			break;
		}
	}

	// Close the RL file
	rlFile.close();

	// Verify we really found the resource
	if (!found) {
		warning("Groovie::Resource: Couldn't find resource %s", name.c_str());
		return (uint32)-1;
	}

	return resNum;
}

bool ResMan_v2::getResInfo(uint32 fileRef, ResInfo &resInfo) {
	// Open the RL file
	Common::File rlFile;
	if (!rlFile.open("dir.rl")) {
		error("Groovie::Resource: Couldn't open dir.rl");
		return false;
	}

	// Seek to the position of the desired resource
	rlFile.seek(fileRef * 32);
	if (rlFile.eos()) {
		rlFile.close();
		error("Groovie::Resource: Invalid resource number: 0x%04X", fileRef);
		return false;
	}

	// Read the resource information
	resInfo.disks = rlFile.readUint32LE(); // Seems to be a bitfield indicating on which disk(s) the file can be found
	resInfo.offset = rlFile.readUint32LE();
	resInfo.size = rlFile.readUint32LE();
	resInfo.gjd = rlFile.readUint16LE();

	// Read the resource name
	char resname[19];
	resname[18] = 0;
	rlFile.read(resname, 18);
	debugC(2, kDebugResource, "Groovie::Resource: Resource name: %18s", resname);
	resInfo.filename = resname;

	// 6 padding bytes? (it looks like they're always 0)

	// Close the resource RL file
	rlFile.close();

	return true;
}

} // End of Groovie namespace
