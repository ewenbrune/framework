﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
//-----------------------------------------------------------------------------
// Copyright 2000-2024 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* MeshMaterialVariableArrayTpl1.cc                            (C) 2000-2024 */
/*                                                                           */
/* Variable tableau sur un matériau du maillage.                             */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#include "arcane/materials/MeshMaterialVariableArray.inst.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace Arcane::Materials
{

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ARCANE_INSTANTIATE_MAT(Real);
ARCANE_INSTANTIATE_MAT(Real2);
ARCANE_INSTANTIATE_MAT(Real3);
ARCANE_INSTANTIATE_MAT(Real2x2);
ARCANE_INSTANTIATE_MAT(Real3x3);

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

} // End namespace Arcane::Materials

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
