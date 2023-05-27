﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
//-----------------------------------------------------------------------------
// Copyright 2000-2023 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* CartesianMeshAllocateBuildInfo.h                            (C) 2000-2023 */
/*                                                                           */
/* Informations pour allouer les entités d'un maillage cartésien.            */
/*---------------------------------------------------------------------------*/
#ifndef ARCANE_CORE_CARTESIANMESHALLOCATEBUILDINFO_H
#define ARCANE_CORE_CARTESIANMESHALLOCATEBUILDINFO_H
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#include "arcane/core/ItemTypes.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace Arcane
{
class CartesianMeshAllocateBuildInfoInternal;

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*!
 * \brief Informations pour allouer les entités d'un maillage non structuré.
 *
 * Cette classe permet de spécifier les mailles qui seront ajoutées lors
 * de l'allocation initiale du maillage.
 * Il faut appeler setMeshDimension() pour spécifier la dimension du maillage
 * puis appeler addCell() pour chaque maille qu'on souhaite ajouter. Une fois
 * toute les mailles ajoutées il faut appeler allocateMesh().
 */
class ARCANE_CORE_EXPORT CartesianMeshAllocateBuildInfo
{
 public:

  class Impl;
  class Intenrnal;

 public:

  explicit CartesianMeshAllocateBuildInfo(IPrimaryMesh* mesh);
  ~CartesianMeshAllocateBuildInfo();

 public:

  CartesianMeshAllocateBuildInfo(CartesianMeshAllocateBuildInfo&& from) = delete;
  CartesianMeshAllocateBuildInfo(const CartesianMeshAllocateBuildInfo& from) = delete;
  CartesianMeshAllocateBuildInfo& operator=(CartesianMeshAllocateBuildInfo&& from) = delete;
  CartesianMeshAllocateBuildInfo& operator=(const CartesianMeshAllocateBuildInfo& from) = delete;

 public:

  //! Positionne les informations pour un maillage 3D.
  void setInfos(std::array<Int64, 3> global_nb_cells,
                std::array<Int32, 3> own_nb_cells,
                Int64 cell_unique_id_offset,
                Int64 node_unique_id_offset);

  /*!
   * \brief Alloue le maillage.
   *
   * Il est nécessaire d'avoir appelé setInfos() avant.
   *
   * Cette méthode est collective.
   */
  void allocateMesh();

 public:

  //! Partie interne réservée à Arcane
  CartesianMeshAllocateBuildInfoInternal* _internal();

 private:

  Impl* m_p = nullptr;
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

} // End namespace Arcane

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#endif
