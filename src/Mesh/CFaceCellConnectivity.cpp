// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include <set>
#include <deque>

#include "Common/OptionT.hpp"
#include "Common/ComponentPredicates.hpp"
#include "Common/CLink.hpp"
#include "Common/Log.hpp"

#include "Mesh/CFaceCellConnectivity.hpp"
#include "Mesh/CNodeElementConnectivity.hpp"
#include "Mesh/CDynTable.hpp"
#include "Mesh/CNodes.hpp"
#include "Mesh/CRegion.hpp"

namespace CF {
namespace Mesh {

using namespace Common;

////////////////////////////////////////////////////////////////////////////////

CFaceCellConnectivity::CFaceCellConnectivity ( const std::string& name ) : 
  Component(name)
{
  m_properties.add_option< OptionT<bool> >
      ( "StoreIsBdry",
        "Insert CList<bool> is_bdry in CElements, that are at the boundary",
        true );
  m_properties.link_to_parameter ( "StoreIsBdry", &m_store_is_bdry );
  
  m_properties.add_option< OptionT<bool> >
      ( "FilterBdry",
        "Only try to connect cells marked as boundary",
        true );
  m_properties.link_to_parameter ( "FilterBdry", &m_filter_bdry );
  
  m_elements = create_static_component<CUnifiedData<CElements> >("elements");
  m_elements_1 = create_static_component<CUnifiedData<CElements> >("elements_1");
  m_elements_2 = create_static_component<CUnifiedData<CElements> >("elements_2");
  m_connectivity = create_static_component<CTable<Uint> >("connectivity_table");
  m_connectivity->set_row_size(2);
  m_bdry_faces = create_static_component<CList<Uint> >("bdry_face_connectivity");
  m_face_nb_in_first_elem = create_static_component<CList<Uint> >("face_number");
  m_bdry_face_nb_in_first_elem = create_static_component<CList<Uint> >("bdry_face_number");
  m_is_bdry_face = create_static_component<CList<Uint> >("is_bdry_face");
  mark_basic();
}

////////////////////////////////////////////////////////////////////////////////

void CFaceCellConnectivity::setup(CRegion& region)
{
  m_elements->add_data(find_components_recursively_with_filter<CElements>(region,IsElementsVolume()).as_vector());  
  build_connectivity();
}

////////////////////////////////////////////////////////////////////////////////

void CFaceCellConnectivity::setup(CRegion& region1, CRegion& region2)
{
  m_elements->add_data(find_components_recursively_with_filter<CElements>(region1,IsElementsVolume()).as_vector());  
  m_elements->add_data(find_components_recursively_with_filter<CElements>(region2,IsElementsVolume()).as_vector());  
  m_elements_1->add_data(find_components_recursively_with_filter<CElements>(region1,IsElementsVolume()).as_vector());  
  m_elements_2->add_data(find_components_recursively_with_filter<CElements>(region2,IsElementsVolume()).as_vector());  
  
  build_interface_connectivity();
}

////////////////////////////////////////////////////////////////////////////////

void CFaceCellConnectivity::build_connectivity()
{
  /// variable that will count nb_faces;
  m_nb_faces=0;
  
  CTable<Uint>::Buffer f2c = m_connectivity->create_buffer();
  
  std::deque<bool> is_bdry_face;
  CList<Uint>::Buffer bdry_faces = m_bdry_faces->create_buffer();
  
  CList<Uint>::Buffer face_number = m_face_nb_in_first_elem->create_buffer();
  CList<Uint>::Buffer bdry_face_number = m_bdry_face_nb_in_first_elem->create_buffer();
  
  // create a table to store the connectivity element-face ID locally to this processor
  // and store the connectivity in MeshData
  // elem_to_face->resize(m_elements->size());

  if (m_elements->size() == 0 )
  {
    CFwarn << "No elements are given to build faces of" << CFendl;
    return;
  }
  CNodes& nodes = m_elements->data_components()[0]->nodes(); 
  // allocate a table mapping node-face ID
  std::vector < std::deque<Uint> > mapNodeFace(nodes.size());

  // atomic number to indicate the maximum possible number
  // of nodes in a face
  // allows to avoid frequent reallocations of the vector nodesInFace
  std::vector<Uint> face_nodes;
  face_nodes.reserve(100);
  
  std::vector<Uint> empty_connectivity_row(0);
  std::vector<Uint> dummy_row(2);
  
  Uint max_nb_faces = 0;
  
  boost_foreach ( CElements::Ptr elements, m_elements->data_components() )
  {
    //end_idx = begin_idx + elements->size();
    
    if (elements->element_type().dimensionality() != elements->element_type().dimension() )
      continue;
    const Uint nb_faces = elements->element_type().nb_faces();
    max_nb_faces += nb_faces * elements->size() ;
    
    //for (Uint idx=begin_idx; idx<end_idx; ++idx)
    //{
    //  elem_to_face->set_row_size(idx,nb_faces);
    //}
    
    //begin_idx = end_idx;
  }
  
  
  boost_foreach (CElements::Ptr elements, m_elements->data_components())
  {
    CList<bool>::Ptr is_bdry = elements->get_child<CList<bool> >("is_bdry");
    const Uint nb_elem = elements->size();
    if (is_null(is_bdry))
    {
      is_bdry = elements->create_component<CList<bool> >("is_bdry");
      is_bdry->resize(nb_elem);
      for (Uint e=0; e<nb_elem; ++e)
        (*is_bdry)[e]=true;
    }
  }

  // loop over the elements and construct faceIDs
  Uint elem_idx = 0;
  Uint nb_inner_faces = 0;
  Uint nb_matched_nodes = 1;
  Uint face_node_idx;
  Uint face_node;
  Uint face;
  Uint connected_face;
  Uint node;
  Uint nb_nodes;
  bool found_face = false;
  CElements::Ptr elem_location_comp;
  Uint elem_location_idx;

  // during the first big loop the following is done:
  // 1. set the element-faces connectivity
  // 2. set the geometric entity type IDs of each face
  // 3. select which are the boundary faces and which are internal ones

  // loop over the types
  boost_foreach (CElements::Ptr& elements, m_elements->data_components() )
  {

    const Uint nb_faces_in_elem = elements->element_type().nb_faces();

    CList<bool>& is_bdry = *elements->get_child<CList<bool> >("is_bdry");

    // loop over the elements of this type
    Uint loc_elem_idx=0;
    boost_foreach(CTable<Uint>::ConstRow elem, elements->connectivity_table().array() ) 
    {
      if ( is_bdry[loc_elem_idx] )
      {
        // loop over the faces in the current element
        for (Uint face_idx = 0; face_idx != nb_faces_in_elem; ++face_idx)
        {
          // construct sets of nodes that make the corresponding face in this element
          nb_nodes = elements->element_type().face_type(face_idx).nb_nodes();
          face_nodes.resize(nb_nodes);
          index_foreach(i,const Uint face_node_idx, elements->element_type().face_connectivity().face_node_range(face_idx))
             face_nodes[i] = elem[face_node_idx];


          // consider the first node belonging to the current face
          // check if you find a face ID shared between all the other
          // nodes building a face
          node = face_nodes[0];

          found_face = false;
          // search for matching face if they are registered to the node yet
          boost_foreach( face, mapNodeFace[node] )
          {
            nb_matched_nodes = 1;
            if (nb_nodes > 1) 
            {
              for (face_node_idx=1; face_node_idx!=nb_nodes; ++face_node_idx)
              {
                boost_foreach (connected_face, mapNodeFace[face_nodes[face_node_idx]])
                {
                  if (connected_face == face)
                  {
                    ++nb_matched_nodes;
                    break;
                  }
                }


                if (nb_matched_nodes == nb_nodes) // face matches the nodes
                {
                  // the corresponding face already exists, meaning
                  // that the face is an internal one, shared by two elements
                  // here you set the second element (==state) neighbor of the face
                  found_face = true;
                  //(*m_connectivity)[elemID][iFace] = currFaceID;
                  f2c.get_row(face)[1]=elem_idx;
                  is_bdry_face[face]=false;
                  // since it has two neighbor cells,
                  // this face is surely NOT a boundary face
                  // m_isBFace[currFaceID] = false;

                  // increment number of inner faces (they always have 2 states)
                  ++nb_inner_faces;
                  break;
                }
              }
            }
            else // this only applies to the 1D case 
            {
              // the corresponding faceID already exists, meaning
              // that the face is an internal one, shared by two elements
              // here you set the second element (==state) neighbor of the face
              found_face = true;
              //(*elem_to_face)[elemID][iFace] = currFaceID;
              f2c.get_row(face)[1]=elem_idx;
              is_bdry_face[face]=false;

              // since it has two neighbor cells,
              // this face is surely NOT a boundary face
              //m_isBFace[currFaceID] = false;

              // increment number of inner faces (they always have 2 states)
              ++nb_inner_faces;
              break;
            }
          }
          if (found_face == false) 
          {
            // a new face has been found
            // add the ID of the new face in the corresponding nodes
            // referencing it
            boost_foreach (face_node, face_nodes)
            {
              mapNodeFace[face_node].push_back(m_nb_faces);
            }

            face_number.add_row(face_idx);
            // store the geometric entity type for the current face
            //m_geoTypeIDs[m_nbFaces] = faceGeoTypeID[iFace];

            //(*elem_to_face)[elemID][iFace] = m_nbFaces;
            //nbFaceNodes.push_back(nbNodesPerFace);

            // increment the number of faces

            dummy_row[0]=elem_idx;
            f2c.add_row(dummy_row);
            is_bdry_face.push_back(true);
            ++m_nb_faces;
          }
        }
        ++elem_idx;
        ++loc_elem_idx;
      }
    }
  }
  f2c.flush();
  face_number.flush();

  CFinfo << "Total nb faces [" << m_nb_faces << "]" << CFendl;
  CFinfo << "Inner nb faces [" << nb_inner_faces << "]" << CFendl;

  // total number of boundary + partition boundary faces
  const Uint nb_bdry_plus_partition_faces = m_nb_faces - nb_inner_faces;
  CFinfo << "Boundary and Partition faces [" << nb_bdry_plus_partition_faces << "]" << CFendl;

  cf_assert(m_nb_faces <= max_nb_faces);
  cf_assert(nb_inner_faces <= max_nb_faces);


  for (Uint f=0; f<m_connectivity->size(); ++f)
  {
    boost_foreach (Uint elem, (*m_connectivity)[f])
    {
      boost::tie(elem_location_comp,elem_location_idx) = m_elements->data_location(elem);
      CList<bool>& is_bdry = *elem_location_comp->get_child<CList<bool> >("is_bdry");
      is_bdry[elem_location_idx] = is_bdry[elem_location_idx] || is_bdry_face[f] ;
    }
  }

  // cleanup
  for (Uint f=0; f<m_connectivity->size(); ++f)
  {
    if ( is_bdry_face[f] )
    {
      CFinfo << "removing row " << f << CFendl;
      bdry_faces.add_row(f2c.get_row(f)[0]);
      bdry_face_number.add_row(face_number.get_row(f));
      f2c.rm_row(f);
      face_number.rm_row(f);
      --m_nb_faces;
    }
  }
  f2c.flush();
  face_number.flush();
  bdry_faces.flush();
}

////////////////////////////////////////////////////////////////////////////////

void CFaceCellConnectivity::build_interface_connectivity()
{
  if (m_elements->size() == 0 )
  {
    CFwarn << "No elements are given to build faces of" << CFendl;
    return;
  }
  
  /// nodes component of the mesh
  CNodes& nodes = m_elements->data_components()[0]->nodes(); 
  
  /// variable that will count nb_faces;
  m_nb_faces=0;
  
  /// connectivity_table buffer
  CTable<Uint>::Buffer f2c = m_connectivity->create_buffer();
  
  /// storage for every face if it is connected to more than 1 element
  std::deque<bool> is_bdry_face;
  
  /// buffer to the list of bdry faces
  CList<Uint>::Buffer bdry_faces = m_bdry_faces->create_buffer();
  
  /// buffer to the list of face index in the first element of the face-element connectivity
  CList<Uint>::Buffer face_number = m_face_nb_in_first_elem->create_buffer();

  /// calculate set of boundary nodes of region 1
  std::set<Uint> region1_bdry_nodes;
  std::set<Uint>::iterator not_found_in_region1 = region1_bdry_nodes.end();
  boost_foreach( CElements::Ptr elements, m_elements_1->data_components() )
  {
    if (elements->has_tag("inner_faces"))
    {
      CF_DEBUG_POINT;
      CFaceCellConnectivity& f2c_1 = find_component<CFaceCellConnectivity>(*elements);
      CF_DEBUG_POINT;
      Uint nb_bdry_faces = f2c_1.get_child<CList<Uint> >("bdry_face_connectivity")->size();
      for (Uint f=0; f<nb_bdry_faces; ++f)
      {
        boost_foreach ( const Uint node, f2c_1.nodes_using_bdry_face_connectivity(f) )
          region1_bdry_nodes.insert(node);
      }      
    }
  }
  
  /// calculate set of boundary nodes of region 2
  std::set<Uint> region2_bdry_nodes;
  std::set<Uint>::iterator not_found_in_region2 = region2_bdry_nodes.end();
  boost_foreach( CElements::Ptr elements, m_elements_2->data_components() )
  {
    if (elements->has_tag("inner_faces"))
    {
      CF_DEBUG_POINT;
      CFaceCellConnectivity& f2c_2 = find_component<CFaceCellConnectivity>(*elements);
      CF_DEBUG_POINT;
      Uint nb_bdry_faces = f2c_2.get_child<CList<Uint> >("bdry_face_connectivity")->size();
      for (Uint f=0; f<nb_bdry_faces; ++f)
      {
        boost_foreach ( const Uint node, f2c_2.nodes_using_bdry_face_connectivity(f) )
          region2_bdry_nodes.insert(node);
      }
    }
  }
  
  /// node to faces, will be built gradually in the searching algorithm
  /// as new faces are discovered
  std::vector < std::deque<Uint> > mapNodeFace(nodes.size());

  // atomic number to indicate the maximum possible number of nodes in a face,
  // allows to avoid frequent reallocations of the vector nodesInFace
  std::vector<Uint> face_nodes;
  face_nodes.reserve(100);
  
  // preallocated vector of size 2
  std::vector<Uint> dummy_row(2);


  // Temporary variables used in the algorithm loop, to avoid reallocations
  Uint elem_idx = 0;
  Uint nb_inner_faces = 0;
  Uint nb_matched_nodes = 1;
  Uint face_node_idx;
  Uint face_node;
  Uint face;
  Uint connected_face;
  Uint node;
  bool found_face = false;
  CElements::Ptr elem_location_comp;
  Uint elem_location_idx;

  // loop over the element types
  boost_foreach (CElements::Ptr& elements, m_elements->data_components() )
  {
    if (IsElementsVolume()(elements))
    {
      const Uint nb_faces_in_elem = elements->element_type().nb_faces();

      CList<bool>&is_bdry_elem = *elements->get_child<CList<bool> >("is_bdry");

      // loop over the elements of this type
      Uint nb_elem=elements->size();
      for (Uint e=0; e<nb_elem; ++e , ++elem_idx)
      {
        if ( is_bdry_elem[e] )
        {
          CTable<Uint>::ConstRow elem = elements->connectivity_table()[e]; 

          // loop over the faces in the current element
          for (Uint face_idx = 0; face_idx != nb_faces_in_elem; ++face_idx)
          {
            // construct sets of nodes that make the corresponding face in this element
            const Uint nb_nodes = elements->element_type().face_type(face_idx).nb_nodes();
            face_nodes.resize(nb_nodes);
            index_foreach(i,const Uint face_node_idx, elements->element_type().face_connectivity().face_node_range(face_idx))
               face_nodes[i] = elem[face_node_idx];


            // consider the first node belonging to the current face
            // check if you find a face ID shared between all the other
            // nodes building a face
            node = face_nodes[0];

            found_face = false;
            // search for matching face if they are registered to the node yet
            boost_foreach( face, mapNodeFace[node] )
            {
              nb_matched_nodes = 1;
              if (nb_nodes > 1) 
              {
                for (face_node_idx=1; face_node_idx!=nb_nodes; ++face_node_idx)
                {
                  const Uint face_node = face_nodes[face_node_idx];
                  boost_foreach (connected_face, mapNodeFace[face_node])
                  {
                    if (connected_face == face)
                    {
                      ++nb_matched_nodes;
                      break;
                    }
                  }

                  if (nb_matched_nodes == nb_nodes) // face matches the nodes
                  {
                    // the corresponding face already exists, meaning
                    // that the face is an internal one, shared by two elements
                    // here you set the second element (==state) neighbor of the face
                    found_face = true;

                    //(*m_connectivity)[elemID][iFace] = currFaceID;
                    f2c.get_row(face)[1]=elem_idx;
                    // since it has two neighbor cells,
                    // this face is surely NOT a boundary face
                    // m_isBFace[currFaceID] = false;

                    // increment number of inner faces (they always have 2 states)
                    ++nb_inner_faces;

                    break;
                  }
                }
              }
              else // this only applies to the 1D case 
              {
                // the corresponding faceID already exists, meaning
                // that the face is an internal one, shared by two elements
                // here you set the second element (==state) neighbor of the face
                found_face = true;

                //(*elem_to_face)[elemID][iFace] = currFaceID;
                f2c.get_row(face)[1]=elem_idx;
                // since it has two neighbor cells,
                // this face is surely NOT a boundary face
                //m_isBFace[currFaceID] = false;

                // increment number of inner faces (they always have 2 states)
                ++nb_inner_faces;
                break;
              }
            }
            if (found_face == false) 
            {
              // a new face has been found
              // add the ID of the new face in the corresponding nodes
              // referencing it
              bool is_interface_face = true;
              boost_foreach (face_node, face_nodes)
              {
                if (   region1_bdry_nodes.find(face_node) == not_found_in_region1
                    || region2_bdry_nodes.find(face_node) == not_found_in_region2)
                {
                  // This is not an interface face between region1 and region2
                  is_interface_face=false;
                  break;
                }
              }
              if (is_interface_face)
              {
                boost_foreach (face_node, face_nodes)
                {
                  mapNodeFace[face_node].push_back(m_nb_faces);
                }
                face_number.add_row(face_idx);

                // store the geometric entity type for the current face
                //m_geoTypeIDs[m_nbFaces] = faceGeoTypeID[iFace];

                //(*elem_to_face)[elemID][iFace] = m_nbFaces;
                //nbFaceNodes.push_back(nbNodesPerFace);

                // increment the number of faces
                dummy_row[0]=elem_idx;
                f2c.add_row(dummy_row);
                ++m_nb_faces;
              }
            }
          }
        }
      }
    }
  }

  face_number.flush();
  f2c.flush();

  CFinfo << "Total nb faces [" << m_nb_faces << "]" << CFendl;
  CFinfo << "Inner nb faces [" << nb_inner_faces << "]" << CFendl;

  // total number of boundary + partition boundary faces
  const Uint nb_bdry_plus_partition_faces = m_nb_faces - nb_inner_faces;
  CFinfo << "Boundary and Partition faces [" << nb_bdry_plus_partition_faces << "]" << CFendl;

//  cf_assert(m_nb_faces <= max_nb_faces);
  cf_assert(nb_inner_faces <= m_nb_faces);

  boost_foreach (CTable<Uint>::ConstRow face_elems, connectivity().array() )
  {
    boost_foreach (Uint elem, face_elems)
    {
      boost::tie(elem_location_comp,elem_location_idx) = m_elements->data_location(elem);
      CList<bool>& is_bdry_elem = *elem_location_comp->get_child<CList<bool> >("is_bdry");
      is_bdry_elem[elem_location_idx] = false;
      CFinfo << "elem [" << elem_location_comp->glb_idx()[elem_location_idx] << "] is now interface";
      CFinfo << " but it could still be a boundary element on one side!!!" << CFendl;
    }
  }


  // 
  // m_nbInFacesNodes.resize(nbInnerFaces);
  // m_nbBFacesNodes.resize(nbBPlusPartitionFaces);
  // 
  // // set the number of nodes in faces
  // CFuint iBFace = 0;
  // CFuint iInFace = 0;
  // for (CFuint i = 0; i < nbFaceNodes.size(); ++i) {
  //   if (!m_isBFace[i]) {
  //     m_nbInFacesNodes[iInFace++] = nbFaceNodes[i];
  //   }
  //   else {
  //     m_nbBFacesNodes[iBFace++] = nbFaceNodes[i];
  //   }
  // }

}

////////////////////////////////////////////////////////////////////////////////
void CFaceCellConnectivity::match_faces()
{
  CList<Uint>::Buffer face_number = m_face_nb_in_first_elem->create_buffer();
  CTable<Uint>::Buffer f2c = m_connectivity->create_buffer();
  CList<Uint>::Buffer is_bdry_face = m_is_bdry_face->create_buffer();

  
  CNodeElementConnectivity& node2faces = *create_component<CNodeElementConnectivity>("node2faces");
  node2faces.add_elements(m_elements_1->data_components()); // it is assumed this is only face types
  Uint start_idx_faces2 = node2faces.get_child<CUnifiedData<CElements> >("elements")->size();
  CFLogVar(start_idx_faces2);
  node2faces.add_elements(m_elements_2->data_components()); // it is assumed this is only face types
  Uint tot_nb_elements_to_match = node2faces.get_child<CUnifiedData<CElements> >("elements")->size();
  CFLogVar(tot_nb_elements_to_match);
  
  
  boost_foreach (CElements::Ptr faces1 , m_elements_1->data_components())
  {
    const Uint nb_faces1 = faces1->size();
    const Uint nb_nodes_per_face1 = faces1->element_type().nb_nodes();
    for (Uint face1_idx = 0; face1_idx != nb_faces1; ++face1_idx)
    {
      CTable<Uint>::ConstRow face_nodes = faces1->connectivity_table()[face1_idx];
      Uint nb_matched_nodes = 0;
      std::set<Uint> matched_faces;
      std::set<Uint>::iterator not_found = matched_faces.end();
      boost_foreach(const Uint face_node, face_nodes)
      {
        boost_foreach(const Uint face, node2faces.elements(face_node))
        {
          std::set<Uint>::iterator it = matched_faces.find(face);
          if ( it == not_found)
            matched_faces.insert(face);
          else
            matched_faces.erase(it);
        }
      }
    }
  }
  
  remove_component("node2faces");
  
  std::set<Uint> region1_bdry_nodes;
  std::set<Uint>::iterator not_found_in_region1 = region1_bdry_nodes.end();
  boost_foreach( CElements::Ptr elements, m_elements_1->data_components() )
  {
    CList<bool>& is_bdry = *elements->get_child<CList<bool> >("is_bdry");
    Uint nb_elem = elements->size();
    for (Uint e=0; e<nb_elem; ++e)
    {
      if (is_bdry[e])
      {
        boost_foreach ( const Uint node, elements->connectivity_table()[e] )
          region1_bdry_nodes.insert(node);
      }
    }
  }
  std::set<Uint> region2_bdry_nodes;
  std::set<Uint>::iterator not_found_in_region2 = region2_bdry_nodes.end();
  boost_foreach( CElements::Ptr elements, m_elements_2->data_components() )
  {
    CList<bool>& is_bdry = *elements->get_child<CList<bool> >("is_bdry");
    Uint nb_elem = elements->size();
    for (Uint e=0; e<nb_elem; ++e)
    {
      if (is_bdry[e])
      {
        boost_foreach ( const Uint node, elements->connectivity_table()[e] )
          region2_bdry_nodes.insert(node);
      }
    }
  }
  /// variable that will count nb_faces;
  m_nb_faces=0;
    
  // create a table to store the connectivity element-face ID locally to this processor
  // and store the connectivity in MeshData
  // elem_to_face->resize(m_elements->size());

  if (m_elements->size() == 0 )
  {
    CFwarn << "No elements are given to build faces of" << CFendl;
    return;
  }
  CNodes& nodes = m_elements->data_components()[0]->nodes(); 
  // allocate a table mapping node-face ID
  std::vector < std::deque<Uint> > mapNodeFace(nodes.size());

  std::vector<Uint> dummy_row(2);

  // atomic number to indicate the maximum possible number
  // of nodes in a face
  // allows to avoid frequent reallocations of the vector nodesInFace
  std::vector<Uint> face_nodes;
  face_nodes.reserve(100);
  
  std::vector<Uint> empty_connectivity_row(0);
  
  Uint max_nb_faces = 0;
  
  boost_foreach ( CElements::Ptr elements, m_elements->data_components() )
  {
    const Uint nb_faces = elements->element_type().nb_faces();
    max_nb_faces += nb_faces * elements->size() ;
  }

  // loop over the elements and construct faceIDs
  Uint elem_idx = 0;
  Uint nb_inner_faces = 0;
  Uint nb_matched_nodes = 1;
  Uint face_node_idx;
  Uint face_node;
  Uint face;
  Uint connected_face;
  Uint node;
  bool found_face = false;
  CElements::Ptr elem_location_comp;
  Uint elem_location_idx;

  // during the first big loop the following is done:
  // 1. set the element-faces connectivity
  // 2. set the geometric entity type IDs of each face
  // 3. select which are the boundary faces and which are internal ones

  // loop over the types
  boost_foreach (CElements::Ptr& elements, m_elements->data_components() )
  {
    const Uint nb_faces_in_elem = elements->element_type().nb_faces();

    CList<bool>::Ptr is_bdry = elements->get_child<CList<bool> >("is_bdry");
    
    // loop over the elements of this type
    Uint nb_elem=elements->size();
    for (Uint e=0; e<nb_elem; ++e , ++elem_idx)
    {
      if (m_filter_bdry)
      {
        cf_assert( is_not_null(is_bdry) );
        if ( (*is_bdry)[e] == false )
        {
          continue;
        }
      }
      CTable<Uint>::ConstRow elem = elements->connectivity_table()[e]; 
      
      // loop over the faces in the current element
      for (Uint face_idx = 0; face_idx != nb_faces_in_elem; ++face_idx)
      {
        // construct sets of nodes that make the corresponding face in this element
        const Uint nb_nodes = elements->element_type().face_type(face_idx).nb_nodes();
        face_nodes.resize(nb_nodes);
        index_foreach(i,const Uint face_node_idx, elements->element_type().face_connectivity().face_node_range(face_idx))
           face_nodes[i] = elem[face_node_idx];


        // consider the first node belonging to the current face
        // check if you find a face ID shared between all the other
        // nodes building a face
        node = face_nodes[0];
        
        found_face = false;
        // search for matching face if they are registered to the node yet
        boost_foreach( face, mapNodeFace[node] )
        {
          nb_matched_nodes = 1;
          if (nb_nodes > 1) 
          {
            for (face_node_idx=1; face_node_idx!=nb_nodes; ++face_node_idx)
            {
              const Uint face_node = face_nodes[face_node_idx];
              boost_foreach (connected_face, mapNodeFace[face_node])
              {
                if (connected_face == face)
                {
                  ++nb_matched_nodes;
                  break;
                }
              }

              if (nb_matched_nodes == nb_nodes) // face matches the nodes
              {
                // the corresponding face already exists, meaning
                // that the face is an internal one, shared by two elements
                // here you set the second element (==state) neighbor of the face
                found_face = true;
                
                //(*m_connectivity)[elemID][iFace] = currFaceID;
                f2c.get_row(face)[1]=elem_idx;
                // since it has two neighbor cells,
                // this face is surely NOT a boundary face
                // m_isBFace[currFaceID] = false;

                // increment number of inner faces (they always have 2 states)
                ++nb_inner_faces;
                
                break;
              }
            }
          }
          else // this only applies to the 1D case 
          {
            // the corresponding faceID already exists, meaning
            // that the face is an internal one, shared by two elements
            // here you set the second element (==state) neighbor of the face
            found_face = true;
            
            //(*elem_to_face)[elemID][iFace] = currFaceID;
            f2c.get_row(face)[1]=elem_idx;
            // since it has two neighbor cells,
            // this face is surely NOT a boundary face
            //m_isBFace[currFaceID] = false;

            // increment number of inner faces (they always have 2 states)
            ++nb_inner_faces;
            break;
          }
        }
        if (found_face == false) 
        {
          // a new face has been found
          // add the ID of the new face in the corresponding nodes
          // referencing it
          bool is_interface_face = true;
          boost_foreach (face_node, face_nodes)
          {
            if (   region1_bdry_nodes.find(face_node) == not_found_in_region1
                || region2_bdry_nodes.find(face_node) == not_found_in_region2)
            {
              // This is not an interface face between region1 and region2
              is_interface_face=false;
              break;
            }
          }
          if (is_interface_face)
          {
            boost_foreach (face_node, face_nodes)
            {
              mapNodeFace[face_node].push_back(m_nb_faces);
            }
            face_number.add_row(face_idx);
            
            // store the geometric entity type for the current face
            //m_geoTypeIDs[m_nbFaces] = faceGeoTypeID[iFace];

            //(*elem_to_face)[elemID][iFace] = m_nbFaces;
            //nbFaceNodes.push_back(nbNodesPerFace);

            // increment the number of faces
            dummy_row[0]=elem_idx;
            f2c.add_row(dummy_row);
            ++m_nb_faces;
          }
        }
      }        
    }
  }

  face_number.flush();
  is_bdry_face.flush();
  f2c.flush();

  CFinfo << "Total nb faces [" << m_nb_faces << "]" << CFendl;
  CFinfo << "Inner nb faces [" << nb_inner_faces << "]" << CFendl;

  // total number of boundary + partition boundary faces
  const Uint nb_bdry_plus_partition_faces = m_nb_faces - nb_inner_faces;
  CFinfo << "Boundary and Partition faces [" << nb_bdry_plus_partition_faces << "]" << CFendl;

  cf_assert(m_nb_faces <= max_nb_faces);
  cf_assert(nb_inner_faces <= m_nb_faces);

  boost_foreach (CTable<Uint>::ConstRow face_elems, connectivity().array() )
  {
    boost_foreach (Uint elem, face_elems)
    {
      boost::tie(elem_location_comp,elem_location_idx) = m_elements->data_location(elem);
      CList<bool>::Ptr is_bdry = elem_location_comp->get_child<CList<bool> >("is_bdry");
      (*is_bdry)[elem_location_idx] = false;
      CFinfo << "elem [" << elem_location_comp->glb_idx()[elem_location_idx] << "] is now interface";
      CFinfo << " but it could still be a boundary element on one side!!!" << CFendl;
    }
  }


  // 
  // m_nbInFacesNodes.resize(nbInnerFaces);
  // m_nbBFacesNodes.resize(nbBPlusPartitionFaces);
  // 
  // // set the number of nodes in faces
  // CFuint iBFace = 0;
  // CFuint iInFace = 0;
  // for (CFuint i = 0; i < nbFaceNodes.size(); ++i) {
  //   if (!m_isBFace[i]) {
  //     m_nbInFacesNodes[iInFace++] = nbFaceNodes[i];
  //   }
  //   else {
  //     m_nbBFacesNodes[iBFace++] = nbFaceNodes[i];
  //   }
  // }

}

////////////////////////////////////////////////////////////////////////////////

CTable<Uint>::ConstRow CFaceCellConnectivity::elements(const Uint unified_face_idx) const
{
  return (*m_connectivity)[unified_face_idx];
}

////////////////////////////////////////////////////////////////////////////////

CUnifiedData<CElements>::data_location_type CFaceCellConnectivity::element_location(const Uint unified_elem_idx)
{
  return m_elements->data_location(unified_elem_idx);
}

////////////////////////////////////////////////////////////////////////////////

CUnifiedData<CElements>::const_data_location_type CFaceCellConnectivity::element_location(const Uint unified_elem_idx) const
{
  return m_elements->data_location(unified_elem_idx);
}

////////////////////////////////////////////////////////////////////////////////

std::vector<Uint> CFaceCellConnectivity::nodes_using_inner_face_connectivity(const Uint face) const
{
  Uint unified_elem_idx = (*m_connectivity)[face][0];
  CElements::Ptr elem_comp;
  Uint elem_idx;
  boost::tie(elem_comp,elem_idx) = m_elements->data_location(unified_elem_idx);
  std::vector<Uint> nodes(elem_comp->element_type().face_type((*m_face_nb_in_first_elem)[face]).nb_nodes());
  index_foreach (i, Uint node_in_face, elem_comp->element_type().face_connectivity().face_node_range((*m_face_nb_in_first_elem)[face]))
  {
    nodes[i] = elem_comp->connectivity_table()[elem_idx][node_in_face];
  }
  return nodes;
}

////////////////////////////////////////////////////////////////////////////////

std::vector<Uint> CFaceCellConnectivity::nodes_using_bdry_face_connectivity(const Uint face) const
{
  Uint unified_elem_idx = (*m_bdry_faces)[face];
  CElements::Ptr elem_comp;
  Uint elem_idx;
  boost::tie(elem_comp,elem_idx) = m_elements->data_location(unified_elem_idx);
  std::vector<Uint> nodes(elem_comp->element_type().face_type((*m_bdry_face_nb_in_first_elem)[face]).nb_nodes());
  index_foreach (i, Uint node_in_face, elem_comp->element_type().face_connectivity().face_node_range((*m_bdry_face_nb_in_first_elem)[face]))
  {
    nodes[i] = elem_comp->connectivity_table()[elem_idx][node_in_face];
  }
  return nodes;
}

////////////////////////////////////////////////////////////////////////////////

void CFaceCellConnectivity::set_elements( CUnifiedData<CElements>::Ptr elements )
{
  m_elements->add_data(find_components(elements->data_links()).as_vector());
}

////////////////////////////////////////////////////////////////////////////////

} // Mesh
} // CF
