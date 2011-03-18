// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#ifndef CF_FVM_BCReflectCons1D_hpp
#define CF_FVM_BCReflectCons1D_hpp

#include "Solver/Actions/CLoopOperation.hpp"
#include "FVM/LibFVM.hpp"

/////////////////////////////////////////////////////////////////////////////////////

namespace CF {
namespace Mesh {
  class CConnectedFieldView;
  class CFieldView;
}
namespace FVM {

///////////////////////////////////////////////////////////////////////////////////////

class FVM_API BCReflectCons1D : public Solver::Actions::CLoopOperation
{
public: // typedefs

  /// pointers
  typedef boost::shared_ptr<BCReflectCons1D> Ptr;
  typedef boost::shared_ptr<BCReflectCons1D const> ConstPtr;

public: // functions
  /// Contructor
  /// @param name of the component
  BCReflectCons1D ( const std::string& name );

  /// Virtual destructor
  virtual ~BCReflectCons1D() {};

  /// Get the class name
  static std::string type_name () { return "BCReflectCons1D"; }

  /// execute the action
  virtual void execute ();

private: // helper functions

  void config_solution();
  void config_normal();

  void trigger_elements();
  
private: // data
  
  enum {INNER=0,GHOST=1};
  
  Mesh::CConnectedFieldView m_connected_solution;
  Mesh::CFieldView          m_face_normal;

};

////////////////////////////////////////////////////////////////////////////////

} // FVM
} // CF

/////////////////////////////////////////////////////////////////////////////////////

#endif // CF_FVM_BCReflectCons1D_hpp
