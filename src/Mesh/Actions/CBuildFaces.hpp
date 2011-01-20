// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#ifndef CF_Mesh_CBuildFaces_hpp
#define CF_Mesh_CBuildFaces_hpp

////////////////////////////////////////////////////////////////////////////////

#include "Mesh/CMeshTransformer.hpp"

#include "Mesh/Actions/LibActions.hpp"

////////////////////////////////////////////////////////////////////////////////

namespace CF {
namespace Mesh {
  
  class CField;
  class CFaceCellConnectivity;

namespace Actions {
  
//////////////////////////////////////////////////////////////////////////////

/// This class defines a mesh transformer
/// that returns information about the mesh
/// @author Willem Deconinck
class Mesh_Actions_API CBuildFaces : public CMeshTransformer
{
public: // typedefs

    typedef boost::shared_ptr<CBuildFaces> Ptr;
    typedef boost::shared_ptr<CBuildFaces const> ConstPtr;

private: // typedefs
  
public: // functions
  
  /// constructor
  CBuildFaces( const std::string& name );
  
  /// Gets the Class name
  static std::string type_name() { return "CBuildFaces"; }

  virtual void transform(const CMesh::Ptr& mesh, const std::vector<std::string>& args);
  
  /// brief description, typically one line
  virtual std::string brief_description() const;
  
  /// extended help that user can query
  virtual std::string help() const;
  
private: // functions
 
  void make_interfaces(Component::Ptr parent);

  void build_inner_faces_bottom_up(Component::Ptr parent);

  void build_inner_face_elements(CRegion& in_region, CFaceCellConnectivity& from_face_to_cell);
  
  void build_outer_face_elements(CRegion& in_region, CFaceCellConnectivity& from_face_to_cell);
  

private: // data

  CMesh::Ptr m_mesh;
  
}; // end CBuildFaces


////////////////////////////////////////////////////////////////////////////////

} // Actions
} // Mesh
} // CF

////////////////////////////////////////////////////////////////////////////////

#endif // CF_Mesh_CBuildFaces_hpp
