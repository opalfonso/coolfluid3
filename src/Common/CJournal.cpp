// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include <boost/date_time/posix_time/posix_time.hpp>
#include "Common/CBuilder.hpp"
#include "Common/Core.hpp"
#include "Common/Log.hpp"
#include "Common/LibCommon.hpp"

#include "Common/CJournal.hpp"


////////////////////////////////////////////////////////////////////////////////

namespace CF {
namespace Common {

///////////////////////////////////////////////////////////////////////////////

Common::ComponentBuilder < CJournal, Component, LibCommon > CJournal_Builder;

////////////////////////////////////////////////////////////////////////////////

CJournal::CJournal (const std::string & name)
  : Component(name),
    m_xmldoc(XmlOps::create_doc())
{
  regist_signal("list_journal", "Lists all journal entries.", "List journal")->
      connect( boost::bind( &CJournal::list_journal, this, _1) );
  regist_signal("load_journal", "Loads the journal entries from file.", "Load journal")->
      connect( boost::bind( &CJournal::load_journal, this, _1) );
  regist_signal("dump_journal", "Dumps all journal entries.", "Dump journal")->
      connect( boost::bind( &CJournal::dump_journal, this, _1) );

  signal("list_journal").is_hidden = true;

  /// @todo change this when the XML layer arrives
  XmlNode & doc_node = *XmlOps::goto_doc_node(*m_xmldoc.get());

  m_info_node = XmlOps::add_node_to(doc_node, XmlParams::tag_node_map());
  m_signals_map = XmlOps::add_node_to(doc_node, XmlParams::tag_node_map());

  XmlOps::add_attribute_to(*m_info_node, XmlParams::tag_attr_key(), "journalInfo");

  XmlParams::add_value_to(*m_info_node, "hostname", Core::instance().network_info().hostname());
  XmlParams::add_value_to(*m_info_node, "port", (Uint)Core::instance().network_info().port());
}

////////////////////////////////////////////////////////////////////////////////

CJournal::~CJournal()
{

}

////////////////////////////////////////////////////////////////////////////////

CJournal::Ptr CJournal::create_from_file ( const std::string & name,
                                           const boost::filesystem::path & file_path )
{
  CJournal::Ptr journal( new CJournal(name) );

  journal->load_journal_file(file_path);

  return journal;
}

////////////////////////////////////////////////////////////////////////////////

void CJournal::load_journal_file ( const boost::filesystem::path & file_path )
{

}

////////////////////////////////////////////////////////////////////////////////

void CJournal::dump_journal_to ( const boost::filesystem::path & file_path ) const
{

}

////////////////////////////////////////////////////////////////////////////////

void CJournal::add_signal ( const XmlNode & signal_node )
{
  XmlNode * copy = copy_node(signal_node, *m_signals_map);

  boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
  XmlOps::add_attribute_to(*copy, "time", boost::posix_time::to_simple_string(now));
}

////////////////////////////////////////////////////////////////////////////////

void CJournal::list_journal ( XmlNode & node )
{
  XmlNode & reply_node = *XmlOps::add_reply_frame(node);

  XmlOps::add_attribute_to( reply_node, "sender", full_path().string_without_scheme() );

  copy_node(*m_signals_map, reply_node);
}

////////////////////////////////////////////////////////////////////////////////

void CJournal::load_journal ( XmlNode & node )
{
  throw NotImplemented(FromHere(), "CJournal::load_journal()");
}

////////////////////////////////////////////////////////////////////////////////

void CJournal::dump_journal ( XmlNode & node )
{
  URI file_path("./server-journal.xml", URI::Scheme::FILE);
  boost::filesystem::path path(file_path.string_without_scheme());

  XmlOps::write_xml_node(*m_xmldoc.get(), path);

  CFinfo << "Journal dumped to '" << path.canonize().string() << "'" << CFendl;
}

////////////////////////////////////////////////////////////////////////////////

XmlNode * CJournal::copy_node(const XmlNode & in, XmlNode & out) const
{
  XmlNode * copy = XmlOps::add_node_to(out, in.name(), in.value());
  XmlAttr * attr = in.first_attribute();
  XmlNode * node = in.first_node();

  while( attr != nullptr )
  {
    XmlOps::add_attribute_to(*copy, attr->name(), attr->value());
    attr = attr->next_attribute();
  }


  while( node != nullptr )
  {
    copy_node(*node, *copy);

    node = node->next_sibling();
  }

  return copy;
}


////////////////////////////////////////////////////////////////////////////////

} // Common
} // CF

