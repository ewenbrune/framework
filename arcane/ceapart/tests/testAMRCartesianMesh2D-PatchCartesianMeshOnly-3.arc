<?xml version="1.0"?>
<cas codename="ArcaneTest" xml:lang="fr" codeversion="1.0">
  <arcane>
    <titre>Test CartesianMesh 2D PatchCartesianMeshOnly (Variant 3)</titre>

    <description>Test du raffinement d'un maillage cartesian 2D avec le type d'AMR PatchCartesianMeshOnly</description>

    <boucle-en-temps>AMRCartesianMeshTestLoop</boucle-en-temps>

    <modules>
      <module name="ArcanePostProcessing" active="true" />
      <module name="ArcaneCheckpoint" active="true" />
    </modules>

  </arcane>

  <arcane-post-traitement>
    <periode-sortie>1</periode-sortie>
    <depouillement>
      <variable>Density</variable>
      <variable>NodeDensity</variable>
      <groupe>AllCells</groupe>
      <groupe>AllNodes</groupe>
      <groupe>AllFacesDirection0</groupe>
      <groupe>AllFacesDirection1</groupe>
    </depouillement>
  </arcane-post-traitement>


  <maillage amr-type="3">
    <meshgenerator>
      <cartesian>
        <nsd>2 2</nsd>
        <origine>0.0 0.0</origine>
        <lx nx='2'>4.0</lx>
        <ly ny='2'>4.0</ly>
      </cartesian>
    </meshgenerator>
  </maillage>

  <a-m-r-cartesian-mesh-tester>
    <renumber-patch-method>0</renumber-patch-method>
    <refinement-2d>
      <position>2.0 0.0</position>
      <length>2.0 2.0</length>
    </refinement-2d>
    <refinement-2d>
      <position>2.0 2.0</position>
      <length>2.0 2.0</length>
    </refinement-2d>
    <refinement-2d>
      <position>0.0 0.0</position>
      <length>2.0 2.0</length>
    </refinement-2d>
    <refinement-2d>
      <position>0.0 2.0</position>
      <length>2.0 2.0</length>
    </refinement-2d>
    <expected-number-of-cells-in-patchs>4 4 4 4 4</expected-number-of-cells-in-patchs>
    <expected-number-of-ghost-cells-in-patchs>12 12 12 12 12</expected-number-of-ghost-cells-in-patchs>
    <nodes-uid-hash>c162b8092f50639d0e8d83ef6439043e</nodes-uid-hash>
    <faces-uid-hash>a4b9d143dabca55819722e363022c00c</faces-uid-hash>
    <cells-uid-hash>b1a1189e7febabd5c2b0e3d0f1e91c57</cells-uid-hash>
    <nodes-direction-hash>15bcbeededc9fb9bfbf6b5347cfece0d</nodes-direction-hash>
    <faces-direction-hash>d03cc496dfc49a02aafcc4a01b1add9e</faces-direction-hash>
    <cells-direction-hash>329c56f1ccba78ce502cfc2001e0e716</cells-direction-hash>
  </a-m-r-cartesian-mesh-tester>

  <arcane-protections-reprises>
    <service-protection name="ArcaneBasic2CheckpointWriter" />
  </arcane-protections-reprises>
</cas>
