﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
//-----------------------------------------------------------------------------
// Copyright 2000-2025 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* ArcaneBasicMeshSubdividerService.cc                         (C) 2000-2025 */
/*                                                                           */
/* Service Arcane gérant un maillage du jeu de données.                      */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

// get parameter
#include "arcane/utils/ApplicationInfo.h"
#include "arcane/utils/CommandLineArguments.h"
#include "arcane/utils/List.h"
#include "arcane/utils/MDDim.h"

#include "arcane/core/IMeshSubdivider.h"
#include "arcane/impl/ArcaneBasicMeshSubdividerService_axl.h"

#include "arcane/core/ItemGroup.h"
#include "arcane/core/ItemPrinter.h"
#include "arcane/core/IItemFamily.h"
#include "arcane/core/IGhostLayerMng.h"
#include "arcane/core/MeshUtils.h"
#include "arcane/core/IMeshModifier.h"

#include "arcane/core/SimpleSVGMeshExporter.h" // Write au format svg pour le 2D
// Write variables

#include "arcane/core/ServiceBuilder.h"
#include "arcane/core/Directory.h"
#include "arcane/core/IVariableMng.h"
#include "arcane/core/IParallelMng.h"
#include "arcane/core/BasicService.h"
#include "arcane/core/IPrimaryMesh.h"
#include "arcane/core/Item.h"
// Post processor
#include "arcane/core/PostProcessorWriterBase.h"
// Ajouter des variables
#include "arcane/core/VariableBuildInfo.h"
#include "arcane/core/IMeshUtilities.h"
#include "arcane/core/ISubDomain.h"
#include "arcane/cartesianmesh/ICartesianMesh.h"
#include "arcane/core/Properties.h"
#include "arcane/std/IMeshGenerator.h"

