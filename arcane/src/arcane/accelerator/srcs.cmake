set( ARCANE_SOURCES
  Accelerator.cc
  Accelerator.h
  AcceleratorGlobal.h
  AsyncRunQueuePool.h
  Atomic.h
  CommonCudaHipReduceImpl.h
  CommonCudaHipAtomicImpl.h
  CommonUtils.h
  CommonUtils.cc
  GenericFilterer.h
  GenericPartitioner.h
  GenericReducer.h
  GenericScanner.h
  GenericSorter.h
  IReduceMemoryImpl.h
  MaterialVariableViews.h
  MemoryCopier.cc
  NumArray.h
  NumArrayViews.h
  NumArrayViews.cc
  Reduce.h
  RunCommand.h
  RunCommandEnumerate.h
  RunCommandLaunchInfo.cc
  RunCommandLaunchInfo.h
  RunCommandLoop.h
  RunCommandMaterialEnumerate.cc
  RunCommandMaterialEnumerate.h
  Runner.h
  RunQueue.h
  RunQueueImpl.h
  RunQueueInternal.cc
  RunQueueInternal.h
  Filter.h
  Filterer.cc
  Partitioner.h
  Partitioner.cc
  Scan.h
  ScanImpl.h
  Scan.cc
  Sort.h
  Sort.cc
  SpanViews.h
  VariableViews.h
  VariableViews.cc
  Views.h
  ViewsCommon.h
)
