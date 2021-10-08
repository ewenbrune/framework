set(ARCANE_SOURCES
  ArcaneStdRegisterer.cc
  ArcaneStdRegisterer.h
  BasicReaderWriter.cc
  BasicReaderWriterDatabase.cc
  BasicReaderWriterDatabase.h
  ParallelDataReader.cc
  ParallelDataReader.h
  ParallelDataWriter.cc
  ParallelDataWriter.h
  TextReader.cc
  TextReader.h
  TextWriter.cc
  TextWriter.h
  Hdf5Utils.h
  Hdf5VariableInfoBase.h
  Hdf5ReaderWriter.h
  Hdf5MpiReaderWriter.h
  ArcaneCurveWriter.cc
  ArcaneLoadBalanceModule.cc
  DumpW.cc
  DumpW.h
  DumpWEnsight7.cc
  DumpWUCD.cc
  JsonMessagePassingProfilingService.h
  JsonMessagePassingProfilingService.cc
  MeshGeneratorService.cc
  SodMeshGenerator.cc
  SodMeshGenerator.h
  SimpleMeshGenerator.cc
  SimpleMeshGenerator.h
  CartesianMeshGenerator.cc
  CartesianMeshGenerator.h
  ArcanePostProcessingModule.cc
  ArcaneVerifierModule.cc
  ArcaneCheckpointModule.cc
  ArcaneDirectExecution.cc
  ArcaneCasePartitioner.cc
  ArcaneMeshConverter.cc
  VtkMeshIOService.cc
  VoronoiMeshIOService.cc
  MeshPartitionerBase.cc
  MeshPartitionerBase.h
  PapiPerformanceService.h
  ProfilingInfo.cc
  ProfilingInfo.h
  MasterModule.cc
  UnitTestModule.cc
  ArcaneStdRegisterer.h
  ParallelDataReader.h
  ParallelDataWriter.h
  TextReader.h
  TextWriter.h
  Hdf5Utils.h
  Hdf5VariableInfoBase.h
  Hdf5ReaderWriter.h
  Hdf5MpiReaderWriter.h
  DumpW.h
  SodMeshGenerator.h
  SimpleMeshGenerator.h
  CartesianMeshGenerator.h
  MeshPartitionerBase.h
  PapiPerformanceService.h
  ProfilingInfo.h
  ArcaneSession.h
  Hdf5VariableInfoBase.h
  PartitionConverter.h
  GraphDistributor.h
  IMeshGenerator.h
  SodMeshGenerator.h
  SimpleMeshGenerator.h
  CartesianMeshGenerator.h

  MshMeshReader.cc
  MshMeshWriter.cc
  internal/IosFile.cc
  internal/IosFile.h
  internal/IosGmsh.h

  internal/SodStandardGroupsBuilder.h
  internal/SodStandardGroupsBuilder.cc
  )

set(AXL_FILES 
  Hdf5ReaderWriter
  Hdf5VariableReader
  Hdf5VariableWriter
  Hdf5MpiReaderWriter
  EnsightHdfPostProcessor
  ArcaneBasicCheckpoint
  ArcaneLoadBalance
  Ensight7PostProcessor
  ArcaneVerifier
  ArcanePostProcessing
  ArcaneCheckpoint
  ArcaneDirectExecution
  ArcaneCasePartitioner
  ArcaneMeshConverter
  MetisMeshPartitioner
  ZoltanMeshPartitioner
  PTScotchMeshPartitioner
  Master
  UnitTest
  Cartesian2DMeshGenerator
  Cartesian3DMeshGenerator
  Sod3DMeshGenerator
  )
