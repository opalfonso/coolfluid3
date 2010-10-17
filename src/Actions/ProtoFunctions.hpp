// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#ifndef CF_Actions_ProtoFunctions_hpp
#define CF_Actions_ProtoFunctions_hpp

#include <cmath>

#include "Actions/ProtoDomain.hpp"
#include <boost/concept_check.hpp>

#include "Math/RealMatrix.hpp"
#include "Math/RealVector.hpp"
#include "Common/Log.hpp"

namespace CF {
namespace Actions {

/// Tags functions that are evaluated using element shape functions
template<typename FunctionT>
struct sf_function_tag
{};

#define SF_FUNCTION(__FName)    \
struct __FName ## _tag {}; \
\
template<typename NodesT> \
typename boost::proto::result_of::make_expr< \
    sf_function_tag<__FName ## _tag> \
  , MeshDomain \
  , NodesT const & \
>::type const \
__FName(NodesT const &nodes) \
{ \
  return boost::proto::make_expr<sf_function_tag<__FName ## _tag>, MeshDomain>(boost::ref(nodes)); \
}

/// The set of shape function wrappers is defined using macros to avoid code reptition. See the
/// shape function docs for more info.
SF_FUNCTION(volume)
SF_FUNCTION(jacobian)
SF_FUNCTION(jacobian_adjoint)
SF_FUNCTION(jacobian_determinant)
SF_FUNCTION(mapped_gradient)
SF_FUNCTION(normal)
SF_FUNCTION(laplacian)

#undef SF_FUNCTION

/// Integrates the argument
template<Uint Order>
struct integral_tag
{};

template<Uint Order, typename Arg>
typename boost::proto::result_of::make_expr<
    integral_tag<Order>
  , MeshDomain
  , Arg const &
>::type const
integral(Arg const &arg)
{
  return boost::proto::make_expr<integral_tag<Order>, MeshDomain>(boost::ref(arg));
}

/// Executes the second argument for each node of the element data given in the first argument
struct for_each_node_tag
{};

template<typename NodesT, typename ExprT>
typename boost::proto::result_of::make_expr<
    for_each_node_tag
  , MeshDomain
  , NodesT const&
  , ExprT const&
>::type const
for_each_element_node(NodesT const& nodes, ExprT const& expr)
{
  return boost::proto::make_expr<for_each_node_tag, MeshDomain>(boost::ref(nodes), boost::ref(expr));
}

/// Pow function based on Proto docs example
template<Uint Exp>
struct pow_fun
{
  typedef Real result_type;
  Real operator()(Real d) const
  {
    for(Uint i = 0; i != (Exp-1); ++i)
      d *= d;
    return d;
  }
};

template<Uint Exp, typename Arg>
typename boost::proto::result_of::make_expr<
    boost::proto::tag::function  // Tag type
  , pow_fun<Exp>          // First child (by value)
  , Arg const &           // Second child (by reference)
>::type const
pow(Arg const &arg)
{
  return boost::proto::make_expr<boost::proto::tag::function>(
      pow_fun<Exp>()    // First child (by value)
    , boost::ref(arg)   // Second child (by reference)
  );
}

/// Accept a 2D realvector for atan2
Real atan_vec(const RealVector& vec)
{
  return atan2(vec[1], vec[0]);
}

// Wrap some math functions
boost::proto::terminal< double(*)(double) >::type const _sin = {&sin};
boost::proto::terminal< double(*)(double, double) >::type const _atan2 = {&atan2};
boost::proto::terminal< double(*)(const RealVector&) >::type const _atan_vec = {&atan_vec};

// Wrap std::cout
boost::proto::terminal< std::ostream & >::type _cout = {std::cout};

/// Transpose a matrix. TODO: use expression templates (Eigen?)
/// Don't use this, it's a bad hack as proof of concept
const RealMatrix& transpose(const RealMatrix& to_transpose)
{
  static RealMatrix transposed;
  transposed.resize(to_transpose.nbCols(), to_transpose.nbRows());
  to_transpose.transpose(transposed);
  return transposed;
}


typedef const RealMatrix&(*TransposeT)(const RealMatrix&);
boost::proto::terminal< TransposeT >::type const _transpose = {&transpose};

} // namespace Actions
} // namespace CF

#endif // CF_Actions_ProtoFunctions_hpp
