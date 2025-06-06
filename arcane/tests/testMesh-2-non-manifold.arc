<?xml version="1.0" ?>
<case codename="ArcaneTest" xml:lang="en" codeversion="1.0">
 <arcane>
  <title>Test mesh with loose items</title>
  <description>Test mesh with loose items</description>
  <timeloop>UnitTest</timeloop>
 </arcane>

 <meshes>
   <mesh>
     <filename>mesh_with_loose_items.msh</filename>
     <non-manifold-mesh>true</non-manifold-mesh>
     <face-numbering-version>0</face-numbering-version>
   </mesh>
 </meshes>

 <unit-test-module>
  <test name="MeshUnitTest">
    <create-edges>true</create-edges>
    <write-mesh-service-name>VtkLegacyMeshWriter</write-mesh-service-name>
  </test>
 </unit-test-module>

</case>
