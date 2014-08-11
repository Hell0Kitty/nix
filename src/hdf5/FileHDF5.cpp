// Copyright (c) 2013, German Neuroinformatics Node (G-Node)
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted under the terms of the BSD License. See
// LICENSE file in the root of the Project.

#include <fstream>
#include <vector>
#include <ctime>

#include <nix/util/util.hpp>
#include <nix/hdf5/FileHDF5.hpp>
#include <nix/hdf5/BlockHDF5.hpp>
#include <nix/hdf5/SectionHDF5.hpp>

using namespace std;

using namespace nix;
using namespace nix::hdf5;


// Format definition
#define FILE_VERSION std::vector<int>{1, 0, 0}
#define FILE_FORMAT  std::string("nix")


static unsigned int map_file_mode(FileMode mode) {
    switch (mode) {
        case FileMode::ReadWrite:
            return H5F_ACC_RDWR;

        case FileMode::ReadOnly:
            return H5F_ACC_RDONLY;

        case FileMode::Overwrite:
            return H5F_ACC_TRUNC;

        default:
            return H5F_ACC_DEFAULT;
    }

}


FileHDF5::FileHDF5(const string &name, FileMode mode)
{
    if (!fileExists(name)) {
        mode = FileMode::Overwrite;
    }

    unsigned int h5mode =  map_file_mode(mode);
    h5file = H5::H5File(name.c_str(), h5mode);

    root = Group(h5file.openGroup("/"));
    metadata = root.openGroup("metadata");
    data = root.openGroup("data");

    setCreatedAt();
    setUpdatedAt();

    if (!checkHeader()) {
        throw std::runtime_error("Invalid file header: either file format or file version not correct");
    }
}


bool FileHDF5::hasBlock(const std::string &id) const {
    return data.hasGroup(id);
}


shared_ptr<base::IBlock> FileHDF5::getBlock(const std::string &id) const {
    shared_ptr<BlockHDF5> block;

    if (hasBlock(id)) {
        Group group = data.openGroup(id, false);
        block = make_shared<BlockHDF5>(file(), group);
    }

    return block;
}


shared_ptr<base::IBlock> FileHDF5::getBlock(size_t index) const {
    string id = data.objectName(index);
    return getBlock(id);
}


shared_ptr<base::IBlock> FileHDF5::createBlock(const std::string &name, const string &type) {
    string id = util::createId("block");

    while (data.hasObject(id)) {
        id = util::createId("block");
    }

    Group group = data.openGroup(id, true);
    return make_shared<BlockHDF5>(file(), group, id, type, name);
}


bool FileHDF5::deleteBlock(const std::string &id) {
    if (data.hasGroup(id)) {
        data.removeGroup(id);
        return true;
    } else {
        return false;
    }
}


size_t FileHDF5::blockCount() const {
    return data.objectCount();
}


bool FileHDF5::hasSection(const std::string &id) const {
    return metadata.hasGroup(id);
}


shared_ptr<base::ISection> FileHDF5::getSection(const std::string &id) const {
    shared_ptr<SectionHDF5> sec;

    if (hasSection(id)) {
        Group group = metadata.openGroup(id, false);
        sec = make_shared<SectionHDF5>(file(), group);
    }

    return sec;
}


shared_ptr<base::ISection> FileHDF5::getSection(size_t index) const{
    string id = metadata.objectName(index);
    return getSection(id);
}


shared_ptr<base::ISection> FileHDF5::createSection(const string &name, const  string &type) {
    string id = util::createId("section");

    while (metadata.hasObject(id))
        id = util::createId("section");

    Group group = metadata.openGroup(id, true);
    return make_shared<SectionHDF5>(file(), group, id, type, name);
}


bool FileHDF5::deleteSection(const std::string &id) {
    // call deleteSection on sources to trigger recursive call to all sub-sections
    if (hasSection(id)) {
        // get instance of section about to get deleted
        Section section = getSection(id);
        // loop through all child sections and call deleteSection on them
        for(auto &child : section.sections()) {
            child.deleteSection(child.id());
        }
        metadata.removeAllLinks(id);
    }
    
    return hasSection(id);
}


size_t FileHDF5::sectionCount() const {
    return metadata.objectCount();
}


time_t FileHDF5::updatedAt() const {
    string t;
    root.getAttr("updated_at", t);
    return util::strToTime(t);
}


void FileHDF5::setUpdatedAt() {
    if (!root.hasAttr("updated_at")) {
        time_t t = time(NULL);
        root.setAttr("updated_at", util::timeToStr(t));
    }
}


void FileHDF5::forceUpdatedAt() {
    time_t t = time(NULL);
    root.setAttr("updated_at", util::timeToStr(t));
}


time_t FileHDF5::createdAt() const {
    string t;
    root.getAttr("created_at", t);
    return util::strToTime(t);
}


void FileHDF5::setCreatedAt() {
    if (!root.hasAttr("created_at")) {
        time_t t = time(NULL);
        root.setAttr("created_at", util::timeToStr(t));
    }
}


void FileHDF5::forceCreatedAt(time_t t) {
    root.setAttr("created_at", util::timeToStr(t));
}


vector<int> FileHDF5::version() const {
    vector<int> version;
    root.getAttr("version",version);
    return version;
}


string FileHDF5::format() const {
    string t;
    root.getAttr("format", t);
    return t;
}


string FileHDF5::location() const {
    return h5file.getFileName();
}


void FileHDF5::close() {

    if (!isOpen())
        return;

    unsigned types = H5F_OBJ_GROUP|H5F_OBJ_DATASET|H5F_OBJ_DATATYPE;

    hsize_t obj_count = h5file.getObjCount(types);
    hid_t*  objs = new hid_t[obj_count];
    h5file.getObjIDs(types, obj_count, objs);


    for (hsize_t i = 0; i < obj_count; i++) {
        hid_t   obj = objs[i];
        hsize_t ref_count = H5Iget_ref(obj);

        for (hsize_t j = 0; j < ref_count; j++) {
            H5Oclose(obj);
        }
    }

    delete[] objs;

    h5file.close();
}


bool FileHDF5::isOpen() const {
    hid_t file_id = h5file.getId();

    if (file_id == 0)
        return false;

    H5I_type_t id_type = H5Iget_type(file_id);

    if (id_type <= H5I_BADID || id_type >= H5I_NTYPES)
        return false;
    else
        return true;
}


shared_ptr<base::IFile> FileHDF5::file() const {
    return  const_pointer_cast<FileHDF5>(shared_from_this());
}


bool FileHDF5::checkHeader() const {
    bool check = true;
    vector<int> version;
    string str;
    // check format
    if (root.hasAttr("format")) {
        if (!root.getAttr("format", str) || str != FILE_FORMAT) {
            check = false;
        }
    } else {
        root.setAttr("format", FILE_FORMAT);
    }
    // check version
    if (root.hasAttr("version")) {
        if (!root.getAttr("version", version) || version != FILE_VERSION) {
            check = false;
        }
    } else {
        root.setAttr("version", FILE_VERSION);
    }
    return check;
}


bool FileHDF5::fileExists(const string &name) const {
    ifstream f(name.c_str());
    if (f) {
        f.close();
        return true;
    } else {
        return false;
    }
}


bool FileHDF5::operator==(const FileHDF5 &other) const {
    return h5file.getFileName() == other.h5file.getFileName();
}


bool FileHDF5::operator!=(const FileHDF5 &other) const {
    return h5file.getFileName() != other.h5file.getFileName();
}


FileHDF5::~FileHDF5() {
    close();
}



