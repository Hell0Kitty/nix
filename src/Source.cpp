// Copyright (c) 2013, German Neuroinformatics Node (G-Node)
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted under the terms of the BSD License. See
// LICENSE file in the root of the Project.

/**
 * @file src/Source.cpp
 * @brief Implementation of methods and functions related to the class Source.
 */

#include <pandora/Util.hpp>
#include <pandora/File.hpp>
#include <pandora/Group.hpp>
#include <pandora/Source.hpp>

using namespace std;

namespace pandora {


Source::Source(const Source &source)
  : EntityWithMetadata(source.file, source.group, source.entity_id)
{
  source_group = source.source_group;
}


Source::Source(File file, Group group, const std::string &id)
  : EntityWithMetadata(file, group, id)
{
  source_group = group.openGroup("sources");
}


Source::Source(File file, Group group, const std::string &id, time_t time)
  : EntityWithMetadata(file, group, id, time)
{
  source_group = group.openGroup("sources");
}


bool Source::hasSource(const string &id) const {
  return source_group.hasGroup(id);
}


Source Source::getSource(const string &id) const {
  return Source(file, source_group.openGroup(id, false), id);
}


Source Source::getSource(size_t index) const {
  string id = source_group.objectName(index);
  return Source(file, source_group.openGroup(id, false), id);
}


void Source::findSourcesRec(const Source &cur_source,
                            std::vector<Source> &results,
                            std::function<bool(const Source &)> predicate,
                            int level,
                            int max_depth) const
{
  size_t source_count = cur_source.sourceCount();
  std::vector<Source> my_children;
  
  for (size_t i = 0; i < source_count; i++) {
    Source s = cur_source.getSource(i);
    
    if (predicate(s)) {
      results.push_back(s);
    }
    
    my_children.push_back(s);
  }
  
  if (max_depth > 0 && level > max_depth) {
    return;
  }
  
  for (size_t i = 0; i < my_children.size(); i++) {
    findSourcesRec(my_children[i], results, predicate, level + 1, max_depth);
  }
  
}
  

std::vector<Source> Source::findSources(std::function<bool(const Source &)> predicate, bool exclude_root, int max_depth) const
{
  std::vector<Source> results;
  
  if (!exclude_root && predicate(*this)) {
    results.push_back(*this);
  }
  
  findSourcesRec(*this, results, predicate, max_depth, 1);
  return results;
}
  
size_t Source::sourceCount() const {
  return source_group.objectCount();
}


std::vector<Source> Source::sources() const {
  vector<Source> source_obj;

  source_obj = findSources([](const Source &source) {
    return true;
  }, true, 1);

  return source_obj;
}


Source Source::createSource(const string &name, const string &type) {
  string id = util::createId("source");

  while(source_group.hasObject(id)) {
    id = util::createId("source");
  }

  Source s(file, source_group.openGroup(id, true), id);
  s.name(name);
  s.type(type);

  return s;
}


ostream& operator<<(ostream &out, const Source &ent) {
  out << "Source: {name = " << ent.name();
  out << ", type = " << ent.type();
  out << ", id = " << ent.id() << "}";
  return out;
}


Source& Source::operator=(const Source &other) {
  if (*this != other) {
    this->file = other.file;
    this->group = other.group;
    this->entity_id = other.entity_id;
    this->source_group = other.source_group;
  }
  return *this;
}



Source::~Source() {
  //dtor
}

} // end namespace pandora