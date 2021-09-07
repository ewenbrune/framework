﻿// -*- tab-width: 2; indent-tabs-mode: nil; coding: utf-8-with-signature -*-
//-----------------------------------------------------------------------------
// Copyright 2000-2021 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
/*---------------------------------------------------------------------------*/
/* RunQueueInternal.h                                          (C) 2000-2021 */
/*                                                                           */
/* Implémentation de la gestion d'une file d'exécution sur accélérateur.     */
/*---------------------------------------------------------------------------*/
#ifndef ARCANE_ACCELERATOR_RUNQUEUEINTERNAL_H
#define ARCANE_ACCELERATOR_RUNQUEUEINTERNAL_H
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#include "arcane/utils/CheckedConvert.h"

#include "arcane/accelerator/AcceleratorGlobal.h"
#include "arcane/accelerator/IRunQueueRuntime.h"
#include "arcane/accelerator/NumArray.h"
#include "arcane/accelerator/RunCommandLaunchInfo.h"

#include "arcane/ItemGroup.h"
#include "arcane/Concurrency.h"

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
// TODO: fusionner les parties communes des fonctions applyItems(),
// applyLoop() et applyGenericLoop()

namespace Arcane::Accelerator
{

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

namespace impl
{

template <typename T>
struct Privatizer
{
  using value_type = T;
  using reference_type = value_type&;
  value_type m_private_copy;

