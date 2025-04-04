﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
//-----------------------------------------------------------------------------
// Copyright 2000-2024 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* ConstituentConnectivityList.cc                              (C) 2000-2024 */
/*                                                                           */
/* Gestion des listes de connectivité des constituants.                      */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#include "arcane/materials/internal/ConstituentConnectivityList.h"

#include "arcane/core/IItemFamily.h"

#include "arcane/utils/ITraceMng.h"
#include "arcane/core/MeshUtils.h"
#include "arcane/core/internal/IDataInternal.h"
#include "arcane/core/internal/IItemFamilyInternal.h"

#include "arcane/materials/internal/MeshMaterialMng.h"

#include "arcane/accelerator/core/RunQueue.h"
#include "arcane/accelerator/RunCommandLoop.h"
#include "arcane/accelerator/Scan.h"
#include "arcane/accelerator/Reduce.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace Arcane::Materials
{
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace
{
  // Cette méthode est la même que MeshUtils::removeItemAndKeepOrder().
  // Il faudrait pouvoir fusionner les deux.
  // NOTE: Avec le C++20, on devrait pouvoir supprimer cette méthode et
  // utiliser std::erase()

  template <typename DataType>
  ARCCORE_HOST_DEVICE void _removeValueAndKeepOrder(ArrayView<DataType> values, DataType value_to_remove)
  {
    Integer n = values.size();
#ifndef ARCCORE_DEVICE_CODE
    if (n <= 0)
      ARCANE_FATAL("Can not remove item lid={0} because list is empty", value_to_remove);
#endif
    --n;
    if (n == 0) {
      if (values[0] == value_to_remove)
        return;
    }
    else {
      // Si l'élément est le dernier, ne fait rien.
      if (values[n] == value_to_remove)
        return;
      for (Integer i = 0; i < n; ++i) {
        if (values[i] == value_to_remove) {
          for (Integer z = i; z < n; ++z)
            values[z] = values[z + 1];
          return;
        }
      }
    }
#if defined(ARCCORE_DEVICE_CODE)
    // Avec Intel DPC++ 2024.1 l'utilisation de cette fonction avec le
    // backend CUDA provoque une erreur dans l'assembleur généré.
#  if !defined(__INTEL_LLVM_COMPILER)
    assert(false);
#  endif
#else
    ARCANE_FATAL("No value to remove '{0}' found in list {1}", value_to_remove, values);
#endif
  }

} // namespace

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*!
 * \brief Conteneur des données de connectivité pour un type de constituant.
 */
class ConstituentConnectivityList::ConstituentContainer
{
 public:

  /*!
   * \brief Vue sur une liste de constituants des mailles.
   */
  class View
  {
   public:

    explicit View(ConstituentContainer& c)
    : nb_components_view(c.m_nb_component_as_array.view())
    , component_indexes_view(c.m_component_index_as_array.view())
    , component_list_view(c.m_component_list_as_array.view())
    {
    }

   public:

    ARCCORE_HOST_DEVICE SmallSpan<const Int16> components(Int32 item_lid) const
    {
      Int16 n = nb_components_view[item_lid];
      Int32 index = component_indexes_view[item_lid];
      return component_list_view.subPart(index, n);
    }

   private:

    SmallSpan<Int16> nb_components_view;
    SmallSpan<Int32> component_indexes_view;
    SmallSpan<Int16> component_list_view;
  };

 public:

  ConstituentContainer(const MeshHandle& mesh, const String& var_base_name)
  : m_nb_component(VariableBuildInfo(mesh, var_base_name + "NbComponent", IVariable::PPrivate))
  , m_component_index(VariableBuildInfo(mesh, var_base_name + "Index", IVariable::PPrivate))
  , m_component_list(VariableBuildInfo(mesh, var_base_name + "List", IVariable::PPrivate))
  , m_nb_component_as_array(m_nb_component._internalTrueData()->_internalDeprecatedValue())
  , m_component_index_as_array(m_component_index._internalTrueData()->_internalDeprecatedValue())
  , m_component_list_as_array(m_component_list._internalTrueData()->_internalDeprecatedValue())
  {
  }

 public:

  void endCreate(bool is_continue)
  {
    if (!is_continue)
      _resetConnectivities();
  }

  ArrayView<Int16> components(CellLocalId item_lid)
  {
    Int16 n = m_nb_component[item_lid];
    Int32 index = m_component_index[item_lid];
    //Int32 list_size = m_component_list.size();
    //std::cout << "CELL=" << item_lid << " nb_mat=" << n
    //          << " index=" << index << " list_size=" << list_size << "\n";
    return m_component_list_as_array.subView(index, n);
  }

  void checkResize(Int64 size)
  {
    if (MeshUtils::checkResizeArray(m_nb_component_as_array, size, false))
      m_nb_component.updateFromInternal();
    if (MeshUtils::checkResizeArray(m_component_index_as_array, size, false))
      m_component_index.updateFromInternal();
  }

  void reserve(Int64 capacity)
  {
    m_nb_component_as_array.reserve(capacity);
    m_component_index_as_array.reserve(capacity);
  }

  void changeLocalIds(Int32ConstArrayView new_to_old_ids)
  {
    m_nb_component.variable()->compact(new_to_old_ids);
    m_component_index.variable()->compact(new_to_old_ids);
  }

  void notifyUpdateConnectivityList()
  {
    m_component_list.updateFromInternal();
  }

  void removeAllConnectivities()
  {
    _resetConnectivities();
  }

 private:

  //! Nombre de milieux par maille (dimensionné au nombre de mailles)
  VariableArrayInt16 m_nb_component;
  //! Indice dans \a m_component_list (Dimensionné au nombre de mailles)
  VariableArrayInt32 m_component_index;
  //! Liste des constituants
  VariableArrayInt16 m_component_list;

 public:

  VariableArrayInt16::ContainerType& m_nb_component_as_array;
  VariableArrayInt32::ContainerType& m_component_index_as_array;
  VariableArrayInt16::ContainerType& m_component_list_as_array;

 private:

  void _resetConnectivities()
  {
    m_nb_component.fill(0);
    m_component_index.fill(0);
    // Le premier élément de la liste est utilisé pour les constituants vides
    m_component_list.resize(1);
    m_component_list[0] = 0;
  }
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*!
 * \brief Classe pour calculer le nombre de matériaux d'un milieu.
 */
class ConstituentConnectivityList::NumberOfMaterialComputer
{
 public:

  NumberOfMaterialComputer(ConstituentContainer::View view,
                           SmallSpan<const Int16> environment_for_materials)
  : m_view(view)
  , m_environment_for_materials(environment_for_materials)
  {
  }

 public:

  ARCCORE_HOST_DEVICE Int16 cellNbMaterial(Int32 cell_local_id, Int16 env_id) const
  {
    auto mats = m_view.components(cell_local_id);
    Int16 nb_mat = 0;
    for (Int16 mat_id : mats) {
      Int16 current_id = m_environment_for_materials[mat_id];
      if (current_id == env_id)
        ++nb_mat;
    }
    return nb_mat;
  }

 private:

  ConstituentContainer::View m_view;
  //! Vue indiquant le milieu associé aux matériaux
  SmallSpan<const Int16> m_environment_for_materials;
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

class ConstituentConnectivityList::Container
{
 public:

  Container(const MeshHandle& mesh, const String& var_base_name)
  : m_environment(mesh, var_base_name + String("ComponentEnvironment"))
  , m_material(mesh, var_base_name + String("ComponentMaterial"))
  {
  }

 public:

  void checkResize(Int32 lid)
  {
    Int64 wanted_size = lid + 1;
    m_environment.checkResize(wanted_size);
    m_material.checkResize(wanted_size);
  }

  void changeLocalIds(Int32ConstArrayView new_to_old_ids)
  {
    m_environment.changeLocalIds(new_to_old_ids);
    m_material.changeLocalIds(new_to_old_ids);
  }

  void reserve(Int64 capacity)
  {
    m_environment.reserve(capacity);
    m_material.reserve(capacity);
  }

 public:

  ConstituentContainer m_environment;
  ConstituentContainer m_material;
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ConstituentConnectivityList::
ConstituentConnectivityList(MeshMaterialMng* mm)
: TraceAccessor(mm->traceMng())
, m_material_mng(mm)
, m_container(new Container(mm->meshHandle(), String("ComponentEnviroment") + mm->name()))
{
  // Indique si on force la modification dans fillModifiedConstituents()
  if (auto v = Convert::Type<Int32>::tryParseFromEnvironment("ARCANE_MATERIAL_FORCE_TRANSFORM", true)) {
    m_is_force_transform_all_constituants = (v.value() != 0);
    info() << "Force transformation in 'ConstituentConnectivityList' v=" << m_is_force_transform_all_constituants;
  }
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ConstituentConnectivityList::
~ConstituentConnectivityList()
{
  delete m_container;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
endCreate(bool is_continue)
{
  // Maintenant (février 2024) on construit toujours les connectivités incrémentales
  const bool always_build_connectivity = true;

  // S'enregistre auprès la famille pour être notifié des évolutions
  // mais uniquement si on a demandé le support des modifications incrémentales
  // pour éviter de consommer inutilement de la mémoire.
  // A terme on le fera tout le temps
  m_cell_family = m_material_mng->mesh()->cellFamily();
  {
    int opt_flag_value = m_material_mng->modificationFlags();
    bool use_incremental = (opt_flag_value & (int)eModificationFlags::IncrementalRecompute) != 0;
    if (use_incremental || always_build_connectivity) {
      m_cell_family->_internalApi()->addSourceConnectivity(this);
      m_is_active = true;
      info() << "Activating incremental material connectivities";
    }
  }
  if (!is_continue) {
    Int32 max_local_id = m_cell_family->maxLocalId();
    m_container->checkResize(max_local_id + 1);
    m_container->m_environment.endCreate(is_continue);
    m_container->m_material.endCreate(is_continue);
  }

  // Remplit un tableau indiquant pour chaque index de matériau l'index
  // du milieu correspondant.
  {
    ConstArrayView<MeshMaterial*> materials = m_material_mng->trueMaterials();
    const Int32 nb_mat = materials.size();
    auto environment_for_materials = m_environment_for_materials.hostModifier();
    environment_for_materials.resize(nb_mat);
    auto local_view = environment_for_materials.view();
    for (Int32 i = 0; i < nb_mat; ++i)
      local_view[i] = materials[i]->trueEnvironment()->componentId();
  }
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
_addCells(Int16 component_id, SmallSpan<const Int32> cells_local_id,
          ConstituentContainer& component, RunQueue& queue)
{
  const Int32 nb_item = cells_local_id.size();
  if (nb_item == 0)
    return;
  Array<Int16>& nb_component = component.m_nb_component_as_array;
  Array<Int32>& component_index = component.m_component_index_as_array;
  Array<Int16>& component_list = component.m_component_list_as_array;

  SmallSpan<Int16> nb_component_view = component.m_nb_component_as_array.view();

  // TODO: Utiliser des tableaux de travail à conserver.
  NumArray<Int32, MDDim1> new_indexes(nb_item, queue.memoryRessource());
  // Pour recopier le nombre d'éléments à ajouter du device vers le CPU
  NumArray<Int32, MDDim1> new_indexes_to_add(1, eMemoryRessource::HostPinned);

  // Calcul l'index des nouveaux éléments
  {
    Accelerator::GenericScanner scanner(queue);
    SmallSpan<Int32> new_indexes_view = new_indexes;
    SmallSpan<Int32> new_indexes_to_add_view = new_indexes_to_add;
    auto getter = [=] ARCCORE_HOST_DEVICE(Int32 index) -> Int32 {
      return 1 + nb_component_view[cells_local_id[index]];
    };
    auto setter = [=] ARCCORE_HOST_DEVICE(Int32 index, Int32 value) {
      new_indexes_view[index] = value;
      if (index == (nb_item - 1))
        new_indexes_to_add_view[0] = new_indexes_view[index] + nb_component_view[cells_local_id[index]] + 1;
    };
    Accelerator::ScannerSumOperator<Int32> op;
    scanner.applyWithIndexExclusive(nb_item, 0, getter, setter, op, A_FUNCINFO);
  }
  queue.barrier();

  const Int32 nb_indexes_to_add = new_indexes_to_add[0];
  const Int32 current_list_index = component_list.size();

  MemoryUtils::checkResizeArrayWithCapacity(component_list, current_list_index + nb_indexes_to_add, false);

  {
    auto command = makeCommand(queue);
    SmallSpan<Int16> nb_component_view = nb_component.view();
    SmallSpan<Int32> component_index_view = component_index.view();
    SmallSpan<Int16> component_list_view = component_list.view();
    SmallSpan<const Int32> new_indexes_view = new_indexes;
    command << RUNCOMMAND_LOOP1(iter, nb_item)
    {
      auto [i] = iter();
      Int32 cell_id = cells_local_id[i];
      const Int16 n = nb_component_view[cell_id];
      Int32 new_pos = current_list_index + new_indexes_view[i];
      if (n != 0) {
        // Recopie les anciennes valeurs.
        // TODO: cela laisse des trous dans la liste qu'il faudra supprimer
        // via un compactage.
        Int32 current_pos = component_index_view[cell_id];
        SmallSpan<const Int16> current_values(&component_list_view[current_pos], n);
        SmallSpan<Int16> new_values(&component_list_view[new_pos], n);
        new_values.copy(current_values);
      }
      component_index_view[cell_id] = new_pos;
      component_list_view[new_pos + n] = component_id;
      ++nb_component_view[cell_id];
    };
  }

  component.notifyUpdateConnectivityList();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
_removeCells(Int16 component_id, SmallSpan<const Int32> cells_local_id,
             ConstituentContainer& component, RunQueue& queue)
{
  SmallSpan<Int16> nb_component = component.m_nb_component_as_array.view();
  SmallSpan<Int32> component_index = component.m_component_index_as_array.view();
  SmallSpan<Int16> component_list = component.m_component_list_as_array.view();

  const Int32 n = cells_local_id.size();
  auto command = makeCommand(queue);
  command << RUNCOMMAND_LOOP1(iter, n)
  {
    auto [i] = iter();
    Int32 id = cells_local_id[i];
    //for (Int32 id : cell_ids) {
    CellLocalId cell_id(id);
    const Int32 current_pos = component_index[cell_id];
    const Int32 n = nb_component[cell_id];
    ArrayView<Int16> current_values(n, &component_list[current_pos]);
    // Enlève de la liste le milieu supprimé
    _removeValueAndKeepOrder(current_values, component_id);
    // Met une valeur invalide pour indiquer que l'emplacement est libre
    current_values[n - 1] = (-1);
    --nb_component[cell_id];
  };
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
addCellsToEnvironment(Int16 env_id, SmallSpan<const Int32> cell_ids, RunQueue& queue)
{
  _addCells(env_id, cell_ids, m_container->m_environment, queue);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
removeCellsToEnvironment(Int16 env_id, SmallSpan<const Int32> cell_ids, RunQueue& queue)
{
  _removeCells(env_id, cell_ids, m_container->m_environment, queue);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
addCellsToMaterial(Int16 mat_id, SmallSpan<const Int32> cell_ids, RunQueue& queue)
{
  _addCells(mat_id, cell_ids, m_container->m_material, queue);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
removeCellsToMaterial(Int16 mat_id, SmallSpan<const Int32> cell_ids, RunQueue& queue)
{
  _removeCells(mat_id, cell_ids, m_container->m_material, queue);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ConstArrayView<Int16> ConstituentConnectivityList::
cellsNbEnvironment() const
{
  return m_container->m_environment.m_nb_component_as_array;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ConstArrayView<Int16> ConstituentConnectivityList::
cellsNbMaterial() const
{
  return m_container->m_material.m_nb_component_as_array;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

Int16 ConstituentConnectivityList::
cellNbMaterial(CellLocalId cell_id, Int16 env_id) const
{
  auto environment_for_materials = m_environment_for_materials.hostView();
  Int16 nb_mat = 0;
  ArrayView<Int16> mats = m_container->m_material.components(cell_id);
  for (Int16 mat_id : mats) {
    Int16 current_id = environment_for_materials[mat_id];
    if (current_id == env_id)
      ++nb_mat;
  }
  return nb_mat;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
fillCellsNbMaterial(SmallSpan<const Int32> cells_local_id, Int16 env_id,
                    SmallSpan<Int16> cells_nb_material, RunQueue& queue)
{
  ConstituentContainer::View materials_container_view(m_container->m_material);
  bool is_device = isAcceleratorPolicy(queue.executionPolicy());
  auto environment_for_materials = m_environment_for_materials.view(is_device);
  const Int32 n = cells_local_id.size();
  auto command = makeCommand(queue);
  command << RUNCOMMAND_LOOP1(iter, n)
  {
    auto [i] = iter();
    Int32 cell_id = cells_local_id[i];
    Int16 nb_mat = 0;
    SmallSpan<const Int16> mats = materials_container_view.components(cell_id);
    for (Int16 mat_id : mats) {
      Int16 current_id = environment_for_materials[mat_id];
      if (current_id == env_id)
        ++nb_mat;
    }
    cells_nb_material[i] = nb_mat;
  };
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

Int32 ConstituentConnectivityList::
fillCellsToTransform(SmallSpan<const Int32> cells_local_id, Int16 env_id,
                     SmallSpan<bool> cells_do_transform, bool is_add, RunQueue& queue)
{
  ConstituentContainer::View materials_container_view(m_container->m_material);
  bool is_device = queue.isAcceleratorPolicy();
  auto environment_for_materials = m_environment_for_materials.view(is_device);

  NumberOfMaterialComputer nb_mat_computer(materials_container_view, environment_for_materials);
  SmallSpan<const Int16> cells_nb_env = cellsNbEnvironment();
  const Int32 n = cells_local_id.size();
  auto command = makeCommand(queue);
  Accelerator::ReducerSum2<Int32> sum_transformed(command);
  command << RUNCOMMAND_LOOP1(iter, n, sum_transformed)
  {
    auto [i] = iter();
    Int32 local_id = cells_local_id[i];
    bool do_transform = false;
    // En cas d'ajout on passe de pure à partiel s'il y a plusieurs milieux ou
    // plusieurs matériaux dans le milieu.
    // En cas de suppression, on passe de partiel à pure si on est le seul matériau
    // et le seul milieu.
    const Int16 nb_env = cells_nb_env[local_id];
    if (is_add) {
      do_transform = (nb_env > 1);
      if (!do_transform)
        do_transform = nb_mat_computer.cellNbMaterial(local_id, env_id) > 1;
    }
    else {
      do_transform = (nb_env == 1);
      if (do_transform)
        do_transform = nb_mat_computer.cellNbMaterial(local_id, env_id) == 1;
    }
    if (do_transform) {
      cells_do_transform[local_id] = do_transform;
      sum_transformed.combine(1);
    }
  };
  return sum_transformed.reducedValue();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
fillCellsIsPartial(SmallSpan<const Int32> cells_local_id, Int16 env_id,
                   SmallSpan<bool> cells_is_partial, RunQueue& queue)
{
  ConstituentContainer::View materials_container_view(m_container->m_material);
  bool is_device = queue.isAcceleratorPolicy();
  auto environment_for_materials = m_environment_for_materials.view(is_device);
  NumberOfMaterialComputer nb_mat_computer(materials_container_view, environment_for_materials);
  SmallSpan<const Int16> cells_nb_env = cellsNbEnvironment();
  const Int32 n = cells_local_id.size();
  auto command = makeCommand(queue);

  command << RUNCOMMAND_LOOP1(iter, n)
  {
    auto [i] = iter();
    Int32 local_id = cells_local_id[i];
    // On ne prend l'indice global que si on est le seul matériau et le seul
    // milieu de la maille. Sinon, on prend un indice multiple
    bool is_partial = (cells_nb_env[local_id] > 1 || nb_mat_computer.cellNbMaterial(local_id, env_id) > 1);
    cells_is_partial[i] = is_partial;
  };
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*!
 * \brief Affiche les constituants d'une liste d'entités
 * @param cells_local_id Liste des numéros locaux des entitiés
 */
void ConstituentConnectivityList::
printConstituents(SmallSpan<const Int32> cells_local_id) const
{
  const ConstituentContainer::View materials_view(m_container->m_material);
  const ConstituentContainer::View environments_view(m_container->m_environment);

  for (Int32 i = 0, n = cells_local_id.size(); i < n; ++i) {
    Int32 lid = cells_local_id[i];
    info() << "Cell index=" << i << " lid=" << lid
           << " materials=" << materials_view.components(lid)
           << " environments=" << environments_view.components(lid);
  }
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*!
 * \brief Remplit les constituants concernés par une modification.
 *
 * Les mailles concernées par la modification sont données \a cells_local_id.
 * \a modified_mat_id est le numéro du matériau ajouté (si \a is_add est vrai)
 * ou supprimé (si \a is_add est faux).
 *
 * Met à \a true les valeurs de \a is_modified_materials et \a is_modified_environments
 * s'ils sont dans une des mailles de \a cells_local_id.
 *
 * Cette méthode permet d'optimiser le calcul des constituants qui seront impactés
 * par une modification en tentant de déterminer à l'avance ceux qui seront
 * impactés. Cela permet d'éviter d'appeler des noyaux de calcul (par exemple calculer la liste
 * de mailles à transformer d'un constituant) si on sait que le constituant n'est pas impacté.
 *
 * Il est possible de revenir à l'ancien mécanisme et de supprimer cette optimisation
 * en remplissant \a is_modified_materials et \a is_modified_environments à \a true pour tout
 * le monde. Cela est le cas si m_is_force_transform_all_constituants est vrai.
 *
 * \note il est important que \a is_modified_materials et \a is_modified_environments soit
 * vrai si le constituant correspondant est modifié. Sans cela, il y aura une incohérence
 * dans les constituants. Par contre, ce n'est pas grave si on marque un constituant modifié
 * et qu'il s'avère que ce n'est pas le cas. Cela signifie juste qu'on va faire des
 * opérations inutiles (dans MeshMaterialVariableIndexer::transformCells()).
 */
void ConstituentConnectivityList::
fillModifiedConstituents(SmallSpan<const Int32> cells_local_id,
                         SmallSpan<bool> is_modified_materials,
                         SmallSpan<bool> is_modified_environments,
                         int modified_mat_id, bool is_add, const RunQueue& queue)
{
  const Int32 n = cells_local_id.size();
  if (n <= 0)
    return;

  bool is_device = queue.isAcceleratorPolicy();
  const ConstituentContainer::View materials_view(m_container->m_material);
  const ConstituentContainer::View environments_view(m_container->m_environment);
  auto env_for_mat = m_environment_for_materials.view(is_device);
  NumberOfMaterialComputer nb_mat_computer(materials_view, m_environment_for_materials.view(is_device));
  Int16 modified_env_id = m_environment_for_materials.hostView()[modified_mat_id];
  ConstArrayView<Int16> cells_nb_environment = cellsNbEnvironment();

  auto command = makeCommand(queue);
  ITraceMng* tm = traceMng();
  tm->info(4) << "FillModifiedConstituents modified_mat=" << modified_mat_id
              << " modified_env=" << modified_env_id << " is_add=" << is_add;
  const bool force_transform = m_is_force_transform_all_constituants;

  if (force_transform) {
    // Si on force la modification pour tout le monde,
    // remplit directement le tableau des milieux et matériaux modifiés
    command << RUNCOMMAND_LOOP1(iter, n)
    {
      auto [i] = iter();
      const Int32 local_id = cells_local_id[i];
      SmallSpan<const Int16> cell_envs(environments_view.components(local_id));
      for (Int16 x : cell_envs)
        is_modified_environments[x] = true;
      SmallSpan<const Int16> cell_mats(materials_view.components(local_id));
      for (Int16 x : cell_mats)
        is_modified_materials[x] = true;
    };
    return;
  }

  command << RUNCOMMAND_LOOP1(iter, n)
  {
    auto [i] = iter();
    const Int32 local_id = cells_local_id[i];
    const Int16 nb_mat_in_modified_env = nb_mat_computer.cellNbMaterial(local_id, modified_env_id);
    const Int16 nb_env = cells_nb_environment[local_id];
    // Pas de milieux dans la maille. Il s'agit de l'initialisation.
    // Aucun matériau ni milieu autre que celui ajouté n'est concerné.
    if (nb_env == 0)
      return;
    //tm->info() << "FillModified: Cell lid=" << local_id << " nb_mat_in_modified_env=" << nb_mat_in_modified_env << " nb_env=" << nb_env;
    SmallSpan<const Int16> cell_envs(environments_view.components(local_id));
    for (Int16 x : cell_envs) {
      // Ne traite pas le milieu en cours de modification
      if (x == modified_env_id)
        continue;
      bool do_transform = false;
      if (is_add) {
        // En cas d'ajout, on se transforme s'il n'y a qu'un milieu et que c'est nous
        // (c'est forcément le cas, car on est dans la boucle des milieux de la maille)
        do_transform = nb_env == 1;
      }
      else {
        // Je me transforme si on passe de 2 milieux à 1 seul (je suis forcément ce milieu
        // si je suis dans la boucle)
        do_transform = nb_env == 2;
      }
      if (do_transform) {
        //tm->info() << "FillModified:   SetTransform Cell lid=" << local_id << " env=" << x;
        is_modified_environments[x] = true;
      }
    }

    SmallSpan<const Int16> cell_mats(materials_view.components(local_id));
    for (Int16 x : cell_mats) {
      // Ne traite pas le matériau en cours de modification
      if (x == modified_mat_id)
        continue;
      bool do_transform = false;
      Int16 my_env_id = env_for_mat[x];
      // TODO: ne calculer que si nécessaire.
      Int16 my_mat_nb_env = nb_mat_computer.cellNbMaterial(local_id, my_env_id);
      //tm->info() << "FillModified:   CheckMat lid=" << local_id
      //           << " mat_id=" << x << " mat_env=" << my_env_id
      //           << " my_mat_nb_env=" << my_mat_nb_env;
      if (is_add) {
        if (my_env_id != modified_env_id && (nb_mat_in_modified_env != 0))
          continue;
        do_transform = (nb_env == 1) && (my_mat_nb_env == 1);
      }
      else {
        // Je me transforme si je suis le matériau du milieu modifié et qu'il n'y
        // a que deux matériaux dans le milieu (car je vais me retrouver le seul matériau
        // du milieu)
        // NOTE: cette condition est nécessaire mais ne garantit par que je vais
        // forcément me transformer. Pour l'instant on laisse cela pour ne pas être trop
        // restrictif.
        do_transform = true;
        if (nb_env == 1)
          do_transform = (my_env_id == modified_env_id) && (my_mat_nb_env == 2);
      }
      if (do_transform) {
        //tm->info() << "FillModified:   SetTransform Cell lid=" << local_id << " mat=" << x;
        is_modified_materials[x] = true;
      }
    }
  };
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
notifySourceFamilyLocalIdChanged([[maybe_unused]] Int32ConstArrayView new_to_old_ids)
{
  m_container->changeLocalIds(new_to_old_ids);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
notifySourceItemAdded(ItemLocalId item)
{
  Int32 lid = item.localId();
  m_container->checkResize(lid + 1);

  m_container->m_environment.m_nb_component_as_array[lid] = 0;
  m_container->m_environment.m_component_index_as_array[lid] = 0;

  m_container->m_material.m_nb_component_as_array[lid] = 0;
  m_container->m_material.m_component_index_as_array[lid] = 0;
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
reserveMemoryForNbSourceItems(Int32 n, [[maybe_unused]] bool pre_alloc_connectivity)
{
  info() << "Constituent: reserve=" << n;
  m_container->reserve(n);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
notifyReadFromDump()
{
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

Ref<IIncrementalItemSourceConnectivity> ConstituentConnectivityList::
toSourceReference()
{
  return Arccore::makeRef<IIncrementalItemSourceConnectivity>(this);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void ConstituentConnectivityList::
removeAllConnectivities()
{
  m_container->m_environment.removeAllConnectivities();
  m_container->m_material.removeAllConnectivities();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

} // namespace Arcane::Materials

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