// Utils
#include <unordered_set>
#include <algorithm>
#include <iterator>
#include <map>

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace Arcane
{

typedef UniqueArray<UniqueArray<Int64>> StorageRefine;
namespace MeshSubdivider
{

  /*---------------------------------------------------------------------------*/
  /*---------------------------------------------------------------------------*/
  /*!
 * \brief Classe Pattern qui permet de manipuler un motif (pattern en anglais) de raffinement.
 */
  class Pattern
  {
   public:

    //! Type de l'élément a rafiner
    Int16 type;
    //! Type de la face de l'élément a rafiner
    Int16 face_type;
    //! Type des cellules enfants
    Int16 cell_type;
    //! Matrice pour la génération des nouveaux noeuds
    StorageRefine nodes;
    //! Matrice pour la génération des nouvelles faces
    StorageRefine faces;
    //! Matrice pour la génération des nouvelles cellules
    StorageRefine cells;
    StorageRefine child_faces; // Lien entre faces de la cellule mère et les faces de la cellule fille.
    // ^-- Pour la gestion des groups ou des propriétés. Par exemple pour le sod, une face mère dans le groupe "membrane" doit engendrer des faces avec le même groupe.
    // Les faces internes n'ont pas de face mère mais pourrait avoir besoin de propager ou déposer des propriétés sur ces faces.
    // Pour l'instant elles ne sont juste pas dans les groupes.

    // En fait les seules informations importantes sont dans 'cells' et 'nodes'. Arcane peut déduire les faces pour nous et on garde le même ordre pour le parallel.
    // Pour les childs faces

   public:

    Pattern()
    : type(IT_NullType)
    , face_type(IT_NullType)
    , cell_type(IT_NullType)
    {}

    Pattern(Int16 type, Int16 face_type, Int16 cell_type, StorageRefine nodes, StorageRefine faces, StorageRefine cells, StorageRefine child_faces)
    {
      this->type = type;
      this->face_type = face_type;
      this->cell_type = cell_type;
      this->nodes = nodes;
      this->faces = faces;
      this->cells = cells;
      this->child_faces = child_faces;
    }

    Pattern(Pattern&& other) noexcept
    : type(other.type)
    , face_type(other.face_type)
    , cell_type(other.cell_type)
    , nodes(other.nodes)
    , faces(other.faces)
    , cells(other.cells)
    , child_faces(other.child_faces)
    {}

    Pattern(const Pattern&) = delete;

    Pattern(Pattern& other) noexcept
    : type(other.type)
    , face_type(other.face_type)
    , cell_type(other.cell_type)
    , nodes(other.nodes)
    , faces(other.faces)
    , cells(other.cells)
    , child_faces(other.child_faces)
    {}
    Pattern& operator=(const Pattern& other)
    {
      if (this != &other) {
        type = other.type;
        face_type = other.face_type;
        cell_type = other.cell_type;
        nodes = other.nodes; // Référence partagée
        faces = other.faces; // Référence partagée
        cells = other.cells; // Référence partagée
        child_faces = other.child_faces;
      }
      return *this;
    }

    Pattern& operator=(Pattern&& other) noexcept
    {
      if (this != &other) {
        type = other.type;
        face_type = other.face_type;
        cell_type = other.cell_type;
        nodes = other.nodes;
        faces = other.faces;
        cells = other.cells;
        child_faces = other.child_faces;
      }
      return *this;
    }
    Pattern& operator=(Pattern& other) noexcept
    {
      if (this != &other) {
        type = other.type;
        face_type = other.face_type;
        cell_type = other.cell_type;
        nodes = other.nodes;
        faces = other.faces;
        cells = other.cells;
        child_faces = other.child_faces;
      }
      return *this;
    }
  };

} // namespace MeshSubdivider
/*!
 * \brief Classe qui permet de construire des patterns
 */
class PatternBuilder
{
 public:

  // 2D
  // TODO test + f + cf
  static MeshSubdivider::Pattern quadtoquad();
  // TODO test + f + cf
  static MeshSubdivider::Pattern quadtotri();
  // TODO test + f + cf
  static MeshSubdivider::Pattern tritotri();
  // TODO test + f + cf
  static MeshSubdivider::Pattern tritoquad();
  // 3D
  // N'utilise pas la numérotation des faces de arcane pour l'instant
  static MeshSubdivider::Pattern hextohex();

  static MeshSubdivider::Pattern tettotet();

  static MeshSubdivider::Pattern hextotet(); // Ne fonctionne pas car on ne prend pas en compte la rotation.

  static MeshSubdivider::Pattern tettohex();

  static MeshSubdivider::Pattern hextotet24();
};

/*

Noeuds
(3) --- (2)
 |       |
 |       |
(0) --- (1)
Ordre des arêtes
(3) -2- (2)
 |       |
 3       1 
 |       |
(0) -0- (1)
Nouveaux noeuds
(3) -6- (2)
 |       |
 7   8   5 
 |       |
(0) -4- (1)
Nouveaux noeuds avec ordre nouvelles arêtes
(3) -5- (6) -7- (2)
|        |       |
4        6       8
|        |       |
(7) -1- (8) -9- (5)
|        |       |
0        2       10
|        |       | 
(0) -3- (4) -11- (1)
  ---   ---
|     |     |
|     |     |
  ---   ---  
|     |     |
|     |     | 
  ---   --- 
*/
MeshSubdivider::Pattern PatternBuilder::quadtoquad()
{
  StorageRefine nodes({
  { 0, 1 }, // 4
  { 1, 2 }, // 5
  { 2, 3 }, // 6
  { 3, 0 }, // 7
  { 0, 1, 2, 3 }, // 8
  });
  StorageRefine faces({
  { 0, 7 }, // 0
  { 7, 8 }, // 1
  { 4, 8 }, // 2
  { 0, 4 }, // 3
  { 3, 7 }, // 4
  { 3, 6 }, // 5
  { 6, 8 }, // 6
  { 2, 6 }, // 7
  { 2, 5 }, // 8
  { 5, 8 }, // 9
  { 1, 5 }, // 10
  { 1, 4 }, // 11
  });
  StorageRefine cells({
  { 0, 7, 8, 4 },
  { 7, 3, 6, 8 },
  { 6, 2, 5, 8 },
  { 5, 1, 4, 8 },
  });
  StorageRefine child_faces({
  // pas testé
  { 3, 11 },
  { 10, 8 },
  { 5, 7 },
  { 0, 4 },
  });
  return { IT_Quad4, IT_Line2, IT_Quad4, nodes, faces, cells, child_faces };
}

/*
 * Pour un quad:
 * 3 --- 2
 * |     |
 * |     |
 * 0 --- 1
 * 
 * 3 --> 2
 * |   / |
 * | /   |
 * 0 <-- 1
 * 
 * 3 --- 4 --- 5
 * |     |     |
 * |     |     |
 * 0 --- 1 --- 2 
 * Ici on ajoute une seule face arcane (0,2). 
*/
MeshSubdivider::Pattern PatternBuilder::quadtotri()
{
  StorageRefine nodes({}); // Pas de noeud à ajouter
  StorageRefine faces({
  /*{0,1},
      {1,3},
      {2,3},*/
  { 0, 2 },
  //{0,3}
  });
  StorageRefine cells({ { 0, 3, 2 }, { 2, 1, 0 } });
  StorageRefine child_faces({});
  return { IT_Quad4, IT_Line2, IT_Triangle3, nodes, faces, cells, child_faces };
}

/*
  Numérotation nouveaux noeuds
         2
        / \
       /   \
      5 --- 4
     / \   / \
    /   \ /   \
   0 --- 3 --- 1

  Numérotation arêtes
           2 
          / \
         6   7 
        /     \
       + --8-- +
      / \     / \
     1   2   5   3    
    /     \ /     \
   + --0-- + --4-- +
  */

MeshSubdivider::Pattern PatternBuilder::tritotri()
{
  StorageRefine nodes({
  { 0, 1 }, // 3
  { 1, 2 }, // 4
  { 2, 0 }, // 5
  });
  StorageRefine faces({
  { 0, 3 },
  { 0, 5 },
  { 3, 5 },
  { 1, 4 },
  { 1, 3 },
  { 3, 4 },
  { 2, 5 },
  { 2, 4 },
  { 4, 5 },
  });
  StorageRefine cells({ { 3, 0, 5 },
                        { 4, 1, 3 },
                        { 5, 2, 4 },
                        { 3, 5, 4 } });
  StorageRefine child_faces(
  { { 0, 4 },
    { 3, 7 },
    { 1, 6 } });
  return { IT_Triangle3, IT_Line2, IT_Triangle3, nodes, faces, cells, child_faces };
}

MeshSubdivider::Pattern PatternBuilder::tritoquad()
{
  StorageRefine nodes({
  { 0, 1 }, // 3
  { 1, 2 }, // 4
  { 2, 0 }, // 5
  { 0, 1, 2 }, // 6
  });
  StorageRefine faces({

  });
  StorageRefine cells({
  { 0, 3, 6, 5 },
  { 1, 4, 6, 3 },
  { 2, 5, 6, 4 },
  });
  StorageRefine child_faces({});
  return { IT_Triangle3, IT_Line2, IT_Quad4, nodes, faces, cells, child_faces };
}

// N'utilise pas la numérotation des faces de arcane pour l'instant
MeshSubdivider::Pattern PatternBuilder::hextohex()
{
  StorageRefine nodes = {
    { 0, 1 }, // 8  // Sur arêtes
    { 0, 3 }, // 9
    { 0, 4 }, // 10
    { 1, 2 }, // 11
    { 1, 5 }, // 12
    { 2, 3 }, // 13
    { 2, 6 }, // 14
    { 3, 7 }, // 15
    { 4, 5 }, // 16
    { 4, 7 }, // 17
    { 5, 6 }, // 18
    { 6, 7 }, // 19
    { 0, 1, 2, 3 }, // 20 // Sur faces
    { 0, 1, 5, 4 }, // 21
    { 0, 4, 7, 3 }, // 22
    { 1, 5, 6, 2 }, // 23
    { 2, 3, 7, 6 }, // 24
    { 4, 5, 6, 7 }, // 25
    { 0, 1, 5, 4, 3, 2, 7, 6 } // 26 // Centroid
  };
  StorageRefine faces = {
    // Externes
    { 0, 8, 20, 9 }, // Derrière // 0 1 2 3  // 0
    { 9, 20, 13, 3 },
    { 8, 1, 11, 20 },
    { 20, 11, 2, 13 },
    { 0, 10, 22, 9 }, // Gauche // 0 3 7 4 // 1
    { 9, 22, 15, 3 },
    { 10, 4, 17, 22 },
    { 22, 17, 7, 15 },
    { 4, 16, 21, 10 }, // Bas // 4 5 0 1 // 2
    { 10, 21, 8, 0 },
    { 16, 5, 12, 21 },
    { 21, 12, 1, 8 },
    { 4, 16, 25, 17 }, // Devant // 4 5 6 7 // 3
    { 17, 25, 19, 7 },
    { 16, 5, 18, 25 },
    { 25, 18, 6, 19 },
    { 1, 12, 23, 11 }, // Droite // 1 2 5 6 // 4
    { 11, 23, 14, 2 },
    { 12, 5, 18, 23 },
    { 23, 18, 6, 14 },
    { 7, 19, 24, 15 }, // Haut // 7 6 2 3 // 5
    { 19, 6, 14, 24 },
    { 15, 24, 13, 3 },
    { 24, 14, 2, 13 },
    // Internes
    { 8, 20, 26, 21 },
    { 20, 13, 24, 26 },
    { 9, 22, 26, 20 },
    { 20, 26, 23, 11 },
    { 21, 16, 25, 26 },
    { 26, 25, 19, 24 },
    { 22, 17, 25, 26 },
    { 26, 25, 18, 23 },
    { 10, 21, 26, 22 },
    { 21, 12, 23, 26 },
    { 22, 26, 24, 15 },
    { 26, 23, 14, 24 },

  };
  StorageRefine child_faces = {
    { 0, 1, 2, 3 },
    { 4, 5, 6, 7 },
    { 8, 9, 10, 11 },
    { 12, 13, 14, 15 },
    { 16, 17, 18, 19 },
    { 20, 21, 22, 23 }
  };
  StorageRefine cells = {
    { 0, 8, 20, 9, 10, 21, 26, 22 },
    { 10, 21, 26, 22, 4, 16, 25, 17 },
    { 8, 1, 11, 20, 21, 12, 23, 26 },
    { 21, 12, 23, 26, 16, 5, 18, 25 },
    { 9, 20, 13, 3, 22, 26, 24, 15 },
    { 22, 26, 24, 15, 17, 25, 19, 7 },
    { 20, 11, 2, 13, 26, 23, 14, 24 },
    { 26, 23, 14, 24, 25, 18, 6, 19 }
  };
  return { IT_Hexaedron8, IT_Quad4, IT_Hexaedron8, nodes, faces, cells, child_faces };
}

MeshSubdivider::Pattern PatternBuilder::tettotet()
{
  StorageRefine nodes = {
    { 0, 1 }, // 4
    { 1, 2 }, // 5
    { 0, 2 }, // 6
    { 0, 3 }, // 7
    { 2, 3 }, // 8
    { 1, 3 }, // 9
  };

  StorageRefine faces = {
    { 0, 4, 6 }, // 0
    { 0, 6, 7 }, // 1
    { 0, 4, 7 }, // 2
    { 4, 6, 7 }, // 3
    { 1, 4, 5 }, // 4
    { 4, 5, 9 }, // 5
    { 1, 4, 9 }, // 6
    { 1, 5, 9 }, // 7
    { 2, 5, 6 }, // 8
    { 2, 6, 8 }, // 9
    { 5, 6, 8 }, // 10
    { 2, 5, 8 }, // 11
    { 7, 8, 9 }, // 12
    { 3, 7, 8 }, // 13
    { 3, 7, 9 }, // 14
    { 3, 8, 9 }, // 15
    { 4, 7, 9 }, // 16
    { 4, 6, 9 }, // 17
    { 6, 7, 9 }, // 18
    { 4, 5, 6 }, // 19
    { 5, 6, 9 }, // 20
    { 6, 8, 9 }, // 21
    { 6, 7, 8 }, // 22
    { 5, 8, 9 }, // 23
  };
  StorageRefine child_faces = {
    { 0, 19, 4, 8 },
    { 1, 22, 13, 9 },
    { 2, 16, 6, 14 },
    { 11, 23, 7, 15 }
  };
  StorageRefine cells = {
    { 0, 4, 6, 7 },
    { 4, 1, 5, 9 },
    { 6, 5, 2, 8 },
    { 7, 9, 8, 3 },
    { 4, 6, 7, 9 },
    { 4, 9, 5, 6 },
    { 6, 7, 9, 8 },
    { 6, 8, 9, 5 }
  };
  return { IT_Tetraedron4, IT_Triangle3, IT_Tetraedron4, nodes, faces, cells, child_faces };
}

// Attention lors de la génération des faces, il ne faut pas utiliser le cartesian (l'ordre du cartesian builder est différent)
MeshSubdivider::Pattern PatternBuilder::hextotet()
{
  StorageRefine nodes = {}; // Pas de nouveaux noeuds
  StorageRefine faces = {
    // Ne fonctionne pas avec les même faces que arcane pourtant
    { 0, 1, 3 }, // 0
    { 0, 3, 4 }, // 1
    { 0, 1, 4 }, // 2
    { 1, 3, 4 }, // 3
    { 1, 4, 5 }, // 4
    { 1, 5, 6 }, // 5
    { 1, 4, 6 }, // 6
    { 4, 5, 6 }, // 7
    { 1, 2, 3 }, // 8
    { 1, 3, 6 }, // 9
    { 1, 2, 6 }, // 10
    { 2, 3, 6 }, // 11
    { 3, 4, 6 }, // 12
    { 3, 6, 7 }, // 13
    { 3, 4, 7 }, // 14
    { 4, 6, 7 }, // 15
  };

  StorageRefine child_faces = { // 6*2 = 12 faces
                                { 0, 8 },
                                { 1, 14 },
                                { 2, 4 },
                                { 15, 7 },
                                { 10, 5 },
                                { 11, 13 }
  };
  StorageRefine cells = {
    { 0, 1, 3, 4 },
    { 1, 4, 5, 6 },
    { 1, 2, 3, 6 },
    { 3, 4, 6, 7 },
    { 1, 3, 4, 6 }
  };
  return { IT_Hexaedron8, IT_Triangle3, IT_Tetraedron4, nodes, faces, cells, child_faces };
}

MeshSubdivider::Pattern PatternBuilder::tettohex()
{
  StorageRefine nodes = {
    { 0, 1 }, // 4
    { 1, 2 }, // 5
    { 0, 2 }, // 6
    { 0, 3 }, // 7
    { 2, 3 }, // 8
    { 1, 3 }, // 9
    { 0, 1, 3 }, // 10
    { 1, 2, 3 }, // 11
    { 0, 1, 2 }, // 12
    { 0, 2, 3 }, // 13
    { 0, 1, 2, 3 }, // 14
  };
  StorageRefine faces = {
    { 7, 10, 14, 13 }, // 0 x
    { 0, 6, 13, 7 }, // 1
    { 0, 4, 10, 7 }, // 2
    { 0, 4, 12, 6 }, // 3
    { 4, 10, 14, 12 }, // 4 x
    { 6, 12, 14, 13 }, // 5 x
    { 2, 5, 11, 8 }, // 6
    { 2, 6, 13, 8 }, // 7
    { 8, 11, 14, 13 }, // 8 x
    { 5, 11, 14, 12 }, // 9 x
    { 2, 5, 12, 6 }, // 10
    { 3, 8, 11, 9 }, // 11
    { 3, 7, 13, 8 }, // 12
    { 3, 7, 10, 9 }, // 13
    { 9, 10, 14, 11 }, // 14 x
    { 1, 5, 11, 9 }, // 15
    { 1, 4, 10, 9 }, // 16
    { 1, 4, 12, 5 }, // 17
  };
  StorageRefine child_faces = {
    /*{0,10,17},
      {1,6,11},
      {2,13,15},
      {8,12,16}*/
    { 3, 10, 17 },
    { 1, 7, 12 },
    { 2, 13, 16 },
    { 6, 11, 15 },
  };
  StorageRefine cells = {
    /*{0,4,12,6,7,10,14,13},
        {10,4,12,14,9,1,5,11},
        {13,14,12,6,8,11,5,2},
        {7,10,14,13,3,9,11,8},*/
    /*{13,14,10,7,6,12,4,0},
        {2,5,11,8,6,12,14,13},
        {8,11,9,3,13,14,10,7},
        {11,5,1,9,14,12,4,10},*/
    /*{6,12,4,0,13,14,10,7},
        {6,12,14,13,2,5,11,8},
        {13,14,10,7,8,11,9,3},
        {14,12,4,10,11,5,1,9},*/
    { 7, 10, 14, 13, 0, 4, 12, 6 },
    { 8, 11, 5, 2, 13, 14, 12, 6 },
    { 3, 9, 11, 8, 7, 10, 14, 13 },
    { 9, 1, 5, 11, 10, 4, 12, 14 }
  };
  return { IT_Tetraedron4, IT_Quad4, IT_Hexaedron8, nodes, faces, cells, child_faces };
}

MeshSubdivider::Pattern PatternBuilder::hextotet24()
{
  StorageRefine nodes({
  { 0, 1, 2, 3 }, // 8
  { 0, 3, 7, 4 }, // 9
  { 0, 1, 4, 5 }, // 10
  { 4, 5, 6, 7 }, // 11
  { 1, 2, 5, 6 }, // 12
  { 2, 3, 7, 6 }, // 13
  { 0, 1, 2, 3, 4, 5, 6, 7 }, // 14
  });
  StorageRefine faces({
  // OK
  { 0, 3, 8 }, // 0
  { 0, 8, 14 }, // 1
  { 0, 3, 14 }, // 2
  { 3, 8, 14 }, // 3
  { 2, 3, 8 }, // 4
  { 2, 3, 14 }, // 5
  { 2, 8, 14 }, // 6
  { 1, 2, 8 }, // 7
  { 1, 2, 14 }, // 8
  { 1, 8, 14 }, // 9
  { 0, 1, 8 }, // 10
  { 0, 1, 14 }, // 11
  { 0, 4, 9 }, // 12
  { 0, 9, 14 }, // 13
  { 0, 4, 14 }, // 14
  { 4, 9, 14 }, // 15
  { 4, 7, 9 }, // 16
  { 4, 7, 14 }, // 17
  { 7, 9, 14 }, // 18
  { 3, 7, 9 }, // 19
  { 3, 7, 14 }, // 20
  { 3, 9, 14 }, // 21
  { 0, 3, 9 }, // 22
  { 0, 4, 10 }, // 23
  { 4, 10, 14 }, // 24
  { 0, 10, 14 }, // 25
  { 0, 1, 10 }, // 26
  { 1, 10, 14 }, // 27
  { 1, 5, 10 }, // 28
  { 1, 5, 14 }, // 29
  { 5, 10, 14 }, // 30
  { 4, 5, 10 }, // 31
  { 4, 5, 14 }, // 32
  { 4, 7, 11 }, // 33
  { 7, 11, 14 }, // 34
  { 4, 11, 14 }, // 35
  { 4, 5, 11 }, // 36
  { 5, 11, 14 }, // 37
  { 5, 6, 11 }, // 38
  { 5, 6, 14 }, // 39
  { 6, 11, 14 }, // 40
  { 6, 7, 11 }, // 41
  { 6, 7, 14 }, // 42
  { 1, 5, 12 }, // 43
  { 5, 12, 14 }, // 44
  { 1, 12, 14 }, // 45
  { 1, 2, 12 }, // 46
  { 2, 12, 14 }, // 47
  { 2, 6, 12 }, // 48
  { 2, 6, 14 }, // 49
  { 6, 12, 14 }, // 50
  { 5, 6, 12 }, // 51
  { 2, 6, 13 }, // 52
  { 6, 13, 14 }, // 53
  { 2, 13, 14 }, // 54
  { 2, 3, 13 }, // 55
  { 3, 13, 14 }, // 56
  { 3, 7, 13 }, // 57
  { 7, 13, 14 }, // 58
  { 6, 7, 13 }, // 59
  });
  StorageRefine cells({
  { 0, 3, 8, 14 },
  { 3, 2, 8, 14 },
  { 2, 1, 8, 14 },
  { 1, 0, 8, 14 },

  { 0, 4, 9, 14 },
  { 4, 7, 9, 14 },
  { 7, 3, 9, 14 },
  { 3, 0, 9, 14 },

  { 4, 0, 10, 14 },
  { 0, 1, 10, 14 },
  { 1, 5, 10, 14 },
  { 5, 4, 10, 14 },

  { 7, 4, 11, 14 },
  { 4, 5, 11, 14 },
  { 5, 6, 11, 14 },
  { 6, 7, 11, 14 },

  { 5, 1, 12, 14 },
  { 1, 2, 12, 14 },
  { 2, 6, 12, 14 },
  { 6, 5, 12, 14 },

  { 6, 2, 13, 14 },
  { 2, 3, 13, 14 },
  { 3, 7, 13, 14 },
  { 7, 6, 13, 14 },
  });

  StorageRefine child_faces({
  // a refaire ancienne numérotation
  { 0, 4, 7, 10 },
  { 12, 16, 19, 22 },
  { 23, 26, 28, 31 },

  { 33, 36, 38, 41 },
  { 43, 46, 48, 51 },
  { 52, 55, 57, 59 },
  });
  return { IT_Hexaedron8, IT_Triangle3, IT_Tetraedron4, nodes, faces, cells, child_faces };
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*!
 * \brief Service Arcane un maillage du jeu de données.
 */
class ArcaneBasicMeshSubdividerService
: public ArcaneArcaneBasicMeshSubdividerServiceObject
{
 public:

  explicit ArcaneBasicMeshSubdividerService(const ServiceBuildInfo& sbi);
  //! Raffine le maillage un nombre nb-subdivision
  void subdivideMesh([[maybe_unused]] IPrimaryMesh* mesh) override;

 private:

  void _init();
  //! Calcule le unique id en fonction des noeuds de node_uid
  static UniqueArray<Int64> _computeNodeUid(UniqueArray<Int64> nodes_uid, const StorageRefine& node_pattern);
  /*
  void _computeNodeCoord();
  void _computeNodeUid();
  void _computeFaceUid();
  void _computeCellUid();
  void _processOwner();
  void _setOwner();
  void _processOwnerCell();
  void _processOwnerFace();
  void _processOwnerNode();
  void _getRefinePattern(Int16 type);
  void _execute();
  */

  //! Génère l'ordre des faces arcane pour tout les motifs. 
  void _faceOrderArcane(IPrimaryMesh* mesh); 
  //! Raffine en utilisant les faces d'arcane et le motif (Pattern) p
  /* Méthode qui permet de récuperer les faces générés par arcanes.
  * Ces faces doivent donner les indices locaux des noeuds de la cellule initiale.
  * Pour ça on construit une map global to my_local_index. <Int64,Int64>
  */
  void _refineWithArcaneFaces(IPrimaryMesh* mesh, MeshSubdivider::Pattern p);
  //! Génère un triangle
  void _generateOneTri(IPrimaryMesh* mesh);
  //! Génère un quadrilatère
  void _generateOneQuad(IPrimaryMesh* mesh);
  //! Génère un tetraèdre
  void _generateOneTetra(IPrimaryMesh* mesh);
  //! Génère un hexaèdre
  void _generateOneHexa(IPrimaryMesh* mesh);
  //! Raffine une fois le maillage avec les motifs présent dans le pattern_manager
  void _refineOnce([[maybe_unused]] IPrimaryMesh* mesh, std::unordered_map<Arccore::Int16, MeshSubdivider::Pattern>& pattern_manager);
  //! Methode pour avoir la manière dont sont crée les nouvelles faces pour un élément. Utile pour remplir les tableau child faces dans le PatternBuilder
  void _getArcaneOrder(IPrimaryMesh* mesh);
  //! Methode pour obtenir les fichier vtk pour les différent pattern disponibles en 2D
  void _generatePattern2D(IPrimaryMesh* mesh);
  //! Methode pour obtenir les fichier vtk pour les différent pattern disponibles en 3D
  void _generatePattern3D(IPrimaryMesh* mesh);
  //! Méthode pour générer les motifs sur un seul élément de base
  void _generatePattern(IPrimaryMesh* mesh);
};

void ArcaneBasicMeshSubdividerService::_generatePattern(IPrimaryMesh* mesh)
{
  if (mesh->dimension() == 2) {
    _generatePattern2D(mesh);
  }
  else if (mesh->dimension() == 3) {
    _generatePattern3D(mesh);
  }
}

void ArcaneBasicMeshSubdividerService::_generatePattern2D(IPrimaryMesh* mesh)
{
  std::unordered_map<Arccore::Int16, MeshSubdivider::Pattern> pattern_manager;

  mesh->faceFamily()->destroyGroups();
  _generateOneTri(mesh);
  pattern_manager[IT_Triangle3] = PatternBuilder::tritotri();
  _refineOnce(mesh, pattern_manager);
  std::string prefix("subdivider_pattern2D_");
  mesh->utilities()->writeToFile(prefix + "tritotri.vtk", "VtkLegacyMeshWriter");

  mesh->faceFamily()->destroyGroups();
  _generateOneTri(mesh);
  pattern_manager[IT_Triangle3] = PatternBuilder::tritoquad();
  _refineOnce(mesh, pattern_manager);
  mesh->utilities()->writeToFile(prefix + "tritoquad.vtk", "VtkLegacyMeshWriter");

  mesh->faceFamily()->destroyGroups();
  _generateOneQuad(mesh);
  pattern_manager[IT_Quad4] = PatternBuilder::quadtoquad();
  _refineOnce(mesh, pattern_manager);
  mesh->utilities()->writeToFile(prefix + "quadtoquad.vtk", "VtkLegacyMeshWriter");

  mesh->faceFamily()->destroyGroups();
  _generateOneQuad(mesh);
  pattern_manager[IT_Quad4] = PatternBuilder::quadtotri();
  _refineOnce(mesh, pattern_manager);
  mesh->utilities()->writeToFile(prefix + "quadtotri.vtk", "VtkLegacyMeshWriter");
}

void ArcaneBasicMeshSubdividerService::_generatePattern3D(IPrimaryMesh* mesh)
{
  std::unordered_map<Arccore::Int16, MeshSubdivider::Pattern> pattern_manager;
  std::string prefix("subdivider_pattern3D_");
  mesh->faceFamily()->destroyGroups();
  _generateOneTetra(mesh);
  pattern_manager[IT_Tetraedron4] = PatternBuilder::tettotet();
  _refineOnce(mesh, pattern_manager);
  mesh->utilities()->writeToFile(prefix + "tettotet.vtk", "VtkLegacyMeshWriter");

  mesh->faceFamily()->destroyGroups();
  _generateOneTetra(mesh);
  pattern_manager[IT_Tetraedron4] = PatternBuilder::tettohex();
  _refineOnce(mesh, pattern_manager);
  mesh->utilities()->writeToFile(prefix + "tettohex.vtk", "VtkLegacyMeshWriter");

  mesh->faceFamily()->destroyGroups();
  _generateOneHexa(mesh);
  pattern_manager[IT_Hexaedron8] = PatternBuilder::hextohex();
  _refineOnce(mesh, pattern_manager);
  mesh->utilities()->writeToFile(prefix + "hextohex.vtk", "VtkLegacyMeshWriter");

  mesh->faceFamily()->destroyGroups();
  _generateOneHexa(mesh);
  pattern_manager[IT_Hexaedron8] = PatternBuilder::hextotet24();
  _refineOnce(mesh, pattern_manager);
  mesh->utilities()->writeToFile(prefix + "hextotet24.vtk", "VtkLegacyMeshWriter");

  mesh->faceFamily()->destroyGroups();
  _generateOneHexa(mesh);
  pattern_manager[IT_Hexaedron8] = PatternBuilder::hextotet();
  _refineOnce(mesh, pattern_manager);
  mesh->utilities()->writeToFile(prefix + "subdivider_hextotet.vtk", "VtkLegacyMeshWriter");
}

/*void ArcaneBasicMeshSubdividerService::_getArcaneOrder(IPrimaryMesh* mesh){
  Tritotri generate faces 
  
  _generateOneTri(mesh);
  pattern_manager[IT_Triangle3] = PatternBuilder::tritotri();
  _refineOnce(mesh,pattern_manager);
  
  
   Quadtoquad generate faces
  
  _generateOneQuad(mesh);
  _refineWithArcaneFaces(mesh,PatternBuilder::quadtoquad());
  pattern_manager[IT_Quad4] = PatternBuilder::quadtoquad();
  _refineOnce(mesh,pattern_manager);
  
  
   Tettotet generate faces
  
  _generateOneTetra(mesh);
  _refineWithArcaneFaces(mesh,PatternBuilder::tettotet());
  
  
  
  _faceOrderArcane(mesh); 
  mesh_utils::writeMeshInfos(mesh,"meshInSubdivide");
  pattern_manager[IT_Triangle3] = PatternBuilder::tritotri();
  _refineOnce(mesh,pattern_manager);
  mesh_utils::writeMeshInfos(mesh,"meshOutSubdivide");
  

  Tettohex generate faces input one tet
  
  _generateOneTetra(mesh);
  _refineWithArcaneFaces(mesh,PatternBuilder::tettohex());
  
  Hextohex generate faces input one hex 
  _refineWithArcaneFaces(mesh,PatternBuilder::hextotet());
  
  pattern_manager[IT_Hexaedron8] = PatternBuilder::hextotet();
  _refineOnce(mesh,pattern_manager);
  
}*/

void ArcaneBasicMeshSubdividerService::_refineOnce(IPrimaryMesh* mesh, std::unordered_map<Arccore::Int16, MeshSubdivider::Pattern>& pattern_manager)
{

  info() << "#subdivide mesh";
  // On vérifie qu'on sait raffiner les cellules (peut-être pas en même temps)
  ENUMERATE_CELL (icell, mesh->ownCells()) {
    const Cell& cell = *icell;
    // Liste des éléments où les patterns sont implémentés pour l'instant
    if (cell.itemTypeId() != IT_Hexaedron8 && cell.itemTypeId() != IT_Tetraedron4 && cell.itemTypeId() != IT_Quad4 && cell.itemTypeId() != IT_Triangle3) {
      ARCANE_FATAL("Not implemented item type '{0}'", cell.itemTypeId());
      return;
    }
  }

  info() << "subdivide mesh with " << options()->nbSubdivision() << " nb_refine";

  Int32 my_rank = mesh->parallelMng()->commRank();
  IMeshModifier* mesh_modifier = mesh->modifier();
  IGhostLayerMng* gm = mesh->ghostLayerMng();
  debug() << "PART 3D nb ghostlayer" << gm->nbGhostLayer();
  // mesh->utilities()->writeToFile(String::format("3D_last_input{0}.vtk",my_rank), "VtkLegacyMeshWriter");
  Integer nb_ghost_layer_init = gm->nbGhostLayer(); // On garde le nombre de ghost layer initial pour le remettre à la fin
  Int32 version = gm->builderVersion();
  if (version < 3)
    gm->setBuilderVersion(3);

  gm->setNbGhostLayer(0);
  mesh_modifier->setDynamic(true);
  mesh_modifier->updateGhostLayers();

  // Uniquement pour les vérifications asserts à la fin
  Integer nb_cell_init = mesh->nbCell();
  Integer nb_face_init = mesh->nbFace();
  //Integer nb_edge_init = mesh->nbEdge();
  //Integer nb_node_init = mesh->nbNode();

  // Compter les arêtes
  // On cherche un moyen de compter les arêtes pour faire un test facile sur le nombre de noeud inséré.
  // ARCANE_ASSERT((nb_edge_init+ nb_cell_init + nb_face_init)== nb_node_added,("Mauvais nombre de noeuds insérés"));
  //debug() << "#NOMBRE INITS " << nb_node_init << " " << mesh->allEdges().size() << " " << edg.size() << " " << nb_face_init << " " << nb_cell_init ;

  // VARIABLES
  // Items à ajouter avec connectivités pour E F et C
  UniqueArray<Int64> nodes_to_add;
  UniqueArray<Int64> edges_to_add;
  UniqueArray<Int64> faces_to_add;
  UniqueArray<Int64> cells_to_add;

  Integer nb_cell_to_add = 0;
  Integer nb_face_to_add = 0;

  VariableNodeReal3& nodes_coords = mesh->nodesCoordinates();
  std::unordered_map<Int64, Real3> nodes_to_add_coords;
  debug() << "ARRAY SIZE " << nodes_coords.arraySize();
  // Noeuds sur les entités
  std::set<Int64> new_nodes; // Utiliser une map permet s'assurer qu'on ajoute une seule fois un noeud avec un uniqueId()
  std::set<Int64> new_faces; // ^--- Pareil pour les faces
  // Maps pour la gestions des propriétaires
  std::unordered_map<Int64, Int32> node_uid_to_owner;
  std::unordered_map<Int64, Int32> edge_uid_to_owner; // pas utilisé
  std::unordered_map<Int64, Int32> face_uid_to_owner;
  std::unordered_map<Int64, Int32> child_cell_owner; // pas utilisé
  std::unordered_map<Int32, Int32> old_face_lid_to_owner; // pas utilisé

  UniqueArray<Int32> cells_to_detach; // Cellules à détacher
  UniqueArray<Int64> faces_uids; // Contient uniquement les uids pas les connectivités
  UniqueArray<Int64> edges_uids; // Idem

  // Calcul nombre de noeuds à insérer
  // const Integer nb_node_to_add_total = mesh->nbCell()+mesh->nbFace()+mesh->nbEdge(); // Attention pattern dépendant
  //nodes_to_add.reserve(nb_node_to_add_total);
  //nodes_to_add_coords.reserve(nb_node_to_add_total);

  Integer ind_new_cell = 0;

  ARCANE_ASSERT((mesh->nbEdge() == 0), ("Wrong number of edge"));

  UniqueArray<Int64> parent_faces(mesh->ownFaces().size());
  UniqueArray<Int64> parent_cells(mesh->ownCells().size());
  UniqueArray<Int64> child_cells; // Toutes les nouvelles cells
  UniqueArray<Int64> child_faces; // Au bord de l'élément uniquement (pas de faces interne à l'élément)

  // Permet de récupere les entités enfants à partir d'une cellule parent
  std::unordered_map<Int64, std::pair<Int64, Int64>> parents_to_childs_cell; // A partir d'un uid on récupère le premier enfant (pair<index,number of child>)
  std::unordered_map<Int64, std::pair<Int64, Int64>> parents_to_childs_faces; // A partir d'un uid on récupère le premier enfant (pair<index,number of child>)
  // ^--- uniquement pour les faces "externes"
  Int64 childs_count = 0; // A chaque nouvelle cellule on se décale du nombre d'enfant (  +4 si quad, +3 ou +4 pour les tri selon le pattern )

  // Groups
  std::unordered_map<Int64, std::pair<Int64, Int64>> parents_to_childs_faces_groups; // Map face parente -> face enfant externe dans array face_external_uid
  UniqueArray<Int64> face_external_uid; // Toutes les faces externes du proc uid

  // Debug node
  /*
  ENUMERATE_NODE(inode,mesh->ownNodes())
  {
    const Node & node = *inode;
    nodes_to_add_coords[node.uniqueId().asInt64()] = nodes_coords[node];
  }
  */
  // Traitement pour une cellule²
  ENUMERATE_CELL (icell, mesh->ownCells()) {
    debug() << "Refining element";
    // POUR UN ELEMNT:
    // Détacher cellules parente
    // Génération des nouveaux noeuds (uid et coordonnées)
    // Sur Arêtes
    // Sur Faces
    // Sur Cellule
    // Nouveaux noeuds,coordonnées

    // Génération des Faces (uid et composants (Noeuds)) utilises nouveaux noeuds
    // Internes
    // Externes

    // Génération des Cellules (uid et composants (Noeuds))
    // Gestion des groupes
    // FIN UN ELEMENT

    // Détachement des cellules
    // Ajout des noeuds enfants
    // Ajout des faces enfants
    // Ajout des cellules enfants (et assignation propriétaire)

    // Ajout d'une couche fantome
    // Calcul des propriétaires des noeuds
    // Calcul des propriétaires des faces
    // Supression de la couche fantome
    // ?? Calcul des groupes F C

    // Assignation des noeuds au propriétaire
    // Assignation des faces au propriétaire

    const Cell& cell = *icell;

    MeshSubdivider::Pattern& p = pattern_manager[cell.type()]; // Pattern Manager
    StorageRefine& node_pattern = p.nodes;

    UniqueArray<Int64> face_in_cell; // Toutes les faces de la cellule uid
    StorageRefine& child_faces = p.child_faces;

    cells_to_detach.add(cell.localId());
    // Génération des noeud
    UniqueArray<Int64> node_in_cell;
    node_in_cell.resize(node_pattern.size() + cell.nbNode()); // pattern dépendant
    UniqueArray<Real3> coords_in_cell;
    debug() << "Noeuds initiaux";
    // Noeuds initiaux
    for (Int32 i = 0; i < cell.nbNode(); i++) {
      node_in_cell[i] = cell.node(static_cast<Int32>(i)).uniqueId().asInt64();
      //debug() << i << " " << node_in_cell[i] << " size " << node_in_cell.size() ;
    }

    Integer index_27 = cell.nbNode();

    // - Génération des uid noeuds
    debug() << "Génération des uid noeuds";

    for (Integer i = 0; i < node_pattern.size(); i++) {
      // uid
      UniqueArray<Int64> tmp;
      for (Integer j = 0; j < node_pattern[i].size(); j++) {
        tmp.add(node_in_cell[node_pattern[i][j]]);
      }
      std::sort(tmp.begin(), tmp.end());
      Int64 uid = MeshUtils::generateHashUniqueId(tmp.constView());
      node_in_cell[index_27 + i] = uid;

      if (new_nodes.find(node_in_cell[i + index_27]) == new_nodes.end()) {
        // Coords
        Arcane::Real3 middle_coord(0.0, 0.0, 0.0);
        for (Integer j = 0; j < node_pattern[i].size(); j++) {
          info() << "loop" << cell.node(static_cast<Integer>(node_pattern[i][j])) << " " << nodes_coords[cell.node(static_cast<Integer>(node_pattern[i][j]))];
          middle_coord += nodes_coords[cell.node(static_cast<Integer>(node_pattern[i][j]))];
        }
        if (node_pattern[i].size() == 0) {
          ARCANE_FATAL("Wrong size for refined pattern with code IT:'{0}'", p.cell_type);
        }
        middle_coord /= node_pattern[i].size();
        if (middle_coord == Real3(0.0, 0.0, 0.0)) {
          ARCANE_FATAL("Bad coordinate for new node, the node '{0}' probably has a default coordinate (0.0,0.0,0.0).", uid);
        }
        new_nodes.insert(node_in_cell[i + index_27]);
        // Insertion dans map uid -> coord
        nodes_to_add_coords[node_in_cell[i + index_27]] = middle_coord;
        // Insertion dans Uarray uid
        nodes_to_add.add(uid);
        //info() << node_pattern[i];
        //debug() << i << " " << uid << " sizenic " << tmp << middle_coord;
      }
    }
    // Est-ce qu'on doit ajouter les anciens noeuds ? Normalement non
    // #TAG
    /*
      for(Integer i = 0 ; i < cell.nbNode() ; i++ ) {
        nodes_to_add.add(cell.node(i).uniqueId().asInt64());
        new_nodes.insert(cell.node(i).uniqueId().asInt64());
      }
      */

    debug() << "nodetoadd size " << nodes_to_add.size() << " " << nodes_to_add_coords.size();
    debug() << "Node coord & nb node to add" << nodes_to_add_coords.size() << " " << nodes_to_add.size();
    //ARCANE_ASSERT((nodes_to_add_coords.size() == static_cast<size_t>(nodes_to_add.size())),("Has to be same"));
    //ARCANE_ASSERT((nodes_to_add_coords.size() == new_nodes.size()),("Has to be same"));

    // Génération des Faces
    StorageRefine& face_refine = p.faces;
    debug() << "face_refine.size() " << face_refine.size();
    //ARCANE_ASSERT((face_refine.size() == 36), ("WRONG NUMBER OF CELL ADDED")); // One cube assert
    debug() << "Refine face";
    for (Integer i = 0; i < face_refine.size(); i++) {
      // Generation du hash de la face
      UniqueArray<Int64> tmp;
      //tmp.resize(face_refine[i].size());
      for (Integer j = 0; j < face_refine[i].size(); j++) {
        tmp.add(node_in_cell[face_refine[i][j]]);
      }
      std::sort(tmp.begin(), tmp.end());
      //ARCANE_ASSERT(( tmp.size() == 4 ),("Wrong size of UniqueArray")); // one cube assert
      Int64 uid = Arcane::MeshUtils::generateHashUniqueId(tmp.constView());
      face_in_cell.add(uid); // Pour groups
      // Vérifier si on l'a déja crée
      if (new_faces.find(uid) == new_faces.end()) {
        // Ajout
        faces_to_add.add(p.face_type);
        faces_to_add.add(uid);
        //debug() << "Face " << uid << " " << tmp ;
        for (Integer j = 0; j < face_refine[i].size(); j++) {
          //debug() << node_in_cell[face_refine[i][j]] ;
          faces_to_add.add(node_in_cell[face_refine[i][j]]);
        }
        // Ajouter dans tableau uids faces
        faces_uids.add(uid);
        nb_face_to_add++;
        new_faces.insert(uid);
      }
    }

    // Nouvelle Gestion des groupes
    // Pour chaque faces
    // Générer hash
    // associer hash uid face
    // Parcours des faces parentes
    debug() << "Gestion face groupe";
    for (Integer i = 0; i < child_faces.size(); i++) {
      parents_to_childs_faces_groups[cell.face(i).uniqueId()] = std::pair<Int64, Int64>(face_external_uid.size(), child_faces[i].size());
      for (Integer j = 0; j < child_faces[i].size(); j++) {
        face_external_uid.add(face_in_cell[child_faces[i][j]]); // start c'est l'index des face_in_cell // en fait on fait un array pour chaque élément aussi // face_in_cell (sinon les index seront faux donc on ajoute plusieurs fois les faces)
      }
    }

    // Génération des cells
    StorageRefine& cells_refine = p.cells;
    // Génération des cellules enfants
    debug() << "Génération des cellules enfants";
    // L'uid est généré à partir du hash de chaque noeuds triés par ordre croissant
    for (Integer i = 0; i < cells_refine.size(); i++) {
      // Le nouvel uid est généré avec le hash des nouveaux noeuds qui composent la nouvelle cellule
      UniqueArray<Int64> tmp;
      for (Integer j = 0; j < cells_refine[i].size(); j++) {
        tmp.add(node_in_cell[cells_refine[i][j]]);
      }
      std::sort(tmp.begin(), tmp.end());
      Int64 cell_uid = Arcane::MeshUtils::generateHashUniqueId(tmp.constView()); //max_cell_uid+ind_new_cell;

      cells_to_add.add(p.cell_type); // Type
      cells_to_add.add(cell_uid); // TODO CHANGER par max_uid + cell_uid * max_nb_node
      for (Integer j = 0; j < cells_refine[i].size(); j++) {
        cells_to_add.add(node_in_cell[cells_refine[i][j]]);
      }
      child_cell_owner[cell_uid] = cell.owner();
      parent_cells.add(cell.uniqueId());
      child_cells.add(cell_uid); // groups doublons d'informations avec cells_to_add
      nb_cell_to_add++;
      ind_new_cell++;
    }
    // groups
    parents_to_childs_cell[cell.uniqueId()] = std::pair<Int64, Int64>(childs_count, cells_refine.size());
    childs_count += cells_refine.size(); // à modifier selon le nombre d'enfant associé au motif de rafinement !
  }
  // Ajout des nouveaux Noeuds
  Integer nb_node_added = nodes_to_add.size();
  UniqueArray<Int32> nodes_lid(nb_node_added);

  info() << "JustBeforeAdd " << nodes_to_add;
  mesh->modifier()->addNodes(nodes_to_add, nodes_lid.view());

  // Edges: Pas de génération d'arrête

  debug() << "Faces_uids " << faces_uids << " faces_to_add " << faces_to_add.size() << " faces_to_add/6 " << faces_to_add.size() / 6;

  //ARCANE_ASSERT((nodes_to_add.size() != 0),("End"));
  //ARCANE_ASSERT((nb_face_to_add == 68),("WRONG NUMBER OF FACES")); // two hex
  // Ajout des Faces enfants
  UniqueArray<Int32> face_lid(faces_uids.size());
  debug() << "Before addOneFace " << nb_face_to_add;

  //Setup faces
  mesh->modifier()->addFaces(nb_face_to_add, faces_to_add.constView(), face_lid.view());
  debug() << "addOneFace " << nb_face_to_add;
  mesh->faceFamily()->itemsUniqueIdToLocalId(face_lid, faces_uids, true);
  debug() << "NB_FACE_ADDED AFTER " << face_lid.size() << " " << new_faces.size();

  //ARCANE_ASSERT((nb_face_to_add == (faces_to_add.size()/6)),("non consistant number of faces")); // Pour hex

  // Ajout des cellules enfants
  mesh->modifier()->detachCells(cells_to_detach);

  UniqueArray<Int32> cells_lid(nb_cell_to_add);
  mesh->modifier()->addCells(nb_cell_to_add, cells_to_add.constView(), cells_lid);
  info() << "After addCells";
  // mesh->modifier()->addCells()
  // Pour tout les itemgroups
  UniqueArray<Int32> child_cells_lid(child_cells.size());
  mesh->cellFamily()->itemsUniqueIdToLocalId(child_cells_lid, child_cells, true);

  // Gestion des itemgroups ici (différents matériaux par exemple)
  // - On cherche à ajouter les enfants dans les mêmes groupes que les parents pour :
  //   - Faces
  //   - Cells
  // - Pas de déduction automatique pour :
  //   - Noeuds
  //   - Arêtes
  // Algo
  // Pour chaque group
  //   Pour chaque cellules de ce group
  //     ajouter cellules filles de ce group

  // Traiter les groupes pour les faces
  //
  // En fait on ne peut traiter que les faces externes. Est-ce qu'on doit/peut déduire les groupes des faces internes ?
  // Dans le cas du test microhydro on peut car on a que les faces externes aux éléments: XYZ min max
  // A ce moment nous n'avons pas fait de lien face_parent_externe -> face_enfant_externe
  // Pour le faire nous allons parcourir les faces internes parentes, trier les ids  et trier les éléménts

  // Problème des groupes.
  // Tableau faces_externals
  // Pour chaque face parent
  //   - Ajouter dans une nouvelle variable faces_externals l'uid de chaque nouvelle face
  //   - dans map sauvegarder uid, et index

  IItemFamily* face_family = mesh->faceFamily();
  IItemFamily* cell_family = mesh->cellFamily();

  UniqueArray<Int32> face_external_lid(face_external_uid.size());
  mesh->faceFamily()->itemsUniqueIdToLocalId(face_external_lid, face_external_uid);
  // Traiter les groups pour les faces
  info() << "#mygroupname face " << face_family->groups().count();
  for (ItemGroupCollection::Enumerator igroup(face_family->groups()); ++igroup;) {
    ItemGroup group = *igroup;
    info() << "#mygroupname face " << group.fullName();
    if (group.isOwn() && mesh->parallelMng()->isParallel()) {
      info() << "#groups: OWN";
      continue;
    }
    if (group.isAllItems()) { // besoin de ça pour seq et //
      info() << "#groups: ALLITEMS";
      continue;
    }
    info() << "#groups: Added ";
    UniqueArray<Int32> to_add_to_group;

    ENUMERATE_ (Item, iitem, group) { // Pour chaque cellule du groupe on ajoute ses 8 enfants ( ou n )
      Int64 step = parents_to_childs_faces_groups[iitem->uniqueId().asInt64()].first;
      Int64 n_childs = parents_to_childs_faces_groups[iitem->uniqueId().asInt64()].second;
      auto subview = face_external_lid.subView(step, static_cast<Integer>(n_childs));
      //ARCANE_ASSERT((subview.size() == 4 ), ("SUBVIEW"));
      to_add_to_group.addRange(subview);
    }
    group.addItems(to_add_to_group, true);
  }

  // Traiter les groupes pour les cellules
  for (ItemGroupCollection::Enumerator igroup(cell_family->groups()); ++igroup;) {
    CellGroup group = *igroup;
    info() << "#mygroupname" << group.fullName() << "nb item" << cell_family->nbItem();
    if (group.isOwn() && mesh->parallelMng()->isParallel()) {
      info() << "#groups: OWN";
      continue;
    }
    if (group.isAllItems()) { // besoin de ça pour seq et //
      info() << "#groups: ALLITEMS";
      continue;
    }

    info() << "#groups: Added ";
    UniqueArray<Int32> to_add_to_group;

    ENUMERATE_ (Item, iitem, group) { // Pour chaque cellule du groupe on ajoute ses 8 enfants ( ou n )
      Int64 step = parents_to_childs_cell[iitem->uniqueId().asInt64()].first;
      Int64 n_childs = parents_to_childs_cell[iitem->uniqueId().asInt64()].second;
      auto subview = child_cells_lid.subView(step, static_cast<Integer>(n_childs));
      to_add_to_group.addRange(subview);
    }
    info() << "#Added " << to_add_to_group.size() << " to group " << group.fullName();
    group.addItems(to_add_to_group, true);
  }
  // fin gestion itemgroups
  mesh->modifier()->removeDetachedCells(cells_to_detach.constView());
  //mesh->modifier()->removeCells(cells_to_detach.constView());
  mesh->modifier()->endUpdate();

  // DEBUG
  debug() << "Debug faces " << faces_to_add;
  /*for(Integer i = 0 ; i < faces_to_add.size() ; i++){
        debug() << new_faces[i] ;
    }*/
  // ENDEBUG

  // Gestion et assignation du propriétaire pour chaque cellule
  // Le propriétaire est simplement le sous domaine qui a générer les nouvelles cellules
  ENUMERATE_ (Cell, icell, mesh->allCells()) {
    Cell cell = *icell;
    cell.mutableItemBase().setOwner(my_rank, my_rank);
  }
  mesh->cellFamily()->notifyItemsOwnerChanged();

  // ARCANE_ASSERT((nodes_lid.size() != 0),("End"));
  ARCANE_ASSERT((nodes_lid.size() == nodes_to_add.size()), ("End"));
  // Assignation des coords aux noeuds

  UniqueArray<Int32> to_add_to_nodes(nodes_to_add.size()); // Bis
  mesh->nodeFamily()->itemsUniqueIdToLocalId(to_add_to_nodes, nodes_to_add, true);

  ENUMERATE_ (Node, inode, mesh->nodeFamily()->view(to_add_to_nodes)) { // recalculer nodes_lid
    Node node = *inode;
    debug() << node.uniqueId().asInt64();
    //ARCANE_ASSERT((new_nodes.find(node.uniqueId().asInt64()) != new_nodes.end()),("Not found in set !"))
    //ARCANE_ASSERT((nodes_to_add_coords.find(node.uniqueId().asInt64()) != nodes_to_add_coords.end()),("Not found in map coord!"))
    // si il est pas dans la map c'est qu'il existe déja !

    nodes_coords[node] = nodes_to_add_coords[node.uniqueId().asInt64()];
    debug() << "InSBD" << node.uniqueId().asInt64() << " " << nodes_to_add_coords[node.uniqueId().asInt64()];
  }

  //info() << "#NODECOORDS" << nodes_coords.asArray() ;
  // Ajout d'une couche fantôme
  Arcane::IGhostLayerMng* gm2 = mesh->ghostLayerMng();
  gm2->setNbGhostLayer(1);
  mesh->updateGhostLayers(true);

  // Gestion des propriétaires de noeuds
  // Le propriétaire est la cellule incidente au noeud avec le plus petit uniqueID()
  ENUMERATE_ (Node, inode, mesh->allNodes()) {
    Node node = *inode;
    auto it = std::min_element(node.cells().begin(), node.cells().end());
    Cell cell = node.cell(static_cast<Int32>(std::distance(node.cells().begin(), it)));
    node_uid_to_owner[node.uniqueId().asInt64()] = cell.owner();
  }

  // Gestion des propriétaires des faces
  // Le propriétaire est la cellule incidente à la face avec le plus petit uniqueID()
  ENUMERATE_ (Face, iface, mesh->allFaces()) {
    Face face = *iface;
    auto it = std::min_element(face.cells().begin(), face.cells().end());
    Cell cell = face.cell(static_cast<Int32>(std::distance(face.cells().begin(), it)));
    face_uid_to_owner[face.uniqueId().asInt64()] = cell.owner();
  }

  // Utiliser les couches fantôme est couteux (construction destruction)
  // - Optim: pour les noeuds partager avoir une variable all to all (gather) qui permet de récuper le rank de l'owner pour chaque item shared
  // - Déduction possible des owners des faces enfants avec la face parent directement
  // - Les cellules enfants ont le même owner que la cellule parent
  // Supression de la couche fantôme
  gm2->setNbGhostLayer(0);
  mesh->updateGhostLayers(true);

  // DEBUG
  /*debug() << "#Faces mesh";
  ENUMERATE_ (Face, iface, mesh->allFaces()) {
    Face face = *iface;
    debug() << face.uniqueId().asInt64();
  }*/

  // Quelques sur le nombres d'entités insérés
  // ARCANE_ASSERT((mesh->nbCell() == nb_cell_init*8 ),("Wrong number of cell added"));
  debug() << "nbface " << mesh->nbFace() << " " << nb_face_to_add << " expected " << nb_face_init * 4 + 12 * nb_cell_init;
  // ARCANE_ASSERT((mesh->nbFace() <= nb_face_init*4 + 12 * nb_cell_init ),("Wrong number of face added"));
  // A ajouter pour vérifier le nombre de noeud si les arêtes sont crées
  // ARCANE_ASSERT((mesh->nbNode() == nb_edge_init + nb_face_init + nb_cell_init ),("Wrong number of node added"))

  // Assignation du nouveau propriétaire pour chaque noeud
  ENUMERATE_ (Node, inode, mesh->allNodes()) {
    Node node = *inode;
    node.mutableItemBase().setOwner(node_uid_to_owner[node.uniqueId().asInt64()], my_rank);
  }
  mesh->nodeFamily()->notifyItemsOwnerChanged();

  // Assignation du nouveaux propriétaires pour chaque face
  ENUMERATE_ (Face, iface, mesh->allFaces()) {
    Face face = *iface;
    face.mutableItemBase().setOwner(face_uid_to_owner[face.uniqueId().asInt64()], my_rank);
  }
  mesh->faceFamily()->notifyItemsOwnerChanged();

  // On met de nouveau le ghost layer pour une future simulation
  gm2->setNbGhostLayer(nb_ghost_layer_init);
  mesh->updateGhostLayers(true);

  // Ecriture au format VTK
  /*
  mesh->utilities()->writeToFile("3Drefined" + std::to_string(my_rank) + ".vtk", "VtkLegacyMeshWriter");
  info() << "Writing VTK 3Drefine";
  debug() << "END 3D fun";
  debug() << "NB CELL " << mesh->nbCell() << " " << nb_cell_init * 8;
  debug() << mesh->nbNode() << " " << nb_node_init << " " << nb_edge_init << " " << nb_face_init << " " << nb_cell_init;
  debug() << mesh->nbFace() << "nb_face_init " << nb_face_init << " " << nb_face_init << " " << nb_cell_init;
  debug() << "Faces: " << mesh->nbFace() << " theorical nb_face_to add: " << nb_face_init * 4 + nb_cell_init * 12 << " nb_face_init " << nb_face_init << " nb_cell_init " << nb_cell_init;
  info() << "#NODES_CHECK #all" << mesh->allNodes().size() << " #own " << mesh->ownNodes().size();
  */
}

void ArcaneBasicMeshSubdividerService::_refineWithArcaneFaces(IPrimaryMesh* mesh, MeshSubdivider::Pattern p)
{
  IMeshModifier* modifier = mesh->modifier();
  Int64UniqueArray cells_infos;
  Int64UniqueArray faces_infos;
  Int64UniqueArray nodes_uid;
  std::unordered_map<Int64, Real3> nodes_to_add_coords;
  VariableNodeReal3& nodes_coords = mesh->nodesCoordinates();

  // get max uid for cells
  Int64 max_offset = 0;
  ENUMERATE_CELL (icell, mesh->allCells()) {
    Cell cell = *icell;
    info() << cell.uniqueId().asInt64() << " ";
    if (max_offset < cell.uniqueId())
      max_offset = cell.uniqueId();
  }
  //ARCANE_ASSERT((max_offset!=0),("BAD OFFSET"));

  info() << "#_refineWithArcaneFaces";
  ENUMERATE_CELL (icell, mesh->allCells()) {
    const Cell& cell = *icell;
    info() << "cell[" << icell->localId() << "," << icell->uniqueId() << "] type="
           << icell->type() << ", nb nodes=" << icell->nbNode();

    for (Face face : cell.faces()) {
      info() << "Face " << face.uniqueId() << " nodes ";
      for (Node node : face.nodes()) {
        info() << node.uniqueId() << " ";
      }
    }
  }
  //mesh->utilities()->writeToFile("subdivider_one_tetra_output.vtk", "VtkLegacyMeshWriter");
  ARCANE_ASSERT((!p.cells.empty()), ("Pattern not init"));
  Integer cellcount = 0;
  UniqueArray<Int32> cells_to_detach; // Cellules à détacher

  Int64 face_count = 0;

  std::map<Int64, Int32> node_uid_to_cell_local_id; // Donne le local id par rapport à une cell

  ENUMERATE_CELL (icell, mesh->allCells()) {
    UniqueArray<Int64> node_in_cell;
    const Cell& cell = *icell;
    info() << "Detach";
    cells_to_detach.add(cell.localId());
    info() << "Get Pattern";
    StorageRefine& node_pattern = p.nodes;
    StorageRefine& cells = p.cells;

    info() << "Get Nodes";
    for (Integer i = 0; i < cell.nbNode(); i++) {
      node_in_cell.add(cell.node(i).uniqueId().asInt64());
      info() << "Node " << cell.node(i).uniqueId().asInt64() << " " << nodes_coords[cell.node(i)];
    }
    info() << "Node pattern " << node_pattern.size() << "nic " << node_in_cell;
    _computeNodeUid(node_in_cell, node_pattern);
    // New nodes and coords
    for (Integer i = 0; i < node_pattern.size(); i++) {
      info() << "test " << i;
      UniqueArray<Int64> tmp = node_pattern[i];

      // uid == node_in_cell[uid]
      Int64 uid = cell.nbNode() + i;
      node_in_cell.add(uid); //= 4+i;// = 4+i; //Arcane::MeshUtils::generateHashUniqueId(tmp.constView());
      nodes_uid.add(uid);
      // Coords
      Arcane::Real3 middle_coord(0.0, 0.0, 0.0);

      for (Integer j = 0; j < node_pattern[i].size(); j++) {
        middle_coord += nodes_coords[cell.node(static_cast<Int32>(node_pattern[i][j]))];
        info() << node_pattern[i][j] << cell.node(static_cast<Int32>(node_pattern[i][j]));
      }
      middle_coord /= node_pattern[i].size();
      nodes_to_add_coords[uid] = middle_coord;
      info() << "NodeX " << uid << " " << " coord " << nodes_to_add_coords[uid] << " " << middle_coord;
      node_uid_to_cell_local_id[uid] = cell.nbNode() + i;
    }

    info() << "#node in cell " << node_in_cell;
    // Génération nouvelles faces et cells
    // New faces
    /*for( Integer i = 0 ; i < faces.size() ; i++ ){
      // Header
      faces_infos.add(p.face_pattern);            // type  // Dépendant pattern //#HERE
      faces_infos.add(i);                    // face uid
      for( Integer j = 0 ; j < faces[i].size() ; j++ ) {
        faces_infos.add(node_in_cell[faces[i][j]]);  // node 0
      }
      // Face_info
      info() << "face " << face_count << " " << node_in_cell[faces[i][0]] << " " << node_in_cell[faces[i][1]] << " " << node_in_cell[faces[i][2]];
      face_count++;
    }*/
    // New cells
    for (Integer i = 0; i < cells.size(); i++) {
      // Header
      max_offset++;
      cells_infos.add(p.cell_type); // type  // Dépendant pattern
      cells_infos.add(max_offset); // cell uid
      // Cell_info
      info() << "Cell " << i;
      for (Integer j = 0; j < cells[i].size(); j++) {
        info() << "test2bis " << node_in_cell[cells[i][j]] << " " << node_in_cell.size() << " " << node_pattern.size();
        cells_infos.add(node_in_cell[cells[i][j]]);
      }
      cellcount++;
    }
    info() << "test2bisbis ";
    for (Integer i = 0; i < node_in_cell.size(); i++) {
      info() << "node_in_cell[ " << i << " ] " << node_in_cell[i];
    }
    info() << "test3 ";
  }
  UniqueArray<Int32> nodes_lid(nodes_uid.size());
  // Debug ici
  info() << "test3 " << nodes_uid.size() << " " << nodes_lid.size();
  nodes_lid.clear();
  nodes_lid.reserve(nodes_uid.size());

  modifier->addNodes(nodes_uid, nodes_lid);
  info() << "After nodes";
  UniqueArray<Int32> faces_lid(face_count);
  //modifier->addFaces(face_count, faces_infos, faces_lid);
  info() << "After faces";
  UniqueArray<Int32> cells_lid(cellcount);

  modifier->addCells(cellcount, cells_infos, cells_lid);
  info() << "cellsize " << cells_infos.size() << " " << cellcount;
  modifier->removeCells(cells_to_detach.constView());
  modifier->endUpdate();
  // Assignation coords aux nouveaux noeuds
  VariableNodeReal3 coords_bis = mesh->nodesCoordinates();

  info() << nodes_lid.size();
  UniqueArray<Int32> to_add_to_nodes(nodes_uid.size()); // Bis
  mesh->nodeFamily()->itemsUniqueIdToLocalId(to_add_to_nodes, nodes_uid, true);

  info() << "#NODESHERE";
  ENUMERATE_ (Node, inode, mesh->nodeFamily()->view(to_add_to_nodes)) {
    Node node = *inode;
    coords_bis[node] = nodes_to_add_coords[node.uniqueId()];
    info() << "node " << node.uniqueId() << " coord " << nodes_to_add_coords[node.uniqueId()];
    info() << node.uniqueId() << " " << nodes_coords[node];
  }

  //mesh->utilities()->writeToFile("subdivider_one_tetra_refine_output1.vtk", "VtkLegacyMeshWriter");

  info() << "#coords" << coords_bis.asArray();
  info() << "#My mesh ";
  UniqueArray<Int64> stuff;
  // Affichage maillage
  ENUMERATE_CELL (icell, mesh->allCells()) {
    const Cell& cell = *icell;
    info() << "Cell " << cell.uniqueId() << " " << cell.nodeIds();
    for (Face face : cell.faces()) {
      for (Node node : face.nodes()) {
        stuff.add(node.uniqueId());
      }
      info() << "Faces " << face.uniqueId() << " node " << stuff;
    }
    stuff.clear();
  }
  info() << "#Arcane face numbering:";
  std::cout << "{";
  ENUMERATE_FACE (iface, mesh->allFaces()) {
    const Face& face = *iface;
    UniqueArray<Int64> stuff;
    std::cout << "{" << face.nodes()[0].uniqueId().asInt64();

    for (Integer i = 1; i < face.nodes().size(); i++) {
      std::cout << "," << face.nodes()[i].uniqueId().asInt64();
    }
    //stuff.add(node_uid_to_cell_local_id[node.uniqueId().asInt64()]);
    //info() << "Faces " << face.uniqueId() << " node " << stuff ;
    //std::cout << node_uid_to_cell_local_id[node.uniqueId().asInt64()] ;

    //info() << "Faces " << face.uniqueId() << " node " << stuff ;
    std::cout << "}," << std::endl;
  }
  std::cout << "}";
  Arcane::VariableScalarInteger m_temperature(Arcane::VariableBuildInfo(mesh, "ArcaneCheckpointNextIteration"));

  VariableCellInt64* arcane_cell_uid = nullptr;
  VariableFaceInt64* arcane_face_uid = nullptr;
  VariableNodeInt64* arcane_node_uid = nullptr;
  arcane_cell_uid = new VariableCellInt64(Arcane::VariableBuildInfo(mesh, "arcane_cell_uid", mesh->cellFamily()->name()));
  arcane_face_uid = new VariableFaceInt64(Arcane::VariableBuildInfo(mesh, "arcane_face_uid", mesh->faceFamily()->name()));
  arcane_node_uid = new VariableNodeInt64(Arcane::VariableBuildInfo(mesh, "arcane_node_uid", mesh->nodeFamily()->name()));

  ENUMERATE_CELL (icell, mesh->allCells()) {
    (*arcane_cell_uid)[icell] = icell->uniqueId().asInt64();
  }
  ENUMERATE_FACE (iface, mesh->allFaces()) {
    (*arcane_face_uid)[iface] = iface->uniqueId().asInt64();
  }
  info() << "#INODE";
  ENUMERATE_NODE (inode, mesh->allNodes()) {
    (*arcane_node_uid)[inode] = inode->uniqueId().asInt64();
    info() << inode->uniqueId().asInt64();
  }
  /*
  ENUMERATE_(Node, inode, mesh->nodeFamily()->view().subView(4,nodes_uid.size())){
    Node node = *inode;
    nodes_coords[node] = nodes_to_add_coords[node.uniqueId()];
    info() << node.uniqueId() << " " << nodes_coords[node] ;
  }
  */
  //
  // On va chercher le service directement sans utiliser dans le .arc
  /*Directory d = mesh->subDomain()->exportDirectory();
  info() << "Writing at " << d.path() ;
  ServiceBuilder<IPostProcessorWriter> spp(mesh->handle());

  Ref<IPostProcessorWriter> post_processor = spp.createReference("Ensight7PostProcessor"); // vtkHdf5PostProcessor
  //Ref<IPostProcessorWriter> post_processor = spp.createReference("VtkLegacyMeshWriter"); // (valid values = UCDPostProcessor, UCDPostProcessor, Ensight7PostProcessor, Ensight7PostProcessor)
  // Path de base
  // <fichier-binaire>false</fichier-binaire>
  post_processor->setBaseDirectoryName(d.path());

  post_processor->setTimes(UniqueArray<Real>{0.1}); // Juste pour fixer le pas de temps

  VariableList variables;
  variables.add(mesh->nodesCoordinates().variable());
  variables.add(*arcane_node_uid);
  variables.add(*arcane_face_uid);
  variables.add(*arcane_cell_uid);
  post_processor->setVariables(variables);

  ItemGroupList groups;
  groups.add(mesh->allNodes());
  groups.add(mesh->allFaces());
  groups.add(mesh->allCells());
  post_processor->setGroups(groups);

  IVariableMng* vm = mesh->subDomain()->variableMng();

  vm->writePostProcessing(post_processor.get());
  mesh->utilities()->writeToFile("subdivider_one_tetra_refine_output.vtk", "VtkLegacyMeshWriter");
  */
  info() << "#ENDSUBDV ";
}

void ArcaneBasicMeshSubdividerService::_generateOneQuad(IPrimaryMesh* mesh)
{
  mesh->utilities()->writeToFile("subdivider_one_quad_input.vtk", "VtkLegacyMeshWriter");

  // On supprime l'ancien maillage
  Int32UniqueArray lids(mesh->allCells().size());
  ENUMERATE_CELL (icell, mesh->allCells()) {
    info() << "cell[" << icell->localId() << "," << icell->uniqueId() << "] type="
           << icell->type() << ", nb nodes=" << icell->nbNode();
    lids[icell.index()] = icell->localId();
  }
  IMeshModifier* modifier = mesh->modifier();
  modifier->removeCells(lids);
  modifier->endUpdate();

  // On creer notre Quad
  Int64UniqueArray nodes_uid(4);
  for (Integer i = 0; i < 4; i++)
    nodes_uid[i] = i;

  UniqueArray<Int32> nodes_lid(nodes_uid.size());
  modifier->addNodes(nodes_uid, nodes_lid.view());
  mesh->nodeFamily()->endUpdate();
  VariableNodeReal3& nodes_coords = mesh->nodesCoordinates();
  NodeInfoListView new_nodes(mesh->nodeFamily());
  nodes_coords[new_nodes[nodes_lid[0]]] = Arcane::Real3(0.0, 0.0, 0.0);
  nodes_coords[new_nodes[nodes_lid[1]]] = Arcane::Real3(10.0, 0.0, 0.0);
  nodes_coords[new_nodes[nodes_lid[2]]] = Arcane::Real3(10.0, 10.0, 0.0);
  nodes_coords[new_nodes[nodes_lid[3]]] = Arcane::Real3(0.0, 10.0, 0.0);

  Int64UniqueArray cells_infos(6);
  Int64UniqueArray faces_infos;
  cells_infos[0] = IT_Quad4; // type
  cells_infos[1] = 44; // cell uid
  cells_infos[2] = nodes_uid[0]; // node 0
  cells_infos[3] = nodes_uid[1]; // ...  1
  cells_infos[4] = nodes_uid[2]; // ...  2
  cells_infos[5] = nodes_uid[3]; // ...  3

  IntegerUniqueArray cells_lid;
  modifier->addCells(1, cells_infos, cells_lid);
  modifier->endUpdate();
  mesh->utilities()->writeToFile("subdivider_one_quad_ouput.vtk", "VtkLegacyMeshWriter");
}


void ArcaneBasicMeshSubdividerService::_generateOneTri(IPrimaryMesh* mesh)
{
  mesh->utilities()->writeToFile("subdivider_one_hexa_input.vtk", "VtkLegacyMeshWriter");
  // On supprime l'ancien maillage
  Int32UniqueArray lids(mesh->allCells().size());
  ENUMERATE_CELL (icell, mesh->allCells()) {
    info() << "cell[" << icell->localId() << "," << icell->uniqueId() << "] type="
           << icell->type() << ", nb nodes=" << icell->nbNode();
    lids[icell.index()] = icell->localId();
  }
  IMeshModifier* modifier = mesh->modifier();
  modifier->removeCells(lids);
  modifier->endUpdate();
  // On creer notre Hexa
  Int64UniqueArray nodes_uid(3);
  for (Integer i = 0; i < 3; i++)
    nodes_uid[i] = i;

  UniqueArray<Int32> nodes_lid(nodes_uid.size());
  modifier->addNodes(nodes_uid, nodes_lid.view());
  mesh->nodeFamily()->endUpdate();
  VariableNodeReal3& nodes_coords = mesh->nodesCoordinates();
  NodeInfoListView new_nodes(mesh->nodeFamily());
  nodes_coords[new_nodes[nodes_lid[0]]] = Arcane::Real3(0.0, 0.0, 0.0);
  nodes_coords[new_nodes[nodes_lid[1]]] = Arcane::Real3(10.0, 0.0, 0.0);
  nodes_coords[new_nodes[nodes_lid[2]]] = Arcane::Real3(10.0, 10.0, 0.0);

  Int64UniqueArray cells_infos(10);
  Int64UniqueArray faces_infos;
  cells_infos[0] = IT_Triangle3; // type
  cells_infos[1] = 44; // cell uid
  cells_infos[2] = nodes_uid[0]; // node 0
  cells_infos[3] = nodes_uid[1]; // ...  1
  cells_infos[4] = nodes_uid[2]; // ...  2

  IntegerUniqueArray cells_lid;
  modifier->addCells(1, cells_infos, cells_lid);
  modifier->endUpdate();
  mesh->utilities()->writeToFile("subdivider_one_tri.vtk", "VtkLegacyMeshWriter");
}

UniqueArray<Int64> ArcaneBasicMeshSubdividerService::_computeNodeUid(UniqueArray<Int64> node_in_cell, const StorageRefine& node_pattern)
{
  UniqueArray<Int64> new_node_uid;
  Integer init_size = node_in_cell.size();
  for (Integer i = 0; i < node_pattern.size(); i++) {
    //info() << "test " << i ;
    UniqueArray<Int64> tmp = node_pattern[i];
    tmp.resize(node_pattern[i].size());
    for (Integer j = 0; j < node_pattern[i].size(); j++) {
      tmp.add(node_in_cell[node_pattern[i][j]]);
    }
    // uid
    std::sort(tmp.begin(), tmp.end());
    node_in_cell.add(init_size + i); //= 4+i;// = 4+i; //Arcane::MeshUtils::generateHashUniqueId(tmp.constView());
    new_node_uid.add(node_in_cell[init_size + i]);
  }
  return new_node_uid;
}

ArcaneBasicMeshSubdividerService::
ArcaneBasicMeshSubdividerService(const ServiceBuildInfo& sbi)
: ArcaneArcaneBasicMeshSubdividerServiceObject(sbi)
{
}

void ArcaneBasicMeshSubdividerService::_generateOneHexa(IPrimaryMesh* mesh)
{
  mesh->utilities()->writeToFile("subdivider_one_hexa_input.vtk", "VtkLegacyMeshWriter");
  // On supprime l'ancien maillage
  Int32UniqueArray lids(mesh->allCells().size());
  ENUMERATE_CELL (icell, mesh->allCells()) {
    info() << "cell[" << icell->localId() << "," << icell->uniqueId() << "] type="
           << icell->type() << ", nb nodes=" << icell->nbNode();
    lids[icell.index()] = icell->localId();
  }
  IMeshModifier* modifier = mesh->modifier();
  modifier->removeCells(lids);
  modifier->endUpdate();
  // On creer notre Hexa
  Int64UniqueArray nodes_uid(8);
  for (Integer i = 0; i < 8; i++)
    nodes_uid[i] = i;

  UniqueArray<Int32> nodes_lid(nodes_uid.size());
  modifier->addNodes(nodes_uid, nodes_lid.view());
  mesh->nodeFamily()->endUpdate();
  VariableNodeReal3& nodes_coords = mesh->nodesCoordinates();
  NodeInfoListView new_nodes(mesh->nodeFamily());
  nodes_coords[new_nodes[nodes_lid[0]]] = Arcane::Real3(0.0, 0.0, 0.0);
  nodes_coords[new_nodes[nodes_lid[1]]] = Arcane::Real3(10.0, 0.0, 0.0);
  nodes_coords[new_nodes[nodes_lid[2]]] = Arcane::Real3(10.0, 10.0, 0.0);
  nodes_coords[new_nodes[nodes_lid[3]]] = Arcane::Real3(0.0, 10.0, 0.0);
  nodes_coords[new_nodes[nodes_lid[4]]] = Arcane::Real3(0.0, 0.0, 10.0);
  nodes_coords[new_nodes[nodes_lid[5]]] = Arcane::Real3(10.0, 0.0, 10.0);
  nodes_coords[new_nodes[nodes_lid[6]]] = Arcane::Real3(10.0, 10.0, 10.0);
  nodes_coords[new_nodes[nodes_lid[7]]] = Arcane::Real3(0.0, 10.0, 10.0);

  Int64UniqueArray cells_infos(10);
  Int64UniqueArray faces_infos;
  cells_infos[0] = IT_Hexaedron8; // type
  cells_infos[1] = 44; // cell uid
  cells_infos[2] = nodes_uid[0]; // node 0
  cells_infos[3] = nodes_uid[1]; // ...  1
  cells_infos[4] = nodes_uid[2]; // ...  2
  cells_infos[5] = nodes_uid[3]; // ...  3
  cells_infos[6] = nodes_uid[4]; // ...  4
  cells_infos[7] = nodes_uid[5]; // ...  5
  cells_infos[8] = nodes_uid[6]; // ...  6
  cells_infos[9] = nodes_uid[7]; // ...  7

  IntegerUniqueArray cells_lid;
  modifier->addCells(1, cells_infos, cells_lid);
  modifier->endUpdate();
  // On crée un groupe test
  UniqueArray<Int64> face_uid;
  ENUMERATE_FACE (iface, mesh->allFaces()) {
    Face face = *iface;
    face_uid.add(face.uniqueId());
  }
  UniqueArray<Int32> face_lid(face_uid.size());
  mesh->faceFamily()->itemsUniqueIdToLocalId(face_lid, face_uid, true);
  mesh->faceFamily()->createGroup("GroupeTest", face_lid);
  mesh->utilities()->writeToFile("subdivider_one_hexa_ouput.vtk", "VtkLegacyMeshWriter");
}

void ArcaneBasicMeshSubdividerService::_generateOneTetra(IPrimaryMesh* mesh)
{

  mesh->utilities()->writeToFile("subdivider_one_tetra_input.vtk", "VtkLegacyMeshWriter");

  // On supprime l'ancien maillage si besoin
  Int32UniqueArray lids(mesh->allCells().size());
  VariableNodeReal3& nodes_coords = mesh->nodesCoordinates();
  ENUMERATE_CELL (icell, mesh->allCells()) {
    info() << "cell[" << icell->localId() << "," << icell->uniqueId() << "] type="
           << icell->type() << ", nb nodes=" << icell->nbNode();
    lids[icell.index()] = icell->localId();
  }
  IMeshModifier* modifier = mesh->modifier();
  modifier->removeCells(lids);
  modifier->endUpdate();

  // Maillage vide, on créer notre tetra

  info() << "===================== THE MESH IS EMPTY";

  // On ajoute des noeuds
  Int64UniqueArray nodes_uid(4);
  for (Integer i = 0; i < 4; i++)
    nodes_uid[i] = i;

  UniqueArray<Int32> nodes_lid(nodes_uid.size());
  modifier->addNodes(nodes_uid, nodes_lid.view());
  mesh->nodeFamily()->endUpdate();
  info() << "===================== THE MESH IS EMPTY";

  NodeInfoListView new_nodes(mesh->nodeFamily());

  nodes_coords[new_nodes[nodes_lid[0]]] = Arcane::Real3(0.0, 0.0, 0.0);
  nodes_coords[new_nodes[nodes_lid[1]]] = Arcane::Real3(10.0, 0.0, 0.0);
  nodes_coords[new_nodes[nodes_lid[2]]] = Arcane::Real3(5.0, 5.0 / 3.0, 10.0);
  nodes_coords[new_nodes[nodes_lid[3]]] = Arcane::Real3(5.0, 5.0, 0.0);

  Int64UniqueArray cells_infos(1 * 6);
  Int64UniqueArray faces_infos;

  cells_infos[0] = IT_Tetraedron4; // type
  cells_infos[1] = 44; // cell uid
  cells_infos[2] = nodes_uid[0]; // node 0
  cells_infos[3] = nodes_uid[1]; // ...  1
  cells_infos[4] = nodes_uid[2]; // ...  2
  cells_infos[5] = nodes_uid[3]; // ...  3

  IntegerUniqueArray cells_lid;
  modifier->addCells(1, cells_infos, cells_lid);
  modifier->endUpdate();

  // On crée un groupe test
  UniqueArray<Int64> face_uid;
  ENUMERATE_FACE (iface, mesh->allFaces()) {
    Face face = *iface;
    face_uid.add(face.uniqueId());
  }
  UniqueArray<Int32> face_lid(face_uid.size());
  mesh->faceFamily()->itemsUniqueIdToLocalId(face_lid, face_uid, true);
  mesh->faceFamily()->createGroup("GroupeTest", face_lid);

  info() << "===================== THE CELLS ARE ADDED";
}

/* Le but est simplement d'avoir l'ordre des faces dans un élément*/
void ArcaneBasicMeshSubdividerService::_faceOrderArcane(IPrimaryMesh* mesh)
{
  mesh->utilities()->writeToFile("3D_last_input_seq.vtk", "VtkLegacyMeshWriter");
  info() << "#ORDRE FACES";
  ENUMERATE_CELL (icell, mesh->ownCells()) {

    const Cell& cell = *icell;
    for (Face face : cell.faces()) {
      UniqueArray<Int64> n;
      for (Node node : face.nodes()) {
        n.add(node.uniqueId().asInt64());
      }
      info() << face.uniqueId() << " nodes " << n;
    }
  }
}

void ArcaneBasicMeshSubdividerService::
subdivideMesh([[maybe_unused]] IPrimaryMesh* mesh)
{
  std::unordered_map<Arccore::Int16, MeshSubdivider::Pattern> pattern_manager;
  // Default pattern manager
  pattern_manager[IT_Quad4] = PatternBuilder::quadtoquad();
  pattern_manager[IT_Triangle3] = PatternBuilder::tritotri();
  pattern_manager[IT_Hexaedron8] = PatternBuilder::hextohex();
  pattern_manager[IT_Tetraedron4] = PatternBuilder::tettotet();

  if(options()->differentElementTypeOutput()){
    pattern_manager[IT_Quad4] = PatternBuilder::quadtotri();
    pattern_manager[IT_Triangle3] = PatternBuilder::tritoquad();
    pattern_manager[IT_Hexaedron8] = PatternBuilder::hextotet24();
    pattern_manager[IT_Tetraedron4] = PatternBuilder::tettohex();
    info() << "The refinement patterns have changed for meshes of the following types: Quad4, Triangle3, Hexaedron8, Tetraedron4." ; 
    info() << "The output element types will be:\nQuad4->Triangle3\nTriangle3->Quad4\nHexaedron8->Tetraedron4\nTetraedron4->Hexaedron8" ;
  }

  for (Integer i = 0; i < options()->nbSubdivision; i++) {
    _refineOnce(mesh, pattern_manager);
    debug() << i << "refine done";
  }
  
  // Debug After
  
  //mesh->utilities()->writeToFile("subdivider_output_"+std::to_string(mesh->parallelMng()->commRank())+".vtk", "VtkLegacyMeshWriter");
  
  
  //mesh->utilities()->writeToFile("subdivider_after_" + std::to_string(options()->nbSubdivision) + "refine.vtk", "VtkLegacyMeshWriter");
  //debug() << "write file with name:" << "subdivider_after_";
  /*
  mesh->utilities()->writeToFile("subdivider_output.vtk", "VtkLegacyMeshWriter");
  */
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ARCANE_REGISTER_SERVICE_ARCANEBASICMESHSUBDIVIDERSERVICE(ArcaneBasicMeshSubdivider,
                                                         ArcaneBasicMeshSubdividerService);

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

} // End namespace Arcane

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