  ARCCORE_HOST_DEVICE Privatizer(const T& o) : m_private_copy{o} {}
  ARCCORE_HOST_DEVICE reference_type privateCopy() { return m_private_copy; }
};

template <typename T>
ARCCORE_HOST_DEVICE auto privatize(const T& item) -> Privatizer<T>
{
  return Privatizer<T>{item};
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#if defined(ARCANE_COMPILING_CUDA)

template<typename ItemType,typename Lambda> __global__
void doIndirectCUDALambda(Span<const Int32> ids,Lambda func)
{
  typedef typename ItemType::LocalIdType LocalIdType;

  auto privatizer = privatize(func);
  auto& body = privatizer.privateCopy();

  int i = blockDim.x * blockIdx.x + threadIdx.x;
  if (i<ids.size()){
    LocalIdType lid(ids[i]);
    //if (i<10)
    //printf("CUDA %d lid=%d\n",i,lid.localId());
    body(lid);
  }
}

template<typename ItemType,typename Lambda> __global__
void doDirectCUDALambda(Int64 vsize,Lambda func)
{
  auto privatizer = privatize(func);
  auto& body = privatizer.privateCopy();

  int i = blockDim.x * blockIdx.x + threadIdx.x;
  if (i<vsize){
    //if (i<10)
    //printf("CUDA %d lid=%d\n",i,lid.localId());
    body(i);
  }
}

template<int N,typename Lambda> __global__
void doDirectCUDALambdaArrayBounds(ArrayBounds<N> bounds,Lambda func)
{
  auto privatizer = privatize(func);
  auto& body = privatizer.privateCopy();

  int i = blockDim.x * blockIdx.x + threadIdx.x;
  if (i<bounds.nbElement()){
    func(bounds.getIndices(i));
  }
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#endif // ARCANE_COMPILING_CUDA

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

template<typename ItemType,typename Lambda>
void DoIndirectThreadLambda(ItemVectorViewT<ItemType> sub_items,Lambda func)
{
  typedef typename ItemType::LocalIdType LocalIdType;

  auto privatizer = privatize(func);
  auto& body = privatizer.privateCopy();

  ENUMERATE_ITEM(iitem,sub_items){
    body(LocalIdType(iitem.itemLocalId()));
  }
}

template<typename Lambda>
void doDirectThreadLambda(Integer begin,Integer size,Lambda func)
{
  auto privatizer = privatize(func);
  auto& body = privatizer.privateCopy();

  for( Int32 i=0; i<size; ++i ){
    func(begin+i);
  }
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

template<typename Lambda> inline void
applyGenericLoopSequential(ArrayBounds<1> bounds,const Lambda& func)
{
  for( Int64 i0 = 0; i0 < bounds.extent(0); ++i0 )
    func(ArrayBoundsIndex<1>(i0));
}

template<typename Lambda> inline void
applyGenericLoopSequential(ArrayBounds<2> bounds,const Lambda& func)
{
  for( Int64 i0 = 0; i0 < bounds.extent(0); ++i0 )
    for( Int64 i1 = 0; i1 < bounds.extent(1); ++i1 )
      func(ArrayBoundsIndex<2>(i0,i1));
}

template<typename Lambda> inline void
applyGenericLoopSequential(ArrayBounds<3> bounds,const Lambda& func)
{
  for( Int64 i0 = 0; i0 < bounds.extent(0); ++i0 )
    for( Int64 i1 = 0; i1 < bounds.extent(1); ++i1 )
      for( Int64 i2 = 0; i2 < bounds.extent(2); ++i2 )
        func(ArrayBoundsIndex<3>(i0,i1,i2));
}

template<typename Lambda> inline void
applyGenericLoopSequential(ArrayBounds<4> bounds,const Lambda& func)
{
  for( Int64 i0 = 0; i0 < bounds.extent(0); ++i0 )
    for( Int64 i1 = 0; i1 < bounds.extent(1); ++i1 )
      for( Int64 i2 = 0; i2 < bounds.extent(2); ++i2 )
        for( Int64 i3 = 0; i3 < bounds.extent(3); ++i3 )
          func(ArrayBoundsIndex<4>(i0,i1,i2,i3));
}

template<typename Lambda> inline void
applyGenericLoopParallel(Int64 begin,Int64 end,[[maybe_unused]] ArrayBounds<1> bounds,const Lambda& func)
{
  for( Int64 i0 = begin; i0 < end; ++i0 )
    func(ArrayBoundsIndex<1>(i0));
}

template<typename Lambda> inline void
applyGenericLoopParallel(Int64 begin,Int64 end,ArrayBounds<2> bounds,const Lambda& func)
{
  for( Int64 i0 = begin; i0 < end; ++i0 )
    for( Int64 i1 = 0; i1 < bounds.extent(1); ++i1 )
      func(ArrayBoundsIndex<2>(i0,i1));
}

template<typename Lambda> inline void
applyGenericLoopParallel(Int64 begin,Int64 end,ArrayBounds<3> bounds,const Lambda& func)
{
  for( Int64 i0 = begin; i0 < end; ++i0 )
    for( Int64 i1 = 0; i1 < bounds.extent(1); ++i1 )
      for( Int64 i2 = 0; i2 < bounds.extent(2); ++i2 )
        func(ArrayBoundsIndex<3>(i0,i1,i2));
}

template<typename Lambda> inline void
applyGenericLoopParallel(Int64 begin,Int64 end,ArrayBounds<4> bounds,const Lambda& func)
{
  for( Int64 i0 = begin; i0 < end; ++i0 )
    for( Int64 i1 = 0; i1 < bounds.extent(1); ++i1 )
      for( Int64 i2 = 0; i2 < bounds.extent(2); ++i2 )
        for( Int64 i3 = 0; i3 < bounds.extent(3); ++i3 )
          func(ArrayBoundsIndex<4>(i0,i1,i2,i3));
}

} // End namespace impl

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*!
 * \brief Applique l'enumération \a func sur la liste d'entité \a items.
 */
template<typename ItemType,typename Lambda> void
applyItems(RunCommand& command,ItemVectorViewT<ItemType> items,Lambda func)
{
  // TODO: fusionner la partie commune avec 'applyLoop'
  Integer vsize = items.size();
  if (vsize==0)
    return;
  typedef typename ItemType::LocalIdType LocalIdType;
  impl::RunCommandLaunchInfo launch_info(command);
  switch(launch_info.executionPolicy()){
  case eExecutionPolicy::CUDA:
#if defined(ARCANE_COMPILING_CUDA)
    {
      launch_info.beginExecute();
      Span<const Int32> local_ids = items.localIds();
      auto [b,t] = launch_info.computeThreadBlockInfo(vsize);
      cudaStream_t* s = reinterpret_cast<cudaStream_t*>(launch_info._internalStreamImpl());
      // TODO: utiliser cudaLaunchKernel() à la place.
      impl::doIndirectCUDALambda<ItemType,Lambda> <<<b,t,0,*s>>>(local_ids,std::forward<Lambda>(func));
    }
#endif
    break;
  case eExecutionPolicy::Sequential:
    {
      launch_info.beginExecute();
      ENUMERATE_ITEM(iitem,items){
        func(LocalIdType(iitem.itemLocalId()));
      }
    }
    break;
  case eExecutionPolicy::Thread:
    {
      launch_info.beginExecute();
      arcaneParallelForeach(items,
                            [&](ItemVectorViewT<ItemType> sub_items)
                            {
                              impl::DoIndirectThreadLambda(sub_items,func);
                            });
    }
  }
  launch_info.endExecute();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*!
 * \brief Applique la lambda \a func sur une boucle de \a vsize itérations
 */
template<int N,typename Lambda> void
applyGenericLoop(RunCommand& command,ArrayBounds<N> bounds,const Lambda& func)
{
  Int64 vsize = bounds.nbElement();
  if (vsize==0)
    return;
  impl::RunCommandLaunchInfo launch_info(command);
  switch(launch_info.executionPolicy()){
  case eExecutionPolicy::CUDA:
#if defined(ARCANE_COMPILING_CUDA)
    {
      launch_info.beginExecute();
      auto [b,t] = launch_info.computeThreadBlockInfo(vsize);
      cudaStream_t* s = reinterpret_cast<cudaStream_t*>(launch_info._internalStreamImpl());
      // TODO: utiliser cudaLaunchKernel() à la place.
      impl::doDirectCUDALambdaArrayBounds<N,Lambda> <<<b, t, 0, *s>>>(bounds,func);
    }
#endif
    break;
  case eExecutionPolicy::Sequential:
    launch_info.beginExecute();
    impl::applyGenericLoopSequential(bounds,func);
    break;
  case eExecutionPolicy::Thread:
    launch_info.beginExecute();
    Integer my_size = CheckedConvert::toInteger(bounds.extent(0));
    ParallelLoopOptions loop_options;
    Integer nb_thread = TaskFactory::nbAllowedThread();
    if (nb_thread==0)
      nb_thread = 1;
    loop_options.setGrainSize(my_size/nb_thread);
    //std::cout << "DO_PARALLEL_FOR n=" << my_size << "\n";
    // TODO: implementer en utilisant une boucle 2D ou 3D qui existe dans TBB
    // ou alors déterminer l'interval à la main
    arcaneParallelFor(0,my_size,loop_options,[&](Integer begin, Integer size)
    {
      //std::cout << "DO_PARALLEL_FOR_IMPL begin=" << begin << " size=" << size << "\n";
      impl::applyGenericLoopParallel(begin,begin+size,bounds,func);
    });
    break;
  }
  launch_info.endExecute();
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

template<int N,typename Lambda> void
run(RunCommand& command,ArrayBounds<N> bounds,const Lambda& func)
{
  applyGenericLoop(command,bounds,func);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

template<typename ItemType,typename Lambda> void
run(RunCommand& command,const ItemGroupT<ItemType>& items,Lambda func)
{
  applyItems<ItemType>(command,items.view(),std::forward<Lambda>(func));
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

template<typename ItemType,typename Lambda> void
run(RunCommand& command,ItemVectorViewT<ItemType> items,Lambda func)
{
  applyItems<ItemType>(command,items,std::forward<Lambda>(func));
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

} // End namespace Arcane::Accelerator

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#endif  
